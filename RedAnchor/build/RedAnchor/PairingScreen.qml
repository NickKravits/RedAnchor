import QtQuick

// Шаг 2: перевод DualSense с кабеля на Bluetooth.
SetupPage {
    id: page

    // MAC запоминаем сразу, пока контроллер ещё подключён по USB.
    readonly property string controllerMac: GamepadManager.macAddress
    property string savedMac: ""

    readonly property bool usbAttached: GamepadManager.connected && GamepadManager.connection === "usb"
    readonly property int btState: BluetoothManager.state
    readonly property bool paired: btState === BluetoothManager.Connected
    readonly property bool failed: btState === BluetoothManager.Failed
    property bool cableGone: false

    signal skipped()

    step: 0
    title: paired ? qsTr("Контроллер сопряжён")
         : failed ? qsTr("Не удалось сопрячь контроллер")
         : qsTr("Беспроводное подключение")
    subtitle: paired ? qsTr("DualSense подключён по Bluetooth. Кабель больше не нужен.")
            : failed ? BluetoothManager.errorString
            : qsTr("Переключим контроллер на Bluetooth, чтобы играть без провода.")

    hints: failed ? [{ button: "cross", text: qsTr("Выбрать") }]
         : paired ? []
         : [{ button: "circle", text: qsTr("Оставить кабель") }]

    initialFocus: page

    Component.onCompleted: {
        savedMac = controllerMac
        BluetoothManager.startControllerPairing(savedMac)
    }
    Component.onDestruction: {
        if (!paired)
            BluetoothManager.cancel()
    }

    onUsbAttachedChanged: {
        if (!usbAttached)
            hideCableTimer.start()
        else
            cableGone = false
    }

    onPairedChanged: {
        if (paired)
            finishTimer.start()
    }

    onFailedChanged: {
        if (failed)
            retryButton.forceActiveFocus()
    }

    // Контроллер может подключиться сам (входящее соединение) — тоже считаем успехом.
    Connections {
        target: GamepadManager
        function onActiveChanged() {
            if (GamepadManager.hasBluetoothDualSense(page.savedMac))
                BluetoothManager.markConnected()
        }
        function onButtonPressed(button) { controller.flash(button) }
    }

    Timer {
        id: hideCableTimer
        interval: 950
        onTriggered: page.cableGone = true
    }

    Timer {
        id: finishTimer
        interval: 1800
        onTriggered: page.next()
    }

    Keys.onEscapePressed: {
        BluetoothManager.cancel()
        page.skipped()
    }

    Column {
        anchors.left: parent.left
        anchors.verticalCenter: parent.verticalCenter
        spacing: Theme.px(34)

        InstructionStep {
            number: 1
            text: qsTr("Отсоедините USB-кабель")
            status: page.usbAttached && !page.paired ? "active" : "done"
        }
        InstructionStep {
            number: 2
            text: qsTr("Зажмите кнопки PS и Create")
            detail: qsTr("Держите их около 3 секунд, пока световая панель не начнёт часто мигать.")
            status: page.paired || page.btState === BluetoothManager.Pairing || page.btState === BluetoothManager.Connecting
                    ? "done" : page.usbAttached ? "pending" : "active"
        }
        InstructionStep {
            number: 3
            text: qsTr("Подключение по Bluetooth")
            detail: page.btState === BluetoothManager.Pairing ? qsTr("Сопряжение с %1…").arg(BluetoothManager.deviceName || "DualSense")
                  : page.btState === BluetoothManager.Connecting ? qsTr("Подключение…")
                  : page.paired ? (BluetoothManager.deviceAddress !== "" ? BluetoothManager.deviceAddress : "")
                  : page.failed ? ""
                  : qsTr("Поиск контроллера…")
            status: page.paired ? "done"
                  : (page.btState === BluetoothManager.Pairing || page.btState === BluetoothManager.Connecting) ? "active"
                  : "pending"
        }

        Row {
            spacing: Theme.px(16)
            visible: !page.paired && !page.failed
            leftPadding: Theme.px(68)
            Spinner {
                size: Theme.px(30)
                anchors.verticalCenter: parent.verticalCenter
            }
            Text {
                anchors.verticalCenter: parent.verticalCenter
                text: BluetoothManager.demo ? qsTr("Bluetooth в демо-режиме") : qsTr("Bluetooth включён, идёт поиск")
                color: Theme.textMuted
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontSmall
            }
        }

        Row {
            visible: page.failed
            spacing: Theme.px(20)
            leftPadding: Theme.px(68)

            ConsoleButton {
                id: retryButton
                text: qsTr("Повторить")
                icon: "refresh"
                primary: true
                KeyNavigation.right: skipButton
                onClicked: BluetoothManager.startControllerPairing(page.savedMac)
            }
            ConsoleButton {
                id: skipButton
                text: qsTr("Пропустить")
                KeyNavigation.left: retryButton
                onClicked: page.skipped()
            }
        }
    }

    DualSense {
        id: controller
        anchors.right: parent.right
        anchors.verticalCenter: parent.verticalCenter
        anchors.verticalCenterOffset: Theme.px(40)
        width: Math.min(parent.width * 0.5, parent.height * 1.25)
        height: width * 2 / 3

        cableState: page.usbAttached ? "plugged" : page.cableGone ? "hidden" : "unplugged"
        lightbarMode: page.paired ? "solid" : page.usbAttached ? "breathe" : page.failed ? "off" : "blink"
        playerLeds: page.paired ? 1 : 0
        highlighted: !page.usbAttached && !page.paired && !page.failed
                     && page.btState !== BluetoothManager.Pairing && page.btState !== BluetoothManager.Connecting
                     ? ["ps", "create"] : []
    }
}
