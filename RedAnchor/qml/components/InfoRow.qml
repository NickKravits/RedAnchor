import QtQuick

// Строка «название — значение» (раздел «О системе»). Снизу — тонкий разделитель.
Item {
    id: root

    property string label
    property string value

    implicitWidth: Theme.px(900)
    implicitHeight: Math.max(labelText.implicitHeight, valueText.implicitHeight) + Theme.px(20)

    Text {
        id: labelText
        width: Theme.px(300)
        text: root.label
        color: Theme.textSecondary
        font.family: Theme.fontFamily
        font.pixelSize: Theme.fontBody
    }
    Text {
        id: valueText
        x: Theme.px(324)
        width: Math.max(0, root.width - x)
        text: root.value !== "" ? root.value : "—"
        color: Theme.textPrimary
        font.family: Theme.fontFamily
        font.pixelSize: Theme.fontBody
        font.weight: Font.Medium
        wrapMode: Text.WrapAnywhere
    }
    Rectangle {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        height: 1
        color: Theme.outline
    }
}
