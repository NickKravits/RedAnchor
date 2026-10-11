import QtQuick
import QtQuick.Shapes

Item {
    id: root

    property real size: Theme.px(40)
    property color color: Theme.textPrimary
    property bool running: visible

    implicitWidth: size
    implicitHeight: size

    Shape {
        id: arc
        anchors.fill: parent
        preferredRendererType: Shape.CurveRenderer

        ShapePath {
            strokeColor: Qt.rgba(1, 1, 1, 0.14)
            strokeWidth: Math.max(1.5, root.size * 0.06)
            fillColor: "transparent"
            PathAngleArc {
                centerX: root.size / 2; centerY: root.size / 2
                radiusX: root.size * 0.42; radiusY: root.size * 0.42
                startAngle: 0; sweepAngle: 360
            }
        }
        ShapePath {
            strokeColor: root.color
            strokeWidth: Math.max(1.5, root.size * 0.06)
            fillColor: "transparent"
            capStyle: ShapePath.RoundCap
            PathAngleArc {
                centerX: root.size / 2; centerY: root.size / 2
                radiusX: root.size * 0.42; radiusY: root.size * 0.42
                startAngle: -90; sweepAngle: 100
            }
        }

        RotationAnimator on rotation {
            from: 0; to: 360
            duration: 900
            loops: Animation.Infinite
            running: root.running
        }
    }
}
