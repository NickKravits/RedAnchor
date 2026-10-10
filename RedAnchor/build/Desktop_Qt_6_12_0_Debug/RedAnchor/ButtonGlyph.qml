import QtQuick
import QtQuick.Shapes

// Обозначение кнопки геймпада (✕ ○ □ △, L1, Options…).
// Если геймпад не подключён — показывает соответствующую клавишу клавиатуры.
Item {
    id: root

    property string button: "cross"
    property real size: Theme.px(34)
    property bool keyboardMode: !GamepadManager.connected

    readonly property var symbolPaths: ({
        "cross": "M7.6 7.6 L16.4 16.4 M16.4 7.6 L7.6 16.4",
        "circle": "M12 6.6 A5.4 5.4 0 1 1 11.99 6.6 Z",
        "square": "M7.4 7.4 L16.6 7.4 L16.6 16.6 L7.4 16.6 Z",
        "triangle": "M12 6.4 L17.4 15.9 L6.6 15.9 Z"
    })
    readonly property var buttonLabels: ({
        "l1": "L1", "r1": "R1", "l2": "L2", "r2": "R2", "l3": "L3", "r3": "R3",
        "options": "OPTIONS", "create": "CREATE", "ps": "PS", "dpad": "✥"
    })
    readonly property var keyLabels: ({
        "cross": "Enter", "circle": "Esc", "square": "⌫", "triangle": "Пробел",
        "l1": "PgUp", "r1": "PgDn", "l2": "Home", "r2": "End",
        "options": "Menu", "create": "F2", "ps": "Home", "dpad": "←↑→"
    })

    readonly property bool isSymbol: !keyboardMode && symbolPaths[button] !== undefined
    readonly property string label: keyboardMode ? (keyLabels[button] || button)
                                                 : (buttonLabels[button] || button)

    implicitHeight: size
    implicitWidth: isSymbol ? size : Math.max(size, labelText.implicitWidth + size * 0.6)

    Rectangle {
        anchors.fill: parent
        radius: root.isSymbol ? width / 2 : height * 0.3
        color: "#2a2f43"
        border.color: "#3b4160"
        border.width: Math.max(1, Theme.px(1.5))
    }

    Shape {
        visible: root.isSymbol
        width: 24
        height: 24
        scale: root.size / 24
        transformOrigin: Item.TopLeft
        preferredRendererType: Shape.CurveRenderer
        ShapePath {
            strokeColor: Theme.textPrimary
            strokeWidth: 1.9
            fillColor: "transparent"
            capStyle: ShapePath.RoundCap
            joinStyle: ShapePath.RoundJoin
            PathSvg { path: root.symbolPaths[root.button] || "" }
        }
    }

    Text {
        id: labelText
        visible: !root.isSymbol
        anchors.centerIn: parent
        text: root.label
        color: Theme.textPrimary
        font.family: Theme.fontFamily
        font.pixelSize: root.size * (text.length > 3 ? 0.36 : 0.44)
        font.weight: Font.DemiBold
        font.letterSpacing: text.length > 3 ? 0.5 : 0
    }
}
