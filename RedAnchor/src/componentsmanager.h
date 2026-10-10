#pragma once

#include <QHash>
#include <QJsonObject>
#include <QList>
#include <QObject>
#include <QSet>
#include <QString>
#include <QStringList>
#include <QUrl>
#include <QVariantList>
#include <QtQml/qqmlregistration.h>

#include <functional>

class QFile;
class QNetworkAccessManager;
class QNetworkReply;
class QProcess;

// Дополнительные компоненты, нужные для игр (Ubuntu / Debian), в порядке установки:
//   "system"  — всё для сессии: gamescope (или cage), Xwayland, libxcb-cursor0, Python 3
//               (на нём написан Proton), PipeWire (громкость), шрифт Inter;
//   "drivers" — графический драйвер: NVIDIA или Mesa (AMD, Intel), 64 и 32 бита;
//   "vulkan"  — загрузчик Vulkan и драйверы Mesa Vulkan, 64 и 32 бита;
//   "steam"   — официальный клиент Steam (обновляется сам, состояния «update» у него нет);
//   "proton"  — GE-Proton (последний выпуск с GitHub) для Windows-игр из GOG;
//   "gogdl"   — загрузчик игр GOG (последний выпуск с GitHub,
//               в ~/.local/share/RedAnchor/RedAnchor Shell/tools — оттуда его берёт StoreManager).
// Системные пакеты (system, drivers, vulkan, steam) ставит помощник redanchor-components
// (apt, от root через pkexec). Proton скачивается в
// ~/.local/share/RedAnchor/RedAnchor Shell/compatibilitytools без прав root.
// Установка идёт по очереди, по одному компоненту.
class ComponentsManager : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

    // [{ key, name, description, status, version, latest, progress, message, recommended }]
    //   status: "checking" | "missing" | "installed" | "update" | "queued" | "installing" | "failed"
    //   progress: 0..1, или -1 — без точного прогресса
    //   recommended: false — компонент не обязателен (без него работает часть возможностей)
    Q_PROPERTY(QVariantList components READ components NOTIFY componentsChanged)
    // Идёт установка или ждёт очередь
    Q_PROPERTY(bool busy READ busy NOTIFY componentsChanged)
    // Идёт проверка состояния
    Q_PROPERTY(bool checking READ checking NOTIFY componentsChanged)
    // Есть что установить или обновить
    Q_PROPERTY(bool needsInstall READ needsInstall NOTIFY componentsChanged)
    // Драйвер NVIDIA заработает после перезагрузки
    Q_PROPERTY(bool rebootRequired READ rebootRequired NOTIFY componentsChanged)
    // "nvidia" | "amd" | "intel" | "" — основная видеокарта
    Q_PROPERTY(QString gpuVendor READ gpuVendor CONSTANT)
    // Каталог установленного Proton (пусто, если не установлен)
    Q_PROPERTY(QString protonPath READ protonPath NOTIFY componentsChanged)
    // Исполняемый файл клиента Steam (пусто, если Steam не установлен)
    Q_PROPERTY(QString steamPath READ steamPath NOTIFY componentsChanged)
    Q_PROPERTY(bool steamInstalled READ steamInstalled NOTIFY componentsChanged)

public:
    explicit ComponentsManager(QObject *parent = nullptr);

    QVariantList components() const;
    bool busy() const { return !m_current.isEmpty() || !m_queue.isEmpty(); }
    bool checking() const { return m_pendingChecks > 0; }
    bool needsInstall() const;
    bool rebootRequired() const { return m_rebootRequired; }
    QString gpuVendor() const { return m_gpuVendor; }
    QString protonPath() const;
    QString steamPath() const { return steamExecutable(); }
    bool steamInstalled() const { return !steamExecutable().isEmpty(); }

    // Клиент Steam: из PATH, иначе /usr/games/steam или /usr/bin/steam (у экрана входа
    // в PATH может не быть /usr/games)
    static QString steamExecutable();

    // Проверить, что установлено, и узнать последние версии Proton и gogdl.
    Q_INVOKABLE void check();
    // Поставить компонент в очередь установки.
    Q_INVOKABLE void install(const QString &key);
    // Установить всё отсутствующее и обновить устаревшее
    // (по порядку: system → drivers → vulkan → steam → proton → gogdl).
    Q_INVOKABLE void installAll();

signals:
    void componentsChanged();
    // Очередь установки опустела
    void finished();

private:
    struct Component {
        QString key;
        QString title;
        QString description;
        bool recommended = true;
        QString state = QStringLiteral("checking");
        QString version;
        QString latest;
        double progress = -1;
        QString message;
    };
    // Выпуск на GitHub: тег, ссылка на файл, имя файла, ссылка на контрольную сумму
    struct Release {
        QString tag;
        QUrl url;
        QString fileName;
        QUrl checksumUrl;
    };
    using ReleaseCallback = std::function<void(const QJsonObject &release, const QString &error)>;

    Component *find(const QString &key);
    int rank(const QString &key) const;
    void setState(const QString &key, const QString &state, const QString &message = QString());
    void setProgress(double progress, const QString &message);
    void beginCheck();
    void endCheck();
    void checkPackages();
    void applyPackageState(const QHash<QString, QString> &installed, bool dpkgOk);
    void checkProtonLocal();
    void fetchProtonRelease();
    void checkToolLocal(const QString &key);
    void fetchToolRelease(const QString &key);
    void fetchRelease(const QString &repo, const ReleaseCallback &callback);
    void download(const QUrl &url, const QString &target, const std::function<void(bool ok)> &done);
    void installTool(const QString &key);
    QString toolsDir() const;
    static QString toolFile(const QString &key);
    static QString toolRepo(const QString &key);
    void removeObsoleteTools();
    void processQueue();
    void finishCurrent(bool ok, const QString &message = QString());
    void runHelper(const QString &key);
    void downloadProton();
    void verifyAndExtractProton(const QString &archive);
    void extractProton(const QString &archive);
    QString compatToolsDir() const;
    static QString detectGpuVendor();

    QList<Component> m_items;
    QStringList m_queue;
    QString m_current;
    int m_pendingChecks = 0;
    bool m_rebootRequired = false;
    QString m_gpuVendor;
    // Компоненты, которые помощник уже пытался поставить в этом запуске: если чего-то
    // из рекомендуемого всё равно нет (нет в репозиториях), не предлагаем ставить снова
    QSet<QString> m_attempted;

    QNetworkAccessManager *m_network = nullptr;
    QProcess *m_process = nullptr;
    QNetworkReply *m_download = nullptr;
    Release m_proton;
    // Последние выпуски загрузчиков: ключ -> выпуск
    QHash<QString, Release> m_toolReleases;
};
