import QtQuick

// Кнопка в стиле консоли: управляется фокусом (стрелки/D-pad) и ✕/Enter.
// В фокусе — светлая заливка, без фокуса — полупрозрачная «стеклянная» плашка.
FocusScope {
    id: root

    property string text
    property string icon: ""
    property bool primary: false
    property bool enabledState: true

    signal clicked()

    implicitWidth: Math.max(Theme.px(200), row.implicitWidth + Theme.px(64))
    implicitHeight: Theme.px(64)
    opacity: enabledState ? 1 : 0.35
    scale: activeFocus ? 1.02 : 1.0
    Behavior on scale { NumberAnimation { duration: Theme.animFast; easing.type: Easing.OutCubic } }

    Keys.onReturnPressed: if (enabledState) clicked()
    Keys.onEnterPressed: if (enabledState) clicked()

    // Мягкий ореол фокуса
    Rectangle {
        anchors.fill: parent
        anchors.margins: -Theme.px(5)
        radius: Theme.radius + Theme.px(5)
        color: "transparent"
        border.width: Theme.px(1.5)
        border.color: Qt.rgba(1, 1, 1, 0.22)
        opacity: root.activeFocus ? 1 : 0
        Behavior on opacity { NumberAnimation { duration: Theme.animFast } }
    }

    Rectangle {
        anchors.fill: parent
        radius: Theme.radius
        color: root.activeFocus ? Theme.focus : (root.primary ? Qt.rgba(1, 1, 1, 0.14) : Theme.surface)
        border.width: root.activeFocus ? 0 : 1
        border.color: root.primary ? Theme.outlineStrong : Theme.outline
        Behavior on color { ColorAnimation { duration: Theme.animFast } }
    }

    Row {
        id: row
        anchors.centerIn: parent
        spacing: Theme.px(12)

        Icon {
            visible: root.icon !== ""
            name: root.icon
            size: Theme.px(24)
            anchors.verticalCenter: parent.verticalCenter
            color: label.color
        }
        Text {
            id: label
            anchors.verticalCenter: parent.verticalCenter
            text: root.text
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontBody
            font.weight: Font.Medium
            color: root.activeFocus ? Theme.background : Theme.textPrimary
        }
    }

    MouseArea {
        anchors.fill: parent
        cursorShape: Qt.PointingHandCursor
        onClicked: {
            root.forceActiveFocus()
            if (root.enabledState)
                root.clicked()
        }
    }
}
