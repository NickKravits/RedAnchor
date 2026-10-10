#pragma once

#include <QDBusMessage>
#include <QDBusObjectPath>
#include <QDBusServiceWatcher>
#include <QHash>
#include <QObject>
#include <QSet>
#include <QTimer>

#include <functional>
#include <QtQml/qqmlregistration.h>

#include "dbustypes.h"

class QProcess;

// Bluetooth через BlueZ (org.bluez по системной шине D-Bus).
//
// Сопряжение DualSense выполняется «через кабель», как на консоли. BlueZ сам этого
// для DualSense не умеет (плагин sixaxis знает только DualShock 3/4), поэтому
// оболочка запускает помощника redanchor-pair-controller (через pkexec, от root):
// он записывает в контроллер адрес адаптера и ключ связи и сохраняет тот же ключ
// в хранилище BlueZ. После отключения кабеля контроллер по нажатию PS подключается
// по Bluetooth сам. При первом таком подключении BlueZ ещё не знает сервисов
// контроллера — тогда мы вызываем Device1.Connect, и BlueZ опрашивает его.
//
// Агент (org.bluez.Agent1) держим зарегистрированным всё время работы и
// подтверждаем запросы только для геймпадов.
class BluetoothManager : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

    // Есть ли в системе работающий Bluetooth-адаптер
    Q_PROPERTY(bool adapterAvailable READ adapterAvailable NOTIFY adapterChanged)
    // "idle" | "pairing" | "paired" | "failed" — состояние кабельного сопряжения
    Q_PROPERTY(QString pairingState READ pairingState NOTIFY pairingStateChanged)
    // Контроллер на кабеле сопряжён и после отключения кабеля подключится по Bluetooth сам
    Q_PROPERTY(bool controllerPaired READ controllerPaired NOTIFY pairingStateChanged)

    // --- Настройки Bluetooth ---
    // Адаптер всегда включён: оболочка не даёт его выключить, чтобы не отвалились геймпады
    Q_PROPERTY(bool powered READ powered NOTIFY adapterStateChanged)
    Q_PROPERTY(bool discovering READ discovering NOTIFY adapterStateChanged)
    // Устройства, кроме геймпадов: [{ path, address, name, icon, paired, connected, busy }].
    // Несопряжённые попадают в список, только если у них есть имя (видны во время поиска).
    Q_PROPERTY(QVariantList devices READ devices NOTIFY devicesChanged)
    // Подключена Bluetooth-гарнитура или наушники (значок в строке состояния)
    Q_PROPERTY(bool audioConnected READ audioConnected NOTIFY devicesChanged)

public:
    explicit BluetoothManager(QObject *parent = nullptr);
    ~BluetoothManager() override;

    bool adapterAvailable() const { return m_adapterAvailable; }
    QString pairingState() const { return m_pairingState; }
    bool controllerPaired() const { return m_pairingState == QLatin1String("paired"); }

    bool powered() const { return m_powered; }
    bool discovering() const { return m_discovering; }
    QVariantList devices() const;
    bool audioConnected() const;

    Q_INVOKABLE void startDiscovery();
    Q_INVOKABLE void stopDiscovery();
    Q_INVOKABLE void pairDevice(const QString &path);
    Q_INVOKABLE void connectDevice(const QString &path);
    Q_INVOKABLE void disconnectDevice(const QString &path);
    Q_INVOKABLE void removeDevice(const QString &path);
    // Отключить устройство по MAC (для геймпадов из раздела «Геймпады»).
    Q_INVOKABLE void disconnectAddress(const QString &mac);
    // Сопряжено ли устройство с этим адаптером (BlueZ знает его ключ).
    Q_INVOKABLE bool isPairedDevice(const QString &mac) const;

    // Сопрячь DualSense, подключённый по USB (MAC — из GamepadManager).
    Q_INVOKABLE void pairController(const QString &mac);

    // Используется агентом: разрешаем сопряжение геймпадам и устройствам,
    // которые пользователь сам выбрал в настройках.
    bool isAllowedDevice(const QString &devicePath);
    bool isUserPairing(const QString &devicePath) const { return m_userPairing.contains(devicePath); }

signals:
    void adapterChanged();
    void pairingStateChanged();
    void adapterStateChanged();
    void devicesChanged();
    // Ошибка действия с устройством из настроек (текст для пользователя)
    void deviceError(const QString &path, const QString &message);

private slots:
    void onInterfacesAdded(const QDBusObjectPath &path, const InterfaceMap &interfaces);
    void onInterfacesRemoved(const QDBusObjectPath &path, const QStringList &interfaces);
    void onPropertiesChanged(const QDBusMessage &message);

private:
    void init();
    void reset();
    void setAdapter(const QString &path, const QString &address);
    bool registerAgent();
    void unregisterAgent();
    void setPairingState(const QString &state);
    void onHelperFinished(int exitCode);
    void checkDevice(const QString &path);
    bool isOurController(const QVariantMap &props) const;
    void callDevice(const QString &path, const QString &method, int timeoutMs, const QString &errorText,
                    std::function<void()> onSuccess = {});
    void setBusy(const QString &path, bool busy);
    void updateAdapterState(const QVariantMap &props);
    void setDBusProperty(const QString &path, const QString &iface, const QString &name, const QVariant &value);

    bool m_adapterAvailable = false;
    bool m_agentRegistered = false;
    QString m_adapterPath;
    QString m_adapterAddress;
    QString m_target;                   // MAC контроллера, который сопрягаем / сопрягли
    QString m_pairingState = QStringLiteral("idle");
    QHash<QString, QVariantMap> m_devices; // путь -> свойства org.bluez.Device1
    QSet<QString> m_resolving;          // устройства, для которых уже вызвали Connect
    QSet<QString> m_userPairing;        // устройства, сопряжение которых начал пользователь
    QSet<QString> m_busy;               // устройства, с которыми идёт действие
    bool m_powered = false;
    bool m_discovering = false;
    QObject *m_agent = nullptr;
    QDBusServiceWatcher *m_watcher = nullptr;
    QProcess *m_helper = nullptr;
    QTimer m_helperTimeout;
};
