pragma ComponentBehavior: Bound
import QtQuick

// Подсказки по управлению внизу экрана: [✕] Выбрать  [○] Назад …
// model: [{ button: "cross", text: "Выбрать" }, …]
Row {
    id: root

    property var model: []

    spacing: Theme.px(36)
    layoutDirection: Qt.RightToLeft

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
                color: Theme.textSecondary
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontSmall
            }
        }
    }
}
