#pragma once

#include <QObject>
#include <QTimer>
#include <QVariantList>
#include <QtQml/qqmlregistration.h>

// Сеть через NetworkManager (org.freedesktop.NetworkManager по системной шине D-Bus):
// состояние проводного подключения, список Wi-Fi сетей и подключение с паролем.
// wifiAvailable == false — Wi-Fi модуля нет, подключение возможно только по кабелю.
class NetworkBackend : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

    Q_PROPERTY(bool online READ online NOTIFY statusChanged)
    Q_PROPERTY(bool wiredConnected READ wiredConnected NOTIFY statusChanged)
    Q_PROPERTY(bool wifiAvailable READ wifiAvailable NOTIFY statusChanged)
    // Радиомодуль Wi-Fi включён (NetworkManager WirelessEnabled)
    Q_PROPERTY(bool wifiEnabled READ wifiEnabled WRITE setWifiEnabled NOTIFY statusChanged)
    Q_PROPERTY(QString activeSsid READ activeSsid NOTIFY statusChanged)
    // Элементы: { ssid, strength (0..100), secured, active, saved }
    Q_PROPERTY(QVariantList networks READ networks NOTIFY networksChanged)
    Q_PROPERTY(bool scanning READ scanning NOTIFY scanningChanged)
    Q_PROPERTY(ConnectState connectState READ connectState NOTIFY connectStateChanged)
    Q_PROPERTY(QString errorString READ errorString NOTIFY connectStateChanged)

public:
    enum ConnectState { Idle, Connecting, Connected, Failed };
    Q_ENUM(ConnectState)

    explicit NetworkBackend(QObject *parent = nullptr);

    bool online() const { return m_online; }
    bool wiredConnected() const { return m_wiredConnected; }
    bool wifiAvailable() const { return !m_wifiDevice.isEmpty(); }
    bool wifiEnabled() const { return m_wifiEnabled; }
    void setWifiEnabled(bool enabled);
    QString activeSsid() const { return m_activeSsid; }
    QVariantList networks() const { return m_networks; }
    bool scanning() const { return m_scanning; }
    ConnectState connectState() const { return m_connectState; }
    QString errorString() const { return m_error; }

    Q_INVOKABLE void scan();
    // Сеть требует пароль, если secured == true и она ещё не сохранена.
    Q_INVOKABLE void connectToNetwork(const QString &ssid, const QString &password = QString());
    Q_INVOKABLE void resetConnectState();
    // Отключиться от текущей сети Wi-Fi (сеть остаётся сохранённой).
    Q_INVOKABLE void disconnectWifi();

public slots:
    void refresh();

private slots:
    void onDevicesChanged();

signals:
    void statusChanged();
    void networksChanged();
    void scanningChanged();
    void connectStateChanged();

private:
    void findDevices();
    void refreshStatus();
    void refreshNetworks();
    QString savedConnectionFor(const QString &ssid) const;
    void setConnectState(ConnectState state, const QString &error = QString());
    void pollActivation();

    bool m_online = false;
    bool m_wifiEnabled = true;
    bool m_wiredConnected = false;
    bool m_scanning = false;
    QString m_activeSsid;
    QVariantList m_networks;
    QStringList m_wiredDevices;
    QString m_wifiDevice;

    ConnectState m_connectState = Idle;
    QString m_error;
    QString m_pendingSsid;
    QString m_addedConnection;
    bool m_sawActivating = false;
    QTimer m_refreshTimer;
    QTimer m_pollTimer;
    QTimer m_connectTimeout;
};
