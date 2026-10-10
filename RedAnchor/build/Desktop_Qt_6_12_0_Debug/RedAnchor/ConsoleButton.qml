import QtQuick

// Кнопка в стиле консоли: управляется фокусом (стрелки/D-pad) и ✕/Enter.
FocusScope {
    id: root

    property string text
    property string icon: ""
    property bool primary: false
    property bool enabledState: true
    readonly property bool highlighted: activeFocus || mouse.containsMouse

    signal clicked()

    implicitWidth: Math.max(Theme.px(220), row.implicitWidth + Theme.px(64))
    implicitHeight: Theme.px(68)
    opacity: enabledState ? 1 : 0.4
    scale: activeFocus ? 1.04 : 1.0
    Behavior on scale { NumberAnimation { duration: Theme.animFast; easing.type: Easing.OutCubic } }

    Keys.onReturnPressed: if (enabledState) clicked()
    Keys.onEnterPressed: if (enabledState) clicked()

    Rectangle {
        id: bg
        anchors.fill: parent
        radius: Theme.radius
        color: root.primary ? (root.activeFocus ? Theme.accentSoft : Theme.accent)
                            : (root.activeFocus ? Theme.textPrimary : Theme.surfaceRaised)
        border.width: root.activeFocus ? 0 : Math.max(1, Theme.px(1.5))
        border.color: Theme.outline
        Behavior on color { ColorAnimation { duration: Theme.animFast } }
    }

    // Внешнее свечение фокуса
    Rectangle {
        anchors.fill: parent
        anchors.margins: -Theme.px(6)
        radius: Theme.radius + Theme.px(6)
        color: "transparent"
        border.width: Theme.px(3)
        border.color: root.primary ? Theme.textPrimary : Theme.accent
        opacity: root.activeFocus ? 1 : 0
        Behavior on opacity { NumberAnimation { duration: Theme.animFast } }
    }

    Row {
        id: row
        anchors.centerIn: parent
        spacing: Theme.px(12)

        Icon {
            visible: root.icon !== ""
            name: root.icon
            anchors.verticalCenter: parent.verticalCenter
            color: label.color
        }
        Text {
            id: label
            anchors.verticalCenter: parent.verticalCenter
            text: root.text
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontBody
            font.weight: Font.DemiBold
            color: root.primary ? Theme.textPrimary : (root.activeFocus ? Theme.background : Theme.textPrimary)
        }
    }

    MouseArea {
        id: mouse
        anchors.fill: parent
        hoverEnabled: true
        cursorShape: Qt.PointingHandCursor
        onClicked: {
            root.forceActiveFocus()
            if (root.enabledState)
                root.clicked()
        }
    }
}
