pragma ComponentBehavior: Bound
import QtQuick

// Шаг 4: подключение к сети (Ethernet или Wi-Fi с вводом пароля).
SetupPage {
    id: page

    property string mode: "list"            // "list" | "password"
    property string selectedSsid
    property string lastError
    readonly property bool connecting: NetworkBackend.connectState === NetworkBackend.Connecting

    readonly property var entries: {
        const list = []
        if (NetworkBackend.wiredConnected)
            list.push({ kind: "wired", ssid: qsTr("Проводная сеть"), strength: 100, secured: false, active: true, saved: true })
        for (const n of NetworkBackend.networks)
            list.push(Object.assign({ kind: "wifi" }, n))
        return list
    }

    step: 2
    title: qsTr("Подключение к интернету")
    subtitle: NetworkBackend.wiredConnected ? qsTr("Консоль подключена по кабелю. Можно продолжать.")
            : NetworkBackend.activeSsid !== "" ? qsTr("Подключено к сети «%1».").arg(NetworkBackend.activeSsid)
            : !NetworkBackend.wifiAvailable ? qsTr("Wi-Fi адаптер не найден. Подключите кабель Ethernet.")
            : qsTr("Выберите сеть Wi-Fi или подключите кабель Ethernet.")

    hints: mode === "password"
           ? [{ button: "circle", text: qsTr("Назад") }, { button: "cross", text: qsTr("Выбрать") }]
           : [{ button: "triangle", text: qsTr("Обновить") }, { button: "cross", text: qsTr("Выбрать") }]

    initialFocus: NetworkBackend.online ? continueButton : networkList

    Component.onCompleted: {
        NetworkBackend.resetConnectState()
        NetworkBackend.scan()
    }

    function activate(entry) {
        lastError = ""
        if (entry.kind === "wired" || entry.active) {
            continueButton.forceActiveFocus()
            return
        }
        selectedSsid = entry.ssid
        if (entry.secured && !entry.saved) {
            passwordField.text = ""
            passwordField.errorText = ""
            mode = "password"
            passwordField.forceActiveFocus()
            Keyboard.open(passwordField)
        } else {
            NetworkBackend.connectToNetwork(entry.ssid, "")
        }
    }

    function connectWithPassword() {
        if (passwordField.text.length < 8) {
            passwordField.errorText = qsTr("Пароль Wi-Fi должен содержать не менее 8 символов.")
            passwordField.forceActiveFocus()
            return
        }
        passwordField.errorText = ""
        NetworkBackend.connectToNetwork(selectedSsid, passwordField.text)
    }

    function backToList() {
        mode = "list"
        NetworkBackend.resetConnectState()
        networkList.forceActiveFocus()
    }

    Connections {
        target: NetworkBackend
        function onConnectStateChanged() {
            if (NetworkBackend.connectState === NetworkBackend.Connected) {
                page.mode = "list"
                continueButton.forceActiveFocus()
            } else if (NetworkBackend.connectState === NetworkBackend.Failed) {
                if (page.mode === "password") {
                    passwordField.errorText = NetworkBackend.errorString
                    passwordField.forceActiveFocus()
                } else {
                    page.lastError = NetworkBackend.errorString
                }
            }
        }
    }

    Keys.onSpacePressed: if (mode === "list") NetworkBackend.scan()
    Keys.onEscapePressed: if (mode === "password" && !connecting) backToList()

    // ---------- Список сетей ----------
    Item {
        id: listMode
        anchors.fill: parent
        visible: page.mode === "list"

        Text {
            id: listHeader
            text: NetworkBackend.scanning ? qsTr("Поиск сетей…") : qsTr("Доступные сети")
            color: Theme.textSecondary
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontSmall
        }

        ListView {
            id: networkList
            anchors.top: listHeader.bottom
            anchors.topMargin: Theme.px(16)
            anchors.bottom: buttons.top
            anchors.bottomMargin: Theme.px(28)
            width: Math.min(parent.width, Theme.px(1000))
            clip: true
            spacing: Theme.px(12)
            keyNavigationEnabled: true
            highlightMoveDuration: Theme.animFast
            model: page.entries

            KeyNavigation.down: continueButton
            KeyNavigation.priority: KeyNavigation.AfterItem

            Keys.onReturnPressed: if (currentIndex >= 0) page.activate(page.entries[currentIndex])
            Keys.onEnterPressed: if (currentIndex >= 0) page.activate(page.entries[currentIndex])

            delegate: ConsoleListItem {
                id: netItem
                required property var modelData
                required property int index
                width: ListView.view.width
                icon: modelData.kind === "wired" ? "ethernet" : ""
                title: modelData.ssid
                subtitle: modelData.active ? qsTr("Подключено")
                        : page.connecting && page.selectedSsid === modelData.ssid ? qsTr("Подключение…")
                        : modelData.saved ? qsTr("Сохранена")
                        : modelData.secured ? qsTr("Защищённая сеть") : qsTr("Открытая сеть")
                selected: modelData.active
                current: ListView.isCurrentItem && networkList.activeFocus
                onClicked: {
                    networkList.currentIndex = index
                    networkList.forceActiveFocus()
                    page.activate(modelData)
                }

                Spinner {
                    visible: page.connecting && page.selectedSsid === netItem.modelData.ssid
                    size: Theme.px(26)
                    anchors.verticalCenter: parent.verticalCenter
                }
                Icon {
                    visible: netItem.modelData.secured
                    name: "lock"
                    size: Theme.px(24)
                    color: Theme.textSecondary
                    anchors.verticalCenter: parent.verticalCenter
                }
                SignalBars {
                    visible: netItem.modelData.kind === "wifi"
                    strength: netItem.modelData.strength
                    anchors.verticalCenter: parent.verticalCenter
                }
            }

            Text {
                anchors.centerIn: parent
                visible: networkList.count === 0
                text: NetworkBackend.scanning ? "" : qsTr("Сети не найдены")
                color: Theme.textMuted
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontBody
            }
        }

        Row {
            id: buttons
            anchors.bottom: parent.bottom
            spacing: Theme.px(20)

            ConsoleButton {
                id: continueButton
                text: NetworkBackend.online ? qsTr("Продолжить") : qsTr("Пропустить")
                icon: "arrowRight"
                primary: NetworkBackend.online
                KeyNavigation.up: networkList
                KeyNavigation.right: refreshButton
                onClicked: page.next()
            }
            ConsoleButton {
                id: refreshButton
                text: qsTr("Обновить")
                icon: "refresh"
                KeyNavigation.up: networkList
                KeyNavigation.left: continueButton
                onClicked: NetworkBackend.scan()
            }
            Text {
                anchors.verticalCenter: parent.verticalCenter
                visible: page.lastError !== ""
                text: page.lastError
                color: Theme.accentSoft
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontSmall
            }
        }
    }

    // ---------- Ввод пароля ----------
    Column {
        id: passwordMode
        visible: page.mode === "password"
        width: Math.min(parent.width, Theme.px(860))
        spacing: Theme.px(30)

        Row {
            spacing: Theme.px(16)
            Icon {
                name: "lock"
                anchors.verticalCenter: parent.verticalCenter
            }
            Text {
                anchors.verticalCenter: parent.verticalCenter
                text: page.selectedSsid
                color: Theme.textPrimary
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontLarge
                font.weight: Font.DemiBold
            }
        }

        ConsoleTextField {
            id: passwordField
            width: parent.width
            label: qsTr("Пароль")
            placeholder: qsTr("Нажмите ✕, чтобы ввести пароль")
            password: true
            KeyNavigation.down: connectButton
            onAccepted: page.connectWithPassword()
        }

        Row {
            spacing: Theme.px(20)
            ConsoleButton {
                id: connectButton
                text: page.connecting ? qsTr("Подключение…") : qsTr("Подключиться")
                primary: true
                enabledState: !page.connecting
                KeyNavigation.up: passwordField
                KeyNavigation.right: cancelButton
                onClicked: page.connectWithPassword()
            }
            ConsoleButton {
                id: cancelButton
                text: qsTr("Назад")
                enabledState: !page.connecting
                KeyNavigation.up: passwordField
                KeyNavigation.left: connectButton
                onClicked: page.backToList()
            }
            Spinner {
                visible: page.connecting
                anchors.verticalCenter: parent.verticalCenter
            }
        }
    }
}
