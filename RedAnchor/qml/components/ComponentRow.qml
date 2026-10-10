import QtQuick

// Строка компонента (системные пакеты, драйвер, Vulkan, Steam, Proton, gogdl) для списков
// с моделью ComponentsManager.components: название, состояние, прогресс установки.
// ✕ — установить или обновить, если это нужно.
ConsoleListItem {
    id: root

    required property string key
    required property string name
    required property string description
    required property string status
    required property string version
    required property string latest
    required property real progress
    required property string message
    required property bool recommended

    readonly property bool actionable: status === "missing" || status === "update" || status === "failed"

    icon: {
        switch (key) {
        case "system": return "apps"
        case "drivers": return "chip"
        case "vulkan": return "vulkan"
        case "steam": return "controller"
        case "proton": return "proton"
        default: return "store"
        }
    }
    title: name
    subtitle: {
        switch (status) {
        case "checking": return qsTr("Проверка…")
        case "missing":
            if (message !== "")
                return message
            return (recommended ? qsTr("Не установлено") : qsTr("Не установлено, необязательно")) + " · " + description
        case "installed":
            return qsTr("Установлено") + (version !== "" ? " · " + version : "")
                   + (message !== "" ? " · " + message
                      : key === "steam" ? " · " + qsTr("обновляется сам") : "")
        case "update": return qsTr("Доступно обновление: %1").arg(latest) + (version !== "" ? qsTr(" (сейчас %1)").arg(version) : "")
        case "queued": return qsTr("В очереди")
        case "installing": return message !== "" ? message : qsTr("Установка…")
        case "failed": return message !== "" ? message : qsTr("Не удалось установить")
        default: return description
        }
    }
    implicitHeight: Theme.px(92)

    onClicked: if (actionable) ComponentsManager.install(key)

    Spinner {
        visible: root.status === "checking" || root.status === "queued" || (root.status === "installing" && root.progress < 0)
        size: Theme.px(26)
        anchors.verticalCenter: parent.verticalCenter
    }

    // Прогресс установки
    Row {
        visible: root.status === "installing" && root.progress >= 0
        spacing: Theme.px(14)
        anchors.verticalCenter: parent.verticalCenter

        Rectangle {
            anchors.verticalCenter: parent.verticalCenter
            width: Theme.px(200)
            height: Theme.px(6)
            radius: height / 2
            color: Qt.rgba(1, 1, 1, 0.12)
            Rectangle {
                width: parent.width * Math.max(0, Math.min(1, root.progress))
                height: parent.height
                radius: height / 2
                color: Theme.focus
                Behavior on width { NumberAnimation { duration: Theme.animNormal } }
            }
        }
        Text {
            anchors.verticalCenter: parent.verticalCenter
            width: Theme.px(56)
            horizontalAlignment: Text.AlignRight
            text: Math.round(root.progress * 100) + "%"
            color: Theme.textSecondary
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontSmall
            font.features: { "tnum": 1 }
        }
    }

    Icon {
        visible: root.status === "installed"
        name: "check"
        size: Theme.px(24)
        color: Theme.success
        anchors.verticalCenter: parent.verticalCenter
    }
    Icon {
        visible: root.status === "failed"
        name: "warning"
        size: Theme.px(24)
        lineWidth: 1.5
        color: Theme.warning
        anchors.verticalCenter: parent.verticalCenter
    }

    // [✕] Установить / Обновить — на выбранной строке
    Row {
        visible: root.actionable && root.current
        spacing: Theme.px(10)
        anchors.verticalCenter: parent.verticalCenter
        ButtonGlyph {
            button: "cross"
            size: Theme.px(28)
            anchors.verticalCenter: parent.verticalCenter
        }
        Text {
            anchors.verticalCenter: parent.verticalCenter
            text: root.status === "update" ? qsTr("Обновить") : root.status === "failed" ? qsTr("Повторить") : qsTr("Установить")
            color: Theme.textPrimary
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontSmall
        }
    }
}
