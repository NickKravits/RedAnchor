pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls.Basic
import QtCore

ApplicationWindow {
    id: window

    // Задаются из main.cpp
    property bool fullscreen: false
    property bool resetSetup: false
    property bool demoMode: false

    width: 1280
    height: 720
    minimumWidth: 960
    minimumHeight: 540
    visible: true
    visibility: fullscreen ? Window.FullScreen : Window.Windowed
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
        property string accountEmail: ""
        property string accountName: ""
    }

    readonly property var stepNames: [qsTr("Контроллер"), qsTr("Язык"), qsTr("Сеть"), qsTr("Аккаунт")]
    readonly property SetupPage page: stack.currentItem as SetupPage

    function go(component, properties) {
        stack.replace(null, component, properties || {})
    }

    function controllerConfirmed() {
        // Сопряжение по Bluetooth имеет смысл только для DualSense на кабеле.
        if (GamepadManager.dualSense && GamepadManager.connection === "usb")
            go(pairingPage)
        else
            go(languagePage)
    }

    Component.onCompleted: {
        if (resetSetup)
            setup.completed = false
        if (setup.completed)
            stack.push(homePage, { userName: setup.accountName })
        else
            stack.push(controllerPage)
    }

    AppBackground {
        anchors.fill: parent
    }

    // ---------- Экраны ----------
    Component {
        id: controllerPage
        ControllerScreen { onNext: window.controllerConfirmed() }
    }
    Component {
        id: pairingPage
        PairingScreen {
            onNext: window.go(languagePage)
            onSkipped: window.go(languagePage)
        }
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
        NetworkScreen { onNext: window.go(accountPage) }
    }
    Component {
        id: accountPage
        AccountScreen {
            onSignedIn: (email, displayName) => {
                setup.accountEmail = email
                setup.accountName = displayName
                window.go(welcomePage, { userName: displayName })
            }
            onSkipped: {
                setup.accountEmail = ""
                setup.accountName = ""
                window.go(welcomePage, { userName: "" })
            }
        }
    }
    Component {
        id: welcomePage
        WelcomeScreen {
            onNext: {
                setup.completed = true
                window.go(homePage, { userName: setup.accountName })
            }
        }
    }
    Component {
        id: homePage
        HomeScreen {
            onResetRequested: {
                setup.completed = false
                window.go(controllerPage)
            }
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
            NumberAnimation { property: "opacity"; from: 0; to: 1; duration: 900; easing.type: Easing.OutCubic }
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

        Row {
            anchors.verticalCenter: parent.verticalCenter
            spacing: Theme.px(14)
            Icon {
                name: "anchor"
                color: Theme.accent
                size: Theme.px(36)
                anchors.verticalCenter: parent.verticalCenter
            }
            Text {
                anchors.verticalCenter: parent.verticalCenter
                text: "RedAnchor"
                color: Theme.textPrimary
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontLarge
                font.weight: Font.Bold
                font.letterSpacing: 0.5
            }
            Rectangle {
                visible: window.demoMode || BluetoothManager.demo || NetworkBackend.demo
                anchors.verticalCenter: parent.verticalCenter
                width: demoText.implicitWidth + Theme.px(20)
                height: Theme.px(30)
                radius: height / 2
                color: "transparent"
                border.color: Theme.warning
                border.width: Math.max(1, Theme.px(1.5))
                Text {
                    id: demoText
                    anchors.centerIn: parent
                    text: qsTr("ДЕМО")
                    color: Theme.warning
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.px(15)
                    font.weight: Font.DemiBold
                }
            }
        }

        StepIndicator {
            anchors.centerIn: parent
            visible: window.page !== null && window.page.step >= 0
            steps: window.stepNames
            current: window.page ? window.page.step : 0
        }

        Row {
            anchors.right: parent.right
            anchors.verticalCenter: parent.verticalCenter
            spacing: Theme.px(26)

            Row {
                visible: GamepadManager.connected
                spacing: Theme.px(8)
                anchors.verticalCenter: parent.verticalCenter
                Icon {
                    name: GamepadManager.connection === "bluetooth" ? "bluetooth" : "controller"
                    size: Theme.px(28)
                    color: Theme.textSecondary
                    anchors.verticalCenter: parent.verticalCenter
                }
                Text {
                    anchors.verticalCenter: parent.verticalCenter
                    text: GamepadManager.connection === "usb" ? "USB" : GamepadManager.connection === "bluetooth" ? "BT" : ""
                    color: Theme.textSecondary
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontSmall
                }
            }
            Icon {
                visible: NetworkBackend.online
                name: NetworkBackend.wiredConnected ? "ethernet" : "globe"
                size: Theme.px(28)
                color: Theme.textSecondary
                anchors.verticalCenter: parent.verticalCenter
            }
            Text {
                id: clock
                anchors.verticalCenter: parent.verticalCenter
                color: Theme.textPrimary
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontLarge
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
