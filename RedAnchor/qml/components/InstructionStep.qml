import QtQuick

// Пункт пошаговой инструкции: номер в круге, текст и пояснение.
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

    // Номер шага в круглой плашке; пройденный шаг — галочка
    Rectangle {
        width: Theme.px(44)
        height: width
        radius: width / 2
        color: root.status === "active" ? Qt.rgba(1, 1, 1, 0.12)
             : root.status === "done" ? Qt.rgba(0.35, 0.82, 0.6, 0.12) : "transparent"
        border.width: root.status === "active" ? Math.max(1, Theme.px(2)) : 1
        border.color: root.status === "active" ? Theme.focus
                    : root.status === "done" ? Qt.rgba(0.35, 0.82, 0.6, 0.5) : Theme.outlineStrong
        antialiasing: true
        Behavior on color { ColorAnimation { duration: Theme.animNormal } }

        Text {
            anchors.centerIn: parent
            visible: root.status !== "done"
            text: root.number
            color: root.status === "active" ? Theme.textPrimary : Theme.textSecondary
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontSmall
            font.weight: Font.DemiBold
            font.features: { "tnum": 1 }
        }
        Icon {
            anchors.centerIn: parent
            visible: root.status === "done"
            name: "check"
            size: Theme.px(22)
            lineWidth: 2
            color: Theme.success
        }
    }

    Column {
        spacing: Theme.px(6)
        width: Theme.px(560)
        topPadding: Theme.px(8)

        Text {
            width: parent.width
            text: root.text
            color: Theme.textPrimary
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontBody
            font.weight: root.status === "active" ? Font.Medium : Font.Normal
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
            height: Math.max(2, Theme.px(3))
            radius: height / 2
            color: Theme.accent
            Behavior on width { NumberAnimation { duration: Theme.animSlow; easing.type: Easing.OutCubic } }
        }
    }
}
