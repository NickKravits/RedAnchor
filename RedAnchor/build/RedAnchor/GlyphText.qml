pragma ComponentBehavior: Bound
import QtQuick

// Текст со значками кнопок геймпада вместо букв: «Нажмите {cross}, чтобы продолжить».
// Метки — названия кнопок ButtonGlyph: {cross} {circle} {square} {triangle} {ps} {options} …
// Переносится по словам; horizontalAlignment: Text.AlignHCenter центрирует блок текста.
Item {
    id: root

    property string text
    property color color: Theme.textSecondary
    property int pixelSize: Theme.fontBody
    property int weight: Font.Normal
    property int horizontalAlignment: Text.AlignLeft

    // Слова, каждое — список кусочков { glyph, text } (у «{cross},» — значок и запятая)
    readonly property var words: {
        const out = []
        for (const word of text.split(" ")) {
            if (word === "")
                continue
            const parts = []
            const re = /\{(\w+)\}/g
            let last = 0
            let m
            while ((m = re.exec(word)) !== null) {
                if (m.index > last)
                    parts.push({ glyph: "", text: word.slice(last, m.index) })
                parts.push({ glyph: m[1], text: "" })
                last = m.index + m[0].length
            }
            if (last < word.length)
                parts.push({ glyph: "", text: word.slice(last) })
            out.push(parts)
        }
        return out
    }

    implicitWidth: measure.implicitWidth
    implicitHeight: flow.implicitHeight

    TextMetrics {
        id: space
        font.family: Theme.fontFamily
        font.pixelSize: root.pixelSize
        text: " "
    }

    component Word: Row {
        id: word
        required property var modelData
        Repeater {
            model: word.modelData
            delegate: Item {
                id: part
                required property var modelData
                width: modelData.glyph !== "" ? glyph.implicitWidth + Theme.px(4) : label.implicitWidth
                height: Math.max(label.implicitHeight, glyph.implicitHeight)

                ButtonGlyph {
                    id: glyph
                    visible: part.modelData.glyph !== ""
                    button: part.modelData.glyph
                    size: Math.round(root.pixelSize * 1.3)
                    anchors.centerIn: parent
                }
                Text {
                    id: label
                    visible: part.modelData.glyph === ""
                    anchors.verticalCenter: parent.verticalCenter
                    text: part.modelData.text
                    color: root.color
                    font.family: Theme.fontFamily
                    font.pixelSize: root.pixelSize
                    font.weight: root.weight
                }
            }
        }
    }

    // Ширина текста в одну строку — чтобы центрировать короткий текст
    Row {
        id: measure
        visible: false
        spacing: space.advanceWidth
        Repeater { model: root.words; delegate: Word {} }
    }

    Flow {
        id: flow
        width: Math.min(measure.implicitWidth, root.width > 0 ? root.width : measure.implicitWidth)
        x: root.horizontalAlignment === Text.AlignHCenter ? (root.width - width) / 2
         : root.horizontalAlignment === Text.AlignRight ? root.width - width : 0
        spacing: space.advanceWidth
        Repeater { model: root.words; delegate: Word {} }
    }
}
