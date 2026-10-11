pragma ComponentBehavior: Bound
import QtQuick

// «Стеклянная» панель: полупрозрачная тёмная заливка поверх живого фона,
// тонкая светлая кромка, блик по верхнему краю и мягкая тень снаружи.
// Содержимое кладётся внутрь как в обычный Rectangle.
//   GlassPanel { radius: Theme.radiusLarge; strong: true }
// Тень рисуется кольцами только снаружи панели, чтобы не темнить само стекло.
Rectangle {
    id: root

    // Плотнее (меню, оверлеи): текст читается поверх любой картинки
    property bool strong: false
    property bool shadow: true
    property bool highlight: true

    radius: Theme.radiusLarge
    color: strong ? Theme.glassStrong : Theme.glass
    border.width: 1
    border.color: Theme.glassBorder
    antialiasing: true

    // Мягкая тень: кольца, смещённые вниз
    Repeater {
        model: root.shadow ? 4 : 0
        delegate: Rectangle {
            required property int index
            readonly property real spread: (index + 1) * Theme.px(5)
            z: -1
            x: -spread
            y: -spread + Theme.px(6)
            width: root.width + spread * 2
            height: root.height + spread * 2
            radius: root.radius + spread
            color: "transparent"
            border.width: Theme.px(5)
            border.color: Qt.rgba(0, 0, 0, 0.11 - index * 0.025)
            antialiasing: true
        }
    }

    // Блик по верхнему краю: стекло «ловит» свет сверху
    Rectangle {
        visible: root.highlight
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.margins: 1
        height: Math.min(parent.height * 0.5, Theme.px(120))
        radius: Math.max(0, root.radius - 1)
        gradient: Gradient {
            GradientStop { position: 0.0; color: Theme.glassHighlight }
            GradientStop { position: 1.0; color: "transparent" }
        }
    }
}
