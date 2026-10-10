pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls.Basic
import QtCore

ApplicationWindow {
    id: window

    // Задаются из main.cpp
    property bool fullscreen: false
    property bool resetSetup: false

    width: 1280
    height: 720
    minimumWidth: 960
    minimumHeight: 540
    visible: true
    // Полный экран — только после первого кадра: cage аварийно завершается, если окно
    // просит полный экран до того, как показалось (wlr_xdg_surface: surface->initialized)
    property bool shown: false
    onFrameSwapped: if (!shown) shown = true
    visibility: fullscreen && shown ? Window.FullScreen : Window.Windowed
    title: "RedAnchor"
    color: Theme.background
    font.family: Theme.fontFamily

    Binding {
        target: Theme
        property: "scale"
        value: Math.min(window.width / 1920, window.height / 1080)
    }

    Settings {
        id: setup
        category: "Setup"
        property bool completed: false
        property string language: "ru"
    }

    readonly property var stepNames: [qsTr("Контроллер"), qsTr("Язык"), qsTr("Сеть"), qsTr("Обновления"), qsTr("Аккаунт")]
    readonly property SetupPage page: stack.currentItem as SetupPage

    function go(component, properties) {
        stack.replace(null, component, properties || {})
    }

    // DualSense подключили кабелем — сопрягаем его по Bluetooth, как на консоли.
    // Работает всегда, а не только в мастере настройки.
    // После настройки новый (ещё не сопряжённый) DualSense открывает отдельную сцену
    // сопряжения поверх текущего экрана.
    property string usbControllerMac: ""
    function checkUsbController() {
        // Ищем DualSense на кабеле среди всех геймпадов: активным может быть другой, беспроводной
        const pad = GamepadManager.gamepads.find(p => p.dualSense && p.connection === "usb" && p.mac !== "")
        const mac = pad ? String(pad.mac).toUpperCase() : ""
        if (mac === usbControllerMac)
            return
        usbControllerMac = mac
        if (mac === "" || !BluetoothManager.adapterAvailable)
            return
        const isNew = !BluetoothManager.isPairedDevice(mac)
        BluetoothManager.pairController(mac)
        if (isNew && setup.completed && stack.currentItem && stack.currentItem.objectName !== "newController")
            stack.push(newControllerPage, { mac: mac })
    }
    Connections {
        target: GamepadManager
        function onGamepadsChanged() { window.checkUsbController() }
        function onGamepadConnected(name, connection) {
            if (setup.completed)
                toast.show(connection === "bluetooth" ? "bluetooth" : "controller", qsTr("%1 подключён").arg(name),
                           connection === "bluetooth" ? qsTr("Bluetooth") : connection === "usb" ? qsTr("USB-кабель") : "")
        }
        function onBatteryLow(name, percent) {
            if (setup.completed)
                toast.show("battery", qsTr("%1: заряд %2%").arg(name).arg(percent),
                           qsTr("Подключите контроллер к зарядке"))
        }
        function onGamepadDisconnected(name, connection) {
            if (setup.completed)
                toast.show("controller", qsTr("%1 отключён").arg(name),
                           connection === "bluetooth" ? qsTr("Bluetooth") : connection === "usb" ? qsTr("USB-кабель") : "")
        }
    }
    Connections {
        target: BluetoothManager
        function onAdapterChanged() {
            // Адаптер появился позже контроллера
            if (!BluetoothManager.adapterAvailable || BluetoothManager.controllerPaired)
                return
            window.usbControllerMac = ""
            window.checkUsbController()
        }
    }

    Component.onCompleted: {
        checkUsbController()
        if (resetSetup)
            setup.completed = false
        if (!setup.completed)
            stack.push(introPage)
        else if (AccountManager.count === 0)
            stack.push(accountPage)     // учётных записей не осталось — создаём заново
        else
            stack.push(userSelectPage)
    }

    AppBackground {
        anchors.fill: parent
        animated: !(window.page && window.page.immersive) && GameLauncher.state !== "running"
    }
    // Однотонный фон для экранов на весь экран (магазин)
    Rectangle {
        anchors.fill: parent
        color: "#070b16"
        opacity: window.page && window.page.immersive ? 1 : 0
        visible: opacity > 0
        Behavior on opacity { NumberAnimation { duration: Theme.animNormal } }
    }

    // ---------- Экраны ----------
    Component {
        id: introPage
        IntroScreen { onNext: window.go(controllerPage) }
    }
    Component {
        id: controllerPage
        ControllerScreen { onNext: window.go(languagePage) }
    }
    Component {
        id: languagePage
        LanguageScreen {
            id: languageScreen
            onNext: {
                setup.language = languageScreen.language
                window.go(networkPage)
            }
        }
    }
    Component {
        id: networkPage
        NetworkScreen { onNext: window.go(updatesPage) }
    }
    Component {
        id: updatesPage
        UpdatesScreen { onNext: window.go(accountPage) }
    }
    Component {
        id: accountPage
        AccountScreen {
            onDone: window.go(welcomePage, { userName: AccountManager.nickname })
        }
    }
    Component {
        id: welcomePage
        WelcomeScreen {
            onNext: {
                setup.completed = true
                window.go(homePage)
            }
        }
    }
    Component {
        id: userSelectPage
        UserSelectScreen { onChosen: window.go(homePage) }
    }
    Component {
        id: newControllerPage
        NewControllerScreen {
            objectName: "newController"
            onFinished: if (stack.depth > 1) stack.pop()
        }
    }
    // Браузер для входа в GOG — отдельное большое окно
    Component {
        id: webLoginWindow
        WebLoginWindow {}
    }
    function openWebLogin(store) {
        const login = webLoginWindow.createObject(null, {
            store: store, fullscreen: window.fullscreen, transientParent: window,
            width: window.width, height: window.height
        })
        login.finished.connect(() => {
            // Сначала прячем: скрытое окно больше не получает нажатий геймпада
            login.hide()
            login.destroy()
            window.requestActivate()
        })
        login.show()
    }
    Component {
        id: homePage
        HomeScreen {
            onSwitchUserRequested: window.go(userSelectPage)
            onWebLoginRequested: (store) => window.openWebLogin(store)
        }
    }

    StackView {
        id: stack
        anchors.fill: parent
        focus: true

        replaceEnter: Transition {
            ParallelAnimation {
                NumberAnimation { property: "opacity"; from: 0; to: 1; duration: Theme.animSlow; easing.type: Easing.OutCubic }
                NumberAnimation { property: "x"; from: window.width * 0.06; to: 0; duration: Theme.animSlow; easing.type: Easing.OutCubic }
            }
        }
        replaceExit: Transition {
            ParallelAnimation {
                NumberAnimation { property: "opacity"; from: 1; to: 0; duration: Theme.animNormal; easing.type: Easing.InCubic }
                NumberAnimation { property: "x"; from: 0; to: -window.width * 0.06; duration: Theme.animNormal; easing.type: Easing.InCubic }
            }
        }
        pushEnter: Transition {
            NumberAnimation { property: "opacity"; from: 0; to: 1; duration: Theme.animSlow; easing.type: Easing.OutCubic }
        }
        pushExit: Transition {
            NumberAnimation { property: "opacity"; from: 1; to: 0; duration: Theme.animNormal; easing.type: Easing.InCubic }
        }
        popEnter: Transition {
            NumberAnimation { property: "opacity"; from: 0; to: 1; duration: Theme.animSlow; easing.type: Easing.OutCubic }
        }
        popExit: Transition {
            NumberAnimation { property: "opacity"; from: 1; to: 0; duration: Theme.animNormal; easing.type: Easing.InCubic }
        }
    }

    // ---------- Верхняя панель ----------
    Item {
        id: topBar
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.leftMargin: Theme.gutter
        anchors.rightMargin: Theme.gutter
        height: Theme.px(130)
        // На экранах «на весь экран» (магазин) верхняя панель скрывается
        opacity: window.page && window.page.immersive ? 0 : 1
        visible: opacity > 0
        Behavior on opacity { NumberAnimation { duration: Theme.animNormal } }

        StepIndicator {
            anchors.left: parent.left
            anchors.verticalCenter: parent.verticalCenter
            visible: window.page !== null && window.page.step >= 0
            steps: window.stepNames
            current: window.page ? window.page.step : 0
        }

        // Профиль текущего пользователя (главный экран)
        Row {
            anchors.left: parent.left
            anchors.verticalCenter: parent.verticalCenter
            visible: window.page !== null && window.page.showProfile && AccountManager.hasAccount
            spacing: Theme.px(14)

            Avatar {
                name: AccountManager.nickname
                size: Theme.px(38)
                anchors.verticalCenter: parent.verticalCenter
            }
            Text {
                anchors.verticalCenter: parent.verticalCenter
                text: AccountManager.nickname
                color: Theme.textPrimary
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontSmall
                font.weight: Font.Medium
            }
        }

        Row {
            anchors.right: parent.right
            anchors.verticalCenter: parent.verticalCenter
            spacing: Theme.px(26)

            // Активные загрузки игр; нет загрузок — значка нет
            Row {
                visible: StoreManager.activeDownloads > 0
                spacing: Theme.px(8)
                anchors.verticalCenter: parent.verticalCenter
                Icon {
                    name: "download"
                    size: Theme.px(26)
                    lineWidth: 1.5
                    color: Theme.textSecondary
                    anchors.verticalCenter: parent.verticalCenter
                }
                Text {
                    anchors.verticalCenter: parent.verticalCenter
                    text: (StoreManager.downloadProgress >= 0 ? Math.round(StoreManager.downloadProgress * 100) + "%" : "…")
                          + (StoreManager.activeDownloads > 1 ? qsTr(" · ещё %1").arg(StoreManager.activeDownloads - 1) : "")
                    color: Theme.textSecondary
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontSmall
                    font.features: { "tnum": 1 }
                }
            }
            // Bluetooth-гарнитура или наушники
            Icon {
                visible: BluetoothManager.audioConnected
                name: "headphones"
                size: Theme.px(26)
                lineWidth: 1.5
                color: Theme.textSecondary
                anchors.verticalCenter: parent.verticalCenter
            }
            Icon {
                visible: NetworkBackend.online
                name: NetworkBackend.wiredConnected ? "ethernet" : "globe"
                size: Theme.px(26)
                lineWidth: 1.5
                color: Theme.textSecondary
                anchors.verticalCenter: parent.verticalCenter
            }
            Text {
                id: clock
                anchors.verticalCenter: parent.verticalCenter
                color: Theme.textPrimary
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontBody
                font.weight: Font.Light
                font.features: { "tnum": 1 }
                text: Qt.formatTime(new Date(), "HH:mm")
                Timer {
                    interval: 1000
                    running: true
                    repeat: true
                    onTriggered: clock.text = Qt.formatTime(new Date(), "HH:mm")
                }
            }
        }
    }

    // Магазины: установка и удаление игр
    Connections {
        target: StoreManager
        function onGameInstalled(title) { toast.show("download", qsTr("%1 установлена").arg(title), qsTr("Игра в библиотеке")) }
        function onGameRemoved(title) { toast.show("trash", qsTr("%1 удалена").arg(title), "") }
    }

    // ---------- Запуск игры ----------
    GameLaunchOverlay {
        anchors.fill: parent
        z: 30
    }
    // Пока идёт игра, геймпад не должен управлять оболочкой в фоне
    Binding {
        target: GamepadManager
        property: "keyEventsEnabled"
        value: false
        when: GameLauncher.state === "running"
    }

    // ---------- Уведомления сверху по центру ----------
    Toast {
        id: toast
        anchors.horizontalCenter: parent.horizontalCenter
        y: Theme.px(31)
        z: 20
    }

    // ---------- Экранная клавиатура и подсказки ----------
    OnScreenKeyboard {
        id: keyboard
        width: parent.width
        z: 10
    }

    HintBar {
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.rightMargin: Theme.gutter
        anchors.bottomMargin: Theme.px(48)
        z: 11
        model: Keyboard.active ? keyboard.hints : (window.page ? window.page.hints : [])
    }
}
