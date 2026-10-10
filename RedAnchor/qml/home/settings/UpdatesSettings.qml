pragma ComponentBehavior: Bound
import QtQuick
import "ModelSync.js" as ModelSync

// Настройки → Обновления: состояние и установка компонентов для игр —
// системные пакеты сессии, графический драйвер, Vulkan, клиент Steam, Proton и gogdl.
FocusScope {
    id: root

    readonly property bool focusable: true

    Component.onCompleted: {
        ModelSync.sync(items, ComponentsManager.components, "key")
        if (!ComponentsManager.busy)
            ComponentsManager.check()
    }

    Connections {
        target: ComponentsManager
        function onComponentsChanged() { ModelSync.sync(items, ComponentsManager.components, "key") }
    }

    ListModel { id: items }

    Column {
        id: head
        width: Math.min(parent.width, Theme.px(1000))
        spacing: Theme.px(20)

        Row {
            spacing: Theme.px(20)

            ConsoleButton {
                id: checkButton
                focus: true
                text: ComponentsManager.checking ? qsTr("Проверка…") : qsTr("Проверить обновления")
                icon: "refresh"
                enabledState: !ComponentsManager.checking && !ComponentsManager.busy
                KeyNavigation.right: installButton.visible ? installButton : null
                KeyNavigation.down: list
                onClicked: ComponentsManager.check()
            }
            ConsoleButton {
                id: installButton
                visible: ComponentsManager.needsInstall && !ComponentsManager.busy && !ComponentsManager.checking
                text: qsTr("Установить всё")
                icon: "download"
                primary: true
                KeyNavigation.left: checkButton
                KeyNavigation.down: list
                onClicked: ComponentsManager.installAll()
            }
        }

        Text {
            visible: ComponentsManager.rebootRequired
            width: parent.width
            wrapMode: Text.WordWrap
            text: qsTr("Драйвер NVIDIA начнёт работать после перезагрузки консоли.")
            color: Theme.warning
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontSmall
        }
        Text {
            visible: ComponentsManager.steamInstalled
            width: parent.width
            wrapMode: Text.WordWrap
            text: qsTr("Steam обновляется сам. Вход — один раз в Big Picture; игры Steam запускаются через Proton Experimental.")
            color: Theme.textSecondary
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontSmall
        }
        Text {
            visible: !NetworkBackend.online
            width: parent.width
            wrapMode: Text.WordWrap
            text: qsTr("Нет подключения к интернету — установка недоступна.")
            color: Theme.textSecondary
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontSmall
        }
    }

    ListView {
        id: list
        anchors.top: head.bottom
        anchors.topMargin: Theme.px(28)
        anchors.bottom: parent.bottom
        width: head.width
        clip: true
        spacing: Theme.px(12)
        keyNavigationEnabled: true
        highlightMoveDuration: Theme.animFast
        model: items

        KeyNavigation.up: checkButton
        KeyNavigation.priority: KeyNavigation.AfterItem

        delegate: ComponentRow {
            required property int index
            width: ListView.view.width
            current: ListView.isCurrentItem && list.activeFocus
        }
    }
}
