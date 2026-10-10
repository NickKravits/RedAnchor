import QtQuick

// Шаг 1: подключение DualSense кабелем и подтверждение кнопкой PS.
SetupPage {
    id: page

    // "waiting" — геймпада нет, "plugged" — найден, ждём PS, "ready" — подтверждён
    readonly property string phase: confirmed ? "ready" : GamepadManager.connected ? "plugged" : "waiting"
    property bool confirmed: false

    step: 0
    title: phase === "ready" ? qsTr("Контроллер подключён")
         : phase === "plugged" ? qsTr("Нажмите кнопку PS")
         : qsTr("Подключите контроллер")
    subtitle: phase === "ready" ? qsTr("Отлично! Продолжаем настройку.")
            : phase === "plugged"
              ? (GamepadManager.dualSense ? qsTr("DualSense обнаружен. Нажмите кнопку PS в центре контроллера.")
                                          : qsTr("Обнаружен «%1». Нажмите центральную кнопку или ✕.").arg(GamepadManager.controllerName))
              : qsTr("Подключите беспроводной контроллер DualSense к консоли с помощью USB-кабеля.")

    hints: phase === "waiting" ? [{ button: "cross", text: qsTr("Продолжить с клавиатуры") }]
         : phase === "plugged" ? [{ button: "ps", text: qsTr("Подтвердить") }]
         : []

    function confirm() {
        if (phase !== "plugged")
            return
        confirmed = true
        finishTimer.start()
    }

    // Если геймпад отключили до подтверждения — возвращаемся к ожиданию.
    Connections {
        target: GamepadManager
        function onButtonPressed(button) { controller.flash(button) }
        function onActiveChanged() {
            if (!GamepadManager.connected && page.confirmed && finishTimer.running) {
                finishTimer.stop()
                page.confirmed = false
            }
        }
    }

    Timer {
        id: finishTimer
        interval: 1500
        onTriggered: page.next()
    }

    Keys.onPressed: (event) => {
        if (event.key === Qt.Key_HomePage
                || ((event.key === Qt.Key_Return || event.key === Qt.Key_Enter) && page.phase === "plugged"
                    && !GamepadManager.dualSense)) {
            page.confirm()
            event.accepted = true
        } else if ((event.key === Qt.Key_Return || event.key === Qt.Key_Enter) && page.phase === "waiting") {
            page.next()
            event.accepted = true
        }
    }

    Column {
        anchors.left: parent.left
        anchors.verticalCenter: parent.verticalCenter
        spacing: Theme.px(36)

        InstructionStep {
            number: 1
            text: qsTr("Подключите USB-кабель")
            detail: page.phase === "waiting" ? qsTr("Разъём USB-C находится на верхней грани контроллера.")
                                             : GamepadManager.connection === "bluetooth" ? qsTr("Контроллер уже подключён по Bluetooth.")
                                                                                         : ""
            status: page.phase === "waiting" ? "active" : "done"
        }
        InstructionStep {
            number: 2
            text: qsTr("Нажмите кнопку PS")
            status: page.phase === "ready" ? "done" : page.phase === "plugged" ? "active" : "pending"
        }
    }

    DualSense {
        id: controller
        anchors.right: parent.right
        anchors.verticalCenter: parent.verticalCenter
        anchors.verticalCenterOffset: Theme.px(40)
        width: Math.min(parent.width * 0.5, parent.height * 1.25)
        height: width * 2 / 3

        cableState: page.phase === "waiting" ? "hint"
                  : GamepadManager.connection === "usb" ? "plugged" : "hidden"
        lightbarMode: page.phase === "ready" ? "solid" : page.phase === "plugged" ? "breathe" : "off"
        playerLeds: page.phase === "ready" ? 1 : 0
        highlighted: page.phase === "plugged" ? ["ps"] : []
    }
}
