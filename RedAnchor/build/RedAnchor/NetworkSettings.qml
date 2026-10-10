import QtQuick

// Настройки → Сеть: включение Wi-Fi, список сетей и подключение, состояние кабеля.
FocusScope {
    id: root

    readonly property bool focusable: NetworkBackend.wifiAvailable

    ActionMenu { id: menu }

    ToggleRow {
        id: wifiToggle
        visible: NetworkBackend.wifiAvailable
        focus: true
        width: Math.min(parent.width, Theme.px(1000))
        icon: "globe"
        title: qsTr("Wi-Fi")
        subtitle: !NetworkBackend.wifiEnabled ? qsTr("Выключен")
                : NetworkBackend.activeSsid !== "" ? qsTr("Подключено к «%1»").arg(NetworkBackend.activeSsid)
                : qsTr("Включён")
        checked: NetworkBackend.wifiEnabled
        onToggled: NetworkBackend.wifiEnabled = !NetworkBackend.wifiEnabled
        KeyNavigation.down: panel.listVisible ? panel : null
    }

    NetworkPanel {
        id: panel
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: wifiToggle.visible ? wifiToggle.bottom : parent.top
        anchors.topMargin: wifiToggle.visible ? Theme.px(32) : 0
        anchors.bottom: parent.bottom

        onActiveNetworkChosen: (entry) => {
            if (entry.kind !== "wifi")
                return
            menu.show(entry.ssid, qsTr("Подключено"),
                      [{ text: qsTr("Отключиться"), icon: "power", run: () => NetworkBackend.disconnectWifi() },
                       { text: qsTr("Отмена") }], panel)
        }
    }

    // Из верха списка сетей — к переключателю Wi-Fi
    Keys.onUpPressed: (event) => {
        if (panel.activeFocus && panel.mode === "list" && wifiToggle.visible) {
            wifiToggle.forceActiveFocus()
            event.accepted = true
        } else {
            event.accepted = false
        }
    }
}
