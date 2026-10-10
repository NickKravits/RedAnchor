#include "virtualpad.h"

#include <QtGlobal>

#include <algorithm>
#include <cerrno>
#include <cmath>
#include <cstring>
#include <fcntl.h>
#include <linux/uinput.h>
#include <sys/ioctl.h>
#include <unistd.h>

namespace {

const int kButtons[] = {BTN_A, BTN_B, BTN_X, BTN_Y, BTN_TL, BTN_TR, BTN_SELECT, BTN_START,
                        BTN_MODE, BTN_THUMBL, BTN_THUMBR};

void setupAbs(int fd, int code, int min, int max, int fuzz, int flat)
{
    uinput_abs_setup abs = {};
    abs.code = quint16(code);
    abs.absinfo.minimum = min;
    abs.absinfo.maximum = max;
    abs.absinfo.fuzz = fuzz;
    abs.absinfo.flat = flat;
    ioctl(fd, UI_ABS_SETUP, &abs);
}

} // namespace

VirtualPad::~VirtualPad()
{
    destroy();
}

bool VirtualPad::create(QString *error)
{
    if (m_fd >= 0)
        return true;
    const int fd = ::open("/dev/uinput", O_WRONLY | O_NONBLOCK | O_CLOEXEC);
    if (fd < 0) {
        if (error)
            *error = QString::fromLocal8Bit(std::strerror(errno));
        return false;
    }

    ioctl(fd, UI_SET_EVBIT, EV_KEY);
    for (int code : kButtons)
        ioctl(fd, UI_SET_KEYBIT, code);
    ioctl(fd, UI_SET_EVBIT, EV_ABS);
    for (int code : {ABS_X, ABS_Y, ABS_RX, ABS_RY}) {
        ioctl(fd, UI_SET_ABSBIT, code);
        setupAbs(fd, code, -32768, 32767, 16, 128);
    }
    for (int code : {ABS_Z, ABS_RZ}) {
        ioctl(fd, UI_SET_ABSBIT, code);
        setupAbs(fd, code, 0, 255, 0, 0);
    }
    for (int code : {ABS_HAT0X, ABS_HAT0Y}) {
        ioctl(fd, UI_SET_ABSBIT, code);
        setupAbs(fd, code, -1, 1, 0, 0);
    }
    ioctl(fd, UI_SET_PHYS, VirtualPad::Phys);

    // Так контроллер Xbox 360 показывает драйвер xpad: по этим кодам его узнают SDL и Wine
    uinput_setup setup = {};
    setup.id.bustype = BUS_USB;
    setup.id.vendor = 0x045e;
    setup.id.product = 0x028e;
    setup.id.version = 0x0114;
    std::strncpy(setup.name, "Microsoft X-Box 360 pad", UINPUT_MAX_NAME_SIZE - 1);
    if (ioctl(fd, UI_DEV_SETUP, &setup) < 0 || ioctl(fd, UI_DEV_CREATE) < 0) {
        if (error)
            *error = QString::fromLocal8Bit(std::strerror(errno));
        ::close(fd);
        return false;
    }
    m_fd = fd;
    return true;
}

void VirtualPad::destroy()
{
    if (m_fd < 0)
        return;
    ioctl(m_fd, UI_DEV_DESTROY);
    ::close(m_fd);
    m_fd = -1;
}

void VirtualPad::emitEvent(int type, int code, int value)
{
    if (m_fd < 0)
        return;
    input_event ev = {};
    ev.type = quint16(type);
    ev.code = quint16(code);
    ev.value = value;
    if (::write(m_fd, &ev, sizeof(ev)) < 0) {
        // Очередь переполнена — событие теряется, следующее состояние всё равно придёт
    }
}

void VirtualPad::button(int code, bool pressed)
{
    emitEvent(EV_KEY, code, pressed ? 1 : 0);
}

void VirtualPad::stick(int code, float value)
{
    const float v = std::clamp(value, -1.f, 1.f);
    emitEvent(EV_ABS, code, int(std::lround(v >= 0 ? v * 32767.f : v * 32768.f)));
}

void VirtualPad::trigger(int code, float value)
{
    emitEvent(EV_ABS, code, int(std::lround(std::clamp(value, 0.f, 1.f) * 255.f)));
}

void VirtualPad::hat(int x, int y)
{
    emitEvent(EV_ABS, ABS_HAT0X, std::clamp(x, -1, 1));
    emitEvent(EV_ABS, ABS_HAT0Y, std::clamp(y, -1, 1));
}

void VirtualPad::sync()
{
    emitEvent(EV_SYN, SYN_REPORT, 0);
}

void VirtualPad::reset()
{
    for (int code : kButtons)
        emitEvent(EV_KEY, code, 0);
    for (int code : {ABS_X, ABS_Y, ABS_RX, ABS_RY, ABS_Z, ABS_RZ, ABS_HAT0X, ABS_HAT0Y})
        emitEvent(EV_ABS, code, 0);
    sync();
}
