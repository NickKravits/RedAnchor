import QtQuick

// Строка настроек с переключателем справа. ✕ — переключить.
ConsoleListItem {
    id: root

    property bool checked: false

    signal toggled()

    onClicked: toggled()

    Rectangle {
        id: track
        anchors.verticalCenter: parent.verticalCenter
        width: Theme.px(66)
        height: Theme.px(36)
        radius: height / 2
        color: root.checked ? Theme.focus : Qt.rgba(0, 0, 0, 0.25)
        border.width: root.checked ? 0 : 1
        border.color: Theme.outlineStrong
        antialiasing: true
        Behavior on color { ColorAnimation { duration: Theme.animFast } }

        Rectangle {
            width: track.height - Theme.px(8)
            height: width
            radius: width / 2
            anchors.verticalCenter: parent.verticalCenter
            x: root.checked ? track.width - width - Theme.px(4) : Theme.px(4)
            color: root.checked ? Theme.textOnFocus : Theme.textSecondary
            antialiasing: true
            Behavior on x { NumberAnimation { duration: Theme.animNormal; easing.type: Easing.OutBack; easing.overshoot: 1.2 } }
            Behavior on color { ColorAnimation { duration: Theme.animFast } }
        }
    }
}
