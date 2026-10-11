pragma ComponentBehavior: Bound
import QtQuick

// Поиск игр сразу в GOG и Epic Games Store. Текст вводится экранной клавиатурой
// (открывается сама при входе на страницу), результаты — сеткой карточек.
// ↓ из поля — к результатам, ↑ из первого ряда — к полю, △ — новый поиск.
FocusScope {
    id: root

    readonly property var hints: [
        { button: "circle", text: qsTr("Назад") },
        { button: "triangle", text: qsTr("Новый поиск") },
        { button: "cross", text: qsTr("Выбрать") }
    ]

    signal activated(var item)

    // Вход на страницу: сразу клавиатура
    function start() {
        field.text = StoreCatalog.searchText
        field.forceActiveFocus()
        Keyboard.open(field)
    }

    function focusFirst() {
        if (grid.count > 0)
            grid.forceActiveFocus()
        else
            field.forceActiveFocus()
    }

    Keys.onSpacePressed: start()

    // ---------- Поле поиска ----------
    Row {
        id: searchRow
        spacing: Theme.px(24)

        ConsoleTextField {
            id: field
            width: Math.min(root.width * 0.55, Theme.px(900))
            placeholder: qsTr("Название игры")
            focus: true
            KeyNavigation.down: grid.count > 0 ? grid : null
            onAccepted: StoreCatalog.search(field.text)
        }
        Row {
            anchors.verticalCenter: parent.verticalCenter
            spacing: Theme.px(14)
            Spinner {
                visible: StoreCatalog.searching
                size: Theme.px(30)
                anchors.verticalCenter: parent.verticalCenter
            }
            Text {
                anchors.verticalCenter: parent.verticalCenter
                text: StoreCatalog.searching ? qsTr("Ищем в GOG и Epic Games…")
                    : StoreCatalog.searchText !== "" && grid.count > 0 ? qsTr("Найдено: %1").arg(grid.count) : ""
                color: Theme.textSecondary
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontSmall
            }
        }
    }

    // ---------- Пустые состояния ----------
    EmptyState {
        visible: grid.count === 0 && !StoreCatalog.searching
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.top: searchRow.bottom
        anchors.topMargin: Theme.px(70)
        icon: StoreCatalog.searchError !== "" ? "warning" : "search"
        title: StoreCatalog.searchError !== "" ? qsTr("Поиск не удался")
             : StoreCatalog.searchText !== "" ? qsTr("Ничего не найдено")
             : qsTr("Поиск по магазинам")
        text: StoreCatalog.searchError !== "" ? StoreCatalog.searchError
            : StoreCatalog.searchText !== "" ? qsTr("По запросу «%1» игр нет. Попробуйте другое название.").arg(StoreCatalog.searchText)
            : qsTr("Введите название — найдём игру в GOG и Epic Games Store.")
    }

    // ---------- Результаты ----------
    GridView {
        id: grid
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: searchRow.bottom
        anchors.topMargin: Theme.px(30)
        anchors.bottom: parent.bottom
        readonly property int columns: Math.max(2, Math.floor(width / Theme.px(400)))
        cellWidth: Math.floor(width / columns)
        cellHeight: Math.round((cellWidth - Theme.px(32)) * 9 / 16) + Theme.px(118)
        clip: true
        keyNavigationEnabled: true
        highlightMoveDuration: Theme.animFast
        highlightRangeMode: GridView.ApplyRange
        preferredHighlightBegin: Theme.px(10)
        preferredHighlightEnd: height - cellHeight
        boundsBehavior: Flickable.StopAtBounds
        topMargin: Theme.px(24)
        model: StoreCatalog.searchResults.length > 0 ? StoreCatalog.searchResults
             : StoreCatalog.searching ? 8 : 0

        KeyNavigation.up: field
        KeyNavigation.priority: KeyNavigation.AfterItem

        function openCurrent() {
            const item = StoreCatalog.searchResults[currentIndex]
            if (item)
                root.activated(item)
        }
        Keys.onReturnPressed: openCurrent()
        Keys.onEnterPressed: openCurrent()

        delegate: Item {
            id: cell
            required property int index
            required property var modelData
            width: grid.cellWidth
            height: grid.cellHeight

            StoreTile {
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: parent.top
                anchors.margins: Theme.px(16)
                item: typeof cell.modelData === "object" ? cell.modelData : null
                showStore: true
                active: cell.GridView.isCurrentItem && grid.activeFocus
            }
            MouseArea {
                anchors.fill: parent
                onClicked: {
                    grid.currentIndex = cell.index
                    grid.forceActiveFocus()
                    if (typeof cell.modelData === "object")
                        root.activated(cell.modelData)
                }
            }
        }
    }

    // Пришли результаты, пока клавиатура закрыта и фокус в поле, — переходим к ним
    Connections {
        target: StoreCatalog
        function onSearchChanged() {
            if (!StoreCatalog.searching && field.activeFocus && !Keyboard.active && StoreCatalog.searchResults.length > 0)
                grid.forceActiveFocus()
        }
    }
}
