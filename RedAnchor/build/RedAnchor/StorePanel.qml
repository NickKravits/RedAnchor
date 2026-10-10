pragma ComponentBehavior: Bound
import QtQuick
import "ModelSync.js" as ModelSync
import "Format.js" as Format
import "Compat.js" as Compat

// Раздел магазина (Steam, GOG): вход и игры библиотеки сеткой. Библиотеку Steam, имя
// профиля и аватар получает помощник на SteamKit2, купленные игры GOG — gogdl.
// ✕ на игре — установить; на установленной или загружаемой — меню действий.
// Загрузчики игр ставятся в «Настройки → Обновления».
FocusScope {
    id: root

    property string store                       // "steam" | "gog"
    readonly property var state_: StoreManager.stores[store] || ({})
    // Без загрузчика выбирать нечего — фокус остаётся на панели магазинов
    readonly property bool focusable: state_.toolInstalled === true
    readonly property string toolName: store === "steam" ? "DepotDownloader" : "gogdl"

    // Встроенный браузер для входа (GOG) открывает главный экран
    signal webLoginRequested(string store)

    function syncGames() {
        ModelSync.sync(games, StoreManager.owned[store] || [], "gameId")
    }

    function entry(i) {
        const e = games.get(i)
        return { id: e.gameId, appId: e.appId, title: e.title, installed: e.installed, status: e.status }
    }

    // Причина неудачной загрузки
    function failureText(id) {
        for (const d of StoreManager.downloads) {
            if (d.id === id && d.message)
                return d.message
        }
        return qsTr("Предыдущая загрузка не удалась")
    }

    function activate(game) {
        if (game.status === "queued" || game.status === "downloading") {
            menu.show(game.title, game.status === "queued" ? qsTr("В очереди на загрузку") : qsTr("Загружается"), [
                { text: qsTr("Отменить загрузку"), icon: "close", run: () => StoreManager.cancel(game.id) },
                { text: qsTr("Отмена") }
            ], grid)
            return
        }
        if (game.installed) {
            const pinned = GameLibrary.isPinned(game.id)
            menu.show(game.title, qsTr("Установлена — игра есть в библиотеке"), [
                pinned ? { text: qsTr("Открепить с главного экрана"), icon: "close", run: () => GameLibrary.unpin(game.id) }
                       : { text: qsTr("Закрепить на главном экране"), icon: "plus", run: () => GameLibrary.pin(game.id) },
                { text: qsTr("Удалить игру"), icon: "trash", run: () => StoreManager.uninstall(game.id) },
                { text: qsTr("Отмена") }
            ], grid)
            return
        }
        // Перед установкой — поддержка геймпада: предупреждаем, если игра без поддержки
        // DualSense (будет система раскладки RedAnchor) или только для мыши и клавиатуры
        const compat = root.compatOf(game.appId)
        const warn = compat.level === "remap" || compat.level === "keyboard"
        menu.show(game.title,
                  game.status === "failed" ? root.failureText(game.id) : compat.text, [
            { text: game.status === "failed" ? qsTr("Повторить загрузку")
                                             : warn ? qsTr("Установить всё равно") : qsTr("Установить"),
              icon: "download", run: () => StoreManager.install(root.store, game.appId, game.title) },
            { text: qsTr("Отмена") }
        ], grid)
    }

    // ---------- Поддержка геймпада ----------
    property int compatRevision: 0
    function compatOf(appId) {
        root.compatRevision
        return Compat.describe(StoreManager.compatibility(root.store, appId), GamepadManager.dualSense)
    }
    Connections {
        target: StoreManager
        function onCompatibilityChanged(id) { root.compatRevision++ }
    }

    function focusFirst() {
        if (!root.state_.toolInstalled)
            root.forceActiveFocus()
        else if (!root.state_.signedIn)
            (root.store === "steam" ? steamForm : signInButton).forceActiveFocus()
        else if (grid.count > 0)
            grid.forceActiveFocus()
        else
            refreshButton.forceActiveFocus()
    }


    function open() {
        StoreManager.refreshTools()
        games.clear()
        syncGames()
        // Список купленных игр загружаем при первом открытии раздела
        if (state_.signedIn && (StoreManager.owned[store] || []).length === 0)
            StoreManager.refresh(store)
    }

    // Тот же компонент показывает разные магазины — при переключении (L2/R2) обновляемся
    onStoreChanged: open()
    Component.onCompleted: open()

    Connections {
        target: StoreManager
        function onOwnedChanged() { root.syncGames() }
    }
    // Загрузчик поставили в «Обновлениях»
    Connections {
        target: ComponentsManager
        function onFinished() { StoreManager.refreshTools() }
    }

    ListModel { id: games }
    ActionMenu { id: menu }

    // ---------- Нет загрузчика: он ставится в «Обновлениях» ----------
    EmptyState {
        visible: !root.state_.toolInstalled
        anchors.centerIn: parent
        anchors.verticalCenterOffset: -Theme.px(30)
        icon: "download"
        title: qsTr("Нужно установить обновления")
        text: qsTr("Для %1 нужен загрузчик игр %2. Установите его: Настройки → Обновления.")
              .arg(Format.storeName(root.store)).arg(root.toolName)
    }

    // ---------- Вход ----------
    Column {
        visible: root.state_.toolInstalled && !root.state_.signedIn
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.top: parent.top
        anchors.topMargin: root.store === "steam" ? 0 : Theme.px(60)
        // Steam: рядом с формой входа помещается QR-код
        width: Math.min(parent.width, root.store === "steam" ? Theme.px(1240) : Theme.px(860))
        spacing: Theme.px(28)

        EmptyState {
            visible: root.store !== "steam"
            anchors.horizontalCenter: parent.horizontalCenter
            icon: "user"
            title: qsTr("Войдите в %1").arg(Format.storeName(root.store))
            text: root.state_.message || qsTr("Откроется страница входа %1. Вводить текст можно экранной клавиатурой.").arg(Format.storeName(root.store))
        }
        ConsoleButton {
            id: signInButton
            visible: root.store !== "steam"
            anchors.horizontalCenter: parent.horizontalCenter
            text: root.state_.busy ? qsTr("Вход…") : qsTr("Войти")
            icon: "user"
            primary: true
            enabledState: !root.state_.busy
            onClicked: root.webLoginRequested(root.store)
        }

        SteamLoginForm {
            id: steamForm
            visible: root.store === "steam"
            width: parent.width
        }
    }

    // ---------- Игры (Steam — витрина и поиск, GOG — купленные) ----------
    Item {
        anchors.fill: parent
        visible: root.state_.toolInstalled && root.state_.signedIn

        Row {
            id: accountRow
            spacing: Theme.px(20)

            // Аватар профиля (Steam), без него — буква имени
            Item {
                width: Theme.px(44)
                height: width
                anchors.verticalCenter: parent.verticalCenter

                Avatar {
                    anchors.fill: parent
                    visible: profileImage.status !== Image.Ready
                    name: root.state_.account || "?"
                    size: parent.width
                }
                RoundedClip {
                    anchors.fill: parent
                    radius: width / 2
                    visible: profileImage.status === Image.Ready
                    Image {
                        id: profileImage
                        anchors.fill: parent
                        source: root.state_.avatar || ""
                        fillMode: Image.PreserveAspectCrop
                        asynchronous: true
                    }
                }
            }
            Column {
                anchors.verticalCenter: parent.verticalCenter
                spacing: Theme.px(2)
                Text {
                    text: root.state_.account || Format.storeName(root.store)
                    color: Theme.textPrimary
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontBody
                    font.weight: Font.Medium
                }
                Text {
                    text: root.state_.busy ? (root.state_.message || qsTr("Загрузка…"))
                        : root.state_.message !== "" ? root.state_.message
                        : qsTr("Игр: %1").arg(grid.count)
                    color: Theme.textSecondary
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontSmall
                }
            }
            Spinner {
                visible: root.state_.busy
                size: Theme.px(26)
                anchors.verticalCenter: parent.verticalCenter
            }
            Item { width: Theme.px(20); height: 1 }
            ConsoleButton {
                id: refreshButton
                anchors.verticalCenter: parent.verticalCenter
                text: qsTr("Обновить")
                icon: "refresh"
                enabledState: !root.state_.busy
                KeyNavigation.right: signOutButton
                KeyNavigation.down: grid.count > 0 ? grid : null
                onClicked: StoreManager.refresh(root.store)
            }
            ConsoleButton {
                id: signOutButton
                anchors.verticalCenter: parent.verticalCenter
                text: qsTr("Выйти")
                icon: "logout"
                KeyNavigation.left: refreshButton
                KeyNavigation.down: grid.count > 0 ? grid : null
                onClicked: menu.show(qsTr("Выйти из %1?").arg(Format.storeName(root.store)),
                                     qsTr("Установленные игры останутся на консоли."), [
                    { text: qsTr("Выйти"), icon: "logout", run: () => StoreManager.signOut(root.store) },
                    { text: qsTr("Отмена") }
                ], signOutButton)
            }
        }

        EmptyState {
            visible: grid.count === 0 && !root.state_.busy
            anchors.centerIn: parent
            icon: "store"
            title: qsTr("Игр не найдено")
            text: qsTr("В этом аккаунте %1 пока нет игр.").arg(Format.storeName(root.store))
        }

        GridView {
            id: grid
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: accountRow.bottom
            anchors.topMargin: Theme.px(24)
            anchors.bottom: info.top
            anchors.bottomMargin: Theme.px(10)
            readonly property int columns: Math.max(3, Math.floor(width / Theme.px(220)))
            cellWidth: Math.floor(width / columns)
            cellHeight: cellWidth
            clip: true
            keyNavigationEnabled: true
            highlightMoveDuration: Theme.animFast
            model: games

            KeyNavigation.up: refreshButton
            KeyNavigation.priority: KeyNavigation.AfterItem

            Keys.onReturnPressed: if (currentIndex >= 0) root.activate(root.entry(currentIndex))
            Keys.onEnterPressed: if (currentIndex >= 0) root.activate(root.entry(currentIndex))

            delegate: Item {
                id: cell
                required property int index
                required property string gameId
                required property string appId
                required property string title
                required property bool installed
                required property string status
                required property real progress
                width: grid.cellWidth
                height: grid.cellHeight

                GameCover {
                    anchors.fill: parent
                    anchors.margins: Theme.px(12)
                    gameId: cell.gameId
                    store: root.store
                    appId: cell.appId
                    title: cell.title
                    status: cell.status
                    progress: cell.progress
                    installed: cell.installed
                    showInstalled: true
                    showStore: false
                    active: cell.GridView.isCurrentItem && grid.activeFocus
                    scale: active ? 1.04 : 1
                    Behavior on scale { NumberAnimation { duration: Theme.animFast; easing.type: Easing.OutCubic } }
                }
                MouseArea {
                    anchors.fill: parent
                    onClicked: {
                        grid.currentIndex = cell.index
                        grid.forceActiveFocus()
                        root.activate(root.entry(cell.index))
                    }
                }
            }
        }

        // Выбранная игра: название и поддержка геймпада
        Column {
            id: info
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            anchors.leftMargin: Theme.px(12)
            spacing: Theme.px(6)

            readonly property var selected: grid.currentIndex >= 0 && grid.count > 0 ? games.get(grid.currentIndex) : null
            readonly property var compat: selected ? root.compatOf(selected.appId) : Compat.describe(null, false)

            Text {
                text: info.selected ? info.selected.title : ""
                color: Theme.textPrimary
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontLarge
                font.weight: Font.Light
            }
            Row {
                visible: info.selected !== null
                spacing: Theme.px(10)
                Icon {
                    name: info.compat.level === "keyboard" ? "warning" : "controller"
                    size: Theme.px(22)
                    lineWidth: 1.5
                    color: info.compat.level === "native" ? Theme.success
                         : info.compat.level === "remap" || info.compat.level === "keyboard" ? Theme.warning
                         : Theme.textMuted
                    anchors.verticalCenter: parent.verticalCenter
                }
                Text {
                    anchors.verticalCenter: parent.verticalCenter
                    width: info.width - Theme.px(40)
                    text: info.compat.text
                    elide: Text.ElideRight
                    color: Theme.textSecondary
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontSmall
                }
            }
        }
    }
}
