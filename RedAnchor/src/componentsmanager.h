#pragma once

#include <QHash>
#include <QList>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QUrl>
#include <QVariantList>
#include <QtQml/qqmlregistration.h>

class QNetworkAccessManager;
class QNetworkReply;
class QProcess;

// Дополнительные компоненты, нужные для игр (Ubuntu / Debian):
//   "drivers" — графический драйвер: NVIDIA или Mesa (AMD, Intel), 64 и 32 бита;
//   "vulkan"  — загрузчик Vulkan и драйверы Mesa Vulkan, 64 и 32 бита;
//   "proton"  — GE-Proton (последний выпуск с GitHub) для запуска Windows-игр;
//   "depotdownloader", "gogdl" — загрузчики игр магазинов Steam и GOG (последние выпуски с GitHub,
//   в ~/.local/share/RedAnchor/RedAnchor Shell/tools — оттуда их берёт StoreManager).
// Драйверы и Vulkan ставит помощник redanchor-components (apt, от root через pkexec),
// Proton скачивается в ~/.local/share/RedAnchor/RedAnchor Shell/compatibilitytools
// без прав root. Установка идёт по очереди, по одному компоненту.
class ComponentsManager : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

    // [{ key, name, description, status, version, latest, progress, message }]
    //   status: "checking" | "missing" | "installed" | "update" | "queued" | "installing" | "failed"
    //   progress: 0..1, или -1 — без точного прогресса
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

public:
    explicit ComponentsManager(QObject *parent = nullptr);

    QVariantList components() const;
    bool busy() const { return !m_current.isEmpty() || !m_queue.isEmpty(); }
    bool checking() const { return m_pendingChecks > 0; }
    bool needsInstall() const;
    bool rebootRequired() const { return m_rebootRequired; }
    QString gpuVendor() const { return m_gpuVendor; }
    QString protonPath() const;

    // Проверить, что установлено, и узнать последнюю версию Proton.
    Q_INVOKABLE void check();
    // Поставить компонент в очередь установки.
    Q_INVOKABLE void install(const QString &key);
    // Установить всё отсутствующее и обновить устаревшее.
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
        QString state = QStringLiteral("checking");
        QString version;
        QString latest;
        double progress = -1;
        QString message;
    };

    Component *find(const QString &key);
    void setState(const QString &key, const QString &state, const QString &message = QString());
    void beginCheck();
    void endCheck();
    void checkPackages();
    void checkProtonLocal();
    void fetchProtonRelease();
    void checkToolLocal(const QString &key);
    void fetchToolRelease(const QString &key);
    void installTool(const QString &key);
    QString toolsDir() const;
    static QString toolFile(const QString &key);
    static QString toolRepo(const QString &key);
    void processQueue();
    void finishCurrent(bool ok, const QString &message = QString());
    void runHelper(const QString &key);
    void downloadProton();
    void verifyAndExtractProton(const QString &archive);
    QString compatToolsDir() const;
    static QString detectGpuVendor();

    QList<Component> m_items;
    QStringList m_queue;
    QString m_current;
    int m_pendingChecks = 0;
    bool m_rebootRequired = false;
    QString m_gpuVendor;

    QNetworkAccessManager *m_network = nullptr;
    QProcess *m_process = nullptr;
    QNetworkReply *m_download = nullptr;
    QUrl m_protonUrl;
    QUrl m_protonChecksumUrl;
    // Последние выпуски загрузчиков: ключ -> { tag, url, name }
    QHash<QString, QStringList> m_toolReleases;
    QString m_protonChecksum;
};
