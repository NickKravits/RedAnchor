pragma ComponentBehavior: Bound
import QtQuick

// Подсказки по управлению внизу экрана: [✕] Выбрать  [○] Назад …
// model: [{ button: "cross", text: "Выбрать" }, …]
// Подсказки сменяются мягким появлением, чтобы смена экрана не «мигала».
Row {
    id: root

    property var model: []

    spacing: Theme.px(36)
    layoutDirection: Qt.RightToLeft

    onModelChanged: appear.restart()
    NumberAnimation { id: appear; target: root; property: "opacity"; from: 0.4; to: 1; duration: Theme.animNormal; easing.type: Easing.OutCubic }

    Repeater {
        model: root.model
        delegate: Row {
            id: hint
            required property var modelData
            spacing: Theme.px(12)
            layoutDirection: Qt.LeftToRight

            ButtonGlyph {
                button: hint.modelData.button
                anchors.verticalCenter: parent.verticalCenter
            }
            Text {
                anchors.verticalCenter: parent.verticalCenter
                text: hint.modelData.text
                color: Theme.textPrimary
                opacity: 0.82
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontSmall
                font.weight: Font.Medium
            }
        }
    }
}
