import QtQuick

// Заглушка на время загрузки: «стеклянная» плашка с бегущим бликом.
Rectangle {
    id: root

    property bool running: visible

    radius: Theme.radiusTile
    color: Theme.glass
    border.width: 1
    border.color: Theme.glassBorder
    clip: true

    Rectangle {
        id: band
        width: root.width * 0.6
        height: root.height
        x: -width
        opacity: 0.9
        gradient: Gradient {
            orientation: Gradient.Horizontal
            GradientStop { position: 0.0; color: "transparent" }
            GradientStop { position: 0.5; color: Theme.glassHighlight }
            GradientStop { position: 1.0; color: "transparent" }
        }
        NumberAnimation on x {
            from: -band.width
            to: root.width
            duration: 1400
            loops: Animation.Infinite
            running: root.running
            easing.type: Easing.InOutQuad
        }
    }
}
