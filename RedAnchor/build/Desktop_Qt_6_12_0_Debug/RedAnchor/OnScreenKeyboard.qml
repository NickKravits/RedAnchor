pragma ComponentBehavior: Bound
import QtQuick

// Экранная клавиатура, рассчитанная на геймпад:
//   D-pad/стик — выбор клавиши, ✕ — нажать, □ — стереть, △ — пробел,
//   L1 — Shift, R1 — смена языка, L2 — символы, Options — готово, ○ — закрыть.
// Работает и с мышью, и с физической клавиатурой (печатаемые символы вводятся напрямую).
FocusScope {
    id: root

    readonly property ConsoleTextField target: Keyboard.target
    readonly property bool shown: Keyboard.active

    property string layout: "ru"        // "ru" | "en"
    property bool symbols: false
    property int shift: 0               // 0 — нет, 1 — одна буква, 2 — Caps Lock
    property int curRow: 1
    property int curCol: 0

    readonly property var hints: [
        { button: "options", text: qsTr("Готово") },
        { button: "r1", text: qsTr("Язык") },
        { button: "l1", text: "Shift" },
        { button: "triangle", text: qsTr("Пробел") },
        { button: "square", text: qsTr("Стереть") },
        { button: "cross", text: qsTr("Ввод") }
    ]

    readonly property var letterRows: ({
        "ru": ["йцукенгшщзхъ", "фывапролджэё", "ячсмитьбю"],
        "en": ["qwertyuiop", "asdfghjkl", "zxcvbnm"]
    })
    readonly property var symbolRows: ["!@#$%^&*()", "-_=+[]{};:", "'\"/?\\|~`<>"]

    readonly property var rows: buildRows(layout, symbols, shift)
    readonly property real maxUnits: 12.7
    readonly property real unit: Math.min(Theme.px(118), (width - Theme.gutter * 2) / maxUnits)
    readonly property real keyHeight: Theme.px(72)
    readonly property real keyGap: Theme.px(8)

    height: panel.height
    visible: y < parent.height
    y: shown ? parent.height - height : parent.height
    Behavior on y { NumberAnimation { duration: Theme.animNormal; easing.type: Easing.OutCubic } }

    onShownChanged: {
        if (!shown)
            return
        const hint = target ? target.inputHint : "text"
        layout = hint === "text" ? "ru" : "en"
        symbols = false
        shift = (hint === "text" && target && target.text === "") ? 1 : 0
        curRow = 1
        curCol = 0
        forceActiveFocus()
    }

    function charKey(c) { return { label: c, value: c, w: 1 } }

    function buildRows(lay, sym, sh) {
        const result = []
        result.push("1234567890".split("").map(charKey))
        const src = sym ? symbolRows : letterRows[lay]
        for (let i = 0; i < src.length; ++i) {
            let keys = src[i].split("").map(c => charKey(sh > 0 && !sym ? c.toUpperCase() : c))
            if (i === src.length - 1) {
                if (!sym)
                    keys.unshift({ label: "", action: "shift", w: 1.5, glyph: "l1" })
                keys.push({ label: "", action: "backspace", w: 1.5, glyph: "square" })
            }
            result.push(keys)
        }
        result.push([
            { label: sym ? (lay === "ru" ? "АБВ" : "ABC") : "?123", action: "symbols", w: 1.6, glyph: "l2" },
            { label: lay === "ru" ? "RU" : "EN", action: "lang", w: 1.4, glyph: "r1" },
            charKey(","),
            { label: qsTr("Пробел"), action: "space", w: 4.5, glyph: "triangle" },
            charKey("."),
            charKey("@"),
            { label: qsTr("Готово"), action: "done", w: 2.2, glyph: "options" }
        ])
        return result
    }

    function rowUnits(r) {
        return rows[r].reduce((sum, k) => sum + k.w, 0)
    }

    function keyCenter(r, c) {
        let x = (maxUnits - rowUnits(r)) / 2
        for (let i = 0; i < c; ++i)
            x += rows[r][i].w
        return x + rows[r][c].w / 2
    }

    function moveVertical(delta) {
        const target = curRow + delta
        if (target < 0 || target >= rows.length)
            return
        const cx = keyCenter(curRow, curCol)
        let best = 0, bestDist = 1e9
        for (let c = 0; c < rows[target].length; ++c) {
            const d = Math.abs(keyCenter(target, c) - cx)
            if (d < bestDist) { bestDist = d; best = c }
        }
        curRow = target
        curCol = best
    }

    function moveHorizontal(delta) {
        const n = rows[curRow].length
        curCol = (curCol + delta + n) % n
    }

    function insert(text) {
        if (!target)
            return
        target.text += text
        if (shift === 1)
            shift = 0
    }

    function backspace() {
        if (target && target.text.length > 0)
            target.text = target.text.slice(0, -1)
    }

    function toggleLayout() {
        layout = layout === "ru" ? "en" : "ru"
        symbols = false
        clampCursor()
    }

    function toggleSymbols() {
        symbols = !symbols
        clampCursor()
    }

    function clampCursor() {
        curRow = Math.min(curRow, rows.length - 1)
        curCol = Math.min(curCol, rows[curRow].length - 1)
    }

    function activate(key) {
        switch (key.action) {
        case "shift": shift = (shift + 1) % 3; break
        case "backspace": backspace(); break
        case "space": insert(" "); break
        case "symbols": toggleSymbols(); break
        case "lang": toggleLayout(); break
        case "done": Keyboard.close(true); break
        default: insert(key.value)
        }
    }

    Keys.onPressed: (event) => {
        switch (event.key) {
        case Qt.Key_Up: moveVertical(-1); break
        case Qt.Key_Down: moveVertical(1); break
        case Qt.Key_Left: moveHorizontal(-1); break
        case Qt.Key_Right: moveHorizontal(1); break
        case Qt.Key_Return:
        case Qt.Key_Enter:
            // Enter с физической клавиатуры (у событий геймпада text пустой) сразу подтверждает ввод
            if (event.text === "\r") Keyboard.close(true)
            else activate(rows[curRow][curCol])
            break
        case Qt.Key_Backspace: backspace(); break
        case Qt.Key_Space: insert(" "); break
        case Qt.Key_PageUp: shift = shift === 0 ? 1 : 0; break
        case Qt.Key_PageDown: toggleLayout(); break
        case Qt.Key_Home: toggleSymbols(); break
        case Qt.Key_Menu: Keyboard.close(true); break
        case Qt.Key_Escape: Keyboard.close(false); break
        default:
            if (event.text.length === 1 && event.text >= " ") {
                insert(event.text)
                break
            }
            return
        }
        event.accepted = true
    }

    Rectangle {
        id: panel
        width: root.width
        height: content.height + Theme.px(140)
        color: "#0b0d14"
        opacity: 0.98

        Rectangle {
            width: parent.width
            height: Theme.px(2)
            color: Theme.accent
            opacity: 0.8
        }

        Column {
            id: content
            anchors.horizontalCenter: parent.horizontalCenter
            y: Theme.px(28)
            spacing: Theme.px(22)

            // Предпросмотр вводимого текста (поле может быть скрыто клавиатурой)
            Column {
                width: root.unit * root.maxUnits
                spacing: Theme.px(6)

                Text {
                    text: root.target ? root.target.label : ""
                    color: Theme.textSecondary
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontSmall
                }
                Row {
                    spacing: Theme.px(2)
                    height: Theme.px(44)
                    Text {
                        anchors.verticalCenter: parent.verticalCenter
                        width: Math.min(implicitWidth, root.unit * root.maxUnits - Theme.px(20))
                        elide: Text.ElideLeft
                        text: !root.target ? "" : root.target.password ? "•".repeat(root.target.text.length) : root.target.text
                        color: Theme.textPrimary
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontLarge
                    }
                    Rectangle {
                        anchors.verticalCenter: parent.verticalCenter
                        width: Theme.px(3)
                        height: Theme.px(36)
                        color: Theme.accent
                        SequentialAnimation on opacity {
                            running: root.shown
                            loops: Animation.Infinite
                            NumberAnimation { to: 0; duration: 450 }
                            NumberAnimation { to: 1; duration: 450 }
                        }
                    }
                }
            }

            Column {
                spacing: root.keyGap

                Repeater {
                    model: root.rows.length
                    delegate: Row {
                        id: keyRow
                        required property int index
                        readonly property int rowIndex: index
                        anchors.horizontalCenter: parent.horizontalCenter
                        spacing: 0

                        Repeater {
                            model: root.rows[keyRow.rowIndex]
                            delegate: Item {
                                id: keyItem
                                required property var modelData
                                required property int index
                                readonly property bool current: root.curRow === keyRow.rowIndex && root.curCol === index
                                readonly property bool special: modelData.action !== undefined

                                width: root.unit * modelData.w
                                height: root.keyHeight
                                z: current ? 1 : 0

                                Rectangle {
                                    anchors.fill: parent
                                    anchors.margins: root.keyGap / 2
                                    radius: Theme.px(10)
                                    scale: keyItem.current ? 1.06 : 1
                                    color: keyItem.current ? Theme.textPrimary
                                         : keyItem.modelData.action === "done" ? Theme.accent
                                         : (keyItem.modelData.action === "shift" && root.shift > 0) ? "#3a4060"
                                         : keyItem.special ? Theme.surfaceRaised : Theme.surface
                                    Behavior on scale { NumberAnimation { duration: 90 } }

                                    Text {
                                        anchors.centerIn: parent
                                        visible: keyItem.modelData.label !== ""
                                        text: keyItem.modelData.label
                                        color: keyItem.current ? Theme.background : Theme.textPrimary
                                        font.family: Theme.fontFamily
                                        font.pixelSize: keyItem.special ? Theme.fontSmall : Theme.fontBody
                                        font.weight: keyItem.special ? Font.DemiBold : Font.Normal
                                    }
                                    Icon {
                                        anchors.centerIn: parent
                                        visible: keyItem.modelData.action === "shift" || keyItem.modelData.action === "backspace"
                                        name: keyItem.modelData.action === "backspace" ? "backspace"
                                                                                       : (root.shift === 2 ? "caps" : "shift")
                                        color: keyItem.current ? Theme.background : Theme.textPrimary
                                        size: Theme.px(30)
                                    }
                                    ButtonGlyph {
                                        visible: keyItem.modelData.glyph !== undefined && GamepadManager.connected
                                        button: keyItem.modelData.glyph || "cross"
                                        size: Theme.px(20)
                                        anchors.right: parent.right
                                        anchors.top: parent.top
                                        anchors.margins: Theme.px(5)
                                        opacity: 0.85
                                    }

                                    MouseArea {
                                        anchors.fill: parent
                                        onClicked: {
                                            root.curRow = keyRow.rowIndex
                                            root.curCol = keyItem.index
                                            root.activate(keyItem.modelData)
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}
