#include "componentsmanager.h"

#include <QDateTime>

#include <QCollator>
#include <QCoreApplication>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QFontDatabase>
#include <QGuiApplication>
#include <QHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QProcess>
#include <QRegularExpression>
#include <QStandardPaths>

#include <algorithm>
#include <memory>

namespace {

const QString kProtonRepo = QStringLiteral("GloriousEggroll/proton-ge-custom");
// Инструменты, которые оболочка сама скачивает с GitHub в каталог tools
const QStringList kTools = {QStringLiteral("umu"), QStringLiteral("gogdl"), QStringLiteral("legendary")};
const QString kProtonPrefix = QStringLiteral("GE-Proton");
// Каталог для распаковки нового Proton до проверки (точка — его не примут за установленный)
const QString kProtonStaging = QStringLiteral(".redanchor-extract");

// Тайм-ауты сети: запрос к API и простой загрузки без данных
constexpr int kApiTimeoutMs = 20000;
constexpr int kDownloadStallMs = 60000;

// Установленный помощник, а при запуске из каталога сборки — соседний файл.
QString componentsHelperPath()
{
    const QString installed = QStringLiteral(REDANCHOR_COMPONENTS_HELPER);
    if (QFileInfo::exists(installed))
        return installed;
    const QString local = QCoreApplication::applicationDirPath() + QStringLiteral("/redanchor-components");
    return QFileInfo::exists(local) ? local : QString();
}

QNetworkRequest githubRequest(const QUrl &url, int timeoutMs)
{
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("RedAnchor-Shell"));
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
    request.setTransferTimeout(timeoutMs);
    return request;
}

QString readFileTrimmed(const QString &path)
{
    QFile file(path);
    return file.open(QIODevice::ReadOnly) ? QString::fromLatin1(file.readAll()).trimmed() : QString();
}

bool writeFile(const QString &path, const QByteArray &data)
{
    QFile file(path);
    return file.open(QIODevice::WriteOnly | QIODevice::Truncate) && file.write(data) == data.size();
}

// Исполняемый файл в PATH или в одном из известных каталогов
QString findProgram(const QString &name, const QStringList &extraDirs = {})
{
    const QString found = QStandardPaths::findExecutable(name);
    if (!found.isEmpty())
        return found;
    return QStandardPaths::findExecutable(name, extraDirs);
}

const QStringList kSystemDirs = {QStringLiteral("/usr/games"), QStringLiteral("/usr/bin"), QStringLiteral("/usr/local/bin"),
                                 QStringLiteral("/usr/local/games")};

bool libraryExists(const QString &soname)
{
    for (const QString &dir : {QStringLiteral("/usr/lib/x86_64-linux-gnu"), QStringLiteral("/usr/lib64"),
                               QStringLiteral("/usr/lib"), QStringLiteral("/lib/x86_64-linux-gnu")}) {
        if (QFileInfo::exists(dir + QLatin1Char('/') + soname))
            return true;
    }
    return false;
}

bool interFontInstalled()
{
    if (qobject_cast<QGuiApplication *>(QCoreApplication::instance()))
        return QFontDatabase::families().contains(QStringLiteral("Inter"), Qt::CaseInsensitive);
    return false;
}

} // namespace

ComponentsManager::ComponentsManager(QObject *parent)
    : QObject(parent)
    , m_gpuVendor(detectGpuVendor())
    , m_network(new QNetworkAccessManager(this))
{
    const QString driver = m_gpuVendor == QLatin1String("nvidia") ? tr("Драйвер NVIDIA")
                         : m_gpuVendor == QLatin1String("amd") ? tr("Mesa для видеокарт AMD")
                         : m_gpuVendor == QLatin1String("intel") ? tr("Mesa для графики Intel")
                         : tr("Mesa");
    // Порядок списка — порядок установки
    m_items = {
        {QStringLiteral("system"), tr("Системные компоненты"),
         tr("gamescope, Xwayland, Python 3, PipeWire и шрифт Inter — для сессии и запуска игр")},
        {QStringLiteral("drivers"), tr("Графический драйвер"), driver},
        {QStringLiteral("vulkan"), tr("Vulkan"), tr("Графический интерфейс для игр и Proton")},
        {QStringLiteral("proton"), tr("Proton"), tr("GE-Proton — запуск Windows-игр из GOG и Epic Games Store")},
        {QStringLiteral("umu"), tr("umu-launcher"), tr("Запуск игр в Steam Runtime с исправлениями Proton для конкретных игр")},
        {QStringLiteral("gogdl"), tr("gogdl"), tr("Загрузка игр из GOG"), false},
        {QStringLiteral("legendary"), tr("legendary"), tr("Загрузка игр из Epic Games Store"), false},
    };
}

QString ComponentsManager::detectGpuVendor()
{
    QString vendor;
    const QDir pci(QStringLiteral("/sys/bus/pci/devices"));
    for (const QString &dev : pci.entryList(QDir::Dirs | QDir::NoDotAndDotDot)) {
        const QString base = pci.filePath(dev);
        if (!readFileTrimmed(base + QStringLiteral("/class")).startsWith(QLatin1String("0x03")))
            continue;
        const QString id = readFileTrimmed(base + QStringLiteral("/vendor"));
        // Дискретная видеокарта важнее встроенной
        if (id == QLatin1String("0x10de"))
            return QStringLiteral("nvidia");
        if (id == QLatin1String("0x1002"))
            vendor = QStringLiteral("amd");
        else if (id == QLatin1String("0x8086") && vendor.isEmpty())
            vendor = QStringLiteral("intel");
    }
    return vendor;
}

QVariantList ComponentsManager::components() const
{
    QVariantList list;
    for (const Component &c : m_items) {
        list.append(QVariantMap{
            {QStringLiteral("key"), c.key},
            {QStringLiteral("name"), c.title},
            {QStringLiteral("description"), c.description},
            {QStringLiteral("status"), c.state},
            {QStringLiteral("version"), c.version},
            {QStringLiteral("latest"), c.latest},
            {QStringLiteral("progress"), c.progress},
            {QStringLiteral("message"), c.message},
            {QStringLiteral("recommended"), c.recommended},
        });
    }
    return list;
}

bool ComponentsManager::needsInstall() const
{
    for (const Component &c : m_items) {
        if (c.state == QLatin1String("missing") || c.state == QLatin1String("update") || c.state == QLatin1String("failed"))
            return true;
    }
    return false;
}

ComponentsManager::Component *ComponentsManager::find(const QString &key)
{
    for (Component &c : m_items) {
        if (c.key == key)
            return &c;
    }
    return nullptr;
}

int ComponentsManager::rank(const QString &key) const
{
    for (int i = 0; i < m_items.size(); ++i) {
        if (m_items.at(i).key == key)
            return i;
    }
    return int(m_items.size());
}

void ComponentsManager::setState(const QString &key, const QString &state, const QString &message)
{
    // Пока компонент ждёт установки или ставится, его состояние меняет только установка
    if (key != m_current && !m_queue.contains(key)) {
        if (Component *c = find(key)) {
            c->state = state;
            c->message = message;
        }
    }
    emit componentsChanged();
}

void ComponentsManager::setProgress(double progress, const QString &message)
{
    if (Component *c = find(m_current)) {
        c->progress = progress;
        c->message = message;
    }
    emit componentsChanged();
}

void ComponentsManager::beginCheck()
{
    ++m_pendingChecks;
    emit componentsChanged();
}

void ComponentsManager::endCheck()
{
    m_pendingChecks = qMax(0, m_pendingChecks - 1);
    emit componentsChanged();
}

QString ComponentsManager::compatToolsDir() const
{
    return QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + QStringLiteral("/compatibilitytools");
}

QString ComponentsManager::protonPath() const
{
    for (const Component &c : m_items) {
        if (c.key == QLatin1String("proton") && !c.version.isEmpty())
            return compatToolsDir() + QLatin1Char('/') + c.version;
    }
    return {};
}

// ---------- Проверка ----------

void ComponentsManager::check()
{
    if (m_pendingChecks > 0)
        return;
    for (Component &c : m_items) {
        if (c.key == m_current || m_queue.contains(c.key))
            continue;
        c.state = QStringLiteral("checking");
        c.message.clear();
        c.progress = -1;
    }
    emit componentsChanged();
    removeObsoleteTools();
    checkPackages();
    checkProtonLocal();
    fetchProtonRelease();
    for (const QString &key : kTools) {
        checkToolLocal(key);
        fetchToolRelease(key);
    }
}

// Выпуск с GitHub. Ответ кэшируется вместе с ETag: повторная проверка с If-None-Match
// не расходует лимит API (60 запросов в час без входа), а при ошибке или исчерпанном
// лимите используется последний известный ответ.
void ComponentsManager::fetchRelease(const QString &repo, const ReleaseCallback &callback)
{
    const QString cacheDir = QStandardPaths::writableLocation(QStandardPaths::CacheLocation) + QStringLiteral("/github");
    const QString cacheBase = cacheDir + QLatin1Char('/') + QString(repo).replace(QLatin1Char('/'), QLatin1Char('_'));
    QNetworkRequest request = githubRequest(QUrl(QStringLiteral("https://api.github.com/repos/%1/releases/latest").arg(repo)),
                                            kApiTimeoutMs);
    request.setRawHeader("Accept", "application/vnd.github+json");
    const QByteArray cached = [&] {
        QFile f(cacheBase + QStringLiteral(".json"));
        return f.open(QIODevice::ReadOnly) ? f.readAll() : QByteArray();
    }();
    const QByteArray etag = readFileTrimmed(cacheBase + QStringLiteral(".etag")).toLatin1();
    if (!cached.isEmpty() && !etag.isEmpty())
        request.setRawHeader("If-None-Match", etag);

    QNetworkReply *reply = m_network->get(request);
    connect(reply, &QNetworkReply::finished, this, [reply, callback, cacheDir, cacheBase, cached, repo] {
        reply->deleteLater();
        const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const QByteArray body = reply->readAll();
        const auto fromCache = [&](const QString &why) {
            const QJsonObject release = QJsonDocument::fromJson(cached).object();
            if (!release.isEmpty()) {
                qInfo("GitHub %s: %s, using cached release", qPrintable(repo), qPrintable(why));
                callback(release, QString());
            } else {
                callback({}, why);
            }
        };
        if (status == 304) {
            fromCache(QStringLiteral("not modified"));
            return;
        }
        if (reply->error() != QNetworkReply::NoError || status != 200) {
            const bool limited = status == 403 || status == 429;
            qWarning("GitHub %s: HTTP %d %s", qPrintable(repo), status, qPrintable(reply->errorString()));
            // Причина как есть (имя хоста не найдено, ошибка TLS, таймаут…) — по ней видно,
            // что именно не так, а не просто «нет интернета»
            fromCache(limited ? tr("GitHub временно ограничил запросы — повторите позже")
                              : status == 0 ? tr("Нет связи с GitHub: %1").arg(reply->errorString())
                                            : tr("Сервер GitHub ответил ошибкой %1").arg(status));
            return;
        }
        const QJsonObject release = QJsonDocument::fromJson(body).object();
        if (release.value(QStringLiteral("tag_name")).toString().isEmpty()) {
            fromCache(tr("Непонятный ответ GitHub"));
            return;
        }
        QDir().mkpath(cacheDir);
        writeFile(cacheBase + QStringLiteral(".json"), body);
        writeFile(cacheBase + QStringLiteral(".etag"), reply->rawHeader("ETag"));
        callback(release, QString());
    });
}

// Загрузка файла в target (через target.part). done(true) — файл на месте.
void ComponentsManager::download(const QUrl &url, const QString &target, const std::function<void(bool ok)> &done)
{
    auto file = std::make_shared<QFile>(target + QStringLiteral(".part"));
    if (!file->open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        qWarning("Cannot write %s: %s", qPrintable(file->fileName()), qPrintable(file->errorString()));
        finishCurrent(false, tr("Не удалось сохранить загрузку."));
        return;
    }
    auto writeFailed = std::make_shared<bool>(false);
    m_download = m_network->get(githubRequest(url, kDownloadStallMs));
    QNetworkReply *reply = m_download;
    connect(reply, &QNetworkReply::readyRead, this, [reply, file, writeFailed] {
        const QByteArray chunk = reply->readAll();
        if (!*writeFailed && file->write(chunk) != chunk.size()) {
            *writeFailed = true;   // закончилось место на диске
            reply->abort();
        }
    });
    connect(reply, &QNetworkReply::downloadProgress, this, [this](qint64 received, qint64 total) {
        setProgress(total > 0 ? 0.9 * double(received) / double(total) : -1,
                    total > 0 ? tr("Загрузка: %1 из %2 МБ").arg(received / 1048576).arg(total / 1048576)
                              : tr("Загрузка: %1 МБ").arg(received / 1048576));
    });
    connect(reply, &QNetworkReply::finished, this, [this, reply, file, writeFailed, target, done] {
        if (!*writeFailed) {
            const QByteArray rest = reply->readAll();
            if (file->write(rest) != rest.size())
                *writeFailed = true;
        }
        file->close();
        const bool ok = reply->error() == QNetworkReply::NoError && !*writeFailed;
        const QString error = reply->errorString();
        reply->deleteLater();
        m_download = nullptr;
        if (!ok) {
            QFile::remove(file->fileName());
            qWarning("Download of %s failed: %s", qPrintable(reply->url().toString()), qPrintable(error));
            finishCurrent(false, *writeFailed ? tr("Недостаточно места на диске.")
                                              : tr("Загрузка прервалась. Проверьте подключение к интернету."));
            return;
        }
        QFile::remove(target);
        if (!QFile::rename(file->fileName(), target)) {
            QFile::remove(file->fileName());
            finishCurrent(false, tr("Не удалось сохранить загрузку."));
            return;
        }
        done(true);
    });
}

// ---------- Инструменты с GitHub: gogdl, legendary, umu-launcher ----------

QString ComponentsManager::toolsDir() const
{
    return QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + QStringLiteral("/tools");
}

bool ComponentsManager::isTool(const QString &key)
{
    return kTools.contains(key);
}

// Имя исполняемого файла в каталоге tools (его же ищут StoreManager и GameLauncher)
QString ComponentsManager::toolFile(const QString &key)
{
    if (key == QLatin1String("umu"))
        return QStringLiteral("umu-run");
    return key;   // gogdl, legendary
}

QString ComponentsManager::toolRepo(const QString &key)
{
    if (key == QLatin1String("legendary"))
        return QStringLiteral("derrod/legendary");
    if (key == QLatin1String("umu"))
        return QStringLiteral("Open-Wine-Components/umu-launcher");
    return QStringLiteral("Heroic-Games-Launcher/heroic-gogdl");
}

// Насколько файл выпуска подходит как сборка для Linux x64 (0 — не подходит)
//   umu-launcher: архив umu-launcher-<версия>-zipapp.tar (umu/umu-run — zipapp на Python);
//   legendary:    legendary_linux_x64 (zipapp на Python, с 0.21), в старых выпусках — просто legendary;
//   gogdl:        gogdl_linux_x86_64 или архив.
int ComponentsManager::assetScore(const QString &key, const QString &assetName)
{
    const QString lower = assetName.toLower();
    if (lower.endsWith(QLatin1String(".sha256")) || lower.endsWith(QLatin1String(".sha512"))
        || lower.endsWith(QLatin1String(".sha512sum")) || lower.endsWith(QLatin1String(".sig"))
        || lower.endsWith(QLatin1String(".asc")) || lower.contains(QLatin1String("arm"))
        || lower.contains(QLatin1String("aarch64")))
        return 0;
    if (key == QLatin1String("umu"))
        return lower.endsWith(QLatin1String("-zipapp.tar")) ? 2 : 0;
    if (key == QLatin1String("legendary") && lower == QLatin1String("legendary"))
        return 1;
    if (!lower.contains(QLatin1String("linux")))
        return 0;
    if (key == QLatin1String("legendary") && lower.endsWith(QLatin1String(".zip")))
        return 0;
    return lower.contains(QLatin1String("x64")) || lower.contains(QLatin1String("x86_64")) || lower.contains(QLatin1String("amd64"))
            ? 2 : 1;
}

// Остатки прежних версий: DepotDownloader (игры Steam) больше не нужен
void ComponentsManager::removeObsoleteTools()
{
    const QString dir = toolsDir();
    if (!QFileInfo::exists(dir + QStringLiteral("/DepotDownloader")) && !QFileInfo::exists(dir + QStringLiteral("/DepotDownloader-dist")))
        return;
    qInfo("Removing obsolete DepotDownloader from %s", qPrintable(dir));
    QFile::remove(dir + QStringLiteral("/DepotDownloader"));
    QFile::remove(dir + QStringLiteral("/DepotDownloader.version"));
    QDir(dir + QStringLiteral("/DepotDownloader-dist")).removeRecursively();
}

void ComponentsManager::checkToolLocal(const QString &key)
{
    const QString path = toolsDir() + QLatin1Char('/') + toolFile(key);
    const bool installed = QFileInfo(path).isExecutable();
    if (Component *c = find(key))
        c->version = installed ? readFileTrimmed(path + QStringLiteral(".version")) : QString();
    setState(key, installed ? QStringLiteral("installed") : QStringLiteral("missing"));
}

void ComponentsManager::fetchToolRelease(const QString &key)
{
    beginCheck();
    fetchRelease(toolRepo(key), [this, key](const QJsonObject &release, const QString &error) {
        Component *c = find(key);
        const QString local = c ? c->version : QString();
        const bool installed = QFileInfo(toolsDir() + QLatin1Char('/') + toolFile(key)).isExecutable();
        if (!error.isEmpty()) {
            setState(key, installed ? QStringLiteral("installed") : QStringLiteral("missing"),
                     installed ? tr("Обновления не проверены: %1").arg(error) : error);
            endCheck();
            return;
        }
        const QString tag = release.value(QStringLiteral("tag_name")).toString();
        // Сборка для Linux x64: исполняемый файл (gogdl_linux_x86_64, legendary_linux_x64) или архив
        Release found;
        found.tag = tag;
        int best = 0;
        for (const QJsonValue &value : release.value(QStringLiteral("assets")).toArray()) {
            const QJsonObject asset = value.toObject();
            const QString assetName = asset.value(QStringLiteral("name")).toString();
            const int score = assetScore(key, assetName);
            if (score <= best)
                continue;
            best = score;
            found.url = QUrl(asset.value(QStringLiteral("browser_download_url")).toString());
            found.fileName = QFileInfo(assetName).fileName();   // без каталогов в имени
        }
        if (!found.url.isValid() || found.fileName.isEmpty()) {
            qWarning("No Linux build of %s in the latest release", qPrintable(toolRepo(key)));
            setState(key, installed ? QStringLiteral("installed") : QStringLiteral("failed"),
                     installed ? QString() : tr("Не найдена сборка для Linux"));
            endCheck();
            return;
        }
        m_toolReleases.insert(key, found);
        if (c)
            c->latest = tag;
        setState(key, !installed ? QStringLiteral("missing")
                    : (!tag.isEmpty() && local != tag) ? QStringLiteral("update")
                    : QStringLiteral("installed"));
        endCheck();
    });
}

void ComponentsManager::installTool(const QString &key)
{
    if (!m_toolReleases.contains(key)) {
        finishCurrent(false, tr("Нет сведений о выпуске. Проверьте подключение к интернету."));
        return;
    }
    const Release release = m_toolReleases.value(key);
    const QString archive = toolsDir() + QLatin1Char('/') + release.fileName;
    const QString target = toolsDir() + QLatin1Char('/') + toolFile(key);
    QDir().mkpath(toolsDir());

    download(release.url, archive, [this, archive, target, key, release](bool) {
        const auto done = [this, target, release] {
            QFile::setPermissions(target, QFile::permissions(target) | QFile::ExeOwner | QFile::ExeGroup | QFile::ExeOther);
            if (!QFileInfo(target).isExecutable()) {
                finishCurrent(false, tr("Не удалось установить."));
                return;
            }
            writeFile(target + QStringLiteral(".version"), release.tag.toUtf8());
            if (Component *c = find(m_current))
                c->latest = release.tag;
            finishCurrent(true);
        };

        const QString lower = archive.toLower();
        if (!lower.endsWith(QLatin1String(".zip")) && !lower.contains(QLatin1String(".tar"))) {
            if (archive != target) {
                QFile::remove(target);
                QFile::rename(archive, target);
            }
            done();
            return;
        }
        // Архив: распаковываем и ищем в нём исполняемый файл
        setProgress(0.95, tr("Распаковка…"));
        const QString unpackDir = target + QStringLiteral("-dist");
        QDir(unpackDir).removeRecursively();
        QDir().mkpath(unpackDir);
        const bool zip = lower.endsWith(QLatin1String(".zip"));
        auto *unpack = new QProcess(this);
        connect(unpack, &QProcess::finished, this, [this, unpack, archive, unpackDir, target, key, done](int code, QProcess::ExitStatus status) {
            unpack->deleteLater();
            QFile::remove(archive);
            if (status != QProcess::NormalExit || code != 0) {
                finishCurrent(false, tr("Не удалось распаковать."));
                return;
            }
            QDirIterator it(unpackDir, QDir::Files, QDirIterator::Subdirectories);
            while (it.hasNext()) {
                const QString path = it.next();
                const QString name = QFileInfo(path).fileName();
                // Сам инструмент, а не его библиотеки и конфиги
                if (name.startsWith(toolFile(key), Qt::CaseInsensitive) && !name.contains(QLatin1Char('.'))) {
                    QFile::remove(target);
                    QFile::setPermissions(path, QFile::permissions(path) | QFile::ExeOwner | QFile::ExeGroup | QFile::ExeOther);
                    QFile::link(path, target);
                    break;
                }
            }
            done();
        });
        connect(unpack, &QProcess::errorOccurred, this, [this, unpack, archive](QProcess::ProcessError error) {
            if (error != QProcess::FailedToStart)
                return;
            unpack->deleteLater();
            QFile::remove(archive);
            finishCurrent(false, tr("Не удалось распаковать: нет программы распаковки."));
        });
        unpack->start(zip ? QStringLiteral("unzip") : QStringLiteral("tar"),
                      zip ? QStringList{QStringLiteral("-o"), archive, QStringLiteral("-d"), unpackDir}
                          : QStringList{QStringLiteral("-xf"), archive, QStringLiteral("-C"), unpackDir});
    });
}

// ---------- Системные пакеты ----------

void ComponentsManager::checkPackages()
{
    beginCheck();
    auto *proc = new QProcess(this);
    const QStringList packages = {
        QStringLiteral("libvulkan1"), QStringLiteral("libvulkan1:i386"),
        QStringLiteral("mesa-vulkan-drivers"), QStringLiteral("mesa-vulkan-drivers:i386"),
        QStringLiteral("libgl1-mesa-dri"), QStringLiteral("libgl1-mesa-dri:i386"),
        QStringLiteral("nvidia-driver"), QStringLiteral("nvidia-driver-*"),
        QStringLiteral("gamescope"), QStringLiteral("libxcb-cursor0"),
        QStringLiteral("fonts-inter"), QStringLiteral("fonts-inter-variable"),
    };
    connect(proc, &QProcess::finished, this, [this, proc] {
        proc->deleteLater();
        // «libvulkan1:amd64 ii  1.3.275.0-1» — только установленные пакеты
        QHash<QString, QString> installed;
        const QStringList lines = QString::fromUtf8(proc->readAllStandardOutput()).split(QLatin1Char('\n'), Qt::SkipEmptyParts);
        for (const QString &line : lines) {
            const QStringList parts = line.split(QLatin1Char(' '), Qt::SkipEmptyParts);
            if (parts.size() >= 3 && parts.at(1) == QLatin1String("ii"))
                installed.insert(parts.at(0), parts.at(2));
        }
        // Код 1 — часть пакетов не найдена, это нормально
        const bool ok = proc->exitStatus() == QProcess::NormalExit
                        && (!installed.isEmpty() || proc->exitCode() == 0 || proc->exitCode() == 1);
        applyPackageState(installed, ok);
        endCheck();
    });
    connect(proc, &QProcess::errorOccurred, this, [this, proc](QProcess::ProcessError error) {
        if (error != QProcess::FailedToStart)
            return;
        proc->deleteLater();
        applyPackageState({}, false);
        endCheck();
    });
    proc->start(QStringLiteral("dpkg-query"),
                QStringList{QStringLiteral("-W"), QStringLiteral("-f=${Package}:${Architecture} ${db:Status-Abbrev} ${Version}\\n")}
                    + packages);
}

void ComponentsManager::applyPackageState(const QHash<QString, QString> &installed, bool dpkgOk)
{
    // Версия пакета основной архитектуры (amd64 или all)
    const auto native = [&](const QString &pkg) {
        for (auto it = installed.cbegin(); it != installed.cend(); ++it) {
            if (it.key().startsWith(pkg + QLatin1Char(':')) && !it.key().endsWith(QLatin1String(":i386")))
                return it.value();
        }
        return QString();
    };
    const auto foreign32 = [&](const QString &pkg) { return installed.value(pkg + QStringLiteral(":i386")); };
    const auto any = [&](const QString &pkg) {
        const QString v = native(pkg);
        return v.isEmpty() ? foreign32(pkg) : v;
    };

    // --- Системные компоненты: проверяем сами файлы, пакеты — только для версии и шрифта ---
    {
        QStringList missing, missingRecommended;
        const bool gamescope = !findProgram(QStringLiteral("gamescope"), kSystemDirs).isEmpty();
        // Запасные композиторы: Weston (kiosk-shell) и cage
        const bool cage = !findProgram(QStringLiteral("cage"), kSystemDirs).isEmpty()
                          || !findProgram(QStringLiteral("weston"), kSystemDirs).isEmpty();
        // Proton — скрипт на Python 3
        if (findProgram(QStringLiteral("python3"), kSystemDirs).isEmpty())
            missing << QStringLiteral("Python 3");
        // umu запускает игры в контейнере Steam Runtime (bwrap). Ubuntu 24.04+ через AppArmor
        // запрещает программам непривилегированные пространства имён — пакет umu для Ubuntu
        // ставит для этого профиль bwrap-userns-restrict из apparmor-profiles; делаем так же
        if (readFileTrimmed(QStringLiteral("/proc/sys/kernel/apparmor_restrict_unprivileged_userns")) == QLatin1String("1")
            && !QFileInfo::exists(QStringLiteral("/etc/apparmor.d/bwrap-userns-restrict"))
            && !QFileInfo::exists(QStringLiteral("/etc/apparmor.d/bwrap-userns-restrict-umu")))
            missingRecommended << tr("профиль AppArmor для umu (bwrap)");
        if (!libraryExists(QStringLiteral("libxcb-cursor.so.0")) && native(QStringLiteral("libxcb-cursor0")).isEmpty())
            missing << QStringLiteral("libxcb-cursor0");
        if (!gamescope && !cage)
            missing << QStringLiteral("gamescope");
        else if (!gamescope)
            missingRecommended << QStringLiteral("gamescope");
        if (gamescope && findProgram(QStringLiteral("Xwayland"), kSystemDirs).isEmpty())
            missing << QStringLiteral("Xwayland");
        if (findProgram(QStringLiteral("wpctl"), kSystemDirs).isEmpty() && findProgram(QStringLiteral("pactl"), kSystemDirs).isEmpty())
            missingRecommended << tr("управление звуком (WirePlumber)");
        if (native(QStringLiteral("fonts-inter")).isEmpty() && native(QStringLiteral("fonts-inter-variable")).isEmpty()
            && !interFontInstalled())
            missingRecommended << tr("шрифт Inter");

        if (Component *c = find(QStringLiteral("system")))
            c->version = native(QStringLiteral("gamescope")).isEmpty() ? QString()
                                                                       : QStringLiteral("gamescope ") + native(QStringLiteral("gamescope"));
        const bool attempted = m_attempted.contains(QStringLiteral("system"));
        if (!missing.isEmpty()) {
            setState(QStringLiteral("system"), QStringLiteral("missing"), tr("Не хватает: %1").arg(missing.join(QStringLiteral(", "))));
        } else if (!missingRecommended.isEmpty()) {
            // Помощник уже пробовал — значит, этого нет в репозиториях системы; не зацикливаемся
            setState(QStringLiteral("system"), attempted ? QStringLiteral("installed") : QStringLiteral("missing"),
                     (attempted ? tr("Нет в репозиториях: %1") : tr("Не хватает: %1")).arg(missingRecommended.join(QStringLiteral(", "))));
        } else {
            setState(QStringLiteral("system"), QStringLiteral("installed"));
        }
    }

    if (!dpkgOk) {
        const QString error = tr("Не удалось проверить пакеты: нужна система на базе Ubuntu или Debian.");
        setState(QStringLiteral("drivers"), QStringLiteral("failed"), error);
        setState(QStringLiteral("vulkan"), QStringLiteral("failed"), error);
        return;
    }

    const bool nvidia = m_gpuVendor == QLatin1String("nvidia");

    // Vulkan: загрузчик 64 и 32 бита, для AMD/Intel ещё и драйверы Mesa Vulkan
    const bool vulkan = !native(QStringLiteral("libvulkan1")).isEmpty() && !foreign32(QStringLiteral("libvulkan1")).isEmpty()
                        && (nvidia || (!native(QStringLiteral("mesa-vulkan-drivers")).isEmpty()
                                       && !foreign32(QStringLiteral("mesa-vulkan-drivers")).isEmpty()));
    if (Component *c = find(QStringLiteral("vulkan")))
        c->version = native(QStringLiteral("libvulkan1"));
    setState(QStringLiteral("vulkan"), vulkan ? QStringLiteral("installed") : QStringLiteral("missing"));

    // Драйвер: Ubuntu — nvidia-driver-<версия>[-open|-server], Debian — nvidia-driver
    QString driverVersion;
    if (nvidia) {
        static const QRegularExpression nvidiaPkg(QStringLiteral("^nvidia-driver(-\\d+[a-z-]*)?:"));
        for (auto it = installed.cbegin(); it != installed.cend(); ++it) {
            if (nvidiaPkg.match(it.key()).hasMatch() && !it.key().endsWith(QLatin1String(":i386")))
                driverVersion = it.value();
        }
        if (driverVersion.isEmpty() && QFileInfo::exists(QStringLiteral("/proc/driver/nvidia/version")))
            driverVersion = tr("загружен");
    } else if (!foreign32(QStringLiteral("libgl1-mesa-dri")).isEmpty()) {
        driverVersion = native(QStringLiteral("libgl1-mesa-dri"));
    }
    if (Component *c = find(QStringLiteral("drivers")))
        c->version = driverVersion;
    setState(QStringLiteral("drivers"), driverVersion.isEmpty() ? QStringLiteral("missing") : QStringLiteral("installed"),
             !driverVersion.isEmpty() && nvidia && m_rebootRequired ? tr("Заработает после перезагрузки") : QString());
}

// ---------- Proton ----------

void ComponentsManager::checkProtonLocal()
{
    QStringList versions;
    const QDir dir(compatToolsDir());
    for (const QString &name : dir.entryList({kProtonPrefix + QLatin1Char('*')}, QDir::Dirs | QDir::NoDotAndDotDot)) {
        if (QFileInfo(dir.filePath(name) + QStringLiteral("/proton")).isFile())
            versions.append(name);
    }
    QCollator collator;
    collator.setNumericMode(true);
    std::sort(versions.begin(), versions.end(), collator);
    if (Component *c = find(QStringLiteral("proton")))
        c->version = versions.isEmpty() ? QString() : versions.constLast();
    qInfo("Proton: found %s in %s", versions.isEmpty() ? "nothing" : qPrintable(versions.join(QStringLiteral(", "))),
          qPrintable(compatToolsDir()));
    setState(QStringLiteral("proton"), versions.isEmpty() ? QStringLiteral("missing") : QStringLiteral("installed"));
}

void ComponentsManager::fetchProtonRelease()
{
    beginCheck();
    fetchRelease(kProtonRepo, [this](const QJsonObject &release, const QString &error) {
        Component *c = find(QStringLiteral("proton"));
        const QString local = c ? c->version : QString();
        if (!error.isEmpty()) {
            setState(QStringLiteral("proton"), local.isEmpty() ? QStringLiteral("missing") : QStringLiteral("installed"),
                     local.isEmpty() ? error : tr("Обновления не проверены: %1").arg(error));
            endCheck();
            return;
        }
        const QString tag = release.value(QStringLiteral("tag_name")).toString();
        Release found;
        found.tag = tag;
        for (const QJsonValue &value : release.value(QStringLiteral("assets")).toArray()) {
            const QJsonObject asset = value.toObject();
            const QString name = asset.value(QStringLiteral("name")).toString();
            const QUrl url(asset.value(QStringLiteral("browser_download_url")).toString());
            if (name.endsWith(QLatin1String(".tar.gz"))) {
                found.url = url;
                found.fileName = QFileInfo(name).fileName();
            } else if (name.endsWith(QLatin1String(".sha512sum"))) {
                found.checksumUrl = url;
            }
        }
        m_proton = found;
        if (c)
            c->latest = tag;
        const QString state = local.isEmpty() ? QStringLiteral("missing")
                            : (!tag.isEmpty() && local != tag) ? QStringLiteral("update")
                            : QStringLiteral("installed");
        setState(QStringLiteral("proton"), state);
        endCheck();
    });
}

// ---------- Установка ----------

void ComponentsManager::install(const QString &key)
{
    Component *c = find(key);
    if (!c || key == m_current || m_queue.contains(key) || c->state == QLatin1String("installed"))
        return;
    c->state = QStringLiteral("queued");
    c->message.clear();
    c->progress = -1;
    // Очередь держим в порядке установки: драйверы раньше Vulkan, Vulkan раньше Proton
    auto pos = std::find_if(m_queue.begin(), m_queue.end(), [&](const QString &k) { return rank(k) > rank(key); });
    m_queue.insert(pos, key);
    emit componentsChanged();
    processQueue();
}

void ComponentsManager::installAll()
{
    // install() может сразу начать установку и поменять m_items — идём по копии ключей
    QStringList keys;
    for (const Component &c : std::as_const(m_items)) {
        if (c.state == QLatin1String("missing") || c.state == QLatin1String("update") || c.state == QLatin1String("failed"))
            keys.append(c.key);
    }
    for (const QString &key : std::as_const(keys))
        install(key);
}

void ComponentsManager::processQueue()
{
    if (!m_current.isEmpty() || m_queue.isEmpty())
        return;
    m_current = m_queue.takeFirst();
    if (Component *c = find(m_current)) {
        c->state = QStringLiteral("installing");
        c->progress = -1;
        c->message = tr("Подготовка…");
    }
    emit componentsChanged();
    if (m_current == QLatin1String("proton"))
        downloadProton();
    else if (isTool(m_current))
        installTool(m_current);
    else
        runHelper(m_current);
}

void ComponentsManager::finishCurrent(bool ok, const QString &message)
{
    const QString key = m_current;
    m_current.clear();
    if (Component *c = find(key)) {
        c->progress = -1;
        if (ok) {
            c->state = QStringLiteral("installed");
            c->message = key == QLatin1String("drivers") && m_rebootRequired ? tr("Заработает после перезагрузки") : QString();
            if (key == QLatin1String("proton") || isTool(key))
                c->version = c->latest;
        } else {
            c->state = QStringLiteral("failed");
            c->message = message;
        }
    }
    qInfo("Component %s: %s", qPrintable(key), ok ? "installed" : qPrintable(message));
    emit componentsChanged();

    // Уточняем версии установленных пакетов
    if (ok && key != QLatin1String("proton") && !isTool(key))
        checkPackages();

    processQueue();
    if (m_current.isEmpty() && m_queue.isEmpty())
        emit finished();
}

void ComponentsManager::runHelper(const QString &key)
{
    const QString helper = componentsHelperPath();
    if (helper.isEmpty()) {
        finishCurrent(false, tr("Помощник установки не найден — установите оболочку (sudo cmake --install build)."));
        return;
    }
    m_attempted.insert(key);

    m_process = new QProcess(this);
    m_process->setProcessChannelMode(QProcess::SeparateChannels);
    const auto parseOutput = [this] {
        while (m_process && m_process->canReadLine()) {
            const QString line = QString::fromUtf8(m_process->readLine()).trimmed();
            if (line == QLatin1String("REBOOT")) {
                m_rebootRequired = true;
                continue;
            }
            if (!line.startsWith(QLatin1String("PROGRESS ")))
                continue;
            const QString rest = line.mid(9);
            const qsizetype space = rest.indexOf(QLatin1Char(' '));
            const int percent = qBound(0, (space > 0 ? rest.left(space) : rest).toInt(), 100);
            setProgress(percent / 100.0, space > 0 ? rest.mid(space + 1) : QString());
        }
    };
    connect(m_process, &QProcess::readyReadStandardOutput, this, parseOutput);
    // Весь вывод помощника (apt) — в журнал ~/.local/state/redanchor/components.log,
    // а последняя ошибка — в сообщение компонента
    const QString logDir = QDir::homePath() + QStringLiteral("/.local/state/redanchor");
    QDir().mkpath(logDir);
    auto log = std::make_shared<QFile>(logDir + QStringLiteral("/components.log"));
    if (log->open(QIODevice::Append))
        log->write("\n==== " + QDateTime::currentDateTime().toString(Qt::ISODate).toUtf8() + " " + key.toUtf8() + " ====\n");
    auto lastError = std::make_shared<QString>();
    auto aptError = std::make_shared<QString>();   // ошибки apt/dpkg важнее остальных
    const auto takeErrors = [this, log, lastError, aptError] {
        const QByteArray text = m_process->readAllStandardError();
        if (text.isEmpty())
            return;
        if (log->isOpen()) {
            log->write(text);
            log->flush();
        }
        for (const QByteArray &raw : text.split('\n')) {
            const QString line = QString::fromUtf8(raw).trimmed();
            if (line.isEmpty())
                continue;
            qInfo("redanchor-components: %s", qPrintable(line));
            // «E: …» — ошибка apt, «redanchor-components: …» — сообщение помощника
            if (line.startsWith(QLatin1String("E: ")))
                *aptError = line.mid(3);
            else if (line.startsWith(QLatin1String("dpkg: error")) || line.startsWith(QLatin1String("dpkg: ошибка")))
                *aptError = line;
            else if (line.startsWith(QLatin1String("ERROR ")))
                *lastError = line.mid(6);
            else if (line.startsWith(QLatin1String("redanchor-components: ")) && line.contains(QLatin1String("не ")))
                *lastError = line.mid(22);
        }
    };
    connect(m_process, &QProcess::readyReadStandardError, this, takeErrors);
    connect(m_process, &QProcess::finished, this, [this, parseOutput, takeErrors, lastError, aptError](int code, QProcess::ExitStatus status) {
        parseOutput();   // последние строки (REBOOT) могли прийти вместе с завершением
        takeErrors();
        m_process->deleteLater();
        m_process = nullptr;
        const bool ok = status == QProcess::NormalExit && code == 0;
        // 126/127 — pkexec: в доступе отказано или аутентификация не пройдена
        QString message;
        if (!ok) {
            message = code == 126 || code == 127 ? tr("Нет прав на установку (pkexec).")
                    : !aptError->isEmpty() ? *aptError
                    : !lastError->isEmpty() ? *lastError
                    : tr("Ошибка установки (код %1).").arg(code);
            message += tr(" Подробности: ~/.local/state/redanchor/components.log");
        }
        finishCurrent(ok, message);
    });
    connect(m_process, &QProcess::errorOccurred, this, [this](QProcess::ProcessError error) {
        if (error != QProcess::FailedToStart)
            return;
        m_process->deleteLater();
        m_process = nullptr;
        finishCurrent(false, tr("Не удалось запустить установку (нужен pkexec)."));
    });
    m_process->start(QStringLiteral("pkexec"), {helper, key});
}

void ComponentsManager::downloadProton()
{
    if (!m_proton.url.isValid() || m_proton.fileName.isEmpty()) {
        finishCurrent(false, tr("Нет сведений о выпуске Proton. Проверьте подключение к интернету."));
        return;
    }
    const QString dir = QStandardPaths::writableLocation(QStandardPaths::CacheLocation) + QStringLiteral("/downloads");
    QDir().mkpath(dir);
    const QString target = dir + QLatin1Char('/') + m_proton.fileName;
    download(m_proton.url, target, [this, target](bool) {
        qInfo("Proton: downloaded %s (%lld bytes)", qPrintable(target), qlonglong(QFileInfo(target).size()));
        verifyAndExtractProton(target);
    });
}

void ComponentsManager::verifyAndExtractProton(const QString &archive)
{
    if (!m_proton.checksumUrl.isValid()) {
        qWarning("Proton: no checksum in the release, skipping verification");
        extractProton(archive);
        return;
    }
    setProgress(0.92, tr("Проверка архива…"));

    // Контрольная сумма с GitHub; SHA-512 считаем в отдельном процессе, чтобы не блокировать интерфейс
    QNetworkReply *reply = m_network->get(githubRequest(m_proton.checksumUrl, kApiTimeoutMs));
    connect(reply, &QNetworkReply::finished, this, [this, reply, archive] {
        reply->deleteLater();
        const QString expected = QString::fromLatin1(reply->readAll()).section(QLatin1Char(' '), 0, 0).trimmed().toLower();
        static const QRegularExpression sha512(QStringLiteral("^[0-9a-f]{128}$"));
        if (reply->error() != QNetworkReply::NoError || !sha512.match(expected).hasMatch()) {
            // Без суммы tar всё равно проверит целостность gzip
            qWarning("Proton checksum unavailable (%s), skipping verification", qPrintable(reply->errorString()));
            extractProton(archive);
            return;
        }
        auto *sum = new QProcess(this);
        connect(sum, &QProcess::finished, this, [this, sum, archive, expected] {
            sum->deleteLater();
            const QString actual = QString::fromLatin1(sum->readAllStandardOutput()).section(QLatin1Char(' '), 0, 0).trimmed().toLower();
            qInfo("Proton: checksum %s", actual == expected ? "ok" : "MISMATCH");
            if (actual != expected) {
                QFile::remove(archive);
                finishCurrent(false, tr("Архив Proton повреждён. Попробуйте ещё раз."));
                return;
            }
            extractProton(archive);
        });
        connect(sum, &QProcess::errorOccurred, this, [this, sum, archive](QProcess::ProcessError error) {
            if (error != QProcess::FailedToStart)
                return;
            sum->deleteLater();
            qWarning("Proton: sha512sum unavailable, skipping verification");
            extractProton(archive);
        });
        sum->start(QStringLiteral("sha512sum"), {archive});
    });
}

// Распаковка во временный каталог рядом и перенос на место: прерванная распаковка не
// оставит «установленный» наполовину Proton, а старые версии удаляются только после
// того, как новая на месте.
void ComponentsManager::extractProton(const QString &archive)
{
    setProgress(0.95, tr("Распаковка…"));
    const QString tools = compatToolsDir();
    const QString staging = tools + QLatin1Char('/') + kProtonStaging;
    QDir(staging).removeRecursively();
    QDir().mkpath(staging);
    qInfo("Proton: extracting %s (%lld bytes) to %s", qPrintable(archive), qlonglong(QFileInfo(archive).size()), qPrintable(staging));

    auto *tar = new QProcess(this);
    tar->setProcessChannelMode(QProcess::MergedChannels);
    connect(tar, &QProcess::finished, this, [this, tar, archive, tools, staging](int code, QProcess::ExitStatus status) {
        const QString output = QString::fromUtf8(tar->readAll()).trimmed();
        tar->deleteLater();
        QFile::remove(archive);
        if (status != QProcess::NormalExit || code != 0) {
            qWarning("Proton: tar failed (exit %d): %s", code, qPrintable(output));
            QDir(staging).removeRecursively();
            finishCurrent(false, tr("Не удалось распаковать Proton (не хватает места?)."));
            return;
        }
        // Каталог с файлом proton — независимо от того, совпадает ли его имя с тегом выпуска
        const QDir stage(staging);
        QString name;
        for (const QString &entry : stage.entryList(QDir::Dirs | QDir::NoDotAndDotDot)) {
            if (QFileInfo(stage.filePath(entry) + QStringLiteral("/proton")).isFile())
                name = entry;
        }
        if (name.isEmpty()) {
            qWarning("Proton: no directory with 'proton' after extraction; contents: %s",
                     qPrintable(stage.entryList(QDir::Dirs | QDir::NoDotAndDotDot).join(QStringLiteral(", "))));
            QDir(staging).removeRecursively();
            finishCurrent(false, tr("Proton распакован не полностью. Попробуйте ещё раз."));
            return;
        }
        const QDir dir(tools);
        const QString final = dir.filePath(name);
        // Переустановка той же версии: старый каталог убираем
        if (QFileInfo::exists(final))
            QDir(final).removeRecursively();
        if (!QDir().rename(stage.filePath(name), final)) {
            qWarning("Proton: cannot move %s to %s", qPrintable(stage.filePath(name)), qPrintable(final));
            QDir(staging).removeRecursively();
            finishCurrent(false, tr("Не удалось установить Proton."));
            return;
        }
        QDir(staging).removeRecursively();
        qInfo("Proton: installed %s", qPrintable(final));
        // Старые выпуски GE-Proton — после того, как новый на месте
        for (const QString &old : dir.entryList({kProtonPrefix + QLatin1Char('*')}, QDir::Dirs | QDir::NoDotAndDotDot)) {
            if (old != name) {
                qInfo("Proton: removing old %s", qPrintable(old));
                QDir(dir.filePath(old)).removeRecursively();
            }
        }
        if (Component *c = find(m_current))
            c->latest = name;   // finishCurrent запишет его в version
        finishCurrent(true);
    });
    connect(tar, &QProcess::errorOccurred, this, [this, tar, archive](QProcess::ProcessError error) {
        if (error != QProcess::FailedToStart)
            return;
        tar->deleteLater();
        QFile::remove(archive);
        finishCurrent(false, tr("Не удалось распаковать Proton: нет программы tar."));
    });
    tar->start(QStringLiteral("tar"), {QStringLiteral("-xzf"), archive, QStringLiteral("-C"), staging});
}
