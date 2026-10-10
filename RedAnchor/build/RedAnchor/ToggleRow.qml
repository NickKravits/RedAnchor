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
        width: Theme.px(64)
        height: Theme.px(34)
        radius: height / 2
        color: root.checked ? Theme.focus : Theme.surfaceRaised
        border.width: root.checked ? 0 : 1
        border.color: Theme.outlineStrong
        Behavior on color { ColorAnimation { duration: Theme.animFast } }

        Rectangle {
            width: track.height - Theme.px(8)
            height: width
            radius: width / 2
            anchors.verticalCenter: parent.verticalCenter
            x: root.checked ? track.width - width - Theme.px(4) : Theme.px(4)
            color: root.checked ? Theme.background : Theme.textSecondary
            Behavior on x { NumberAnimation { duration: Theme.animFast; easing.type: Easing.OutCubic } }
        }
    }
}
