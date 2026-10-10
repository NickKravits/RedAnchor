pragma ComponentBehavior: Bound
import QtQuick

// Отдельная сцена для нового DualSense, подключённого кабелем после настройки консоли:
//   сопряжение по кабелю -> «отключите кабель и нажмите PS» -> подключён по Bluetooth -> ✕.
// Сопряжение запускает Main.qml (BluetoothManager.pairController) перед показом сцены.
// ○ закрывает сцену в любой момент — сопряжение при этом продолжается в фоне.
SetupPage {
    id: page

    // MAC контроллера, который сопрягаем
    property string mac

    // "pairing" | "unplug" | "wireless" | "failed"
    readonly property string phase: wireless ? "wireless"
                                  : failed ? "failed"
                                  : BluetoothManager.controllerPaired ? "unplug"
                                  : "pairing"
    property bool usbPresent: true
    property bool wireless: false
    property bool timedOut: false
    property bool cableLost: false
    property bool slow: false
    readonly property bool failed: BluetoothManager.pairingState === "failed" || timedOut || cableLost

    signal finished()

    hints: phase === "wireless" || phase === "failed" || (phase === "unplug" && slow)
           ? [{ button: "circle", text: qsTr("Закрыть") }, { button: "cross", text: qsTr("Готово") }]
           : [{ button: "circle", text: qsTr("Закрыть") }]

    function refresh() {
        const target = mac.toUpperCase()
        const pads = GamepadManager.gamepads
        usbPresent = pads.some(p => String(p.mac).toUpperCase() === target && p.connection === "usb")
        wireless = pads.some(p => String(p.mac).toUpperCase() === target && p.connection === "bluetooth")
        // Кабель вынули до конца сопряжения
        if (!usbPresent && !wireless && phase === "pairing")
            cableLost = true
    }

    function close() {
        page.finished()
    }

    Component.onCompleted: {
        refresh()
        pairingTimeout.start()
    }

    onPhaseChanged: {
        if (phase === "unplug") {
            pairingTimeout.stop()
            slow = false
            slowTimer.restart()
        } else {
            slowTimer.stop()
        }
    }

    Connections {
        target: GamepadManager
        function onGamepadsChanged() { page.refresh() }
        function onButtonPressed(button) { controller.flash(button) }
    }

    Timer {
        id: pairingTimeout
        interval: 20000  // помощник перезапускает bluetoothd, это занимает несколько секунд
        onTriggered: if (page.phase === "pairing") page.timedOut = true
    }
    Timer {
        id: slowTimer
        interval: 25000
        onTriggered: page.slow = true
    }

    Keys.onPressed: (event) => {
        const enter = event.key === Qt.Key_Return || event.key === Qt.Key_Enter
        if (event.key === Qt.Key_Escape
                || (enter && (phase === "wireless" || phase === "failed" || (phase === "unplug" && slow)))) {
            page.close()
            event.accepted = true
        }
    }

    Item {
        parent: page
        anchors.fill: parent

        NumberAnimation on opacity { from: 0; to: 1; duration: Theme.animSlow; easing.type: Easing.OutCubic }

        // ---------- Контроллер ----------
        DualSense {
            id: controller
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.top: parent.top
            anchors.topMargin: Theme.px(150)
            width: Math.min(parent.width * 0.42, parent.height * 0.62 * implicitWidth / implicitHeight)
            height: width * implicitHeight / implicitWidth
            showDimensions: false

            cableState: !page.usbPresent ? "hidden"
                      : page.phase === "unplug" ? "unplugHint"
                      : "plugged"
            lightbarMode: page.phase === "wireless" || page.phase === "failed" ? "solid"
                        : page.phase === "pairing" ? "breathe"
                        : page.usbPresent ? "off" : "blink"
            playerLeds: page.phase === "wireless" ? 1 : 0
            highlighted: page.phase === "wireless" ? ["cross"]
                       : page.phase === "unplug" && !page.usbPresent ? ["ps"] : []
            callouts: page.phase === "unplug" ? (page.usbPresent ? ["usb"] : ["ps"]) : []
        }

        // ---------- Текст ----------
        Column {
            id: texts
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.top: controller.bottom
            anchors.topMargin: Theme.px(24)
            width: Math.min(parent.width - 2 * Theme.gutter, Theme.px(1100))
            spacing: Theme.px(14)

            Text {
                width: parent.width
                horizontalAlignment: Text.AlignHCenter
                text: {
                    switch (page.phase) {
                    case "pairing": return qsTr("Новый контроллер")
                    case "unplug": return qsTr("Сопряжение выполнено")
                    case "wireless": return qsTr("Контроллер подключён")
                    default: return qsTr("Не удалось настроить Bluetooth")
                    }
                }
                color: Theme.textPrimary
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontTitle
                font.weight: Font.Light
            }
            GlyphText {
                width: parent.width
                horizontalAlignment: Text.AlignHCenter
                text: {
                    switch (page.phase) {
                    case "pairing":
                        return qsTr("Настраиваем беспроводное подключение. Не отсоединяйте кабель.")
                    case "unplug":
                        if (page.slow)
                            return page.usbPresent
                                    ? qsTr("Не подключается? Нажмите {cross} — контроллер продолжит работать по кабелю.")
                                    : qsTr("Не подключается? Подключите кабель снова — контроллер будет работать по кабелю.")
                        return page.usbPresent ? qsTr("Отключите кабель и нажмите кнопку PS — контроллер подключится по Bluetooth.")
                                               : qsTr("Нажмите кнопку PS на контроллере.")
                    case "wireless":
                        return qsTr("Контроллер работает по Bluetooth. Нажмите {cross}, чтобы продолжить.")
                    default:
                        return page.cableLost ? qsTr("Кабель отсоединён до завершения сопряжения. Подключите контроллер снова.")
                                              : qsTr("Контроллер продолжит работать по кабелю. Нажмите {cross}, чтобы продолжить.")
                    }
                }
                color: Theme.textSecondary
                pixelSize: Theme.fontBody
            }
        }

        // ---------- Шаги ----------
        Row {
            id: stepper
            visible: page.phase !== "failed"
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.top: texts.bottom
            anchors.topMargin: Theme.px(40)
            spacing: Theme.px(18)

            readonly property int stage: ["pairing", "unplug", "wireless"].indexOf(page.phase)

            Repeater {
                model: [qsTr("Сопряжение"), qsTr("Отключите кабель"), qsTr("Bluetooth")]
                delegate: Row {
                    id: stepItem
                    required property string modelData
                    required property int index
                    readonly property bool done: index < stepper.stage || (index === 2 && stepper.stage === 2)
                    readonly property bool active: index === stepper.stage && !done
                    spacing: Theme.px(18)

                    // Соединительная линия перед шагом
                    Rectangle {
                        visible: stepItem.index > 0
                        anchors.verticalCenter: parent.verticalCenter
                        width: Theme.px(56)
                        height: Theme.px(2)
                        radius: height / 2
                        color: stepItem.done || stepItem.active ? Theme.textPrimary : Theme.outlineStrong
                        Behavior on color { ColorAnimation { duration: Theme.animNormal } }
                    }

                    Row {
                        anchors.verticalCenter: parent.verticalCenter
                        spacing: Theme.px(12)

                        Item {
                            width: Theme.px(30)
                            height: width
                            anchors.verticalCenter: parent.verticalCenter

                            Rectangle {
                                anchors.fill: parent
                                radius: width / 2
                                color: stepItem.done ? Theme.focus : "transparent"
                                border.width: stepItem.done ? 0 : Theme.px(2)
                                border.color: stepItem.active ? Theme.textPrimary : Theme.outlineStrong
                            }
                            Icon {
                                visible: stepItem.done
                                anchors.centerIn: parent
                                name: "check"
                                size: Theme.px(20)
                                lineWidth: 2
                                color: Theme.background
                            }
                            Spinner {
                                visible: stepItem.active
                                anchors.centerIn: parent
                                size: Theme.px(18)
                            }
                        }
                        Text {
                            anchors.verticalCenter: parent.verticalCenter
                            text: stepItem.modelData
                            color: stepItem.done || stepItem.active ? Theme.textPrimary : Theme.textMuted
                            font.family: Theme.fontFamily
                            font.pixelSize: Theme.fontSmall
                            font.weight: stepItem.active ? Font.Medium : Font.Normal
                        }
                    }
                }
            }
        }
    }
}
