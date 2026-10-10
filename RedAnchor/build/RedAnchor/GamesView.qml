pragma ComponentBehavior: Bound
import QtQuick
import "Format.js" as Format

// Вкладка «Мои игры»: закреплённые игры рядом крупных карточек, выбранная — больше
// и держится у левого края; под ними название и сведения. Широкую картинку выбранной
// игры (hero) главный экран показывает фоном — см. heroSource.
FocusScope {
    id: root

    readonly property bool focusable: GameLibrary.pinned.length > 0
    // Фокус внутри раздела (не на меню вкладок): выбранная карточка крупнее, видны сведения
    readonly property bool engaged: row.activeFocus || menu.visible
    readonly property var hints: current && current.installed
                                 ? [{ button: "options", text: qsTr("Меню") }, { button: "cross", text: qsTr("Играть") }]
                                 : [{ button: "options", text: qsTr("Меню") }]
    readonly property var current: GameLibrary.pinned[row.currentIndex] || null
    // Фон для главного экрана
    property string heroSource: ""

    function updateHero() {
        if (!current) {
            heroSource = ""
            return
        }
        heroSource = GameArt.art(current.id, current.store, current.appId, current.title).hero || ""
    }

    onCurrentChanged: updateHero()
    Component.onCompleted: updateHero()

    Connections {
        target: GameArt
        function onArtChanged(id) {
            if (root.current && id === root.current.id)
                root.updateHero()
        }
    }

    function downloadOf(id) {
        for (const d of StoreManager.downloads) {
            if (d.id === id)
                return d
        }
        return null
    }

    // ✕ — играть (загружаемая игра — меню), Options — меню игры
    function play(game) {
        if (game.installed)
            GameLauncher.launch(game.id)
        else
            activate(game)
    }

    function activate(game) {
        const actions = []
        if (game.installed)
            actions.push({ text: qsTr("Играть"), icon: "play", run: () => GameLauncher.launch(game.id) })
        menu.show(game.title, Format.storeName(game.store), actions.concat([
            { text: qsTr("Открепить с главного экрана"), icon: "close", run: () => GameLibrary.unpin(game.id) },
            game.installed ? { text: qsTr("Удалить игру"), icon: "trash", run: () => root.confirmRemove(game) }
                           : { text: qsTr("Отменить загрузку"), icon: "close", run: () => StoreManager.cancel(game.id) },
            { text: qsTr("Отмена") }
        ]), row)
    }

    function confirmRemove(game) {
        menu.show(qsTr("Удалить «%1»?").arg(game.title),
                  qsTr("Игра будет удалена с консоли. Установить её снова можно из магазина."), [
            { text: qsTr("Удалить"), icon: "trash", run: () => StoreManager.uninstall(game.id) },
            { text: qsTr("Отмена") }
        ], row)
    }

    ActionMenu { id: menu }

    EmptyState {
        visible: GameLibrary.pinned.length === 0
        anchors.centerIn: parent
        anchors.verticalCenterOffset: -Theme.px(40)
        icon: "controller"
        title: qsTr("Игр пока нет")
        text: GameLibrary.count > 0 ? qsTr("Закрепите игры из библиотеки — они появятся здесь.")
                                    : qsTr("Добавьте их через библиотеку.")
    }

    // ---------- Карточки ----------
    ListView {
        id: row
        visible: GameLibrary.pinned.length > 0
        focus: true
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.leftMargin: -Theme.px(8)
        height: cardSize * 1.2 + Theme.px(20)
        orientation: ListView.Horizontal
        spacing: Theme.px(22)
        clip: false
        keyNavigationEnabled: true
        // Выбранная карточка держится правее левого края — предыдущая видна наполовину.
        // ApplyRange: у начала списка первая карточка стоит у края, без пустоты слева.
        highlightRangeMode: ListView.ApplyRange
        preferredHighlightBegin: cardSize * 0.5 + spacing
        preferredHighlightEnd: preferredHighlightBegin + cardSize * 1.2
        // Прокрутка — той же длительности и кривой, что и увеличение карточки
        highlightMoveDuration: Theme.animSlow
        highlightMoveVelocity: -1
        model: GameLibrary.pinned

        readonly property real cardSize: Theme.px(230)

        Keys.onReturnPressed: if (root.current) root.play(root.current)
        Keys.onEnterPressed: if (root.current) root.play(root.current)
        Keys.onPressed: (event) => {
            if (event.key === Qt.Key_Menu && root.current) {
                root.activate(root.current)
                event.accepted = true
            }
        }

        delegate: Item {
            id: card
            required property var modelData
            required property int index
            readonly property bool selected: ListView.isCurrentItem
            readonly property var download: root.downloadOf(modelData.id)
            // Крупнее — только выбранная и только когда фокус в разделе
            width: (selected && root.engaged ? 1.2 : 1) * row.cardSize
            height: row.height
            Behavior on width { NumberAnimation { duration: Theme.animSlow; easing.type: Easing.OutQuint } }

            GameCover {
                anchors.left: parent.left
                anchors.top: parent.top
                anchors.topMargin: Theme.px(10)
                width: parent.width
                height: width
                gameId: card.modelData.id
                store: card.modelData.store
                appId: card.modelData.appId
                title: card.modelData.title
                status: card.modelData.installed ? "" : (card.download ? card.download.status : "queued")
                progress: card.download ? card.download.progress : -1
                showStore: false
                active: card.selected && root.engaged
                opacity: card.selected || !root.engaged ? 1 : 0.75
                Behavior on opacity { NumberAnimation { duration: Theme.animNormal } }
            }

            MouseArea {
                anchors.fill: parent
                onClicked: {
                    row.currentIndex = card.index
                    row.forceActiveFocus()
                    root.play(card.modelData)
                }
            }
        }
    }

    // ---------- Выбранная игра ----------
    // Сведения видны, только когда фокус в разделе
    Column {
        visible: opacity > 0
        opacity: root.current !== null && root.engaged ? 1 : 0
        Behavior on opacity { NumberAnimation { duration: Theme.animNormal } }
        anchors.left: parent.left
        anchors.top: row.bottom
        anchors.topMargin: Theme.px(36)
        spacing: Theme.px(22)

        Text {
            text: root.current ? root.current.title : ""
            color: Theme.textPrimary
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontHuge
            font.weight: Font.Medium
        }

        Row {
            spacing: Theme.px(64)

            Repeater {
                model: root.current ? [
                    { label: qsTr("Магазин"), value: Format.storeName(root.current.store) },
                    { label: qsTr("Размер"), value: !root.current.installed
                                                    ? (root.downloadOf(root.current.id) && root.downloadOf(root.current.id).progress >= 0
                                                       ? qsTr("Загрузка %1%").arg(Math.round(root.downloadOf(root.current.id).progress * 100))
                                                       : qsTr("Загружается"))
                                                    : root.current.sizeBytes > 0 ? Format.bytes(root.current.sizeBytes) : "—" },
                    { label: qsTr("Установлена"), value: root.current.installedAt > 0
                                                         ? new Date(root.current.installedAt * 1000).toLocaleDateString(Qt.locale(), "d MMMM yyyy")
                                                         : "—" }
                ] : []
                delegate: Column {
                    id: stat
                    required property var modelData
                    spacing: Theme.px(6)
                    Text {
                        text: stat.modelData.label
                        color: Theme.textSecondary
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontSmall
                    }
                    Text {
                        text: stat.modelData.value
                        color: Theme.textPrimary
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontLarge
                        font.weight: Font.Light
                    }
                }
            }
        }
    }
}
