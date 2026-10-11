pragma ComponentBehavior: Bound
import QtQuick

// Оверлей по кнопке PS — поверх оболочки и поверх игры (отдельное прозрачное окно:
// композитор ставит его над окном игры).
// Снизу строка значков, название выбранного — над ним:
//   Игра (логотип; только пока игра запущена) — закрыть, перезапустить;
//   Звук — ползунок громкости;
//   Питание — выйти из системы, перезагрузить, выключить.
// PS или ○ на строке значков закрывают оверлей.
Window {
    id: overlay

    property Window mainWindow: null
    property bool fullscreen: false
    // Под cage каждое окно и так на весь экран
    property bool kiosk: false
    readonly property bool opened: content.opened

    function toggle() {
        if (content.opened)
            close()
        else
            open()
    }
    function open() {
        if (content.opened)
            return
        SystemControl.refreshVolume()
        content.panel = ""
        content.current = 0
        content.opened = true
        hideTimer.stop()
        fitToScreen()
        if (fullscreen && !kiosk)
            showFullScreen()
        else
            show()
        raise()
        requestActivate()
        row.forceActiveFocus()
    }
    // Размер окна задаём заранее, до показа: иначе первый кадр рисуется в старом размере
    // (по умолчанию 1280×720), и на экранах с другим соотношением сторон (1280×800,
    // 1366×768, портретные) снизу видна полоса, пока композитор не пришлёт настоящий размер.
    function fitToScreen() {
        if (mainWindow && mainWindow.screen)
            screen = mainWindow.screen
        const s = screen
        if (fullscreen || kiosk) {
            // Полный экран: ровно по экрану, на котором оболочка
            if (s && s.width > 0 && s.height > 0) {
                x = s.virtualX
                y = s.virtualY
                width = s.width
                height = s.height
            } else if (mainWindow) {
                width = mainWindow.width
                height = mainWindow.height
            }
        } else if (mainWindow) {
            // Окно: поверх главного окна, того же размера
            x = mainWindow.x
            y = mainWindow.y
            width = mainWindow.width
            height = mainWindow.height
        }
    }
    function close() {
        if (!content.opened)
            return
        content.opened = false
        hideTimer.restart()
    }

    objectName: "systemOverlay"   // GamepadManager отдаёт нажатия ему, пока он виден
    title: "RedAnchor"
    color: "transparent"
    flags: Qt.Window | Qt.FramelessWindowHint | Qt.WindowStaysOnTopHint
    visible: false
    width: Screen.width > 0 ? Screen.width : 1280
    height: Screen.height > 0 ? Screen.height : 720

    // Прячем окно после того, как содержимое погасло
    Timer {
        id: hideTimer
        interval: Theme.animNormal
        onTriggered: {
            overlay.hide()
            // Игры и Steam поверх нет — возвращаем управление оболочке
            if (GameLauncher.state !== "running" && !overlay.steamShown && overlay.mainWindow)
                overlay.mainWindow.requestActivate()
        }
    }

    property int artRevision: 0
    Connections {
        target: GameArt
        function onArtChanged(id) { overlay.artRevision++ }
    }

    readonly property bool gameRunning: GameLauncher.state === "running" || GameLauncher.state === "starting"
    readonly property var game: GameLauncher.game
    readonly property var gameArt: {
        overlay.artRevision
        return gameRunning && game && game.id ? GameArt.art(game.id, game.store, game.appId, game.title) : ({})
    }
    readonly property string steamPhase: (StoreManager.stores.steam || {}).phase || ""
    readonly property bool steamShown: steamPhase === "signin"
    readonly property var items: {
        const list = []
        if (gameRunning && game)
            list.push({ key: "game", title: game.title || qsTr("Игра") })
        if (steamShown)
            list.push({ key: "steam", title: qsTr("Steam") })
        list.push({ key: "sound", title: SystemControl.volume >= 0
                                         ? qsTr("Звук: %1%").arg(SystemControl.muted ? 0 : SystemControl.volume)
                                         : qsTr("Звук") })
        list.push({ key: "power", title: qsTr("Питание") })
        return list
    }
    // Игра закрылась, пока открыт оверлей — значок игры исчез
    onItemsChanged: content.current = Math.min(content.current, items.length - 1)

    FocusScope {
        id: content
        anchors.fill: parent
        focus: true

        property bool opened: false
        property int current: 0
        // "" — строка значков; "game" | "sound" | "power" — открыта панель значка
        property string panel: ""
        readonly property var currentItem: overlay.items[current] || ({})

        opacity: opened ? 1 : 0
        Behavior on opacity { NumberAnimation { duration: Theme.animNormal; easing.type: Easing.OutCubic } }

        Keys.onPressed: (event) => {
            if (event.key === Qt.Key_Escape) {
                if (content.panel !== "") {
                    content.panel = ""
                    row.forceActiveFocus()
                } else {
                    overlay.close()
                }
                event.accepted = true
            }
        }

        // Затемнение: сильнее к низу, слабее к верху
        Rectangle {
            anchors.fill: parent
            gradient: Gradient {
                GradientStop { position: 0.0; color: Qt.rgba(0.02, 0.022, 0.026, 0.25) }
                GradientStop { position: 0.55; color: Qt.rgba(0.02, 0.022, 0.026, 0.55) }
                GradientStop { position: 1.0; color: Qt.rgba(0.02, 0.022, 0.026, 0.92) }
            }
        }

        // ---------- Панель выбранного значка ----------
        Rectangle {
            id: panelBox
            visible: opacity > 0
            opacity: content.panel !== "" ? 1 : 0
            Behavior on opacity { NumberAnimation { duration: Theme.animFast } }
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.bottom: label.top
            anchors.bottomMargin: Theme.px(24)
            // На узких экранах панель не шире окна
            width: Math.min(Theme.px(520), overlay.width - 2 * Theme.px(24))
            // Высота — по загруженной панели (у Loader implicitHeight только для чтения)
            height: (panelContent.item ? panelContent.item.implicitHeight : 0) + Theme.px(48)
            radius: Theme.radius * 1.5
            color: Qt.rgba(0.07, 0.08, 0.1, 0.96)
            border.width: 1
            border.color: Theme.outline

            Loader {
                id: panelContent
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: parent.top
                anchors.margins: Theme.px(24)
                sourceComponent: content.panel === "sound" ? soundPanel
                                 : content.panel === "game" ? gamePanel
                                 : content.panel === "steam" ? steamPanel
                                 : content.panel === "power" ? powerPanel : null
                onLoaded: item.forceActiveFocus()
            }
        }

        // Название выбранного значка
        Text {
            id: label
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.bottom: row.top
            anchors.bottomMargin: Theme.px(22)
            text: content.currentItem.title || ""
            color: Theme.textPrimary
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontBody
            font.weight: Font.Medium
            opacity: content.panel === "" ? 1 : 0.6
        }

        // ---------- Строка значков ----------
        FocusScope {
            id: row
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.bottom: hints.top
            anchors.bottomMargin: Theme.px(40)
            width: icons.width
            height: icons.height

            Keys.onLeftPressed: content.current = Math.max(0, content.current - 1)
            Keys.onRightPressed: content.current = Math.min(overlay.items.length - 1, content.current + 1)
            Keys.onReturnPressed: content.panel = content.currentItem.key || ""
            Keys.onEnterPressed: content.panel = content.currentItem.key || ""

            Row {
                id: icons
                spacing: Theme.px(28)

                Repeater {
                    model: overlay.items
                    delegate: Item {
                        id: tile
                        required property var modelData
                        required property int index
                        readonly property bool selected: content.current === index
                        readonly property bool focused: selected && row.activeFocus

                        width: Theme.px(104)
                        height: width

                        Rectangle {
                            anchors.fill: parent
                            radius: Theme.radius * 1.5
                            color: tile.selected ? Qt.rgba(1, 1, 1, 0.16) : Qt.rgba(1, 1, 1, 0.07)
                            border.width: tile.focused ? Theme.px(3) : 1
                            border.color: tile.focused ? Theme.focus : Theme.outline
                            scale: tile.selected ? 1.12 : 1
                            Behavior on scale { NumberAnimation { duration: Theme.animFast; easing.type: Easing.OutCubic } }
                            Behavior on color { ColorAnimation { duration: Theme.animFast } }

                            // Игра: логотип, если его нет — обложка, если и её нет — значок
                            Image {
                                id: logo
                                visible: tile.modelData.key === "game" && status === Image.Ready
                                anchors.fill: parent
                                anchors.margins: Theme.px(12)
                                source: tile.modelData.key === "game" ? (overlay.gameArt.logo || "") : ""
                                fillMode: Image.PreserveAspectFit
                                asynchronous: true
                                sourceSize.width: Theme.px(160)
                            }
                            RoundedClip {
                                visible: tile.modelData.key === "game" && !logo.visible && cover.status === Image.Ready
                                anchors.fill: parent
                                anchors.margins: Theme.px(6)
                                radius: Theme.radius
                                Image {
                                    id: cover
                                    anchors.fill: parent
                                    source: tile.modelData.key === "game" ? (overlay.gameArt.grid || "") : ""
                                    fillMode: Image.PreserveAspectCrop
                                    asynchronous: true
                                    sourceSize.width: Theme.px(160)
                                }
                            }
                            Icon {
                                visible: tile.modelData.key !== "game" || (!logo.visible && cover.status !== Image.Ready)
                                anchors.centerIn: parent
                                size: Theme.px(40)
                                lineWidth: 1.5
                                name: tile.modelData.key === "game" ? "play"
                                    : tile.modelData.key === "steam" ? "steam"
                                    : tile.modelData.key === "sound" ? (SystemControl.muted || SystemControl.volume === 0 ? "volumeMute" : "volume")
                                    : "power"
                            }
                        }
                    }
                }
            }
        }

        HintBar {
            id: hints
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            anchors.rightMargin: Theme.gutter
            anchors.bottomMargin: Theme.px(48)
            model: content.panel === "sound"
                   ? [{ button: "ps", text: qsTr("Закрыть") }, { button: "circle", text: qsTr("Назад") }, { button: "dpad", text: qsTr("Громкость") }]
                   : [{ button: "ps", text: qsTr("Закрыть") }, { button: "circle", text: qsTr("Назад") }, { button: "cross", text: qsTr("Выбрать") }]
        }
    }

    // ---------- Панели ----------
    Component {
        id: gamePanel
        ActionList {
            actions: [
                { text: qsTr("Закрыть игру"), icon: "close", run: () => { overlay.close(); GameLauncher.stop() } },
                { text: qsTr("Перезапустить игру"), icon: "refresh", run: () => { overlay.close(); GameLauncher.restart() } }
            ]
        }
    }
    Component {
        id: steamPanel
        ActionList {
            actions: [
                { text: qsTr("Отменить вход в Steam"), icon: "close",
                  run: () => { overlay.close(); StoreManager.cancelSteamSignIn() } }
            ]
        }
    }
    Component {
        id: powerPanel
        ActionList {
            actions: [
                { text: qsTr("Выйти из системы"), icon: "logout", run: () => SystemControl.logout() },
                { text: qsTr("Перезагрузить консоль"), icon: "refresh", run: () => SystemControl.reboot() },
                { text: qsTr("Выключить консоль"), icon: "power", run: () => SystemControl.powerOff() }
            ]
        }
    }
    Component {
        id: soundPanel
        FocusScope {
            id: sound
            implicitHeight: soundColumn.implicitHeight
            readonly property int value: SystemControl.muted ? 0 : Math.max(0, SystemControl.volume)

            Keys.onLeftPressed: SystemControl.setVolume(sound.value - 5)
            Keys.onRightPressed: SystemControl.setVolume(sound.value + 5)
            // ✕ — выключить / включить звук
            Keys.onReturnPressed: SystemControl.setMuted(!SystemControl.muted)

            Column {
                id: soundColumn
                width: parent.width
                spacing: Theme.px(18)

                Row {
                    width: parent.width
                    spacing: Theme.px(18)
                    Icon {
                        name: sound.value === 0 ? "volumeMute" : "volume"
                        size: Theme.px(32)
                        lineWidth: 1.5
                        anchors.verticalCenter: parent.verticalCenter
                    }
                    // Ползунок
                    Item {
                        width: parent.width - Theme.px(32) - Theme.px(80) - 2 * parent.spacing
                        height: Theme.px(32)
                        anchors.verticalCenter: parent.verticalCenter
                        Rectangle {
                            id: track
                            anchors.verticalCenter: parent.verticalCenter
                            width: parent.width
                            height: Theme.px(6)
                            radius: height / 2
                            color: Qt.rgba(1, 1, 1, 0.14)
                            Rectangle {
                                width: parent.width * sound.value / 100
                                height: parent.height
                                radius: parent.radius
                                color: Theme.focus
                                Behavior on width { NumberAnimation { duration: Theme.animFast } }
                            }
                        }
                        Rectangle {
                            width: Theme.px(24)
                            height: width
                            radius: width / 2
                            color: Theme.focus
                            border.width: sound.activeFocus ? Theme.px(4) : 0
                            border.color: Qt.rgba(1, 1, 1, 0.35)
                            anchors.verticalCenter: parent.verticalCenter
                            x: track.width * sound.value / 100 - width / 2
                            Behavior on x { NumberAnimation { duration: Theme.animFast } }
                        }
                        MouseArea {
                            anchors.fill: parent
                            onPressed: (mouse) => SystemControl.setVolume(Math.round(mouse.x / width * 100))
                            onPositionChanged: (mouse) => SystemControl.setVolume(Math.round(mouse.x / width * 100))
                        }
                    }
                    Text {
                        width: Theme.px(80)
                        anchors.verticalCenter: parent.verticalCenter
                        horizontalAlignment: Text.AlignRight
                        text: SystemControl.volume < 0 ? "—" : sound.value + "%"
                        color: Theme.textPrimary
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontBody
                        font.features: { "tnum": 1 }
                    }
                }
                GlyphText {
                    text: SystemControl.volume < 0 ? qsTr("Звуковой сервер недоступен.")
                          : SystemControl.muted ? qsTr("{cross} — включить звук") : qsTr("{cross} — выключить звук")
                    color: Theme.textSecondary
                    pixelSize: Theme.fontSmall
                }
            }
        }
    }

    // Вертикальный список действий панели
    component ActionList: FocusScope {
        id: list
        property var actions: []
        property int current: 0
        implicitHeight: actionColumn.implicitHeight

        Keys.onUpPressed: list.current = Math.max(0, list.current - 1)
        Keys.onDownPressed: list.current = Math.min(list.actions.length - 1, list.current + 1)
        Keys.onReturnPressed: list.actions[list.current].run()
        Keys.onEnterPressed: list.actions[list.current].run()

        Column {
            id: actionColumn
            width: parent.width
            spacing: Theme.px(6)
            Repeater {
                model: list.actions
                delegate: Rectangle {
                    id: action
                    required property var modelData
                    required property int index
                    readonly property bool focused: list.current === index && list.activeFocus
                    width: actionColumn.width
                    height: Theme.px(64)
                    radius: Theme.radius
                    color: focused ? Theme.focus : "transparent"
                    Behavior on color { ColorAnimation { duration: Theme.animFast } }

                    Row {
                        anchors.left: parent.left
                        anchors.leftMargin: Theme.px(18)
                        anchors.verticalCenter: parent.verticalCenter
                        spacing: Theme.px(16)
                        Icon {
                            name: action.modelData.icon
                            size: Theme.px(28)
                            lineWidth: 1.5
                            color: action.focused ? Theme.background : Theme.textPrimary
                            anchors.verticalCenter: parent.verticalCenter
                        }
                        Text {
                            text: action.modelData.text
                            color: action.focused ? Theme.background : Theme.textPrimary
                            font.family: Theme.fontFamily
                            font.pixelSize: Theme.fontBody
                            anchors.verticalCenter: parent.verticalCenter
                        }
                    }
                    MouseArea {
                        anchors.fill: parent
                        onClicked: action.modelData.run()
                    }
                }
            }
        }
    }
}
