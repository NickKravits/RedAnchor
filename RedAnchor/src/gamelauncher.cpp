#include "gamelauncher.h"

#include "accountmanager.h"
#include "gamelibrary.h"
#include "storemanager.h"

#include <QCollator>
#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QProcess>
#include <QStandardPaths>
#include <QTimer>
#include <QUrlQuery>

#include <algorithm>
#include <csignal>
#include <utility>

namespace {

// Запуск идёт дольше обычного — подсказка на экране
constexpr int SlowMs = 30000;
// Дольше не ждём процесса игры
constexpr int GiveUpMs = 120000;
// С umu: при первом запуске он скачивает Steam Runtime (сотни мегабайт) — ждём дольше,
// пока жив процесс запуска (отменить можно в любой момент)
constexpr int UmuGiveUpMs = 600000;
// Игра закрылась так быстро после появления — значит, упала при запуске
constexpr int QuickExitMs = 15000;
// Процессов игры не видно столько времени подряд — игра закрылась (лаунчеры перезапускаются)
constexpr int GoneMs = 4000;
// Сколько ждать ответа базы umu и сколько помнить найденный id (как кэш Heroic — 6 часов)
constexpr int UmuApiTimeoutMs = 5000;
constexpr qint64 UmuCacheSecs = 6 * 3600;

// Геймпады Sony: прячем их от игры, когда работает система раскладки (контроллер Xbox)
const QString kSonyPads = QStringLiteral("0x054c/0x0ce6,0x054c/0x0df2,0x054c/0x05c4,0x054c/0x09cc");

// Последние строки журнала для экрана ошибки
QString logTail(const QString &path, int lines)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return {};
    const qint64 size = file.size();
    file.seek(qMax<qint64>(0, size - 16384));
    QStringList all = QString::fromUtf8(file.readAll()).split(QLatin1Char('\n'), Qt::SkipEmptyParts);
    // Строки «fixme:» от Wine почти никогда не причина — пропускаем их
    QStringList useful;
    for (const QString &line : std::as_const(all)) {
        if (!line.contains(QLatin1String("fixme:")))
            useful.append(line.trimmed());
    }
    return useful.mid(qMax<qsizetype>(0, useful.size() - lines)).join(QLatin1Char('\n'));
}

QByteArray procFile(const QString &pid, const char *name)
{
    QFile file(QStringLiteral("/proc/") + pid + QLatin1Char('/') + QLatin1String(name));
    return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray();
}

void signalAll(const QList<qint64> &pids, int sig)
{
    for (qint64 pid : pids)
        ::kill(pid_t(pid), sig);
}

// Аргумент для --wrapper: gogdl и legendary разбирают его как строку оболочки (shlex.split)
QString shellQuote(QString value)
{
    return QLatin1Char('\'') + value.replace(QLatin1Char('\''), QStringLiteral("'\\''")) + QLatin1Char('\'');
}

// Магазин в терминах umu и protonfixes (как у Heroic: gog, egs)
QString umuStore(const QString &store)
{
    return store == QLatin1String("epic") ? QStringLiteral("egs") : store;
}

} // namespace

GameLauncher::GameLauncher(QObject *parent)
    : QObject(parent)
    , m_network(new QNetworkAccessManager(this))
{
}

void GameLauncher::setState(const QString &state, const QString &message)
{
    m_state = state;
    m_message = message;
    if (state != QLatin1String("failed"))
        m_details.clear();
    if (state != QLatin1String("starting"))
        m_slow = false;
    // Пока игра идёт, следим за её процессами; остановится — экран уберётся
    if ((state == QLatin1String("idle") || state == QLatin1String("failed")) && m_watch)
        m_watch->stop();
    emit stateChanged();
}

void GameLauncher::fail(const QString &message)
{
    m_state = QStringLiteral("failed");
    m_message = message;
    m_slow = false;
    if (m_watch)
        m_watch->stop();
    m_details = m_logFile.isEmpty() ? QString() : logTail(m_logFile, 6);
    qWarning("Launch failed: %s", qPrintable(message));
    emit stateChanged();
}

GameLauncher::Processes GameLauncher::scanProcesses() const
{
    // Wine пишет путь к файлу игры в виде Z:\home\…; приводим к виду /home/…
    const QString needle = m_game.value(QStringLiteral("installPath")).toString().toLower();
    const QByteArray prefixVar = "STEAM_COMPAT_DATA_PATH=" + m_prefix.toUtf8();
    const QByteArray markerVar = "REDANCHOR_LAUNCH=" + m_launchToken.toUtf8();
    const qint64 self = QCoreApplication::applicationPid();
    const qint64 launcher = m_process ? m_process->processId() : 0;
    Processes found;
    if (needle.isEmpty())
        return found;
    const QDir proc(QStringLiteral("/proc"));
    for (const QString &entry : proc.entryList(QDir::Dirs | QDir::NoDotAndDotDot)) {
        bool numeric = false;
        const qint64 pid = entry.toLongLong(&numeric);
        if (!numeric || pid == self || pid == launcher)
            continue;
        // Смотрим на сам исполняемый файл процесса (argv[0]), не на аргументы:
        // путь к игре есть и в аргументах Proton (python …/proton run …), umu-run, gogdl.
        // Wine показывает в argv[0] путь Windows (Z:\home\…\game.exe).
        QString exe = QString::fromUtf8(procFile(entry, "cmdline").split('\0').value(0)).toLower();
        exe.replace(QLatin1Char('\\'), QLatin1Char('/')).replace(QLatin1String("z:/"), QLatin1String("/"));
        bool game = exe.startsWith(needle) && !exe.endsWith(QLatin1String("/steam.exe"));
        bool inPrefix = false;
        bool marked = false;
        if (!m_launchToken.isEmpty() || (m_processDone && !m_prefix.isEmpty())) {
            for (const QByteArray &var : procFile(entry, "environ").split('\0')) {
                if (var == prefixVar)
                    inPrefix = true;
                else if (var == markerVar)
                    marked = true;
            }
        }
        // Процесс запуска уже вышел: игра — всё, что живёт в её префиксе Wine
        // (сама игра могла быть запущена лаунчером из другой папки). umu задаёт
        // STEAM_COMPAT_DATA_PATH только Proton и его потомкам — сам umu-run сюда не попадает.
        if (m_processDone && !m_prefix.isEmpty() && inPrefix)
            game = true;
        if (game)
            found.game.append(pid);
        // Метку наследуют все потомки запуска: umu-run живёт, пока идёт игра, и после выхода
        // legendary (тот запускает игру и сразу выходит)
        if (marked)
            found.launcher.append(pid);
    }
    return found;
}

// Раз в секунду: появились ли процессы игры, живы ли они ещё
void GameLauncher::checkStarted()
{
    if (m_state != QLatin1String("starting") && m_state != QLatin1String("running"))
        return;
    const qint64 elapsed = m_started.elapsed();
    const Processes found = scanProcesses();
    const bool alive = !found.game.isEmpty();
    // Запуск ещё идёт: жив процесс запуска или его потомки (umu-run готовит Steam Runtime и префикс)
    const bool launching = (m_process && !m_processDone) || !found.launcher.isEmpty();
    if (alive)
        m_lastSeen = elapsed;

    if (m_state == QLatin1String("starting")) {
        if (alive) {
            // Окно игры встаёт поверх оболочки — дальше нажатия геймпада идут только игре
            m_sawGame = true;
            qInfo("Game process detected after %lld ms", qlonglong(elapsed));
            setState(QStringLiteral("running"));
            return;
        }
        if (!m_slow && elapsed >= SlowMs) {
            m_slow = true;
            emit stateChanged();
        }
        if (elapsed >= (m_umu.isEmpty() ? GiveUpMs : UmuGiveUpMs)) {
            qWarning("Game process not detected after %lld ms", qlonglong(elapsed));
            // Процесс запуска жив, а процесса игры не видно — убираем экран, игра, возможно, идёт
            if (launching) {
                setState(QStringLiteral("running"));
                return;
            }
            fail(failureReason(tr("Игра не запустилась.")));
        }
        return;
    }

    // Игра идёт: закончилась, когда её процессов нет несколько секунд подряд
    // (и процессы запуска — gogdl, umu-run — уже вышли)
    if (alive || launching || elapsed - m_lastSeen < GoneMs)
        return;
    qInfo("Game %s finished after %lld ms", qPrintable(m_game.value(QStringLiteral("id")).toString()), qlonglong(elapsed));
    const bool quick = m_lastSeen < QuickExitMs && !m_stopping;
    m_stopping = false;
    const QString relaunch = std::exchange(m_relaunch, QString());
    if (!relaunch.isEmpty()) {
        setState(QStringLiteral("idle"));
        launch(relaunch);
        return;
    }
    if (quick) {
        fail(tr("Игра закрылась сразу после запуска."));
        return;
    }
    setState(QStringLiteral("idle"));
}

QString GameLauncher::failureReason(const QString &fallback) const
{
    QFile file(m_logFile);
    if (m_logFile.isEmpty() || !file.open(QIODevice::ReadOnly))
        return fallback;
    file.seek(qMax<qint64>(0, file.size() - 65536));
    const QString log = QString::fromUtf8(file.readAll());
    // legendary проверяет версию перед запуском и не запускает устаревшую игру
    if (log.contains(QLatin1String("Game is out of date")))
        return tr("Вышло обновление игры. Обновите её: Магазин → Epic Games → игра → «Обновить».");
    if (log.contains(QLatin1String("Login failed")))
        return tr("Вход в магазин устарел. Войдите снова: Магазин → Epic Games.");
    // umu при первом запуске скачивает Steam Runtime
    if (log.contains(QLatin1String("internet connection is required to setup umu")))
        return tr("Для первого запуска нужен интернет: umu-launcher скачивает Steam Runtime.");
    return fallback;
}

QString GameLauncher::protonPath() const
{
    const QDir dir(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + QStringLiteral("/compatibilitytools"));
    QStringList versions;
    for (const QString &name : dir.entryList({QStringLiteral("GE-Proton*")}, QDir::Dirs | QDir::NoDotAndDotDot)) {
        if (QFileInfo(dir.filePath(name) + QStringLiteral("/proton")).isFile())
            versions.append(name);
    }
    if (versions.isEmpty()) {
        qWarning("Launch: no GE-Proton in %s", qPrintable(dir.absolutePath()));
        return {};
    }
    QCollator collator;
    collator.setNumericMode(true);
    std::sort(versions.begin(), versions.end(), collator);
    return dir.filePath(versions.constLast());
}

// umu-launcher: свой («Настройки → Обновления»), иначе установленный в системе
// (Heroic тоже сначала ищет umu-run в PATH)
QString GameLauncher::umuPath()
{
    const QString own = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + QStringLiteral("/tools/umu-run");
    if (QFileInfo(own).isExecutable())
        return own;
    return QStandardPaths::findExecutable(QStringLiteral("umu-run"));
}

// Как Heroic (wiki_game_info/umu/utils.ts): umu_api.php?codename=<id игры в магазине, строчными>&store=<gog|egs>
// отвечает [{ title, umu_id }]. Найденный id помним 6 часов; если базы нет или игры в ней нет —
// запускаем с GAMEID=umu-0 (так же делает Heroic).
void GameLauncher::lookupUmuId(const QString &store, const QString &appId, std::function<void(const QString &)> onReady)
{
    const QString cacheFile = QStandardPaths::writableLocation(QStandardPaths::CacheLocation) + QStringLiteral("/umu-ids.json");
    const QString key = store + QLatin1Char('_') + appId;
    const QJsonObject cached = [&] {
        QFile file(cacheFile);
        return file.open(QIODevice::ReadOnly) ? QJsonDocument::fromJson(file.readAll()).object() : QJsonObject();
    }();
    const QJsonObject entry = cached.value(key).toObject();
    const QString known = entry.value(QStringLiteral("id")).toString();
    if (!known.isEmpty() && QDateTime::currentSecsSinceEpoch() - qint64(entry.value(QStringLiteral("time")).toDouble()) < UmuCacheSecs) {
        onReady(known);
        return;
    }

    QUrl url(QStringLiteral("https://umu.openwinecomponents.org/umu_api.php"));
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("codename"), appId.toLower());
    query.addQueryItem(QStringLiteral("store"), umuStore(store));
    url.setQuery(query);
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("RedAnchor-Shell"));
    request.setTransferTimeout(UmuApiTimeoutMs);
    QNetworkReply *reply = m_network->get(request);
    connect(reply, &QNetworkReply::finished, this, [reply, onReady, cacheFile, key, known] {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            // Нет сети — берём прежний id, даже устаревший
            qWarning("umu database: %s", qPrintable(reply->errorString()));
            onReady(known);
            return;
        }
        const QString id = QJsonDocument::fromJson(reply->readAll()).array().at(0).toObject()
                               .value(QStringLiteral("umu_id")).toString();
        qInfo("umu database: %s -> %s", qPrintable(key), id.isEmpty() ? "not found" : qPrintable(id));
        if (!id.isEmpty()) {
            QFile file(cacheFile);
            QJsonObject cache = file.open(QIODevice::ReadOnly) ? QJsonDocument::fromJson(file.readAll()).object() : QJsonObject();
            file.close();
            cache.insert(key, QJsonObject{{QStringLiteral("id"), id},
                                          {QStringLiteral("time"), double(QDateTime::currentSecsSinceEpoch())}});
            QDir().mkpath(QFileInfo(cacheFile).absolutePath());
            if (file.open(QIODevice::WriteOnly | QIODevice::Truncate))
                file.write(QJsonDocument(cache).toJson());
        }
        onReady(id);
    });
}

void GameLauncher::launch(const QString &gameId)
{
    if (m_state == QLatin1String("starting") || m_state == QLatin1String("running"))
        return;
    const QVariantMap game = GameLibrary::instance()->game(gameId);
    if (game.isEmpty() || !game.value(QStringLiteral("installed")).toBool())
        return;
    m_game = game;
    m_sawGame = false;
    m_stopping = false;
    m_processDone = false;
    m_lastSeen = 0;
    m_logFile.clear();
    m_prefix.clear();
    m_launchToken.clear();
    const int serial = ++m_launchSerial;
    // DualSense напрямую — только играм, которые официально его поддерживают; остальным
    // (и пока поддержка неизвестна) — контроллер Xbox: его понимают почти все игры
    const QVariantMap compat = StoreManager::instance()->compatibility(game.value(QStringLiteral("store")).toString(),
                                                                       game.value(QStringLiteral("appId")).toString());
    m_inputMode = compat.value(QStringLiteral("ps5")).toBool() ? QStringLiteral("native") : QStringLiteral("xbox");
    m_umu = umuPath();
    // Без umu игра всё равно запустится (через GE-Proton напрямую) — подсказываем, что лучше с ним
    setState(QStringLiteral("starting"), m_umu.isEmpty()
             ? tr("Игра запускается без umu-launcher. Установите его: Настройки → Обновления.")
             : tr("Игра запускается…"));

    const QString store = game.value(QStringLiteral("store")).toString();
    const QString appId = game.value(QStringLiteral("appId")).toString();
    if (store != QLatin1String("gog") && store != QLatin1String("epic")) {
        fail(tr("Запуск игр этого магазина пока не поддерживается."));
        return;
    }

    m_proton = protonPath();
    if (m_proton.isEmpty()) {
        fail(tr("Для запуска нужен Proton. Установите его: Настройки → Обновления."));
        return;
    }
    // Proton, umu-run и legendary — программы на Python
    if (QStandardPaths::findExecutable(QStringLiteral("python3")).isEmpty()) {
        fail(tr("Для запуска через Proton нужен Python 3. Установите системные компоненты: Настройки → Обновления."));
        return;
    }
    StoreManager *stores = StoreManager::instance();
    if (store == QLatin1String("epic") && !QFileInfo(stores->legendaryExecutable()).isExecutable()) {
        fail(tr("Для запуска игр Epic Games нужен legendary. Установите его: Настройки → Обновления."));
        return;
    }
    if (store == QLatin1String("gog") && !QFileInfo(stores->gogdlExecutable()).isExecutable()) {
        fail(tr("Для запуска игр GOG нужен gogdl. Установите его: Настройки → Обновления."));
        return;
    }
    if (m_umu.isEmpty()) {
        qWarning("Launch: umu-launcher is not installed, using GE-Proton directly");
        startGame(QString());
        return;
    }
    // Исправления для игры umu находит по её id в своей базе
    lookupUmuId(store, appId, [this, serial](const QString &umuId) {
        // Запуск отменили или начали другой, пока ждали ответа
        if (serial != m_launchSerial || m_state != QLatin1String("starting"))
            return;
        startGame(umuId);
    });
}

void GameLauncher::startGame(const QString &umuId)
{
    const QString store = m_game.value(QStringLiteral("store")).toString();
    const QString appId = m_game.value(QStringLiteral("appId")).toString();
    const QString installPath = m_game.value(QStringLiteral("installPath")).toString();
    StoreManager *stores = StoreManager::instance();
    // Загрузчик магазина сам знает, какой файл запускать; обёртка — umu-run или Proton напрямую.
    // waitforexitandrun: перед запуском дождаться, пока закроется прежний сеанс Wine этого
    // префикса (после перезапуска игры); umu делает так же по умолчанию (PROTON_VERB)
    const QString wrapper = m_umu.isEmpty() ? shellQuote(m_proton + QStringLiteral("/proton")) + QStringLiteral(" waitforexitandrun")
                                            : shellQuote(m_umu);
    if (store == QLatin1String("epic")) {
        // legendary добавляет к команде игры параметры входа Epic (exchange code для EOS) —
        // поэтому запускаем через него, а не сами
        start(stores->legendaryExecutable(),
              {QStringLiteral("launch"), appId, QStringLiteral("--no-wine"), QStringLiteral("--wrapper"), wrapper},
              installPath, umuId);
        return;
    }
    start(stores->gogdlExecutable(),
          {QStringLiteral("--auth-config-path"), stores->gogAuthConfig(), QStringLiteral("launch"), installPath, appId,
           QStringLiteral("--platform"), QStringLiteral("windows"), QStringLiteral("--no-wine"),
           QStringLiteral("--wrapper"), wrapper},
          installPath, umuId);
}

void GameLauncher::startWatch()
{
    if (!m_watch) {
        m_watch = new QTimer(this);
        m_watch->setInterval(1000);
        connect(m_watch, &QTimer::timeout, this, &GameLauncher::checkStarted);
    }
    m_watch->start();
}

void GameLauncher::start(const QString &program, const QStringList &args, const QString &workingDir, const QString &umuId)
{
    const QString store = m_game.value(QStringLiteral("store")).toString();
    const QString appId = m_game.value(QStringLiteral("appId")).toString();
    const QString installPath = m_game.value(QStringLiteral("installPath")).toString();
    const QString profile = AccountManager::instance()->profileDir();
    const QString key = store + QLatin1Char('_') + appId;

    // Свой префикс Wine у каждой игры каждого пользователя. Путь — без символических ссылок:
    // umu передаёт Proton путь префикса в таком виде, по нему ищем процессы игры
    QDir().mkpath(profile + QStringLiteral("/prefixes/") + key);
    m_prefix = QDir(profile + QStringLiteral("/prefixes/") + key).canonicalPath();
    // Proton проверяет, что каталог «клиента Steam» существует; настоящий клиент ему не нужен
    const QString clientDir = profile + QStringLiteral("/prefixes/.client");
    QDir().mkpath(clientDir);
    QDir().mkpath(profile + QStringLiteral("/logs"));
    // Метка в окружении: по ней видны все процессы этого запуска
    m_launchToken = key + QLatin1Char('-') + QString::number(QDateTime::currentMSecsSinceEpoch());

    QProcessEnvironment env = StoreManager::instance()->storeEnvironment(store);
    env.insert(QStringLiteral("REDANCHOR_LAUNCH"), m_launchToken);
    // Исправления GE-Proton для конкретных игр (protonfixes) ищут игру по магазину и id
    env.insert(QStringLiteral("STORE"), umuStore(store));
    if (!m_umu.isEmpty()) {
        // Как Heroic: GAMEID — id игры в базе umu (иначе umu-0), PROTONPATH — наш GE-Proton,
        // WINEPREFIX — префикс игры (umu сам задаёт по нему STEAM_COMPAT_DATA_PATH и прочее).
        // STEAM_COMPAT_DATA_PATH не передаём: иначе сам umu-run считался бы процессом игры
        env.insert(QStringLiteral("GAMEID"), umuId.isEmpty() ? QStringLiteral("umu-0") : umuId);
        env.insert(QStringLiteral("PROTONPATH"), m_proton);
        env.insert(QStringLiteral("WINEPREFIX"), m_prefix);
        env.insert(QStringLiteral("STEAM_COMPAT_INSTALL_PATH"), installPath);
        env.remove(QStringLiteral("STEAM_COMPAT_DATA_PATH"));
    } else {
        env.insert(QStringLiteral("STEAM_COMPAT_DATA_PATH"), m_prefix);
        env.insert(QStringLiteral("STEAM_COMPAT_CLIENT_INSTALL_PATH"), clientDir);
        env.insert(QStringLiteral("STEAM_COMPAT_INSTALL_PATH"), installPath);
        env.insert(QStringLiteral("STEAM_COMPAT_SHADER_PATH"), m_prefix + QStringLiteral("/shadercache"));
        env.insert(QStringLiteral("SteamAppId"), QStringLiteral("0"));
        env.insert(QStringLiteral("SteamGameId"), QStringLiteral("0"));
    }
    // Игры SDL не сворачиваются, когда поверх открывается оверлей оболочки
    env.insert(QStringLiteral("SDL_VIDEO_MINIMIZE_ON_FOCUS_LOSS"), QStringLiteral("0"));
    // Переменные Qt оболочки игре не нужны (QT_QPA_PLATFORM ломает игры на Qt)
    for (const QString &name : env.keys()) {
        if (name.startsWith(QLatin1String("QT_")))
            env.remove(name);
    }
    if (m_inputMode == QLatin1String("native")) {
        // Игра поддерживает DualSense — отдаём его напрямую (hidraw): вибрация, сенсоры, значки кнопок
        env.insert(QStringLiteral("PROTON_ENABLE_HIDRAW"), QStringLiteral("1"));
    } else {
        // Система раскладки: игра видит только виртуальный контроллер Xbox, настоящие геймпады
        // Sony от неё спрятаны (их события и так захвачены оболочкой)
        env.insert(QStringLiteral("PROTON_ENABLE_HIDRAW"), QStringLiteral("0"));
        env.insert(QStringLiteral("SDL_GAMECONTROLLER_IGNORE_DEVICES"), kSonyPads);
        env.insert(QStringLiteral("SDL_JOYSTICK_HIDAPI_PS5"), QStringLiteral("0"));
        env.insert(QStringLiteral("SDL_JOYSTICK_HIDAPI_PS4"), QStringLiteral("0"));
    }
    // В сессии с cage игры идут через Wayland-драйвер Wine, без Xwayland:
    // на окнах Xwayland cage падает и уносит с собой всю сессию
    if (qEnvironmentVariable("REDANCHOR_SESSION_COMPOSITOR") == QLatin1String("cage")) {
        env.insert(QStringLiteral("PROTON_ENABLE_WAYLAND"), QStringLiteral("1"));
        env.remove(QStringLiteral("DISPLAY"));
    }

    m_process = new QProcess(this);
    m_process->setProcessEnvironment(env);
    m_process->setWorkingDirectory(workingDir);
    m_process->setProcessChannelMode(QProcess::MergedChannels);
    // Вывод наследуют и потомки (umu-run, Proton) — даже после выхода legendary
    const QString log = profile + QStringLiteral("/logs/") + key + QStringLiteral(".log");
    m_process->setStandardOutputFile(log, QIODevice::Truncate);

    m_logFile = log;
    QProcess *process = m_process;
    connect(process, &QProcess::finished, this, [this, process](int exitCode, QProcess::ExitStatus status) {
        process->deleteLater();
        if (m_process != process)
            return;
        m_process = nullptr;
        m_processDone = true;
        const qint64 elapsed = m_started.elapsed();
        qInfo("Game launcher %s finished: exit %d after %lld ms, game process %s",
              qPrintable(m_game.value(QStringLiteral("id")).toString()), exitCode, qlonglong(elapsed),
              m_sawGame ? "seen" : "never seen");
        if (m_state == QLatin1String("idle") || m_state == QLatin1String("failed"))
            return;   // запуск отменили
        // Процесс игры так и не появился и запуск не продолжается (legendary выходит сразу,
        // а umu-run остаётся) — игра не запустилась, даже если загрузчик вышел с кодом 0
        const Processes found = scanProcesses();
        if (!m_sawGame && m_state == QLatin1String("starting") && found.game.isEmpty() && found.launcher.isEmpty()
            && !m_stopping) {
            fail(failureReason(status != QProcess::NormalExit || exitCode != 0 ? tr("Игра закрылась с ошибкой при запуске.")
                                                                                 : tr("Игра не запустилась.")));
            return;
        }
        // Дальше за игрой следит checkStarted: она могла остаться работать после выхода лаунчера
        checkStarted();
    });
    connect(process, &QProcess::errorOccurred, this, [this, process](QProcess::ProcessError error) {
        if (error != QProcess::FailedToStart || m_process != process)
            return;
        process->deleteLater();
        m_process = nullptr;
        m_processDone = true;
        fail(tr("Не удалось запустить игру."));
    });

    qInfo("Launching %s (%s, GAMEID %s): %s %s", qPrintable(key), m_umu.isEmpty() ? "GE-Proton directly" : "umu",
          qPrintable(env.value(QStringLiteral("GAMEID"), QStringLiteral("-"))), qPrintable(program),
          qPrintable(args.join(QLatin1Char(' '))));
    m_started.start();
    m_process->start(program, args);
    startWatch();
}

void GameLauncher::killGame()
{
    const Processes found = scanProcesses();
    QList<qint64> pids = found.game;
    for (qint64 pid : found.launcher) {
        if (!pids.contains(pid))
            pids.append(pid);
    }
    if (!m_prefix.isEmpty() && !m_proton.isEmpty()) {
        // Всё, что живёт в префиксе игры (лаунчеры, службы Wine), закрывает сам wineserver
        // того же Proton. Префикс Wine — <префикс>/pfx (у umu это ссылка на сам префикс)
        QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
        env.insert(QStringLiteral("WINEPREFIX"), m_prefix + QStringLiteral("/pfx"));
        QProcess wineserver;
        wineserver.setProcessEnvironment(env);
        wineserver.start(m_proton + QStringLiteral("/files/bin/wineserver"), {QStringLiteral("-k")});
        wineserver.waitForFinished(3000);
    }
    // Сначала просим закрыться (umu-run передаёт сигнал своим потомкам), через пару секунд —
    // завершаем принудительно
    signalAll(pids, SIGTERM);
    if (m_process)
        m_process->terminate();
    QTimer::singleShot(2500, this, [this, pids] {
        signalAll(pids, SIGKILL);
        if (m_process)
            m_process->kill();
    });
}

void GameLauncher::cancel()
{
    if (m_state != QLatin1String("starting"))
        return;
    qInfo("Launch cancelled");
    killGame();
    setState(QStringLiteral("idle"));
}

void GameLauncher::stop()
{
    if (m_state != QLatin1String("starting") && m_state != QLatin1String("running"))
        return;
    qInfo("Game stop requested");
    if (m_state == QLatin1String("starting")) {
        // Игра ещё не появилась — просто отменяем запуск (перезапуск — чуть позже,
        // когда процессы прежнего запуска закроются)
        cancel();
        const QString relaunch = std::exchange(m_relaunch, QString());
        if (!relaunch.isEmpty())
            QTimer::singleShot(3000, this, [this, relaunch] { launch(relaunch); });
        return;
    }
    m_stopping = true;
    killGame();
    // checkStarted уберёт экран, когда процессы игры закроются
    startWatch();
}

void GameLauncher::restart()
{
    const QString id = m_game.value(QStringLiteral("id")).toString();
    if (id.isEmpty()) {
        stop();
        return;
    }
    m_relaunch = id;
    stop();
}

void GameLauncher::dismiss()
{
    if (m_state == QLatin1String("failed"))
        setState(QStringLiteral("idle"));
}
