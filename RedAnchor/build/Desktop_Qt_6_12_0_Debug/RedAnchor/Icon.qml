import QtQuick
import QtQuick.Shapes

// Линейные иконки в сетке 24×24, отрисовываются векторно.
Item {
    id: root

    property string name: "check"
    property color color: Theme.textPrimary
    property real size: Theme.px(28)
    property real lineWidth: 1.9

    readonly property var paths: ({
        "check": "M5 12.5 L10 17.5 L19 7",
        "lock": "M6.5 11 L17.5 11 L17.5 20 L6.5 20 Z M9 11 L9 8 A3 3 0 0 1 15 8 L15 11",
        "ethernet": "M4 8 L20 8 L20 15.5 L16 15.5 L16 19 L8 19 L8 15.5 L4 15.5 Z M8 11 L8 12.5 M11 11 L11 12.5 M13 11 L13 12.5 M16 11 L16 12.5",
        "refresh": "M19.5 12 A7.5 7.5 0 1 1 17 6.4 M19 3.5 L19 8.5 L14 8.5",
        "bluetooth": "M7 7.5 L17 16.5 L12 21 L12 3 L17 7.5 L7 16.5",
        "controller": "M7 7.5 L17 7.5 C20.2 7.5 22.2 13 21.6 16.6 C21.1 19.3 18.6 19.4 17 17.3 L15.5 15.3 L8.5 15.3 L7 17.3 C5.4 19.4 2.9 19.3 2.4 16.6 C1.8 13 3.8 7.5 7 7.5 Z M7 10 L7 13 M5.5 11.5 L8.5 11.5 M16 10.5 L16.01 10.5 M18 12.5 L18.01 12.5",
        "anchor": "M12 7 A2.2 2.2 0 1 0 12 2.6 A2.2 2.2 0 1 0 12 7 Z M12 7 L12 21 M8 10 L16 10 M4 13.5 C4.5 18 8 21 12 21 C16 21 19.5 18 20 13.5 M2 15.5 L4 13.5 L6 15.5 M18 15.5 L20 13.5 L22 15.5",
        "user": "M12 12 A4 4 0 1 0 12 4 A4 4 0 1 0 12 12 Z M4.5 21 C4.5 16.5 8 14.5 12 14.5 C16 14.5 19.5 16.5 19.5 21",
        "globe": "M12 3 A9 9 0 1 0 12 21 A9 9 0 1 0 12 3 Z M3 12 L21 12 M12 3 C8.5 6.5 8.5 17.5 12 21 M12 3 C15.5 6.5 15.5 17.5 12 21",
        "backspace": "M9 6 L20.5 6 L20.5 18 L9 18 L3 12 Z M12 9.5 L17 14.5 M17 9.5 L12 14.5",
        "shift": "M12 4 L20 12.5 L15.5 12.5 L15.5 20 L8.5 20 L8.5 12.5 L4 12.5 Z",
        "caps": "M12 3.5 L20 11.5 L15.5 11.5 L15.5 17 L8.5 17 L8.5 11.5 L4 11.5 Z M8.5 20.5 L15.5 20.5",
        "space": "M4 13 L4 17 L20 17 L20 13",
        "enter": "M19 5 L19 13 L6 13 M10 9 L6 13 L10 17",
        "warning": "M12 3.5 L21.5 20 L2.5 20 Z M12 10 L12 14 M12 17 L12.01 17",
        "arrowRight": "M5 12 L19 12 M13 6 L19 12 L13 18"
    })

    implicitWidth: size
    implicitHeight: size

    Shape {
        width: 24
        height: 24
        scale: root.size / 24
        transformOrigin: Item.TopLeft
        preferredRendererType: Shape.CurveRenderer

        ShapePath {
            strokeColor: root.color
            strokeWidth: root.lineWidth
            fillColor: "transparent"
            capStyle: ShapePath.RoundCap
            joinStyle: ShapePath.RoundJoin
            PathSvg { path: root.paths[root.name] || "" }
        }
    }
}
