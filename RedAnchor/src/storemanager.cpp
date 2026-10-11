#include "storemanager.h"

#include "accountmanager.h"
#include "gamelibrary.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFutureWatcher>
#include <QJSEngine>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QProcess>
#include <QRegularExpression>
#include <QSet>
#include <QStandardPaths>
#include <QTimer>
#include <QUrlQuery>
#include <QtConcurrent/QtConcurrentRun>

#include <algorithm>
#include <csignal>
#include <memory>

namespace {

const QStringList kStores = {QStringLiteral("gog"), QStringLiteral("epic")};

// Вход GOG, который ожидает gogdl: код приходит в адресе on_login_success?code=…
const QString kGogLogin = QStringLiteral(
    "https://auth.gog.com/auth?client_id=46899977096215655"
    "&redirect_uri=https%3A%2F%2Fembed.gog.com%2Fon_login_success%3Forigin%3Dclient"
    "&response_type=code&layout=galaxy");

// Вход Epic, как у legendary (api/egs.py, get_auth_url): после входа Epic открывает страницу
// id/api/redirect с JSON, в котором есть authorizationCode — его принимает legendary auth --code
const QString kEpicLogin = QStringLiteral(
    "https://www.epicgames.com/id/login?redirectUrl="
    "https%3A//www.epicgames.com/id/api/redirect%3FclientId%3D34a02cf8f4414e29b15921876da36f9a%26responseType%3Dcode");

QNetworkRequest request(const QUrl &url, const QString &token = QString())
{
    QNetworkRequest req(url);
    req.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("RedAnchor-Shell"));
    if (!token.isEmpty())
        req.setRawHeader("Authorization", "Bearer " + token.toLatin1());
    return req;
}

QJsonObject readJson(const QString &path)
{
    QFile file(path);
    return file.open(QIODevice::ReadOnly) ? QJsonDocument::fromJson(file.readAll()).object() : QJsonObject();
}

void writeJson(const QString &path, const QJsonObject &object)
{
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile file(path);
    if (file.open(QIODevice::WriteOnly | QIODevice::Truncate))
        file.write(QJsonDocument(object).toJson());
}

// Последний JSON-объект или массив в выводе (инструменты иногда печатают что-то до него)
QJsonDocument lastJson(const QString &text)
{
    const QJsonDocument whole = QJsonDocument::fromJson(text.toUtf8());
    if (!whole.isNull())
        return whole;
    const QStringList lines = text.split(QLatin1Char('\n'), Qt::SkipEmptyParts);
    for (auto it = lines.crbegin(); it != lines.crend(); ++it) {
        const QJsonDocument doc = QJsonDocument::fromJson(it->trimmed().toUtf8());
        if (!doc.isNull())
            return doc;
    }
    return {};
}

double parseProgress(const QString &line, const QRegularExpression &re)
{
    const QRegularExpressionMatch m = re.match(line);
    return m.hasMatch() ? qBound(0.0, m.captured(1).toDouble() / 100.0, 1.0) : -1;
}

// Все потомки процесса (по PPid из /proc/<pid>/stat), без него самого
QList<qint64> processTree(qint64 root)
{
    QHash<qint64, QList<qint64>> children;
    const QDir proc(QStringLiteral("/proc"));
    for (const QString &entry : proc.entryList(QDir::Dirs | QDir::NoDotAndDotDot)) {
        bool numeric = false;
        const qint64 pid = entry.toLongLong(&numeric);
        if (!numeric)
            continue;
        QFile file(proc.filePath(entry) + QStringLiteral("/stat"));
        if (!file.open(QIODevice::ReadOnly))
            continue;
        // «pid (имя) состояние ppid …» — имя может содержать пробелы и скобки
        const QByteArray stat = file.readAll();
        const QList<QByteArray> fields = stat.mid(stat.lastIndexOf(')') + 2).split(' ');
        if (fields.size() > 1)
            children[fields.at(1).toLongLong()].append(pid);
    }
    QList<qint64> tree;
    QList<qint64> pending{root};
    while (!pending.isEmpty()) {
        for (qint64 child : children.value(pending.takeFirst())) {
            if (!tree.contains(child)) {
                tree.append(child);
                pending.append(child);
            }
        }
    }
    return tree;
}

// Строка журнала legendary («[cli] INFO: …»); вывод --json в журнал не пишем — он огромный
bool isLegendaryLog(const QString &line)
{
    static const QRegularExpression re(QStringLiteral("^\\[\\w+\\] "));
    return re.match(line).hasMatch();
}

// Прогресс загрузки legendary (downloader/mp/manager.py):
// «[DLManager] INFO: = Progress: 12.34% (123/1000), Running for 00:00:10, ETA: 00:01:20» → 0.1234
double legendaryProgress(const QString &line)
{
    static const QRegularExpression re(QStringLiteral("Progress:\\s*([\\d.]+)%"));
    return parseProgress(line, re);
}

// Купленные игры из legendary list --json: [{ app_name, app_title, asset_infos: { Windows: … }, metadata, dlcs }]
// → пары (app_name, название). Без сборки для Windows и игры чужих магазинов (Origin/EA app,
// Ubisoft Connect — у legendary это customAttributes.ThirdPartyManagedApp / ThirdPartyManagedProvider)
// не показываем: legendary их не скачает.
QList<std::pair<QString, QString>> parseEpicLibrary(const QJsonArray &list)
{
    QList<std::pair<QString, QString>> games;
    for (const QJsonValue &value : list) {
        const QJsonObject game = value.toObject();
        const QString appName = game.value(QStringLiteral("app_name")).toString();
        const QString title = game.value(QStringLiteral("app_title")).toString().trimmed();
        const QJsonObject metadata = game.value(QStringLiteral("metadata")).toObject();
        const QJsonObject attributes = metadata.value(QStringLiteral("customAttributes")).toObject();
        const bool thirdParty =
            !attributes.value(QStringLiteral("ThirdPartyManagedApp")).toObject().value(QStringLiteral("value")).toString().isEmpty()
            || !attributes.value(QStringLiteral("ThirdPartyManagedProvider")).toObject().value(QStringLiteral("value")).toString().isEmpty();
        if (appName.isEmpty() || title.isEmpty() || thirdParty || metadata.contains(QStringLiteral("mainGameItem"))
            || !game.value(QStringLiteral("asset_infos")).toObject().contains(QStringLiteral("Windows")))
            continue;
        games.append({appName, title});
    }
    return games;
}

// Игры с вышедшим обновлением: версия в installed.json ({ app_name: { version, platform, … } })
// против последней сборки в assets.json ({ платформа: [{ app_name, build_version, … }] })
QSet<QString> epicUpdates(const QJsonObject &installed, const QJsonObject &assets)
{
    QSet<QString> updates;
    for (auto it = installed.begin(); it != installed.end(); ++it) {
        const QJsonObject game = it.value().toObject();
        const QString platform = game.value(QStringLiteral("platform")).toString(QStringLiteral("Windows"));
        for (const QJsonValue &value : assets.value(platform).toArray()) {
            const QJsonObject asset = value.toObject();
            if (asset.value(QStringLiteral("app_name")).toString() != it.key())
                continue;
            const QString latest = asset.value(QStringLiteral("build_version")).toString();
            if (!latest.isEmpty() && latest != game.value(QStringLiteral("version")).toString())
                updates.insert(it.key());
            break;
        }
    }
    return updates;
}

} // namespace

StoreManager *StoreManager::instance()
{
    static StoreManager *manager = new StoreManager(qApp);
    return manager;
}

StoreManager *StoreManager::create(QQmlEngine *, QJSEngine *)
{
    StoreManager *manager = instance();
    QJSEngine::setObjectOwnership(manager, QJSEngine::CppOwnership);
    return manager;
}

StoreManager::StoreManager(QObject *parent)
    : QObject(parent)
    , m_network(new QNetworkAccessManager(this))
{
    // Сведения о поддержке геймпада общие для всех пользователей (кэш)
    const QJsonObject compat = readJson(compatFile());
    for (auto it = compat.begin(); it != compat.end(); ++it)
        m_compat.insert(it.key(), it.value().toObject().toVariantMap());

    for (const QString &key : kStores) {
        Store s;
        s.key = key;
        m_stores.insert(key, s);
    }
    connect(AccountManager::instance(), &AccountManager::accountChanged, this, [this] {
        // Загрузки шли в папку прежнего пользователя — останавливаем
        cancelAllDownloads();
        reload();
    });
    connect(GameLibrary::instance(), &GameLibrary::gamesChanged, this, &StoreManager::ownedChanged);

    reload();
}

// ---------- Пути и окружение ----------

StoreManager::Store &StoreManager::store(const QString &key)
{
    return m_stores[key];
}

const StoreManager::Store *StoreManager::findStore(const QString &key) const
{
    const auto it = m_stores.constFind(key);
    return it == m_stores.cend() ? nullptr : &it.value();
}

QString StoreManager::toolsDir() const
{
    return QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + QStringLiteral("/tools");
}

QString StoreManager::storeDir(const QString &key) const
{
    const QString profile = AccountManager::instance()->profileDir();
    if (profile.isEmpty())
        return {};
    const QString dir = profile + QStringLiteral("/stores/") + key;
    QDir().mkpath(dir);
    return dir;
}

QString StoreManager::gogdlPath() const { return toolsDir() + QStringLiteral("/gogdl"); }
QString StoreManager::gogAuthFile() const { return storeDir(QStringLiteral("gog")) + QStringLiteral("/auth.json"); }
QString StoreManager::legendaryPath() const { return toolsDir() + QStringLiteral("/legendary"); }
QString StoreManager::legendaryConfigDir() const { return storeDir(QStringLiteral("epic")) + QStringLiteral("/legendary"); }

QString StoreManager::toolPath(const QString &key) const
{
    return key == QLatin1String("epic") ? legendaryPath() : gogdlPath();
}

// У каждого пользователя свои настройки инструментов: отдельные каталоги в профиле
QProcessEnvironment StoreManager::environment(const QString &key) const
{
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    const QString dir = storeDir(key);
    if (key == QLatin1String("gog")) {
        env.insert(QStringLiteral("XDG_CONFIG_HOME"), dir + QStringLiteral("/config"));
    } else if (key == QLatin1String("epic")) {
        // legendary берёт каталог настроек из LEGENDARY_CONFIG_PATH, затем из XDG_CONFIG_HOME
        // (lfs/lgndry.py): вход, installed.json и метаданные — в профиле пользователя
        env.insert(QStringLiteral("LEGENDARY_CONFIG_PATH"), legendaryConfigDir());
        env.insert(QStringLiteral("XDG_CONFIG_HOME"), dir + QStringLiteral("/config"));
    }
    return env;
}


// ---------- Процессы и загрузки файлов ----------

QProcess *StoreManager::run(const QString &program, const QStringList &args, const QProcessEnvironment &env,
                            LineHandler onLine, DoneHandler onDone, bool pty)
{
    auto *proc = new QProcess(this);
    proc->setProcessEnvironment(env);
    auto out = std::make_shared<QString>();
    auto outLine = std::make_shared<QString>();
    auto errLine = std::make_shared<QString>();
    static const QRegularExpression lineBreak(QStringLiteral("[\r\n]"));
    // Управляющие последовательности терминала (цвет и т. п.) и прочие служебные символы.
    // Пробелы по краям оставляем: из них состоит QR-код.
    static const QRegularExpression ansi(QStringLiteral("\\x1B(?:\\[[0-?]*[ -/]*[@-~]|[@-_])|[\\x00-\\x08\\x0B-\\x1F\\x7F]"));
    const auto clean = [](QString text) { return text.remove(ansi); };

    // Строки прогресса часто разделяются \r — режем и по нему
    const auto split = [onLine, clean](QString *buffer, const QString &chunk) {
        buffer->append(chunk);
        qsizetype pos;
        while ((pos = buffer->indexOf(lineBreak)) >= 0) {
            const QString line = clean(buffer->left(pos));
            buffer->remove(0, pos + 1);
            // Строки из одних пробелов тоже отдаём: в QR-коде это светлые ряды модулей
            if (!line.isEmpty() && onLine)
                onLine(line);
        }
        // Запрос ввода («…auth code: ») приходит без перевода строки — отдаём его сразу.
        // Смотрим только на конец буфера: legendary list --json выводит мегабайты одной строкой
        if (onLine && clean(buffer->right(256)).trimmed().endsWith(QLatin1Char(':'))) {
            onLine(clean(*buffer).trimmed());
            buffer->clear();
        }
    };
    connect(proc, &QProcess::readyReadStandardOutput, this, [proc, out, outLine, split] {
        const QString chunk = QString::fromUtf8(proc->readAllStandardOutput());
        out->append(chunk);
        split(outLine.get(), chunk);
    });
    connect(proc, &QProcess::readyReadStandardError, this, [proc, errLine, split] {
        split(errLine.get(), QString::fromUtf8(proc->readAllStandardError()));
    });
    connect(proc, &QProcess::finished, this, [proc, out, outLine, errLine, onLine, onDone, clean](int code, QProcess::ExitStatus status) {
        for (const auto &rest : {outLine, errLine}) {
            const QString line = clean(*rest);
            if (!line.trimmed().isEmpty() && onLine)
                onLine(line);
        }
        proc->deleteLater();
        if (onDone)
            onDone(status == QProcess::NormalExit ? code : -1, *out);
    });
    connect(proc, &QProcess::errorOccurred, this, [proc, program, onDone](QProcess::ProcessError error) {
        if (error != QProcess::FailedToStart)
            return;
        qWarning("Failed to start %s", qPrintable(program));
        proc->deleteLater();
        if (onDone)
            onDone(-2, QString());
    });

    const QString script = pty ? QStandardPaths::findExecutable(QStringLiteral("script")) : QString();
    if (script.isEmpty()) {
        proc->start(program, args);
    } else {
        const auto quote = [](QString value) -> QString {
            return QLatin1Char('\'') + value.replace(QLatin1Char('\''), QStringLiteral("'\\''")) + QLatin1Char('\'');
        };
        QStringList command{quote(program)};
        for (const QString &arg : args)
            command.append(quote(arg));
        proc->start(script, {QStringLiteral("-q"), QStringLiteral("-f"), QStringLiteral("-e"), QStringLiteral("-c"),
                             command.join(QLatin1Char(' ')), QStringLiteral("/dev/null")});
    }
    return proc;
}

void StoreManager::downloadFile(const QUrl &url, const QString &path, std::function<void(double)> onProgress,
                                std::function<void(bool, const QString &)> onDone)
{
    QDir().mkpath(QFileInfo(path).absolutePath());
    auto *file = new QFile(path + QStringLiteral(".part"), this);
    if (!file->open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        file->deleteLater();
        onDone(false, tr("Не удалось сохранить файл."));
        return;
    }
    QNetworkReply *reply = m_network->get(request(url));
    connect(reply, &QNetworkReply::readyRead, this, [reply, file] { file->write(reply->readAll()); });
    connect(reply, &QNetworkReply::downloadProgress, this, [onProgress](qint64 received, qint64 total) {
        if (onProgress)
            onProgress(total > 0 ? double(received) / double(total) : -1);
    });
    connect(reply, &QNetworkReply::finished, this, [reply, file, path, onDone] {
        file->write(reply->readAll());
        file->close();
        const bool ok = reply->error() == QNetworkReply::NoError;
        reply->deleteLater();
        file->deleteLater();
        if (!ok) {
            QFile::remove(file->fileName());
            onDone(false, tr("Загрузка прервалась. Проверьте подключение к интернету."));
            return;
        }
        QFile::remove(path);
        QFile::rename(file->fileName(), path);
        onDone(true, QString());
    });
}

// ---------- Состояние для QML ----------

QVariantMap StoreManager::stores() const
{
    QVariantMap map;
    for (const QString &key : kStores) {
        const Store *s = findStore(key);
        map.insert(key, QVariantMap{
            {QStringLiteral("toolInstalled"), s->toolInstalled},
            {QStringLiteral("tool"), key == QLatin1String("epic") ? QStringLiteral("legendary") : QStringLiteral("gogdl")},
            {QStringLiteral("signedIn"), s->signedIn},
            {QStringLiteral("account"), s->account},
            {QStringLiteral("avatar"), QString()},
            {QStringLiteral("busy"), s->busy},
            {QStringLiteral("message"), s->message},
            {QStringLiteral("listTitle"), s->listTitle},
        });
    }
    return map;
}

QVariantMap StoreManager::owned() const
{
    GameLibrary *library = GameLibrary::instance();
    QVariantMap map;
    for (const QString &key : kStores) {
        const Store *s = findStore(key);
        QList<OwnedGame> games = s->owned;
        std::sort(games.begin(), games.end(), [](const OwnedGame &a, const OwnedGame &b) {
            return a.title.compare(b.title, Qt::CaseInsensitive) < 0;
        });
        QVariantList list;
        for (const OwnedGame &g : std::as_const(games)) {
            const QString id = key + QLatin1Char(':') + g.appId;
            QString status;
            double progress = -1;
            for (const Download &d : m_downloads) {
                if (d.id() == id) {
                    status = d.status;
                    progress = d.progress;
                }
            }
            list.append(QVariantMap{
                {QStringLiteral("gameId"), id},
                {QStringLiteral("appId"), g.appId},
                {QStringLiteral("title"), g.title},
                {QStringLiteral("installed"), library->isInstalled(id)},
                {QStringLiteral("updateAvailable"), s->updates.contains(g.appId) && library->isInstalled(id)},
                {QStringLiteral("status"), status},
                {QStringLiteral("progress"), progress},
            });
        }
        map.insert(key, list);
    }
    return map;
}

QVariantList StoreManager::downloads() const
{
    QVariantList list;
    for (const Download &d : m_downloads) {
        list.append(QVariantMap{
            {QStringLiteral("id"), d.id()},
            {QStringLiteral("store"), d.store},
            {QStringLiteral("appId"), d.appId},
            {QStringLiteral("title"), d.title},
            {QStringLiteral("status"), d.status},
            {QStringLiteral("progress"), d.progress},
            {QStringLiteral("message"), d.message},
        });
    }
    return list;
}

int StoreManager::activeDownloads() const
{
    const auto active = [](const QString &status) {
        return status == QLatin1String("queued") || status == QLatin1String("downloading");
    };
    int count = int(std::count_if(m_downloads.cbegin(), m_downloads.cend(), [&](const Download &d) { return active(d.status); }));
    return count;
}

double StoreManager::downloadProgress() const
{
    for (const Download &d : m_downloads) {
        if (d.id() == m_activeDownload)
            return d.progress;
    }
    return -1;
}

QString StoreManager::downloadTitle() const
{
    for (const Download &d : m_downloads) {
        if (d.id() == m_activeDownload)
            return d.title;
    }
    return {};
}

void StoreManager::setBusy(const QString &key, bool busy, const QString &message)
{
    Store &s = store(key);
    s.busy = busy;
    s.message = message;
    emit storesChanged();
}

// ---------- Загрузка состояния пользователя ----------

void StoreManager::reload()
{
    const QString profile = AccountManager::instance()->profileDir();
    for (const QString &key : kStores) {
        Store &s = store(key);
        s.busy = false;
        s.message.clear();
        s.signedIn = false;
        s.account.clear();
        s.owned.clear();
        s.updates.clear();
        s.toolInstalled = QFileInfo(toolPath(key)).isExecutable();
    }

    if (!profile.isEmpty()) {
        // GOG: файл входа gogdl и имя пользователя
        Store &gog = store(QStringLiteral("gog"));
        if (QFileInfo::exists(gogAuthFile())) {
            gog.signedIn = true;
            gog.account = readJson(storeDir(QStringLiteral("gog")) + QStringLiteral("/account.json"))
                              .value(QStringLiteral("username")).toString();
        }
        // Epic: вход legendary (user.json) — в нём и имя аккаунта
        Store &epic = store(QStringLiteral("epic"));
        const QJsonObject user = readJson(legendaryConfigDir() + QStringLiteral("/user.json"));
        if (!user.isEmpty()) {
            epic.signedIn = true;
            epic.account = user.value(QStringLiteral("displayName")).toString();
        }
        for (const QString &key : kStores)
            loadOwned(key);
        loadEpicUpdates();
    }
    emit storesChanged();
    emit ownedChanged();
}

void StoreManager::loadOwned(const QString &key)
{
    Store &s = store(key);
    s.owned.clear();
    const QJsonObject cache = readJson(storeDir(key) + QStringLiteral("/owned.json"));
    s.listTitle = cache.value(QStringLiteral("title")).toString();
    for (const QJsonValue &value : cache.value(QStringLiteral("games")).toArray()) {
        const QJsonObject o = value.toObject();
        s.owned.append(OwnedGame{o.value(QStringLiteral("appId")).toString(), o.value(QStringLiteral("title")).toString(),
                                 o.value(QStringLiteral("type")).toString()});
    }
}

void StoreManager::saveOwned(const QString &key) const
{
    const Store *s = findStore(key);
    QJsonArray array;
    for (const OwnedGame &g : s->owned) {
        array.append(QJsonObject{{QStringLiteral("appId"), g.appId}, {QStringLiteral("title"), g.title},
                                 {QStringLiteral("type"), g.type}});
    }
    writeJson(storeDir(key) + QStringLiteral("/owned.json"),
              QJsonObject{{QStringLiteral("title"), s->listTitle}, {QStringLiteral("games"), array}});
}

// ---------- Инструменты ----------
// Сами инструменты ставит и обновляет ComponentsManager («Настройки → Обновления»)

void StoreManager::refreshTools()
{
    for (const QString &key : kStores)
        store(key).toolInstalled = QFileInfo(toolPath(key)).isExecutable();
    emit storesChanged();
}

// ---------- Вход ----------

QString StoreManager::loginUrl(const QString &key) const
{
    if (key == QLatin1String("gog"))
        return kGogLogin;
    if (key == QLatin1String("epic"))
        return kEpicLogin;
    return {};
}

void StoreManager::completeWebLogin(const QString &key, const QString &code)
{
    if (code.trimmed().isEmpty())
        return;
    if (key == QLatin1String("epic")) {
        // legendary auth --code: код обменивается на вход, он сохраняется в user.json
        setBusy(key, true, tr("Вход…"));
        run(legendaryPath(), {QStringLiteral("auth"), QStringLiteral("--code"), code.trimmed()}, environment(key),
            [](const QString &line) {
                if (!line.trimmed().isEmpty())
                    qInfo("legendary: %s", qPrintable(line.trimmed()));
            },
            [this](int exitCode, const QString &) {
                // При неудаче legendary пишет ошибку, но выходит с кодом 0 — смотрим на сам вход
                const QString name = readJson(legendaryConfigDir() + QStringLiteral("/user.json"))
                                         .value(QStringLiteral("displayName")).toString();
                if (exitCode != 0 || name.isEmpty()) {
                    setBusy(QStringLiteral("epic"), false, tr("Не удалось войти в Epic Games. Попробуйте ещё раз."));
                    return;
                }
                Store &epic = store(QStringLiteral("epic"));
                epic.signedIn = true;
                epic.account = name;
                setBusy(QStringLiteral("epic"), false);
                refresh(QStringLiteral("epic"));
            });
        return;
    }
    if (key != QLatin1String("gog"))
        return;
    setBusy(key, true, tr("Вход…"));
    run(gogdlPath(), {QStringLiteral("--auth-config-path"), gogAuthFile(), QStringLiteral("auth"), QStringLiteral("--code"), code.trimmed()},
        environment(key), [](const QString &line) {
            if (!line.trimmed().isEmpty())
                qInfo("gogdl: %s", qPrintable(line.trimmed()));
        },
        [this](int exitCode, const QString &) {
            if (exitCode != 0 || !QFileInfo::exists(gogAuthFile())) {
                QFile::remove(gogAuthFile());
                setBusy(QStringLiteral("gog"), false, tr("Не удалось войти в GOG. Попробуйте ещё раз."));
                return;
            }
            // Имя пользователя — из профиля GOG
            gogToken([this](const QString &token) {
                QNetworkReply *reply = m_network->get(request(QUrl(QStringLiteral("https://embed.gog.com/userData.json")), token));
                connect(reply, &QNetworkReply::finished, this, [this, reply] {
                    reply->deleteLater();
                    const QString username = QJsonDocument::fromJson(reply->readAll()).object().value(QStringLiteral("username")).toString();
                    writeJson(storeDir(QStringLiteral("gog")) + QStringLiteral("/account.json"),
                              QJsonObject{{QStringLiteral("username"), username}});
                    Store &gog = store(QStringLiteral("gog"));
                    gog.signedIn = true;
                    gog.account = username;
                    setBusy(QStringLiteral("gog"), false);
                    refresh(QStringLiteral("gog"));
                });
            });
        });
}

void StoreManager::signOut(const QString &key)
{
    const QString dir = storeDir(key);
    if (dir.isEmpty())
        return;
    if (key == QLatin1String("epic")) {
        // Вход legendary — user.json (то же удаляет legendary auth --delete). Список установленных
        // игр (installed.json) остаётся: игры не удаляются
        QFile::remove(legendaryConfigDir() + QStringLiteral("/user.json"));
    } else {
        QFile::remove(gogAuthFile());
        QFile::remove(dir + QStringLiteral("/account.json"));
    }
    QFile::remove(dir + QStringLiteral("/owned.json"));
    reload();
}

// ---------- Списки игр ----------

void StoreManager::refresh(const QString &key)
{
    const Store &s = store(key);
    if (!s.signedIn || s.busy || !s.toolInstalled)
        return;
    if (key == QLatin1String("epic"))
        refreshEpic();
    else
        refreshGog();
}

void StoreManager::gogToken(std::function<void(const QString &)> onToken)
{
    // gogdl сам обновляет просроченный токен и печатает текущие данные входа
    run(gogdlPath(), {QStringLiteral("--auth-config-path"), gogAuthFile(), QStringLiteral("auth")}, environment(QStringLiteral("gog")),
        {}, [onToken](int, const QString &out) {
            onToken(lastJson(out).object().value(QStringLiteral("access_token")).toString());
        });
}
void StoreManager::refreshGog()
{
    setBusy(QStringLiteral("gog"), true, tr("Загрузка списка игр…"));
    gogToken([this](const QString &token) {
        if (token.isEmpty()) {
            setBusy(QStringLiteral("gog"), false, tr("Вход в GOG устарел. Войдите снова."));
            return;
        }
        QNetworkReply *reply = m_network->get(request(QUrl(QStringLiteral("https://embed.gog.com/user/data/games")), token));
        connect(reply, &QNetworkReply::finished, this, [this, reply] {
            reply->deleteLater();
            if (reply->error() != QNetworkReply::NoError) {
                setBusy(QStringLiteral("gog"), false, tr("Не удалось получить список игр GOG."));
                return;
            }
            QStringList ids;
            for (const QJsonValue &value : QJsonDocument::fromJson(reply->readAll()).object().value(QStringLiteral("owned")).toArray())
                ids.append(QString::number(qint64(value.toDouble())));
            // Названия уже известных игр берём из кэша, остальные запрашиваем
            Store &gog = store(QStringLiteral("gog"));
            QList<OwnedGame> kept;
            QStringList missing;
            for (const QString &id : std::as_const(ids)) {
                const auto it = std::find_if(gog.owned.cbegin(), gog.owned.cend(), [&](const OwnedGame &g) { return g.appId == id; });
                if (it != gog.owned.cend())
                    kept.append(*it);
                else
                    missing.append(id);
            }
            gog.owned = kept;
            emit ownedChanged();
            fetchGogProducts(missing, 0);
        });
    });
}
void StoreManager::fetchGogProducts(QStringList ids, int index)
{
    if (index >= ids.size()) {
        saveOwned(QStringLiteral("gog"));
        setBusy(QStringLiteral("gog"), false);
        emit ownedChanged();
        return;
    }
    QNetworkReply *reply = m_network->get(request(QUrl(QStringLiteral("https://api.gog.com/products/") + ids.at(index))));
    connect(reply, &QNetworkReply::finished, this, [this, reply, ids, index] {
        reply->deleteLater();
        const QJsonObject product = QJsonDocument::fromJson(reply->readAll()).object();
        const QString title = product.value(QStringLiteral("title")).toString();
        const QString type = product.value(QStringLiteral("game_type")).toString();
        // Дополнения и наборы в список игр не попадают
        if (!title.isEmpty() && (type.isEmpty() || type == QLatin1String("game"))) {
            store(QStringLiteral("gog")).owned.append(OwnedGame{ids.at(index), title, QStringLiteral("game")});
            if (index % 10 == 9)
                emit ownedChanged();
        }
        fetchGogProducts(ids, index + 1);
    });
}

// Купленные игры Epic: legendary list --json (он сам обновляет вход и метаданные).
// В списке только игры (дополнения legendary кладёт в dlcs, движок Unreal и ассеты — пропускает).
void StoreManager::refreshEpic()
{
    setBusy(QStringLiteral("epic"), true, tr("Загрузка списка игр…"));
    auto loginFailed = std::make_shared<bool>(false);
    run(legendaryPath(), {QStringLiteral("list"), QStringLiteral("--json")}, environment(QStringLiteral("epic")),
        [loginFailed](const QString &line) {
            if (!isLegendaryLog(line))
                return;
            qInfo("legendary: %s", qPrintable(line.trimmed()));
            if (line.contains(QLatin1String("Login failed")))
                *loginFailed = true;
        },
        [this, loginFailed](int exitCode, const QString &out) {
            const QJsonDocument doc = lastJson(out);
            if (exitCode != 0 || !doc.isArray()) {
                setBusy(QStringLiteral("epic"), false, *loginFailed ? tr("Вход в Epic Games устарел. Войдите снова.")
                                                                     : tr("Не удалось получить список игр Epic Games."));
                return;
            }
            QList<OwnedGame> games;
            for (const auto &[appName, title] : parseEpicLibrary(doc.array()))
                games.append(OwnedGame{appName, title, QStringLiteral("game")});
            Store &epic = store(QStringLiteral("epic"));
            epic.owned = games;
            epic.listTitle = tr("Библиотека Epic Games");
            saveOwned(QStringLiteral("epic"));
            // legendary list заодно обновил сведения о сборках — по ним видно, вышли ли обновления
            loadEpicUpdates();
            syncEpicLibrary();
            setBusy(QStringLiteral("epic"), false);
            emit ownedChanged();
        });
}

// Обновления установленных игр Epic: версия в installed.json против последней сборки
// в assets.json (их обновляют legendary list и launch)
void StoreManager::loadEpicUpdates()
{
    store(QStringLiteral("epic")).updates = epicUpdates(readJson(legendaryConfigDir() + QStringLiteral("/installed.json")),
                                                        readJson(legendaryConfigDir() + QStringLiteral("/assets.json")));
}

// Установленные игры Epic ведёт legendary (installed.json): библиотека сверяется с ним —
// например, если загрузка закончилась, когда оболочка была закрыта
void StoreManager::syncEpicLibrary()
{
    QList<GameLibrary::StoreGame> installed;
    const QJsonObject list = readJson(legendaryConfigDir() + QStringLiteral("/installed.json"));
    for (auto it = list.begin(); it != list.end(); ++it) {
        const QJsonObject game = it.value().toObject();
        const QString path = game.value(QStringLiteral("install_path")).toString();
        if (game.value(QStringLiteral("is_dlc")).toBool() || path.isEmpty() || !QFileInfo(path).isDir())
            continue;
        installed.append({it.key(), game.value(QStringLiteral("title")).toString(), path,
                          qint64(game.value(QStringLiteral("install_size")).toDouble())});
    }
    QStringList pending;
    for (const Download &d : std::as_const(m_downloads)) {
        if (d.store == QLatin1String("epic"))
            pending.append(d.appId);
    }
    GameLibrary::instance()->syncStore(QStringLiteral("epic"), installed, pending);
}


// ---------- Загрузки ----------

StoreManager::Download *StoreManager::findDownload(const QString &id)
{
    for (Download &d : m_downloads) {
        if (d.id() == id)
            return &d;
    }
    return nullptr;
}

void StoreManager::install(const QString &key, const QString &appId, const QString &title)
{
    const QString id = key + QLatin1Char(':') + appId;
    GameLibrary *library = GameLibrary::instance();
    if (library->isInstalled(id))
        return;
    if (!store(key).signedIn)
        return;
    if (Download *existing = findDownload(id)) {
        if (existing->status != QLatin1String("failed"))
            return;
        existing->status = QStringLiteral("queued");   // повтор после ошибки
        existing->message.clear();
    } else {
        Download added;
        added.store = key;
        added.appId = appId;
        added.title = title;
        m_downloads.append(added);
    }
    // Игра сразу появляется в библиотеке — её уже можно закрепить на главном экране
    library->addPending(key, appId, title);
    emit downloadsChanged();
    emit ownedChanged();
    processDownloads();
}

void StoreManager::update(const QString &gameId)
{
    const QVariantMap game = GameLibrary::instance()->game(gameId);
    const QString key = game.value(QStringLiteral("store")).toString();
    // Обновления сам ставит только legendary; gogdl обновляет игру при повторной загрузке
    if (!game.value(QStringLiteral("installed")).toBool() || key != QLatin1String("epic") || !store(key).signedIn)
        return;
    if (Download *existing = findDownload(gameId)) {
        if (existing->status != QLatin1String("failed"))
            return;
        existing->status = QStringLiteral("queued");
        existing->message.clear();
        existing->update = true;
    } else {
        Download added;
        added.store = key;
        added.appId = game.value(QStringLiteral("appId")).toString();
        added.title = game.value(QStringLiteral("title")).toString();
        added.update = true;
        m_downloads.append(added);
    }
    emit downloadsChanged();
    emit ownedChanged();
    processDownloads();
}

void StoreManager::processDownloads()
{
    if (!m_activeDownload.isEmpty())
        return;
    Download *next = nullptr;
    for (Download &d : m_downloads) {
        if (d.status == QLatin1String("queued")) {
            next = &d;
            break;
        }
    }
    if (!next)
        return;

    next->status = QStringLiteral("downloading");
    next->progress = -1;
    next->message = tr("Подготовка…");
    m_activeDownload = next->id();
    const QString key = next->store;
    const QString appId = next->appId;
    const QString title = next->title;
    const bool update = next->update;
    emit downloadsChanged();
    emit ownedChanged();

    if (key == QLatin1String("gog"))
        startGogDownload(appId, title);
    else if (key == QLatin1String("epic"))
        startEpicDownload(appId, title, update);
    else
        finishDownload(false, tr("Загрузка игр этого магазина не поддерживается."));
}

// Прогресс текущей загрузки (p < 0 — без изменений)
void StoreManager::setDownloadProgress(double p, const QString &message)
{
    if (Download *d = findDownload(m_activeDownload)) {
        if (p >= 0)
            d->progress = p;
        if (!message.isEmpty())
            d->message = message;
        emit downloadsChanged();
        emit ownedChanged();
    }
}

void StoreManager::startGogDownload(const QString &appId, const QString &title)
{
    const QString base = GameLibrary::instance()->gamesDir(QStringLiteral("gog"));
    const QStringList before = QDir(base).entryList(QDir::Dirs | QDir::NoDotAndDotDot);
    static const QRegularExpression re(QStringLiteral("Progress:\\s*([\\d.]+)"));
    QProcess *proc = run(gogdlPath(),
        {QStringLiteral("--auth-config-path"), gogAuthFile(), QStringLiteral("download"), appId,
         QStringLiteral("--platform"), QStringLiteral("windows"), QStringLiteral("--path"), base,
         QStringLiteral("--skip-dlcs"), QStringLiteral("--lang"), QStringLiteral("en-US")},
        environment(QStringLiteral("gog")),
        [this](const QString &line) {
            const double p = parseProgress(line, re);
            if (p >= 0)
                setDownloadProgress(p, tr("Загрузка: %1%").arg(qRound(p * 100)));
        },
        [this, base, before, appId, title](int exitCode, const QString &) {
            m_activeProcess = nullptr;
            // Папка игры — та, что появилась после загрузки
            QString path;
            for (const QString &name : QDir(base).entryList(QDir::Dirs | QDir::NoDotAndDotDot)) {
                if (!before.contains(name))
                    path = base + QLatin1Char('/') + name;
            }
            if (exitCode != 0 || path.isEmpty()) {
                finishDownload(false, tr("Не удалось установить игру."));
                return;
            }
            GameLibrary::instance()->addInstalled(QStringLiteral("gog"), appId, title, path, 0);
            finishDownload(true);
        });
    if (proc->state() != QProcess::NotRunning)
        m_activeProcess = proc;
}

// Epic: legendary install в папку игр пользователя (папку игры называет сам legendary).
// -y — без вопросов, --skip-sdl — без выбора частей игры (ставится всё нужное), --skip-dlcs — без
// дополнений. Прогресс legendary пишет в журнал: «[DLManager] INFO: = Progress: 12.34% (…), …».
// Загрузку можно прервать: повтор той же командой продолжит её с того же места.
void StoreManager::startEpicDownload(const QString &appId, const QString &title, bool update)
{
    QStringList args{QStringLiteral("-y"), QStringLiteral("install"), appId,
                     QStringLiteral("--base-path"), GameLibrary::instance()->gamesDir(QStringLiteral("epic")),
                     QStringLiteral("--platform"), QStringLiteral("Windows"),
                     QStringLiteral("--skip-sdl"), QStringLiteral("--skip-dlcs")};
    if (update)
        args.append(QStringLiteral("--update-only"));
    auto failure = std::make_shared<QString>();
    QProcess *proc = run(legendaryPath(), args, environment(QStringLiteral("epic")),
        [this, failure, update](const QString &raw) {
            const QString line = raw.trimmed();
            const double p = legendaryProgress(line);
            if (p >= 0) {
                setDownloadProgress(p, (update ? tr("Обновление: %1%") : tr("Загрузка: %1%")).arg(qRound(p * 100)));
                return;
            }
            if (line.isEmpty())
                return;
            qInfo("legendary: %s", qPrintable(line));
            if (line.contains(QLatin1String("Login failed"))) {
                *failure = tr("Вход в Epic Games устарел. Выйдите и войдите снова.");
            } else if (line.contains(QLatin1String("installed via a third-party store"))) {
                *failure = tr("Эту игру устанавливает другой магазин — Epic Games её не скачает.");
            } else if (line.startsWith(QLatin1String("! Failure:"))) {
                // Проверка перед установкой не прошла (например, не хватает места на диске)
                *failure = tr("Установка невозможна: %1").arg(line.mid(10).trimmed());
            } else if (line.contains(QLatin1String("Installation failed after"))) {
                *failure = tr("Загрузка прервалась. Повторите — она продолжится с того же места.");
            }
        },
        [this, appId, title, failure](int exitCode, const QString &) {
            m_activeProcess = nullptr;
            // Итог — по installed.json: при части ошибок legendary выходит с кодом 0
            const QJsonObject game = readJson(legendaryConfigDir() + QStringLiteral("/installed.json")).value(appId).toObject();
            const QString path = game.value(QStringLiteral("install_path")).toString();
            if (exitCode != 0 || !failure->isEmpty() || path.isEmpty() || !QFileInfo(path).isDir()) {
                finishDownload(false, failure->isEmpty() ? tr("Не удалось установить игру.") : *failure);
                return;
            }
            store(QStringLiteral("epic")).updates.remove(appId);
            GameLibrary::instance()->addInstalled(QStringLiteral("epic"), appId, title, path,
                                                  qint64(game.value(QStringLiteral("install_size")).toDouble()));
            finishDownload(true);
        });
    if (proc->state() != QProcess::NotRunning)
        m_activeProcess = proc;
}

void StoreManager::finishDownload(bool ok, const QString &message)
{
    const QString id = m_activeDownload;
    m_activeDownload.clear();
    for (int i = 0; i < m_downloads.size(); ++i) {
        if (m_downloads.at(i).id() != id)
            continue;
        if (ok) {
            emit gameInstalled(m_downloads.at(i).title);
            m_downloads.removeAt(i);
        } else if (m_downloads.at(i).status == QLatin1String("cancelled")) {
            m_downloads.removeAt(i);
            if (!GameLibrary::instance()->isInstalled(id))
                GameLibrary::instance()->removeRecord(id);
        } else {
            m_downloads[i].status = QStringLiteral("failed");
            m_downloads[i].progress = -1;
            m_downloads[i].message = message;
        }
        break;
    }
    emit downloadsChanged();
    emit ownedChanged();
    processDownloads();
}

// Остановить загрузчик вместе с его рабочими процессами (legendary качает в отдельных
// процессах multiprocessing — без родителя они продолжили бы загрузку)
void StoreManager::stopActiveProcess()
{
    if (!m_activeProcess)
        return;
    for (qint64 pid : processTree(m_activeProcess->processId()))
        ::kill(pid_t(pid), SIGKILL);
    m_activeProcess->kill();
}

void StoreManager::cancel(const QString &gameId)
{
    if (gameId == m_activeDownload && m_activeProcess) {
        if (Download *d = findDownload(gameId))
            d->status = QStringLiteral("cancelled");   // finishDownload уберёт её из списка
        stopActiveProcess();
        m_activeProcess = nullptr;
    } else {
        for (int i = 0; i < m_downloads.size(); ++i) {
            if (m_downloads.at(i).id() == gameId) {
                m_downloads.removeAt(i);
                break;
            }
        }
        // Незавершённая игра уходит и из библиотеки
        if (!GameLibrary::instance()->isInstalled(gameId))
            GameLibrary::instance()->removeRecord(gameId);
    }
    emit downloadsChanged();
    emit ownedChanged();
}

void StoreManager::cancelAllDownloads()
{
    if (m_activeProcess) {
        m_activeProcess->disconnect(this);
        stopActiveProcess();
        m_activeProcess->deleteLater();
        m_activeProcess = nullptr;
    }
    m_activeDownload.clear();
    m_downloads.clear();
    emit downloadsChanged();
}

void StoreManager::uninstall(const QString &gameId)
{
    GameLibrary *library = GameLibrary::instance();
    const QVariantMap game = library->game(gameId);
    if (game.isEmpty())
        return;
    // Загружается — сначала останавливаем загрузку
    if (findDownload(gameId)) {
        cancel(gameId);
        if (!game.value(QStringLiteral("installed")).toBool())
            return;
    }
    const QString title = game.value(QStringLiteral("title")).toString();
    const QString path = game.value(QStringLiteral("installPath")).toString();

    // Папку удаляем в фоне: у больших игр это может занять время
    const auto removeFolder = [this, path, title] {
        auto *watcher = new QFutureWatcher<bool>(this);
        connect(watcher, &QFutureWatcher<bool>::finished, this, [this, watcher, title] {
            watcher->deleteLater();
            GameLibrary::instance()->refreshStorage();
            emit gameRemoved(title);
        });
        watcher->setFuture(QtConcurrent::run([path] { return path.isEmpty() || !QFileInfo::exists(path) || QDir(path).removeRecursively(); }));
    };
    library->removeRecord(gameId);

    // Epic: удаляет legendary — иначе игра останется в его installed.json. Что он не удалил — дочищаем
    if (game.value(QStringLiteral("store")).toString() == QLatin1String("epic") && QFileInfo(legendaryPath()).isExecutable()) {
        const QString appId = game.value(QStringLiteral("appId")).toString();
        store(QStringLiteral("epic")).updates.remove(appId);
        run(legendaryPath(), {QStringLiteral("-y"), QStringLiteral("uninstall"), appId}, environment(QStringLiteral("epic")),
            [](const QString &line) {
                if (!line.trimmed().isEmpty())
                    qInfo("legendary: %s", qPrintable(line.trimmed()));
            },
            [removeFolder](int, const QString &) { removeFolder(); });
        return;
    }
    removeFolder();
}

// ---------- Поддержка геймпада ----------

QString StoreManager::compatFile() const
{
    return QStandardPaths::writableLocation(QStandardPaths::CacheLocation) + QStringLiteral("/compatibility.json");
}

void StoreManager::saveCompatibility() const
{
    QJsonObject object;
    for (auto it = m_compat.cbegin(); it != m_compat.cend(); ++it)
        object.insert(it.key(), QJsonObject::fromVariantMap(it.value()));
    writeJson(compatFile(), object);
}

QVariantMap StoreManager::compatibility(const QString &key, const QString &appId)
{
    const QString id = key + QLatin1Char(':') + appId;
    // Epic: открытого источника сведений о поддержке геймпада нет (в данных, которые получает
    // legendary, такого поля нет) — честно говорим, что неизвестно, и ничего не придумываем
    if (key == QLatin1String("epic")) {
        return {{QStringLiteral("checked"), true}, {QStringLiteral("failed"), true}, {QStringLiteral("unknown"), true},
                {QStringLiteral("controller"), false}, {QStringLiteral("ps5"), false}, {QStringLiteral("ps4"), false}};
    }
    const auto cached = m_compat.constFind(id);
    if (cached != m_compat.cend())
        return cached.value();
    if (m_compatPending.contains(id) || appId.isEmpty())
        return {{QStringLiteral("checked"), false}};
    m_compatPending.insert(id);

    // GOG — список возможностей игры
    const QUrl url(QStringLiteral("https://api.gog.com/v2/games/%1?locale=en-US").arg(appId));
    QNetworkReply *reply = m_network->get(request(url));
    connect(reply, &QNetworkReply::finished, this, [this, reply, id] {
        reply->deleteLater();
        m_compatPending.remove(id);
        const QJsonObject root = QJsonDocument::fromJson(reply->readAll()).object();
        QVariantMap info{{QStringLiteral("checked"), true}};
        bool ok = reply->error() == QNetworkReply::NoError;
        bool controller = false, ps5 = false, ps4 = false;
        if (ok) {
            for (const QJsonValue &value : root.value(QStringLiteral("_embedded")).toObject().value(QStringLiteral("features")).toArray()) {
                if (value.toObject().value(QStringLiteral("name")).toString().contains(QLatin1String("controller"), Qt::CaseInsensitive))
                    controller = true;
            }
        }
        info.insert(QStringLiteral("failed"), !ok);
        info.insert(QStringLiteral("controller"), controller || ps5 || ps4);
        info.insert(QStringLiteral("ps5"), ps5);
        info.insert(QStringLiteral("ps4"), ps4);
        // Неудачную проверку не запоминаем — попробуем снова в следующий раз
        if (ok) {
            m_compat.insert(id, info);
            saveCompatibility();
        } else {
            m_compat.remove(id);
        }
        emit compatibilityChanged(id);
        if (!ok)
            m_compat.insert(id, info);   // до перезапуска не дёргаем сеть повторно
    });
    return {{QStringLiteral("checked"), false}};
}
