pragma ComponentBehavior: Bound
import QtQuick

// Вкладка «Магазин»: разделы RedStore, GOG.
// Фокус сначала попадает на панель разделов: ←/→ (или L2 / R2 откуда угодно) — магазин,
// ↓ или ✕ — внутрь раздела, ↑ или ○ из раздела — обратно к панели разделов.
FocusScope {
    id: root

    readonly property bool focusable: true
    // На весь экран (задаёт главный экран): разделы компактнее, фон однотонный
    property bool compact: false
    property int section: 0
    readonly property var sections: [
        { key: "redstore", title: "RedStore" },
        { key: "gog", title: "GOG" }
    ]

    signal webLoginRequested(string store)

    // L2 / R2 (вызывает главный экран) и ←/→ на панели разделов
    // enterContent — L2 / R2: фокус сразу на элементе раздела; ←/→ на панели разделов
    // оставляют фокус на ней
    function switchSection(delta, enterContent) {
        const next = Math.max(0, Math.min(sections.length - 1, section + delta))
        if (next === section)
            return
        const inPanel = activeFocus && !sectionBar.activeFocus
        section = next
        if (inPanel || enterContent)
            focusFirst()
        else if (activeFocus)
            sectionBar.forceActiveFocus()
    }

    // Главный экран вызывает при входе во вкладку: фокус — на элемент раздела
    // (кнопку, форму, игру), а если в разделе выбирать нечего — на панель разделов
    function focusFirst() {
        const item = content.item
        if (item && item.focusable)
            focusContent()
        else
            sectionBar.forceActiveFocus()
    }

    function focusContent() {
        const item = content.item
        if (!item || !item.focusable)
            return
        if (item.focusFirst)
            item.focusFirst()
        else
            (item as Item).forceActiveFocus()
    }

    // ↑ или ○ из раздела — к панели разделов; с самой панели ↑ уходит к вкладкам
    Keys.onUpPressed: (event) => {
        if (sectionBar.activeFocus) {
            event.accepted = false
            return
        }
        sectionBar.forceActiveFocus()
    }
    Keys.onEscapePressed: (event) => {
        if (sectionBar.activeFocus) {
            event.accepted = false
            return
        }
        sectionBar.forceActiveFocus()
    }

    // Тёмная подложка: на живом фоне без неё обложки и текст сливаются
    // (на весь экран фон и так однотонный)
    Rectangle {
        anchors.fill: parent
        anchors.margins: -Theme.px(32)
        opacity: root.compact ? 0 : 1
        Behavior on opacity { NumberAnimation { duration: Theme.animNormal } }
        radius: Theme.px(24)
        color: Qt.rgba(0.06, 0.068, 0.08, 0.82)
        border.width: 1
        border.color: Qt.rgba(1, 1, 1, 0.08)
    }

    // ---------- Разделы ----------
    FocusScope {
        id: sectionBar
        width: sectionRow.width
        height: sectionRow.height

        Keys.onLeftPressed: root.switchSection(-1)
        Keys.onRightPressed: root.switchSection(1)
        Keys.onDownPressed: root.focusContent()
        Keys.onReturnPressed: root.focusContent()
        Keys.onEnterPressed: root.focusContent()

    Row {
        id: sectionRow
        spacing: Theme.px(8)

        ButtonGlyph {
            button: "l2"
            size: Theme.px(26)
            anchors.verticalCenter: parent.verticalCenter
        }
        Item { width: Theme.px(6); height: 1 }

        Repeater {
            model: root.sections
            delegate: Item {
                id: chip
                required property var modelData
                required property int index
                readonly property bool selected: root.section === index
                readonly property bool focused: selected && sectionBar.activeFocus
                width: chipLabel.implicitWidth + (root.compact ? Theme.px(28) : Theme.px(40))
                height: root.compact ? Theme.px(38) : Theme.px(46)
                Behavior on height { NumberAnimation { duration: Theme.animNormal } }

                Rectangle {
                    anchors.fill: parent
                    radius: height / 2
                    // Как у главных вкладок: в фокусе — светлая «таблетка» с тёмным текстом
                    color: chip.focused ? Theme.focus : Qt.rgba(1, 1, 1, chip.selected ? 0.14 : 0)
                    border.width: chip.focused ? 0 : 1
                    border.color: chip.selected ? Theme.outlineStrong : Theme.outline
                    Behavior on color { ColorAnimation { duration: Theme.animFast } }
                }
                Text {
                    id: chipLabel
                    anchors.centerIn: parent
                    text: chip.modelData.title
                    color: chip.focused ? Theme.background : chip.selected ? Theme.textPrimary : Theme.textSecondary
                    font.family: Theme.fontFamily
                    font.pixelSize: root.compact ? Theme.fontCaption + Theme.px(2) : Theme.fontSmall
                    font.weight: chip.selected ? Font.Medium : Font.Normal
                }
                MouseArea {
                    anchors.fill: parent
                    onClicked: {
                        root.switchSection(chip.index - root.section)
                        sectionBar.forceActiveFocus()
                    }
                }
            }
        }

        Item { width: Theme.px(6); height: 1 }
        ButtonGlyph {
            button: "r2"
            size: Theme.px(26)
            anchors.verticalCenter: parent.verticalCenter
        }
    }
    }

    // ---------- Содержимое раздела ----------
    Loader {
        id: content
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: sectionBar.bottom
        anchors.topMargin: root.compact ? Theme.px(20) : Theme.px(30)
        anchors.bottom: parent.bottom
        sourceComponent: root.sections[root.section].key === "redstore" ? redStore : storePanel
        onLoaded: fade.restart()
        NumberAnimation { id: fade; target: content; property: "opacity"; from: 0; to: 1; duration: Theme.animNormal }
    }

    Component {
        id: redStore
        Item {
            readonly property bool focusable: false
            EmptyState {
                anchors.centerIn: parent
                anchors.verticalCenterOffset: -Theme.px(30)
                icon: "store"
                title: qsTr("RedStore скоро откроется")
                text: qsTr("Собственный магазин игр RedAnchor сейчас в разработке.")
            }
        }
    }
    Component {
        id: storePanel
        StorePanel {
            store: root.sections[root.section].key
            onWebLoginRequested: (store) => root.webLoginRequested(store)
        }
    }
}
