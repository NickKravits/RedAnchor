#include "networkbackend.h"

#include "dbustypes.h"

#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusMessage>
#include <QDBusObjectPath>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QDBusReply>
#include <QDBusVariant>
#include <QHash>

#include <algorithm>

namespace {

const QString kNM = QStringLiteral("org.freedesktop.NetworkManager");
const QString kNMPath = QStringLiteral("/org/freedesktop/NetworkManager");
const QString kNMSettingsPath = QStringLiteral("/org/freedesktop/NetworkManager/Settings");
const QString kDeviceIface = QStringLiteral("org.freedesktop.NetworkManager.Device");
const QString kWirelessIface = QStringLiteral("org.freedesktop.NetworkManager.Device.Wireless");
const QString kApIface = QStringLiteral("org.freedesktop.NetworkManager.AccessPoint");
const QString kSettingsIface = QStringLiteral("org.freedesktop.NetworkManager.Settings");
const QString kConnectionIface = QStringLiteral("org.freedesktop.NetworkManager.Settings.Connection");
const QString kPropsIface = QStringLiteral("org.freedesktop.DBus.Properties");

// NMDeviceType / NMDeviceState / NMState
constexpr uint DeviceTypeEthernet = 1;
constexpr uint DeviceTypeWifi = 2;
constexpr uint DeviceStateDisconnected = 30;
constexpr uint DeviceStatePrepare = 40;
constexpr uint DeviceStateNeedAuth = 60;
constexpr uint DeviceStateActivated = 100;
constexpr uint DeviceStateFailed = 120;
constexpr uint NMStateConnectedLocal = 50;

constexpr uint ApFlagsPrivacy = 0x1;
constexpr uint ApSecKeyMgmtSae = 0x400;

constexpr int ConnectTimeoutMs = 45 * 1000;

QDBusConnection bus()
{
    return QDBusConnection::systemBus();
}

QVariant getProperty(const QString &path, const QString &iface, const QString &name)
{
    QDBusMessage call = QDBusMessage::createMethodCall(kNM, path, kPropsIface, QStringLiteral("Get"));
    call << iface << name;
    QDBusReply<QDBusVariant> reply = bus().call(call);
    return reply.isValid() ? reply.value().variant() : QVariant();
}

QVariantMap getAllProperties(const QString &path, const QString &iface)
{
    QDBusMessage call = QDBusMessage::createMethodCall(kNM, path, kPropsIface, QStringLiteral("GetAll"));
    call << iface;
    QDBusReply<QVariantMap> reply = bus().call(call);
    return reply.isValid() ? reply.value() : QVariantMap();
}

QList<QDBusObjectPath> callPathList(const QString &path, const QString &iface, const QString &method)
{
    QDBusMessage call = QDBusMessage::createMethodCall(kNM, path, iface, method);
    QDBusReply<QList<QDBusObjectPath>> reply = bus().call(call);
    return reply.isValid() ? reply.value() : QList<QDBusObjectPath>();
}

QVariantMap makeNetwork(const QString &ssid, int strength, bool secured)
{
    return {{QStringLiteral("ssid"), ssid},
            {QStringLiteral("strength"), strength},
            {QStringLiteral("secured"), secured},
            {QStringLiteral("active"), false},
            {QStringLiteral("saved"), false}};
}

} // namespace

NetworkBackend::NetworkBackend(QObject *parent)
    : QObject(parent)
{
    m_pollTimer.setInterval(500);
    connect(&m_pollTimer, &QTimer::timeout, this, &NetworkBackend::pollActivation);

    m_connectTimeout.setSingleShot(true);
    connect(&m_connectTimeout, &QTimer::timeout, this, [this] {
        m_pollTimer.stop();
        setConnectState(Failed, tr("Превышено время ожидания подключения."));
    });

    if (!bus().interface()->isServiceRegistered(kNM).value())
        qWarning("NetworkManager is not running: network status is unavailable");

    findDevices();
    refresh();

    // Wi-Fi/USB-адаптеры могут появиться и пропасть на ходу.
    bus().connect(kNM, kNMPath, kNM, QStringLiteral("DeviceAdded"), this, SLOT(onDevicesChanged()));
    bus().connect(kNM, kNMPath, kNM, QStringLiteral("DeviceRemoved"), this, SLOT(onDevicesChanged()));

    m_refreshTimer.setInterval(8000);
    connect(&m_refreshTimer, &QTimer::timeout, this, &NetworkBackend::refresh);
    m_refreshTimer.start();

    bus().connect(kNM, kNMPath, kNM, QStringLiteral("StateChanged"), this, SLOT(refresh()));
}

void NetworkBackend::onDevicesChanged()
{
    const QString oldWifi = m_wifiDevice;
    findDevices();
    if (oldWifi != m_wifiDevice) {
        if (m_wifiDevice.isEmpty() && !m_networks.isEmpty()) {
            m_networks.clear();
            emit networksChanged();
        }
        emit statusChanged();
    }
    refresh();
}

void NetworkBackend::findDevices()
{
    m_wiredDevices.clear();
    m_wifiDevice.clear();
    const auto devices = callPathList(kNMPath, kNM, QStringLiteral("GetDevices"));
    for (const QDBusObjectPath &dev : devices) {
        const uint type = getProperty(dev.path(), kDeviceIface, QStringLiteral("DeviceType")).toUInt();
        if (type == DeviceTypeEthernet)
            m_wiredDevices.append(dev.path());
        else if (type == DeviceTypeWifi && m_wifiDevice.isEmpty())
            m_wifiDevice = dev.path();
    }
}

void NetworkBackend::refresh()
{
    if (m_wifiDevice.isEmpty() && m_wiredDevices.isEmpty()) {
        // NetworkManager мог запуститься позже оболочки.
        findDevices();
        if (!m_wifiDevice.isEmpty())
            emit statusChanged();
    }
    refreshStatus();
    refreshNetworks();
}

void NetworkBackend::refreshStatus()
{
    const uint nmState = getProperty(kNMPath, kNM, QStringLiteral("State")).toUInt();
    bool wired = false;
    for (const QString &dev : std::as_const(m_wiredDevices)) {
        if (getProperty(dev, kDeviceIface, QStringLiteral("State")).toUInt() == DeviceStateActivated)
            wired = true;
    }

    QString ssid;
    if (!m_wifiDevice.isEmpty()) {
        const auto ap = getProperty(m_wifiDevice, kWirelessIface, QStringLiteral("ActiveAccessPoint")).value<QDBusObjectPath>();
        if (ap.path().size() > 1)
            ssid = QString::fromUtf8(getProperty(ap.path(), kApIface, QStringLiteral("Ssid")).toByteArray());
    }

    const bool online = nmState >= NMStateConnectedLocal;
    const QVariant wirelessEnabled = getProperty(kNMPath, kNM, QStringLiteral("WirelessEnabled"));
    const bool wifiEnabled = wirelessEnabled.isValid() ? wirelessEnabled.toBool() : m_wifiEnabled;
    if (online != m_online || wired != m_wiredConnected || ssid != m_activeSsid || wifiEnabled != m_wifiEnabled) {
        m_online = online;
        m_wifiEnabled = wifiEnabled;
        m_wiredConnected = wired;
        m_activeSsid = ssid;
        emit statusChanged();
    }
}

void NetworkBackend::refreshNetworks()
{
    if (m_wifiDevice.isEmpty())
        return;

    QHash<QString, QVariantMap> bySsid;
    const auto aps = callPathList(m_wifiDevice, kWirelessIface, QStringLiteral("GetAllAccessPoints"));
    for (const QDBusObjectPath &ap : aps) {
        const QVariantMap p = getAllProperties(ap.path(), kApIface);
        const QString ssid = QString::fromUtf8(p.value(QStringLiteral("Ssid")).toByteArray());
        if (ssid.isEmpty())
            continue;
        const int strength = p.value(QStringLiteral("Strength")).toInt();
        const bool secured = (p.value(QStringLiteral("Flags")).toUInt() & ApFlagsPrivacy)
                             || p.value(QStringLiteral("WpaFlags")).toUInt() || p.value(QStringLiteral("RsnFlags")).toUInt();
        auto it = bySsid.find(ssid);
        if (it == bySsid.end() || it->value(QStringLiteral("strength")).toInt() < strength)
            bySsid.insert(ssid, makeNetwork(ssid, strength, secured));
    }

    QVariantList list;
    for (auto it = bySsid.begin(); it != bySsid.end(); ++it) {
        QVariantMap net = it.value();
        net.insert(QStringLiteral("active"), it.key() == m_activeSsid);
        net.insert(QStringLiteral("saved"), !savedConnectionFor(it.key()).isEmpty());
        list.append(net);
    }
    std::sort(list.begin(), list.end(), [](const QVariant &a, const QVariant &b) {
        const QVariantMap ma = a.toMap(), mb = b.toMap();
        const bool aa = ma.value(QStringLiteral("active")).toBool(), ab = mb.value(QStringLiteral("active")).toBool();
        if (aa != ab)
            return aa;
        return ma.value(QStringLiteral("strength")).toInt() > mb.value(QStringLiteral("strength")).toInt();
    });

    if (list != m_networks) {
        m_networks = list;
        emit networksChanged();
    }
}

QString NetworkBackend::savedConnectionFor(const QString &ssid) const
{
    const auto connections = callPathList(kNMSettingsPath, kSettingsIface, QStringLiteral("ListConnections"));
    for (const QDBusObjectPath &conn : connections) {
        QDBusMessage call = QDBusMessage::createMethodCall(kNM, conn.path(), kConnectionIface, QStringLiteral("GetSettings"));
        QDBusReply<InterfaceMap> reply = bus().call(call);
        if (!reply.isValid())
            continue;
        const QVariantMap wifi = reply.value().value(QStringLiteral("802-11-wireless"));
        if (!wifi.isEmpty() && QString::fromUtf8(wifi.value(QStringLiteral("ssid")).toByteArray()) == ssid)
            return conn.path();
    }
    return {};
}

void NetworkBackend::scan()
{
    if (m_wifiDevice.isEmpty())
        return;

    m_scanning = true;
    emit scanningChanged();
    QDBusMessage call = QDBusMessage::createMethodCall(kNM, m_wifiDevice, kWirelessIface, QStringLiteral("RequestScan"));
    call << QVariantMap();
    bus().asyncCall(call);
    // NM не сообщает об окончании сканирования напрямую — обновляемся через пару секунд.
    QTimer::singleShot(3500, this, [this] {
        refresh();
        m_scanning = false;
        emit scanningChanged();
    });
}

void NetworkBackend::connectToNetwork(const QString &ssid, const QString &password)
{
    m_pendingSsid = ssid;
    m_addedConnection.clear();
    m_sawActivating = false;
    setConnectState(Connecting);

    if (m_wifiDevice.isEmpty()) {
        setConnectState(Failed, tr("Wi-Fi адаптер не найден."));
        return;
    }

    // Ищем точку доступа с этим SSID (с максимальным сигналом).
    QString apPath;
    uint rsnFlags = 0;
    int bestStrength = -1;
    bool secured = false;
    const auto aps = callPathList(m_wifiDevice, kWirelessIface, QStringLiteral("GetAllAccessPoints"));
    for (const QDBusObjectPath &ap : aps) {
        const QVariantMap p = getAllProperties(ap.path(), kApIface);
        if (QString::fromUtf8(p.value(QStringLiteral("Ssid")).toByteArray()) != ssid)
            continue;
        const int strength = p.value(QStringLiteral("Strength")).toInt();
        if (strength > bestStrength) {
            bestStrength = strength;
            apPath = ap.path();
            rsnFlags = p.value(QStringLiteral("RsnFlags")).toUInt();
            secured = (p.value(QStringLiteral("Flags")).toUInt() & ApFlagsPrivacy)
                      || p.value(QStringLiteral("WpaFlags")).toUInt() || rsnFlags;
        }
    }
    if (apPath.isEmpty()) {
        setConnectState(Failed, tr("Сеть «%1» больше не видна.").arg(ssid));
        return;
    }

    const QString saved = savedConnectionFor(ssid);
    if (!saved.isEmpty() && password.isEmpty()) {
        QDBusMessage call = QDBusMessage::createMethodCall(kNM, kNMPath, kNM, QStringLiteral("ActivateConnection"));
        call << QVariant::fromValue(QDBusObjectPath(saved)) << QVariant::fromValue(QDBusObjectPath(m_wifiDevice))
             << QVariant::fromValue(QDBusObjectPath(apPath));
        bus().asyncCall(call);
    } else {
        if (!saved.isEmpty()) {
            // Пароль ввели заново — старый профиль больше не нужен.
            bus().call(QDBusMessage::createMethodCall(kNM, saved, kConnectionIface, QStringLiteral("Delete")));
        }
        InterfaceMap settings;
        settings[QStringLiteral("connection")] = {{QStringLiteral("id"), ssid},
                                                  {QStringLiteral("type"), QStringLiteral("802-11-wireless")}};
        settings[QStringLiteral("802-11-wireless")] = {{QStringLiteral("ssid"), ssid.toUtf8()},
                                                       {QStringLiteral("mode"), QStringLiteral("infrastructure")}};
        if (secured) {
            const bool sae = rsnFlags & ApSecKeyMgmtSae;
            settings[QStringLiteral("802-11-wireless-security")] = {
                {QStringLiteral("key-mgmt"), sae ? QStringLiteral("sae") : QStringLiteral("wpa-psk")},
                {QStringLiteral("psk"), password}};
        }

        QDBusMessage call = QDBusMessage::createMethodCall(kNM, kNMPath, kNM, QStringLiteral("AddAndActivateConnection"));
        call << QVariant::fromValue(settings) << QVariant::fromValue(QDBusObjectPath(m_wifiDevice))
             << QVariant::fromValue(QDBusObjectPath(apPath));
        auto *watcher = new QDBusPendingCallWatcher(bus().asyncCall(call), this);
        connect(watcher, &QDBusPendingCallWatcher::finished, this, [this, ssid](QDBusPendingCallWatcher *w) {
            w->deleteLater();
            QDBusPendingReply<QDBusObjectPath, QDBusObjectPath> reply = *w;
            if (reply.isError()) {
                if (m_connectState == Connecting && m_pendingSsid == ssid) {
                    m_pollTimer.stop();
                    setConnectState(Failed, reply.error().message());
                }
                return;
            }
            m_addedConnection = reply.argumentAt<0>().path();
        });
    }

    m_connectTimeout.start(ConnectTimeoutMs);
    m_pollTimer.start();
}

void NetworkBackend::pollActivation()
{
    const uint state = getProperty(m_wifiDevice, kDeviceIface, QStringLiteral("State")).toUInt();
    if (state >= DeviceStatePrepare && state < DeviceStateActivated)
        m_sawActivating = true;

    auto fail = [this](const QString &message) {
        m_pollTimer.stop();
        if (!m_addedConnection.isEmpty()) {
            // Не оставляем профиль с неверным паролем — следующая попытка начнётся с чистого листа.
            bus().asyncCall(QDBusMessage::createMethodCall(kNM, m_addedConnection, kConnectionIface,
                                                           QStringLiteral("Delete")));
            m_addedConnection.clear();
        }
        setConnectState(Failed, message);
    };

    if (state == DeviceStateActivated) {
        refreshStatus();
        if (m_activeSsid == m_pendingSsid) {
            m_pollTimer.stop();
            refreshNetworks();
            setConnectState(Connected);
        }
    } else if (state == DeviceStateNeedAuth) {
        fail(tr("Неверный пароль."));
    } else if (state == DeviceStateFailed || (m_sawActivating && state <= DeviceStateDisconnected)) {
        fail(tr("Не удалось подключиться к сети «%1».").arg(m_pendingSsid));
    }
}

void NetworkBackend::setConnectState(ConnectState state, const QString &error)
{
    if (state != Connecting)
        m_connectTimeout.stop();
    m_connectState = state;
    m_error = error;
    emit connectStateChanged();
}

void NetworkBackend::setWifiEnabled(bool enabled)
{
    if (enabled == m_wifiEnabled)
        return;
    QDBusMessage call = QDBusMessage::createMethodCall(kNM, kNMPath, kPropsIface, QStringLiteral("Set"));
    call << kNM << QStringLiteral("WirelessEnabled") << QVariant::fromValue(QDBusVariant(enabled));
    const QDBusMessage reply = bus().call(call);
    if (reply.type() == QDBusMessage::ErrorMessage) {
        qWarning("Failed to switch Wi-Fi: %s", qPrintable(reply.errorMessage()));
        return;
    }
    m_wifiEnabled = enabled;
    if (!enabled && !m_networks.isEmpty()) {
        m_networks.clear();
        emit networksChanged();
    }
    emit statusChanged();
    // Радио включается/выключается не мгновенно — обновимся чуть позже.
    QTimer::singleShot(1500, this, &NetworkBackend::refresh);
}

void NetworkBackend::disconnectWifi()
{
    if (m_wifiDevice.isEmpty())
        return;
    QDBusMessage call = QDBusMessage::createMethodCall(kNM, m_wifiDevice, kDeviceIface, QStringLiteral("Disconnect"));
    bus().asyncCall(call);
    QTimer::singleShot(1000, this, &NetworkBackend::refresh);
}

void NetworkBackend::resetConnectState()
{
    m_pollTimer.stop();
    setConnectState(Idle);
}
