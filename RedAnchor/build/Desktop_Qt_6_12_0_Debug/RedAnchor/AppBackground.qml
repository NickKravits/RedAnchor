import QtQuick
import QtQuick.Shapes

// Тёмный фон с медленно плавающими красными «туманностями».
Rectangle {
    id: root

    property bool animated: true

    color: Theme.background

    gradient: Gradient {
        GradientStop { position: 0.0; color: "#0c0e16" }
        GradientStop { position: 1.0; color: Theme.background }
    }

    component Glow: Shape {
        id: glow
        property color tint: Theme.accent
        property real radius: 500
        width: radius * 2
        height: radius * 2
        preferredRendererType: Shape.CurveRenderer
        ShapePath {
            strokeColor: "transparent"
            fillGradient: RadialGradient {
                centerX: glow.radius; centerY: glow.radius
                centerRadius: glow.radius
                focalX: glow.radius; focalY: glow.radius
                GradientStop { position: 0.0; color: Qt.rgba(glow.tint.r, glow.tint.g, glow.tint.b, 0.22) }
                GradientStop { position: 0.45; color: Qt.rgba(glow.tint.r, glow.tint.g, glow.tint.b, 0.08) }
                GradientStop { position: 1.0; color: "transparent" }
            }
            PathAngleArc {
                centerX: glow.radius; centerY: glow.radius
                radiusX: glow.radius; radiusY: glow.radius
                startAngle: 0; sweepAngle: 360
            }
        }
    }

    Glow {
        id: glowA
        radius: root.height * 0.75
        tint: Theme.accent
        x: -radius * 0.6
        y: root.height - radius * 1.1
        SequentialAnimation on x {
            running: root.animated
            loops: Animation.Infinite
            NumberAnimation { to: -glowA.radius * 0.3; duration: 14000; easing.type: Easing.InOutSine }
            NumberAnimation { to: -glowA.radius * 0.6; duration: 14000; easing.type: Easing.InOutSine }
        }
    }

    Glow {
        id: glowB
        radius: root.height * 0.6
        tint: "#7a1430"
        x: root.width - radius * 1.2
        y: -radius * 0.7
        SequentialAnimation on y {
            running: root.animated
            loops: Animation.Infinite
            NumberAnimation { to: -glowB.radius * 0.45; duration: 17000; easing.type: Easing.InOutSine }
            NumberAnimation { to: -glowB.radius * 0.7; duration: 17000; easing.type: Easing.InOutSine }
        }
    }

    // Лёгкая виньетка по краям
    Rectangle {
        anchors.fill: parent
        gradient: Gradient {
            orientation: Gradient.Horizontal
            GradientStop { position: 0.0; color: Qt.rgba(0, 0, 0, 0.35) }
            GradientStop { position: 0.2; color: "transparent" }
            GradientStop { position: 0.8; color: "transparent" }
            GradientStop { position: 1.0; color: Qt.rgba(0, 0, 0, 0.35) }
        }
    }
}
