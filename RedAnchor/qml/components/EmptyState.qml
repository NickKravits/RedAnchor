import QtQuick

// Пустое состояние раздела: иконка, заголовок и пояснение по центру.
Column {
    id: root

    property string icon
    property string title
    property string text

    spacing: Theme.px(18)
    width: Theme.px(820)

    // Иконка в круглом «стеклянном» значке, чтобы не терялась на живом фоне
    Rectangle {
        visible: root.icon !== ""
        anchors.horizontalCenter: parent.horizontalCenter
        width: Theme.px(132)
        height: width
        radius: width / 2
        color: Qt.rgba(0.03, 0.05, 0.12, 0.7)
        border.width: 1
        border.color: Qt.rgba(1, 1, 1, 0.18)

        Icon {
            anchors.centerIn: parent
            name: root.icon
            size: Theme.px(60)
            lineWidth: 1.2
            color: Theme.textPrimary
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
        font.weight: Font.Light
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
