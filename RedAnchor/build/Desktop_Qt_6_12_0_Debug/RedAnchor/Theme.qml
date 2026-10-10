pragma Singleton
import QtQuick

// Общая палитра и метрики оболочки. Все размеры задаются для 1080p
// и умножаются на scale, который Main.qml выставляет по высоте окна.
QtObject {
    property real scale: 1.0
    function px(v) { return Math.round(v * scale) }

    // Палитра
    readonly property color background: "#07080c"
    readonly property color backgroundAlt: "#10121a"
    readonly property color surface: "#161925"
    readonly property color surfaceRaised: "#1f2333"
    readonly property color outline: "#2c3146"
    readonly property color accent: "#e3262f"        // фирменный красный RedAnchor
    readonly property color accentSoft: "#ff5a61"
    readonly property color focus: "#ffffff"
    readonly property color textPrimary: "#f2f3f7"
    readonly property color textSecondary: "#9aa0b4"
    readonly property color textMuted: "#5d6378"
    readonly property color success: "#3ddc84"
    readonly property color warning: "#ffb020"
    readonly property color lightbarBlue: "#3d8bff"

    // Типографика
    readonly property string fontFamily: "Inter"   // при отсутствии fontconfig подставит системный sans
    readonly property int fontHuge: px(64)
    readonly property int fontTitle: px(46)
    readonly property int fontLarge: px(32)
    readonly property int fontBody: px(24)
    readonly property int fontSmall: px(19)

    // Метрики
    readonly property int radius: px(14)
    readonly property int gutter: px(96)
    readonly property int animFast: 140
    readonly property int animNormal: 260
    readonly property int animSlow: 520
}
