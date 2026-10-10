#include "storemanager.h"

#include "accountmanager.h"
#include "gamelibrary.h"

#include <QCoreApplication>
#include <QDir>
#include <QDirIterator>
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

#include <zlib.h>

namespace {

const QStringList kStores = {QStringLiteral("steam"), QStringLiteral("gog")};

// Вход GOG, который ожидает gogdl: код приходит в адресе on_login_success?code=…
const QString kGogLogin = QStringLiteral(
    "https://auth.gog.com/auth?client_id=46899977096215655"
    "&redirect_uri=https%3A%2F%2Fembed.gog.com%2Fon_login_success%3Forigin%3Dclient"
    "&response_type=code&layout=galaxy");

// Бесплатное приложение для проверки входа в Steam: DepotDownloader всегда требует -app,
// а входа нам достаточно — после него процесс останавливаем
const QString kSteamLoginProbeApp = QStringLiteral("228980");

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

// Строка QR-кода из символов-блоков (DepotDownloader рисует QR в консоли)
// (строка из одних пробелов тоже бывает — светлый ряд модулей; учитываем её только внутри QR)
bool isQrRow(const QString &line)
{
    if (line.size() < 10)
        return false;
    for (const QChar c : line) {
        if (c != QLatin1Char(' ') && c != QChar(0x2588) && c != QChar(0x2580) && c != QChar(0x2584))
            return false;
    }
    return true;
}

// Имя аккаунта Steam из файла запомненных входов DepotDownloader (account.config, protobuf):
// в нём словарь «аккаунт → токен входа». Записи словаря — вложенные сообщения
// { 1: имя аккаунта, 2: токен }, токен — JWT («ey…»). Нужен после входа по QR-коду:
// там логин не вводится, а DepotDownloader запоминает вход под именем аккаунта.
// Распаковка zlib: windowBits -15 — «сырой» deflate (как у .NET DeflateStream),
// 15 + 32 — с заголовком zlib или gzip (определяется автоматически)
QByteArray inflateData(const QByteArray &input, int windowBits)
{
    z_stream stream{};
    if (input.isEmpty() || inflateInit2(&stream, windowBits) != Z_OK)
        return {};
    stream.next_in = reinterpret_cast<Bytef *>(const_cast<char *>(input.constData()));
    stream.avail_in = uInt(input.size());
    QByteArray output;
    char buffer[16384];
    int result = Z_OK;
    while (result != Z_STREAM_END) {
        stream.next_out = reinterpret_cast<Bytef *>(buffer);
        stream.avail_out = sizeof(buffer);
        result = inflate(&stream, Z_NO_FLUSH);
        if (result != Z_OK && result != Z_STREAM_END) {
            inflateEnd(&stream);
            return {};
        }
        output.append(buffer, qsizetype(sizeof(buffer) - stream.avail_out));
        if (result == Z_OK && stream.avail_in == 0 && stream.avail_out != 0)
            break;   // входные данные кончились без конца потока
    }
    inflateEnd(&stream);
    return output;
}

struct DepotAccount {
    QString name;
    QString token;
};

DepotAccount depotAccount(const QString &home)
{
    const auto readVarint = [](const QByteArray &data, qsizetype &pos, quint64 &value) {
        value = 0;
        for (int shift = 0; pos < data.size() && shift < 64; shift += 7) {
            const quint8 byte = quint8(data.at(pos++));
            value |= quint64(byte & 0x7f) << shift;
            if (!(byte & 0x80))
                return true;
        }
        return false;
    };
    // { 1: строка, 2: строка } → пара (ключ, значение)
    const auto entry = [&](const QByteArray &data, QString &key, QString &value) {
        qsizetype pos = 0;
        while (pos < data.size()) {
            quint64 tag = 0, length = 0;
            if (!readVarint(data, pos, tag) || (tag & 7) != 2 || !readVarint(data, pos, length)
                || pos + qsizetype(length) > data.size())
                return false;
            const QString text = QString::fromUtf8(data.mid(pos, qsizetype(length)));
            if ((tag >> 3) == 1)
                key = text;
            else if ((tag >> 3) == 2)
                value = text;
            pos += qsizetype(length);
        }
        return !key.isEmpty() && value.startsWith(QLatin1String("ey"));
    };

    // Верхний уровень protobuf: ищем вложенную запись словаря «аккаунт → токен»
    const auto parse = [&](const QByteArray &data) -> DepotAccount {
        qsizetype pos = 0;
        while (pos < data.size()) {
            quint64 tag = 0, value = 0;
            if (!readVarint(data, pos, tag))
                break;
            switch (tag & 7) {
            case 0: readVarint(data, pos, value); break;
            case 1: pos += 8; break;
            case 5: pos += 4; break;
            case 2: {
                if (!readVarint(data, pos, value) || pos + qsizetype(value) > data.size())
                    return {};
                QString key, token;
                if (entry(data.mid(pos, qsizetype(value)), key, token))
                    return {key, token};
                pos += qsizetype(value);
                break;
            }
            default: return {};
            }
        }
        return {};
    };

    QDirIterator it(home, {QStringLiteral("account.config")}, QDir::Files | QDir::Hidden | QDir::NoDotAndDotDot,
                    QDirIterator::Subdirectories);
    bool found = false;
    while (it.hasNext()) {
        const QString path = it.next();
        found = true;
        QFile file(path);
        if (!file.open(QIODevice::ReadOnly))
            continue;
        const QByteArray data = file.readAll();
        // DepotDownloader сохраняет настройки сжатыми (DeflateStream); на всякий случай
        // пробуем и без сжатия, и с заголовком zlib/gzip
        for (const QByteArray &candidate : {inflateData(data, -MAX_WBITS), inflateData(data, MAX_WBITS + 32), data}) {
            if (candidate.isEmpty())
                continue;
            const DepotAccount account = parse(candidate);
            if (!account.name.isEmpty()) {
                qInfo("Steam: saved login for %s found in %s", qPrintable(account.name), qPrintable(path));
                return account;
            }
        }
        qWarning("Steam: no saved login in %s (%lld bytes)", qPrintable(path), qlonglong(data.size()));
    }
    if (!found)
        qWarning("Steam: DepotDownloader account.config not found under %s", qPrintable(home));
    return {};
}

// Помощник Steam на SteamKit2 (библиотека, профиль): установленный или из каталога сборки
QString steamHelperPath()
{
    const QString installed = QStringLiteral(REDANCHOR_STEAM_HELPER);
    if (QFileInfo(installed).isExecutable())
        return installed;
    const QString local = QCoreApplication::applicationDirPath() + QStringLiteral("/redanchor-steam");
    return QFileInfo(local).isExecutable() ? local : QString();
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
        cancelSteamLogin();
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

QString StoreManager::depotPath() const { return toolsDir() + QStringLiteral("/DepotDownloader"); }
QString StoreManager::gogdlPath() const { return toolsDir() + QStringLiteral("/gogdl"); }
QString StoreManager::gogAuthFile() const { return storeDir(QStringLiteral("gog")) + QStringLiteral("/auth.json"); }

QString StoreManager::steamUser() const
{
    return readJson(storeDir(QStringLiteral("steam")) + QStringLiteral("/account.json")).value(QStringLiteral("user")).toString();
}

// У каждого пользователя свои настройки инструментов: отдельные каталоги в профиле
QProcessEnvironment StoreManager::environment(const QString &key) const
{
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    const QString dir = storeDir(key);
    if (key == QLatin1String("gog")) {
        env.insert(QStringLiteral("XDG_CONFIG_HOME"), dir + QStringLiteral("/config"));
    } else if (key == QLatin1String("steam")) {
        // DepotDownloader хранит запомненный вход в своём каталоге данных — у каждого свой HOME
        QDir().mkpath(dir + QStringLiteral("/home"));
        env.insert(QStringLiteral("HOME"), dir + QStringLiteral("/home"));
        // .NET хранит IsolatedStorage в XDG_DATA_HOME, если он задан, — направляем туда же,
        // иначе запомненный вход попадёт в общий каталог системы
        env.insert(QStringLiteral("XDG_DATA_HOME"), dir + QStringLiteral("/home/.local/share"));
        env.insert(QStringLiteral("XDG_CONFIG_HOME"), dir + QStringLiteral("/home/.config"));
        env.insert(QStringLiteral("DOTNET_CLI_TELEMETRY_OPTOUT"), QStringLiteral("1"));
        // DepotDownloader на .NET: без ICU в системе он не запустится, языковые таблицы ему не нужны
        env.insert(QStringLiteral("DOTNET_SYSTEM_GLOBALIZATION_INVARIANT"), QStringLiteral("1"));
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
    for (const QString &key : kStores) {
        const Store *s = findStore(key);
        map.insert(key, QVariantMap{
            {QStringLiteral("toolInstalled"), s->toolInstalled},
            {QStringLiteral("toolBusy"), s->toolBusy},
            {QStringLiteral("toolProgress"), s->toolProgress},
            {QStringLiteral("toolMessage"), s->toolMessage},
            {QStringLiteral("signedIn"), s->signedIn},
            {QStringLiteral("account"), s->account},
            {QStringLiteral("avatar"), s->avatar},
            {QStringLiteral("busy"), s->busy},
            {QStringLiteral("message"), s->message},
            {QStringLiteral("guard"), s->guard},
            {QStringLiteral("qr"), s->qr},
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
        QList<OwnedGame> games = findStore(key)->owned;
        // Витрину Steam показываем в порядке магазина, купленные игры GOG — по алфавиту
        if (key == QLatin1String("gog")) {
            std::sort(games.begin(), games.end(), [](const OwnedGame &a, const OwnedGame &b) {
                return a.title.compare(b.title, Qt::CaseInsensitive) < 0;
            });
        }
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
    return int(std::count_if(m_downloads.cbegin(), m_downloads.cend(), [](const Download &d) {
        return d.status == QLatin1String("queued") || d.status == QLatin1String("downloading");
    }));
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
    const bool hasUser = !AccountManager::instance()->profileDir().isEmpty();
    for (const QString &key : kStores) {
        Store &s = store(key);
        s.busy = false;
        s.message.clear();
        s.guard.clear();
        s.qr.clear();
        s.signedIn = false;
        s.account.clear();
        s.avatar.clear();
        s.owned.clear();
    }
    store(QStringLiteral("steam")).toolInstalled = QFileInfo(depotPath()).isExecutable();
    store(QStringLiteral("gog")).toolInstalled = QFileInfo(gogdlPath()).isExecutable();

    if (hasUser) {
        // GOG: файл входа gogdl и имя пользователя
        if (QFileInfo::exists(gogAuthFile())) {
            store(QStringLiteral("gog")).signedIn = true;
            store(QStringLiteral("gog")).account =
                readJson(storeDir(QStringLiteral("gog")) + QStringLiteral("/account.json")).value(QStringLiteral("username")).toString();
        }
        // Steam: DepotDownloader помнит вход, мы — логин, имя профиля и аватар
        const QJsonObject steamAccount = readJson(storeDir(QStringLiteral("steam")) + QStringLiteral("/account.json"));
        const QString user = steamAccount.value(QStringLiteral("user")).toString();
        if (!user.isEmpty()) {
            Store &steam = store(QStringLiteral("steam"));
            steam.signedIn = true;
            const QString persona = steamAccount.value(QStringLiteral("persona")).toString();
            steam.account = persona.isEmpty() ? user : persona;
            steam.avatar = steamAccount.value(QStringLiteral("avatar")).toString();
        }
        for (const QString &key : kStores)
            loadOwned(key);
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
    const bool steam = QFileInfo(depotPath()).isExecutable();
    const bool gog = QFileInfo(gogdlPath()).isExecutable();
    if (steam == store(QStringLiteral("steam")).toolInstalled && gog == store(QStringLiteral("gog")).toolInstalled)
        return;
    store(QStringLiteral("steam")).toolInstalled = steam;
    store(QStringLiteral("gog")).toolInstalled = gog;
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

void StoreManager::steamQrLogin()
{
    // С -qr DepotDownloader не принимает -username: имя аккаунта узнаём после входа
    startSteamLogin(QString(), {QStringLiteral("-qr")});
}

void StoreManager::steamLogin(const QString &user, const QString &password)
{
    if (password.isEmpty())
        return;
    startSteamLogin(user, {QStringLiteral("-password"), password});
}

// Вход через DepotDownloader: он входит, запоминает вход (-remember-password) под логином
// и переходит к приложению-«пробе» — в этот момент вход уже удался, и процесс останавливаем.
void StoreManager::startSteamLogin(const QString &user, const QStringList &authArgs)
{
    const QString login = user.trimmed();
    const bool qr = authArgs.contains(QStringLiteral("-qr"));
    if (m_steamLogin || (login.isEmpty() && !qr) || !store(QStringLiteral("steam")).toolInstalled)
        return;
    Store &steam = store(QStringLiteral("steam"));
    steam.qr.clear();
    steam.guard.clear();
    setBusy(QStringLiteral("steam"), true, tr("Подключение к Steam…"));

    struct State {
        bool loggedIn = false;
        bool collectingQr = false;
        QStringList qrRows;
        QString failure;
    };
    auto state = std::make_shared<State>();
    const QString probeDir = storeDir(QStringLiteral("steam")) + QStringLiteral("/probe");

    QStringList args{QStringLiteral("-app"), kSteamLoginProbeApp, QStringLiteral("-manifest-only"),
                     QStringLiteral("-remember-password"), QStringLiteral("-dir"), probeDir};
    if (!qr)
        args += {QStringLiteral("-username"), login};
    args += authArgs;

    QProcess *proc = run(depotPath(), args, environment(QStringLiteral("steam")),
        [this, state](const QString &raw) {
            const QString line = raw.trimmed();
            // Строки QR-кода и пустые строки в журнал не пишем
            if (!isQrRow(raw) && !line.isEmpty())
                qInfo("DepotDownloader: %s", qPrintable(line));
            Store &steam = store(QStringLiteral("steam"));

            // ---- QR-код: заголовок, затем строки из блоков ----
            if (line.contains(QLatin1String("QR code"), Qt::CaseInsensitive)) {
                state->collectingQr = true;
                state->qrRows.clear();
                steam.message = tr("Отсканируйте QR-код в мобильном приложении Steam.");
                emit storesChanged();
                return;
            }
            if (state->collectingQr) {
                if (isQrRow(raw)) {
                    // Показываем QR по мере прихода строк: он всегда полный, даже если после
                    // него DepotDownloader долго ничего не пишет (ждёт сканирования)
                    state->qrRows.append(raw);
                    steam.qr = state->qrRows.join(QLatin1Char('\n'));
                    emit storesChanged();
                    return;
                }
                if (!state->qrRows.isEmpty()) {
                    qInfo("Steam QR: %lld rows x %lld columns", qlonglong(state->qrRows.size()),
                          qlonglong(std::max_element(state->qrRows.cbegin(), state->qrRows.cend(),
                                                     [](const QString &a, const QString &b) { return a.size() < b.size(); })->size()));
                    state->collectingQr = false;
                }
                if (line.isEmpty())
                    return;
            }
            if (line.isEmpty())
                return;

            // ---- Steam Guard ----
            if (line.contains(QLatin1String("STEAM GUARD"), Qt::CaseInsensitive)) {
                if (line.contains(QLatin1String("Mobile App to confirm"), Qt::CaseInsensitive)
                    || line.contains(QLatin1String("confirm your sign in"), Qt::CaseInsensitive)) {
                    steam.guard = QStringLiteral("mobile");
                    steam.message = tr("Подтвердите вход в мобильном приложении Steam.");
                } else {
                    steam.guard = QStringLiteral("code");
                    steam.message = line.contains(QLatin1String("email"), Qt::CaseInsensitive)
                            ? tr("Введите код Steam Guard из письма.")
                            : tr("Введите код из мобильного приложения Steam.");
                }
                emit storesChanged();
                return;
            }

            // ---- Итог входа ----
            // «Logging … into Steam3... Done!» — вход есть, но DepotDownloader ещё сохраняет его;
            // останавливаем процесс на следующем шаге, когда сохранение точно завершено
            if (line.contains(QLatin1String("into Steam3"), Qt::CaseInsensitive) && line.contains(QLatin1String("Done"))) {
                state->loggedIn = true;
                return;
            }
            if (line.contains(QLatin1String("Got AppInfo"), Qt::CaseInsensitive)
                || line.contains(QLatin1String("Using app branch"), Qt::CaseInsensitive)
                || line.contains(QLatin1String("not available from this account"), Qt::CaseInsensitive)) {
                // Дальше DepotDownloader занимается приложением-пробой — вход уже удался
                state->loggedIn = true;
                if (m_steamLogin)
                    m_steamLogin->kill();
                return;
            }
            if (line.contains(QLatin1String("Unable to login"), Qt::CaseInsensitive)
                || line.contains(QLatin1String("Failed to authenticate"), Qt::CaseInsensitive)
                || line.contains(QLatin1String("InvalidPassword"), Qt::CaseInsensitive)
                || line.contains(QLatin1String("RateLimit"), Qt::CaseInsensitive)) {
                state->failure = line;
            }
        },
        [this, login, state](int exitCode, const QString &) {
            m_steamLogin = nullptr;
            Store &steam = store(QStringLiteral("steam"));
            steam.guard.clear();
            steam.qr.clear();
            if (exitCode == 0 && state->failure.isEmpty())
                state->loggedIn = true;
            qInfo("Steam login finished: exit %d, %s", exitCode, state->loggedIn ? "ok" : "failed");
            if (!state->loggedIn) {
                QString message = tr("Не удалось войти в Steam.");
                if (state->failure.contains(QLatin1String("InvalidPassword"), Qt::CaseInsensitive))
                    message = tr("Неверный логин или пароль.");
                else if (state->failure.contains(QLatin1String("RateLimit"), Qt::CaseInsensitive))
                    message = tr("Слишком много попыток входа. Попробуйте позже.");
                setBusy(QStringLiteral("steam"), false, message);
                return;
            }
            // Вход по QR: имя аккаунта — из запомненных входов DepotDownloader
            QString account = login;
            if (account.isEmpty())
                account = depotAccount(storeDir(QStringLiteral("steam")) + QStringLiteral("/home")).name;
            if (account.isEmpty()) {
                qWarning("Steam QR login: account name not found in DepotDownloader settings");
                setBusy(QStringLiteral("steam"), false, tr("Вход выполнен, но DepotDownloader не запомнил его. Попробуйте войти с паролем."));
                return;
            }
            writeJson(storeDir(QStringLiteral("steam")) + QStringLiteral("/account.json"), QJsonObject{{QStringLiteral("user"), account}});
            QDir(storeDir(QStringLiteral("steam")) + QStringLiteral("/probe")).removeRecursively();
            steam.signedIn = true;
            steam.account = account;
            setBusy(QStringLiteral("steam"), false);
            refresh(QStringLiteral("steam"));
        },
        true);
    // Если DepotDownloader не запустился, run() уже сообщил об ошибке внутри start()
    if (proc->state() != QProcess::NotRunning)
        m_steamLogin = proc;
}

void StoreManager::steamGuard(const QString &code)
{
    if (!m_steamLogin || code.trimmed().isEmpty())
        return;
    m_steamLogin->write(code.trimmed().toUtf8() + '\n');
    Store &steam = store(QStringLiteral("steam"));
    steam.guard.clear();
    steam.message = tr("Проверяем код…");
    emit storesChanged();
}

void StoreManager::cancelSteamLogin()
{
    if (m_steamLogin)
        m_steamLogin->kill();
}

void StoreManager::signOut(const QString &key)
{
    const QString dir = storeDir(key);
    if (dir.isEmpty())
        return;
    if (key == QLatin1String("gog")) {
        QFile::remove(gogAuthFile());
        QFile::remove(dir + QStringLiteral("/account.json"));
    } else if (key == QLatin1String("steam")) {
        QFile::remove(dir + QStringLiteral("/account.json"));
        // Запомненный вход DepotDownloader хранится в его каталоге данных (HOME пользователя)
        QDir(dir + QStringLiteral("/home")).removeRecursively();
    }
    QFile::remove(dir + QStringLiteral("/owned.json"));
    reload();
}

// ---------- Списки игр ----------

void StoreManager::refresh(const QString &key)
{
    const Store &s = store(key);
    if (!s.signedIn || s.busy)
        return;
    if (key == QLatin1String("gog")) {
        if (s.toolInstalled)
            refreshGog();
        return;
    }
    refreshSteamLibrary();
}

// Библиотека Steam, имя профиля и аватар — через помощник redanchor-steam (SteamKit2).
// Он входит тем же токеном, что сохранил DepotDownloader, — заново входить не нужно.
void StoreManager::refreshSteamLibrary()
{
    const QString helper = steamHelperPath();
    if (helper.isEmpty()) {
        setBusy(QStringLiteral("steam"), false,
                tr("Библиотека Steam недоступна: оболочка собрана без redanchor-steam (нужен .NET SDK 8)."));
        return;
    }
    const DepotAccount account = depotAccount(storeDir(QStringLiteral("steam")) + QStringLiteral("/home"));
    if (account.token.isEmpty()) {
        setBusy(QStringLiteral("steam"), false, tr("Вход в Steam устарел. Выйдите и войдите снова."));
        return;
    }
    setBusy(QStringLiteral("steam"), true, tr("Загрузка библиотеки Steam…"));
    QProcess *proc = run(helper, {QStringLiteral("library"), QStringLiteral("--user"), account.name},
        QProcessEnvironment::systemEnvironment(),
        [](const QString &line) {
            if (!line.trimmed().isEmpty())
                qInfo("redanchor-steam: %s", qPrintable(line.trimmed()));
        },
        [this, account](int exitCode, const QString &out) {
            if (exitCode != 0) {
                setBusy(QStringLiteral("steam"), false,
                        exitCode == 3 ? tr("Вход в Steam устарел. Выйдите и войдите снова.")
                                      : tr("Не удалось загрузить библиотеку Steam. Проверьте подключение к интернету."));
                return;
            }
            const QJsonObject result = lastJson(out).object();
            Store &steam = store(QStringLiteral("steam"));
            const QString persona = result.value(QStringLiteral("persona")).toString();
            if (!persona.isEmpty())
                steam.account = persona;
            const QString avatar = result.value(QStringLiteral("avatar")).toString();
            if (!avatar.isEmpty())
                steam.avatar = avatar;
            writeJson(storeDir(QStringLiteral("steam")) + QStringLiteral("/account.json"),
                      QJsonObject{{QStringLiteral("user"), account.name}, {QStringLiteral("persona"), steam.account},
                                  {QStringLiteral("avatar"), steam.avatar}});

            QList<OwnedGame> games;
            for (const QJsonValue &value : result.value(QStringLiteral("games")).toArray()) {
                const QJsonObject g = value.toObject();
                games.append(OwnedGame{QString::number(qint64(g.value(QStringLiteral("appid")).toDouble())),
                                       g.value(QStringLiteral("name")).toString(), QStringLiteral("game")});
            }
            std::sort(games.begin(), games.end(), [](const OwnedGame &a, const OwnedGame &b) {
                return a.title.compare(b.title, Qt::CaseInsensitive) < 0;
            });
            steam.owned = games;
            steam.listTitle = tr("Библиотека Steam");
            saveOwned(QStringLiteral("steam"));
            setBusy(QStringLiteral("steam"), false);
            emit ownedChanged();
        });
    // Токен — через stdin, чтобы он не был виден в списке процессов
    if (proc->state() != QProcess::NotRunning) {
        proc->write(account.token.toUtf8() + '\n');
        proc->closeWriteChannel();
    }
}

void StoreManager::searchSteam(const QString &term)
{
    const QString text = term.trimmed();
    if (text.isEmpty()) {
        refresh(QStringLiteral("steam"));
        return;
    }
    QUrl url(QStringLiteral("https://store.steampowered.com/api/storesearch/"));
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("term"), text);
    query.addQueryItem(QStringLiteral("l"), QStringLiteral("russian"));
    query.addQueryItem(QStringLiteral("cc"), QStringLiteral("ru"));
    url.setQuery(query);
    loadSteamList(url, tr("Поиск: %1").arg(text), true);
}

// Витрина (популярное, новинки, скидки) или результаты поиска магазина Steam
void StoreManager::loadSteamList(const QUrl &url, const QString &title, bool search)
{
    setBusy(QStringLiteral("steam"), true, tr("Загрузка…"));
    QNetworkReply *reply = m_network->get(request(url));
    connect(reply, &QNetworkReply::finished, this, [this, reply, title, search] {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            setBusy(QStringLiteral("steam"), false, tr("Магазин Steam недоступен. Проверьте подключение к интернету."));
            return;
        }
        const QJsonObject root = QJsonDocument::fromJson(reply->readAll()).object();
        QList<OwnedGame> games;
        QSet<QString> seen;
        const auto add = [&](const QJsonArray &items) {
            for (const QJsonValue &value : items) {
                const QJsonObject item = value.toObject();
                // В поиске бывают наборы и DLC — оставляем приложения
                if (search && item.value(QStringLiteral("type")).toString() != QLatin1String("app"))
                    continue;
                const QString id = QString::number(qint64(item.value(QStringLiteral("id")).toDouble()));
                const QString name = item.value(QStringLiteral("name")).toString();
                if (id == QLatin1String("0") || name.isEmpty() || seen.contains(id))
                    continue;
                seen.insert(id);
                games.append(OwnedGame{id, name, QStringLiteral("game")});
            }
        };
        if (search) {
            add(root.value(QStringLiteral("items")).toArray());
        } else {
            for (const QString &section : {QStringLiteral("top_sellers"), QStringLiteral("new_releases"), QStringLiteral("specials")})
                add(root.value(section).toObject().value(QStringLiteral("items")).toArray());
        }
        Store &steam = store(QStringLiteral("steam"));
        steam.owned = games;
        steam.listTitle = title;
        saveOwned(QStringLiteral("steam"));
        setBusy(QStringLiteral("steam"), false, games.isEmpty() && search ? tr("Ничего не найдено.") : QString());
        emit ownedChanged();
    });
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
    if (library->isInstalled(id) || !store(key).signedIn)
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

    // Steam: версия для Windows (игры запускаются через Proton), каждая игра — в своей папке.
    // Вход — запомненный DepotDownloader под логином пользователя.
    const QString dir = library->gamesDir(key) + QLatin1Char('/') + appId;
    QDir().mkpath(dir);
    auto failure = std::make_shared<QString>();
    static const QRegularExpression re(QStringLiteral("^\\s*(\\d{1,3}(?:[.,]\\d+)?)%"));
    QProcess *proc = run(depotPath(),
        {QStringLiteral("-app"), appId, QStringLiteral("-username"), steamUser(), QStringLiteral("-remember-password"),
         QStringLiteral("-os"), QStringLiteral("windows"), QStringLiteral("-dir"), dir},
        environment(key),
        [this, progress, failure](const QString &raw) {
            const QString line = raw.trimmed();
            if (line.isEmpty())
                return;
            QString normalized = line;
            normalized.replace(QLatin1Char(','), QLatin1Char('.'));
            const double p = parseProgress(normalized, re);
            if (p >= 0) {
                progress(p, tr("Загрузка: %1%").arg(qRound(p * 100)));
                return;
            }
            qInfo("DepotDownloader: %s", qPrintable(line));
            if (line.contains(QLatin1String("not available from this account"), Qt::CaseInsensitive)) {
                *failure = tr("Этой игры нет на вашем аккаунте Steam.");
            } else if (line.contains(QLatin1String("password"), Qt::CaseInsensitive) && line.endsWith(QLatin1Char(':'))) {
                // Запомненный вход не подошёл — DepotDownloader просит пароль
                *failure = tr("Вход в Steam устарел. Выйдите и войдите снова.");
                if (m_activeProcess)
                    m_activeProcess->kill();
            } else if (line.contains(QLatin1String("STEAM GUARD"), Qt::CaseInsensitive)) {
                *failure = tr("Steam просит подтвердить вход. Выйдите и войдите снова.");
                if (m_activeProcess)
                    m_activeProcess->kill();
            }
        },
        [this, dir, appId, title, failure](int exitCode, const QString &) {
            m_activeProcess = nullptr;
            if (exitCode != 0 || !failure->isEmpty()) {
                finishDownload(false, failure->isEmpty() ? tr("Не удалось установить игру.") : *failure);
                return;
            }
            GameLibrary::instance()->addInstalled(QStringLiteral("steam"), appId, title, dir, 0);
            finishDownload(true);
        },
        true);
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

void StoreManager::cancel(const QString &gameId)
{
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

// ---------- Сведения о запуске игр Steam ----------

void StoreManager::steamLaunchInfo(const QString &appId, std::function<void(const QVariantMap &)> onReady)
{
    const QString cacheFile = storeDir(QStringLiteral("steam")) + QStringLiteral("/launch/") + appId + QStringLiteral(".json");
    const QJsonObject cached = readJson(cacheFile);
    if (!cached.value(QStringLiteral("executable")).toString().isEmpty()) {
        onReady(cached.toVariantMap());
        return;
    }
    const QString helper = steamHelperPath();
    const DepotAccount account = depotAccount(storeDir(QStringLiteral("steam")) + QStringLiteral("/home"));
    if (helper.isEmpty() || account.token.isEmpty()) {
        onReady({});
        return;
    }
    QProcess *proc = run(helper, {QStringLiteral("launch"), QStringLiteral("--user"), account.name, QStringLiteral("--app"), appId},
        QProcessEnvironment::systemEnvironment(),
        [](const QString &line) {
            if (!line.trimmed().isEmpty())
                qInfo("redanchor-steam: %s", qPrintable(line.trimmed()));
        },
        [cacheFile, onReady](int exitCode, const QString &out) {
            const QJsonObject info = exitCode == 0 ? lastJson(out).object() : QJsonObject();
            if (!info.value(QStringLiteral("executable")).toString().isEmpty())
                writeJson(cacheFile, info);
            onReady(info.toVariantMap());
        });
    if (proc->state() != QProcess::NotRunning) {
        proc->write(account.token.toUtf8() + '\n');
        proc->closeWriteChannel();
    }
}
