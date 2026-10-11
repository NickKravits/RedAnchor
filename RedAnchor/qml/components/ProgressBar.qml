import QtQuick

// Тонкая полоса прогресса со скруглёнными концами.
//   ProgressBar { width: 300; value: 0.62 }
// value < 0 — прогресс неизвестен: по дорожке бежит короткий блик.
Item {
    id: root

    property real value: 0
    property color trackColor: Theme.progressTrack
    property color fillColor: Theme.progressFill
    readonly property bool indeterminate: value < 0

    implicitWidth: Theme.px(320)
    implicitHeight: Math.max(2, Theme.px(6))

    Rectangle {
        id: track
        anchors.fill: parent
        radius: height / 2
        color: root.trackColor
        clip: root.indeterminate

        Rectangle {
            visible: !root.indeterminate
            width: Math.max(height, parent.width * Math.min(1, Math.max(0, root.value)))
            height: parent.height
            radius: height / 2
            color: root.fillColor
            Behavior on width { NumberAnimation { duration: Theme.animNormal; easing.type: Easing.OutCubic } }
        }

        // Неизвестный прогресс: блик едет слева направо
        Rectangle {
            id: runner
            visible: root.indeterminate
            width: parent.width * 0.3
            height: parent.height
            radius: height / 2
            color: root.fillColor
            opacity: 0.8
            NumberAnimation on x {
                running: root.indeterminate && root.visible
                from: -runner.width
                to: track.width
                duration: 1300
                loops: Animation.Infinite
                easing.type: Easing.InOutQuad
            }
        }
    }
}
