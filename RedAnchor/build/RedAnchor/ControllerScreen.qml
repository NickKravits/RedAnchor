import QtQuick

// Подключение контроллера:
//   кабель -> кнопка PS -> сопряжение по Bluetooth (само, через кабель)
//   -> «отключите кабель и нажмите PS» -> контроллер подключился по Bluetooth -> ✕.
// Сопряжение запускает Main.qml, как только DualSense подключён по USB
// (см. BluetoothManager). Экран подтверждения с ✕ показывается, только когда
// контроллер действительно подключился по Bluetooth. Если Bluetooth недоступен
// или сопряжение не удалось, контроллер остаётся на кабеле.
SetupPage {
    id: page

    //  "waiting"  — геймпада нет
    //  "plugged"  — найден, ждём PS
    //  "pairing"  — идёт сопряжение по кабелю
    //  "unplug"   — сопряжено: отключите кабель и нажмите PS
    //  "wireless" — контроллер подключён по Bluetooth, ждём ✕
    //  "cable"    — работаем по кабелю (нет Bluetooth или сопряжение не удалось), ждём ✕
    readonly property string phase: {
        if (!confirmed)
            return GamepadManager.connected ? "plugged" : "waiting"
        if (wirelessDualSense)
            return "wireless"
        if (wirelessFlow) {
            if (BluetoothManager.controllerPaired)
                return "unplug"
            if (!pairingFailed)
                return "pairing"
        }
        return "cable"
    }
    property bool confirmed: false
    // Подтверждённый контроллер — DualSense на кабеле и Bluetooth есть: идём к беспроводному подключению
    property bool wirelessFlow: false
    property bool pairingTimedOut: false
    // Контроллер долго не подключается по Bluetooth — подсказываем, как продолжить по кабелю
    property bool unplugSlow: false

    readonly property bool overUsb: GamepadManager.connection === "usb"
    readonly property bool wirelessDualSense: GamepadManager.dualSense && GamepadManager.connection === "bluetooth"
    readonly property bool pairingFailed: BluetoothManager.pairingState === "failed" || pairingTimedOut

    step: 0
    title: {
        switch (phase) {
        case "plugged": return qsTr("Нажмите кнопку PS")
        case "pairing": return qsTr("Сопряжение…")
        case "unplug": return qsTr("Сопряжение выполнено")
        case "wireless": return qsTr("Контроллер подключён")
        case "cable": return qsTr("Контроллер подключён")
        default: return qsTr("Подключите контроллер")
        }
    }
    subtitle: {
        switch (phase) {
        case "plugged":
            return GamepadManager.dualSense ? qsTr("Контроллер обнаружен. Нажмите кнопку PS в центре, чтобы подтвердить.")
                                            : qsTr("Обнаружен «%1». Нажмите центральную кнопку или {cross}.").arg(GamepadManager.controllerName)
        case "pairing":
            return qsTr("Настраиваем беспроводное подключение. Не отсоединяйте кабель.")
        case "unplug":
            return overUsb ? qsTr("Отключите кабель и нажмите кнопку PS — контроллер подключится по Bluetooth.")
                           : qsTr("Нажмите кнопку PS на контроллере.")
        case "wireless":
            return qsTr("Контроллер работает по Bluetooth. Нажмите {cross}, чтобы продолжить.")
        case "cable":
            if (!BluetoothManager.adapterAvailable || !wirelessFlow)
                return qsTr("Контроллер работает по кабелю. Нажмите {cross}, чтобы продолжить.")
            return qsTr("Не удалось настроить Bluetooth. Контроллер продолжит работать по кабелю — нажмите {cross}.")
        default:
            return qsTr("Подключите DualSense к консоли кабелем USB-C — дальше всё произойдёт автоматически.")
        }
    }

    hints: {
        switch (phase) {
        case "waiting": return [{ button: "cross", text: qsTr("Продолжить с клавиатуры") }]
        case "plugged": return [{ button: "ps", text: qsTr("Подтвердить") }]
        case "pairing": return []
        case "unplug": return unplugSlow && overUsb ? [{ button: "cross", text: qsTr("Продолжить по кабелю") }] : []
        default: return [{ button: "cross", text: qsTr("Продолжить") }]
        }
    }

    function confirm() {
        if (phase !== "plugged")
            return
        wirelessFlow = GamepadManager.dualSense && overUsb && BluetoothManager.adapterAvailable
        confirmed = true
        if (wirelessFlow)
            pairingTimeout.start()
    }

    onPhaseChanged: {
        if (phase === "unplug") {
            pairingTimeout.stop()
            unplugSlow = false
            slowTimer.restart()
        } else {
            slowTimer.stop()
        }
    }

    Connections {
        target: GamepadManager
        function onButtonPressed(button) { controller.flash(button) }
        function onActiveChanged() {
            // Отключили кабель до сопряжения — возвращаемся к ожиданию.
            if (!GamepadManager.connected && page.confirmed && page.phase !== "unplug") {
                pairingTimeout.stop()
                page.confirmed = false
                page.wirelessFlow = false
                page.pairingTimedOut = false
            }
        }
    }

    Timer {
        id: pairingTimeout
        interval: 20000  // помощник перезапускает bluetoothd, это занимает несколько секунд
        onTriggered: page.pairingTimedOut = true
    }

    Timer {
        id: slowTimer
        interval: 25000
        onTriggered: page.unplugSlow = true
    }

    Keys.onPressed: (event) => {
        const enter = event.key === Qt.Key_Return || event.key === Qt.Key_Enter
        if (page.phase === "plugged" && (event.key === Qt.Key_HomePage || (enter && !GamepadManager.dualSense))) {
            page.confirm()
        } else if (enter && (page.phase === "waiting" || page.phase === "wireless" || page.phase === "cable"
                             || (page.phase === "unplug" && page.unplugSlow && page.overUsb))) {
            page.next()
        } else {
            return
        }
        event.accepted = true
    }

    Column {
        anchors.left: parent.left
        anchors.verticalCenter: parent.verticalCenter
        spacing: Theme.px(34)

        InstructionStep {
            number: 1
            text: qsTr("Подключите кабель USB-C")
            detail: page.phase === "waiting" ? qsTr("Разъём находится на верхней грани контроллера.") : ""
            status: page.phase === "waiting" ? "active" : "done"
        }
        InstructionStep {
            number: 2
            text: qsTr("Нажмите кнопку PS")
            status: page.phase === "plugged" ? "active" : page.confirmed ? "done" : "pending"
        }
        InstructionStep {
            number: 3
            visible: BluetoothManager.adapterAvailable
            text: qsTr("Сопряжение по Bluetooth")
            detail: page.phase === "pairing" ? qsTr("Не отсоединяйте кабель…")
                  : page.phase === "unplug" || page.phase === "wireless" ? qsTr("Выполнено")
                  : page.phase === "cable" ? qsTr("Не удалось, используется кабель") : ""
            status: page.phase === "pairing" ? "active"
                  : page.phase === "unplug" || page.phase === "wireless" || page.phase === "cable" ? "done" : "pending"
        }
        InstructionStep {
            number: 4
            visible: BluetoothManager.adapterAvailable && page.phase !== "cable"
            text: qsTr("Отключите кабель и нажмите PS")
            detail: page.phase === "unplug"
                    ? (page.unplugSlow
                       ? (page.overUsb ? qsTr("Не подключается? Нажмите {cross}, чтобы пока продолжить по кабелю.")
                                       : qsTr("Не подключается? Подключите кабель снова — можно продолжить по кабелю."))
                       : page.overUsb ? qsTr("Кабель больше не нужен.") : qsTr("Ожидаем подключения по Bluetooth…"))
                    : page.phase === "wireless" ? qsTr("Подключено по Bluetooth") : ""
            status: page.phase === "unplug" ? "active" : page.phase === "wireless" ? "done" : "pending"
        }

        // Нет Bluetooth-модуля — только кабель
        Row {
            visible: !BluetoothManager.adapterAvailable
            spacing: Theme.px(16)
            Icon {
                name: "warning"
                size: Theme.px(24)
                lineWidth: 1.5
                color: Theme.warning
                anchors.verticalCenter: parent.verticalCenter
            }
            Text {
                anchors.verticalCenter: parent.verticalCenter
                width: Theme.px(560)
                wrapMode: Text.WordWrap
                text: qsTr("Bluetooth-модуль не найден. Подключение контроллера возможно только по кабелю.")
                color: Theme.textSecondary
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontSmall
            }
        }
    }

    DualSense {
        id: controller
        anchors.right: parent.right
        anchors.verticalCenter: parent.verticalCenter
        anchors.verticalCenterOffset: Theme.px(40)
        width: Math.min(parent.width * 0.52, parent.height * 1.3)
        height: width * implicitHeight / implicitWidth

        cableState: page.phase === "waiting" ? "hint"
                  : !page.overUsb ? "hidden"
                  : page.phase === "unplug" ? "unplugHint"
                  : "plugged"
        lightbarMode: page.phase === "wireless" || page.phase === "cable" ? "solid"
                    : page.phase === "plugged" || page.phase === "pairing" ? "breathe"
                    : page.phase === "unplug" && !page.overUsb ? "blink"
                    : "off"
        playerLeds: page.phase === "wireless" || page.phase === "cable" ? 1 : 0
        highlighted: page.phase === "plugged" || (page.phase === "unplug" && !page.overUsb) ? ["ps"]
                   : page.phase === "wireless" || page.phase === "cable" ? ["cross"] : []
        callouts: page.phase === "waiting" ? ["usb"]
                : page.phase === "plugged" || (page.phase === "unplug" && !page.overUsb) ? ["ps"]
                : page.phase === "unplug" ? ["usb"] : []
    }
}
