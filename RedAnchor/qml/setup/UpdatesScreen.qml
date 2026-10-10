pragma ComponentBehavior: Bound
import QtQuick
import "ModelSync.js" as ModelSync

// Шаг мастера «Проверка обновлений»: проверяем и устанавливаем компоненты для игр —
// графический драйвер, Vulkan и Proton. При подключении к интернету установка
// начинается сама; продолжить можно в любой момент — установка доделается в фоне.
SetupPage {
    id: page

    property bool autoStarted: false
    readonly property bool hasFailed: ComponentsManager.components.some(c => c.status === "failed")

    step: 3
    title: qsTr("Проверка обновлений")
    subtitle: {
        if (ComponentsManager.checking)
            return qsTr("Проверяем, какие компоненты нужны для игр…")
        if (ComponentsManager.busy)
            return qsTr("Устанавливаем компоненты для игр. Это может занять несколько минут.")
        if (!ComponentsManager.needsInstall)
            return qsTr("Всё необходимое для игр установлено.")
        if (!NetworkBackend.online)
            return qsTr("Нет подключения к интернету. Компоненты можно установить позже: Настройки → Обновления.")
        return qsTr("Не всё удалось установить. Можно повторить или продолжить — установить позже можно в настройках.")
    }
    hints: [{ button: "cross", text: qsTr("Выбрать") }]
    initialFocus: continueButton

    function maybeAutoInstall() {
        if (autoStarted || ComponentsManager.checking || !NetworkBackend.online || !ComponentsManager.needsInstall)
            return
        autoStarted = true
        ComponentsManager.installAll()
    }

    Component.onCompleted: {
        ModelSync.sync(items, ComponentsManager.components, "key")
        if (!ComponentsManager.busy)
            ComponentsManager.check()
    }

    Connections {
        target: ComponentsManager
        function onComponentsChanged() {
            ModelSync.sync(items, ComponentsManager.components, "key")
            page.maybeAutoInstall()
        }
    }

    ListModel { id: items }

    ListView {
        id: list
        anchors.top: parent.top
        anchors.bottom: notes.top
        anchors.bottomMargin: Theme.px(20)
        width: Math.min(parent.width, Theme.px(1100))
        spacing: Theme.px(12)
        interactive: false
        keyNavigationEnabled: true
        highlightMoveDuration: Theme.animFast
        model: items

        KeyNavigation.down: continueButton
        KeyNavigation.priority: KeyNavigation.AfterItem

        delegate: ComponentRow {
            required property int index
            width: ListView.view.width
            current: ListView.isCurrentItem && list.activeFocus
        }
    }

    Column {
        id: notes
        anchors.bottom: buttons.top
        anchors.bottomMargin: Theme.px(28)
        width: list.width
        spacing: Theme.px(8)

        Text {
            visible: ComponentsManager.rebootRequired
            width: parent.width
            wrapMode: Text.WordWrap
            text: qsTr("Драйвер NVIDIA начнёт работать после перезагрузки консоли.")
            color: Theme.warning
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontSmall
        }
    }

    Row {
        id: buttons
        anchors.bottom: parent.bottom
        spacing: Theme.px(20)

        ConsoleButton {
            id: continueButton
            text: ComponentsManager.busy ? qsTr("Продолжить в фоне") : qsTr("Продолжить")
            icon: "arrowRight"
            primary: !ComponentsManager.busy
            KeyNavigation.up: list
            KeyNavigation.right: retryButton.visible ? retryButton : null
            onClicked: page.next()
        }
        ConsoleButton {
            id: retryButton
            visible: page.hasFailed && !ComponentsManager.busy && NetworkBackend.online
            text: qsTr("Повторить")
            icon: "refresh"
            KeyNavigation.up: list
            KeyNavigation.left: continueButton
            onClicked: ComponentsManager.installAll()
        }
    }
}
