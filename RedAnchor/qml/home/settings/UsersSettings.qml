pragma ComponentBehavior: Bound
import QtQuick

// Настройки → Пользователи: список учётных записей, добавление локального
// пользователя, удаление и смена пользователя.
FocusScope {
    id: root

    readonly property bool focusable: true
    property string mode: "list"            // "list" | "add"

    signal switchUserRequested()

    function describe(a) {
        return a.local ? qsTr("Локальная учётная запись") : qsTr("Аккаунт RedAnchor · %1").arg(a.email)
    }

    function activate(a) {
        if (a.id === AccountManager.id) {
            menu.show(a.nickname, qsTr("Сейчас играет"), [
                { text: qsTr("Сменить пользователя"), icon: "swap", run: () => root.switchUserRequested() },
                { text: qsTr("Отмена") }
            ], userList)
            return
        }
        menu.show(a.nickname, describe(a), [
            { text: qsTr("Удалить пользователя"), icon: "trash", run: () => root.confirmRemove(a) },
            { text: qsTr("Отмена") }
        ], userList)
    }

    function confirmRemove(a) {
        menu.show(qsTr("Удалить «%1»?").arg(a.nickname),
                  qsTr("Учётная запись будет удалена с консоли вместе со всеми её играми, сохранениями и входами в магазины."), [
            { text: qsTr("Удалить"), icon: "trash", run: () => AccountManager.removeAccount(a.id) },
            { text: qsTr("Отмена") }
        ], userList)
    }

    function startAdd() {
        mode = "add"
        nicknameField.text = ""
        nicknameField.errorText = ""
        nicknameField.forceActiveFocus()
        Keyboard.open(nicknameField)
    }

    function finishAdd() {
        nicknameField.errorText = AccountManager.validateNickname(nicknameField.text)
        if (nicknameField.errorText !== "") {
            nicknameField.forceActiveFocus()
            return
        }
        if (AccountManager.createLocal(nicknameField.text, false))
            cancelAdd()
    }

    function cancelAdd() {
        mode = "list"
        addButton.forceActiveFocus()
    }

    Keys.onEscapePressed: (event) => {
        if (mode === "add") {
            cancelAdd()
            event.accepted = true
        } else {
            event.accepted = false
        }
    }

    ActionMenu { id: menu }

    // ---------- Список ----------
    Row {
        id: buttons
        visible: root.mode === "list"
        spacing: Theme.px(20)

        ConsoleButton {
            id: addButton
            focus: true
            text: qsTr("Добавить пользователя")
            icon: "plus"
            KeyNavigation.right: switchButton
            KeyNavigation.down: userList
            onClicked: root.startAdd()
        }
        ConsoleButton {
            id: switchButton
            text: qsTr("Сменить пользователя")
            icon: "swap"
            KeyNavigation.left: addButton
            KeyNavigation.down: userList
            onClicked: root.switchUserRequested()
        }
    }

    ListView {
        id: userList
        visible: root.mode === "list"
        anchors.top: buttons.bottom
        anchors.topMargin: Theme.px(28)
        anchors.bottom: parent.bottom
        width: Math.min(parent.width, Theme.px(1000))
        clip: true
        spacing: Theme.px(12)
        keyNavigationEnabled: true
        highlightMoveDuration: Theme.animFast
        model: AccountManager.accounts

        KeyNavigation.up: addButton
        KeyNavigation.priority: KeyNavigation.AfterItem

        Keys.onReturnPressed: if (currentIndex >= 0) root.activate(AccountManager.accounts[currentIndex])
        Keys.onEnterPressed: if (currentIndex >= 0) root.activate(AccountManager.accounts[currentIndex])

        delegate: ConsoleListItem {
            id: userItem
            required property var modelData
            required property int index
            width: ListView.view.width
            implicitHeight: Theme.px(96)
            title: modelData.nickname
            subtitle: root.describe(modelData) + (modelData.id === AccountManager.id ? qsTr(" · сейчас играет") : "")
            current: ListView.isCurrentItem && userList.activeFocus
            onClicked: {
                userList.currentIndex = index
                userList.forceActiveFocus()
                root.activate(modelData)
            }

            Avatar {
                name: userItem.modelData.nickname
                size: Theme.px(52)
                anchors.verticalCenter: parent.verticalCenter
            }
        }
    }

    // ---------- Новый пользователь ----------
    Column {
        visible: root.mode === "add"
        width: Math.min(parent.width, Theme.px(860))
        spacing: Theme.px(28)

        Text {
            width: parent.width
            wrapMode: Text.WordWrap
            text: qsTr("Новый пользователь получит свою локальную учётную запись и отдельный профиль на этой консоли.")
            color: Theme.textSecondary
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontBody
        }

        ConsoleTextField {
            id: nicknameField
            width: parent.width
            label: qsTr("Никнейм")
            placeholder: qsTr("От 3 до 16 символов: буквы, цифры, _ - .")
            inputHint: "nickname"
            KeyNavigation.down: createButton
            onAccepted: root.finishAdd()
        }

        Row {
            spacing: Theme.px(20)
            ConsoleButton {
                id: createButton
                text: qsTr("Создать")
                primary: true
                KeyNavigation.up: nicknameField
                KeyNavigation.right: cancelButton
                onClicked: root.finishAdd()
            }
            ConsoleButton {
                id: cancelButton
                text: qsTr("Отмена")
                KeyNavigation.up: nicknameField
                KeyNavigation.left: createButton
                onClicked: root.cancelAdd()
            }
        }
    }
}
