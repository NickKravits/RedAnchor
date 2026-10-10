import QtQuick

// Строка «название — значение» (раздел «О системе»).
Row {
    id: root

    property string label
    property string value

    spacing: Theme.px(24)

    Text {
        width: Theme.px(300)
        text: root.label
        color: Theme.textSecondary
        font.family: Theme.fontFamily
        font.pixelSize: Theme.fontBody
    }
    Text {
        width: Math.max(0, root.width - Theme.px(324))
        text: root.value !== "" ? root.value : "—"
        color: Theme.textPrimary
        font.family: Theme.fontFamily
        font.pixelSize: Theme.fontBody
        wrapMode: Text.WrapAnywhere
    }
}
