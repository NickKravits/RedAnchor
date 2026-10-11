#include "steamclient.h"

#include "vdf.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QFutureWatcher>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QRegularExpression>
#include <QSaveFile>
#include <QStandardPaths>
#include <QTimer>
#include <QUrl>
#include <QtConcurrent/QtConcurrentRun>

#include <algorithm>
#include <csignal>
#include <unistd.h>

namespace {

// Флаги состояния приложения в appmanifest (EAppState)
constexpr qint64 StateUpdateRequired = 2;
constexpr qint64 StateFullyInstalled = 4;
constexpr qint64 StateUpdateRunning = 256;
constexpr qint64 StateUpdatePaused = 512;
constexpr qint64 StateUpdateStarted = 1024;
constexpr qint64 StateDownloading = 1048576;
constexpr qint64 StateStaging = 2097152;
constexpr qint64 StateCommitting = 4194304;
constexpr qint64 StateInProgress = StateUpdateRunning | StateUpdatePaused | StateUpdateStarted | StateDownloading
                                   | StateStaging | StateCommitting;

// SteamID64 = это число + id аккаунта
constexpr quint64 SteamIdBase = 76561197960265728ULL;

// Клиент сам не начал загрузку за это время — пробуем ещё раз
constexpr qint64 RetryAfterMs = 120000;
// Столько ждём, пока клиент войдёт в аккаунт, чтобы запустить игру
constexpr qint64 OnlineTimeoutMs = 180000;

// Служебные приложения Steam (Proton, среды выполнения) — не игры
const QSet<QString> kToolApps = {
    QStringLiteral("228980"),  QStringLiteral("1070560"), QStringLiteral("1391110"), QStringLiteral("1628350"),
    QStringLiteral("1493710"), QStringLiteral("2180100"), QStringLiteral("2348590"), QStringLiteral("2805730"),
    QStringLiteral("3658110"), QStringLiteral("1887720"), QStringLiteral("1580130"), QStringLiteral("1420170"),
    QStringLiteral("1245040"), QStringLiteral("1113280"), QStringLiteral("1054830"), QStringLiteral("961940"),
    QStringLiteral("930400"),  QStringLiteral("858280"),  QStringLiteral("1161040"), QStringLiteral("1826330"),
};

bool looksLikeTool(const QString &appId, const QString &name)
{
    return kToolApps.contains(appId) || name.startsWith(QLatin1String("Proton"))
           || name.startsWith(QLatin1String("Steam Linux Runtime"))
           || name.startsWith(QLatin1String("Steamworks Common"));
}

bool pidAlive(qint64 pid)
{
    return pid > 0 && QFileInfo::exists(QStringLiteral("/proc/%1").arg(pid));
}

QByteArray readProcFile(qint64 pid, const char *name)
{
    QFile file(QStringLiteral("/proc/%1/%2").arg(pid).arg(QLatin1String(name)));
    return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray();
}

QList<qint64> allPids()
{
    QList<qint64> pids;
    const QDir proc(QStringLiteral("/proc"));
    for (const QString &entry : proc.entryList(QDir::Dirs | QDir::NoDotAndDotDot)) {
        bool numeric = false;
        const qint64 pid = entry.toLongLong(&numeric);
        if (numeric)
            pids.append(pid);
    }
    return pids;
}

// Название папки установки из названия игры
QString folderName(const QString &title)
{
    static const QRegularExpression bad(QStringLiteral("[\\\\/:*?\"<>|\\x00-\\x1f]"));
    QString name = title;
    name.remove(bad);
    name = name.simplified();
    return name.isEmpty() ? QStringLiteral("Game") : name;
}

QString shellQuote(QString value)
{
    return QLatin1Char('\'') + value.replace(QLatin1Char('\''), QStringLiteral("'\\''")) + QLatin1Char('\'');
}

struct LibraryResult {
    bool found = false;
    QList<SteamClient::OwnedGame> owned;
    QHash<QString, QString> installDirs;
};

// Купленные игры из кэша клиента. Читается в фоне: appinfo.vdf бывает в сотни мегабайт.
LibraryResult readLibrary(const QString &root, const QHash<QString, QString> &installed)
{
    LibraryResult result;
    QHash<quint32, QList<quint32>> packages;
    const bool havePackages = Vdf::readPackageInfo(root + QStringLiteral("/appcache/packageinfo.vdf"), &packages);
    QHash<quint32, bool> wanted;
    for (auto it = packages.cbegin(); it != packages.cend(); ++it) {
        for (quint32 app : it.value())
            wanted.insert(app, true);
    }
    for (auto it = installed.cbegin(); it != installed.cend(); ++it)
        wanted.insert(it.key().toUInt(), true);
    QHash<quint32, Vdf::AppInfo> infos;
    const bool haveApps = !wanted.isEmpty()
                          && Vdf::readAppInfo(root + QStringLiteral("/appcache/appinfo.vdf"), wanted, &infos);
    result.found = havePackages && haveApps;

    QSet<QString> seen;
    for (auto it = infos.cbegin(); it != infos.cend(); ++it) {
        const Vdf::AppInfo &info = it.value();
        const QString id = QString::number(info.appId);
        if (!info.installDir.isEmpty())
            result.installDirs.insert(id, info.installDir);
        if (info.type != QLatin1String("game") || info.name.isEmpty() || looksLikeTool(id, info.name))
            continue;
        result.owned.append({id, info.name});
        seen.insert(id);
    }
    // Установленные игры, о которых в кэше ничего нет (например, кэш ещё не записан)
    for (auto it = installed.cbegin(); it != installed.cend(); ++it) {
        if (!seen.contains(it.key()) && !infos.contains(it.key().toUInt()) && !looksLikeTool(it.key(), it.value()))
            result.owned.append({it.key(), it.value()});
    }
    std::sort(result.owned.begin(), result.owned.end(), [](const SteamClient::OwnedGame &a, const SteamClient::OwnedGame &b) {
        return a.title.compare(b.title, Qt::CaseInsensitive) < 0;
    });
    return result;
}

} // namespace

SteamClient::SteamClient(QObject *parent)
    : QObject(parent)
    , m_poll(new QTimer(this))
    , m_restartTimer(new QTimer(this))
{
    m_clock.start();
    m_poll->setInterval(2000);
    connect(m_poll, &QTimer::timeout, this, &SteamClient::poll);
    m_restartTimer->setSingleShot(true);
    m_restartTimer->setInterval(1500);
    connect(m_restartTimer, &QTimer::timeout, this, &SteamClient::restartSilent);
}

SteamClient::~SteamClient()
{
    if (m_process)
        m_process->disconnect(this);
}

// ---------- Пути ----------

// Каталог клиента. Steam сам ставит ссылку ~/.steam/root на него; пока клиента ещё нет —
// туда, куда его поставит запуск: пакет Debian/Ubuntu (steam-installer) кладёт клиент в
// ~/.steam/debian-installation, пакет Valve — в ~/.local/share/Steam
QString SteamClient::root() const
{
    if (m_home.isEmpty())
        return {};
    for (const char *link : {"/.steam/root", "/.steam/steam"}) {
        const QString target = QFileInfo(m_home + QLatin1String(link)).canonicalFilePath();
        if (!target.isEmpty())
            return target;
    }
    const QString debian = m_home + QStringLiteral("/.steam/debian-installation");
    if (QFileInfo::exists(debian))
        return debian;
    QFile launcher(steamExecutable());
    if (launcher.open(QIODevice::ReadOnly) && launcher.read(64 * 1024).contains("debian-installation"))
        return debian;
    return m_home + QStringLiteral("/.local/share/Steam");
}

// HOME клиента — вне профиля, в пути без пробелов: в пути профиля есть пробел
// («RedAnchor Shell»), а скрипт запуска Steam из Ubuntu на нём ломается (сверка SHA-256
// загруженного клиента отрезает путь по последнему пробелу и всегда не сходится)
QString SteamClient::homePath(const QString &profileDir)
{
    if (profileDir.isEmpty())
        return {};
    return QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) + QStringLiteral("/redanchor/steam/")
           + QFileInfo(profileDir).fileName();
}

QString SteamClient::homeFor(const QString &profileDir)
{
    if (profileDir.isEmpty())
        return {};
    const QString home = homePath(profileDir);
    // Перенос из прежнего места (profiles/<id>/stores/steam/home)
    const QString old = profileDir + QStringLiteral("/stores/steam/home");
    if (QFileInfo(old).isDir() && !QFileInfo::exists(home)) {
        QDir().mkpath(QFileInfo(home).absolutePath());
        if (QDir().rename(old, home)) {
            qInfo("Steam home moved to %s", qPrintable(home));
            // Ссылки ~/.steam/steam и ~/.steam/root указывают на старый путь
            for (const char *name : {"/.steam/steam", "/.steam/root"}) {
                const QString link = home + QLatin1String(name);
                const QString target = QFileInfo(link).symLinkTarget();
                if (!target.isEmpty() && target.startsWith(old)) {
                    QFile::remove(link);
                    QFile::link(home + target.mid(old.size()), link);
                }
            }
            // Пути библиотек в настройках клиента
            const QStringList vdfs = {QStringLiteral("/steamapps/libraryfolders.vdf"), QStringLiteral("/config/libraryfolders.vdf")};
            for (const QString &steamDir : {home + QStringLiteral("/.steam/debian-installation"), home + QStringLiteral("/.local/share/Steam")}) {
                for (const QString &vdf : vdfs) {
                    QFile file(steamDir + vdf);
                    if (!file.open(QIODevice::ReadOnly))
                        continue;
                    QByteArray text = file.readAll();
                    file.close();
                    const QByteArray from = old.toUtf8(), to = home.toUtf8();
                    if (!text.contains(from))
                        continue;
                    text.replace(from, to);
                    QSaveFile out(steamDir + vdf);
                    if (out.open(QIODevice::WriteOnly)) {
                        out.write(text);
                        out.commit();
                    }
                }
            }
        } else {
            qWarning("Cannot move Steam home %s to %s, keeping the old one", qPrintable(old), qPrintable(home));
            return old;
        }
    }
    return home;
}

QString SteamClient::steamExecutable() const
{
    const QString found = QStandardPaths::findExecutable(QStringLiteral("steam"));
    if (!found.isEmpty())
        return found;
    for (const QString &path : {QStringLiteral("/usr/games/steam"), QStringLiteral("/usr/bin/steam")}) {
        if (QFileInfo(path).isExecutable())
            return path;
    }
    return {};
}

bool SteamClient::installed() const
{
    return !steamExecutable().isEmpty();
}

// Обёртка лежит вне профиля: в его пути есть пробел («RedAnchor Shell»),
// а параметры запуска Steam разбирает по пробелам
QString SteamClient::wrapperPath() const
{
    return QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) + QStringLiteral("/redanchor/steam-game-wrapper");
}

QString SteamClient::shimDir() const
{
    return QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) + QStringLiteral("/redanchor/steam-shim");
}

QString SteamClient::envDir() const
{
    return m_profile.isEmpty() ? QString() : m_profile + QStringLiteral("/stores/steam/env");
}

QProcessEnvironment SteamClient::environment() const
{
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    env.insert(QStringLiteral("HOME"), m_home);
    env.insert(QStringLiteral("XDG_DATA_HOME"), m_home + QStringLiteral("/.local/share"));
    env.insert(QStringLiteral("XDG_CONFIG_HOME"), m_home + QStringLiteral("/.config"));
    env.insert(QStringLiteral("XDG_CACHE_HOME"), m_home + QStringLiteral("/.cache"));
    env.insert(QStringLiteral("XDG_STATE_HOME"), m_home + QStringLiteral("/.local/state"));
    // Настройки ввода игр для обёртки запуска
    env.insert(QStringLiteral("REDANCHOR_STEAM_ENV"), envDir());
    // Игры SDL не сворачиваются, когда поверх открывается оверлей оболочки
    env.insert(QStringLiteral("SDL_VIDEO_MINIMIZE_ON_FOCUS_LOSS"), QStringLiteral("0"));
    // Переменные Qt оболочки клиенту не нужны
    for (const QString &name : env.keys()) {
        if (name.startsWith(QLatin1String("QT_")) || name.startsWith(QLatin1String("QML")))
            env.remove(name);
    }
    env.remove(QStringLiteral("SDL_GAMECONTROLLER_IGNORE_DEVICES"));
    // Заглушка zenity — первой в PATH (см. writeInstallerShim)
    const QString path = env.value(QStringLiteral("PATH"), QStringLiteral("/usr/local/bin:/usr/bin:/bin:/usr/games"));
    env.insert(QStringLiteral("PATH"), shimDir() + QLatin1Char(':') + path);
    return env;
}

QStringList SteamClient::libraryFolders() const
{
    QStringList folders;
    const QString main = root() + QStringLiteral("/steamapps");
    folders.append(main);
    const Vdf::Node file = Vdf::parseFile(main + QStringLiteral("/libraryfolders.vdf"));
    for (const Vdf::Node &entry : file[QStringLiteral("libraryfolders")].children) {
        const QString path = entry.object ? entry.str(QStringLiteral("path")) : entry.value;
        if (path.isEmpty())
            continue;
        const QString apps = QDir::cleanPath(path + QStringLiteral("/steamapps"));
        if (!folders.contains(apps) && QFileInfo(apps).canonicalFilePath() != QFileInfo(main).canonicalFilePath())
            folders.append(apps);
    }
    return folders;
}

qint64 SteamClient::pidFromFile() const
{
    if (m_home.isEmpty())
        return 0;
    QFile file(m_home + QStringLiteral("/.steam/steam.pid"));
    if (!file.open(QIODevice::ReadOnly))
        return 0;
    const qint64 pid = file.readAll().trimmed().toLongLong();
    if (!pidAlive(pid))
        return 0;
    // Номер мог достаться другому процессу
    return readProcFile(pid, "cmdline").contains("steam") ? pid : 0;
}

bool SteamClient::running() const
{
    return (m_process && m_process->state() != QProcess::NotRunning) || pidFromFile() > 0;
}

// Аккаунт, под которым клиент сейчас вошёл (0 — не вошёл)
quint32 SteamClient::activeUser() const
{
    const Vdf::Node registry = Vdf::parseFile(m_home + QStringLiteral("/.steam/registry.vdf"));
    return quint32(registry.at(QStringLiteral("Registry/HKCU/Software/Valve/Steam/ActiveProcess")).integer(QStringLiteral("ActiveUser")));
}

// ---------- Состояние ----------

void SteamClient::setPhase(const QString &phase, double progress)
{
    if (phase == m_phase && progress == m_phaseProgress)
        return;
    m_phase = phase;
    m_phaseProgress = progress;
    emit stateChanged();
}

void SteamClient::setMessage(const QString &message)
{
    if (message == m_message)
        return;
    m_message = message;
    emit stateChanged();
}

// Аккаунт — из запомненных входов клиента (loginusers.vdf)
void SteamClient::readAccount()
{
    m_accountName.clear();
    m_persona.clear();
    m_avatar.clear();
    m_steamId.clear();
    if (m_home.isEmpty())
        return;
    const Vdf::Node file = Vdf::parseFile(root() + QStringLiteral("/config/loginusers.vdf"));
    const Vdf::Node *best = nullptr;
    for (const Vdf::Node &user : file[QStringLiteral("users")].children) {
        if (!user.object || user.str(QStringLiteral("RememberPassword")) != QLatin1String("1"))
            continue;
        if (!best || user.str(QStringLiteral("MostRecent")) == QLatin1String("1")
            || (best->str(QStringLiteral("MostRecent")) != QLatin1String("1")
                && user.integer(QStringLiteral("Timestamp")) > best->integer(QStringLiteral("Timestamp")))) {
            best = &user;
        }
    }
    if (!best)
        return;
    m_steamId = best->key;
    m_accountName = best->str(QStringLiteral("AccountName"));
    m_persona = best->str(QStringLiteral("PersonaName"));
    const QString avatar = root() + QStringLiteral("/config/avatarcache/") + m_steamId + QStringLiteral(".png");
    if (QFileInfo::exists(avatar))
        m_avatar = QUrl::fromLocalFile(avatar).toString();
}

void SteamClient::setProfile(const QString &profileDir)
{
    if (profileDir == m_profile)
        return;
    const auto switchTo = [this, profileDir] {
        ++m_generation;
        m_profile = profileDir;
        m_home = homeFor(profileDir);
        if (!m_home.isEmpty())
            QDir().mkpath(m_home);
        m_requests.clear();
        m_active.clear();
        m_removing.clear();
        m_offline.clear();
        m_onOnline.clear();
        m_owned.clear();
        m_installed.clear();
        m_installDirs.clear();
        m_manifestDirs.clear();
        m_online = false;
        m_message.clear();
        m_signInStarted = 0;
        m_signInDone = false;
        m_phase.clear();
        readAccount();

        // Список игр с прошлого раза — сразу, свежий читается в фоне
        QFile cache(m_profile + QStringLiteral("/stores/steam/library.json"));
        if (!m_profile.isEmpty() && cache.open(QIODevice::ReadOnly)) {
            for (const QJsonValue &value : QJsonDocument::fromJson(cache.readAll()).array()) {
                const QJsonObject game = value.toObject();
                m_owned.append({game.value(QStringLiteral("appId")).toString(), game.value(QStringLiteral("title")).toString()});
            }
        }
        scanManifests();
        emit stateChanged();
        emit libraryChanged();
        emit downloadsChanged();
        emit installedChanged();

        if (m_home.isEmpty()) {
            m_poll->stop();
            return;
        }
        m_poll->start();
        // Клиент работает в фоне, пока пользователь вошёл в Steam: загрузки, обновления, сеть
        if (signedIn() && installed())
            startClient(Mode::Silent);
        refreshLibrary();
    };
    if (running())
        stopClient(switchTo);
    else
        switchTo();
}

QList<SteamClient::Download> SteamClient::downloads() const
{
    return m_active.values();
}

QString SteamClient::installPath(const QString &appId) const
{
    for (const InstalledGame &game : m_installed) {
        if (game.appId == appId)
            return game.path;
    }
    return {};
}

// ---------- Процесс клиента ----------

// Ход обновления клиента. В свой вывод загрузчик Steam его больше не пишет (вывод уходит
// в журнал Steam), зато пишет в logs/bootstrap_log.txt:
//   [2026-10-11 15:25:43] Downloading update (58,477 of 496,367 KB)...
//   [2026-10-11 15:26:01] Update complete, launching...
// Читаем строки после запуска клиента
void SteamClient::readBootstrapLog()
{
    QFile file(root() + QStringLiteral("/logs/bootstrap_log.txt"));
    if (!file.open(QIODevice::ReadOnly))
        return;
    if (file.size() > 32 * 1024)
        file.seek(file.size() - 32 * 1024);
    static const QRegularExpression stamp(QStringLiteral("^\\[(\\d{4}-\\d{2}-\\d{2} \\d{2}:\\d{2}:\\d{2})\\] (.*)$"));
    static const QRegularExpression download(QStringLiteral("Downloading update \\(([\\d,]+) of ([\\d,]+) KB\\)"));
    double progress = m_phaseProgress;
    QString message = m_message;
    bool complete = false;
    for (const QByteArray &raw : file.readAll().split('\n')) {
        const QRegularExpressionMatch m = stamp.match(QString::fromUtf8(raw).trimmed());
        if (!m.hasMatch())
            continue;
        const QDateTime when = QDateTime::fromString(m.captured(1), QStringLiteral("yyyy-MM-dd HH:mm:ss"));
        if (!when.isValid() || when < m_clientStartedAt)
            continue;
        const QString text = m.captured(2);
        if (const QRegularExpressionMatch d = download.match(text); d.hasMatch()) {
            const double done = QString(d.captured(1)).remove(QLatin1Char(',')).toDouble();
            const double total = QString(d.captured(2)).remove(QLatin1Char(',')).toDouble();
            if (total > 0) {
                progress = qBound(0.0, done / total, 0.99);
                message = tr("Steam обновляется…");
            }
            complete = false;
        } else if (text.startsWith(QLatin1String("Download Complete")) || text.startsWith(QLatin1String("Extracting package"))
                   || text.startsWith(QLatin1String("Installing update"))) {
            progress = 0.99;
            message = tr("Steam устанавливает обновление…");
            complete = false;
        } else if (text.startsWith(QLatin1String("Update complete")) || text.startsWith(QLatin1String("Verification complete"))
                   || text.startsWith(QLatin1String("Nothing to do"))) {
            progress = -1;
            message.clear();
            complete = true;
        }
    }
    if (progress != m_phaseProgress || message != m_message) {
        m_phaseProgress = progress;
        m_message = message;
        emit stateChanged();
    }
    // Первая загрузка на невидимом экране закончилась
    if (complete && m_bootstrapping && !m_bootstrapLogDone && !needsBootstrap()) {
        m_bootstrapLogDone = true;
        QTimer::singleShot(3000, this, &SteamClient::finishBootstrap);
    }
}

bool SteamClient::needsBootstrap() const
{
    return !root().isEmpty() && !QFileInfo::exists(root() + QStringLiteral("/ubuntu12_32/steamui.so"));
}

void SteamClient::finishBootstrap()
{
    if (!m_bootstrapping || m_stopping || !m_process)
        return;
    qInfo("Steam: first download finished, restarting the client normally");
    const Mode mode = m_bootstrapMode;
    stopClient([this, mode] {
        m_bootstrapping = false;
        m_phaseProgress = -1;
        m_message.clear();
        startClient(mode);
    });
}

void SteamClient::runCommand(const QStringList &args)
{
    QProcess process;
    process.setProgram(steamExecutable());
    process.setArguments(args);
    process.setProcessEnvironment(environment());
    process.setWorkingDirectory(m_home);
    process.setStandardOutputFile(QProcess::nullDevice());
    process.setStandardErrorFile(QProcess::nullDevice());
    qInfo("Steam: steam %s", qPrintable(args.join(QLatin1Char(' '))));
    process.startDetached();
}

void SteamClient::startClient(Mode mode)
{
    if (m_home.isEmpty())
        return;
    if (!installed()) {
        setMessage(tr("Steam не установлен. Установите его: Настройки → Обновления."));
        setPhase(QString());
        return;
    }
    if (m_stopping) {
        // Клиент ещё закрывается — запустим, когда закроется
        m_afterStop.append([this, mode] { startClient(mode); });
        return;
    }
    if (running())
        return;

    writeInstallerShim();
    applyConfig();
    m_online = false;
    m_wantRunning = true;
    QStringList args;
    if (mode == Mode::BigPicture) {
        // Big Picture — только для входа в аккаунт
        args = {QStringLiteral("-gamepadui")};
        setPhase(QStringLiteral("signin"));
    } else {
        // В фоне: без окон, без чата и списка друзей
        args = {QStringLiteral("-silent"), QStringLiteral("-nochatui"), QStringLiteral("-nofriendsui")};
        setPhase(QStringLiteral("starting"));
    }
    // Клиента в этом HOME ещё нет: первый запуск докачивает его (около 500 МБ) и показывает
    // своё окно «Updating Steam…». Делаем это на невидимом экране (Xvfb) — прогресс виден
    // в оболочке, — а когда клиент готов, закрываем его и запускаем как обычно
    QString program = steamExecutable();
    const QString xvfb = QStandardPaths::findExecutable(QStringLiteral("xvfb-run"));
    m_bootstrapping = needsBootstrap() && !xvfb.isEmpty();
    m_clientStartedAt = QDateTime::currentDateTime().addSecs(-2);
    m_bootstrapLogDone = false;
    m_bootstrapMode = mode;
    m_bootstrapReadySince = -1;
    if (m_bootstrapping) {
        args = QStringList{QStringLiteral("-a"), QStringLiteral("-s"), QStringLiteral("-screen 0 1280x800x24"), program,
                           QStringLiteral("-silent"), QStringLiteral("-nochatui"), QStringLiteral("-nofriendsui")};
        program = xvfb;
        m_phaseProgress = 0;
        m_message = tr("Steam загружается…");
        emit stateChanged();
    }
    QDir().mkpath(m_profile + QStringLiteral("/logs"));
    auto log = std::make_shared<QFile>(m_profile + QStringLiteral("/logs/steam.log"));
    if (!log->open(QIODevice::WriteOnly | QIODevice::Truncate))
        log.reset();

    m_process = new QProcess(this);
    m_process->setProgram(program);
    m_process->setArguments(args);
    m_process->setProcessEnvironment(environment());
    m_process->setWorkingDirectory(m_home);
    m_process->setProcessChannelMode(QProcess::MergedChannels);
    QProcess *process = m_process;
    auto buffer = std::make_shared<QByteArray>();
    connect(process, &QProcess::readyRead, this, [this, process, log, buffer] {
        const QByteArray chunk = process->readAll();
        if (log)
            log->write(chunk);
        buffer->append(chunk);
        static const QRegularExpression progress(QStringLiteral("^\\[\\s*(\\d{1,3})%\\]\\s*(.*)$"));
        qsizetype pos;
        while ((pos = buffer->indexOf('\n')) >= 0) {
            const QString line = QString::fromUtf8(buffer->left(pos)).trimmed();
            buffer->remove(0, pos + 1);
            // Обновление самого клиента: «[ 42%] Downloading update (…)»
            const QRegularExpressionMatch m = progress.match(line);
            if (m.hasMatch()) {
                const int percent = m.captured(1).toInt();
                m_phaseProgress = percent >= 100 ? -1 : percent / 100.0;
                m_message = percent >= 100 ? QString() : tr("Steam обновляется…");
                emit stateChanged();
            }
            // Первая загрузка на невидимом экране закончилась: «Update complete, launching…»
            if (m_bootstrapping && (line.contains(QLatin1String("Update complete"), Qt::CaseInsensitive)
                                    || line.contains(QLatin1String("Verification complete"), Qt::CaseInsensitive)))
                QTimer::singleShot(3000, this, &SteamClient::finishBootstrap);
        }
    });
    connect(process, &QProcess::finished, this, [this, process](int code) {
        qInfo("Steam client exited with code %d", code);
        process->deleteLater();
        if (m_process != process)
            return;
        m_process = nullptr;
        if (m_stopping)
            return;   // закрываем сами — stopClient доделает
        if (m_bootstrapping) {
            // Клиент закрылся во время первой загрузки: если он уже готов — запускаем как обычно
            m_bootstrapping = false;
            if (code == 0 && !needsBootstrap()) {
                startClient(m_bootstrapMode);
                return;
            }
        }
        // Клиент закрылся сам (вход отменили, Big Picture закрыли, клиент упал)
        m_online = false;
        if (m_phase == QLatin1String("signin") && !m_signInDone) {
            m_signInStarted = 0;
            setMessage(tr("Вход в Steam не завершён."));
        }
        if (!pidFromFile()) {
            setPhase(QString());
            for (const auto &callback : std::exchange(m_onOnline, {}))
                callback(false);
            // Действия, ждавшие закрытия клиента
            for (const auto &op : std::exchange(m_offline, {}))
                op();
        }
        emit stateChanged();
    });
    connect(process, &QProcess::errorOccurred, this, [this, process](QProcess::ProcessError error) {
        if (error != QProcess::FailedToStart)
            return;
        qWarning("Steam: failed to start %s", qPrintable(process->program()));
        process->deleteLater();
        if (m_process == process)
            m_process = nullptr;
        setPhase(QString());
        setMessage(tr("Не удалось запустить Steam."));
    });
    qInfo("Steam: starting client (%s%s), HOME=%s", m_bootstrapping ? "first download on a hidden display: " : "",
          qPrintable(args.join(QLatin1Char(' '))), qPrintable(m_home));
    m_process->start();
    m_poll->start();
}

void SteamClient::stopClient(std::function<void()> then)
{
    if (then)
        m_afterStop.append(std::move(then));
    if (m_stopping)
        return;
    const auto finish = [this] {
        m_stopping = false;
        m_online = false;
        if (m_phase == QLatin1String("stopping"))
            setPhase(QString());
        for (const auto &callback : std::exchange(m_onOnline, {}))
            callback(false);
        for (const auto &op : std::exchange(m_offline, {}))
            op();
        for (const auto &next : std::exchange(m_afterStop, {}))
            next();
        emit stateChanged();
    };
    if (!running()) {
        finish();
        return;
    }
    m_stopping = true;
    m_wantRunning = false;
    if (!m_bootstrapping)
        setPhase(QStringLiteral("stopping"));
    runCommand({QStringLiteral("-shutdown")});

    // Ждём, пока клиент закроется; не закрылся сам — завершаем
    auto *timer = new QTimer(this);
    timer->setInterval(500);
    auto started = std::make_shared<QElapsedTimer>();
    started->start();
    connect(timer, &QTimer::timeout, this, [this, timer, started, finish] {
        if (!running()) {
            timer->deleteLater();
            finish();
            return;
        }
        const qint64 pid = pidFromFile();
        if (started->elapsed() > 25000 && started->elapsed() < 25600) {
            qWarning("Steam: client did not exit, terminating");
            if (pid)
                ::kill(pid_t(pid), SIGTERM);
            if (m_process)
                m_process->terminate();
        } else if (started->elapsed() > 35000) {
            qWarning("Steam: killing client");
            if (pid)
                ::kill(pid_t(pid), SIGKILL);
            if (m_process)
                m_process->kill();
            if (started->elapsed() > 40000) {
                timer->deleteLater();
                finish();
            }
        }
    });
    timer->start();
}

void SteamClient::shutdown()
{
    if (running() && !m_home.isEmpty())
        runCommand({QStringLiteral("-shutdown")});
}

// Действие с файлами библиотеки — только пока клиент закрыт
void SteamClient::runOffline(std::function<void()> op)
{
    if (!running() && !m_stopping) {
        op();
        return;
    }
    m_offline.append(std::move(op));
}

void SteamClient::scheduleRestart()
{
    m_restartTimer->start();
}

// Перезапуск в фоне, чтобы клиент подхватил изменения библиотеки. Пока идёт игра или открыт
// Big Picture, ждём (poll повторит попытку): перезапуск закрыл бы игру
void SteamClient::restartSilent()
{
    if (m_home.isEmpty() || !signedIn())
        return;
    if (m_phase == QLatin1String("signin") || m_stopping)
        return;
    if (running()) {
        for (qint64 pid : allPids()) {
            if (readProcFile(pid, "cmdline").contains("SteamLaunch"))
                return;   // идёт игра
        }
        stopClient([this] { startClient(Mode::Silent); });
    } else {
        for (const auto &op : std::exchange(m_offline, {}))
            op();
        startClient(Mode::Silent);
    }
}

// ---------- Настройки клиента ----------

// Пока клиент закрыт: Steam Play для всех игр Windows (Proton Experimental), Steam Input для
// геймпадов PlayStation выключен (геймпадом в оболочке и в играх управляет оболочка — иначе
// клиент в фоне превращает геймпад в мышь и клавиатуру), обёртка запуска у установленных игр
void SteamClient::applyConfig()
{
    if (root().isEmpty())
        return;
    writeWrapper();

    const QString configPath = root() + QStringLiteral("/config/config.vdf");
    Vdf::Node config = Vdf::parseFile(configPath);
    Vdf::Node &steam = config.child(QStringLiteral("InstallConfigStore/Software/Valve/Steam"));
    bool changed = false;
    Vdf::Node &mapping = steam.child(QStringLiteral("CompatToolMapping"));
    if (!mapping.find(QStringLiteral("0"))) {
        Vdf::Node &all = mapping.child(QStringLiteral("0"));
        all.set(QStringLiteral("name"), QStringLiteral("proton_experimental"));
        all.set(QStringLiteral("config"), QString());
        all.set(QStringLiteral("priority"), QStringLiteral("75"));
        changed = true;
    }
    if (steam.str(QStringLiteral("SteamController_PSSupport")) != QLatin1String("0")) {
        steam.set(QStringLiteral("SteamController_PSSupport"), QStringLiteral("0"));
        changed = true;
    }
    if (changed && !Vdf::writeFile(configPath, config))
        qWarning("Steam: cannot write %s", qPrintable(configPath));

    // Обёртка запуска — в параметрах запуска установленных и загружаемых игр
    if (m_steamId.isEmpty())
        return;
    const quint64 accountId = m_steamId.toULongLong() - SteamIdBase;
    const QString localPath = root() + QStringLiteral("/userdata/%1/config/localconfig.vdf").arg(accountId);
    bool ok = false;
    Vdf::Node local = Vdf::parseFile(localPath, &ok);
    if (!ok)
        return;   // клиент создаст файл при первом входе — обёртку добавим в следующий раз
    QStringList ids;
    for (const InstalledGame &game : std::as_const(m_installed))
        ids.append(game.appId);
    ids += m_requests.keys();
    const QString wrapper = wrapperPath();
    Vdf::Node &apps = local.child(QStringLiteral("UserLocalConfigStore/Software/Valve/Steam/apps"));
    bool localChanged = false;
    for (const QString &id : std::as_const(ids)) {
        Vdf::Node &app = apps.child(id);
        QString options = app.str(QStringLiteral("LaunchOptions"));
        if (options.contains(wrapper))
            continue;
        if (options.contains(QLatin1String("%command%")))
            options.replace(QLatin1String("%command%"), wrapper + QStringLiteral(" %command%"));
        else
            options = (wrapper + QStringLiteral(" %command% ") + options).trimmed();
        app.set(QStringLiteral("LaunchOptions"), options);
        localChanged = true;
    }
    if (localChanged && !Vdf::writeFile(localPath, local))
        qWarning("Steam: cannot write %s", qPrintable(localPath));
}

void SteamClient::writeWrapper()
{
    static const QByteArray script =
        "#!/bin/sh\n"
        "# RedAnchor: обёртка запуска игр Steam (параметры запуска «steam-game-wrapper %command%»).\n"
        "# Перед запуском оболочка кладёт настройки ввода игры в $REDANCHOR_STEAM_ENV/<appid>.env\n"
        "id=\"${SteamAppId:-${SteamGameId:-}}\"\n"
        "if [ -n \"$id\" ] && [ -n \"$REDANCHOR_STEAM_ENV\" ] && [ -f \"$REDANCHOR_STEAM_ENV/$id.env\" ]; then\n"
        "    . \"$REDANCHOR_STEAM_ENV/$id.env\"\n"
        "fi\n"
        "exec \"$@\"\n";
    writeScript(wrapperPath(), script);
}

void SteamClient::writeScript(const QString &path, const QByteArray &script)
{
    QFile current(path);
    if (current.open(QIODevice::ReadOnly) && current.readAll() == script && QFileInfo(path).isExecutable())
        return;
    current.close();
    QDir().mkpath(QFileInfo(path).absolutePath());
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly))
        return;
    file.write(script);
    file.commit();
    QFile::setPermissions(path, QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner
                                    | QFileDevice::ReadGroup | QFileDevice::ExeGroup | QFileDevice::ReadOther
                                    | QFileDevice::ExeOther);
}

// Пакет steam-installer (Debian, Ubuntu) при первом запуске в новом HOME спрашивает через
// zenity «Steam installer — Install / Cancel». В gamescope это окно ни геймпадом, ни мышью
// не нажать, а Steam ставит сам пользователь из оболочки — поэтому первым в PATH клиента
// лежит заглушка zenity: на этот вопрос она отвечает «Install», остальное передаёт
// настоящему zenity (или yad)
void SteamClient::writeInstallerShim()
{
    static const QByteArray script =
        "#!/bin/sh\n"
        "# RedAnchor: заглушка zenity для клиента Steam (см. SteamClient::writeInstallerShim)\n"
        "question=; installer=\n"
        "for arg in \"$@\"; do\n"
        "    case \"$arg\" in\n"
        "        --question) question=1 ;;\n"
        "        \"--title=Steam installer\") installer=1 ;;\n"
        "    esac\n"
        "done\n"
        "[ -n \"$question\" ] && [ -n \"$installer\" ] && exit 0\n"
        "here=$(dirname \"$0\")\n"
        "PATH=$(printf '%s' \"$PATH\" | tr ':' '\\n' | grep -vxF \"$here\" | paste -sd: -)\n"
        "export PATH\n"
        "if command -v zenity >/dev/null 2>&1; then exec zenity \"$@\"; fi\n"
        "if command -v yad >/dev/null 2>&1; then exec yad \"$@\"; fi\n"
        "exit 1\n";
    writeScript(shimDir() + QStringLiteral("/zenity"), script);
}

// ---------- Вход ----------

void SteamClient::signIn()
{
    if (m_home.isEmpty())
        return;
    if (!installed()) {
        setMessage(tr("Steam не установлен. Установите его: Настройки → Обновления."));
        return;
    }
    setMessage(QString());
    if (signedIn()) {
        if (!running())
            startClient(Mode::Silent);
        return;
    }
    m_signInStarted = QDateTime::currentSecsSinceEpoch();
    m_signInDone = false;
    // Клиент в фоне без входа не бывает, но на всякий случай — Big Picture с нуля
    if (running())
        stopClient([this] { startClient(Mode::BigPicture); });
    else
        startClient(Mode::BigPicture);
}

// Вход отменили (PS → «Отменить вход» или «Отмена» в магазине)
void SteamClient::cancelSignIn()
{
    if (m_phase != QLatin1String("signin") || m_signInDone)
        return;
    m_signInStarted = 0;
    stopClient(nullptr);
}

void SteamClient::onSignedIn()
{
    readAccount();
    m_signInStarted = 0;
    qInfo("Steam: signed in as %s", qPrintable(m_accountName));
    if (m_accountName.isEmpty()) {
        setMessage(tr("Вход выполнен, но Steam не запомнил его. Отметьте «Запомнить меня» и войдите снова."));
        return;
    }
    setMessage(QString());
    emit stateChanged();
    // Закрываем Big Picture: клиент записывает кэш библиотеки, потом работает в фоне
    stopClient([this] {
        refreshLibrary();
        startClient(Mode::Silent);
    });
}

void SteamClient::signOut()
{
    if (m_home.isEmpty())
        return;
    const QString home = m_home;
    const QString dir = root();
    stopClient([this, dir, home] {
        // Запомненный вход: аккаунты, токены и автовход. Игры остаются на консоли.
        QFile::remove(dir + QStringLiteral("/config/loginusers.vdf"));
        QFile::remove(dir + QStringLiteral("/config/local.vdf"));
        const QString registryPath = home + QStringLiteral("/.steam/registry.vdf");
        Vdf::Node registry = Vdf::parseFile(registryPath);
        if (Vdf::Node *steam = registry.child(QStringLiteral("Registry/HKCU/Software/Valve")).find(QStringLiteral("Steam"))) {
            steam->set(QStringLiteral("AutoLoginUser"), QString());
            steam->set(QStringLiteral("RememberPassword"), QStringLiteral("0"));
            Vdf::writeFile(registryPath, registry);
        }
        QFile::remove(m_profile + QStringLiteral("/stores/steam/library.json"));
        readAccount();
        m_owned.clear();
        m_requests.clear();
        emit libraryChanged();
        emit stateChanged();
        scanManifests();
    });
}

// ---------- Библиотека ----------

void SteamClient::refreshLibrary()
{
    if (m_home.isEmpty() || m_libraryBusy)
        return;
    m_libraryBusy = true;
    emit stateChanged();
    scanManifests();
    QHash<QString, QString> installedTitles;
    for (const InstalledGame &game : std::as_const(m_installed))
        installedTitles.insert(game.appId, game.title);
    const QString dir = root();
    const int generation = m_generation;
    auto *watcher = new QFutureWatcher<LibraryResult>(this);
    connect(watcher, &QFutureWatcher<LibraryResult>::finished, this, [this, watcher, generation] {
        watcher->deleteLater();
        m_libraryBusy = false;
        if (generation != m_generation)
            return;
        const LibraryResult result = watcher->result();
        m_installDirs = result.installDirs;
        if (result.found || !result.owned.isEmpty()) {
            m_owned = result.owned;
            QJsonArray array;
            for (const OwnedGame &game : std::as_const(m_owned))
                array.append(QJsonObject{{QStringLiteral("appId"), game.appId}, {QStringLiteral("title"), game.title}});
            QSaveFile cache(m_profile + QStringLiteral("/stores/steam/library.json"));
            if (cache.open(QIODevice::WriteOnly)) {
                cache.write(QJsonDocument(array).toJson(QJsonDocument::Compact));
                cache.commit();
            }
            if (m_message == tr("Библиотека Steam появится после первой синхронизации клиента."))
                m_message.clear();
        } else if (signedIn() && m_owned.isEmpty()) {
            m_message = tr("Библиотека Steam появится после первой синхронизации клиента.");
        }
        qInfo("Steam library: %lld games (cache %s)", qlonglong(m_owned.size()), result.found ? "found" : "missing");
        emit stateChanged();
        emit libraryChanged();
    });
    watcher->setFuture(QtConcurrent::run([dir, installedTitles] { return readLibrary(dir, installedTitles); }));
}

// Установленные и загружаемые игры — по appmanifest во всех папках библиотек
void SteamClient::scanManifests()
{
    QList<InstalledGame> games;
    QMap<QString, Download> active;
    QHash<QString, QString> manifestDirs;
    QStringList finished;
    if (!m_home.isEmpty()) {
        for (const QString &folder : libraryFolders()) {
            const QDir dir(folder);
            for (const QString &name : dir.entryList({QStringLiteral("appmanifest_*.acf")}, QDir::Files)) {
                const Vdf::Node state = Vdf::parseFile(dir.filePath(name))[QStringLiteral("AppState")];
                const QString appId = state.str(QStringLiteral("appid"));
                const QString title = state.str(QStringLiteral("name"));
                if (appId.isEmpty() || looksLikeTool(appId, title) || m_removing.contains(appId))
                    continue;
                manifestDirs.insert(appId, folder);
                const qint64 flags = state.integer(QStringLiteral("StateFlags"));
                const QString path = folder + QStringLiteral("/common/") + state.str(QStringLiteral("installdir"));
                const bool requested = m_requests.contains(appId);
                // Клиент правда качает или ставит (а не просто запланировал автообновление)
                const bool working = flags & (StateInProgress & ~StateUpdateStarted);
                // Поставленная из оболочки игра ещё не установлена до конца
                const bool pending = requested && (!(flags & StateFullyInstalled) || (flags & StateUpdateRequired)
                                                   || (flags & StateUpdateStarted));
                if ((flags & StateFullyInstalled) && QFileInfo::exists(path))
                    games.append({appId, title, path, state.integer(QStringLiteral("SizeOnDisk"))});
                if (working || pending) {
                    Download d;
                    d.appId = appId;
                    d.title = requested ? m_requests.value(appId).title : title;
                    const qint64 total = state.integer(QStringLiteral("BytesToDownload")) + state.integer(QStringLiteral("BytesToStage"));
                    const qint64 done = state.integer(QStringLiteral("BytesDownloaded")) + state.integer(QStringLiteral("BytesStaged"));
                    d.progress = total > 0 ? qBound(0.0, double(done) / double(total), 1.0) : -1;
                    d.status = (flags & (StateDownloading | StateUpdateRunning | StateStaging | StateCommitting))
                                   ? QStringLiteral("downloading") : QStringLiteral("queued");
                    if (flags & StateUpdatePaused)
                        d.message = tr("Загрузка приостановлена");
                    else if (flags & StateCommitting)
                        d.message = tr("Установка…");
                    else if (d.status == QLatin1String("downloading"))
                        d.message = d.progress >= 0 ? tr("Загрузка: %1%").arg(qRound(d.progress * 100)) : tr("Загрузка…");
                    else
                        d.message = tr("В очереди Steam");
                    active.insert(appId, d);
                } else if (requested && (flags & StateFullyInstalled)) {
                    finished.append(appId);
                }
            }
        }
    }

    // Запросы оболочки, по которым клиент ещё ничего не делает. Пока клиент качает другую
    // игру, остальные просто ждут своей очереди
    bool busyElsewhere = false;
    QStringList retry;
    for (const Download &d : std::as_const(active))
        busyElsewhere = busyElsewhere || d.status == QLatin1String("downloading");
    for (auto it = m_requests.begin(); it != m_requests.end(); ++it) {
        if (finished.contains(it.key()))
            continue;
        Download d = active.value(it.key());
        const bool started = d.status == QLatin1String("downloading") || d.progress > 0;
        if (!started && !it->failed && it->since > 0 && m_online && !busyElsewhere
            && m_clock.elapsed() - it->since > RetryAfterMs) {
            // Клиент не начал загрузку сам: ещё раз кладём appmanifest и тихо перезапускаем
            // клиент; не помогло и со второго раза — сообщаем об ошибке (окон Steam не показываем)
            if (it->retries < 1) {
                ++it->retries;
                it->since = 0;
                qWarning("Steam: %s did not start downloading, retrying", qPrintable(it.key()));
                retry.append(it.key());
            } else {
                it->failed = true;
            }
        }
        d.appId = it.key();
        d.title = it->title;
        if (it->failed) {
            d.status = QStringLiteral("failed");
            d.message = tr("Steam не начал загрузку. Проверьте подключение к интернету и место на диске.");
        } else if (d.status.isEmpty()) {
            d.status = QStringLiteral("queued");
            d.message = m_online ? tr("Steam готовит загрузку…") : tr("Steam запускается…");
        }
        active.insert(it.key(), d);
    }
    for (const QString &appId : std::as_const(retry)) {
        queueManifest(appId);
        scheduleRestart();
    }
    for (const QString &appId : std::as_const(finished)) {
        const QString title = m_requests.take(appId).title;
        qInfo("Steam: %s installed", qPrintable(appId));
        emit gameInstalled(title);
    }

    m_manifestDirs = manifestDirs;
    const auto sameGames = [](const QList<InstalledGame> &a, const QList<InstalledGame> &b) {
        if (a.size() != b.size())
            return false;
        for (qsizetype i = 0; i < a.size(); ++i) {
            if (a.at(i).appId != b.at(i).appId || a.at(i).path != b.at(i).path || a.at(i).title != b.at(i).title)
                return false;
        }
        return true;
    };
    if (!sameGames(games, m_installed)) {
        m_installed = games;
        emit installedChanged();
    }
    bool downloadsDiffer = active.size() != m_active.size();
    for (auto it = active.cbegin(); !downloadsDiffer && it != active.cend(); ++it) {
        const Download &old = m_active.value(it.key());
        downloadsDiffer = old.status != it->status || old.progress != it->progress || old.message != it->message;
    }
    if (downloadsDiffer || !finished.isEmpty()) {
        m_active = active;
        emit downloadsChanged();
    }
}

void SteamClient::poll()
{
    if (m_home.isEmpty())
        return;
    const bool isRunning = running();

    // Вход в Big Picture удался: клиент вошёл или запомнил аккаунт после открытия входа
    if (m_phase == QLatin1String("signin") && !m_signInDone && m_signInStarted > 0) {
        bool done = isRunning && activeUser() != 0;
        if (!done) {
            const Vdf::Node users = Vdf::parseFile(root() + QStringLiteral("/config/loginusers.vdf"))[QStringLiteral("users")];
            for (const Vdf::Node &user : users.children) {
                if (user.str(QStringLiteral("RememberPassword")) == QLatin1String("1")
                    && user.integer(QStringLiteral("Timestamp")) >= m_signInStarted - 60)
                    done = true;
            }
        }
        if (done) {
            m_signInDone = true;
            // Даём клиенту дописать файлы входа
            const int generation = m_generation;
            QTimer::singleShot(4000, this, [this, generation] {
                if (generation == m_generation)
                    onSignedIn();
            });
        }
    }

    // Клиент вошёл: так пишет registry.vdf. Если клиент его не обновляет — считаем, что за
    // полминуты работы с запомненным входом он вошёл (команды steam:// он всё равно примет)
    if (!isRunning)
        m_runningSince = -1;
    else if (m_runningSince < 0)
        m_runningSince = m_clock.elapsed();
    const bool online = isRunning && !m_stopping && m_phase != QLatin1String("signin")
                        && (activeUser() != 0 || (signedIn() && m_clock.elapsed() - m_runningSince > 30000));
    if (online != m_online) {
        m_online = online;
        qInfo("Steam: client %s", online ? "online" : "offline");
        if (online) {
            if (m_phase == QLatin1String("starting"))
                setPhase(QString());
            // С этого момента ждём, начнёт ли клиент поставленные загрузки
            for (Request &request : m_requests)
                request.since = m_clock.elapsed();
            for (const auto &callback : std::exchange(m_onOnline, {}))
                callback(true);
        }
        emit stateChanged();
    }
    // Клиент не вошёл слишком долго — игры, ждавшие запуска, получают ошибку
    if (!online && !m_onOnline.isEmpty() && m_clock.elapsed() - m_onlineWaitSince > OnlineTimeoutMs) {
        for (const auto &callback : std::exchange(m_onOnline, {}))
            callback(false);
    }
    // Клиент закрылся без нас (например, из Big Picture) — фаза больше не действует
    if (!isRunning && !m_stopping && !m_phase.isEmpty() && m_phase != QLatin1String("stopping") && !m_process)
        setPhase(QString());
    if (m_process && (m_bootstrapping || !m_phase.isEmpty()))
        readBootstrapLog();
    // Первая загрузка: строку «Update complete» не увидели, но клиент уже на месте —
    // ждём минуту (докачивает и распаковывает) и запускаем как обычно
    if (m_bootstrapping && !needsBootstrap()) {
        if (m_bootstrapReadySince < 0)
            m_bootstrapReadySince = m_clock.elapsed();
        else if (m_clock.elapsed() - m_bootstrapReadySince > 60000)
            finishBootstrap();
    }
    // Отложенные изменения библиотеки (ждали конца игры)
    if (!m_offline.isEmpty() && !m_restartTimer->isActive() && !m_stopping)
        restartSilent();

    scanManifests();
}

// ---------- Загрузки ----------

void SteamClient::install(const QString &appId, const QString &title)
{
    if (m_home.isEmpty() || !signedIn() || appId.isEmpty())
        return;
    if (m_requests.contains(appId)) {
        if (!m_requests[appId].failed)
            return;
        m_requests.remove(appId);   // повтор после ошибки
    }
    if (!installPath(appId).isEmpty())
        return;
    m_requests.insert(appId, Request{title, running() ? m_clock.elapsed() : 0});
    queueManifest(appId);
    scanManifests();
    emit downloadsChanged();
    scheduleRestart();
}

// appmanifest игры с флагом «нужно обновление» — пока клиент закрыт
void SteamClient::queueManifest(const QString &appId)
{
    const QString title = m_requests.value(appId).title;

    const QString folder = m_manifestDirs.value(appId, root() + QStringLiteral("/steamapps"));
    const QString installDir = m_installDirs.value(appId, folderName(title));
    const QString steamId = m_steamId;
    runOffline([this, appId, title, folder, installDir, steamId] {
        const QString path = folder + QStringLiteral("/appmanifest_") + appId + QStringLiteral(".acf");
        if (QFileInfo::exists(path))
            return;   // клиент уже знает игру (например, загрузку прервали)
        Vdf::Node root;
        root.object = true;
        Vdf::Node &state = root.child(QStringLiteral("AppState"));
        state.set(QStringLiteral("appid"), appId);
        state.set(QStringLiteral("Universe"), QStringLiteral("1"));
        state.set(QStringLiteral("name"), title);
        // Нужно обновление + не установлена: клиент докачает игру при запуске
        state.set(QStringLiteral("StateFlags"), QStringLiteral("1026"));
        state.set(QStringLiteral("installdir"), installDir);
        state.set(QStringLiteral("LastUpdated"), QStringLiteral("0"));
        state.set(QStringLiteral("SizeOnDisk"), QStringLiteral("0"));
        state.set(QStringLiteral("buildid"), QStringLiteral("0"));
        state.set(QStringLiteral("LastOwner"), steamId);
        state.set(QStringLiteral("BytesToDownload"), QStringLiteral("0"));
        state.set(QStringLiteral("BytesDownloaded"), QStringLiteral("0"));
        // Высокий приоритет: качать сразу, а не по расписанию автообновлений
        state.set(QStringLiteral("AutoUpdateBehavior"), QStringLiteral("2"));
        state.set(QStringLiteral("AllowOtherDownloadsWhileRunning"), QStringLiteral("0"));
        state.set(QStringLiteral("ScheduledAutoUpdate"), QStringLiteral("0"));
        state.child(QStringLiteral("UserConfig"));
        state.child(QStringLiteral("MountedConfig"));
        if (!Vdf::writeFile(path, root)) {
            qWarning("Steam: cannot write %s", qPrintable(path));
            if (m_requests.contains(appId))
                m_requests[appId].failed = true;
        }
        qInfo("Steam: queued %s (%s) in %s", qPrintable(appId), qPrintable(title), qPrintable(folder));
    });
}

void SteamClient::cancel(const QString &appId)
{
    const bool installedGame = !installPath(appId).isEmpty();
    const QString title = m_requests.take(appId).title;
    const QString folder = m_manifestDirs.value(appId);
    if (!folder.isEmpty() && !installedGame) {
        // Ещё не установлена: убираем запись клиента и скачанное
        m_removing.insert(appId);
        runOffline([this, folder, appId] {
            m_removing.remove(appId);
            const QString manifest = folder + QStringLiteral("/appmanifest_") + appId + QStringLiteral(".acf");
            const QString dir = Vdf::parseFile(manifest)[QStringLiteral("AppState")].str(QStringLiteral("installdir"));
            QFile::remove(manifest);
            QDir(folder + QStringLiteral("/downloading/") + appId).removeRecursively();
            if (!dir.isEmpty())
                QDir(folder + QStringLiteral("/common/") + dir).removeRecursively();
        });
        scheduleRestart();
    }
    Q_UNUSED(title);
    scanManifests();
    emit downloadsChanged();
}

void SteamClient::uninstall(const QString &appId)
{
    const QString folder = m_manifestDirs.value(appId);
    if (folder.isEmpty())
        return;
    QString title;
    for (const InstalledGame &game : std::as_const(m_installed)) {
        if (game.appId == appId)
            title = game.title;
    }
    m_requests.remove(appId);
    const QString trash = folder + QStringLiteral("/common/.redanchor-removed-") + appId;
    m_removing.insert(appId);
    runOffline([this, folder, appId, trash, title] {
        m_removing.remove(appId);
        const QString manifest = folder + QStringLiteral("/appmanifest_") + appId + QStringLiteral(".acf");
        const QString dir = Vdf::parseFile(manifest)[QStringLiteral("AppState")].str(QStringLiteral("installdir"));
        QFile::remove(manifest);
        QDir(folder + QStringLiteral("/downloading/") + appId).removeRecursively();
        QDir(folder + QStringLiteral("/shadercache/") + appId).removeRecursively();
        // Папку игры (десятки гигабайт) удаляем в фоне, сначала только переименовываем
        if (!dir.isEmpty() && QDir().rename(folder + QStringLiteral("/common/") + dir, trash)) {
            auto *watcher = new QFutureWatcher<void>(this);
            connect(watcher, &QFutureWatcher<void>::finished, watcher, &QObject::deleteLater);
            watcher->setFuture(QtConcurrent::run([trash] { QDir(trash).removeRecursively(); }));
        }
        qInfo("Steam: removed %s", qPrintable(appId));
        scanManifests();
        emit gameRemoved(title);
    });
    // Игра сразу пропадает из списка установленных
    m_installed.erase(std::remove_if(m_installed.begin(), m_installed.end(),
                                     [&](const InstalledGame &g) { return g.appId == appId; }),
                      m_installed.end());
    emit installedChanged();
    scheduleRestart();
}

// ---------- Запуск ----------

void SteamClient::launch(const QString &appId, const QMap<QString, QString> &env,
                         std::function<void(bool, const QString &)> done)
{
    if (m_home.isEmpty() || !signedIn()) {
        done(false, tr("Войдите в Steam: Магазин → Steam."));
        return;
    }
    // Настройки ввода игры — для обёртки запуска
    const QString dir = envDir();
    QDir().mkpath(dir);
    QSaveFile file(dir + QLatin1Char('/') + appId + QStringLiteral(".env"));
    if (file.open(QIODevice::WriteOnly)) {
        file.write("# RedAnchor: окружение игры (пишет оболочка перед запуском)\n");
        for (auto it = env.cbegin(); it != env.cend(); ++it)
            file.write("export " + it.key().toUtf8() + '=' + shellQuote(it.value()).toUtf8() + '\n');
        file.commit();
    }

    const auto run = [this, appId, done](bool ok) {
        if (!ok) {
            done(false, tr("Steam не запустился или не вошёл в аккаунт. Проверьте подключение к интернету."));
            return;
        }
        // -applaunch: режим запуска по умолчанию, без окна выбора режима
        runCommand({QStringLiteral("-applaunch"), appId});
        done(true, QString());
    };
    if (m_online) {
        run(true);
        return;
    }
    if (m_onOnline.isEmpty())
        m_onlineWaitSince = m_clock.elapsed();
    m_onOnline.append(run);
    if (!running() && !m_stopping)
        startClient(Mode::Silent);
}

QList<qint64> SteamClient::gameProcesses(const QString &appId)
{
    QList<qint64> pids;
    if (appId.isEmpty())
        return pids;
    const QByteArray launch = "AppId=" + appId.toLatin1();
    const QByteArray envA = "SteamAppId=" + appId.toLatin1();
    const QByteArray envB = "SteamGameId=" + appId.toLatin1();
    const qint64 self = QCoreApplication::applicationPid();
    for (qint64 pid : allPids()) {
        if (pid == self)
            continue;
        const QByteArray cmdline = readProcFile(pid, "cmdline");
        // reaper SteamLaunch AppId=<id> -- … — живёт, пока идёт игра
        bool match = false;
        for (const QByteArray &arg : cmdline.split('\0')) {
            if (arg == launch) {
                match = true;
                break;
            }
        }
        if (!match) {
            for (const QByteArray &var : readProcFile(pid, "environ").split('\0')) {
                if (var == envA || var == envB) {
                    match = true;
                    break;
                }
            }
        }
        // Сам клиент Steam и его помощники — не игра
        if (match) {
            const QByteArray program = cmdline.left(cmdline.indexOf('\0'));
            const QByteArray name = program.mid(program.lastIndexOf('/') + 1);
            if (name == "steam" || name == "steam.sh" || name == "steamwebhelper" || name == "steamerrorreporter")
                match = false;
        }
        if (match)
            pids.append(pid);
    }
    return pids;
}
