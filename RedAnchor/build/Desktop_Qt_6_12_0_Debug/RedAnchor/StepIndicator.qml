pragma ComponentBehavior: Bound
import QtQuick

// Прогресс первичной настройки сверху экрана.
Row {
    id: root

    property var steps: []
    property int current: 0

    spacing: Theme.px(14)

    Repeater {
        model: root.steps
        delegate: Column {
            id: stepItem
            required property int index
            required property string modelData
            readonly property bool done: index < root.current
            readonly property bool active: index === root.current

            spacing: Theme.px(8)
            width: Theme.px(150)

            Rectangle {
                width: parent.width
                height: Theme.px(4)
                radius: height / 2
                color: Qt.rgba(1, 1, 1, 0.12)

                Rectangle {
                    height: parent.height
                    radius: parent.radius
                    width: stepItem.done || stepItem.active ? parent.width : 0
                    color: stepItem.active ? Theme.accent : Theme.textPrimary
                    Behavior on width { NumberAnimation { duration: Theme.animSlow; easing.type: Easing.OutCubic } }
                    Behavior on color { ColorAnimation { duration: Theme.animNormal } }
                }
            }
            Text {
                text: stepItem.modelData
                color: stepItem.active ? Theme.textPrimary : stepItem.done ? Theme.textSecondary : Theme.textMuted
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontSmall
                font.weight: stepItem.active ? Font.DemiBold : Font.Normal
            }
        }
    }
}
