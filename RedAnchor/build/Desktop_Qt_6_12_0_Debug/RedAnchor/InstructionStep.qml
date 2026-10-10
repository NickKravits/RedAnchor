import QtQuick

// Пункт пошаговой инструкции: номер (или галочка) и текст.
Row {
    id: root

    property int number: 1
    property string text
    property string detail
    // "pending" | "active" | "done"
    property string status: "pending"

    spacing: Theme.px(22)
    opacity: status === "pending" ? 0.45 : 1
    Behavior on opacity { NumberAnimation { duration: Theme.animNormal } }

    Rectangle {
        width: Theme.px(46)
        height: width
        radius: width / 2
        color: root.status === "done" ? Theme.success : root.status === "active" ? Theme.accent : "transparent"
        border.width: root.status === "pending" ? Theme.px(2) : 0
        border.color: Theme.textMuted
        Behavior on color { ColorAnimation { duration: Theme.animNormal } }

        Text {
            anchors.centerIn: parent
            visible: root.status !== "done"
            text: root.number
            color: Theme.textPrimary
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontBody
            font.weight: Font.Bold
        }
        Icon {
            anchors.centerIn: parent
            visible: root.status === "done"
            name: "check"
            color: Theme.background
            lineWidth: 2.6
        }
    }

    Column {
        anchors.verticalCenter: parent.verticalCenter
        spacing: Theme.px(4)
        width: Theme.px(560)
        Text {
            width: parent.width
            text: root.text
            color: Theme.textPrimary
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontBody
            font.weight: root.status === "active" ? Font.DemiBold : Font.Normal
            wrapMode: Text.WordWrap
        }
        Text {
            width: parent.width
            visible: text !== ""
            text: root.detail
            color: Theme.textSecondary
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontSmall
            wrapMode: Text.WordWrap
        }
    }
}
