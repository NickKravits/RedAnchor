#include "componentsmanager.h"

#include <QCollator>
#include <QCoreApplication>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QProcess>
#include <QStandardPaths>

#include <algorithm>

namespace {

const QUrl kProtonRelease(QStringLiteral("https://api.github.com/repos/GloriousEggroll/proton-ge-custom/releases/latest"));
const QString kProtonPrefix = QStringLiteral("GE-Proton");

// Установленный помощник, а при запуске из каталога сборки — соседний файл.
QString componentsHelperPath()
{
    const QString installed = QStringLiteral(REDANCHOR_COMPONENTS_HELPER);
    if (QFileInfo::exists(installed))
        return installed;
    const QString local = QCoreApplication::applicationDirPath() + QStringLiteral("/redanchor-components");
    return QFileInfo::exists(local) ? local : QString();
}

QNetworkRequest githubRequest(const QUrl &url)
{
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("RedAnchor-Shell"));
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
    return request;
}

QString readFileTrimmed(const QString &path)
{
    QFile file(path);
    return file.open(QIODevice::ReadOnly) ? QString::fromLatin1(file.readAll()).trimmed() : QString();
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
    m_items = {
        {QStringLiteral("drivers"), tr("Графический драйвер"), driver},
        {QStringLiteral("vulkan"), tr("Vulkan"), tr("Графический интерфейс для игр и Proton")},
        {QStringLiteral("proton"), tr("Proton"), tr("GE-Proton — запуск игр для Windows")},
        {QStringLiteral("depotdownloader"), tr("DepotDownloader"), tr("Загрузка игр из Steam")},
        {QStringLiteral("gogdl"), tr("gogdl"), tr("Загрузка игр из GOG")},
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
    checkPackages();
    checkProtonLocal();
    fetchProtonRelease();
    for (const QString &key : {QStringLiteral("depotdownloader"), QStringLiteral("gogdl")}) {
        checkToolLocal(key);
        fetchToolRelease(key);
    }
}

// ---------- Загрузчики игр (DepotDownloader, gogdl) ----------

QString ComponentsManager::toolsDir() const
{
    return QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + QStringLiteral("/tools");
}

QString ComponentsManager::toolFile(const QString &key)
{
    return key == QLatin1String("depotdownloader") ? QStringLiteral("DepotDownloader") : QStringLiteral("gogdl");
}

QString ComponentsManager::toolRepo(const QString &key)
{
    return key == QLatin1String("depotdownloader") ? QStringLiteral("SteamRE/DepotDownloader")
                                                   : QStringLiteral("Heroic-Games-Launcher/heroic-gogdl");
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
    QNetworkReply *reply = m_network->get(githubRequest(QUrl(QStringLiteral("https://api.github.com/repos/%1/releases/latest").arg(toolRepo(key)))));
    connect(reply, &QNetworkReply::finished, this, [this, reply, key] {
        reply->deleteLater();
        Component *c = find(key);
        const QString local = c ? c->version : QString();
        const bool installed = QFileInfo(toolsDir() + QLatin1Char('/') + toolFile(key)).isExecutable();
        if (reply->error() != QNetworkReply::NoError) {
            setState(key, installed ? QStringLiteral("installed") : QStringLiteral("missing"),
                     installed ? tr("Не удалось проверить обновления") : tr("Нет подключения к интернету"));
            endCheck();
            return;
        }
        const QJsonObject release = QJsonDocument::fromJson(reply->readAll()).object();
        const QString tag = release.value(QStringLiteral("tag_name")).toString();
        // Сборка для Linux x64: архив или исполняемый файл
        QString url, name;
        for (const QJsonValue &value : release.value(QStringLiteral("assets")).toArray()) {
            const QJsonObject asset = value.toObject();
            const QString assetName = asset.value(QStringLiteral("name")).toString();
            const QString lower = assetName.toLower();
            if (!lower.contains(QLatin1String("linux")) || lower.contains(QLatin1String("arm"))
                || lower.contains(QLatin1String("aarch64")) || lower.endsWith(QLatin1String(".sha256"))
                || lower.endsWith(QLatin1String(".sig")) || lower.endsWith(QLatin1String(".asc")))
                continue;
            url = asset.value(QStringLiteral("browser_download_url")).toString();
            name = assetName;
            if (lower.contains(QLatin1String("x64")) || lower.contains(QLatin1String("x86_64")) || lower.contains(QLatin1String("amd64")))
                break;
        }
        if (url.isEmpty()) {
            qWarning("No Linux build of %s in the latest release", qPrintable(toolRepo(key)));
            setState(key, installed ? QStringLiteral("installed") : QStringLiteral("failed"),
                     installed ? QString() : tr("Не найдена сборка для Linux"));
            endCheck();
            return;
        }
        m_toolReleases.insert(key, {tag, url, name});
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
    const QStringList release = m_toolReleases.value(key);
    if (release.size() < 3) {
        finishCurrent(false, tr("Нет сведений о выпуске. Проверьте подключение к интернету."));
        return;
    }
    const QString tag = release.at(0);
    const QString archive = toolsDir() + QLatin1Char('/') + release.at(2);
    const QString target = toolsDir() + QLatin1Char('/') + toolFile(key);
    QDir().mkpath(toolsDir());

    auto *file = new QFile(archive + QStringLiteral(".part"), this);
    if (!file->open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        file->deleteLater();
        finishCurrent(false, tr("Не удалось сохранить загрузку."));
        return;
    }
    m_download = m_network->get(githubRequest(QUrl(release.at(1))));
    connect(m_download, &QNetworkReply::readyRead, this, [this, file] { file->write(m_download->readAll()); });
    connect(m_download, &QNetworkReply::downloadProgress, this, [this](qint64 received, qint64 total) {
        if (Component *c = find(m_current)) {
            c->progress = total > 0 ? 0.9 * double(received) / double(total) : -1;
            c->message = tr("Загрузка: %1 МБ").arg(received / 1048576);
        }
        emit componentsChanged();
    });
    connect(m_download, &QNetworkReply::finished, this, [this, file, archive, target, tag, key] {
        file->write(m_download->readAll());
        file->close();
        const bool ok = m_download->error() == QNetworkReply::NoError;
        m_download->deleteLater();
        m_download = nullptr;
        file->deleteLater();
        if (!ok) {
            QFile::remove(file->fileName());
            finishCurrent(false, tr("Загрузка прервалась. Проверьте подключение к интернету."));
            return;
        }
        QFile::remove(archive);
        QFile::rename(file->fileName(), archive);

        const auto done = [this, target, tag] {
            QFile::setPermissions(target, QFile::permissions(target) | QFile::ExeOwner | QFile::ExeGroup | QFile::ExeOther);
            if (!QFileInfo(target).isExecutable()) {
                finishCurrent(false, tr("Не удалось установить."));
                return;
            }
            QFile version(target + QStringLiteral(".version"));
            if (version.open(QIODevice::WriteOnly | QIODevice::Truncate))
                version.write(tag.toUtf8());
            finishCurrent(true);
        };

        const QString lower = archive.toLower();
        if (!lower.endsWith(QLatin1String(".zip")) && !lower.contains(QLatin1String(".tar"))) {
            QFile::remove(target);
            QFile::rename(archive, target);
            done();
            return;
        }
        // Архив: распаковываем и ищем в нём исполняемый файл
        if (Component *c = find(m_current)) {
            c->progress = 0.95;
            c->message = tr("Распаковка…");
        }
        emit componentsChanged();
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
        unpack->start(zip ? QStringLiteral("unzip") : QStringLiteral("tar"),
                      zip ? QStringList{QStringLiteral("-o"), archive, QStringLiteral("-d"), unpackDir}
                          : QStringList{QStringLiteral("-xf"), archive, QStringLiteral("-C"), unpackDir});
    });
}

void ComponentsManager::checkPackages()
{
    beginCheck();
    auto *proc = new QProcess(this);
    const QStringList packages = {
        QStringLiteral("libvulkan1"), QStringLiteral("libvulkan1:i386"),
        QStringLiteral("mesa-vulkan-drivers"), QStringLiteral("mesa-vulkan-drivers:i386"),
        QStringLiteral("libgl1-mesa-dri"), QStringLiteral("libgl1-mesa-dri:i386"),
        QStringLiteral("nvidia-driver-*"),
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
        const auto native = [&](const QString &pkg) {
            for (auto it = installed.cbegin(); it != installed.cend(); ++it) {
                if (it.key().startsWith(pkg + QLatin1Char(':')) && !it.key().endsWith(QLatin1String(":i386")))
                    return it.value();
            }
            return QString();
        };
        const auto foreign32 = [&](const QString &pkg) { return installed.value(pkg + QStringLiteral(":i386")); };

        if (proc->exitStatus() != QProcess::NormalExit || (installed.isEmpty() && proc->exitCode() != 0 && proc->exitCode() != 1)) {
            const QString error = tr("Не удалось проверить пакеты: нужна система на базе Ubuntu или Debian.");
            setState(QStringLiteral("drivers"), QStringLiteral("failed"), error);
            setState(QStringLiteral("vulkan"), QStringLiteral("failed"), error);
            endCheck();
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

        // Драйвер
        QString driverVersion;
        if (nvidia) {
            for (auto it = installed.cbegin(); it != installed.cend(); ++it) {
                if (it.key().startsWith(QLatin1String("nvidia-driver-")))
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
        endCheck();
    });
    connect(proc, &QProcess::errorOccurred, this, [this, proc](QProcess::ProcessError error) {
        if (error != QProcess::FailedToStart)
            return;
        proc->deleteLater();
        const QString message = tr("Не удалось проверить пакеты: нужна система на базе Ubuntu или Debian.");
        setState(QStringLiteral("drivers"), QStringLiteral("failed"), message);
        setState(QStringLiteral("vulkan"), QStringLiteral("failed"), message);
        endCheck();
    });
    proc->start(QStringLiteral("dpkg-query"),
                QStringList{QStringLiteral("-W"), QStringLiteral("-f=${Package}:${Architecture} ${db:Status-Abbrev} ${Version}\\n")}
                    + packages);
}

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
    QNetworkReply *reply = m_network->get(githubRequest(kProtonRelease));
    connect(reply, &QNetworkReply::finished, this, [this, reply] {
        reply->deleteLater();
        Component *c = find(QStringLiteral("proton"));
        const QString local = c ? c->version : QString();
        if (reply->error() != QNetworkReply::NoError) {
            qWarning("Proton release check failed: %s", qPrintable(reply->errorString()));
            setState(QStringLiteral("proton"), local.isEmpty() ? QStringLiteral("missing") : QStringLiteral("installed"),
                     local.isEmpty() ? tr("Нет подключения к интернету") : tr("Не удалось проверить обновления"));
            endCheck();
            return;
        }
        const QJsonObject release = QJsonDocument::fromJson(reply->readAll()).object();
        const QString tag = release.value(QStringLiteral("tag_name")).toString();
        m_protonUrl.clear();
        m_protonChecksumUrl.clear();
        for (const QJsonValue &value : release.value(QStringLiteral("assets")).toArray()) {
            const QJsonObject asset = value.toObject();
            const QString name = asset.value(QStringLiteral("name")).toString();
            const QUrl url(asset.value(QStringLiteral("browser_download_url")).toString());
            if (name.endsWith(QLatin1String(".tar.gz")))
                m_protonUrl = url;
            else if (name.endsWith(QLatin1String(".sha512sum")))
                m_protonChecksumUrl = url;
        }
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
    m_queue.append(key);
    emit componentsChanged();
    processQueue();
}

void ComponentsManager::installAll()
{
    for (const Component &c : std::as_const(m_items)) {
        if (c.state == QLatin1String("missing") || c.state == QLatin1String("update") || c.state == QLatin1String("failed"))
            install(c.key);
    }
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
    else if (m_current == QLatin1String("depotdownloader") || m_current == QLatin1String("gogdl"))
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
            if (key == QLatin1String("proton") || key == QLatin1String("depotdownloader") || key == QLatin1String("gogdl"))
                c->version = c->latest;
        } else {
            c->state = QStringLiteral("failed");
            c->message = message;
        }
    }
    qInfo("Component %s: %s", qPrintable(key), ok ? "installed" : qPrintable(message));
    emit componentsChanged();

    // Уточняем версии установленных пакетов
    if (ok && key != QLatin1String("proton"))
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

    m_process = new QProcess(this);
    m_process->setProcessChannelMode(QProcess::SeparateChannels);
    connect(m_process, &QProcess::readyReadStandardOutput, this, [this] {
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
            if (Component *c = find(m_current)) {
                c->progress = qBound(0, rest.left(space).toInt(), 100) / 100.0;
                c->message = space > 0 ? rest.mid(space + 1) : QString();
            }
            emit componentsChanged();
        }
    });
    connect(m_process, &QProcess::readyReadStandardError, this, [this] {
        const QByteArray text = m_process->readAllStandardError().trimmed();
        if (!text.isEmpty())
            qInfo("redanchor-components: %s", text.constData());
    });
    connect(m_process, &QProcess::finished, this, [this](int code, QProcess::ExitStatus status) {
        m_process->deleteLater();
        m_process = nullptr;
        const bool ok = status == QProcess::NormalExit && code == 0;
        // 126/127 — pkexec: в доступе отказано или аутентификация не пройдена
        finishCurrent(ok, ok ? QString()
                          : code == 126 || code == 127 ? tr("Нет прав на установку.")
                          : tr("Не удалось установить. Проверьте подключение к интернету."));
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
    if (!m_protonUrl.isValid()) {
        finishCurrent(false, tr("Нет сведений о выпуске Proton. Проверьте подключение к интернету."));
        return;
    }
    const QString dir = QStandardPaths::writableLocation(QStandardPaths::CacheLocation) + QStringLiteral("/downloads");
    QDir().mkpath(dir);
    const QString target = dir + QLatin1Char('/') + m_protonUrl.fileName();
    auto *file = new QFile(target + QStringLiteral(".part"), this);
    if (!file->open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        file->deleteLater();
        finishCurrent(false, tr("Не удалось сохранить загрузку."));
        return;
    }

    m_download = m_network->get(githubRequest(m_protonUrl));
    connect(m_download, &QNetworkReply::readyRead, this, [this, file] { file->write(m_download->readAll()); });
    connect(m_download, &QNetworkReply::downloadProgress, this, [this](qint64 received, qint64 total) {
        if (Component *c = find(m_current)) {
            c->progress = total > 0 ? 0.9 * double(received) / double(total) : -1;
            c->message = total > 0 ? tr("Загрузка: %1 из %2 МБ").arg(received / 1048576).arg(total / 1048576)
                                   : tr("Загрузка: %1 МБ").arg(received / 1048576);
        }
        emit componentsChanged();
    });
    connect(m_download, &QNetworkReply::finished, this, [this, file, target] {
        file->write(m_download->readAll());
        file->close();
        const bool ok = m_download->error() == QNetworkReply::NoError;
        const QString error = m_download->errorString();
        m_download->deleteLater();
        m_download = nullptr;
        file->deleteLater();
        if (!ok) {
            QFile::remove(file->fileName());
            qWarning("Proton download failed: %s", qPrintable(error));
            finishCurrent(false, tr("Загрузка прервалась. Проверьте подключение к интернету."));
            return;
        }
        QFile::remove(target);
        QFile::rename(file->fileName(), target);
        qInfo("Proton: downloaded %s (%lld bytes)", qPrintable(target), qlonglong(QFileInfo(target).size()));
        verifyAndExtractProton(target);
    });
}

void ComponentsManager::verifyAndExtractProton(const QString &archive)
{
    const auto extract = [this, archive] {
        if (Component *c = find(m_current)) {
            c->progress = 0.95;
            c->message = tr("Распаковка…");
        }
        emit componentsChanged();
        QDir().mkpath(compatToolsDir());
        // Что было до распаковки: новый Proton — каталог, которого не было
        const QStringList before = QDir(compatToolsDir()).entryList(QDir::Dirs | QDir::NoDotAndDotDot);
        qInfo("Proton: extracting %s (%lld bytes) to %s", qPrintable(archive),
              qlonglong(QFileInfo(archive).size()), qPrintable(compatToolsDir()));
        auto *tar = new QProcess(this);
        tar->setProcessChannelMode(QProcess::MergedChannels);
        connect(tar, &QProcess::finished, this, [this, tar, archive, before](int code, QProcess::ExitStatus status) {
            const QString output = QString::fromUtf8(tar->readAll()).trimmed();
            tar->deleteLater();
            QFile::remove(archive);
            if (status != QProcess::NormalExit || code != 0) {
                qWarning("Proton: tar failed (exit %d): %s", code, qPrintable(output));
                finishCurrent(false, tr("Не удалось распаковать Proton."));
                return;
            }
            // Новый каталог с файлом proton — независимо от того, совпадает ли его имя с тегом выпуска
            const QDir dir(compatToolsDir());
            QString installed;
            for (const QString &name : dir.entryList(QDir::Dirs | QDir::NoDotAndDotDot)) {
                if (!before.contains(name) && QFileInfo(dir.filePath(name) + QStringLiteral("/proton")).isFile())
                    installed = name;
            }
            if (installed.isEmpty()) {
                // Переустановка той же версии: каталог был и раньше
                const Component *c = find(m_current);
                const QString latest = c ? c->latest : QString();
                if (!latest.isEmpty() && QFileInfo(dir.filePath(latest) + QStringLiteral("/proton")).isFile())
                    installed = latest;
            }
            if (installed.isEmpty()) {
                qWarning("Proton: no directory with 'proton' after extraction; contents: %s",
                         qPrintable(dir.entryList(QDir::Dirs | QDir::NoDotAndDotDot).join(QStringLiteral(", "))));
                finishCurrent(false, tr("Proton распакован не полностью. Попробуйте ещё раз."));
                return;
            }
            qInfo("Proton: installed %s", qPrintable(dir.filePath(installed)));
            // Старые выпуски удаляем только после того, как новый проверен
            for (const QString &name : before) {
                if (name != installed && name.startsWith(kProtonPrefix))
                    QDir(dir.filePath(name)).removeRecursively();
            }
            if (Component *c = find(m_current))
                c->latest = installed;   // finishCurrent запишет его в version
            finishCurrent(true);
        });
        tar->start(QStringLiteral("tar"), {QStringLiteral("-xzf"), archive, QStringLiteral("-C"), compatToolsDir()});
    };

    if (!m_protonChecksumUrl.isValid()) {
        extract();
        return;
    }
    if (Component *c = find(m_current)) {
        c->progress = 0.92;
        c->message = tr("Проверка архива…");
    }
    emit componentsChanged();

    // Контрольная сумма с GitHub и sha512sum — без блокировки интерфейса на большом архиве
    QNetworkReply *reply = m_network->get(githubRequest(m_protonChecksumUrl));
    connect(reply, &QNetworkReply::finished, this, [this, reply, archive, extract] {
        reply->deleteLater();
        const QString expected = QString::fromLatin1(reply->readAll()).section(QLatin1Char(' '), 0, 0).trimmed().toLower();
        if (reply->error() != QNetworkReply::NoError || expected.isEmpty()) {
            qWarning("Proton checksum unavailable, skipping verification");
            extract();
            return;
        }
        auto *sum = new QProcess(this);
        connect(sum, &QProcess::finished, this, [this, sum, archive, expected, extract] {
            sum->deleteLater();
            const QString actual = QString::fromLatin1(sum->readAllStandardOutput()).section(QLatin1Char(' '), 0, 0).trimmed().toLower();
            qInfo("Proton: checksum %s", actual == expected ? "ok" : "MISMATCH");
            if (actual != expected) {
                QFile::remove(archive);
                finishCurrent(false, tr("Архив Proton повреждён. Попробуйте ещё раз."));
                return;
            }
            extract();
        });
        sum->start(QStringLiteral("sha512sum"), {archive});
    });
}
