import QtQuick

// Пункт пошаговой инструкции: тонкий номер «01», текст и пояснение.
Row {
    id: root

    property int number: 1
    property string text
    property string detail
    // "pending" | "active" | "done"
    property string status: "pending"

    spacing: Theme.px(24)
    opacity: status === "pending" ? 0.38 : 1
    Behavior on opacity { NumberAnimation { duration: Theme.animNormal } }

    Item {
        width: Theme.px(44)
        height: Theme.px(30)

        Text {
            anchors.verticalCenter: parent.verticalCenter
            visible: root.status !== "done"
            text: (root.number < 10 ? "0" : "") + root.number
            color: root.status === "active" ? Theme.textPrimary : Theme.textMuted
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontSmall
            font.weight: Font.Medium
            font.features: { "tnum": 1 }
        }
        Icon {
            anchors.verticalCenter: parent.verticalCenter
            visible: root.status === "done"
            name: "check"
            size: Theme.px(24)
            lineWidth: 1.6
            color: Theme.success
        }
    }

    Column {
        spacing: Theme.px(6)
        width: Theme.px(560)

        Text {
            width: parent.width
            text: root.text
            color: Theme.textPrimary
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontBody
            wrapMode: Text.WordWrap
        }
        Text {
            width: parent.width
            visible: text !== ""
            text: root.detail
            color: Theme.textSecondary
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontSmall
            lineHeight: 1.2
            wrapMode: Text.WordWrap
        }
        // Индикатор текущего шага
        Rectangle {
            width: root.status === "active" ? Theme.px(56) : 0
            height: Theme.px(2)
            color: Theme.accent
            Behavior on width { NumberAnimation { duration: Theme.animSlow; easing.type: Easing.OutCubic } }
        }
    }
}
