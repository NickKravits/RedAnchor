import QtQuick

// Шаг 4: подключение к сети (Ethernet или Wi-Fi с вводом пароля).
SetupPage {
    id: page

    step: 2
    title: qsTr("Подключение к интернету")
    subtitle: NetworkBackend.wiredConnected ? qsTr("Консоль подключена по кабелю. Можно продолжать.")
            : NetworkBackend.activeSsid !== "" ? qsTr("Подключено к сети «%1».").arg(NetworkBackend.activeSsid)
            : !NetworkBackend.wifiAvailable ? qsTr("Wi-Fi модуль не найден. Подключите кабель Ethernet или пропустите этот шаг.")
            : qsTr("Выберите сеть Wi-Fi или подключите кабель Ethernet.")

    hints: panel.mode === "password"
           ? [{ button: "circle", text: qsTr("Назад") }, { button: "cross", text: qsTr("Выбрать") }]
           : !panel.listVisible ? [{ button: "cross", text: qsTr("Выбрать") }]
           : [{ button: "triangle", text: qsTr("Обновить") }, { button: "cross", text: qsTr("Выбрать") }]

    initialFocus: NetworkBackend.online || !panel.listVisible ? continueButton : panel

    NetworkPanel {
        id: panel
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.bottom: mode === "password" ? parent.bottom : buttons.top
        anchors.bottomMargin: mode === "password" ? 0 : Theme.px(28)
        downTarget: continueButton
        onActiveNetworkChosen: continueButton.forceActiveFocus()
        onConnected: continueButton.forceActiveFocus()
    }

    Connections {
        target: NetworkBackend
        function onStatusChanged() {
            if (!panel.listVisible && panel.activeFocus && panel.mode === "list")
                continueButton.forceActiveFocus()
        }
    }

    Row {
        id: buttons
        visible: panel.mode === "list"
        anchors.bottom: parent.bottom
        spacing: Theme.px(20)

        ConsoleButton {
            id: continueButton
            text: NetworkBackend.online ? qsTr("Продолжить") : qsTr("Пропустить")
            icon: "arrowRight"
            primary: NetworkBackend.online
            KeyNavigation.up: panel.listVisible ? panel : null
            KeyNavigation.right: refreshButton
            onClicked: page.next()
        }
        ConsoleButton {
            id: refreshButton
            visible: panel.listVisible
            text: qsTr("Обновить")
            icon: "refresh"
            KeyNavigation.up: panel
            KeyNavigation.left: continueButton
            onClicked: NetworkBackend.scan()
        }
    }
}
