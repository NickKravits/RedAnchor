#include "storemanager.h"

#include "accountmanager.h"
#include "gamelibrary.h"
#include "steamclient.h"

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
#include <memory>

namespace {

const QStringList kStores = {QStringLiteral("steam"), QStringLiteral("gog")};

// Вход GOG, который ожидает gogdl: код приходит в адресе on_login_success?code=…
const QString kGogLogin = QStringLiteral(
    "https://auth.gog.com/auth?client_id=46899977096215655"
    "&redirect_uri=https%3A%2F%2Fembed.gog.com%2Fon_login_success%3Forigin%3Dclient"
    "&response_type=code&layout=galaxy");

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
    m_steam = new SteamClient(this);
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

    connect(m_steam, &SteamClient::stateChanged, this, &StoreManager::storesChanged);
    connect(m_steam, &SteamClient::libraryChanged, this, &StoreManager::ownedChanged);
    connect(m_steam, &SteamClient::downloadsChanged, this, [this] {
        // Загружаемые игры Steam — в библиотеке с самого начала (в том числе поставленные
        // на загрузку из Big Picture)
        for (const SteamClient::Download &d : m_steam->downloads())
            GameLibrary::instance()->addPending(QStringLiteral("steam"), d.appId, d.title);
        emit downloadsChanged();
        emit ownedChanged();
    });
    connect(m_steam, &SteamClient::installedChanged, this, &StoreManager::syncSteamLibrary);
    connect(m_steam, &SteamClient::gameInstalled, this, &StoreManager::gameInstalled);
    connect(m_steam, &SteamClient::gameRemoved, this, [this](const QString &title) {
        GameLibrary::instance()->refreshStorage();
        emit gameRemoved(title);
    });
    // Выход из оболочки — клиент Steam тоже закрываем
    connect(qApp, &QCoreApplication::aboutToQuit, m_steam, &SteamClient::shutdown);
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

// У каждого пользователя свои настройки инструментов: отдельные каталоги в профиле
QProcessEnvironment StoreManager::environment(const QString &key) const
{
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    const QString dir = storeDir(key);
    if (key == QLatin1String("gog"))
        env.insert(QStringLiteral("XDG_CONFIG_HOME"), dir + QStringLiteral("/config"));
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
        // Запрос ввода («…auth code: ») приходит без перевода строки — отдаём его сразу
        const QString pending = clean(*buffer).trimmed();
        if (pending.endsWith(QLatin1Char(':')) && onLine) {
            onLine(pending);
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
    const Store *gog = findStore(QStringLiteral("gog"));
    map.insert(QStringLiteral("gog"), QVariantMap{
        {QStringLiteral("toolInstalled"), gog->toolInstalled},
        {QStringLiteral("signedIn"), gog->signedIn},
        {QStringLiteral("account"), gog->account},
        {QStringLiteral("avatar"), QString()},
        {QStringLiteral("busy"), gog->busy},
        {QStringLiteral("message"), gog->message},
        {QStringLiteral("listTitle"), gog->listTitle},
    });
    map.insert(QStringLiteral("steam"), QVariantMap{
        {QStringLiteral("toolInstalled"), m_steam->installed()},
        {QStringLiteral("signedIn"), m_steam->signedIn()},
        {QStringLiteral("account"), m_steam->account()},
        {QStringLiteral("avatar"), m_steam->avatar()},
        {QStringLiteral("busy"), m_steam->busy()},
        {QStringLiteral("message"), m_steam->message()},
        {QStringLiteral("listTitle"), tr("Библиотека Steam")},
        {QStringLiteral("phase"), m_steam->phase()},
        {QStringLiteral("phaseProgress"), m_steam->phaseProgress()},
        {QStringLiteral("online"), m_steam->online()},
        {QStringLiteral("running"), m_steam->running()},
    });
    return map;
}

QVariantMap StoreManager::owned() const
{
    GameLibrary *library = GameLibrary::instance();
    QVariantMap map;
    const QList<SteamClient::Download> steamDownloads = m_steam->downloads();
    for (const QString &key : kStores) {
        QList<OwnedGame> games;
        if (key == QLatin1String("steam")) {
            for (const SteamClient::OwnedGame &g : m_steam->owned())
                games.append(OwnedGame{g.appId, g.title, QStringLiteral("game")});
        } else {
            games = findStore(key)->owned;
        }
        std::sort(games.begin(), games.end(), [](const OwnedGame &a, const OwnedGame &b) {
            return a.title.compare(b.title, Qt::CaseInsensitive) < 0;
        });
        QVariantList list;
        for (const OwnedGame &g : std::as_const(games)) {
            const QString id = key + QLatin1Char(':') + g.appId;
            QString status;
            double progress = -1;
            if (key == QLatin1String("steam")) {
                for (const SteamClient::Download &d : steamDownloads) {
                    if (d.appId == g.appId) {
                        status = d.status;
                        progress = d.progress;
                    }
                }
            }
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
    for (const SteamClient::Download &d : m_steam->downloads()) {
        list.append(QVariantMap{
            {QStringLiteral("id"), QStringLiteral("steam:") + d.appId},
            {QStringLiteral("store"), QStringLiteral("steam")},
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
    for (const SteamClient::Download &d : m_steam->downloads())
        count += active(d.status) ? 1 : 0;
    return count;
}

double StoreManager::downloadProgress() const
{
    for (const Download &d : m_downloads) {
        if (d.id() == m_activeDownload)
            return d.progress;
    }
    for (const SteamClient::Download &d : m_steam->downloads()) {
        if (d.status == QLatin1String("downloading"))
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
    for (const SteamClient::Download &d : m_steam->downloads()) {
        if (d.status == QLatin1String("downloading"))
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
    Store &gog = store(QStringLiteral("gog"));
    gog.busy = false;
    gog.message.clear();
    gog.signedIn = false;
    gog.account.clear();
    gog.owned.clear();
    gog.toolInstalled = QFileInfo(gogdlPath()).isExecutable();

    if (!profile.isEmpty()) {
        // GOG: файл входа gogdl и имя пользователя
        if (QFileInfo::exists(gogAuthFile())) {
            gog.signedIn = true;
            gog.account = readJson(storeDir(QStringLiteral("gog")) + QStringLiteral("/account.json"))
                              .value(QStringLiteral("username")).toString();
        }
        loadOwned(QStringLiteral("gog"));
    }
    // Steam: свой клиент у каждого пользователя (прежний закрывается)
    m_steam->setProfile(profile);
    syncSteamLibrary();
    emit storesChanged();
    emit ownedChanged();
}

// Установленные игры Steam (в том числе поставленные через Big Picture) — в библиотеке оболочки
void StoreManager::syncSteamLibrary()
{
    QList<GameLibrary::StoreGame> games;
    for (const SteamClient::InstalledGame &g : m_steam->installedGames())
        games.append({g.appId, g.title, g.path, g.sizeBytes});
    QStringList pending;
    for (const SteamClient::Download &d : m_steam->downloads())
        pending.append(d.appId);
    GameLibrary::instance()->syncStore(QStringLiteral("steam"), games, pending);
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
    store(QStringLiteral("gog")).toolInstalled = QFileInfo(gogdlPath()).isExecutable();
    emit storesChanged();
}

// ---------- Вход ----------

QString StoreManager::loginUrl(const QString &key) const
{
    return key == QLatin1String("gog") ? kGogLogin : QString();
}

void StoreManager::completeWebLogin(const QString &key, const QString &code)
{
    if (key != QLatin1String("gog") || code.trimmed().isEmpty())
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

void StoreManager::steamSignIn()
{
    m_steam->signIn();
}

void StoreManager::openSteam()
{
    m_steam->openBigPicture();
}

void StoreManager::closeSteam()
{
    m_steam->closeBigPicture();
}

void StoreManager::signOut(const QString &key)
{
    if (key == QLatin1String("steam")) {
        m_steam->signOut();
        return;
    }
    const QString dir = storeDir(key);
    if (dir.isEmpty())
        return;
    QFile::remove(gogAuthFile());
    QFile::remove(dir + QStringLiteral("/account.json"));
    QFile::remove(dir + QStringLiteral("/owned.json"));
    reload();
}

// ---------- Списки игр ----------

void StoreManager::refresh(const QString &key)
{
    if (key == QLatin1String("steam")) {
        m_steam->refreshLibrary();
        return;
    }
    const Store &s = store(key);
    if (s.signedIn && !s.busy && s.toolInstalled)
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
    if (key == QLatin1String("steam")) {
        if (!m_steam->signedIn())
            return;
        library->addPending(key, appId, title);
        m_steam->install(appId, title);
        return;
    }
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
    emit downloadsChanged();
    emit ownedChanged();

    GameLibrary *library = GameLibrary::instance();
    const auto progress = [this](double p, const QString &message) {
        if (Download *d = findDownload(m_activeDownload)) {
            if (p >= 0)
                d->progress = p;
            if (!message.isEmpty())
                d->message = message;
            emit downloadsChanged();
            emit ownedChanged();
        }
    };

    if (key == QLatin1String("gog")) {
        const QString base = library->gamesDir(key);
        const QStringList before = QDir(base).entryList(QDir::Dirs | QDir::NoDotAndDotDot);
        static const QRegularExpression re(QStringLiteral("Progress:\\s*([\\d.]+)"));
        QProcess *proc = run(gogdlPath(),
            {QStringLiteral("--auth-config-path"), gogAuthFile(), QStringLiteral("download"), appId,
             QStringLiteral("--platform"), QStringLiteral("windows"), QStringLiteral("--path"), base,
             QStringLiteral("--skip-dlcs"), QStringLiteral("--lang"), QStringLiteral("en-US")},
            environment(key),
            [progress](const QString &line) {
                const double p = parseProgress(line, re);
                if (p >= 0)
                    progress(p, tr("Загрузка: %1%").arg(qRound(p * 100)));
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
        return;
    }

    finishDownload(false, tr("Загрузка игр этого магазина не поддерживается."));
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

void StoreManager::cancel(const QString &gameId)
{
    if (gameId.startsWith(QLatin1String("steam:"))) {
        m_steam->cancel(gameId.mid(6));
        if (!GameLibrary::instance()->isInstalled(gameId))
            GameLibrary::instance()->removeRecord(gameId);
        return;
    }
    if (gameId == m_activeDownload && m_activeProcess) {
        if (Download *d = findDownload(gameId))
            d->status = QStringLiteral("cancelled");   // finishDownload уберёт её из списка
        m_activeProcess->kill();
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
        m_activeProcess->kill();
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
    if (gameId.startsWith(QLatin1String("steam:"))) {
        // Игры Steam удаляет клиент Steam (через свою библиотеку)
        if (game.value(QStringLiteral("installed")).toBool())
            m_steam->uninstall(gameId.mid(6));
        else
            m_steam->cancel(gameId.mid(6));
        library->removeRecord(gameId);
        return;
    }
    // Загружается — сначала останавливаем загрузку
    if (findDownload(gameId)) {
        cancel(gameId);
        if (!game.value(QStringLiteral("installed")).toBool())
            return;
    }
    const QString title = game.value(QStringLiteral("title")).toString();
    const QString path = game.value(QStringLiteral("installPath")).toString();

    // Папку удаляем в фоне: у больших игр это может занять время
    library->removeRecord(gameId);
    auto *watcher = new QFutureWatcher<bool>(this);
    connect(watcher, &QFutureWatcher<bool>::finished, this, [this, watcher, title] {
        watcher->deleteLater();
        GameLibrary::instance()->refreshStorage();
        emit gameRemoved(title);
    });
    watcher->setFuture(QtConcurrent::run([path] { return path.isEmpty() || QDir(path).removeRecursively(); }));
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
    const auto cached = m_compat.constFind(id);
    if (cached != m_compat.cend())
        return cached.value();
    if (m_compatPending.contains(id) || appId.isEmpty())
        return {{QStringLiteral("checked"), false}};
    m_compatPending.insert(id);

    // Steam — категории и controller_support из магазина Steam (английские названия категорий),
    // GOG — список возможностей игры
    const QUrl url = key == QLatin1String("steam")
            ? QUrl(QStringLiteral("https://store.steampowered.com/api/appdetails?appids=%1&l=english").arg(appId))
            : QUrl(QStringLiteral("https://api.gog.com/v2/games/%1?locale=en-US").arg(appId));
    QNetworkReply *reply = m_network->get(request(url));
    connect(reply, &QNetworkReply::finished, this, [this, reply, key, appId, id] {
        reply->deleteLater();
        m_compatPending.remove(id);
        const QJsonObject root = QJsonDocument::fromJson(reply->readAll()).object();
        QVariantMap info{{QStringLiteral("checked"), true}};
        bool ok = reply->error() == QNetworkReply::NoError;
        bool controller = false, ps5 = false, ps4 = false;
        if (ok && key == QLatin1String("steam")) {
            const QJsonObject entry = root.value(appId).toObject();
            ok = entry.value(QStringLiteral("success")).toBool();
            const QJsonObject data = entry.value(QStringLiteral("data")).toObject();
            controller = !data.value(QStringLiteral("controller_support")).toString().isEmpty();
            for (const QJsonValue &value : data.value(QStringLiteral("categories")).toArray()) {
                const QString name = value.toObject().value(QStringLiteral("description")).toString().toLower();
                // «Tracked Controller Support» — контроллеры VR, не геймпад
                if (name.contains(QLatin1String("tracked")))
                    continue;
                if (name.contains(QLatin1String("controller")))
                    controller = true;
                if (name.contains(QLatin1String("ps5")) || name.contains(QLatin1String("dualsense")))
                    ps5 = true;
                if (name.contains(QLatin1String("ps4")) || name.contains(QLatin1String("dualshock")))
                    ps4 = true;
            }
        } else if (ok) {
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
