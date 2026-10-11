pragma ComponentBehavior: Bound
import QtQuick

// Кольцо фокуса: белая обводка снаружи элемента с небольшим зазором и мягкий
// ореол вокруг неё. Кладётся внутрь элемента (заполняет родителя и выходит за края):
//   FocusRing { shown: root.activeFocus; radius: Theme.radius }
// Ореол — несколько полупрозрачных колец, без эффектов: дёшево и работает
// даже при программной отрисовке.
Item {
    id: root

    property bool shown: false
    // Скругление самого элемента; кольцо скругляется соответственно
    property real radius: Theme.radius
    property real gap: Theme.focusRingGap
    property real ringWidth: Theme.focusRingWidth
    property color color: Theme.focus
    property bool glow: true

    readonly property real outset: gap + ringWidth

    anchors.fill: parent
    anchors.margins: -outset
    opacity: shown ? 1 : 0
    visible: opacity > 0
    Behavior on opacity { NumberAnimation { duration: Theme.animFast; easing.type: Easing.OutCubic } }

    // Ореол: кольца расходятся наружу и тают
    Repeater {
        model: root.glow ? 3 : 0
        delegate: Rectangle {
            required property int index
            readonly property real spread: (index + 1) * Theme.px(4)
            anchors.fill: parent
            anchors.margins: -spread
            radius: root.radius + root.outset + spread
            color: "transparent"
            border.width: Theme.px(4)
            border.color: Qt.rgba(1, 1, 1, 0.11 - index * 0.035)
            antialiasing: true
        }
    }

    Rectangle {
        anchors.fill: parent
        radius: root.radius + root.outset
        color: "transparent"
        border.width: root.ringWidth
        border.color: root.color
        antialiasing: true
    }
}
