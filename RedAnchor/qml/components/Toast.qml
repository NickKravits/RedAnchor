import QtQuick

// Уведомление сверху по центру: иконка, заголовок и пояснение.
// Выезжает сверху и само скрывается; новое уведомление заменяет текущее.
// Плотное «стекло» с тенью — читается поверх любой картинки и поверх игры.
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

    GlassPanel {
        id: card
        strong: true
        width: row.implicitWidth + Theme.px(44)
        height: Theme.px(76)
        radius: height / 2

        y: root.shown ? 0 : -height - Theme.px(48)
        opacity: root.shown ? 1 : 0
        scale: root.shown ? 1 : 0.94
        Behavior on y { NumberAnimation { duration: Theme.animSlow; easing.type: Easing.OutQuint } }
        Behavior on opacity { NumberAnimation { duration: Theme.animNormal } }
        Behavior on scale { NumberAnimation { duration: Theme.animSlow; easing.type: Easing.OutQuint } }

        Row {
            id: row
            anchors.verticalCenter: parent.verticalCenter
            x: Theme.px(14)
            spacing: Theme.px(16)

            // Значок в светлом круге
            Rectangle {
                anchors.verticalCenter: parent.verticalCenter
                width: Theme.px(50)
                height: width
                radius: width / 2
                color: Qt.rgba(1, 1, 1, 0.10)
                border.width: 1
                border.color: Qt.rgba(1, 1, 1, 0.16)
                Icon {
                    anchors.centerIn: parent
                    name: root.icon
                    size: Theme.px(26)
                    lineWidth: 1.6
                    color: Theme.textPrimary
                }
            }
            Column {
                anchors.verticalCenter: parent.verticalCenter
                spacing: Theme.px(2)
                Text {
                    text: root.title
                    color: Theme.textPrimary
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontSmall
                    font.weight: Font.DemiBold
                }
                Text {
                    visible: text !== ""
                    text: root.subtitle
                    color: Theme.textSecondary
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontCaption + Theme.px(1)
                }
            }
            Item { width: Theme.px(8); height: 1 }
        }
    }
}
