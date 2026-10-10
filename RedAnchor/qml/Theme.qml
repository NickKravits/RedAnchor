pragma Singleton
import QtQuick

// Общая палитра и метрики оболочки. Все размеры задаются для 1080p
// и умножаются на scale, который Main.qml выставляет по размеру окна.
// Стиль: монохром, тонкие линии, акцентный красный — только точечно.
QtObject {
    property real scale: 1.0
    function px(v) { return Math.round(v * scale) }

    // Палитра
    readonly property color background: "#08090b"
    readonly property color backgroundAlt: "#0f1114"
    readonly property color surface: Qt.rgba(1, 1, 1, 0.04)
    readonly property color surfaceRaised: Qt.rgba(1, 1, 1, 0.08)
    readonly property color outline: Qt.rgba(1, 1, 1, 0.10)
    readonly property color outlineStrong: Qt.rgba(1, 1, 1, 0.24)
    readonly property color accent: "#e5383b"
    readonly property color focus: "#f4f5f7"
    readonly property color textPrimary: "#f4f5f7"
    readonly property color textSecondary: "#a1a5b0"
    readonly property color textMuted: "#5f636c"
    readonly property color success: "#5ad19a"
    readonly property color warning: "#f0b44c"

    // Иллюстрации в стиле технического чертежа (белые линии)
    readonly property color sketchLine: "#e9ebef"
    readonly property color sketchDim: Qt.rgba(0.91, 0.92, 0.94, 0.32)
    readonly property color sketchFaint: Qt.rgba(0.91, 0.92, 0.94, 0.12)
    readonly property color lightbarBlue: "#4c9bff"

    // Типографика
    readonly property string fontFamily: "Inter"   // при отсутствии fontconfig подставит системный sans
    readonly property int fontDisplay: px(88)
    readonly property int fontHuge: px(64)
    readonly property int fontTitle: px(52)
    readonly property int fontLarge: px(30)
    readonly property int fontBody: px(23)
    readonly property int fontSmall: px(18)
    readonly property int fontCaption: px(14)

    // Метрики
    readonly property int radius: px(12)
    readonly property int gutter: px(112)
    readonly property int animFast: 140
    readonly property int animNormal: 280
    readonly property int animSlow: 600
}
