pragma ComponentBehavior: Bound
import QtQuick
import "Format.js" as Format

// Вкладка «Библиотека»: игры пользователя сеткой квадратных обложек — установленные
// и загружаемые (с прогрессом). ✕ — меню: закрепить на главном экране / открепить
// (можно и во время загрузки), удалить игру или отменить загрузку.
FocusScope {
    id: root

    readonly property bool focusable: GameLibrary.count > 0
    readonly property var hints: [{ button: "options", text: qsTr("Меню") }, { button: "cross", text: qsTr("Играть") }]

    // ✕ — играть (загружаемая игра — меню), Options — меню игры
    function play(game) {
        if (game.installed)
            GameLauncher.launch(game.id)
        else
            activate(game)
    }

    // Загрузка игры (или null)
    function downloadOf(id) {
        for (const d of StoreManager.downloads) {
            if (d.id === id)
                return d
        }
        return null
    }

    function statusText(game) {
        if (game.installed)
            return Format.storeName(game.store) + (game.sizeBytes > 0 ? " · " + Format.bytes(game.sizeBytes) : "")
        const d = downloadOf(game.id)
        if (!d || d.status === "queued")
            return qsTr("В очереди на загрузку")
        if (d.status === "failed")
            return d.message || qsTr("Загрузка не удалась")
        if (d.status === "confirm")
            return qsTr("Подтвердите установку в Steam")
        return d.progress >= 0 ? qsTr("Загружается: %1%").arg(Math.round(d.progress * 100)) : qsTr("Загружается…")
    }

    function activate(game) {
        const pinned = GameLibrary.isPinned(game.id)
        const actions = [
            pinned ? { text: qsTr("Открепить с главного экрана"), icon: "close", run: () => GameLibrary.unpin(game.id) }
                   : { text: qsTr("Закрепить на главном экране"), icon: "plus", run: () => GameLibrary.pin(game.id) }
        ]
        const d = downloadOf(game.id)
        if (game.installed)
            actions.unshift({ text: qsTr("Играть"), icon: "play", run: () => GameLauncher.launch(game.id) })
        if (game.installed) {
            actions.push({ text: qsTr("Удалить игру"), icon: "trash", run: () => root.confirmRemove(game) })
        } else if (d && d.status === "confirm") {
            actions.push({ text: qsTr("Подтвердить в Steam"), icon: "download", run: () => StoreManager.install(game.store, game.appId, game.title) })
            actions.push({ text: qsTr("Отменить загрузку"), icon: "close", run: () => StoreManager.cancel(game.id) })
        } else if (d && d.status === "failed") {
            actions.push({ text: qsTr("Повторить загрузку"), icon: "download", run: () => StoreManager.install(game.store, game.appId, game.title) })
            actions.push({ text: qsTr("Убрать из библиотеки"), icon: "trash", run: () => StoreManager.cancel(game.id) })
        } else {
            actions.push({ text: qsTr("Отменить загрузку"), icon: "close", run: () => StoreManager.cancel(game.id) })
        }
        actions.push({ text: qsTr("Отмена") })
        menu.show(game.title, statusText(game), actions, grid)
    }

    function confirmRemove(game) {
        menu.show(qsTr("Удалить «%1»?").arg(game.title),
                  qsTr("Игра будет удалена с консоли. Установить её снова можно из магазина."), [
            { text: qsTr("Удалить"), icon: "trash", run: () => StoreManager.uninstall(game.id) },
            { text: qsTr("Отмена") }
        ], grid)
    }

    ActionMenu { id: menu }

    EmptyState {
        visible: GameLibrary.count === 0
        anchors.centerIn: parent
        anchors.verticalCenterOffset: -Theme.px(40)
        icon: "library"
        title: qsTr("У вас нет игр")
        text: qsTr("Установите их из магазина.")
    }

    GridView {
        id: grid
        visible: GameLibrary.count > 0
        focus: true
        anchors.fill: parent
        anchors.bottomMargin: info.height + Theme.px(16)
        readonly property int columns: Math.max(3, Math.floor(width / Theme.px(250)))
        cellWidth: Math.floor(width / columns)
        cellHeight: cellWidth
        clip: true
        keyNavigationEnabled: true
        highlightMoveDuration: Theme.animFast
        model: GameLibrary.games

        Keys.onReturnPressed: if (currentIndex >= 0) root.play(GameLibrary.games[currentIndex])
        Keys.onEnterPressed: if (currentIndex >= 0) root.play(GameLibrary.games[currentIndex])
        Keys.onPressed: (event) => {
            if (event.key === Qt.Key_Menu && currentIndex >= 0) {
                root.activate(GameLibrary.games[currentIndex])
                event.accepted = true
            }
        }

        delegate: Item {
            id: cell
            required property var modelData
            required property int index
            width: grid.cellWidth
            height: grid.cellHeight

            readonly property var download: root.downloadOf(cell.modelData.id)

            GameCover {
                anchors.fill: parent
                anchors.margins: Theme.px(14)
                gameId: cell.modelData.id
                store: cell.modelData.store
                appId: cell.modelData.appId
                title: cell.modelData.title
                status: cell.modelData.installed ? "" : (cell.download ? cell.download.status : "queued")
                progress: cell.download ? cell.download.progress : -1
                active: cell.GridView.isCurrentItem && grid.activeFocus
                scale: active ? 1.04 : 1
                Behavior on scale { NumberAnimation { duration: Theme.animFast; easing.type: Easing.OutCubic } }
            }

            // Закреплена на главном экране
            Rectangle {
                visible: cell.modelData.pinned
                anchors.right: parent.right
                anchors.top: parent.top
                anchors.margins: Theme.px(22)
                width: Theme.px(30)
                height: width
                radius: width / 2
                color: Qt.rgba(0, 0, 0, 0.6)
                Icon {
                    anchors.centerIn: parent
                    name: "pin"
                    size: Theme.px(18)
                    lineWidth: 1.8
                    color: Theme.textPrimary
                }
            }

            MouseArea {
                anchors.fill: parent
                onClicked: {
                    grid.currentIndex = cell.index
                    grid.forceActiveFocus()
                    root.play(cell.modelData)
                }
            }
        }
    }

    // Выбранная игра
    Column {
        id: info
        visible: GameLibrary.count > 0 && grid.currentIndex >= 0
        anchors.left: parent.left
        anchors.bottom: parent.bottom
        anchors.leftMargin: Theme.px(14)
        spacing: Theme.px(4)

        readonly property var game: GameLibrary.games[grid.currentIndex] || ({})

        Text {
            text: info.game.title || ""
            color: Theme.textPrimary
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontLarge
            font.weight: Font.Light
        }
        Text {
            text: info.game.store ? root.statusText(info.game) + (info.game.pinned ? qsTr(" · на главном экране") : "") : ""
            color: Theme.textSecondary
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontSmall
        }
    }
}
