import QtQuick

// Кнопка в стиле консоли: управляется фокусом (стрелки/D-pad) и ✕/Enter.
// Форма — «пилюля». В фокусе — светлая заливка, кольцо с мягким ореолом и лёгкое
// увеличение; без фокуса — полупрозрачная «стеклянная» плашка (основная — светлее).
FocusScope {
    id: root

    property string text
    property string icon: ""
    property bool primary: false
    property bool enabledState: true

    signal clicked()

    implicitWidth: Math.max(Theme.px(200), row.implicitWidth + Theme.px(72))
    implicitHeight: Theme.controlHeight
    opacity: enabledState ? 1 : 0.38
    scale: activeFocus ? Theme.focusScale : 1.0
    Behavior on scale { NumberAnimation { duration: Theme.animFast; easing.type: Easing.OutCubic } }

    Keys.onReturnPressed: if (enabledState) clicked()
    Keys.onEnterPressed: if (enabledState) clicked()

    FocusRing {
        shown: root.activeFocus
        radius: background.radius
    }

    Rectangle {
        id: background
        anchors.fill: parent
        radius: height / 2
        color: root.activeFocus ? Theme.focus
             : root.primary ? Qt.rgba(1, 1, 1, 0.16) : Theme.glass
        border.width: root.activeFocus ? 0 : 1
        border.color: root.primary ? Theme.outlineStrong : Theme.glassBorder
        antialiasing: true
        Behavior on color { ColorAnimation { duration: Theme.animFast } }

        // Блик сверху — только у «стеклянной» плашки
        Rectangle {
            visible: !root.activeFocus
            anchors.fill: parent
            anchors.margins: 1
            radius: height / 2
            gradient: Gradient {
                GradientStop { position: 0.0; color: Theme.glassHighlight }
                GradientStop { position: 0.6; color: "transparent" }
            }
        }
    }

    Row {
        id: row
        anchors.centerIn: parent
        spacing: Theme.px(12)

        Icon {
            visible: root.icon !== ""
            name: root.icon
            size: Theme.px(24)
            lineWidth: 1.8
            anchors.verticalCenter: parent.verticalCenter
            color: label.color
        }
        Text {
            id: label
            anchors.verticalCenter: parent.verticalCenter
            text: root.text
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontBody
            font.weight: root.activeFocus || root.primary ? Font.DemiBold : Font.Medium
            color: root.activeFocus ? Theme.textOnFocus : Theme.textPrimary
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
