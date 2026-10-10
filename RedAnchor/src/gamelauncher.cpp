#include "gamelauncher.h"

#include "accountmanager.h"
#include "gamelibrary.h"
#include "storemanager.h"

#include <QCollator>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QStandardPaths>
#include <QTimer>

#include <algorithm>
#include <utility>

namespace {

// Экран запуска — не меньше этого, даже если процесс игры появился сразу
constexpr int MinSplashMs = 4000;
// Запуск идёт дольше обычного — подсказка на экране
constexpr int SlowMs = 30000;
// Дольше не ждём: процесс Proton жив, а процесса игры не видно — убираем экран
constexpr int GiveUpMs = 120000;
// Игра закрылась так быстро после появления — значит, упала при запуске
constexpr int QuickExitMs = 15000;

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
    if (state != QLatin1String("starting") && m_watch)
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
    QList<qint64> pids;
    if (needle.isEmpty())
        return pids;
    const QDir proc(QStringLiteral("/proc"));
    for (const QString &entry : proc.entryList(QDir::Dirs | QDir::NoDotAndDotDot)) {
        bool numeric = false;
        const qint64 pid = entry.toLongLong(&numeric);
        if (!numeric)
            continue;
        QFile file(proc.filePath(entry) + QStringLiteral("/cmdline"));
        if (!file.open(QIODevice::ReadOnly))
            continue;
        // Смотрим только на сам исполняемый файл процесса (argv[0]), не на аргументы:
        // путь к игре есть и в аргументах Proton (python …/proton run …), gogdl и
        // обёртки Proton steam.exe, но эти процессы появляются раньше игры.
        // Wine показывает в argv[0] путь Windows (Z:\home\…\game.exe).
        QString exe = QString::fromUtf8(file.readAll().split('\0').value(0)).toLower();
        exe.replace(QLatin1Char('\\'), QLatin1Char('/')).replace(QLatin1String("z:/"), QLatin1String("/"));
        if (!exe.startsWith(needle) || exe.endsWith(QLatin1String("/steam.exe")))
            continue;
        pids.append(pid);
    }
    return pids;
}

// Раз в секунду: появился ли процесс игры
void GameLauncher::checkStarted()
{
    if (m_state != QLatin1String("starting") || !m_process)
        return;
    const qint64 elapsed = m_started.elapsed();
    if (!m_sawGame && !gamePids().isEmpty()) {
        m_sawGame = true;
        qInfo("Game process detected after %lld ms", qlonglong(elapsed));
    }
    if (m_sawGame && elapsed >= MinSplashMs) {
        setState(QStringLiteral("running"));
        return;
    }
    if (!m_slow && elapsed >= SlowMs) {
        m_slow = true;
        emit stateChanged();
    }
    if (elapsed >= GiveUpMs) {
        qWarning("Game process not detected after %lld ms, hiding the launch screen", qlonglong(elapsed));
        setState(QStringLiteral("running"));
    }
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
    m_logFile.clear();
    // DualSense напрямую — только играм, которые официально его поддерживают; остальным
    // (и пока поддержка неизвестна) — контроллер Xbox: его понимают почти все игры
    const QVariantMap compat = StoreManager::instance()->compatibility(game.value(QStringLiteral("store")).toString(),
                                                                       game.value(QStringLiteral("appId")).toString());
    m_inputMode = compat.value(QStringLiteral("ps5")).toBool() ? QStringLiteral("native") : QStringLiteral("xbox");
    setState(QStringLiteral("starting"), tr("Игра запускается…"));

    const QString proton = protonPath();
    if (proton.isEmpty()) {
        fail(tr("Для запуска нужен Proton. Установите его: Настройки → Обновления."));
        return;
    }
    const QString store = game.value(QStringLiteral("store")).toString();
    const QString appId = game.value(QStringLiteral("appId")).toString();
    const QString installPath = game.value(QStringLiteral("installPath")).toString();
    StoreManager *stores = StoreManager::instance();

    if (store == QLatin1String("gog")) {
        // gogdl сам знает, какой файл запускать; Proton — обёртка
        start(stores->gogdlExecutable(),
              {QStringLiteral("--auth-config-path"), stores->gogAuthConfig(), QStringLiteral("launch"), installPath, appId,
               QStringLiteral("--platform"), QStringLiteral("windows"), QStringLiteral("--no-wine"),
               QStringLiteral("--wrapper"), QLatin1Char('"') + proton + QStringLiteral("/proton\" run")},
              installPath, QString());
        return;
    }

    if (store == QLatin1String("steam")) {
        stores->steamLaunchInfo(appId, [this, proton, installPath, appId, gameId](const QVariantMap &info) {
            if (m_game.value(QStringLiteral("id")).toString() != gameId || m_state != QLatin1String("starting"))
                return;
            const QString executable = info.value(QStringLiteral("executable")).toString();
            if (executable.isEmpty()) {
                fail(tr("Не удалось получить параметры запуска игры из Steam."));
                return;
            }
            const QString exePath = QDir(installPath).filePath(executable);
            if (!QFileInfo::exists(exePath)) {
                fail(tr("Файл игры не найден: %1. Попробуйте переустановить игру.").arg(executable));
                return;
            }
            const QString workingDir = info.value(QStringLiteral("workingdir")).toString();
            QStringList args{QStringLiteral("run"), exePath};
            args += QProcess::splitCommand(info.value(QStringLiteral("arguments")).toString());
            start(proton + QStringLiteral("/proton"), args,
                  workingDir.isEmpty() ? QFileInfo(exePath).absolutePath() : QDir(installPath).filePath(workingDir), appId);
        });
        return;
    }

    fail(tr("Запуск игр этого магазина пока не поддерживается."));
}

void GameLauncher::start(const QString &program, const QStringList &args, const QString &workingDir, const QString &appIdForSteam)
{
    const QString store = m_game.value(QStringLiteral("store")).toString();
    const QString appId = m_game.value(QStringLiteral("appId")).toString();
    const QString profile = AccountManager::instance()->profileDir();
    const QString key = store + QLatin1Char('_') + appId;

    // Свой префикс Wine у каждой игры каждого пользователя
    const QString prefix = profile + QStringLiteral("/prefixes/") + key;
    const QString steamHome = profile + QStringLiteral("/stores/steam/home");
    QDir().mkpath(prefix);
    QDir().mkpath(steamHome);
    QDir().mkpath(profile + QStringLiteral("/logs"));

    QProcessEnvironment env = StoreManager::instance()->storeEnvironment(store);
    env.insert(QStringLiteral("STEAM_COMPAT_DATA_PATH"), prefix);
    env.insert(QStringLiteral("STEAM_COMPAT_CLIENT_INSTALL_PATH"), steamHome);
    if (m_inputMode == QLatin1String("native")) {
        // Игра поддерживает DualSense — отдаём его напрямую (hidraw): вибрация, сенсоры, значки кнопок
        env.insert(QStringLiteral("PROTON_ENABLE_HIDRAW"), QStringLiteral("1"));
    } else {
        // Система раскладки: игра видит только виртуальный контроллер Xbox, настоящие геймпады
        // Sony от неё спрятаны (их события и так захвачены оболочкой)
        env.insert(QStringLiteral("PROTON_ENABLE_HIDRAW"), QStringLiteral("0"));
        env.insert(QStringLiteral("SDL_GAMECONTROLLER_IGNORE_DEVICES"),
                   QStringLiteral("0x054c/0x0ce6,0x054c/0x0df2,0x054c/0x05c4,0x054c/0x09cc"));
    }
    // В сессии с cage игры идут через Wayland-драйвер Wine, без Xwayland:
    // на окнах Xwayland cage падает и уносит с собой всю сессию
    if (qEnvironmentVariable("REDANCHOR_SESSION_COMPOSITOR") == QLatin1String("cage")) {
        env.insert(QStringLiteral("PROTON_ENABLE_WAYLAND"), QStringLiteral("1"));
        env.remove(QStringLiteral("DISPLAY"));
    }
    if (!appIdForSteam.isEmpty()) {
        env.insert(QStringLiteral("SteamAppId"), appIdForSteam);
        env.insert(QStringLiteral("SteamGameId"), appIdForSteam);
    }

    m_process = new QProcess(this);
    m_process->setProcessEnvironment(env);
    m_process->setWorkingDirectory(workingDir);
    m_process->setProcessChannelMode(QProcess::MergedChannels);
    const QString log = profile + QStringLiteral("/logs/") + key + QStringLiteral(".log");
    m_process->setStandardOutputFile(log, QIODevice::Truncate);

    m_logFile = log;
    connect(m_process, &QProcess::finished, this, [this, log](int exitCode, QProcess::ExitStatus status) {
        m_process->deleteLater();
        m_process = nullptr;
        const qint64 elapsed = m_started.elapsed();
        qInfo("Game %s finished: exit %d after %lld ms, game process %s",
              qPrintable(m_game.value(QStringLiteral("id")).toString()), exitCode, qlonglong(elapsed),
              m_sawGame ? "seen" : "never seen");
        if (m_state == QLatin1String("idle"))
            return;   // запуск отменили
        // Игру закрыли из оверлея (или перезапускают) — это не ошибка
        if (m_stopping) {
            m_stopping = false;
            setState(QStringLiteral("idle"));
            const QString relaunch = std::exchange(m_relaunch, QString());
            if (!relaunch.isEmpty())
                launch(relaunch);
            return;
        }
        // Процесс игры так и не появился — игра не запустилась, даже если Proton вышел с кодом 0
        if (!m_sawGame && m_state == QLatin1String("starting")) {
            fail(tr("Игра не запустилась."));
            return;
        }
        if (elapsed < QuickExitMs && (status != QProcess::NormalExit || exitCode != 0)) {
            fail(tr("Игра закрылась с ошибкой сразу после запуска."));
            return;
        }
        setState(QStringLiteral("idle"));
    });
    connect(m_process, &QProcess::errorOccurred, this, [this](QProcess::ProcessError error) {
        if (error != QProcess::FailedToStart || !m_process)
            return;
        m_process->deleteLater();
        m_process = nullptr;
        fail(tr("Не удалось запустить игру."));
    });

    qInfo("Launching %s: %s %s", qPrintable(key), qPrintable(program), qPrintable(args.join(QLatin1Char(' '))));
    m_started.start();
    m_process->start(program, args);

    // Экран запуска держим, пока не появится процесс самой игры
    if (!m_watch) {
        m_watch = new QTimer(this);
        m_watch->setInterval(1000);
        connect(m_watch, &QTimer::timeout, this, &GameLauncher::checkStarted);
    }
    m_watch->start();
}

void GameLauncher::killGame()
{
    // Процессы игры (Wine) живут отдельно от Proton — завершаем и их
    for (qint64 pid : gamePids())
        QProcess::execute(QStringLiteral("kill"), {QStringLiteral("-9"), QString::number(pid)});
    if (m_process)
        m_process->kill();
}

void GameLauncher::cancel()
{
    if (m_state != QLatin1String("starting"))
        return;
    qInfo("Launch cancelled");
    setState(QStringLiteral("idle"));
    killGame();
}

void GameLauncher::stop()
{
    if (m_state != QLatin1String("starting") && m_state != QLatin1String("running"))
        return;
    qInfo("Game stop requested");
    if (!m_process) {
        setState(QStringLiteral("idle"));
        return;
    }
    m_stopping = true;
    killGame();
}

void GameLauncher::restart()
{
    const QString id = m_game.value(QStringLiteral("id")).toString();
    if (id.isEmpty() || !m_process) {
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
