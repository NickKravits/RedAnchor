#include "gamepadmanager.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFocusEvent>
#include <QSettings>
#include <QGuiApplication>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QSocketNotifier>
#include <QWindow>

#include <algorithm>
#include <cerrno>
#include <cmath>
#include <fcntl.h>
#include <linux/input.h>
#include <sys/ioctl.h>
#include <unistd.h>

namespace {

constexpr quint16 SonyVendor = 0x054c;
constexpr quint16 DualSenseProduct = 0x0ce6;
constexpr quint16 DualSenseEdgeProduct = 0x0df2;

constexpr int RepeatDelayMs = 380;
constexpr int RepeatIntervalMs = 90;
constexpr float StickPress = 0.6f;
// Курсор на правом стике
constexpr float PointerDeadzone = 0.15f;
constexpr float PointerSpeed = 1500.f;    // пикселей в секунду при полном отклонении (для 1080p)
constexpr int PointerHideMs = 5000;
constexpr float StickRelease = 0.35f;

constexpr size_t BitsPerLong = sizeof(unsigned long) * 8;

bool testBit(const unsigned long *bits, int bit)
{
    return (bits[bit / BitsPerLong] >> (bit % BitsPerLong)) & 1UL;
}

struct ButtonMapping {
    int code;
    const char *name;
    int key;
};

const ButtonMapping kButtons[] = {
    {BTN_SOUTH, "cross", Qt::Key_Return},
    {BTN_EAST, "circle", Qt::Key_Escape},
    {BTN_WEST, "square", Qt::Key_Backspace},
    {BTN_NORTH, "triangle", Qt::Key_Space},
    {BTN_TL, "l1", Qt::Key_PageUp},
    {BTN_TR, "r1", Qt::Key_PageDown},
    {BTN_TL2, "l2", Qt::Key_Home},
    {BTN_TR2, "r2", Qt::Key_End},
    {BTN_SELECT, "create", Qt::Key_F2},
    {BTN_START, "options", Qt::Key_Menu},
    {BTN_MODE, "ps", Qt::Key_HomePage},
    {BTN_THUMBL, "l3", Qt::Key_F3},
    {BTN_THUMBR, "r3", Qt::Key_F4},
};

int dirKey(int dir)
{
    switch (dir) {
    case 1: return Qt::Key_Up;
    case 2: return Qt::Key_Down;
    case 3: return Qt::Key_Left;
    case 4: return Qt::Key_Right;
    default: return 0;
    }
}

const char *dirName(int dir)
{
    switch (dir) {
    case 1: return "up";
    case 2: return "down";
    case 3: return "left";
    case 4: return "right";
    default: return "";
    }
}

// Окно для событий геймпада. Окно в фокусе — если оно видимо. Иначе (после закрытия
// второго окна — браузера — фокус под Wayland может не вернуться, а focusWindow указывать
// на закрытое окно) — видимое окно поверх главного (браузер), затем главное окно оболочки.
QWindow *targetWindow()
{
    // Открытый оверлей по PS — всегда первый, даже если композитор не отдал ему фокус
    for (QWindow *window : QGuiApplication::topLevelWindows()) {
        if (window->isVisible() && window->objectName() == QLatin1String("systemOverlay"))
            return window;
    }
    QWindow *focus = QGuiApplication::focusWindow();
    if (focus && focus->isVisible())
        return focus;
    QWindow *main = nullptr;
    for (QWindow *window : QGuiApplication::topLevelWindows()) {
        if (!window->isVisible())
            continue;
        if (window->transientParent())
            return window;
        if (!main)
            main = window;
    }
    return main;
}

} // namespace

bool GamepadManager::Device::isDualSense() const
{
    return vendor == SonyVendor && (product == DualSenseProduct || product == DualSenseEdgeProduct);
}

GamepadManager::GamepadManager(QObject *parent)
    : QObject(parent)
{
    m_rescanTimer.setSingleShot(true);
    m_rescanTimer.setInterval(400); // udev выставляет права чуть позже появления узла
    connect(&m_rescanTimer, &QTimer::timeout, this, &GamepadManager::rescan);

    m_watcher.addPath(QStringLiteral("/dev/input"));
    connect(&m_watcher, &QFileSystemWatcher::directoryChanged, this, [this] { m_rescanTimer.start(); });

    // Страховка на случай, если inotify недоступен или права выдали позже.
    m_pollTimer.setInterval(3000);
    connect(&m_pollTimer, &QTimer::timeout, this, &GamepadManager::rescan);
    m_pollTimer.start();

    connect(&m_repeatTimer, &QTimer::timeout, this, &GamepadManager::onRepeat);

    m_pointerTimer.setInterval(16);
    connect(&m_pointerTimer, &QTimer::timeout, this, &GamepadManager::onPointerTick);
    m_pointerHide.setSingleShot(true);
    m_pointerHide.setInterval(PointerHideMs);
    connect(&m_pointerHide, &QTimer::timeout, this, &GamepadManager::hidePointer);

    m_batteryTimer.setInterval(30 * 1000);
    connect(&m_batteryTimer, &QTimer::timeout, this, &GamepadManager::pollBattery);
    m_batteryTimer.start();

    rescan();
}

GamepadManager::~GamepadManager()
{
    for (const Device &dev : std::as_const(m_devices)) {
        delete dev.notifier;
        ::close(dev.fd);
    }
    for (const Touchpad &pad : std::as_const(m_touchpads)) {
        delete pad.notifier;
        ::close(pad.fd);
    }
}

bool GamepadManager::dualSense() const
{
    return m_active >= 0 && m_devices[m_active].isDualSense();
}

QString GamepadManager::controllerName() const
{
    return m_active >= 0 ? displayName(m_devices[m_active]) : QString();
}

// Ключ настроек геймпада: MAC без двоеточий, а если его нет — VID:PID.
QString GamepadManager::settingsKey(const Device &dev)
{
    QString id = dev.uniq;
    id.remove(QLatin1Char(':'));
    if (id.isEmpty())
        id = QStringLiteral("%1_%2").arg(dev.vendor, 4, 16, QLatin1Char('0')).arg(dev.product, 4, 16, QLatin1Char('0'));
    return QStringLiteral("Gamepads/") + id;
}

QString GamepadManager::displayName(const Device &dev) const
{
    const QString alias = QSettings().value(settingsKey(dev) + QStringLiteral("/name")).toString();
    if (!alias.isEmpty())
        return alias;
    return dev.isDualSense() ? QStringLiteral("DualSense") : dev.name;
}

QString GamepadManager::connectionName(const Device &dev)
{
    switch (dev.bus) {
    case BUS_USB: return QStringLiteral("usb");
    case BUS_BLUETOOTH: return QStringLiteral("bluetooth");
    default: return QStringLiteral("other");
    }
}

QVariantList GamepadManager::gamepads() const
{
    QVariantList list;
    QSettings settings;
    for (const Device &dev : m_devices) {
        const QString conn = connectionName(dev);
        list.append(QVariantMap{
            {QStringLiteral("id"), dev.path},
            {QStringLiteral("name"), displayName(dev)},
            {QStringLiteral("defaultName"), dev.isDualSense() ? QStringLiteral("DualSense") : dev.name},
            {QStringLiteral("mac"), dev.uniq},
            {QStringLiteral("connection"), conn},
            {QStringLiteral("dualSense"), dev.isDualSense()},
            {QStringLiteral("color"), settings.value(settingsKey(dev) + QStringLiteral("/color"), QStringLiteral("#2f6bff"))},
            {QStringLiteral("canSetColor"), !dev.ledPath.isEmpty()},
            {QStringLiteral("battery"), dev.battery},
            {QStringLiteral("charging"), dev.charging},
        });
    }
    return list;
}

void GamepadManager::rename(const QString &id, const QString &name)
{
    for (const Device &dev : std::as_const(m_devices)) {
        if (dev.path != id)
            continue;
        QSettings settings;
        const QString trimmed = name.trimmed().left(24);
        if (trimmed.isEmpty())
            settings.remove(settingsKey(dev) + QStringLiteral("/name"));
        else
            settings.setValue(settingsKey(dev) + QStringLiteral("/name"), trimmed);
        emit gamepadsChanged();
        emit activeChanged();
        return;
    }
}

bool GamepadManager::writeColor(const Device &dev, const QColor &color) const
{
    if (dev.ledPath.isEmpty())
        return false;
    QFile intensity(dev.ledPath + QStringLiteral("/multi_intensity"));
    QFile brightness(dev.ledPath + QStringLiteral("/brightness"));
    if (!intensity.open(QIODevice::WriteOnly) || !brightness.open(QIODevice::WriteOnly)) {
        qWarning("No access to %s (install the RedAnchor udev rule)", qPrintable(dev.ledPath));
        return false;
    }
    intensity.write(QStringLiteral("%1 %2 %3").arg(color.red()).arg(color.green()).arg(color.blue()).toLatin1());
    brightness.write("255");
    return true;
}

bool GamepadManager::setColor(const QString &id, const QColor &color)
{
    for (const Device &dev : std::as_const(m_devices)) {
        if (dev.path != id)
            continue;
        if (!writeColor(dev, color))
            return false;
        QSettings().setValue(settingsKey(dev) + QStringLiteral("/color"), color.name());
        emit gamepadsChanged();
        return true;
    }
    return false;
}

QString GamepadManager::connection() const
{
    return m_active >= 0 ? connectionName(m_devices[m_active]) : QString();
}

QString GamepadManager::macAddress() const
{
    return m_active >= 0 ? m_devices[m_active].uniq : QString();
}

bool GamepadManager::hasBluetoothDualSense(const QString &mac) const
{
    for (const Device &dev : m_devices) {
        if (dev.isDualSense() && dev.bus == BUS_BLUETOOTH
            && (mac.isEmpty() || dev.uniq.compare(mac, Qt::CaseInsensitive) == 0))
            return true;
    }
    return false;
}

void GamepadManager::rescan()
{
    const QDir dir(QStringLiteral("/dev/input"));
    const QStringList entries = dir.entryList({QStringLiteral("event*")}, QDir::System);

    QSet<QString> present;
    for (const QString &entry : entries)
        present.insert(dir.filePath(entry));

    // Узел исчез — забываем, что мы его игнорировали (номер может быть переиспользован).
    for (auto it = m_ignored.begin(); it != m_ignored.end();) {
        if (!present.contains(*it))
            it = m_ignored.erase(it);
        else
            ++it;
    }

    for (const QString &path : std::as_const(present)) {
        if (m_ignored.contains(path))
            continue;
        const bool known = std::any_of(m_devices.cbegin(), m_devices.cend(),
                                       [&](const Device &d) { return d.path == path; })
                           || std::any_of(m_touchpads.cbegin(), m_touchpads.cend(),
                                          [&](const Touchpad &t) { return t.path == path; });
        if (!known)
            openDevice(path);
    }
}

void GamepadManager::openDevice(const QString &path)
{
    const int fd = ::open(QFile::encodeName(path).constData(), O_RDONLY | O_NONBLOCK | O_CLOEXEC);
    if (fd < 0) {
        // EACCES — права ещё не выданы, попробуем на следующем опросе.
        if (errno != EACCES && errno != EPERM)
            m_ignored.insert(path);
        return;
    }

    unsigned long keyBits[(KEY_MAX + BitsPerLong) / BitsPerLong] = {};
    if (ioctl(fd, EVIOCGBIT(EV_KEY, sizeof(keyBits)), keyBits) < 0 || !testBit(keyBits, BTN_GAMEPAD)) {
        if (grabTouchpad(fd, path))
            return;
        ::close(fd);
        m_ignored.insert(path);
        return;
    }

    char buf[256] = {};
    // Наш же виртуальный контроллер Xbox — не настоящий геймпад
    if (ioctl(fd, EVIOCGPHYS(sizeof(buf) - 1), buf) >= 0 && qstrcmp(buf, VirtualPad::Phys) == 0) {
        ::close(fd);
        m_ignored.insert(path);
        return;
    }
    std::fill(std::begin(buf), std::end(buf), 0);

    Device dev;
    dev.fd = fd;
    dev.path = path;

    if (ioctl(fd, EVIOCGNAME(sizeof(buf) - 1), buf) >= 0)
        dev.name = QString::fromUtf8(buf);
    std::fill(std::begin(buf), std::end(buf), 0);
    if (ioctl(fd, EVIOCGUNIQ(sizeof(buf) - 1), buf) >= 0)
        dev.uniq = QString::fromUtf8(buf).toUpper();

    input_id id = {};
    if (ioctl(fd, EVIOCGID, &id) >= 0) {
        dev.bus = id.bustype;
        dev.vendor = id.vendor;
        dev.product = id.product;
    }

    static const int axes[4] = {ABS_X, ABS_Y, ABS_RX, ABS_RY};
    for (int axis = 0; axis < 4; ++axis) {
        input_absinfo info = {};
        if (ioctl(fd, EVIOCGABS(axes[axis]), &info) >= 0 && info.maximum > info.minimum) {
            dev.absMin[axis] = info.minimum;
            dev.absMax[axis] = info.maximum;
        }
    }
    unsigned long absBits[(ABS_MAX + BitsPerLong) / BitsPerLong] = {};
    if (ioctl(fd, EVIOCGBIT(EV_ABS, sizeof(absBits)), absBits) >= 0 && testBit(absBits, ABS_Z) && testBit(absBits, ABS_RZ)) {
        dev.analogTriggers = true;
        static const int triggers[2] = {ABS_Z, ABS_RZ};
        for (int t = 0; t < 2; ++t) {
            input_absinfo info = {};
            if (ioctl(fd, EVIOCGABS(triggers[t]), &info) >= 0 && info.maximum > info.minimum) {
                dev.trigMin[t] = info.minimum;
                dev.trigMax[t] = info.maximum;
            }
        }
    }

    // Световая панель DualSense: hid-playstation называет её по узлу input геймпада.
    if (dev.isDualSense()) {
        const QString input = QFileInfo(QStringLiteral("/sys/class/input/%1/device").arg(QFileInfo(path).fileName()))
                                  .canonicalFilePath();
        const QString led = QStringLiteral("/sys/class/leds/%1:rgb:indicator").arg(QFileInfo(input).fileName());
        if (!input.isEmpty() && QFileInfo::exists(led))
            dev.ledPath = led;
    }

    dev.notifier = new QSocketNotifier(fd, QSocketNotifier::Read, this);
    connect(dev.notifier, &QSocketNotifier::activated, this, [this, fd] { readEvents(fd); });

    qInfo("Gamepad connected: %s (%s, bus 0x%x, %s)", qPrintable(dev.name), qPrintable(path), dev.bus,
          qPrintable(dev.uniq));
    // Идёт игра с системой раскладки — новый геймпад тоже прячем от неё
    if (m_pad.isActive())
        setGrab(dev, true);
    m_devices.append(dev);
    // Возвращаем выбранный пользователем цвет световой панели
    const QString savedColor = QSettings().value(settingsKey(dev) + QStringLiteral("/color")).toString();
    if (!savedColor.isEmpty())
        writeColor(dev, QColor(savedColor));
    updateActive();
    emit gamepadConnected(displayName(dev), connectionName(dev));
    // Драйвер регистрирует батарею чуть позже узла input
    QTimer::singleShot(2000, this, &GamepadManager::pollBattery);
}

bool GamepadManager::grabTouchpad(int fd, const QString &path)
{
    input_id id = {};
    if (ioctl(fd, EVIOCGID, &id) < 0 || id.vendor != SonyVendor)
        return false;
    unsigned long propBits[(INPUT_PROP_MAX + BitsPerLong) / BitsPerLong] = {};
    unsigned long absBits[(ABS_MAX + BitsPerLong) / BitsPerLong] = {};
    ioctl(fd, EVIOCGPROP(sizeof(propBits)), propBits);
    ioctl(fd, EVIOCGBIT(EV_ABS, sizeof(absBits)), absBits);
    // Тачпад: указатель или «кнопочная панель» с координатами касаний.
    // Датчики движения (INPUT_PROP_ACCELEROMETER) курсор не двигают — их не трогаем.
    const bool touchpad = !testBit(propBits, INPUT_PROP_ACCELEROMETER)
                          && (testBit(propBits, INPUT_PROP_POINTER) || testBit(propBits, INPUT_PROP_BUTTONPAD))
                          && testBit(absBits, ABS_MT_POSITION_X);
    if (!touchpad)
        return false;
    if (ioctl(fd, EVIOCGRAB, 1) < 0) {
        qWarning("Gamepad touchpad %s: EVIOCGRAB failed", qPrintable(path));
        return false;
    }
    Touchpad pad;
    pad.fd = fd;
    pad.path = path;
    pad.notifier = new QSocketNotifier(fd, QSocketNotifier::Read, this);
    // События тачпада просто вычитываем: курсор от них больше не двигается
    connect(pad.notifier, &QSocketNotifier::activated, this, [this, fd] {
        input_event events[64];
        for (;;) {
            const ssize_t n = ::read(fd, events, sizeof(events));
            if (n > 0)
                continue;
            if (n < 0 && (errno == EAGAIN || errno == EINTR))
                return;
            closeTouchpad(fd);   // отключили
            return;
        }
    });
    m_touchpads.append(pad);
    qInfo("Gamepad touchpad grabbed: %s", qPrintable(path));
    return true;
}

void GamepadManager::closeTouchpad(int fd)
{
    for (int i = 0; i < m_touchpads.size(); ++i) {
        if (m_touchpads.at(i).fd != fd)
            continue;
        Touchpad pad = m_touchpads.takeAt(i);
        pad.notifier->setEnabled(false);
        pad.notifier->deleteLater();
        ::close(pad.fd);
        return;
    }
}

// Батарея геймпада в /sys/class/power_supply: у DualSense она называется
// ps-controller-battery-<mac>, у остальных ищем батарею того же HID-устройства.
QString GamepadManager::findBattery(const Device &dev)
{
    const QString base = QStringLiteral("/sys/class/power_supply/");
    if (!dev.uniq.isEmpty()) {
        const QString named = base + QStringLiteral("ps-controller-battery-") + dev.uniq.toLower();
        if (QFileInfo::exists(named + QStringLiteral("/capacity")))
            return named;
    }
    const QString input = QFileInfo(QStringLiteral("/sys/class/input/%1").arg(QFileInfo(dev.path).fileName()))
                              .canonicalFilePath();
    if (input.isEmpty())
        return {};
    const QDir dir(base);
    for (const QString &name : dir.entryList(QDir::Dirs | QDir::NoDotAndDotDot)) {
        const QString supply = base + name;
        if (!QFileInfo::exists(supply + QStringLiteral("/capacity")))
            continue;
        const QString owner = QFileInfo(supply + QStringLiteral("/device")).canonicalFilePath();
        if (!owner.isEmpty() && input.startsWith(owner + QLatin1Char('/')))
            return supply;
    }
    return {};
}

void GamepadManager::pollBattery()
{
    static const int thresholds[] = {5, 10, 15, 20};
    bool changed = false;
    for (Device &dev : m_devices) {
        if (dev.batteryPath.isEmpty())
            dev.batteryPath = findBattery(dev);
        if (dev.batteryPath.isEmpty())
            continue;

        QFile capacityFile(dev.batteryPath + QStringLiteral("/capacity"));
        QFile statusFile(dev.batteryPath + QStringLiteral("/status"));
        if (!capacityFile.open(QIODevice::ReadOnly))
            continue;
        bool ok = false;
        const int capacity = QString::fromLatin1(capacityFile.readAll()).trimmed().toInt(&ok);
        if (!ok)
            continue;
        QString status;
        if (statusFile.open(QIODevice::ReadOnly))
            status = QString::fromLatin1(statusFile.readAll()).trimmed();
        const bool charging = status == QLatin1String("Charging") || status == QLatin1String("Full");

        if (capacity != dev.battery || charging != dev.charging) {
            dev.battery = capacity;
            dev.charging = charging;
            changed = true;
        }

        if (charging || capacity > 25) {
            dev.notifiedLevel = 100;   // зарядили — снова предупреждаем с 20 %
            continue;
        }
        // Самый низкий пройденный порог, о котором ещё не предупреждали
        for (int t : thresholds) {
            if (capacity <= t) {
                if (t < dev.notifiedLevel) {
                    dev.notifiedLevel = t;
                    emit batteryLow(displayName(dev), capacity);
                }
                break;
            }
        }
    }
    if (changed)
        emit gamepadsChanged();
}

void GamepadManager::closeDevice(int index)
{
    Device dev = m_devices.takeAt(index);
    qInfo("Gamepad disconnected: %s", qPrintable(dev.name));
    if (dev.dir != None) {
        m_repeatTimer.stop();
        sendKey(dirKey(dev.dir), false);
    }
    dev.notifier->setEnabled(false);
    dev.notifier->deleteLater();
    ::close(dev.fd);
    updateActive();
    emit gamepadDisconnected(displayName(dev), connectionName(dev));
}

void GamepadManager::readEvents(int fd)
{
    int index = -1;
    for (int i = 0; i < m_devices.size(); ++i) {
        if (m_devices[i].fd == fd) {
            index = i;
            break;
        }
    }
    if (index < 0)
        return;

    input_event events[64];
    for (;;) {
        const ssize_t n = ::read(fd, events, sizeof(events));
        if (n < 0) {
            if (errno == EAGAIN || errno == EINTR)
                return;
            closeDevice(index); // ENODEV — устройство отключено
            return;
        }
        if (n == 0) {
            closeDevice(index);
            return;
        }

        Device &dev = m_devices[index];
        const int count = int(n / sizeof(input_event));
        bool dirDirty = false;
        for (int i = 0; i < count; ++i) {
            const input_event &ev = events[i];
            if (ev.type == EV_KEY) {
                switch (ev.code) {
                case BTN_DPAD_UP: dev.dpadButtons[0] = ev.value; dirDirty = true; break;
                case BTN_DPAD_DOWN: dev.dpadButtons[1] = ev.value; dirDirty = true; break;
                case BTN_DPAD_LEFT: dev.dpadButtons[2] = ev.value; dirDirty = true; break;
                case BTN_DPAD_RIGHT: dev.dpadButtons[3] = ev.value; dirDirty = true; break;
                default: handleKey(dev, ev.code, ev.value); break;
                }
            } else if (ev.type == EV_ABS) {
                switch (ev.code) {
                case ABS_HAT0X: dev.hat[0] = ev.value; dirDirty = true; break;
                case ABS_HAT0Y: dev.hat[1] = ev.value; dirDirty = true; break;
                case ABS_X:
                case ABS_Y: {
                    const int axis = ev.code == ABS_X ? 0 : 1;
                    const float range = float(dev.absMax[axis] - dev.absMin[axis]);
                    dev.stick[axis] = (float(ev.value - dev.absMin[axis]) / range) * 2.f - 1.f;
                    dirDirty = true;
                    break;
                }
                case ABS_RX:
                case ABS_RY: {
                    const int axis = ev.code == ABS_RX ? 2 : 3;
                    const float range = float(dev.absMax[axis] - dev.absMin[axis]);
                    dev.rstick[axis - 2] = (float(ev.value - dev.absMin[axis]) / range) * 2.f - 1.f;
                    if (m_pointerEnabled && std::hypot(dev.rstick[0], dev.rstick[1]) > PointerDeadzone
                        && !m_pointerTimer.isActive()) {
                        m_pointerClock.restart();
                        m_pointerTimer.start();
                    }
                    break;
                }
                default: break;
                }
            }
        }
        if (dirDirty)
            updateDirection(dev);
        if (m_pad.isActive() && !m_gameInputBlocked) {
            for (int i = 0; i < count; ++i)
                forwardEvent(dev, events[i]);
        }
    }
}

// ---------- Система раскладки RedAnchor: виртуальный контроллер Xbox ----------

void GamepadManager::setGrab(Device &dev, bool grab)
{
    if (ioctl(dev.fd, EVIOCGRAB, grab ? 1 : 0) < 0)
        qWarning("Gamepad %s: EVIOCGRAB %d failed", qPrintable(dev.path), int(grab));
}

void GamepadManager::setXboxEmulation(bool enabled)
{
    if (enabled == m_xboxEmulation)
        return;
    m_xboxEmulation = enabled;
    if (enabled) {
        QString error;
        if (m_pad.create(&error)) {
            qInfo("Xbox controller emulation on");
            for (Device &dev : m_devices)
                setGrab(dev, true);
        } else {
            qWarning("Xbox controller emulation unavailable: /dev/uinput: %s", qPrintable(error));
        }
    } else if (m_pad.isActive()) {
        for (Device &dev : m_devices)
            setGrab(dev, false);
        m_pad.destroy();
        qInfo("Xbox controller emulation off");
    }
    emit emulationChanged();
}

void GamepadManager::setGameInputBlocked(bool blocked)
{
    if (blocked == m_gameInputBlocked)
        return;
    m_gameInputBlocked = blocked;
    // Всё, что было зажато в игре, отпускаем — иначе персонаж так и будет бежать
    if (blocked && m_pad.isActive())
        m_pad.reset();
    emit emulationChanged();
}

void GamepadManager::forwardEvent(const Device &dev, const input_event &ev)
{
    if (ev.type == EV_SYN) {
        if (ev.code == SYN_REPORT)
            m_pad.sync();
        return;
    }
    if (ev.type == EV_KEY) {
        switch (ev.code) {
        case BTN_SOUTH: m_pad.button(BTN_A, ev.value); break;
        case BTN_EAST: m_pad.button(BTN_B, ev.value); break;
        // У xpad коды X и Y «перепутаны» относительно сторон света: левая кнопка (□) — BTN_X,
        // верхняя (△) — BTN_Y. Так его и понимают SDL и Wine.
        case BTN_WEST: m_pad.button(BTN_X, ev.value); break;
        case BTN_NORTH: m_pad.button(BTN_Y, ev.value); break;
        case BTN_TL:
        case BTN_TR:
        case BTN_SELECT:
        case BTN_START:
        case BTN_THUMBL:
        case BTN_THUMBR:
            m_pad.button(ev.code, ev.value);
            break;
        case BTN_TL2:
        case BTN_TR2:
            // Цифровые курки — только у геймпадов без аналоговых
            if (!dev.analogTriggers)
                m_pad.trigger(ev.code == BTN_TL2 ? ABS_Z : ABS_RZ, ev.value ? 1.f : 0.f);
            break;
        case BTN_DPAD_UP:
        case BTN_DPAD_DOWN:
        case BTN_DPAD_LEFT:
        case BTN_DPAD_RIGHT:
            m_pad.hat(dev.dpadButtons[3] - dev.dpadButtons[2], dev.dpadButtons[1] - dev.dpadButtons[0]);
            break;
        default:
            break;   // PS — кнопка оверлея, в игру не идёт
        }
        return;
    }
    if (ev.type == EV_ABS) {
        switch (ev.code) {
        case ABS_X: m_pad.stick(ABS_X, dev.stick[0]); break;
        case ABS_Y: m_pad.stick(ABS_Y, dev.stick[1]); break;
        case ABS_RX: m_pad.stick(ABS_RX, dev.rstick[0]); break;
        case ABS_RY: m_pad.stick(ABS_RY, dev.rstick[1]); break;
        case ABS_Z:
        case ABS_RZ: {
            const int t = ev.code == ABS_Z ? 0 : 1;
            m_pad.trigger(ev.code, float(ev.value - dev.trigMin[t]) / float(dev.trigMax[t] - dev.trigMin[t]));
            break;
        }
        case ABS_HAT0X:
        case ABS_HAT0Y:
            m_pad.hat(dev.hat[0], dev.hat[1]);
            break;
        default:
            break;
        }
    }
}

void GamepadManager::handleKey(Device &dev, int code, int value)
{
    Q_UNUSED(dev);
    if (value == 2) // автоповтор ядра нам не нужен
        return;
    // Курсор: ✕ щёлкает там, где курсор (пока он виден), R3 — щелчок всегда
    if (m_pointerEnabled && ((code == BTN_SOUTH && m_pointerVisible) || code == BTN_THUMBR)) {
        if (!m_pointerVisible && value == 1)
            onPointerTick();   // показать курсор там, где он был
        if (m_pointerWindow) {
            sendMouse(value == 1 ? QEvent::MouseButtonPress : QEvent::MouseButtonRelease, Qt::LeftButton);
            m_pointerHide.start();
        }
        if (value == 1)
            emit buttonPressed(QString::fromLatin1(code == BTN_SOUTH ? "cross" : "r3"));
        return;
    }
    for (const ButtonMapping &m : kButtons) {
        if (m.code != code)
            continue;
        sendKey(m.key, value == 1);
        if (value == 1)
            emit buttonPressed(QString::fromLatin1(m.name));
        return;
    }
}

void GamepadManager::updateDirection(Device &dev)
{
    Dir dir = None;
    if (dev.hat[1] < 0 || dev.dpadButtons[0]) dir = Up;
    else if (dev.hat[1] > 0 || dev.dpadButtons[1]) dir = Down;
    else if (dev.hat[0] < 0 || dev.dpadButtons[2]) dir = Left;
    else if (dev.hat[0] > 0 || dev.dpadButtons[3]) dir = Right;
    else {
        // Стик с гистерезисом: удерживаем текущее направление до StickRelease.
        const float x = dev.stick[0], y = dev.stick[1];
        const float ax = std::abs(x), ay = std::abs(y);
        const float threshold = dev.dir == None ? StickPress : StickRelease;
        if (std::max(ax, ay) >= threshold) {
            if (ay >= ax) dir = y < 0 ? Up : Down;
            else dir = x < 0 ? Left : Right;
        }
    }

    if (dir == dev.dir)
        return;

    if (dev.dir != None) {
        m_repeatTimer.stop();
        sendKey(dirKey(dev.dir), false);
    }
    dev.dir = dir;
    if (dir != None) {
        hidePointer();   // взялись за крестовину или левый стик — курсор не нужен
        m_repeatKey = dirKey(dir);
        sendKey(m_repeatKey, true);
        emit buttonPressed(QString::fromLatin1(dirName(dir)));
        m_repeatTimer.start(RepeatDelayMs);
    }
}

void GamepadManager::onRepeat()
{
    m_repeatTimer.setInterval(RepeatIntervalMs);
    sendKey(m_repeatKey, true, true);
    // Без клавиш (окно браузера) автоповтор направлений приходит сигналом
    if (!m_keyEventsEnabled) {
        for (const Device &dev : std::as_const(m_devices)) {
            if (dev.dir != None && dirKey(dev.dir) == m_repeatKey) {
                emit buttonPressed(QString::fromLatin1(dirName(dev.dir)));
                break;
            }
        }
    }
}

void GamepadManager::setKeyEventsEnabled(bool enabled)
{
    if (enabled == m_keyEventsEnabled)
        return;
    m_keyEventsEnabled = enabled;
    emit keyEventsEnabledChanged();
}

void GamepadManager::updateActive()
{
    int best = -1;
    int bestScore = -1;
    for (int i = 0; i < m_devices.size(); ++i) {
        const Device &d = m_devices[i];
        const int score = (d.isDualSense() ? 2 : 0) + (d.bus == BUS_BLUETOOTH ? 1 : 0);
        if (score > bestScore) {
            best = i;
            bestScore = score;
        }
    }
    m_active = best;
    emit activeChanged();
    emit gamepadsChanged();
}

void GamepadManager::sendKey(int key, bool press, bool autoRepeat)
{
    if (!key)
        return;
    // Отпускание всё равно отправляем, чтобы не осталось «залипших» клавиш
    if (!m_keyEventsEnabled && press)
        return;
    QWindow *window = targetWindow();
    if (!window)
        return;
    // Окно не в фокусе (только что закрылось другое окно, а композитор не вернул фокус) —
    // Qt Quick тогда не отдаёт клавиши ни одному элементу, и геймпад «перестаёт работать».
    // Просим фокус у композитора, а если фокуса нет ни у одного окна — возвращаем его сразу
    if (press && QGuiApplication::focusWindow() != window) {
        window->requestActivate();
        if (!QGuiApplication::focusWindow()) {
            QFocusEvent focusIn(QEvent::FocusIn, Qt::ActiveWindowFocusReason);
            QCoreApplication::sendEvent(window, &focusIn);
        }
    }
    QCoreApplication::postEvent(window, new QKeyEvent(press ? QEvent::KeyPress : QEvent::KeyRelease, key,
                                                      Qt::NoModifier, QString(), autoRepeat));
}

// ---------- Курсор на правом стике ----------

void GamepadManager::setPointerEnabled(bool enabled)
{
    if (enabled == m_pointerEnabled)
        return;
    m_pointerEnabled = enabled;
    if (!enabled) {
        m_pointerTimer.stop();
        m_pointerVisible = false;
    }
    emit pointerChanged();
}

void GamepadManager::onPointerTick()
{
    // Самый отклонённый правый стик среди геймпадов
    float dx = 0.f, dy = 0.f;
    for (const Device &dev : std::as_const(m_devices)) {
        if (std::hypot(dev.rstick[0], dev.rstick[1]) > std::hypot(dx, dy)) {
            dx = dev.rstick[0];
            dy = dev.rstick[1];
        }
    }

    QWindow *window = targetWindow();
    if (!window)
        return;
    if (window != m_pointerWindow) {
        // Другое окно — курсор появляется в его центре
        m_pointerWindow = window;
        m_pointerPos = QPointF(window->width() / 2.0, window->height() / 2.0);
    }

    const float magnitude = std::hypot(dx, dy);
    const qint64 elapsed = m_pointerClock.restart();
    if (magnitude <= PointerDeadzone) {
        m_pointerTimer.stop();
    } else {
        // Плавное ускорение: малые отклонения — точное наведение, полные — быстро через экран
        const float t = (magnitude - PointerDeadzone) / (1.f - PointerDeadzone);
        const float speed = PointerSpeed * t * t * float(window->height()) / 1080.f;
        const float dt = float(qMin<qint64>(elapsed, 50)) / 1000.f;
        m_pointerPos.rx() = qBound(0.0, m_pointerPos.x() + double(dx / magnitude * speed * dt), double(window->width() - 1));
        m_pointerPos.ry() = qBound(0.0, m_pointerPos.y() + double(dy / magnitude * speed * dt), double(window->height() - 1));
    }
    m_pointerVisible = true;
    m_pointerHide.start();
    sendMouse(QEvent::MouseMove, Qt::NoButton);
    emit pointerChanged();
}

void GamepadManager::hidePointer()
{
    if (!m_pointerVisible)
        return;
    m_pointerVisible = false;
    m_pointerTimer.stop();
    emit pointerChanged();
}

void GamepadManager::sendMouse(QEvent::Type type, Qt::MouseButton button)
{
    if (!m_pointerWindow)
        return;
    const Qt::MouseButtons buttons = type == QEvent::MouseButtonPress ? Qt::MouseButtons(button) : Qt::NoButton;
    QCoreApplication::postEvent(m_pointerWindow, new QMouseEvent(type, m_pointerPos, m_pointerWindow->mapToGlobal(m_pointerPos),
                                                                 button, buttons, Qt::NoModifier));
}
