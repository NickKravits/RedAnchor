pragma ComponentBehavior: Bound
import QtQuick

// Прогресс первичной настройки: тонкие сегменты с подписями капсом.
Row {
    id: root

    property var steps: []
    property int current: 0

    spacing: Theme.px(10)

    Repeater {
        model: root.steps
        delegate: Column {
            id: stepItem
            required property int index
            required property string modelData
            readonly property bool done: index < root.current
            readonly property bool active: index === root.current

            spacing: Theme.px(10)
            width: Theme.px(128)

            Rectangle {
                width: parent.width
                height: Theme.px(2)
                color: Qt.rgba(1, 1, 1, 0.10)

                Rectangle {
                    height: parent.height
                    width: stepItem.done || stepItem.active ? parent.width : 0
                    color: stepItem.active ? Theme.textPrimary : Theme.textSecondary
                    Behavior on width { NumberAnimation { duration: Theme.animSlow; easing.type: Easing.OutCubic } }
                }
            }
            Text {
                text: stepItem.modelData.toUpperCase()
                color: stepItem.active ? Theme.textPrimary : Theme.textMuted
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontCaption
                font.weight: Font.Medium
                font.letterSpacing: Theme.px(1.5)
            }
        }
    }
}
