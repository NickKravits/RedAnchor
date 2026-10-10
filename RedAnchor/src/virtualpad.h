#pragma once

#include <QString>

// Виртуальный контроллер Xbox 360 (uinput) — система раскладки RedAnchor.
//
// Для игр без официальной поддержки DualSense: игра видит обычный проводной контроллер
// Xbox 360 (045e:028e, как его показывает драйвер xpad), а GamepadManager пересылает в него
// нажатия настоящего геймпада. Кнопка PS не пересылается — она открывает оверлей оболочки.
//
// Нужен доступ к /dev/uinput (правило udev оболочки выдаёт его активной сессии).
class VirtualPad
{
public:
    // Метка устройства (phys), по ней GamepadManager не принимает его за настоящий геймпад
    static constexpr const char *Phys = "redanchor-virtual-pad";

    VirtualPad() = default;
    ~VirtualPad();
    VirtualPad(const VirtualPad &) = delete;
    VirtualPad &operator=(const VirtualPad &) = delete;

    bool create(QString *error = nullptr);
    void destroy();
    bool isActive() const { return m_fd >= 0; }

    // Кнопка в кодах Xbox (BTN_A, BTN_B, BTN_X, BTN_Y, BTN_TL, BTN_TR, BTN_SELECT, BTN_START,
    // BTN_THUMBL, BTN_THUMBR)
    void button(int code, bool pressed);
    // Стики: -1..1 (ABS_X, ABS_Y, ABS_RX, ABS_RY); курки: 0..1 (ABS_Z, ABS_RZ); крестовина — hat
    void stick(int code, float value);
    void trigger(int code, float value);
    void hat(int x, int y);
    void sync();
    // Всё отпустить (оверлей открыт — игра не должна получать нажатия)
    void reset();

private:
    void emitEvent(int type, int code, int value);

    int m_fd = -1;
};
