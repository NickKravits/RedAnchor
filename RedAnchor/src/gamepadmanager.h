#pragma once

#include <QFileSystemWatcher>
#include <QList>
#include <QObject>
#include <QSet>
#include <QColor>
#include <QElapsedTimer>
#include <QPointF>
#include <QPointer>
#include <QWindow>
#include <QTimer>
#include <QVariantList>
#include <QtQml/qqmlregistration.h>

#include "virtualpad.h"

class QSocketNotifier;
struct input_event;

// Читает геймпады напрямую через evdev (/dev/input/event*) и превращает
// нажатия в обычные клавиатурные события для активного окна:
//   D-pad / левый стик -> стрелки, ✕ -> Enter, ○ -> Esc, □ -> Backspace,
//   △ -> Space, L1/R1 -> PageUp/PageDown, L2/R2 -> Home/End,
//   Options -> Menu, Create -> F2, PS -> HomePage.
// Благодаря этому весь UI одинаково управляется и с геймпада, и с клавиатуры.
//
// Система раскладки RedAnchor (xboxEmulation): пока идёт игра без официальной поддержки
// DualSense, настоящие геймпады захватываются (EVIOCGRAB — игра их не видит), а их нажатия
// пересылаются в виртуальный контроллер Xbox 360 (VirtualPad). PS не пересылается — это
// кнопка оверлея; пока оверлей открыт (gameInputBlocked), игра нажатий не получает.
class GamepadManager : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

    Q_PROPERTY(bool connected READ connected NOTIFY activeChanged)
    Q_PROPERTY(bool dualSense READ dualSense NOTIFY activeChanged)
    Q_PROPERTY(QString controllerName READ controllerName NOTIFY activeChanged)
    // "usb", "bluetooth", "other" или пустая строка, если геймпада нет
    Q_PROPERTY(QString connection READ connection NOTIFY activeChanged)
    // MAC-адрес контроллера (для DualSense ядро отдаёт его даже по USB)
    Q_PROPERTY(QString macAddress READ macAddress NOTIFY activeChanged)
    Q_PROPERTY(int controllerCount READ controllerCount NOTIFY activeChanged)
    // Все подключённые геймпады:
    // [{ id, name, defaultName, mac, connection, dualSense, color, canSetColor,
    //    battery (0..100 или -1, если неизвестно), charging }]
    Q_PROPERTY(QVariantList gamepads READ gamepads NOTIFY gamepadsChanged)
    // Курсор на правом стике — только там, где его включили (окно браузера для входа в магазины):
    // включён ли, показан ли, где (в координатах окна) и в каком окне
    Q_PROPERTY(bool pointerEnabled READ pointerEnabled WRITE setPointerEnabled NOTIFY pointerChanged)
    Q_PROPERTY(bool pointerVisible READ pointerVisible NOTIFY pointerChanged)
    // false — кнопки геймпада не превращаются в нажатия клавиш, приходят только сигналом
    // buttonPressed (окно браузера: клавиши забрал бы себе Chromium)
    Q_PROPERTY(bool keyEventsEnabled READ keyEventsEnabled WRITE setKeyEventsEnabled NOTIFY keyEventsEnabledChanged)
    Q_PROPERTY(QPointF pointerPos READ pointerPos NOTIFY pointerChanged)
    Q_PROPERTY(QObject *pointerWindow READ pointerWindow NOTIFY pointerChanged)
    // Включить виртуальный контроллер Xbox для запущенной игры
    Q_PROPERTY(bool xboxEmulation READ xboxEmulation WRITE setXboxEmulation NOTIFY emulationChanged)
    // Виртуальный контроллер создан (false — нет доступа к /dev/uinput)
    Q_PROPERTY(bool emulationActive READ emulationActive NOTIFY emulationChanged)
    // Не пересылать нажатия в игру (открыт оверлей)
    Q_PROPERTY(bool gameInputBlocked READ gameInputBlocked WRITE setGameInputBlocked NOTIFY emulationChanged)

public:
    explicit GamepadManager(QObject *parent = nullptr);
    ~GamepadManager() override;

    bool connected() const { return m_active >= 0; }
    bool dualSense() const;
    QString controllerName() const;
    QString connection() const;
    QString macAddress() const;
    int controllerCount() const { return int(m_devices.size()); }

    // Есть ли подключённый DualSense по Bluetooth (опционально — с данным MAC).
    Q_INVOKABLE bool hasBluetoothDualSense(const QString &mac = QString()) const;

    QVariantList gamepads() const;
    bool keyEventsEnabled() const { return m_keyEventsEnabled; }
    void setKeyEventsEnabled(bool enabled);
    bool pointerEnabled() const { return m_pointerEnabled; }
    void setPointerEnabled(bool enabled);
    bool pointerVisible() const { return m_pointerVisible; }
    QPointF pointerPos() const { return m_pointerPos; }
    QObject *pointerWindow() const { return m_pointerWindow.data(); }
    bool xboxEmulation() const { return m_xboxEmulation; }
    void setXboxEmulation(bool enabled);
    bool emulationActive() const { return m_pad.isActive(); }
    bool gameInputBlocked() const { return m_gameInputBlocked; }
    void setGameInputBlocked(bool blocked);
    // Своё имя геймпада (пустая строка — вернуть стандартное). id — из списка gamepads.
    Q_INVOKABLE void rename(const QString &id, const QString &name);
    // Цвет световой панели DualSense. false — нет доступа к светодиоду.
    Q_INVOKABLE bool setColor(const QString &id, const QColor &color);

signals:
    void activeChanged();
    void gamepadsChanged();
    // Для уведомлений: имя геймпада и подключение ("usb" | "bluetooth" | "other")
    void gamepadConnected(const QString &name, const QString &connection);
    void gamepadDisconnected(const QString &name, const QString &connection);
    // Заряд опустился до 20, 15, 10 или 5 % (каждый порог — одно уведомление)
    void batteryLow(const QString &name, int percent);
    void pointerChanged();
    void keyEventsEnabledChanged();
    void emulationChanged();
    // Имя кнопки: cross, circle, square, triangle, l1, r1, l2, r2,
    // create, options, ps, l3, r3, up, down, left, right
    void buttonPressed(const QString &button);

private:
    enum Dir { None, Up, Down, Left, Right };

    struct Device {
        int fd = -1;
        QSocketNotifier *notifier = nullptr;
        QString path;
        QString name;
        QString uniq;
        quint16 bus = 0;
        quint16 vendor = 0;
        quint16 product = 0;
        QString ledPath;   // /sys/class/leds/inputN:rgb:indicator (только DualSense)
        QString batteryPath;   // /sys/class/power_supply/… (если драйвер сообщает заряд)
        int battery = -1;
        bool charging = false;
        int notifiedLevel = 100;   // самый низкий порог, о котором уже уведомили
        // Оси: левый стик X/Y и правый стик X/Y
        int absMin[4] = {-32768, -32768, -32768, -32768};
        int absMax[4] = {32767, 32767, 32767, 32767};
        // Курки L2 / R2 (ABS_Z, ABS_RZ), если они аналоговые
        bool analogTriggers = false;
        int trigMin[2] = {0, 0};
        int trigMax[2] = {255, 255};
        float stick[2] = {0.f, 0.f};
        float rstick[2] = {0.f, 0.f};
        int hat[2] = {0, 0};
        bool dpadButtons[4] = {false, false, false, false}; // up, down, left, right
        Dir dir = None;
        bool isDualSense() const;
    };

    void rescan();
    void openDevice(const QString &path);
    // Тачпад и датчики движения геймпада Sony — отдельные устройства ввода. Тачпад система
    // считает мышью (курсор ездит по экрану от касаний) — забираем его себе (EVIOCGRAB)
    bool grabTouchpad(int fd, const QString &path);
    void closeTouchpad(int fd);
    void closeDevice(int index);
    void readEvents(int fd);
    void handleKey(Device &dev, int code, int value);
    void updateDirection(Device &dev);
    void updateActive();
    void sendKey(int key, bool press, bool autoRepeat = false);
    void forwardEvent(const Device &dev, const input_event &ev);
    void setGrab(Device &dev, bool grab);
    void onRepeat();
    QString displayName(const Device &dev) const;
    static QString connectionName(const Device &dev);
    static QString settingsKey(const Device &dev);
    bool writeColor(const Device &dev, const QColor &color) const;
    void pollBattery();
    void onPointerTick();
    void hidePointer();
    void sendMouse(QEvent::Type type, Qt::MouseButton button);
    static QString findBattery(const Device &dev);

    QList<Device> m_devices;
    QSet<QString> m_ignored;
    struct Touchpad {
        int fd = -1;
        QString path;
        QSocketNotifier *notifier = nullptr;
    };
    QList<Touchpad> m_touchpads;
    int m_active = -1;

    QFileSystemWatcher m_watcher;
    QTimer m_rescanTimer;
    QTimer m_pollTimer;
    QTimer m_repeatTimer;
    QTimer m_batteryTimer;

    QTimer m_pointerTimer;
    QTimer m_pointerHide;
    QElapsedTimer m_pointerClock;
    QPointF m_pointerPos;
    QPointer<QWindow> m_pointerWindow;
    bool m_pointerVisible = false;
    bool m_pointerEnabled = false;
    bool m_keyEventsEnabled = true;
    int m_repeatKey = 0;

    VirtualPad m_pad;
    bool m_xboxEmulation = false;
    bool m_gameInputBlocked = false;
};
