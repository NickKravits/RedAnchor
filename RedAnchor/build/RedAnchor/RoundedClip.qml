import QtQuick
import QtQuick.Effects

// Обрезает содержимое по скруглённому прямоугольнику.
// Rectangle { radius; clip: true } режет только по прямоугольнику — картинка внутри
// остаётся с квадратными углами. Здесь содержимое рисуется в слой и маскируется.
Item {
    id: root

    property real radius: Theme.radius
    default property alias content: holder.data

    Item {
        id: holder
        anchors.fill: parent
        layer.enabled: true
        layer.smooth: true
        layer.effect: MultiEffect {
            maskEnabled: true
            maskSource: mask
            maskThresholdMin: 0.5
            maskSpreadAtMin: 1.0
        }
    }

    // Маска: скруглённый прямоугольник (рисуется только в слой)
    Item {
        id: mask
        anchors.fill: parent
        visible: false
        layer.enabled: true
        layer.smooth: true
        Rectangle {
            anchors.fill: parent
            radius: root.radius
            color: "black"
            antialiasing: true
        }
    }
}
