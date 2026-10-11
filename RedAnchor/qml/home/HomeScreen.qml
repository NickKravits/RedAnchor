pragma ComponentBehavior: Bound
import QtQuick

// Главный экран оболочки: профиль сверху, вкладки (L1/R1) и содержимое вкладки.
SetupPage {
    id: page

    signal switchUserRequested()
    // Вход в магазин через встроенный браузер (GOG)
    signal webLoginRequested(string store)

    // Открываем на «Моих играх», как на консоли
    property int tab: 1
    readonly property var tabs: [
        { key: "store", title: qsTr("Магазин") },
        { key: "games", title: qsTr("Мои игры") },
        { key: "apps", title: qsTr("Приложения") },
        { key: "library", title: qsTr("Библиотека") },
        { key: "settings", title: qsTr("Настройки") }
    ]

    showProfile: true
    // Магазин раскрывается на весь экран, пока фокус не на главных вкладках
    // (меню действий и экранная клавиатура этого не сбрасывают)
    immersive: page.tabs[page.tab].key === "store" && !tabBar.activeFocus
    // Подсказки раздела (Мои игры, Библиотека: «Играть», «Меню»), если он их задаёт
    hints: tabContent.item && tabContent.item.focusable && !tabBar.activeFocus
           ? (tabContent.item.hints || [{ button: "circle", text: qsTr("Назад") }, { button: "cross", text: qsTr("Выбрать") }])
           : [{ button: "cross", text: qsTr("Выбрать") }]
    initialFocus: tabBar

    // enterContent — переключение L1 / R1: фокус сразу на элементе раздела.
    // ←/→ на самой полосе вкладок оставляют фокус на ней, чтобы по ней можно было пройти.
    function setTab(index, enterContent) {
        const next = Math.max(0, Math.min(tabs.length - 1, index))
        if (next === tab)
            return
        const stayOnTabs = tabBar.activeFocus && !enterContent
        tab = next
        if (!stayOnTabs)
            focusContent()
    }

    function focusContent() {
        // Loader — область фокуса: фокус нужно отдавать загруженному элементу, а не самому Loader
        const item = tabContent.item
        if (item && item.focusable) {
            // Вкладка сама знает, куда поставить фокус (магазин — на панель разделов)
            if (item.focusFirst)
                item.focusFirst()
            else
                (item as Item).forceActiveFocus()
        }
        else
            tabBar.forceActiveFocus()
    }

    // L1 / R1 — соседняя вкладка откуда угодно
    Keys.onPressed: (event) => {
        if (event.key === Qt.Key_PageUp) {
            page.setTab(page.tab - 1, true)
            event.accepted = true
        } else if (event.key === Qt.Key_PageDown) {
            page.setTab(page.tab + 1, true)
            event.accepted = true
        } else if ((event.key === Qt.Key_Home || event.key === Qt.Key_End)
                   && tabContent.item && tabContent.item.switchSection) {
            // L2 / R2 — разделы внутри вкладки (магазины)
            tabContent.item.switchSection(event.key === Qt.Key_Home ? -1 : 1, true)
            event.accepted = true
        }
    }
    // Вверх из содержимого (если там некуда идти) и ○ — к вкладкам
    Keys.onUpPressed: tabBar.forceActiveFocus()
    Keys.onEscapePressed: tabBar.forceActiveFocus()

    // Всё рисуем поверх страницы, а не в области контента SetupPage
    // Фон «Моих игр»: широкая картинка выбранной игры поверх живого фона
    Item {
        parent: page
        anchors.fill: parent
        z: -1
        readonly property string source: page.tabs[page.tab].key === "games" && tabContent.item
                                         ? (tabContent.item.heroSource || "") : ""
        opacity: source !== "" ? 1 : 0
        Behavior on opacity { NumberAnimation { duration: Theme.animSlow } }

        Image {
            id: hero
            anchors.fill: parent
            source: parent.source
            fillMode: Image.PreserveAspectCrop
            asynchronous: true
            opacity: status === Image.Ready ? 1 : 0
            Behavior on opacity { NumberAnimation { duration: Theme.animSlow } }
        }
        // Затемнение: текст и карточки должны читаться. Слева и снизу темнее
        // (там сведения об игре), справа сверху картинка видна лучше.
        Rectangle {
            anchors.fill: parent
            gradient: Gradient {
                orientation: Gradient.Horizontal
                GradientStop { position: 0.0; color: Qt.rgba(0.03, 0.035, 0.045, 0.72) }
                GradientStop { position: 0.55; color: Qt.rgba(0.03, 0.035, 0.045, 0.38) }
                GradientStop { position: 1.0; color: Qt.rgba(0.03, 0.035, 0.045, 0.22) }
            }
        }
        Rectangle {
            anchors.fill: parent
            gradient: Gradient {
                GradientStop { position: 0.0; color: Qt.rgba(0.03, 0.035, 0.045, 0.55) }
                GradientStop { position: 0.3; color: Qt.rgba(0.03, 0.035, 0.045, 0.10) }
                GradientStop { position: 0.62; color: Qt.rgba(0.03, 0.035, 0.045, 0.35) }
                GradientStop { position: 1.0; color: Qt.rgba(0.03, 0.035, 0.045, 0.94) }
            }
        }
    }

    Item {
        parent: page
        anchors.fill: parent

        // ---------- Вкладки ----------
        FocusScope {
            id: tabBar
            x: page.immersive ? Theme.px(56) : Theme.gutter
            y: page.immersive ? Theme.px(24) : Theme.px(160)
            width: tabRow.width
            height: tabRow.height
            // На весь экран вкладки остаются видны, но меньше
            scale: page.immersive ? 0.8 : 1
            transformOrigin: Item.TopLeft
            Behavior on x { NumberAnimation { duration: Theme.animNormal; easing.type: Easing.OutCubic } }
            Behavior on y { NumberAnimation { duration: Theme.animNormal; easing.type: Easing.OutCubic } }
            Behavior on scale { NumberAnimation { duration: Theme.animNormal; easing.type: Easing.OutCubic } }

            Keys.onLeftPressed: page.setTab(page.tab - 1)
            Keys.onRightPressed: page.setTab(page.tab + 1)
            Keys.onDownPressed: page.focusContent()
            Keys.onReturnPressed: page.focusContent()
            Keys.onEnterPressed: page.focusContent()

            Row {
                id: tabRow
                spacing: Theme.px(16)

                ButtonGlyph {
                    button: "l1"
                    size: Theme.px(30)
                    anchors.verticalCenter: parent.verticalCenter
                    opacity: page.tab > 0 ? 1 : 0.4
                }

                // Дорожка вкладок: стеклянная «пилюля», по ней скользит индикатор выбранной
                Rectangle {
                    id: track
                    width: tabStrip.width + 2 * padding
                    height: Theme.px(64) + 2 * padding
                    radius: height / 2
                    color: Qt.rgba(0.06, 0.066, 0.078, 0.42)
                    border.width: 1
                    border.color: Theme.glassBorder
                    anchors.verticalCenter: parent.verticalCenter

                    readonly property real padding: Theme.px(6)
                    readonly property Item selectedItem: {
                        page.tab
                        tabRepeater.count
                        return tabRepeater.itemAt(page.tab)
                    }

                    Rectangle {
                        id: indicator
                        x: track.padding + (track.selectedItem ? track.selectedItem.x : 0)
                        y: track.padding
                        width: track.selectedItem ? track.selectedItem.width : 0
                        height: Theme.px(64)
                        radius: height / 2
                        color: tabBar.activeFocus ? Theme.focus : Theme.selection
                        border.width: tabBar.activeFocus ? 0 : 1
                        border.color: Qt.rgba(1, 1, 1, 0.16)
                        Behavior on x { NumberAnimation { duration: Theme.animNormal; easing.type: Easing.OutQuint } }
                        Behavior on width { NumberAnimation { duration: Theme.animNormal; easing.type: Easing.OutQuint } }
                        Behavior on color { ColorAnimation { duration: Theme.animFast } }

                        // Мягкий ореол индикатора, когда фокус на вкладках
                        FocusRing {
                            shown: tabBar.activeFocus
                            radius: indicator.radius
                            gap: 0
                            ringWidth: 0
                        }
                    }

                    Row {
                        id: tabStrip
                        x: track.padding
                        y: track.padding

                        Repeater {
                            id: tabRepeater
                            model: page.tabs
                            delegate: Item {
                                id: tabItem
                                required property var modelData
                                required property int index
                                readonly property bool selected: page.tab === index
                                readonly property bool focused: selected && tabBar.activeFocus

                                width: tabLabel.implicitWidth + Theme.px(60)
                                height: Theme.px(64)

                                Text {
                                    id: tabLabel
                                    anchors.centerIn: parent
                                    text: tabItem.modelData.title
                                    color: tabItem.focused ? Theme.textOnFocus
                                         : tabItem.selected ? Theme.textPrimary : Theme.textSecondary
                                    font.family: Theme.fontFamily
                                    font.pixelSize: Theme.fontBody
                                    font.weight: Font.DemiBold
                                    Behavior on color { ColorAnimation { duration: Theme.animFast } }
                                }
                                MouseArea {
                                    anchors.fill: parent
                                    onClicked: {
                                        page.setTab(tabItem.index)
                                        tabBar.forceActiveFocus()
                                    }
                                }
                            }
                        }
                    }
                }

                ButtonGlyph {
                    button: "r1"
                    size: Theme.px(30)
                    anchors.verticalCenter: parent.verticalCenter
                    opacity: page.tab < page.tabs.length - 1 ? 1 : 0.4
                }
            }
        }

        // ---------- Содержимое вкладки ----------
        Loader {
            id: tabContent
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.bottom: parent.bottom
            anchors.leftMargin: page.immersive ? Theme.px(56) : Theme.gutter
            anchors.rightMargin: page.immersive ? Theme.px(56) : Theme.gutter
            // Под вкладками; на весь экран вкладки меньше и выше
            anchors.topMargin: page.immersive ? Theme.px(24) + tabBar.height * 0.8 + Theme.px(28)
                                              : Theme.px(160) + tabBar.height + Theme.px(60)
            anchors.bottomMargin: page.immersive ? Theme.px(100) : Theme.px(140)
            Behavior on anchors.leftMargin { NumberAnimation { duration: Theme.animNormal; easing.type: Easing.OutCubic } }
            Behavior on anchors.rightMargin { NumberAnimation { duration: Theme.animNormal; easing.type: Easing.OutCubic } }
            Behavior on anchors.topMargin { NumberAnimation { duration: Theme.animNormal; easing.type: Easing.OutCubic } }
            Behavior on anchors.bottomMargin { NumberAnimation { duration: Theme.animNormal; easing.type: Easing.OutCubic } }

            sourceComponent: {
                switch (page.tabs[page.tab].key) {
                case "store": return storeTab
                case "games": return gamesTab
                case "apps": return appsTab
                case "library": return libraryTab
                case "settings": return settingsTab
                default: return null
                }
            }
            onLoaded: fade.restart()

            NumberAnimation { id: fade; target: tabContent; property: "opacity"; from: 0; to: 1; duration: Theme.animNormal; easing.type: Easing.OutCubic }
        }
    }

    // Пустая вкладка: сообщение по центру
    component EmptyTab: Item {
        property bool focusable: false
        property alias icon: emptyState.icon
        property alias title: emptyState.title
        property alias text: emptyState.text

        EmptyState {
            id: emptyState
            anchors.centerIn: parent
            anchors.verticalCenterOffset: -Theme.px(40)
        }
    }

    Component {
        id: storeTab
        StoreView {
            compact: page.immersive
            onWebLoginRequested: (store) => page.webLoginRequested(store)
        }
    }
    Component {
        id: gamesTab
        GamesView {}
    }
    Component {
        id: appsTab
        EmptyTab {
            icon: "apps"
            title: qsTr("Приложений пока нет")
            text: qsTr("Установите их из магазина.")
        }
    }
    Component {
        id: libraryTab
        LibraryView {}
    }
    Component {
        id: settingsTab
        SettingsView {
            onSwitchUserRequested: page.switchUserRequested()
        }
    }
}
