#include "bluetoothmanager.h"

#include <QCoreApplication>
#include <QDBusAbstractAdaptor>
#include <QDBusArgument>
#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusInterface>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QDBusReply>
#include <QDBusVariant>
#include <QFileInfo>
#include <QProcess>

#include <algorithm>

namespace {

const QString kBluez = QStringLiteral("org.bluez");
const QString kAdapterIface = QStringLiteral("org.bluez.Adapter1");
const QString kDeviceIface = QStringLiteral("org.bluez.Device1");
const QString kAgentManagerIface = QStringLiteral("org.bluez.AgentManager1");
const QString kPropsIface = QStringLiteral("org.freedesktop.DBus.Properties");
const QString kObjectManagerIface = QStringLiteral("org.freedesktop.DBus.ObjectManager");
const QString kAgentPath = QStringLiteral("/org/redanchor/shell/agent");
const QString kHidUuid = QStringLiteral("00001124-0000-1000-8000-00805f9b34fb");

constexpr int HelperTimeoutMs = 30 * 1000;

QDBusConnection bus()
{
    return QDBusConnection::systemBus();
}

bool looksLikeGamepad(const QVariantMap &props)
{
    const QString name = props.value(QStringLiteral("Name")).toString();
    return name.contains(QLatin1String("DualSense"), Qt::CaseInsensitive)
           || name == QLatin1String("Wireless Controller")
           || props.value(QStringLiteral("Icon")).toString() == QLatin1String("input-gaming");
}

// Установленный помощник сопряжения, а при запуске из каталога сборки — соседний файл.
QString pairHelperPath()
{
    const QString installed = QStringLiteral(REDANCHOR_PAIR_HELPER);
    if (QFileInfo::exists(installed))
        return installed;
    const QString local = QCoreApplication::applicationDirPath() + QStringLiteral("/redanchor-pair-controller");
    return QFileInfo::exists(local) ? local : QString();
}

} // namespace

// Агент BlueZ без ввода/вывода. Подтверждает запросы только для геймпадов,
// остальные устройства отклоняет.
class BluezAgentAdaptor : public QDBusAbstractAdaptor
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.bluez.Agent1")

public:
    BluezAgentAdaptor(QObject *parent, BluetoothManager *manager)
        : QDBusAbstractAdaptor(parent)
        , m_manager(manager)
    {
    }

public slots:
    void Release() {}
    QString RequestPinCode(const QDBusObjectPath &device, const QDBusMessage &msg)
    {
        // Старые гарнитуры и колонки со стандартным PIN
        if (m_manager->isUserPairing(device.path()))
            return QStringLiteral("0000");
        reject(msg);
        return {};
    }
    void DisplayPinCode(const QDBusObjectPath &, const QString &) {}
    uint RequestPasskey(const QDBusObjectPath &, const QDBusMessage &msg)
    {
        reject(msg);
        return 0;
    }
    void DisplayPasskey(const QDBusObjectPath &, uint, ushort) {}
    void RequestConfirmation(const QDBusObjectPath &device, uint, const QDBusMessage &msg) { check(device, msg); }
    void RequestAuthorization(const QDBusObjectPath &device, const QDBusMessage &msg) { check(device, msg); }
    void AuthorizeService(const QDBusObjectPath &device, const QString &, const QDBusMessage &msg) { check(device, msg); }
    void Cancel() {}

private:
    void check(const QDBusObjectPath &device, const QDBusMessage &msg)
    {
        if (!m_manager->isAllowedDevice(device.path()))
            reject(msg);
    }
    static void reject(const QDBusMessage &msg)
    {
        msg.setDelayedReply(true);
        bus().send(msg.createErrorReply(QStringLiteral("org.bluez.Error.Rejected"), QStringLiteral("Rejected")));
    }

    BluetoothManager *m_manager;
};

BluetoothManager::BluetoothManager(QObject *parent)
    : QObject(parent)
{
    m_helperTimeout.setSingleShot(true);
    m_helperTimeout.setInterval(HelperTimeoutMs);
    connect(&m_helperTimeout, &QTimer::timeout, this, [this] {
        // pkexec может ждать пароль, если правило polkit не установлено.
        qWarning("Controller pairing helper timed out");
        if (m_helper)
            m_helper->kill();
    });

    // bluetoothd может стартовать позже оболочки или перезапуститься
    // (в том числе его перезапускает помощник сопряжения).
    m_watcher = new QDBusServiceWatcher(kBluez, bus(),
                                        QDBusServiceWatcher::WatchForRegistration | QDBusServiceWatcher::WatchForUnregistration,
                                        this);
    connect(m_watcher, &QDBusServiceWatcher::serviceRegistered, this, &BluetoothManager::init);
    connect(m_watcher, &QDBusServiceWatcher::serviceUnregistered, this, &BluetoothManager::reset);

    bus().connect(kBluez, QStringLiteral("/"), kObjectManagerIface, QStringLiteral("InterfacesAdded"), this,
                  SLOT(onInterfacesAdded(QDBusObjectPath, InterfaceMap)));
    bus().connect(kBluez, QStringLiteral("/"), kObjectManagerIface, QStringLiteral("InterfacesRemoved"), this,
                  SLOT(onInterfacesRemoved(QDBusObjectPath, QStringList)));
    bus().connect(kBluez, QString(), kPropsIface, QStringLiteral("PropertiesChanged"), this,
                  SLOT(onPropertiesChanged(QDBusMessage)));

    if (bus().interface()->isServiceRegistered(kBluez).value())
        init();
    else
        qWarning("BlueZ is not running: Bluetooth is unavailable");
}

BluetoothManager::~BluetoothManager()
{
    unregisterAgent();
}

void BluetoothManager::init()
{
    QDBusMessage call = QDBusMessage::createMethodCall(kBluez, QStringLiteral("/"), kObjectManagerIface,
                                                       QStringLiteral("GetManagedObjects"));
    QDBusReply<ManagedObjects> reply = bus().call(call);
    if (!reply.isValid()) {
        qWarning("BlueZ GetManagedObjects failed: %s", qPrintable(reply.error().message()));
        return;
    }

    m_devices.clear();
    m_resolving.clear();
    QString adapter;
    QString address;
    const ManagedObjects objects = reply.value();
    for (auto it = objects.cbegin(); it != objects.cend(); ++it) {
        if (adapter.isEmpty() && it.value().contains(kAdapterIface)) {
            adapter = it.key().path();
            address = it.value().value(kAdapterIface).value(QStringLiteral("Address")).toString();
        }
        if (it.value().contains(kDeviceIface))
            m_devices.insert(it.key().path(), it.value().value(kDeviceIface));
    }

    m_agentRegistered = false;
    registerAgent();
    setAdapter(adapter, address);
    if (!adapter.isEmpty())
        updateAdapterState(objects.value(QDBusObjectPath(adapter)).value(kAdapterIface));
    for (auto it = m_devices.cbegin(); it != m_devices.cend(); ++it)
        checkDevice(it.key());
    emit devicesChanged();
}

void BluetoothManager::reset()
{
    m_devices.clear();
    m_resolving.clear();
    m_userPairing.clear();
    m_busy.clear();
    m_agentRegistered = false;
    setAdapter(QString(), QString());
    updateAdapterState({{QStringLiteral("Powered"), false}, {QStringLiteral("Discovering"), false}});
    emit devicesChanged();
}

void BluetoothManager::setAdapter(const QString &path, const QString &address)
{
    m_adapterPath = path;
    m_adapterAddress = address.toUpper();
    if (!path.isEmpty()) {
        setDBusProperty(path, kAdapterIface, QStringLiteral("Powered"), true);
        // Контроллер подключается к нам сам — адаптер должен принимать входящие соединения.
        setDBusProperty(path, kAdapterIface, QStringLiteral("Pairable"), true);
    }
    // Пока работает помощник сопряжения, он перезапускает bluetoothd — адаптер
    // на это время не считаем пропавшим.
    const bool available = !path.isEmpty() || m_helper;
    if (available != m_adapterAvailable) {
        m_adapterAvailable = available;
        emit adapterChanged();
    }
}

bool BluetoothManager::registerAgent()
{
    if (m_agentRegistered)
        return true;
    if (!m_agent) {
        m_agent = new QObject(this);
        new BluezAgentAdaptor(m_agent, this);
        if (!bus().registerObject(kAgentPath, m_agent)) {
            qWarning("Failed to export Bluetooth agent object");
            return false;
        }
    }
    QDBusInterface manager(kBluez, QStringLiteral("/org/bluez"), kAgentManagerIface, bus());
    QDBusReply<void> reg = manager.call(QStringLiteral("RegisterAgent"), QVariant::fromValue(QDBusObjectPath(kAgentPath)),
                                        QStringLiteral("NoInputNoOutput"));
    if (!reg.isValid()) {
        qWarning("RegisterAgent failed: %s", qPrintable(reg.error().message()));
        return false;
    }
    manager.call(QStringLiteral("RequestDefaultAgent"), QVariant::fromValue(QDBusObjectPath(kAgentPath)));
    m_agentRegistered = true;
    return true;
}

void BluetoothManager::unregisterAgent()
{
    if (!m_agentRegistered)
        return;
    QDBusInterface manager(kBluez, QStringLiteral("/org/bluez"), kAgentManagerIface, bus());
    manager.call(QStringLiteral("UnregisterAgent"), QVariant::fromValue(QDBusObjectPath(kAgentPath)));
    m_agentRegistered = false;
}

void BluetoothManager::setPairingState(const QString &state)
{
    if (state == m_pairingState)
        return;
    m_pairingState = state;
    emit pairingStateChanged();
}

void BluetoothManager::pairController(const QString &mac)
{
    if (m_helper)
        return; // уже идёт
    if (!m_adapterAvailable || m_adapterAddress.isEmpty()) {
        qWarning("Cannot pair controller: no Bluetooth adapter");
        setPairingState(QStringLiteral("failed"));
        return;
    }
    const QString helper = pairHelperPath();
    if (helper.isEmpty()) {
        qWarning("Cannot pair controller: redanchor-pair-controller is not installed");
        setPairingState(QStringLiteral("failed"));
        return;
    }

    m_target = mac.toUpper();
    setPairingState(QStringLiteral("pairing"));
    qInfo("Pairing controller %s with adapter %s", qPrintable(m_target), qPrintable(m_adapterAddress));

    m_helper = new QProcess(this);
    m_helper->setProcessChannelMode(QProcess::SeparateChannels);
    connect(m_helper, &QProcess::finished, this, [this](int exitCode, QProcess::ExitStatus status) {
        onHelperFinished(status == QProcess::NormalExit ? exitCode : -1);
    });
    connect(m_helper, &QProcess::errorOccurred, this, [this](QProcess::ProcessError error) {
        if (error == QProcess::FailedToStart)
            onHelperFinished(-1);
    });
    m_helper->start(QStringLiteral("pkexec"), {helper, m_adapterAddress});
    m_helperTimeout.start();
}

void BluetoothManager::onHelperFinished(int exitCode)
{
    if (!m_helper)
        return;
    m_helperTimeout.stop();
    const QString out = QString::fromUtf8(m_helper->readAllStandardOutput()).trimmed();
    const QString err = QString::fromUtf8(m_helper->readAllStandardError()).trimmed();
    m_helper->deleteLater();
    m_helper = nullptr;
    setAdapter(m_adapterPath, m_adapterAddress);

    if (!err.isEmpty())
        qWarning("%s", qPrintable(err));
    if (exitCode != 0) {
        qWarning("Controller pairing failed (exit code %d)", exitCode);
        setPairingState(QStringLiteral("failed"));
        return;
    }
    qInfo("Controller pairing: %s", qPrintable(out));
    const QStringList parts = out.split(QLatin1Char(' '));
    if (parts.size() >= 2)
        m_target = parts.at(1).toUpper();

    // Ключ уже был у BlueZ — достаточно убедиться, что устройство доверенное.
    for (auto it = m_devices.cbegin(); it != m_devices.cend(); ++it) {
        if (it.value().value(QStringLiteral("Address")).toString().toUpper() == m_target
            && !it.value().value(QStringLiteral("Trusted")).toBool())
            setDBusProperty(it.key(), kDeviceIface, QStringLiteral("Trusted"), true);
    }
    setPairingState(QStringLiteral("paired"));
}

bool BluetoothManager::isOurController(const QVariantMap &props) const
{
    const QString address = props.value(QStringLiteral("Address")).toString().toUpper();
    return (!m_target.isEmpty() && address == m_target) || looksLikeGamepad(props);
}

// Сопряжённый контроллер подключился по Bluetooth, а BlueZ ещё не знает его сервисов
// (первое подключение после сопряжения по кабелю) — просим BlueZ опросить его и
// подключить HID.
void BluetoothManager::checkDevice(const QString &path)
{
    const QVariantMap props = m_devices.value(path);
    if (!props.value(QStringLiteral("Connected")).toBool()) {
        m_resolving.remove(path);
        return;
    }
    if (!props.value(QStringLiteral("Paired")).toBool() || !isOurController(props) || m_resolving.contains(path))
        return;
    if (props.value(QStringLiteral("UUIDs")).toStringList().contains(kHidUuid, Qt::CaseInsensitive))
        return;

    m_resolving.insert(path);
    qInfo("Controller %s connected without known services, resolving",
          qPrintable(props.value(QStringLiteral("Address")).toString()));
    QDBusMessage call = QDBusMessage::createMethodCall(kBluez, path, kDeviceIface, QStringLiteral("Connect"));
    bus().asyncCall(call, 30 * 1000);
}

bool BluetoothManager::isAllowedDevice(const QString &devicePath)
{
    QVariantMap props = m_devices.value(devicePath);
    if (props.isEmpty()) {
        QDBusMessage call = QDBusMessage::createMethodCall(kBluez, devicePath, kPropsIface, QStringLiteral("GetAll"));
        call << kDeviceIface;
        QDBusReply<QVariantMap> reply = bus().call(call);
        if (reply.isValid())
            props = reply.value();
    }
    const bool allowed = isOurController(props) || m_userPairing.contains(devicePath);
    qInfo("Bluetooth agent: %s %s", allowed ? "allow" : "reject",
          qPrintable(props.value(QStringLiteral("Address")).toString()));
    return allowed;
}

void BluetoothManager::onInterfacesAdded(const QDBusObjectPath &path, const InterfaceMap &interfaces)
{
    if (interfaces.contains(kAdapterIface) && m_adapterPath.isEmpty())
        setAdapter(path.path(), interfaces.value(kAdapterIface).value(QStringLiteral("Address")).toString());
    if (interfaces.contains(kDeviceIface)) {
        m_devices.insert(path.path(), interfaces.value(kDeviceIface));
        checkDevice(path.path());
        emit devicesChanged();
    }
}

void BluetoothManager::onInterfacesRemoved(const QDBusObjectPath &path, const QStringList &interfaces)
{
    if (interfaces.contains(kDeviceIface)) {
        m_devices.remove(path.path());
        m_resolving.remove(path.path());
        m_userPairing.remove(path.path());
        m_busy.remove(path.path());
        emit devicesChanged();
    }
    if (interfaces.contains(kAdapterIface) && path.path() == m_adapterPath) {
        // Адаптер вытащили — пробуем найти другой.
        setAdapter(QString(), QString());
        init();
    }
}

void BluetoothManager::onPropertiesChanged(const QDBusMessage &message)
{
    const QList<QVariant> args = message.arguments();
    if (args.size() < 2)
        return;
    const QString iface = args.at(0).toString();
    const QString path = message.path();
    const QVariantMap changed = qdbus_cast<QVariantMap>(args.at(1).value<QDBusArgument>());
    if (iface == kAdapterIface && path == m_adapterPath) {
        updateAdapterState(changed);
        // Адаптер выключили со стороны (rfkill, другая программа) — включаем обратно,
        // иначе беспроводные геймпады отвалятся.
        if (changed.contains(QStringLiteral("Powered")) && !m_powered) {
            qWarning("Bluetooth adapter was powered off, powering it back on");
            QTimer::singleShot(1000, this, [this] {
                if (!m_adapterPath.isEmpty() && !m_powered)
                    setDBusProperty(m_adapterPath, kAdapterIface, QStringLiteral("Powered"), true);
            });
        }
        return;
    }
    if (iface != kDeviceIface || !m_devices.contains(path))
        return;
    QVariantMap &props = m_devices[path];
    for (auto it = changed.cbegin(); it != changed.cend(); ++it)
        props.insert(it.key(), it.value());
    checkDevice(path);
    emit devicesChanged();
}

// ---------- Настройки Bluetooth ----------

void BluetoothManager::updateAdapterState(const QVariantMap &props)
{
    bool changed = false;
    if (props.contains(QStringLiteral("Powered"))) {
        const bool powered = props.value(QStringLiteral("Powered")).toBool();
        changed |= powered != m_powered;
        m_powered = powered;
    }
    if (props.contains(QStringLiteral("Discovering"))) {
        const bool discovering = props.value(QStringLiteral("Discovering")).toBool();
        changed |= discovering != m_discovering;
        m_discovering = discovering;
    }
    if (changed)
        emit adapterStateChanged();
}

bool BluetoothManager::audioConnected() const
{
    // A2DP-приёмник, гарнитура (HSP) или громкая связь (HFP)
    static const QStringList audioUuids = {
        QStringLiteral("0000110b-0000-1000-8000-00805f9b34fb"),
        QStringLiteral("00001108-0000-1000-8000-00805f9b34fb"),
        QStringLiteral("0000111e-0000-1000-8000-00805f9b34fb"),
    };
    for (const QVariantMap &props : m_devices) {
        if (!props.value(QStringLiteral("Connected")).toBool())
            continue;
        if (props.value(QStringLiteral("Icon")).toString().startsWith(QLatin1String("audio-")))
            return true;
        const QStringList uuids = props.value(QStringLiteral("UUIDs")).toStringList();
        for (const QString &uuid : audioUuids) {
            if (uuids.contains(uuid, Qt::CaseInsensitive))
                return true;
        }
    }
    return false;
}

bool BluetoothManager::isPairedDevice(const QString &mac) const
{
    for (const QVariantMap &props : m_devices) {
        if (props.value(QStringLiteral("Address")).toString().compare(mac, Qt::CaseInsensitive) == 0)
            return props.value(QStringLiteral("Paired")).toBool();
    }
    return false;
}

QVariantList BluetoothManager::devices() const
{
    QVariantList list;
    for (auto it = m_devices.cbegin(); it != m_devices.cend(); ++it) {
        const QVariantMap &p = it.value();
        if (looksLikeGamepad(p))
            continue;
        const bool paired = p.value(QStringLiteral("Paired")).toBool();
        const QString name = p.value(QStringLiteral("Name")).toString();
        if (!paired && name.isEmpty())
            continue;
        list.append(QVariantMap{
            {QStringLiteral("path"), it.key()},
            {QStringLiteral("address"), p.value(QStringLiteral("Address")).toString()},
            {QStringLiteral("name"), name.isEmpty() ? p.value(QStringLiteral("Address")).toString() : name},
            {QStringLiteral("icon"), p.value(QStringLiteral("Icon")).toString()},
            {QStringLiteral("paired"), paired},
            {QStringLiteral("connected"), p.value(QStringLiteral("Connected")).toBool()},
            {QStringLiteral("busy"), m_busy.contains(it.key())},
        });
    }
    // Сначала сопряжённые, затем по имени
    std::sort(list.begin(), list.end(), [](const QVariant &a, const QVariant &b) {
        const QVariantMap ma = a.toMap(), mb = b.toMap();
        if (ma.value(QStringLiteral("paired")).toBool() != mb.value(QStringLiteral("paired")).toBool())
            return ma.value(QStringLiteral("paired")).toBool();
        return ma.value(QStringLiteral("name")).toString().compare(mb.value(QStringLiteral("name")).toString(),
                                                                   Qt::CaseInsensitive) < 0;
    });
    return list;
}

void BluetoothManager::startDiscovery()
{
    if (m_adapterPath.isEmpty() || !m_powered || m_discovering)
        return;
    QDBusMessage call = QDBusMessage::createMethodCall(kBluez, m_adapterPath, kAdapterIface, QStringLiteral("StartDiscovery"));
    bus().asyncCall(call);
}

void BluetoothManager::stopDiscovery()
{
    if (m_adapterPath.isEmpty() || !m_discovering)
        return;
    QDBusMessage call = QDBusMessage::createMethodCall(kBluez, m_adapterPath, kAdapterIface, QStringLiteral("StopDiscovery"));
    bus().asyncCall(call);
}

void BluetoothManager::setBusy(const QString &path, bool busy)
{
    if (busy)
        m_busy.insert(path);
    else
        m_busy.remove(path);
    emit devicesChanged();
}

void BluetoothManager::callDevice(const QString &path, const QString &method, int timeoutMs, const QString &errorText,
                                  std::function<void()> onSuccess)
{
    setBusy(path, true);
    QDBusMessage call = QDBusMessage::createMethodCall(kBluez, path, kDeviceIface, method);
    auto *watcher = new QDBusPendingCallWatcher(bus().asyncCall(call, timeoutMs), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this,
            [this, path, method, errorText, onSuccess](QDBusPendingCallWatcher *w) {
                w->deleteLater();
                setBusy(path, false);
                const QDBusPendingReply<> reply = *w;
                if (reply.isError()) {
                    qWarning("Bluetooth %s failed: %s", qPrintable(method), qPrintable(reply.error().message()));
                    emit deviceError(path, errorText);
                    return;
                }
                if (onSuccess)
                    onSuccess();
            });
}

void BluetoothManager::pairDevice(const QString &path)
{
    if (!m_devices.contains(path) || m_busy.contains(path))
        return;
    stopDiscovery(); // поиск мешает сопряжению
    m_userPairing.insert(path);
    callDevice(path, QStringLiteral("Pair"), 60 * 1000, tr("Не удалось выполнить сопряжение."), [this, path] {
        m_userPairing.remove(path);
        setDBusProperty(path, kDeviceIface, QStringLiteral("Trusted"), true);
        connectDevice(path);
    });
}

void BluetoothManager::connectDevice(const QString &path)
{
    if (!m_devices.contains(path) || m_busy.contains(path))
        return;
    callDevice(path, QStringLiteral("Connect"), 30 * 1000, tr("Не удалось подключиться к устройству."));
}

void BluetoothManager::disconnectDevice(const QString &path)
{
    if (!m_devices.contains(path) || m_busy.contains(path))
        return;
    callDevice(path, QStringLiteral("Disconnect"), 15 * 1000, tr("Не удалось отключить устройство."));
}

void BluetoothManager::removeDevice(const QString &path)
{
    if (m_adapterPath.isEmpty() || !m_devices.contains(path))
        return;
    m_userPairing.remove(path);
    QDBusMessage call = QDBusMessage::createMethodCall(kBluez, m_adapterPath, kAdapterIface, QStringLiteral("RemoveDevice"));
    call << QVariant::fromValue(QDBusObjectPath(path));
    bus().asyncCall(call);
}

void BluetoothManager::disconnectAddress(const QString &mac)
{
    for (auto it = m_devices.cbegin(); it != m_devices.cend(); ++it) {
        if (it.value().value(QStringLiteral("Address")).toString().compare(mac, Qt::CaseInsensitive) == 0) {
            QDBusMessage call = QDBusMessage::createMethodCall(kBluez, it.key(), kDeviceIface, QStringLiteral("Disconnect"));
            bus().asyncCall(call);
            return;
        }
    }
}

void BluetoothManager::setDBusProperty(const QString &path, const QString &iface, const QString &name, const QVariant &value)
{
    QDBusMessage call = QDBusMessage::createMethodCall(kBluez, path, kPropsIface, QStringLiteral("Set"));
    call << iface << name << QVariant::fromValue(QDBusVariant(value));
    const QDBusMessage reply = bus().call(call);
    if (reply.type() == QDBusMessage::ErrorMessage)
        qWarning("Failed to set %s.%s: %s", qPrintable(iface), qPrintable(name), qPrintable(reply.errorMessage()));
}

#include "bluetoothmanager.moc"
