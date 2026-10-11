import QtQuick

// Настройки → О системе. Внизу — сброс к заводским настройкам (кроме обновлений).
FocusScope {
    id: root

    readonly property bool focusable: true

    // Два подтверждения: сброс удаляет пользователей и их игры, отменить его нельзя
    function confirmReset() {
        menu.show(qsTr("Сбросить настройки?"),
                  qsTr("Будут удалены все пользователи консоли с их играми и входами в магазины, сопряжённые устройства Bluetooth и сохранённые сети Wi-Fi. Обновления (Proton, загрузчики игр, системные компоненты) останутся."), [
            { text: qsTr("Продолжить"), icon: "warning", run: () => Qt.callLater(root.confirmResetAgain) },
            { text: qsTr("Отмена") }
        ], resetButton)
    }
    function confirmResetAgain() {
        menu.show(qsTr("Точно сбросить?"),
                  qsTr("Это нельзя отменить. Оболочка перезапустится и предложит настроить консоль заново."), [
            { text: qsTr("Сбросить"), icon: "trash", run: () => SystemControl.factoryReset() },
            { text: qsTr("Отмена") }
        ], resetButton)
    }

    ActionMenu { id: menu }

    Column {
        width: Math.min(parent.width, Theme.px(1100))
        spacing: Theme.px(18)

        InfoRow { width: parent.width; label: qsTr("Система"); value: SystemInfo.osName }
        InfoRow { width: parent.width; label: qsTr("Оболочка"); value: qsTr("RedAnchor Shell %1").arg(SystemInfo.shellVersion) }
        InfoRow { width: parent.width; label: qsTr("Имя консоли"); value: SystemInfo.hostName }
        InfoRow {
            width: parent.width
            label: qsTr("Процессор")
            value: SystemInfo.cpu + (SystemInfo.cpuThreads > 0 ? qsTr(" · потоков: %1").arg(SystemInfo.cpuThreads) : "")
        }
        InfoRow { width: parent.width; label: qsTr("Оперативная память"); value: SystemInfo.memory }
        InfoRow { width: parent.width; label: qsTr("Ядро"); value: SystemInfo.kernel + " (" + SystemInfo.architecture + ")" }
        InfoRow { width: parent.width; label: qsTr("Qt"); value: SystemInfo.qtVersion }
        InfoRow { width: parent.width; label: qsTr("Пользователь"); value: AccountManager.nickname }

        Item { width: 1; height: Theme.px(18) }

        ConsoleButton {
            id: resetButton
            focus: true
            text: qsTr("Сбросить настройки")
            icon: "refresh"
            onClicked: root.confirmReset()
        }
        Text {
            width: parent.width
            wrapMode: Text.WordWrap
            text: qsTr("Вернуть консоль к заводским настройкам: пользователи, игры, Bluetooth и Wi-Fi удаляются, обновления остаются.")
            color: Theme.textSecondary
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontSmall
        }
    }
}
