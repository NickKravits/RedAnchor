import QtQuick
import QtQuick.Shapes

// Линейные иконки в сетке 24×24, отрисовываются векторно.
Item {
    id: root

    property string name: "check"
    property color color: Theme.textPrimary
    property real size: Theme.px(28)
    property real lineWidth: 1.6

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
        "arrowRight": "M5 12 L19 12 M13 6 L19 12 L13 18",
        "info": "M12 3 A9 9 0 1 0 12 21 A9 9 0 1 0 12 3 Z M12 11 L12 16.5 M12 7.6 L12.01 7.6",
        "storage": "M3.5 7 L20.5 7 L20.5 17 L3.5 17 Z M7 12 L7.01 12 M11 12 L17 12",
        "plus": "M12 5 L12 19 M5 12 L19 12",
        "trash": "M5 7 L19 7 M10 7 L10 4.5 L14 4.5 L14 7 M7 7 L8 20 L16 20 L17 7 M10.5 11 L10.5 16 M13.5 11 L13.5 16",
        "edit": "M4 20 L8.5 19 L19.5 8 L16 4.5 L5 15.5 Z M14 6.5 L17.5 10",
        "palette": "M12 3 A9 9 0 1 0 12 21 C13.5 21 14 20 13.5 18.8 C13 17.5 13.8 16.5 15.2 16.5 L17 16.5 C19.4 16.5 21 14.8 21 12 C21 7 17 3 12 3 Z M7.5 11 L7.51 11 M10 7.5 L10.01 7.5 M14.5 7.5 L14.51 7.5 M17 11 L17.01 11",
        "volume": "M4 9.5 L8 9.5 L12.5 5.5 L12.5 18.5 L8 14.5 L4 14.5 Z M16 9 A4 4 0 0 1 16 15 M18.5 6.5 A7.5 7.5 0 0 1 18.5 17.5",
        "volumeMute": "M4 9.5 L8 9.5 L12.5 5.5 L12.5 18.5 L8 14.5 L4 14.5 Z M16 9.5 L20.5 14 M20.5 9.5 L16 14",
        "power": "M12 3.5 L12 11 M7.2 6.5 A7.5 7.5 0 1 0 16.8 6.5",
        "store": "M5 8 L19 8 L18 20 L6 20 Z M9 8 A3 3 0 0 1 15 8",
        "apps": "M4 4 L10 4 L10 10 L4 10 Z M14 4 L20 4 L20 10 L14 10 Z M4 14 L10 14 L10 20 L4 20 Z M14 14 L20 14 L20 20 L14 20 Z",
        "library": "M5 4 L5 20 M9 4 L9 20 M13 5 L17.5 19.5",
        "search": "M10.5 4 A6.5 6.5 0 1 0 10.5 17 A6.5 6.5 0 1 0 10.5 4 Z M15.2 15.2 L20 20",
        "users": "M9 11 A3.5 3.5 0 1 0 9 4 A3.5 3.5 0 1 0 9 11 Z M2.5 20 C2.5 16 5.5 14 9 14 C12.5 14 15.5 16 15.5 20 M16 4.3 A3.5 3.5 0 0 1 16 10.7 M18 14.3 C20 15 21.5 16.8 21.5 20",
        "swap": "M4 8 L18 8 M14 4 L18 8 L14 12 M20 16 L6 16 M10 12 L6 16 L10 20",
        "pin": "M9 3.5 L15 3.5 L14 9.5 L17.5 13 L6.5 13 L10 9.5 Z M12 13 L12 20.5",
        "close": "M6 6 L18 18 M18 6 L6 18",
        "play": "M7.5 5 L19 12 L7.5 19 Z",
        "logout": "M10 4 L4.5 4 L4.5 20 L10 20 M9 12 L20 12 M16 8 L20 12 L16 16",
        "headphones": "M4 15 L4 12 A8 8 0 0 1 20 12 L20 15 M4 14 L7.5 14 L7.5 20 L4 20 Z M20 14 L16.5 14 L16.5 20 L20 20 Z",
        "battery": "M3 8 L18 8 L18 16 L3 16 Z M20.5 10.5 L20.5 13.5 M5.5 10.5 L8 10.5 L8 13.5 L5.5 13.5 Z",
        "download": "M12 4 L12 15 M7.5 10.5 L12 15 L16.5 10.5 M5 19.5 L19 19.5",
        "chip": "M7 7 L17 7 L17 17 L7 17 Z M10 10 L14 10 L14 14 L10 14 Z M9.5 3.5 L9.5 7 M14.5 3.5 L14.5 7 M9.5 17 L9.5 20.5 M14.5 17 L14.5 20.5 M3.5 9.5 L7 9.5 M3.5 14.5 L7 14.5 M17 9.5 L20.5 9.5 M17 14.5 L20.5 14.5",
        "vulkan": "M4 5 L12 19 L20 5 M8.5 5 L12 11.5 L15.5 5",
        "proton": "M12 10.5 A1.5 1.5 0 1 0 12 13.5 A1.5 1.5 0 1 0 12 10.5 Z M3 12 A9 3.5 0 1 0 21 12 A9 3.5 0 1 0 3 12 Z M7.5 4.21 A9 3.5 60 1 0 16.5 19.79 A9 3.5 60 1 0 7.5 4.21 Z M7.5 19.79 A9 3.5 -60 1 0 16.5 4.21 A9 3.5 -60 1 0 7.5 19.79 Z"
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
