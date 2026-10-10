pragma ComponentBehavior: Bound
import QtQuick

// Подключение к сети: список Wi-Fi сетей с вводом пароля, а если Wi-Fi модуля
// нет — состояние кабеля Ethernet. Используется в мастере настройки и в настройках.
FocusScope {
    id: root

    property string mode: "list"            // "list" | "password"
    property string selectedSsid
    property string lastError
    // Куда уходит фокус вниз из списка (кнопка «Продолжить» в мастере)
    property Item downTarget: null
    readonly property bool connecting: NetworkBackend.connectState === NetworkBackend.Connecting
    readonly property bool listVisible: NetworkBackend.wifiAvailable && NetworkBackend.wifiEnabled

    readonly property var entries: {
        const list = []
        if (NetworkBackend.wiredConnected)
            list.push({ kind: "wired", ssid: qsTr("Проводная сеть"), strength: 100, secured: false, active: true, saved: true })
        for (const n of NetworkBackend.networks)
            list.push(Object.assign({ kind: "wifi" }, n))
        return list
    }

    // Выбрана сеть, к которой уже подключены (в настройках — предложить отключиться)
    signal activeNetworkChosen(var entry)
    // Подключение установлено
    signal connected()

    function focusList() {
        if (listVisible && networkList.count > 0)
            networkList.forceActiveFocus()
        else if (downTarget)
            downTarget.forceActiveFocus()
    }

    function activate(entry) {
        lastError = ""
        if (entry.kind === "wired" || entry.active) {
            root.activeNetworkChosen(entry)
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
        focusList()
    }

    Component.onCompleted: {
        NetworkBackend.resetConnectState()
        NetworkBackend.scan()
    }

    Connections {
        target: NetworkBackend
        function onStatusChanged() {
            // Wi-Fi адаптер пропал или выключен — уводим фокус со скрытого списка
            if (!root.listVisible && root.mode === "password")
                root.backToList()
        }
        function onConnectStateChanged() {
            if (NetworkBackend.connectState === NetworkBackend.Connected) {
                root.mode = "list"
                root.connected()
            } else if (NetworkBackend.connectState === NetworkBackend.Failed) {
                if (root.mode === "password") {
                    passwordField.errorText = NetworkBackend.errorString
                    passwordField.forceActiveFocus()
                } else {
                    root.lastError = NetworkBackend.errorString
                }
            }
        }
    }

    Keys.onSpacePressed: (event) => {
        if (mode === "list" && listVisible) {
            NetworkBackend.scan()
            event.accepted = true
        } else {
            event.accepted = false
        }
    }
    Keys.onEscapePressed: (event) => {
        if (mode === "password" && !connecting) {
            backToList()
            event.accepted = true
        } else {
            event.accepted = false
        }
    }

    // ---------- Список сетей ----------
    Item {
        anchors.fill: parent
        visible: root.mode === "list"

        // ---------- Wi-Fi нет или выключен: состояние кабеля ----------
        Column {
            visible: !root.listVisible
            spacing: Theme.px(28)
            width: Math.min(parent.width, Theme.px(900))

            Text {
                visible: NetworkBackend.wifiAvailable && !NetworkBackend.wifiEnabled
                width: parent.width
                wrapMode: Text.WordWrap
                text: qsTr("Wi-Fi выключен.")
                color: Theme.textSecondary
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontSmall
            }

            Row {
                spacing: Theme.px(22)
                Icon {
                    name: "ethernet"
                    size: Theme.px(44)
                    lineWidth: 1.3
                    anchors.verticalCenter: parent.verticalCenter
                }
                Column {
                    anchors.verticalCenter: parent.verticalCenter
                    spacing: Theme.px(6)
                    Text {
                        text: NetworkBackend.wiredConnected ? qsTr("Кабель подключён") : qsTr("Подключите кабель Ethernet")
                        color: Theme.textPrimary
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontLarge
                        font.weight: Font.Light
                    }
                    Text {
                        text: NetworkBackend.wiredConnected
                              ? (NetworkBackend.online ? qsTr("Есть доступ к сети") : qsTr("Получение адреса…"))
                              : qsTr("Вставьте сетевой кабель в разъём LAN на консоли или роутере")
                        color: Theme.textSecondary
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontSmall
                    }
                }
                Spinner {
                    visible: NetworkBackend.wiredConnected && !NetworkBackend.online
                    size: Theme.px(28)
                    anchors.verticalCenter: parent.verticalCenter
                }
                Icon {
                    visible: NetworkBackend.online && NetworkBackend.wiredConnected
                    name: "check"
                    size: Theme.px(28)
                    color: Theme.success
                    anchors.verticalCenter: parent.verticalCenter
                }
            }
        }

        Text {
            id: listHeader
            visible: root.listVisible
            text: NetworkBackend.scanning ? qsTr("Поиск сетей…") : qsTr("Доступные сети")
            color: Theme.textSecondary
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontSmall
        }

        ListView {
            id: networkList
            visible: root.listVisible
            focus: true
            anchors.top: listHeader.bottom
            anchors.topMargin: Theme.px(16)
            anchors.bottom: errorText.visible ? errorText.top : parent.bottom
            anchors.bottomMargin: Theme.px(16)
            width: Math.min(parent.width, Theme.px(1000))
            clip: true
            spacing: Theme.px(12)
            keyNavigationEnabled: true
            highlightMoveDuration: Theme.animFast
            model: root.entries

            KeyNavigation.down: root.downTarget
            KeyNavigation.priority: KeyNavigation.AfterItem

            Keys.onReturnPressed: if (currentIndex >= 0) root.activate(root.entries[currentIndex])
            Keys.onEnterPressed: if (currentIndex >= 0) root.activate(root.entries[currentIndex])

            delegate: ConsoleListItem {
                id: netItem
                required property var modelData
                required property int index
                width: ListView.view.width
                icon: modelData.kind === "wired" ? "ethernet" : ""
                title: modelData.ssid
                subtitle: modelData.active ? qsTr("Подключено")
                        : root.connecting && root.selectedSsid === modelData.ssid ? qsTr("Подключение…")
                        : modelData.saved ? qsTr("Сохранена")
                        : modelData.secured ? qsTr("Защищённая сеть") : qsTr("Открытая сеть")
                selected: modelData.active
                current: ListView.isCurrentItem && networkList.activeFocus
                onClicked: {
                    networkList.currentIndex = index
                    networkList.forceActiveFocus()
                    root.activate(modelData)
                }

                Spinner {
                    visible: root.connecting && root.selectedSsid === netItem.modelData.ssid
                    size: Theme.px(26)
                    anchors.verticalCenter: parent.verticalCenter
                }
                Icon {
                    visible: netItem.modelData.secured
                    name: "lock"
                    lineWidth: 1.5
                    size: Theme.px(22)
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

        Text {
            id: errorText
            anchors.bottom: parent.bottom
            visible: root.lastError !== ""
            text: root.lastError
            color: Theme.accent
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontSmall
        }
    }

    // ---------- Ввод пароля ----------
    Column {
        visible: root.mode === "password"
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
                text: root.selectedSsid
                color: Theme.textPrimary
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontLarge
                font.weight: Font.Light
            }
        }

        ConsoleTextField {
            id: passwordField
            width: parent.width
            label: qsTr("Пароль")
            placeholder: qsTr("Пароль не введён")
            password: true
            KeyNavigation.down: connectButton
            onAccepted: root.connectWithPassword()
        }

        Row {
            spacing: Theme.px(20)
            ConsoleButton {
                id: connectButton
                text: root.connecting ? qsTr("Подключение…") : qsTr("Подключиться")
                primary: true
                enabledState: !root.connecting
                KeyNavigation.up: passwordField
                KeyNavigation.right: cancelButton
                onClicked: root.connectWithPassword()
            }
            ConsoleButton {
                id: cancelButton
                text: qsTr("Назад")
                enabledState: !root.connecting
                KeyNavigation.up: passwordField
                KeyNavigation.left: connectButton
                onClicked: root.backToList()
            }
            Spinner {
                visible: root.connecting
                anchors.verticalCenter: parent.verticalCenter
            }
        }
    }
}
