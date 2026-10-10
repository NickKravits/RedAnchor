import QtQuick

// Уровень сигнала Wi-Fi: 4 столбика.
Row {
    id: root

    property int strength: 0          // 0..100
    property color color: Theme.textPrimary
    readonly property int level: strength >= 75 ? 4 : strength >= 50 ? 3 : strength >= 25 ? 2 : 1

    spacing: Theme.px(3)
    height: Theme.px(22)

    Repeater {
        model: 4
        Rectangle {
            required property int index
            anchors.bottom: parent.bottom
            width: Theme.px(5)
            height: root.height * (index + 1) / 4
            radius: Theme.px(1.5)
            color: index < root.level ? root.color : Qt.rgba(1, 1, 1, 0.18)
        }
    }
}
