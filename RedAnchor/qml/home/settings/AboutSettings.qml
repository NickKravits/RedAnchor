import QtQuick

// Настройки → О системе.
Item {
    id: root

    readonly property bool focusable: false

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
    }
}
