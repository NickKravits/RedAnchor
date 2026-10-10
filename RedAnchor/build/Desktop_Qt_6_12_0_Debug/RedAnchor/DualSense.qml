pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Shapes

// Векторная иллюстрация DualSense с анимируемыми элементами:
// световая панель, индикатор игрока, кабель USB-C, подсветка кнопок.
// Внутри — макет 600×400, наружу масштабируется по ширине.
Item {
    id: root

    // "off" | "breathe" (зарядка/ожидание) | "blink" (режим сопряжения) | "solid"
    property string lightbarMode: "off"
    property color lightbarColor: Theme.lightbarBlue
    property int playerLeds: 0
    // "hidden" | "unplugged" (висит над портом) | "plugged" | "hint" (зацикленная подсказка «вставьте кабель»)
    property string cableState: "hidden"
    // Кнопки, которые нужно подсветить пульсирующим кольцом: ["ps", "create"]
    property var highlighted: []

    property real lightbarLevel: 0

    // Короткая вспышка кнопки, когда её нажали на реальном контроллере.
    function flash(name) {
        flashName = name
        flashTimer.restart()
    }
    property string flashName: ""

    implicitWidth: 600
    implicitHeight: 400

    Timer {
        id: flashTimer
        interval: 220
        onTriggered: root.flashName = ""
    }

    // --- Анимации световой панели ---
    onLightbarModeChanged: updateLightbar()
    Component.onCompleted: updateLightbar()
    function updateLightbar() {
        breathe.stop()
        blink.stop()
        if (lightbarMode === "breathe")
            breathe.start()
        else if (lightbarMode === "blink")
            blink.start()
        else {
            levelAnim.to = lightbarMode === "solid" ? 1 : 0
            levelAnim.restart()
        }
    }
    NumberAnimation { id: levelAnim; target: root; property: "lightbarLevel"; duration: 300 }
    SequentialAnimation {
        id: breathe
        loops: Animation.Infinite
        NumberAnimation { target: root; property: "lightbarLevel"; to: 1; duration: 1300; easing.type: Easing.InOutSine }
        NumberAnimation { target: root; property: "lightbarLevel"; to: 0.15; duration: 1300; easing.type: Easing.InOutSine }
    }
    SequentialAnimation {
        id: blink
        loops: Animation.Infinite
        PropertyAction { target: root; property: "lightbarLevel"; value: 1 }
        PauseAnimation { duration: 90 }
        PropertyAction { target: root; property: "lightbarLevel"; value: 0.05 }
        PauseAnimation { duration: 120 }
        PropertyAction { target: root; property: "lightbarLevel"; value: 1 }
        PauseAnimation { duration: 90 }
        PropertyAction { target: root; property: "lightbarLevel"; value: 0.05 }
        PauseAnimation { duration: 650 }
    }

    Item {
        id: canvas
        width: 600
        height: 400
        scale: root.width / 600
        transformOrigin: Item.TopLeft

        function circle(cx, cy, r) {
            return "M " + (cx - r) + " " + cy + " A " + r + " " + r + " 0 1 0 " + (cx + r) + " " + cy
                 + " A " + r + " " + r + " 0 1 0 " + (cx - r) + " " + cy + " Z"
        }

        // Тень под контроллером
        Shape {
            anchors.fill: parent
            preferredRendererType: Shape.CurveRenderer
            ShapePath {
                strokeColor: "transparent"
                fillGradient: RadialGradient {
                    centerX: 300; centerY: 400; centerRadius: 270
                    focalX: 300; focalY: 400
                    GradientStop { position: 0.0; color: Qt.rgba(0, 0, 0, 0.55) }
                    GradientStop { position: 1.0; color: "transparent" }
                }
                PathSvg { path: "M 30 400 A 270 28 0 1 0 570 400 A 270 28 0 1 0 30 400 Z" }
            }
        }

        // Кабель — под корпусом, чтобы наконечник «уходил» в порт
        Item {
            id: cableHolder
            x: 300 - cable.width / 2
            opacity: root.cableState === "hidden" ? 0 : 1
            y: root.cableState === "plugged" ? 60 - cable.tipBottom : -220
            Behavior on opacity { NumberAnimation { duration: Theme.animNormal } }
            Behavior on y { NumberAnimation { duration: 900; easing.type: Easing.InOutCubic } }

            UsbCable {
                id: cable

                // Подсказка «вставьте кабель»: штекер подъезжает к порту и уходит обратно.
                SequentialAnimation on y {
                    id: hintAnim
                    running: root.cableState === "hint"
                    loops: Animation.Infinite
                    onRunningChanged: if (!running) cable.y = 0
                    PauseAnimation { duration: 500 }
                    NumberAnimation { to: 220 + 60 - cable.tipBottom - 14; duration: 1100; easing.type: Easing.InOutCubic }
                    NumberAnimation { to: 220 + 60 - cable.tipBottom; duration: 260; easing.type: Easing.OutQuad }
                    PauseAnimation { duration: 1100 }
                    NumberAnimation { to: 0; duration: 900; easing.type: Easing.InOutCubic }
                    PauseAnimation { duration: 400 }
                }
            }
        }

        Shape {
            id: body
            anchors.fill: parent
            preferredRendererType: Shape.CurveRenderer

            // Бамперы L1 / R1 (видны над верхней кромкой)
            ShapePath {
                strokeColor: "transparent"
                fillColor: "#2b2d35"
                PathSvg { path: "M 78 94 C 92 66 140 52 188 49 L 192 62 C 150 64 110 74 88 98 Z" }
                PathSvg { path: "M 522 94 C 508 66 460 52 412 49 L 408 62 C 450 64 490 74 512 98 Z" }
            }

            // Чёрное ядро корпуса
            ShapePath {
                strokeColor: "transparent"
                fillGradient: LinearGradient {
                    x1: 0; y1: 50; x2: 0; y2: 400
                    GradientStop { position: 0.0; color: "#2a2c34" }
                    GradientStop { position: 1.0; color: "#0e0f12" }
                }
                PathSvg {
                    path: "M 150 62 C 220 44 380 44 450 62 C 522 72 562 94 577 142 C 597 212 602 300 582 352 "
                        + "C 567 388 520 398 494 372 C 468 346 452 304 420 288 C 380 272 220 272 180 288 "
                        + "C 148 304 132 346 106 372 C 80 398 33 388 18 352 C -2 300 3 212 23 142 C 38 94 78 72 150 62 Z"
                }
            }

            // Белые боковые панели
            ShapePath {
                strokeColor: "#ffffff"
                strokeWidth: 1
                fillGradient: LinearGradient {
                    x1: 0; y1: 50; x2: 0; y2: 400
                    GradientStop { position: 0.0; color: "#fdfdfe" }
                    GradientStop { position: 0.7; color: "#e6e8ee" }
                    GradientStop { position: 1.0; color: "#cdd0d8" }
                }
                PathSvg {
                    path: "M 205 54 C 186 55 166 58 150 62 C 78 72 38 94 23 142 C 3 212 -2 300 18 352 "
                        + "C 33 388 80 398 106 372 C 112 366 117 358 122 350 C 132 320 146 286 160 262 "
                        + "C 176 236 190 204 196 160 L 205 54 Z"
                }
                PathSvg {
                    path: "M 395 54 C 414 55 434 58 450 62 C 522 72 562 94 577 142 C 597 212 602 300 582 352 "
                        + "C 567 388 520 398 494 372 C 488 366 483 358 478 350 C 468 320 454 286 440 262 "
                        + "C 424 236 410 204 404 160 L 395 54 Z"
                }
            }

            // Сенсорная панель
            ShapePath {
                strokeColor: "#c4c8d1"
                strokeWidth: 1.2
                fillGradient: LinearGradient {
                    x1: 0; y1: 50; x2: 0; y2: 160
                    GradientStop { position: 0.0; color: "#f6f7f9" }
                    GradientStop { position: 1.0; color: "#e1e4ea" }
                }
                PathSvg { path: "M 212 55 C 262 47 338 47 388 55 L 382 154 C 338 160 262 160 218 154 Z" }
            }

            // Порт USB-C на верхней кромке
            ShapePath {
                strokeColor: "#4a4d57"
                strokeWidth: 1
                fillColor: "#07080a"
                PathSvg { path: "M 289 46 L 311 46 Q 314 46 314 49 Q 314 52 311 52 L 289 52 Q 286 52 286 49 Q 286 46 289 46 Z" }
            }

            // Крестовина
            ShapePath {
                strokeColor: "#b3b8c4"
                strokeWidth: 1.2
                fillColor: "#e9ebf0"
                PathSvg { path: "M 102 140 L 118 140 Q 121 140 121 143 L 121 160 L 110 168 L 99 160 L 99 143 Q 99 140 102 140 Z" }
                PathSvg { path: "M 102 210 L 118 210 Q 121 210 121 207 L 121 190 L 110 182 L 99 190 L 99 207 Q 99 210 102 210 Z" }
                PathSvg { path: "M 75 164 L 75 186 Q 75 189 78 189 L 95 189 L 103 175 L 95 161 L 78 161 Q 75 161 75 164 Z" }
                PathSvg { path: "M 145 164 L 145 186 Q 145 189 142 189 L 125 189 L 117 175 L 125 161 L 142 161 Q 145 161 145 164 Z" }
            }

            // Кнопки действий
            ShapePath {
                strokeColor: "#b3b8c4"
                strokeWidth: 1.2
                fillColor: "#eceef2"
                PathSvg { path: canvas.circle(490, 145, 14) }
                PathSvg { path: canvas.circle(520, 175, 14) }
                PathSvg { path: canvas.circle(490, 205, 14) }
                PathSvg { path: canvas.circle(460, 175, 14) }
            }
            ShapePath {
                strokeColor: "#8d93a1"
                strokeWidth: 1.8
                fillColor: "transparent"
                capStyle: ShapePath.RoundCap
                joinStyle: ShapePath.RoundJoin
                PathSvg { path: "M 490 138.5 L 496 149 L 484 149 Z" }               // △
                PathSvg { path: canvas.circle(520, 175, 6) }                        // ○
                PathSvg { path: "M 485 200 L 495 210 M 495 200 L 485 210" }         // ✕
                PathSvg { path: "M 455 170 L 465 170 L 465 180 L 455 180 Z" }       // □
            }

            // Create / Options
            ShapePath {
                strokeColor: "#9aa0ad"
                strokeWidth: 1
                fillColor: "#3a3d47"
                PathSvg { path: "M 181 64 Q 184 61 187 63 L 192 82 Q 193 86 189 87 Q 186 88 185 84 L 180 67 Q 179 65 181 64 Z" }
                PathSvg { path: "M 419 64 Q 416 61 413 63 L 408 82 Q 407 86 411 87 Q 414 88 415 84 L 420 67 Q 421 65 419 64 Z" }
            }

            // Стики
            ShapePath {
                strokeColor: "transparent"
                fillColor: "#08090b"
                PathSvg { path: canvas.circle(210, 252, 36) }
                PathSvg { path: canvas.circle(390, 252, 36) }
            }
            ShapePath {
                strokeColor: "#3c3f49"
                strokeWidth: 1.5
                fillGradient: RadialGradient {
                    centerX: 300; centerY: 240; centerRadius: 200
                    focalX: 300; focalY: 240
                    GradientStop { position: 0.0; color: "#3a3d46" }
                    GradientStop { position: 1.0; color: "#1d1f25" }
                }
                PathSvg { path: canvas.circle(210, 252, 28) }
                PathSvg { path: canvas.circle(390, 252, 28) }
            }
            ShapePath {
                strokeColor: "#16171c"
                strokeWidth: 2
                fillColor: "transparent"
                PathSvg { path: canvas.circle(210, 252, 20) }
                PathSvg { path: canvas.circle(390, 252, 20) }
            }

            // Кнопка PS и кнопка микрофона
            ShapePath {
                strokeColor: "#6b6f7b"
                strokeWidth: 1.2
                fillColor: "#1b1c21"
                PathSvg { path: canvas.circle(300, 262, 11) }
                PathSvg { path: "M 289 289 L 311 289 Q 315 289 315 293 Q 315 297 311 297 L 289 297 Q 285 297 285 293 Q 285 289 289 289 Z" }
            }
        }

        // Решётка динамика под сенсорной панелью
        Grid {
            x: 284; y: 192
            columns: 6
            spacing: 3
            Repeater {
                model: 18
                Rectangle { width: 2.6; height: 2.6; radius: 1.3; color: "#050506" }
            }
        }

        // Индикаторы игрока
        Row {
            x: 280; y: 164
            spacing: 6
            Repeater {
                model: 5
                Rectangle {
                    required property int index
                    width: 3; height: 3; radius: 1.5
                    // для игрока 1 горит центральный
                    color: root.playerLeds >= 1 && index === 2 ? "#ffffff" : "#b9bcc6"
                    opacity: root.playerLeds >= 1 && index === 2 ? 1 : 0.5
                }
            }
        }

        // Световая панель: широкое свечение + яркая сердцевина
        Shape {
            anchors.fill: parent
            preferredRendererType: Shape.CurveRenderer
            opacity: Math.max(0.12, root.lightbarLevel)

            ShapePath {
                strokeColor: Qt.rgba(root.lightbarColor.r, root.lightbarColor.g, root.lightbarColor.b, 0.28 * root.lightbarLevel)
                strokeWidth: 16
                fillColor: "transparent"
                capStyle: ShapePath.RoundCap
                PathSvg { path: "M 208 62 C 205 95 203 125 201 152" }
                PathSvg { path: "M 392 62 C 395 95 397 125 399 152" }
            }
            ShapePath {
                strokeColor: root.lightbarLevel > 0.02 ? Qt.lighter(root.lightbarColor, 1.0 + 0.4 * root.lightbarLevel) : "#9ea3ae"
                strokeWidth: 3.5
                fillColor: "transparent"
                capStyle: ShapePath.RoundCap
                PathSvg { path: "M 208 62 C 205 95 203 125 201 152" }
                PathSvg { path: "M 392 62 C 395 95 397 125 399 152" }
            }
        }

        // Пульсирующие кольца вокруг кнопок-подсказок и вспышки нажатий
        Repeater {
            model: [
                { name: "cross", x: 490, y: 205, r: 15 }, { name: "circle", x: 520, y: 175, r: 15 },
                { name: "square", x: 460, y: 175, r: 15 }, { name: "triangle", x: 490, y: 145, r: 15 },
                { name: "up", x: 110, y: 153, r: 15 }, { name: "down", x: 110, y: 197, r: 15 },
                { name: "left", x: 88, y: 175, r: 15 }, { name: "right", x: 132, y: 175, r: 15 },
                { name: "ps", x: 300, y: 262, r: 13 }, { name: "create", x: 186, y: 75, r: 15 },
                { name: "options", x: 414, y: 75, r: 15 }, { name: "l3", x: 210, y: 252, r: 37 },
                { name: "r3", x: 390, y: 252, r: 37 }, { name: "l1", x: 132, y: 66, r: 24 },
                { name: "r1", x: 468, y: 66, r: 24 }
            ]
            delegate: Item {
                id: marker
                required property var modelData
                readonly property bool hinted: root.highlighted.indexOf(modelData.name) >= 0
                readonly property bool flashing: root.flashName === modelData.name

                x: modelData.x
                y: modelData.y

                Rectangle {
                    id: ring
                    anchors.centerIn: parent
                    width: marker.modelData.r * 2 + 6
                    height: width
                    radius: width / 2
                    color: marker.flashing ? Qt.rgba(1, 1, 1, 0.35) : "transparent"
                    border.width: 3
                    border.color: marker.flashing ? "#ffffff" : Theme.accent
                    visible: marker.hinted || marker.flashing
                }
                Rectangle {
                    // расходящаяся волна
                    anchors.centerIn: parent
                    width: ring.width
                    height: width
                    radius: width / 2
                    color: "transparent"
                    border.width: 2
                    border.color: Theme.accent
                    visible: marker.hinted
                    SequentialAnimation on scale {
                        running: marker.hinted
                        loops: Animation.Infinite
                        NumberAnimation { from: 1; to: 1.9; duration: 1100; easing.type: Easing.OutCubic }
                    }
                    SequentialAnimation on opacity {
                        running: marker.hinted
                        loops: Animation.Infinite
                        NumberAnimation { from: 0.9; to: 0; duration: 1100; easing.type: Easing.OutCubic }
                    }
                }
            }
        }
    }
}
