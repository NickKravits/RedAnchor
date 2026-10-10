import QtQuick
import QtQuick.Shapes

// USB-C кабель в стиле наброска, в координатах макета DualSense (600×400).
// Точка (0,0) — верх колпачка; кончик штекера заканчивается на y = 84.
Item {
    id: root

    readonly property real tipBottom: 84

    width: 40
    height: tipBottom

    Shape {
        width: 40
        height: 84
        preferredRendererType: Shape.CurveRenderer

        // Непрозрачная подложка, чтобы сетка не просвечивала сквозь штекер
        ShapePath {
            strokeColor: "transparent"
            fillColor: "#1c1f24"   // в тон фону, но непрозрачно
            PathSvg { path: "M 15 -900 L 25 -900 L 25 0 L 15 0 Z M 10 0 L 30 0 L 30 24 L 10 24 Z M 2 22 L 38 22 L 38 70 L 2 70 Z" }
        }

        ShapePath {
            strokeColor: Theme.sketchLine
            strokeWidth: 1.5
            fillColor: "transparent"
            joinStyle: ShapePath.RoundJoin
            // провод
            PathSvg { path: "M 15 -900 L 15 0 M 25 -900 L 25 0" }
            // колпачок
            PathSvg { path: "M 11 0 L 29 0 L 29 22 L 11 22 Z" }
            // корпус штекера
            PathSvg { path: "M 10 22 L 30 22 Q 38 22 38 30 L 38 62 Q 38 70 30 70 L 10 70 Q 2 70 2 62 L 2 30 Q 2 22 10 22 Z" }
            // наконечник
            PathSvg { path: "M 12 70 L 28 70 Q 31 70 31 73 L 31 81 Q 31 84 28 84 L 12 84 Q 9 84 9 81 L 9 73 Q 9 70 12 70 Z" }
        }
        ShapePath {
            strokeColor: Theme.sketchDim
            strokeWidth: 1
            fillColor: "transparent"
            // штриховка рифления
            PathSvg { path: "M 6 34 L 34 34 M 6 40 L 34 40 M 6 46 L 34 46" }
        }
    }
}
