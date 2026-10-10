.pragma library

// Поддержка геймпада игрой (по данным StoreManager.compatibility) для текущего геймпада.
// level: "checking" | "native" | "remap" | "keyboard" | "unknown"
//   native   — игра официально поддерживает наш геймпад;
//   remap    — поддерживает другой (Xbox, DualShock 4): будет система раскладки RedAnchor;
//   keyboard — только мышь и клавиатура.
function describe(info, dualSense) {
    if (!info || !info.checked)
        return { level: "checking", text: "Проверяем поддержку геймпада…" }
    if (info.failed)
        return { level: "unknown", text: "Не удалось проверить поддержку геймпада." }
    if (!info.controller)
        return { level: "keyboard",
                 text: "Игра рассчитана только на мышь и клавиатуру. Установить её можно, но играть на геймпаде не получится." }
    if (!dualSense || info.ps5)
        return { level: "native", text: dualSense ? "Игра поддерживает DualSense." : "Игра поддерживает геймпад." }
    return { level: "remap",
             text: (info.ps4 ? "Игра официально поддерживает DualShock 4, но не DualSense. "
                             : "Игра официально поддерживает контроллер Xbox, но не DualSense. ")
                   + "Будет использована система раскладки RedAnchor: управление может быть неточным, "
                   + "а значки кнопок — отображаться некорректно." }
}
