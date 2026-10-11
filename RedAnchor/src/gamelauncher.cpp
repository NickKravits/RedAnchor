#include "gamelauncher.h"

#include "accountmanager.h"
#include "gamelibrary.h"
#include "storemanager.h"

#include <QCollator>
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QStandardPaths>
#include <QTimer>

#include <algorithm>
#include <csignal>
#include <utility>

namespace {

// Запуск идёт дольше обычного — подсказка на экране
constexpr int SlowMs = 30000;
// Дольше не ждём процесса игры
constexpr int GiveUpMs = 120000;
// Игра закрылась так быстро после появления — значит, упала при запуске
constexpr int QuickExitMs = 15000;
// Процессов игры не видно столько времени подряд — игра закрылась (лаунчеры перезапускаются)
constexpr int GoneMs = 4000;

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

} // namespace

GameLauncher::GameLauncher(QObject *parent)
    : QObject(parent)
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

QList<qint64> GameLauncher::gamePids() const
{
    // Wine пишет путь к файлу игры в виде Z:\home\…; приводим к виду /home/…
    const QString needle = m_game.value(QStringLiteral("installPath")).toString().toLower();
    const QByteArray prefixVar = "STEAM_COMPAT_DATA_PATH=" + m_prefix.toUtf8();
    const qint64 self = QCoreApplication::applicationPid();
    const qint64 launcher = m_process ? m_process->processId() : 0;
    QList<qint64> pids;
    if (needle.isEmpty())
        return pids;
    const QDir proc(QStringLiteral("/proc"));
    for (const QString &entry : proc.entryList(QDir::Dirs | QDir::NoDotAndDotDot)) {
        bool numeric = false;
        const qint64 pid = entry.toLongLong(&numeric);
        if (!numeric || pid == self || pid == launcher)
            continue;
        // Смотрим на сам исполняемый файл процесса (argv[0]), не на аргументы:
        // путь к игре есть и в аргументах Proton (python …/proton run …) и gogdl.
        // Wine показывает в argv[0] путь Windows (Z:\home\…\game.exe).
        QString exe = QString::fromUtf8(procFile(entry, "cmdline").split('\0').value(0)).toLower();
        exe.replace(QLatin1Char('\\'), QLatin1Char('/')).replace(QLatin1String("z:/"), QLatin1String("/"));
        if (exe.startsWith(needle) && !exe.endsWith(QLatin1String("/steam.exe"))) {
            pids.append(pid);
            continue;
        }
        // Процесс запуска уже вышел: игра — всё, что живёт в её префиксе Wine
        // (сама игра могла быть запущена лаунчером из другой папки)
        if (m_processDone && !m_prefix.isEmpty()) {
            for (const QByteArray &var : procFile(entry, "environ").split('\0')) {
                if (var == prefixVar) {
                    pids.append(pid);
                    break;
                }
            }
        }
    }
    return pids;
}

// Раз в секунду: появились ли процессы игры, живы ли они ещё
void GameLauncher::checkStarted()
{
    if (m_state != QLatin1String("starting") && m_state != QLatin1String("running"))
        return;
    const qint64 elapsed = m_started.elapsed();
    const bool alive = !gamePids().isEmpty();
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
        if (elapsed >= GiveUpMs) {
            qWarning("Game process not detected after %lld ms", qlonglong(elapsed));
            // Процесс Proton жив, а процесса игры не видно — убираем экран, игра, возможно, идёт
            if (m_process && !m_processDone) {
                setState(QStringLiteral("running"));
                return;
            }
            fail(tr("Игра не запустилась."));
        }
        return;
    }

    // Игра идёт: закончилась, когда её процессов нет несколько секунд подряд
    // (а у GOG — ещё и процесс запуска вышел)
    if (alive || (m_process && !m_processDone) || elapsed - m_lastSeen < GoneMs)
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
    // DualSense напрямую — только играм, которые официально его поддерживают; остальным
    // (и пока поддержка неизвестна) — контроллер Xbox: его понимают почти все игры
    const QVariantMap compat = StoreManager::instance()->compatibility(game.value(QStringLiteral("store")).toString(),
                                                                       game.value(QStringLiteral("appId")).toString());
    m_inputMode = compat.value(QStringLiteral("ps5")).toBool() ? QStringLiteral("native") : QStringLiteral("xbox");
    setState(QStringLiteral("starting"), tr("Игра запускается…"));

    const QString store = game.value(QStringLiteral("store")).toString();
    const QString appId = game.value(QStringLiteral("appId")).toString();
    if (store != QLatin1String("gog")) {
        fail(tr("Запуск игр этого магазина пока не поддерживается."));
        return;
    }

    m_proton = protonPath();
    if (m_proton.isEmpty()) {
        fail(tr("Для запуска нужен Proton. Установите его: Настройки → Обновления."));
        return;
    }
    // Proton — сценарий на Python
    if (QStandardPaths::findExecutable(QStringLiteral("python3")).isEmpty()) {
        fail(tr("Для запуска через Proton нужен Python 3. Установите системные компоненты: Настройки → Обновления."));
        return;
    }
    StoreManager *stores = StoreManager::instance();
    if (!QFileInfo(stores->gogdlExecutable()).isExecutable()) {
        fail(tr("Для запуска игр GOG нужен gogdl. Установите его: Настройки → Обновления."));
        return;
    }
    const QString installPath = game.value(QStringLiteral("installPath")).toString();
    // gogdl сам знает, какой файл запускать; Proton — обёртка. waitforexitandrun: перед запуском
    // дождаться, пока закроется прежний сеанс Wine этого префикса (после перезапуска игры)
    start(stores->gogdlExecutable(),
          {QStringLiteral("--auth-config-path"), stores->gogAuthConfig(), QStringLiteral("launch"), installPath, appId,
           QStringLiteral("--platform"), QStringLiteral("windows"), QStringLiteral("--no-wine"),
           QStringLiteral("--wrapper"), QLatin1Char('"') + m_proton + QStringLiteral("/proton\" waitforexitandrun")},
          installPath);
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

void GameLauncher::start(const QString &program, const QStringList &args, const QString &workingDir)
{
    const QString store = m_game.value(QStringLiteral("store")).toString();
    const QString appId = m_game.value(QStringLiteral("appId")).toString();
    const QString installPath = m_game.value(QStringLiteral("installPath")).toString();
    const QString profile = AccountManager::instance()->profileDir();
    const QString key = store + QLatin1Char('_') + appId;

    // Свой префикс Wine у каждой игры каждого пользователя
    m_prefix = profile + QStringLiteral("/prefixes/") + key;
    // Proton проверяет, что каталог «клиента Steam» существует; настоящий клиент ему не нужен
    const QString clientDir = profile + QStringLiteral("/prefixes/.client");
    QDir().mkpath(m_prefix);
    QDir().mkpath(clientDir);
    QDir().mkpath(profile + QStringLiteral("/logs"));

    QProcessEnvironment env = StoreManager::instance()->storeEnvironment(store);
    env.insert(QStringLiteral("STEAM_COMPAT_DATA_PATH"), m_prefix);
    env.insert(QStringLiteral("STEAM_COMPAT_CLIENT_INSTALL_PATH"), clientDir);
    env.insert(QStringLiteral("STEAM_COMPAT_INSTALL_PATH"), installPath);
    env.insert(QStringLiteral("STEAM_COMPAT_SHADER_PATH"), m_prefix + QStringLiteral("/shadercache"));
    // Исправления GE-Proton для конкретных игр (protonfixes) ищут игру по магазину и id
    env.insert(QStringLiteral("STORE"), QStringLiteral("gog"));
    env.insert(QStringLiteral("SteamAppId"), QStringLiteral("0"));
    env.insert(QStringLiteral("SteamGameId"), QStringLiteral("0"));
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
        // Процесс игры так и не появился — игра не запустилась, даже если Proton вышел с кодом 0
        if (!m_sawGame && m_state == QLatin1String("starting") && gamePids().isEmpty() && !m_stopping) {
            fail(status != QProcess::NormalExit || exitCode != 0 ? tr("Игра закрылась с ошибкой при запуске.")
                                                                  : tr("Игра не запустилась."));
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

    qInfo("Launching %s: %s %s", qPrintable(key), qPrintable(program), qPrintable(args.join(QLatin1Char(' '))));
    m_started.start();
    m_process->start(program, args);
    startWatch();
}

void GameLauncher::killGame()
{
    QList<qint64> pids = gamePids();
    if (!m_prefix.isEmpty() && !m_proton.isEmpty()) {
        // Всё, что живёт в префиксе игры (лаунчеры, службы Wine), закрывает сам wineserver
        QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
        env.insert(QStringLiteral("WINEPREFIX"), m_prefix + QStringLiteral("/pfx"));
        QProcess wineserver;
        wineserver.setProcessEnvironment(env);
        wineserver.start(m_proton + QStringLiteral("/files/bin/wineserver"), {QStringLiteral("-k")});
        wineserver.waitForFinished(3000);
    }
    // Сначала просим закрыться, через пару секунд — завершаем принудительно
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
