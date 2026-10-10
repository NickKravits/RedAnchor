pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Shapes

// DualSense в виде технического чертежа: вид спереди, перенесённый с эталонного
// чертежа (160 × 106 мм), тонкие белые линии, размерные линии и выноски.
// Анимируемые элементы: проявление рисунка, световая панель, индикатор игрока,
// кабель USB-C, подсветка кнопок.
//
// Внутри — макет 660×470. Геометрия контроллера задана в координатах эталона
// (пиксели исходного чертежа, контур занимает x 38…359.6, y 26.5…240.5) и
// масштабируется коэффициентом k в рамку вида спереди 520×346.
Item {
    id: root

    // "off" | "breathe" | "blink" | "solid"
    property string lightbarMode: "off"
    property color lightbarColor: Theme.lightbarBlue
    property int playerLeds: 0
    // "hidden" | "unplugged" | "plugged" | "hint" (зацикленная подсказка «вставьте кабель»)
    // | "unplugHint" (кабель вставлен, зацикленная подсказка «отключите кабель»)
    property string cableState: "hidden"
    // Кнопки с пульсирующим кольцом: ["ps"]
    property var highlighted: []
    // Выноски с подписями: ["usb", "ps", "create", "lightbar"]
    property var callouts: []
    // Проявление рисунка при появлении
    property real drawProgress: 0
    property bool animateIn: true
    property bool showDimensions: true

    property real lightbarLevel: 0
    property string flashName: ""

    // Рамка вида спереди в координатах макета
    readonly property point front: Qt.point(30, 50)
    readonly property real frontWidth: 520
    readonly property real frontHeight: 346
    // Эталон: границы контура и ось симметрии
    readonly property real srcLeft: 38
    readonly property real srcTop: 26.5
    readonly property real axis: 198.8
    readonly property real k: frontWidth / (2 * (axis - srcLeft))
    // Толщина линии 1.4 px макета в координатах эталона
    readonly property real lw: 1.4 / k
    // Верх порта USB-C — сюда упирается наконечник кабеля
    readonly property real portY: front.y + 1

    // Короткая вспышка кнопки, когда её нажали на реальном контроллере.
    function flash(name) {
        flashName = name
        flashTimer.restart()
    }

    implicitWidth: 660
    implicitHeight: 470

    Component.onCompleted: {
        updateLightbar()
        if (animateIn)
            drawAnim.start()
        else
            drawProgress = 1
    }

    NumberAnimation {
        id: drawAnim
        target: root
        property: "drawProgress"
        from: 0; to: 1
        duration: 1800
        easing.type: Easing.InOutCubic
    }

    Timer {
        id: flashTimer
        interval: 220
        onTriggered: root.flashName = ""
    }

    // --- Световая панель ---
    onLightbarModeChanged: updateLightbar()
    function updateLightbar() {
        breathe.stop()
        blink.stop()
        if (lightbarMode === "breathe") {
            breathe.start()
        } else if (lightbarMode === "blink") {
            blink.start()
        } else {
            levelAnim.to = lightbarMode === "solid" ? 1 : 0
            levelAnim.restart()
        }
    }
    NumberAnimation { id: levelAnim; target: root; property: "lightbarLevel"; duration: 400 }
    SequentialAnimation {
        id: breathe
        loops: Animation.Infinite
        NumberAnimation { target: root; property: "lightbarLevel"; to: 1; duration: 1400; easing.type: Easing.InOutSine }
        NumberAnimation { target: root; property: "lightbarLevel"; to: 0.15; duration: 1400; easing.type: Easing.InOutSine }
    }
    SequentialAnimation {
        id: blink
        loops: Animation.Infinite
        PropertyAction { target: root; property: "lightbarLevel"; value: 1 }
        PauseAnimation { duration: 90 }
        PropertyAction { target: root; property: "lightbarLevel"; value: 0 }
        PauseAnimation { duration: 120 }
        PropertyAction { target: root; property: "lightbarLevel"; value: 1 }
        PauseAnimation { duration: 90 }
        PropertyAction { target: root; property: "lightbarLevel"; value: 0 }
        PauseAnimation { duration: 650 }
    }

    // ---------- Построение контуров ----------
    // Контроллер симметричен: рисуем левую половину сегментами
    // ["L", x, y] / ["C", x1, y1, x2, y2, x, y], правая получается зеркалом
    // относительно оси x = axis.

    function mx(x) { return 2 * axis - x }
    function round2(v) { return Math.round(v * 100) / 100 }

    function segmentsPath(start, segs, mirrored) {
        const fx = mirrored ? (x => root.round2(root.mx(x))) : (x => x)
        let d = "M " + fx(start[0]) + " " + start[1]
        for (const s of segs) {
            if (s[0] === "L")
                d += " L " + fx(s[1]) + " " + s[2]
            else
                d += " C " + fx(s[1]) + " " + s[2] + " " + fx(s[3]) + " " + s[4] + " " + fx(s[5]) + " " + s[6]
        }
        return d
    }

    // Открытая линия и её зеркальная копия
    function pair(start, segs) {
        return segmentsPath(start, segs, false) + " " + segmentsPath(start, segs, true)
    }

    // Замкнутый контур: левая половина от оси до оси, затем она же зеркально в обратную сторону
    function symmetric(start, segs) {
        let d = segmentsPath(start, segs, false)
        const pts = [start]
        for (const s of segs)
            pts.push([s[s.length - 2], s[s.length - 1]])
        for (let i = segs.length - 1; i >= 0; --i) {
            const s = segs[i]
            const p0 = pts[i]
            if (s[0] === "L")
                d += " L " + round2(mx(p0[0])) + " " + p0[1]
            else
                d += " C " + round2(mx(s[3])) + " " + s[4] + " " + round2(mx(s[1])) + " " + s[2] + " "
                     + round2(mx(p0[0])) + " " + p0[1]
        }
        return d + " Z"
    }

    function circle(cx, cy, r) {
        return "M " + (cx - r) + " " + cy + " A " + r + " " + r + " 0 1 0 " + (cx + r) + " " + cy
             + " A " + r + " " + r + " 0 1 0 " + (cx - r) + " " + cy + " Z"
    }

    // Путь из команд [["M"|"L", x, y], ["Q", x1, y1, x, y], ["Z"]], повёрнутый
    // вокруг (cx, cy) на angle градусов (точки заданы относительно центра)
    function rotated(cx, cy, cmds, angle) {
        const a = angle * Math.PI / 180
        const c = Math.cos(a), s = Math.sin(a)
        const p = (x, y) => root.round2(cx + x * c - y * s) + " " + root.round2(cy + x * s + y * c)
        let d = ""
        for (const m of cmds) {
            if (m[0] === "Z")
                d += " Z"
            else if (m[0] === "Q")
                d += " Q " + p(m[1], m[2]) + " " + p(m[3], m[4])
            else
                d += " " + m[0] + " " + p(m[1], m[2])
        }
        return d
    }

    // Капсула (кнопки Create/Options, микрофон) с центром (cx, cy);
    // angle — наклон от вертикали по часовой стрелке
    function capsule(cx, cy, length, width, angle) {
        const r = width / 2
        const half = length / 2 - r
        const a = angle * Math.PI / 180
        const ux = Math.sin(a), uy = -Math.cos(a)     // вдоль капсулы
        const nx = Math.cos(a), ny = Math.sin(a)      // поперёк
        const p = (t, n) => (root.round2(cx + ux * t + nx * n) + " " + root.round2(cy + uy * t + ny * n))
        return "M " + p(half, -r) + " A " + r + " " + r + " 0 0 1 " + p(half, r)
             + " L " + p(-half, r) + " A " + r + " " + r + " 0 0 1 " + p(-half, -r) + " Z"
    }

    // --- Геометрия (координаты эталона) ---
    readonly property string bodyPath: symmetric([198.8, 29], [
        ["C", 170, 29, 140, 32, 120, 34],          // верхняя грань
        ["C", 105, 35.5, 90, 38.3, 77.5, 41],      // под бампером L1
        ["C", 73, 42, 67.5, 50, 63.6, 60],         // плечо
        ["C", 54, 80, 38.5, 125, 38, 175],         // внешний край
        ["C", 37.6, 200, 40, 232, 62.5, 237],      // рукоятка
        ["C", 68, 239.6, 78, 241, 83.6, 238],      // низ рукоятки
        ["C", 92, 220, 104, 193, 112.5, 173],      // внутренний край рукоятки
        ["C", 116, 168, 120, 165.5, 126, 165],
        ["C", 150, 164.4, 180, 166, 198.8, 166]    // нижняя грань
    ])
    // Бамперы L1 / R1 над плечами
    readonly property string bumperPath: pair([77.5, 41], [
        ["C", 77, 36, 86, 29.5, 100, 27],
        ["C", 108, 25.8, 116, 26.5, 118.5, 28.5],
        ["C", 120, 30, 120, 32, 120, 34],
        ["C", 105, 35.5, 90, 38.3, 77.5, 41]
    ])
    // Сенсорная панель (её верх совпадает с верхней гранью)
    readonly property string touchpadPath: pair([135.5, 33.3], [
        ["C", 138, 55, 140, 75, 143, 84],
        ["C", 146, 93, 152, 98.5, 162, 99],
        ["L", 198.8, 99]
    ])
    // Шов: вдоль световой панели и вниз к низу рукоятки
    readonly property string seamPath: pair([128.5, 33.7], [
        ["C", 133, 55, 139, 72, 139.9, 88],
        ["C", 140.5, 96, 134, 103, 126, 111],
        ["C", 110, 132, 82, 192, 65, 237]
    ])
    // Световая панель — полоса между швом и краем сенсорной панели
    readonly property string lightbarPath: pair([133, 40], [["C", 135.5, 55, 138.5, 70, 140.5, 82]])

    readonly property point dpad: Qt.point(98.4, 88.25)
    readonly property point faceButtons: Qt.point(mx(dpad.x), 88)
    readonly property point leftStick: Qt.point(147.25, 132)
    readonly property point rightStick: Qt.point(mx(leftStick.x), 132)

    // Лепесток крестовины, направленный вверх (относительно центра крестовины)
    readonly property var dpadPetal: [
        ["M", -8.75, -11.25], ["L", -8.75, -22.25], ["Q", -8.75, -26.25, -4.75, -26.25],
        ["L", 4.75, -26.25], ["Q", 8.75, -26.25, 8.75, -22.25], ["L", 8.75, -11.25],
        ["Q", 8.75, -9.5, 7.3, -8.4], ["L", 1.3, -5], ["Q", 0, -4.3, -1.3, -5],
        ["L", -7.3, -8.4], ["Q", -8.75, -9.5, -8.75, -11.25], ["Z"]
    ]

    // Тонкий контур в стиле чертежа
    component Line: ShapePath {
        strokeColor: Theme.sketchLine
        strokeWidth: root.lw
        fillColor: "transparent"
        capStyle: ShapePath.RoundCap
        joinStyle: ShapePath.RoundJoin
    }

    // Размерная линия со стрелками на концах (координаты макета)
    component Dimension: Shape {
        id: dim
        property real x1
        property real y1
        property real x2
        property real y2
        readonly property real len: Math.hypot(x2 - x1, y2 - y1)
        readonly property real ux: (x2 - x1) / len
        readonly property real uy: (y2 - y1) / len
        // Разрыв линии посередине под подпись (0 — без разрыва)
        property real gap: 0

        width: 660
        height: 470
        preferredRendererType: Shape.CurveRenderer

        ShapePath {
            strokeColor: Theme.sketchDim
            strokeWidth: 1
            fillColor: "transparent"
            capStyle: ShapePath.RoundCap
            joinStyle: ShapePath.RoundJoin
            PathSvg {
                path: {
                    const mxp = (dim.x1 + dim.x2) / 2, myp = (dim.y1 + dim.y2) / 2
                    const g = dim.gap / 2
                    const a = 7, w = 3.5   // длина и полуширина стрелки
                    const arrow = (px, py, dx, dy) =>
                        "M " + (px + dx * a - dy * w) + " " + (py + dy * a + dx * w)
                        + " L " + px + " " + py
                        + " L " + (px + dx * a + dy * w) + " " + (py + dy * a - dx * w)
                    return "M " + dim.x1 + " " + dim.y1 + " L " + (mxp - dim.ux * g) + " " + (myp - dim.uy * g)
                         + " M " + (mxp + dim.ux * g) + " " + (myp + dim.uy * g) + " L " + dim.x2 + " " + dim.y2
                         + " " + arrow(dim.x1, dim.y1, dim.ux, dim.uy)
                         + " " + arrow(dim.x2, dim.y2, -dim.ux, -dim.uy)
                }
            }
        }
    }

    Item {
        id: canvas
        width: 660
        height: 470
        scale: root.width / 660
        transformOrigin: Item.TopLeft

        // ---------- Кабель (под корпусом, чтобы наконечник «уходил» в порт) ----------
        Item {
            id: cableHolder
            x: root.front.x + (root.axis - root.srcLeft) * root.k - cable.width / 2
            opacity: root.cableState === "hidden" ? 0 : root.drawProgress
            y: root.cableState === "plugged" || root.cableState === "unplugHint" ? root.portY - cable.tipBottom : -230
            Behavior on opacity { NumberAnimation { duration: Theme.animNormal } }
            Behavior on y { NumberAnimation { duration: 900; easing.type: Easing.InOutCubic } }

            UsbCable {
                id: cable

                SequentialAnimation on y {
                    running: root.cableState === "hint"
                    loops: Animation.Infinite
                    onRunningChanged: if (!running) cable.y = 0
                    PauseAnimation { duration: 600 }
                    NumberAnimation { to: 230 + root.portY - cable.tipBottom - 12; duration: 1200; easing.type: Easing.InOutCubic }
                    NumberAnimation { to: 230 + root.portY - cable.tipBottom; duration: 280; easing.type: Easing.OutQuad }
                    PauseAnimation { duration: 1200 }
                    NumberAnimation { to: 0; duration: 1000; easing.type: Easing.InOutCubic }
                    PauseAnimation { duration: 400 }
                }

                SequentialAnimation {
                    running: root.cableState === "unplugHint"
                    loops: Animation.Infinite
                    onRunningChanged: if (!running) cable.y = 0
                    PauseAnimation { duration: 900 }
                    NumberAnimation { target: cable; property: "y"; to: -8; duration: 260; easing.type: Easing.OutQuad }
                    NumberAnimation { target: cable; property: "y"; to: -90; duration: 900; easing.type: Easing.InOutCubic }
                    PauseAnimation { duration: 700 }
                    NumberAnimation { target: cable; property: "y"; to: 0; duration: 900; easing.type: Easing.InOutCubic }
                }
            }
        }

        // ---------- Рисунок, проявляющийся слева направо ----------
        Item {
            id: reveal
            width: 660 * root.drawProgress
            height: 470
            clip: true

            // Размерные линии, как на чертеже
            Item {
                visible: root.showDimensions
                width: 660
                height: 470

                Dimension {
                    x1: root.front.x; y1: root.front.y + root.frontHeight + 42
                    x2: root.front.x + root.frontWidth; y2: y1
                    gap: 84
                }
                Text {
                    x: root.front.x + root.frontWidth / 2 - implicitWidth / 2
                    y: root.front.y + root.frontHeight + 42 - implicitHeight / 2
                    text: qsTr("160 мм")
                    color: Theme.sketchLine
                    font.family: Theme.fontFamily
                    font.pixelSize: 13
                    font.letterSpacing: 0.5
                }

                Dimension {
                    x1: root.front.x + root.frontWidth + 34; y1: root.front.y
                    x2: x1; y2: root.front.y + root.frontHeight
                }
                Text {
                    x: root.front.x + root.frontWidth + 44
                    y: root.front.y + root.frontHeight / 2 - implicitHeight / 2
                    text: qsTr("106 мм")
                    color: Theme.sketchLine
                    font.family: Theme.fontFamily
                    font.pixelSize: 13
                    font.letterSpacing: 0.5
                }
            }

            // ---------- Вид спереди ----------
            Item {
                id: frontView
                x: root.front.x
                y: root.front.y
                width: root.frontWidth
                height: root.frontHeight

                Item {
                    id: geom
                    x: -root.srcLeft * root.k
                    y: -root.srcTop * root.k
                    width: 400
                    height: 260
                    scale: root.k
                    transformOrigin: Item.TopLeft

                    Shape {
                        id: body
                        width: 400
                        height: 260
                        preferredRendererType: Shape.CurveRenderer

                        // Непрозрачная заливка: сетка фона и кабель не просвечивают
                        ShapePath {
                            strokeColor: "transparent"
                            fillColor: "#050a1b"   // в тон фону, но непрозрачно
                            PathSvg { path: root.bodyPath }
                            PathSvg { path: root.bumperPath }
                        }

                        // Внешний контур и бамперы
                        Line {
                            strokeWidth: root.lw * 1.15
                            PathSvg { path: root.bodyPath }
                            PathSvg { path: root.bumperPath }
                        }

                        // Сенсорная панель и шов вдоль световой панели
                        Line {
                            PathSvg { path: root.touchpadPath }
                            PathSvg { path: root.seamPath }
                        }

                        // Порт USB-C на верхней грани и прорезь на нижней
                        Line {
                            strokeWidth: root.lw * 0.8
                            PathSvg {
                                path: "M 194.6 27.4 L 203 27.4 Q 204.6 27.4 204.6 29 Q 204.6 30.6 203 30.6 "
                                    + "L 194.6 30.6 Q 193 30.6 193 29 Q 193 27.4 194.6 27.4 Z"
                            }
                            PathSvg { path: "M 196 162.9 L 201.6 162.9 L 201.6 164.3 L 196 164.3 Z" }
                        }

                        // Крестовина: четыре лепестка, сходящиеся к центру
                        Line {
                            PathSvg {
                                path: [0, 90, 180, 270].map(a => root.rotated(root.dpad.x, root.dpad.y, root.dpadPetal, a)).join(" ")
                            }
                        }

                        // Кнопки действий (на эталоне — без символов)
                        Line {
                            PathSvg {
                                path: {
                                    const c = root.faceButtons, o = 23.5, r = 10.5
                                    return root.circle(c.x, c.y - o, r) + " " + root.circle(c.x + o, c.y, r) + " "
                                         + root.circle(c.x, c.y + o, r) + " " + root.circle(c.x - o, c.y, r)
                                }
                            }
                        }

                        // Create / Options
                        Line {
                            PathSvg { path: root.capsule(123.5, 52.75, 15.5, 7.5, -8.7) }
                            PathSvg { path: root.capsule(root.mx(123.5), 52.75, 15.5, 7.5, 8.7) }
                        }

                        // Стики: двойное кольцо и шляпка
                        Line {
                            PathSvg {
                                path: [root.leftStick, root.rightStick].map(s =>
                                    root.circle(s.x, s.y, 21.25) + " " + root.circle(s.x, s.y, 19.25) + " "
                                    + root.circle(s.x, s.y, 12)).join(" ")
                            }
                        }

                        // Пунктирные дуги вокруг стиков (центр ниже стика, снизу разомкнуты)
                        ShapePath {
                            strokeColor: Theme.sketchLine
                            strokeWidth: root.lw * 0.8
                            fillColor: "transparent"
                            capStyle: ShapePath.RoundCap
                            strokeStyle: ShapePath.DashLine
                            dashPattern: [1.6, 2.6]
                            PathSvg { path: "M 124.1 159.3 A 32.35 32.35 0 1 1 172.85 156.7" }
                            PathSvg {
                                path: "M " + root.round2(root.mx(172.85)) + " 156.7 A 32.35 32.35 0 1 1 "
                                    + root.round2(root.mx(124.1)) + " 159.3"
                            }
                        }

                        // Решётка динамика: 5 + 4 отверстия
                        Line {
                            strokeWidth: root.lw * 0.75
                            PathSvg {
                                path: {
                                    let d = ""
                                    for (let i = -2; i <= 2; ++i)
                                        d += root.circle(root.axis + i * 6, 108.5, 1.6) + " "
                                    for (let i = 0; i < 4; ++i)
                                        d += root.circle(root.axis - 9 + i * 6, 113.6, 1.6) + " "
                                    return d
                                }
                            }
                        }

                        // Кнопка PS (логотип) и кнопка микрофона
                        Line {
                            strokeWidth: root.lw * 0.85
                            PathSvg {
                                path: "M 196 137 L 196 123.3 Q 196 122.4 197 122.6 L 200.6 123.4 "
                                    + "C 205.4 124.6 205.2 131.4 200.6 130.2 L 200.6 137 Z"
                            }
                            PathSvg { path: "M 195.5 132.1 C 191.2 132.3 188.4 133.2 189 134.3 C 189.6 135.3 193 135.3 196 134.7" }
                            PathSvg { path: "M 201.2 135.7 C 205.3 135.5 208.9 134.6 208.6 133.2 C 208.3 132 205.4 131.7 203.2 132" }
                            PathSvg { path: root.capsule(root.axis, 149.3, 16.6, 4.6, 90) }
                        }
                    }

                    // Световая панель — единственный цветной элемент
                    Shape {
                        width: 400
                        height: 260
                        preferredRendererType: Shape.CurveRenderer

                        ShapePath {
                            strokeColor: Qt.rgba(root.lightbarColor.r, root.lightbarColor.g, root.lightbarColor.b, 0.25 * root.lightbarLevel)
                            strokeWidth: 7
                            fillColor: "transparent"
                            capStyle: ShapePath.RoundCap
                            PathSvg { path: root.lightbarPath }
                        }
                        ShapePath {
                            strokeColor: Qt.rgba(root.lightbarColor.r, root.lightbarColor.g, root.lightbarColor.b, 0.9 * root.lightbarLevel)
                            strokeWidth: 1.8
                            fillColor: "transparent"
                            capStyle: ShapePath.RoundCap
                            PathSvg { path: root.lightbarPath }
                        }
                    }

                    // Индикатор игрока под сенсорной панелью — виден, только когда горит
                    Rectangle {
                        x: root.axis - width / 2
                        y: 102
                        width: 5
                        height: 1
                        radius: 0.5
                        color: "#ffffff"
                        opacity: root.playerLeds >= 1 ? 1 : 0
                        Behavior on opacity { NumberAnimation { duration: 300 } }
                    }

                    // ---------- Пульсирующие кольца и вспышки нажатий ----------
                    Repeater {
                        model: [
                            { name: "cross", x: 299.2, y: 111.5, r: 11.5 }, { name: "circle", x: 322.7, y: 88, r: 11.5 },
                            { name: "square", x: 275.7, y: 88, r: 11.5 }, { name: "triangle", x: 299.2, y: 64.5, r: 11.5 },
                            { name: "up", x: 98.4, y: 72, r: 10.5 }, { name: "down", x: 98.4, y: 104.5, r: 10.5 },
                            { name: "left", x: 82.1, y: 88.25, r: 10.5 }, { name: "right", x: 114.7, y: 88.25, r: 10.5 },
                            { name: "ps", x: 198.8, y: 129.5, r: 10 }, { name: "create", x: 123.5, y: 52.75, r: 9 },
                            { name: "options", x: 274.1, y: 52.75, r: 9 }, { name: "l3", x: 147.25, y: 132, r: 22.5 },
                            { name: "r3", x: 250.35, y: 132, r: 22.5 }, { name: "l1", x: 99, y: 31, r: 14 },
                            { name: "r1", x: 298.6, y: 31, r: 14 }
                        ]
                        delegate: Item {
                            id: marker
                            required property var modelData
                            readonly property bool hinted: root.highlighted.indexOf(modelData.name) >= 0 && root.drawProgress >= 1
                            readonly property bool flashing: root.flashName === modelData.name

                            x: modelData.x
                            y: modelData.y

                            Rectangle {
                                anchors.centerIn: parent
                                width: marker.modelData.r * 2 + 2.5
                                height: width
                                radius: width / 2
                                color: Qt.rgba(1, 1, 1, marker.flashing ? 0.45 : 0)
                                border.width: root.lw * 0.85
                                border.color: Theme.textPrimary
                                opacity: marker.hinted || marker.flashing ? 1 : 0
                                Behavior on opacity { NumberAnimation { duration: 120 } }
                            }
                            Rectangle {
                                id: wave
                                anchors.centerIn: parent
                                width: marker.modelData.r * 2 + 2.5
                                height: width
                                radius: width / 2
                                color: "transparent"
                                border.width: root.lw * 0.7
                                border.color: Theme.textPrimary
                                visible: marker.hinted
                                ParallelAnimation {
                                    running: marker.hinted
                                    loops: Animation.Infinite
                                    NumberAnimation { target: wave; property: "scale"; from: 1; to: 2.2; duration: 1400; easing.type: Easing.OutCubic }
                                    NumberAnimation { target: wave; property: "opacity"; from: 0.8; to: 0; duration: 1400; easing.type: Easing.OutCubic }
                                }
                            }
                        }
                    }
                }
            }
        }

        // Линия «сканера», которая прорисовывает чертёж
        Rectangle {
            visible: root.drawProgress > 0 && root.drawProgress < 1
            x: reveal.width - 1
            y: 20
            width: 1.5
            height: 440
            gradient: Gradient {
                GradientStop { position: 0.0; color: "transparent" }
                GradientStop { position: 0.5; color: Theme.sketchLine }
                GradientStop { position: 1.0; color: "transparent" }
            }
        }

        // ---------- Выноски (координаты макета) ----------
        Repeater {
            model: [
                { name: "usb", ax: 300, ay: 54, line: "M 300 54 L 336 20 L 392 20", tx: 400, ty: 20, align: "left", text: "USB-C" },
                { name: "ps", ax: 291, ay: 232, line: "M 291 232 L 291 242 M 291 256 L 291 330", tx: 291, ty: 344, align: "center", text: "PS" },
                { name: "create", ax: 168, ay: 92, line: "M 168 92 L 130 24 L 74 24", tx: 66, ty: 24, align: "right", text: "CREATE" },
                { name: "lightbar", ax: 390, ay: 107, line: "M 390 107 L 446 34 L 500 34", tx: 508, ty: 34, align: "left", text: qsTr("СВЕТОВАЯ ПАНЕЛЬ") }
            ]
            delegate: Item {
                id: callout
                required property var modelData
                readonly property bool shown: root.callouts.indexOf(modelData.name) >= 0 && root.drawProgress >= 1

                width: 660
                height: 470
                opacity: shown ? 1 : 0
                Behavior on opacity { NumberAnimation { duration: Theme.animNormal } }

                Shape {
                    anchors.fill: parent
                    preferredRendererType: Shape.CurveRenderer
                    ShapePath {
                        strokeColor: Theme.textSecondary
                        strokeWidth: 1
                        fillColor: "transparent"
                        PathSvg { path: callout.modelData.line }
                    }
                }
                Rectangle {
                    // точка привязки
                    x: callout.modelData.ax - 2.5
                    y: callout.modelData.ay - 2.5
                    width: 5; height: 5; radius: 2.5
                    color: Theme.textPrimary
                }
                Text {
                    text: callout.modelData.text
                    color: Theme.textPrimary
                    font.family: Theme.fontFamily
                    font.pixelSize: 11
                    font.weight: Font.Medium
                    font.letterSpacing: 1.2
                    x: callout.modelData.align === "left" ? callout.modelData.tx
                     : callout.modelData.align === "right" ? callout.modelData.tx - implicitWidth
                     : callout.modelData.tx - implicitWidth / 2
                    y: callout.modelData.ty - implicitHeight / 2
                }
            }
        }
    }
}
