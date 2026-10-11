import QtQuick

// Пустое состояние раздела: иконка, заголовок и пояснение по центру.
Column {
    id: root

    property string icon
    property string title
    property string text

    spacing: Theme.px(18)
    width: Theme.px(820)

    // Иконка в круглом «стеклянном» значке с тонким внешним кольцом,
    // чтобы не терялась на живом фоне
    Item {
        visible: root.icon !== ""
        anchors.horizontalCenter: parent.horizontalCenter
        width: Theme.px(156)
        height: width

        Rectangle {
            anchors.fill: parent
            radius: width / 2
            color: "transparent"
            border.width: 1
            border.color: Qt.rgba(1, 1, 1, 0.10)
        }
        Rectangle {
            anchors.centerIn: parent
            width: Theme.px(128)
            height: width
            radius: width / 2
            border.width: 1
            border.color: Qt.rgba(1, 1, 1, 0.16)
            gradient: Gradient {
                GradientStop { position: 0.0; color: Qt.rgba(0.16, 0.17, 0.19, 0.85) }
                GradientStop { position: 1.0; color: Qt.rgba(0.06, 0.065, 0.075, 0.85) }
            }

            Icon {
                anchors.centerIn: parent
                name: root.icon
                size: Theme.px(56)
                lineWidth: 1.3
                color: Theme.textPrimary
            }
        }
    }
    Item { width: 1; height: Theme.px(6); visible: root.icon !== "" }
    Text {
        width: parent.width
        horizontalAlignment: Text.AlignHCenter
        text: root.title
        color: Theme.textPrimary
        font.family: Theme.fontFamily
        font.pixelSize: Theme.fontLarge
        font.weight: Font.Normal
        wrapMode: Text.WordWrap
    }
    Text {
        visible: text !== ""
        width: parent.width
        horizontalAlignment: Text.AlignHCenter
        text: root.text
        color: Theme.textSecondary
        font.family: Theme.fontFamily
        font.pixelSize: Theme.fontBody
        lineHeight: 1.25
        wrapMode: Text.WordWrap
    }
}
