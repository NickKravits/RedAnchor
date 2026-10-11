pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls.Basic
import "Format.js" as Format
import "Storefront.js" as Storefront

// Страница игры на витрине: большая картинка, цена и кнопки покупки, описание,
// сведения и скриншоты. Купленную игру можно сразу установить.
// ↓ — к скриншотам (✕ — во весь экран), ↑ — обратно к кнопкам, ○ — назад к витрине.
FocusScope {
    id: root

    // Картинка игры уезжает вверх медленнее страницы — не даём ей вылезти за раздел
    clip: true

    readonly property var item: StoreCatalog.details
    readonly property bool hasItem: item && item.id !== undefined
    readonly property var own: {
        StoreCatalog.revision
        return root.hasItem ? StoreCatalog.ownership(root.item) : ({})
    }
    readonly property var hints: [
        { button: "circle", text: qsTr("Назад") },
        { button: "cross", text: qsTr("Выбрать") }
    ]

    // Покупка в браузере, QR-код для телефона, страница игры на сайте магазина
    signal buyRequested(var item)
    signal phoneRequested(var item)
    signal pageRequested(var item)

    function focusFirst() {
        primaryButton.forceActiveFocus()
        flick.contentY = 0
    }

    // Главная кнопка: купить / получить / установить / играть
    readonly property var primary: {
        const o = root.own
        if (!root.hasItem)
            return { text: "", enabled: false }
        if (o.installed)
            return { text: qsTr("Играть"), icon: "play", enabled: true, action: "play" }
        if (o.status === "queued" || o.status === "downloading")
            return { text: o.status === "queued" ? qsTr("В очереди на загрузку")
                                                 : qsTr("Загружается %1").arg(o.progress >= 0 ? Math.round(o.progress * 100) + "%" : "…"),
                     icon: "download", enabled: false }
        if (o.owned)
            return { text: qsTr("Установить"), icon: "download", enabled: true, action: "install" }
        if (root.item.promo === "upcoming")
            return { text: Storefront.promo(root.item), icon: "store", enabled: true, action: "page" }
        if (root.item.promo === "now" || (root.item.price && root.item.price.free))
            return { text: qsTr("Получить бесплатно"), icon: "plus", enabled: true, action: "buy" }
        const price = Storefront.price(root.item)
        if (root.item.upcoming && !(root.item.price && root.item.price.known))
            return { text: qsTr("Скоро в продаже"), icon: "store", enabled: true, action: "page" }
        return { text: price !== "" ? qsTr("Купить · %1").arg(price) : qsTr("Купить"), icon: "store", enabled: true, action: "buy" }
    }

    function runPrimary() {
        switch (root.primary.action) {
        case "play":
            GameLauncher.launch(root.own.gameId)
            break
        case "install":
            StoreManager.install(root.item.store, root.own.appId, root.own.title || root.item.title)
            break
        case "buy":
            root.buyRequested(root.item)
            break
        case "page":
            root.pageRequested(root.item)
            break
        }
    }

    // ---------- Фон: картинка игры на всю страницу ----------
    Item {
        id: backdrop
        // Картинка уезжает медленнее страницы (параллакс)
        width: parent.width
        height: heroArea.height
        y: -flick.contentY * 0.4
        clip: true

        RoundedClip {
            anchors.fill: parent
            radius: Theme.radiusLarge

            Rectangle {
                anchors.fill: parent
                color: Theme.backgroundAlt
            }
            Image {
                anchors.fill: parent
                source: root.hasItem ? Storefront.thumb(root.item.hero || root.item.cover, 1920) : ""
                fillMode: Image.PreserveAspectCrop
                asynchronous: true
                sourceSize.width: 1920
                opacity: status === Image.Ready ? 1 : 0
                Behavior on opacity { NumberAnimation { duration: Theme.animSlow } }
            }
            Rectangle {
                anchors.fill: parent
                gradient: Gradient {
                    orientation: Gradient.Horizontal
                    GradientStop { position: 0.0; color: Qt.rgba(Theme.background.r, Theme.background.g, Theme.background.b, 0.94) }
                    GradientStop { position: 0.5; color: Qt.rgba(Theme.background.r, Theme.background.g, Theme.background.b, 0.6) }
                    GradientStop { position: 1.0; color: Qt.rgba(Theme.background.r, Theme.background.g, Theme.background.b, 0.1) }
                }
            }
            Rectangle {
                anchors.fill: parent
                gradient: Gradient {
                    GradientStop { position: 0.5; color: "transparent" }
                    GradientStop { position: 1.0; color: Qt.rgba(Theme.background.r, Theme.background.g, Theme.background.b, 0.85) }
                }
            }
        }
    }

    Flickable {
        id: flick
        anchors.fill: parent
        contentHeight: content.height + Theme.px(40)
        boundsBehavior: Flickable.StopAtBounds
        clip: true
        Behavior on contentY { NumberAnimation { duration: Theme.animNormal; easing.type: Easing.OutCubic } }

        Column {
            id: content
            width: flick.width
            spacing: Theme.px(36)

            // ---------- Название, цена и кнопки ----------
            Item {
                id: heroArea
                width: parent.width
                height: Math.max(Theme.px(520), Math.min(root.height * 0.72, Theme.px(640)))

                Column {
                    anchors.left: parent.left
                    anchors.bottom: parent.bottom
                    anchors.leftMargin: Theme.px(56)
                    anchors.bottomMargin: Theme.px(48)
                    width: Math.min(parent.width - Theme.px(112), Theme.px(1100))
                    spacing: Theme.px(16)

                    Row {
                        spacing: Theme.px(12)
                        Badge {
                            text: root.hasItem ? Format.storeName(root.item.store) : ""
                            size: Theme.px(32)
                        }
                        Text {
                            anchors.verticalCenter: parent.verticalCenter
                            text: Storefront.promo(root.item)
                            visible: text !== ""
                            color: Theme.focus
                            font.family: Theme.fontFamily
                            font.pixelSize: Theme.fontSmall
                            font.weight: Font.Medium
                        }
                    }
                    Text {
                        width: parent.width
                        text: root.hasItem ? root.item.title : ""
                        color: Theme.textPrimary
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontHuge
                        font.weight: Font.Light
                        font.letterSpacing: Theme.trackingTight
                        wrapMode: Text.WordWrap
                        maximumLineCount: 2
                        elide: Text.ElideRight
                    }
                    Text {
                        width: parent.width
                        visible: text !== ""
                        text: {
                            if (!root.hasItem)
                                return ""
                            const parts = []
                            if (root.item.developer)
                                parts.push(root.item.developer)
                            if (root.item.releaseDate)
                                parts.push(Storefront.date(root.item.releaseDate))
                            if (root.item.genres && root.item.genres.length > 0)
                                parts.push(root.item.genres.slice(0, 3).join(", "))
                            return parts.join("  ·  ")
                        }
                        color: Theme.textSecondary
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontSmall
                        elide: Text.ElideRight
                    }
                    Item { width: 1; height: Theme.px(4) }
                    StorePrice {
                        item: root.item
                        own: root.own
                        pixelSize: Theme.fontLarge
                    }
                    Item { width: 1; height: Theme.px(6) }

                    // ---------- Кнопки ----------
                    Row {
                        id: buttons
                        spacing: Theme.px(16)

                        ConsoleButton {
                            id: primaryButton
                            text: root.primary.text
                            icon: root.primary.icon || ""
                            primary: true
                            enabledState: root.primary.enabled
                            focus: true
                            KeyNavigation.right: phoneButton.visible ? phoneButton : pageButton
                            KeyNavigation.down: shots.count > 0 ? shots : null
                            onClicked: root.runPrimary()
                            onActiveFocusChanged: if (activeFocus) flick.contentY = 0
                        }
                        ConsoleButton {
                            id: phoneButton
                            visible: root.hasItem && !root.own.owned && root.primary.action === "buy"
                            text: root.item && (root.item.promo === "now" || (root.item.price && root.item.price.free))
                                  ? qsTr("Получить на телефоне") : qsTr("Купить на телефоне")
                            icon: "arrowRight"
                            KeyNavigation.left: primaryButton
                            KeyNavigation.right: pageButton
                            KeyNavigation.down: shots.count > 0 ? shots : null
                            onClicked: root.phoneRequested(root.item)
                            onActiveFocusChanged: if (activeFocus) flick.contentY = 0
                        }
                        ConsoleButton {
                            id: pageButton
                            visible: root.hasItem && root.primary.action !== "page"
                            text: qsTr("Страница в магазине")
                            icon: "globe"
                            KeyNavigation.left: phoneButton.visible ? phoneButton : primaryButton
                            KeyNavigation.down: shots.count > 0 ? shots : null
                            onClicked: root.pageRequested(root.item)
                            onActiveFocusChanged: if (activeFocus) flick.contentY = 0
                        }
                    }

                    // Без входа в магазин купленная игра не появится в библиотеке консоли
                    Row {
                        visible: root.hasItem && !root.own.owned && !StoreCatalog.signedIn(root.item.store)
                        spacing: Theme.px(10)
                        Icon {
                            name: "info"
                            size: Theme.px(22)
                            lineWidth: 1.5
                            color: Theme.textMuted
                            anchors.verticalCenter: parent.verticalCenter
                        }
                        Text {
                            anchors.verticalCenter: parent.verticalCenter
                            text: qsTr("Войдите в %1 в разделе «%1», чтобы купленные игры сразу появлялись в библиотеке.")
                                  .arg(root.hasItem ? Format.storeName(root.item.store) : "")
                            color: Theme.textMuted
                            font.family: Theme.fontFamily
                            font.pixelSize: Theme.fontCaption + Theme.px(2)
                        }
                    }
                }
            }

            // ---------- Описание и сведения ----------
            Row {
                x: Theme.px(56)
                width: parent.width - Theme.px(112)
                spacing: Theme.px(64)

                Column {
                    width: parent.width * 0.58
                    spacing: Theme.px(14)

                    Text {
                        text: qsTr("Об игре")
                        color: Theme.textPrimary
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontBody
                        font.weight: Font.Medium
                    }
                    // Пока описание грузится — заглушки строк
                    Repeater {
                        model: StoreCatalog.detailsLoading && !(root.hasItem && root.item.description) ? 4 : 0
                        delegate: StoreShimmer {
                            required property int index
                            width: index === 3 ? parent.width * 0.6 : parent.width
                            height: Theme.px(20)
                            radius: Theme.radiusSmall
                        }
                    }
                    Text {
                        width: parent.width
                        visible: !StoreCatalog.detailsLoading || text !== ""
                        text: root.hasItem ? (root.item.fullDescription || root.item.description || qsTr("Описания пока нет.")) : ""
                        color: Theme.textSecondary
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontSmall
                        lineHeight: 1.3
                        wrapMode: Text.WordWrap
                        maximumLineCount: 14
                        elide: Text.ElideRight
                    }
                }

                // Сведения: стеклянная панель
                GlassPanel {
                    width: parent.width * 0.42 - parent.spacing
                    height: facts.height + Theme.spacingXl
                    radius: Theme.radiusLarge

                    Column {
                        id: facts
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.top: parent.top
                        anchors.margins: Theme.spacingL - Theme.px(8)
                        spacing: Theme.spacingM

                        Repeater {
                            model: root.hasItem ? [
                                { label: qsTr("Разработчик"), value: root.item.developer || "" },
                                { label: qsTr("Издатель"), value: root.item.publisher || "" },
                                { label: qsTr("Дата выхода"), value: Storefront.date(root.item.releaseDate) || (root.item.upcoming ? qsTr("Скоро") : "") },
                                { label: qsTr("Платформы"), value: Storefront.platforms(root.item) },
                                { label: qsTr("Геймпад"), value: root.item.controller ? qsTr("Поддерживается") : qsTr("Нет данных") }
                            ] : []
                            delegate: Column {
                                id: fact
                                required property var modelData
                                width: facts.width
                                spacing: Theme.px(4)
                                Text {
                                    text: fact.modelData.label
                                    color: Theme.textMuted
                                    font.family: Theme.fontFamily
                                    font.pixelSize: Theme.fontCaption + Theme.px(2)
                                }
                                Text {
                                    width: parent.width
                                    text: fact.modelData.value !== "" ? fact.modelData.value : "—"
                                    color: Theme.textPrimary
                                    font.family: Theme.fontFamily
                                    font.pixelSize: Theme.fontSmall
                                    wrapMode: Text.WordWrap
                                }
                            }
                        }
                        // Поддержка геймпада по данным магазина
                        Row {
                            visible: root.hasItem && root.item.controller === true
                            spacing: Theme.px(10)
                            Icon {
                                name: "controller"
                                size: Theme.px(24)
                                lineWidth: 1.5
                                color: Theme.success
                                anchors.verticalCenter: parent.verticalCenter
                            }
                            Text {
                                anchors.verticalCenter: parent.verticalCenter
                                text: qsTr("Можно играть геймпадом")
                                color: Theme.success
                                font.family: Theme.fontFamily
                                font.pixelSize: Theme.fontSmall
                            }
                        }
                    }
                }
            }

            // ---------- Скриншоты ----------
            Column {
                visible: shots.count > 0
                x: Theme.px(56)
                width: parent.width - Theme.px(112)
                spacing: Theme.px(18)

                Text {
                    text: qsTr("Скриншоты")
                    color: shots.activeFocus ? Theme.textPrimary : Theme.textSecondary
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontBody
                    font.weight: Font.Medium
                }
                ListView {
                    id: shots
                    width: parent.width
                    height: Theme.px(306)
                    orientation: ListView.Horizontal
                    spacing: Theme.px(24)
                    clip: true
                    leftMargin: Theme.px(24)
                    rightMargin: Theme.px(18)
                    topMargin: Theme.px(30)
                    keyNavigationEnabled: true
                    highlightMoveDuration: Theme.animNormal
                    highlightRangeMode: ListView.ApplyRange
                    preferredHighlightBegin: Theme.px(24)
                    preferredHighlightEnd: width * 0.6
                    boundsBehavior: Flickable.StopAtBounds
                    model: root.hasItem && root.item.screenshots ? root.item.screenshots : []

                    KeyNavigation.up: primaryButton
                    onActiveFocusChanged: if (activeFocus) flick.contentY = Math.max(0, flick.contentHeight - flick.height)
                    Keys.onReturnPressed: viewer.show(currentIndex)
                    Keys.onEnterPressed: viewer.show(currentIndex)

                    delegate: Item {
                        id: shot
                        required property int index
                        required property string modelData
                        readonly property bool active: ListView.isCurrentItem && shots.activeFocus
                        width: Math.round(height * 16 / 9)
                        height: shots.height - Theme.px(52)
                        scale: active ? Theme.focusScale : 1
                        Behavior on scale { NumberAnimation { duration: Theme.animFast; easing.type: Theme.easeOut } }

                        FocusRing {
                            shown: shot.active
                            radius: Theme.radiusTile
                        }
                        StoreShimmer {
                            anchors.fill: parent
                            visible: shotImage.status !== Image.Ready
                        }
                        RoundedClip {
                            anchors.fill: parent
                            radius: Theme.radiusTile
                            Image {
                                id: shotImage
                                anchors.fill: parent
                                source: Storefront.thumb(shot.modelData, 640)
                                fillMode: Image.PreserveAspectCrop
                                asynchronous: true
                                sourceSize.width: 640
                                opacity: status === Image.Ready ? 1 : 0
                                Behavior on opacity { NumberAnimation { duration: Theme.animNormal } }
                            }
                        }
                        MouseArea {
                            anchors.fill: parent
                            onClicked: {
                                shots.currentIndex = shot.index
                                shots.forceActiveFocus()
                                viewer.show(shot.index)
                            }
                        }
                    }
                }
            }
        }
    }

    // ---------- Скриншот во весь экран: ←/→ — соседний, ○ — закрыть ----------
    Popup {
        id: viewer

        property int index: 0
        readonly property var images: root.hasItem && root.item.screenshots ? root.item.screenshots : []

        function show(i) {
            index = i
            open()
        }

        parent: Overlay.overlay
        anchors.centerIn: parent
        width: parent ? parent.width : 0
        height: parent ? parent.height : 0
        padding: 0
        modal: true
        focus: true
        closePolicy: Popup.CloseOnEscape
        Overlay.modal: Rectangle { color: Theme.glassStrong }
        background: Item {}

        enter: Transition {
            NumberAnimation { property: "opacity"; from: 0; to: 1; duration: Theme.animNormal }
            NumberAnimation { property: "scale"; from: 0.97; to: 1; duration: Theme.animNormal; easing.type: Easing.OutCubic }
        }
        exit: Transition {
            NumberAnimation { property: "opacity"; to: 0; duration: Theme.animFast }
        }
        onClosed: shots.forceActiveFocus()

        contentItem: FocusScope {
            focus: true
            Keys.onLeftPressed: viewer.index = (viewer.index + viewer.images.length - 1) % Math.max(1, viewer.images.length)
            Keys.onRightPressed: viewer.index = (viewer.index + 1) % Math.max(1, viewer.images.length)
            Keys.onReturnPressed: viewer.close()

            Spinner {
                anchors.centerIn: parent
                visible: big.status === Image.Loading
            }
            Image {
                id: big
                anchors.fill: parent
                anchors.margins: Theme.px(80)
                source: viewer.images[viewer.index] || ""
                fillMode: Image.PreserveAspectFit
                asynchronous: true
                opacity: status === Image.Ready ? 1 : 0
                Behavior on opacity { NumberAnimation { duration: Theme.animNormal } }
            }
            Text {
                anchors.horizontalCenter: parent.horizontalCenter
                anchors.bottom: parent.bottom
                anchors.bottomMargin: Theme.px(30)
                text: (viewer.index + 1) + " / " + viewer.images.length
                color: Theme.textSecondary
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontSmall
                font.features: { "tnum": 1 }
            }
            HintBar {
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                anchors.rightMargin: Theme.px(48)
                anchors.bottomMargin: Theme.px(26)
                model: [{ button: "circle", text: qsTr("Закрыть") }, { button: "dpad", text: qsTr("Листать") }]
            }
        }
    }
}
