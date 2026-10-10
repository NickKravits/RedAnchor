import QtQuick

// Уведомление сверху по центру: иконка, заголовок и пояснение.
// Выезжает сверху и само скрывается; новое уведомление заменяет текущее.
//   toast.show("controller", "DualSense подключён", "Bluetooth")
Item {
    id: root

    property int duration: 3000
    property string icon
    property string title
    property string subtitle
    property bool shown: false

    function show(icon, title, subtitle) {
        root.icon = icon
        root.title = title
        root.subtitle = subtitle || ""
        root.shown = true
        hideTimer.restart()
    }

    width: card.width
    height: card.height

    Timer {
        id: hideTimer
        interval: root.duration
        onTriggered: root.shown = false
    }

    Rectangle {
        id: card
        width: row.implicitWidth + Theme.px(56)
        height: Theme.px(68)
        radius: height / 2
        color: Qt.rgba(0.03, 0.05, 0.13, 0.92)
        border.width: 1
        border.color: Qt.rgba(1, 1, 1, 0.16)

        y: root.shown ? 0 : -height - Theme.px(40)
        opacity: root.shown ? 1 : 0
        Behavior on y { NumberAnimation { duration: Theme.animSlow; easing.type: Easing.OutCubic } }
        Behavior on opacity { NumberAnimation { duration: Theme.animNormal } }

        Row {
            id: row
            anchors.centerIn: parent
            spacing: Theme.px(16)

            Icon {
                anchors.verticalCenter: parent.verticalCenter
                name: root.icon
                size: Theme.px(28)
                lineWidth: 1.5
                color: Theme.textPrimary
            }
            Column {
                anchors.verticalCenter: parent.verticalCenter
                spacing: Theme.px(2)
                Text {
                    text: root.title
                    color: Theme.textPrimary
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontSmall
                    font.weight: Font.Medium
                }
                Text {
                    visible: text !== ""
                    text: root.subtitle
                    color: Theme.textSecondary
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontCaption
                }
            }
        }
    }
}
