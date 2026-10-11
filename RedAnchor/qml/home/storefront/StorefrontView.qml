pragma ComponentBehavior: Bound
import QtQuick
import QtCore
import "Format.js" as Format
import "ModelSync.js" as ModelSync

// Раздел «Витрина» вкладки «Магазин»: витрина GOG и Epic Games Store без входа в магазин.
//   Сверху — источник каталога («Каталог · GOG», ✕ или □ — другой магазин), «Поиск» и «Активировать ключ»;
//   ниже — большая карусель и полки («Скидки», «Новинки», «Бесплатно в Epic»).
// ↑/↓ — между полками, ←/→ — по играм, ✕ — страница игры, □ — другой каталог, △ — поиск,
// ○ — назад (со страницы игры и поиска — к витрине).
// Купить игру можно на сайте магазина во встроенном браузере (StoreBrowserWindow.qml) или
// на телефоне по QR-коду; после покупки купленные игры магазина обновляются сами.
FocusScope {
    id: root

    readonly property bool focusable: true
    property string store: settings.store
    // "home" | "search" | "details"
    property string page: "home"
    // Куда вернуться со страницы игры
    property string detailsFrom: "home"
    readonly property var state_: StoreCatalog.home[store] || ({})

    readonly property var hints: page === "details" ? details.hints
                               : page === "search" ? search.hints
                               : [
        { button: "circle", text: qsTr("Назад") },
        { button: "square", text: qsTr("Сменить каталог") },
        { button: "triangle", text: qsTr("Поиск") },
        { button: "cross", text: qsTr("Выбрать") }
    ]

    Settings {
        id: settings
        category: "Storefront"
        property string store: "gog"
    }

    // ---------- Фокус ----------
    function focusFirst() {
        if (page === "details")
            details.focusFirst()
        else if (page === "search")
            search.focusFirst()
        else if (rowsModel.count > 0 && !emptyState.visible)
            rowsView.forceActiveFocus()
        else
            header.focusFirst()
    }

    function setStore(key) {
        if (key === store)
            return
        store = key
        settings.store = key
        rowsView.currentIndex = 0
        StoreCatalog.load(key)
        syncRows()
    }

    function nextStore() {
        const list = StoreCatalog.stores
        setStore(list[(list.indexOf(store) + 1) % list.length])
    }

    // ---------- Страницы ----------
    function openDetails(item) {
        if (!item)
            return
        detailsFrom = page === "search" ? "search" : "home"
        StoreCatalog.openDetails(item)
        page = "details"
        details.focusFirst()
    }

    function openSearch() {
        page = "search"
        search.start()
    }

    function back() {
        if (page === "details") {
            page = detailsFrom
            StoreCatalog.closeDetails()
            if (page === "search")
                search.focusFirst()
            else
                rowsView.forceActiveFocus()
            return true
        }
        if (page === "search") {
            page = "home"
            focusFirst()
            return true
        }
        return false
    }

    // ○ — назад внутри витрины; на самой витрине — как обычно (к разделам магазина)
    Keys.onEscapePressed: (event) => {
        event.accepted = root.back()
    }
    Keys.onPressed: (event) => {
        if (root.page !== "home")
            return
        if (event.key === Qt.Key_Space) {           // △
            root.openSearch()
            event.accepted = true
        } else if (event.key === Qt.Key_Backspace) { // □
            root.nextStore()
            event.accepted = true
        }
    }

    // ---------- Покупка и активация ключа ----------
    Component {
        id: browserComponent
        StoreBrowserWindow {}
    }
    // Магазину нужно время, чтобы новая покупка попала в список игр аккаунта: проверяем ещё раз
    Timer {
        id: lateRefresh
        property string store
        interval: 20000
        onTriggered: StoreCatalog.refreshOwned(store)
    }

    function openBrowser(store, url, heading) {
        if (!url)
            return
        const main = root.Window.window
        const kiosk = main && main.kiosk === true
        const returnFocus = root.Window.activeFocusItem
        const browser = browserComponent.createObject(null, {
            store: store, startUrl: url, heading: heading,
            // Под cage у окна не должно быть родителя: иначе это «диалог», а не окно на весь экран
            fullscreen: main && main.fullscreen === true && !kiosk,
            transientParent: kiosk ? null : main,
            width: main ? main.width : 1280, height: main ? main.height : 720
        })
        browser.finished.connect(() => {
            // Сначала прячем: скрытое окно больше не получает нажатий геймпада
            browser.hide()
            browser.destroy()
            if (main)
                main.requestActivate()
            if (returnFocus)
                returnFocus.forceActiveFocus()
            StoreCatalog.refreshOwned(store)
            lateRefresh.store = store
            lateRefresh.restart()
        })
        browser.show()
    }

    function buy(item) {
        openBrowser(item.store, item.checkoutUrl || item.url, qsTr("%1 — %2").arg(Format.storeName(item.store)).arg(item.title))
    }

    function redeem(returnFocus) {
        const name = Format.storeName(root.store)
        const url = StoreCatalog.redeemUrl(root.store)
        menu.show(qsTr("Активировать ключ %1").arg(name),
                  qsTr("Ключ вводится на официальной странице %1 — войдите там в свой аккаунт. Игра появится в библиотеке консоли.").arg(name), [
            { text: qsTr("Открыть страницу активации"), icon: "globe", run: () => root.openBrowser(root.store, url, qsTr("Активация ключа %1").arg(name)) },
            { text: qsTr("Активировать на телефоне"), icon: "arrowRight", run: () => phone.show(root.store, url, null, returnFocus) },
            { text: qsTr("Отмена") }
        ], returnFocus)
    }

    ActionMenu { id: menu }
    StorePhoneDialog { id: phone }

    // ---------- Данные ----------
    // Полки: ListModel, чтобы обновление витрины не сбрасывало выбранную полку
    ListModel { id: rowsModel }

    function rowOf(key) {
        const rows = root.state_.rows || []
        for (const r of rows) {
            if (r.key === key)
                return r
        }
        return null
    }

    // Карусель: текущие раздачи Epic, затем популярное
    function heroItems() {
        const featured = rowOf("featured")
        const free = rowOf("free")
        let items = []
        if (free)
            items = free.items.filter(i => i.promo === "now")
        if (featured)
            items = items.concat(featured.items)
        return items.slice(0, 8)
    }

    function syncRows() {
        const rows = root.state_.rows || []
        const list = []
        const featured = rowOf("featured")
        if (!featured || featured.loading || featured.items.length > 0 || heroItems().length > 0)
            list.push({ key: "hero", kind: "hero", title: "" })
        for (const r of rows) {
            if (r.key === "featured")
                continue
            if (r.loading || r.items.length > 0)
                list.push({ key: r.key, kind: "shelf", title: r.title })
        }
        ModelSync.sync(rowsModel, list, "key")
    }

    Connections {
        target: StoreCatalog
        function onHomeChanged() { root.syncRows() }
    }
    Component.onCompleted: {
        // Обе витрины грузим сразу: переключение магазина — без ожидания
        for (const key of StoreCatalog.stores)
            StoreCatalog.load(key)
        syncRows()
    }

    // ---------- Верхняя строка: магазин, поиск, ключ ----------
    FocusScope {
        id: header
        anchors.left: parent.left
        anchors.right: parent.right
        height: Theme.px(64)
        opacity: root.page === "details" ? 0 : 1
        visible: opacity > 0
        Behavior on opacity { NumberAnimation { duration: Theme.animNormal } }

        function focusFirst() {
            storeChips.forceActiveFocus()
        }

        Keys.onDownPressed: (event) => {
            if (root.page === "search")
                search.focusFirst()
            else if (emptyState.visible)
                retryButton.forceActiveFocus()
            else if (rowsModel.count > 0)
                rowsView.forceActiveFocus()
            else
                event.accepted = false
        }

        Row {
            id: headerRow
            spacing: Theme.px(16)
            anchors.verticalCenter: parent.verticalCenter

            // Источник каталога: «КАТАЛОГ · GOG ⇄». ✕ (или □ откуда угодно на витрине) —
            // другой магазин. Это не разделы магазина — те выше, в панели разделов.
            FocusScope {
                id: storeChips
                width: sourceRow.implicitWidth + Theme.px(64)
                height: Theme.controlHeight
                anchors.verticalCenter: parent.verticalCenter
                focus: true
                visible: root.page === "home"
                scale: activeFocus ? Theme.focusScale : 1
                Behavior on scale { NumberAnimation { duration: Theme.animFast; easing.type: Theme.easeOut } }

                Keys.onReturnPressed: root.nextStore()
                Keys.onEnterPressed: root.nextStore()
                Keys.onRightPressed: searchButton.forceActiveFocus()

                FocusRing {
                    shown: storeChips.activeFocus
                    radius: storeChips.height / 2
                }
                Rectangle {
                    anchors.fill: parent
                    radius: height / 2
                    color: storeChips.activeFocus ? Theme.focus : Theme.glass
                    border.width: storeChips.activeFocus ? 0 : 1
                    border.color: Theme.glassBorder
                    antialiasing: true
                    Behavior on color { ColorAnimation { duration: Theme.animFast } }
                }
                Row {
                    id: sourceRow
                    anchors.centerIn: parent
                    spacing: Theme.spacingS

                    Text {
                        anchors.verticalCenter: parent.verticalCenter
                        text: qsTr("Каталог")
                        color: storeChips.activeFocus ? Theme.textOnFocus : Theme.textMuted
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontCaption
                        font.weight: Font.DemiBold
                        font.capitalization: Font.AllUppercase
                        font.letterSpacing: Theme.trackingCaps
                    }
                    Text {
                        anchors.verticalCenter: parent.verticalCenter
                        text: Format.storeName(root.store)
                        color: storeChips.activeFocus ? Theme.textOnFocus : Theme.textPrimary
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontBody
                        font.weight: Font.Medium
                    }
                    Icon {
                        anchors.verticalCenter: parent.verticalCenter
                        name: "swap"
                        size: Theme.px(24)
                        lineWidth: 1.6
                        color: storeChips.activeFocus ? Theme.textOnFocus : Theme.textSecondary
                    }
                }
                MouseArea {
                    anchors.fill: parent
                    onClicked: {
                        storeChips.forceActiveFocus()
                        root.nextStore()
                    }
                }
            }

            // На странице поиска вместо магазинов — заголовок
            Text {
                visible: root.page === "search"
                anchors.verticalCenter: parent.verticalCenter
                text: qsTr("Поиск игр")
                color: Theme.textPrimary
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontLarge
                font.weight: Font.Light
            }

            ConsoleButton {
                id: searchButton
                visible: root.page === "home"
                anchors.verticalCenter: parent.verticalCenter
                text: qsTr("Поиск")
                icon: "search"
                KeyNavigation.left: storeChips
                KeyNavigation.right: redeemButton
                onClicked: root.openSearch()
            }
            ConsoleButton {
                id: redeemButton
                visible: root.page === "home"
                anchors.verticalCenter: parent.verticalCenter
                text: qsTr("Активировать ключ")
                icon: "plus"
                KeyNavigation.left: searchButton
                onClicked: root.redeem(redeemButton)
            }
        }

        // Состояние витрины: загрузка, нет сети
        Row {
            anchors.right: parent.right
            anchors.verticalCenter: parent.verticalCenter
            spacing: Theme.px(12)
            visible: root.page === "home"
            Spinner {
                visible: root.state_.loading === true
                size: Theme.px(26)
                anchors.verticalCenter: parent.verticalCenter
            }
            Icon {
                visible: root.state_.offline === true && root.state_.loading !== true
                name: "warning"
                size: Theme.px(24)
                lineWidth: 1.5
                color: Theme.warning
                anchors.verticalCenter: parent.verticalCenter
            }
            Text {
                anchors.verticalCenter: parent.verticalCenter
                text: root.state_.loading === true ? qsTr("Обновляем витрину…")
                    : root.state_.offline === true ? qsTr("Нет связи с магазином — показана сохранённая витрина")
                    : ""
                color: Theme.textSecondary
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontSmall
            }
        }
    }

    // ---------- Витрина ----------
    Item {
        id: home
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: header.bottom
        anchors.topMargin: Theme.px(26)
        anchors.bottom: parent.bottom
        opacity: root.page === "home" ? 1 : 0
        visible: opacity > 0
        scale: root.page === "home" ? 1 : 0.97
        Behavior on opacity { NumberAnimation { duration: Theme.animNormal } }
        Behavior on scale { NumberAnimation { duration: Theme.animNormal; easing.type: Easing.OutCubic } }

        // ↑ с первой полки — к верхней строке
        Keys.onUpPressed: header.focusFirst()

        ListView {
            id: rowsView
            anchors.fill: parent
            visible: !emptyState.visible
            clip: true
            spacing: Theme.px(24)
            keyNavigationEnabled: true
            highlightMoveDuration: Theme.animNormal
            highlightRangeMode: ListView.ApplyRange
            preferredHighlightBegin: Theme.px(22)
            preferredHighlightEnd: Theme.px(380)
            boundsBehavior: Flickable.StopAtBounds
            topMargin: Theme.px(22)
            bottomMargin: Theme.px(40)
            model: rowsModel

            delegate: Loader {
                id: rowLoader
                required property int index
                required property string key
                required property string kind
                required property string title
                width: rowsView.width
                focus: true
                sourceComponent: kind === "hero" ? heroRow : shelfRow
            }
        }

        EmptyState {
            id: emptyState
            anchors.centerIn: parent
            anchors.verticalCenterOffset: -Theme.px(60)
            visible: root.state_.loading !== true && (root.state_.error || "") !== ""
            icon: "warning"
            title: qsTr("Витрина %1 недоступна").arg(Format.storeName(root.store))
            text: root.state_.error || ""
        }
        ConsoleButton {
            id: retryButton
            visible: emptyState.visible
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.top: emptyState.bottom
            anchors.topMargin: Theme.px(30)
            text: qsTr("Повторить")
            icon: "refresh"
            KeyNavigation.up: storeChips
            onClicked: StoreCatalog.load(root.store, true)
        }
    }

    Component {
        id: heroRow
        // Поля по бокам — под кольцо фокуса (список полок обрезает всё за своими краями)
        FocusScope {
            height: Math.round(Math.min(Theme.px(470), Math.max(Theme.px(360), rowsView.height * 0.62)))
            focus: true
            StoreHero {
                anchors.fill: parent
                anchors.leftMargin: Theme.px(22)
                anchors.rightMargin: Theme.px(22)
                focus: true
                items: root.heroItems()
                loading: root.state_.loading === true
                eyebrow: qsTr("Популярное в %1").arg(Format.storeName(root.store))
                onActivated: (item) => root.openDetails(item)
            }
        }
    }
    Component {
        id: shelfRow
        StoreShelf {
            id: shelf
            // Loader полки задаёт ключ и заголовок
            readonly property Loader loader: parent as Loader
            focus: true
            title: loader ? loader.title : ""
            items: {
                const row = loader ? root.rowOf(loader.key) : null
                return row ? row.items : []
            }
            loading: {
                const row = loader ? root.rowOf(loader.key) : null
                return row ? row.loading : false
            }
            onActivated: (item) => root.openDetails(item)
        }
    }

    // ---------- Поиск ----------
    StoreSearchPage {
        id: search
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: header.bottom
        anchors.topMargin: Theme.px(26)
        anchors.bottom: parent.bottom
        opacity: root.page === "search" ? 1 : 0
        visible: opacity > 0
        enabled: root.page === "search"
        Behavior on opacity { NumberAnimation { duration: Theme.animNormal } }
        onActivated: (item) => root.openDetails(item)
    }

    // ---------- Страница игры ----------
    StoreDetailsPage {
        id: details
        anchors.fill: parent
        opacity: root.page === "details" ? 1 : 0
        visible: opacity > 0
        enabled: root.page === "details"
        transform: Translate {
            y: root.page === "details" ? 0 : Theme.px(40)
            Behavior on y { NumberAnimation { duration: Theme.animNormal; easing.type: Easing.OutCubic } }
        }
        Behavior on opacity { NumberAnimation { duration: Theme.animNormal } }
        onBuyRequested: (item) => root.buy(item)
        onPhoneRequested: (item) => phone.show(item.store, item.url, item, details)
        onPageRequested: (item) => root.openBrowser(item.store, item.url, item.title)
    }
}
