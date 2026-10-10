pragma ComponentBehavior: Bound
import QtQuick
import "ModelSync.js" as ModelSync

// Настройки → Геймпады: отключение (Bluetooth), переименование и цвет световой панели.
FocusScope {
    id: root

    readonly property bool focusable: pads.count > 0
    property string mode: "list"            // "list" | "rename" | "color"
    property var target: null               // { id, name, defaultName, mac, connection, color }
    property string error

    readonly property var colors: [
        { name: qsTr("Синий"), color: "#2f6bff" }, { name: qsTr("Голубой"), color: "#19c3ff" },
        { name: qsTr("Фиолетовый"), color: "#8a4dff" }, { name: qsTr("Розовый"), color: "#ff4fa3" },
        { name: qsTr("Красный"), color: "#ff3b3b" }, { name: qsTr("Оранжевый"), color: "#ff8a1f" },
        { name: qsTr("Жёлтый"), color: "#ffd21f" }, { name: qsTr("Зелёный"), color: "#2fe06b" },
        { name: qsTr("Белый"), color: "#ffffff" }
    ]

    function entry(i) {
        const e = pads.get(i)
        return { id: e.id, name: e.name, defaultName: e.defaultName, mac: e.mac, connection: e.connection,
                 dualSense: e.dualSense, color: e.color, canSetColor: e.canSetColor }
    }

    function connectionText(connection) {
        return connection === "bluetooth" ? "Bluetooth" : connection === "usb" ? qsTr("USB-кабель") : qsTr("Другое подключение")
    }

    function activate(p) {
        error = ""
        const actions = [{ text: qsTr("Переименовать"), icon: "edit", run: () => root.startRename(p) }]
        if (p.canSetColor)
            actions.push({ text: qsTr("Цвет световой панели"), icon: "palette", run: () => root.startColor(p) })
        if (p.connection === "bluetooth")
            actions.push({ text: qsTr("Отключить"), icon: "power", run: () => BluetoothManager.disconnectAddress(p.mac) })
        actions.push({ text: qsTr("Отмена") })
        menu.show(p.name, p.connection === "usb"
                  ? qsTr("Подключён кабелем — чтобы отключить, отсоедините кабель.")
                  : connectionText(p.connection), actions, padList)
    }

    function startRename(p) {
        target = p
        mode = "rename"
        nameField.text = p.name
        nameField.forceActiveFocus()
        Keyboard.open(nameField)
    }

    function finishRename(name) {
        GamepadManager.rename(target.id, name)
        backToList()
    }

    function startColor(p) {
        target = p
        mode = "color"
        let index = 0
        for (let i = 0; i < colors.length; ++i) {
            if (colors[i].color.toLowerCase() === String(p.color).toLowerCase())
                index = i
        }
        swatches.current = index
        swatches.forceActiveFocus()
    }

    function applyColor(index) {
        if (!GamepadManager.setColor(target.id, colors[index].color)) {
            error = qsTr("Нет доступа к световой панели. Установите правило udev оболочки (см. README).")
            return
        }
        backToList()
    }

    function backToList() {
        mode = "list"
        target = null
        padList.forceActiveFocus()
    }

    Component.onCompleted: ModelSync.sync(pads, GamepadManager.gamepads, "id")

    Connections {
        target: GamepadManager
        function onGamepadsChanged() {
            ModelSync.sync(pads, GamepadManager.gamepads, "id")
            // Геймпад, с которым работали, отключился
            if (root.target && !GamepadManager.gamepads.some(p => p.id === root.target.id))
                root.backToList()
        }
    }

    Keys.onEscapePressed: (event) => {
        if (mode !== "list") {
            backToList()
            event.accepted = true
        } else {
            event.accepted = false
        }
    }

    ListModel { id: pads }
    ActionMenu { id: menu }

    EmptyState {
        visible: pads.count === 0
        anchors.centerIn: parent
        icon: "controller"
        title: qsTr("Геймпады не подключены")
        text: qsTr("Подключите DualSense кабелем — он сопряжётся по Bluetooth автоматически.")
    }

    // ---------- Список ----------
    ListView {
        id: padList
        visible: root.mode === "list" && pads.count > 0
        focus: true
        anchors.top: parent.top
        anchors.bottom: errorLabel.visible ? errorLabel.top : parent.bottom
        width: Math.min(parent.width, Theme.px(1000))
        clip: true
        spacing: Theme.px(12)
        keyNavigationEnabled: true
        highlightMoveDuration: Theme.animFast
        model: pads

        Keys.onReturnPressed: if (currentIndex >= 0) root.activate(root.entry(currentIndex))
        Keys.onEnterPressed: if (currentIndex >= 0) root.activate(root.entry(currentIndex))

        delegate: ConsoleListItem {
            id: padItem
            required property int index
            required property string name
            required property string connection
            required property string color
            required property bool canSetColor
            width: ListView.view.width
            icon: "controller"
            title: name
            subtitle: root.connectionText(connection)
            current: ListView.isCurrentItem && padList.activeFocus
            onClicked: {
                padList.currentIndex = index
                padList.forceActiveFocus()
                root.activate(root.entry(index))
            }

            // Текущий цвет световой панели
            Rectangle {
                visible: padItem.canSetColor
                anchors.verticalCenter: parent.verticalCenter
                width: Theme.px(22)
                height: width
                radius: width / 2
                color: padItem.color
                border.width: 1
                border.color: Theme.outlineStrong
            }
        }
    }

    Text {
        id: errorLabel
        visible: root.error !== ""
        anchors.bottom: parent.bottom
        width: Math.min(parent.width, Theme.px(1000))
        wrapMode: Text.WordWrap
        text: root.error
        color: Theme.accent
        font.family: Theme.fontFamily
        font.pixelSize: Theme.fontSmall
    }

    // ---------- Переименование ----------
    Column {
        visible: root.mode === "rename"
        width: Math.min(parent.width, Theme.px(860))
        spacing: Theme.px(28)

        ConsoleTextField {
            id: nameField
            width: parent.width
            label: qsTr("Имя геймпада")
            placeholder: root.target ? root.target.defaultName : ""
            inputHint: "nickname"
            KeyNavigation.down: saveButton
            onAccepted: saveButton.forceActiveFocus()
        }

        Row {
            spacing: Theme.px(20)
            ConsoleButton {
                id: saveButton
                text: qsTr("Сохранить")
                primary: true
                KeyNavigation.up: nameField
                KeyNavigation.right: resetButton
                onClicked: root.finishRename(nameField.text)
            }
            ConsoleButton {
                id: resetButton
                text: qsTr("Стандартное имя")
                KeyNavigation.up: nameField
                KeyNavigation.left: saveButton
                KeyNavigation.right: cancelRenameButton
                onClicked: root.finishRename("")
            }
            ConsoleButton {
                id: cancelRenameButton
                text: qsTr("Отмена")
                KeyNavigation.up: nameField
                KeyNavigation.left: resetButton
                onClicked: root.backToList()
            }
        }
    }

    // ---------- Цвет световой панели ----------
    Column {
        visible: root.mode === "color"
        spacing: Theme.px(30)

        Text {
            text: root.target ? qsTr("Цвет световой панели «%1»").arg(root.target.name) : ""
            color: Theme.textSecondary
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontBody
        }

        FocusScope {
            id: swatches
            property int current: 0
            width: swatchRow.width
            height: swatchRow.height

            Keys.onLeftPressed: (event) => {
                if (current > 0) {
                    current--
                    event.accepted = true
                } else {
                    event.accepted = false
                }
            }
            Keys.onRightPressed: if (current < root.colors.length - 1) current++
            Keys.onReturnPressed: root.applyColor(current)
            Keys.onEnterPressed: root.applyColor(current)

            Row {
                id: swatchRow
                spacing: Theme.px(22)
                Repeater {
                    model: root.colors
                    delegate: Item {
                        id: swatch
                        required property var modelData
                        required property int index
                        readonly property bool active: swatches.current === index && swatches.activeFocus
                        width: Theme.px(64)
                        height: width

                        Rectangle {
                            anchors.centerIn: parent
                            width: parent.width + Theme.px(14)
                            height: width
                            radius: width / 2
                            color: "transparent"
                            border.width: Theme.px(2)
                            border.color: Theme.focus
                            opacity: swatch.active ? 1 : 0
                            Behavior on opacity { NumberAnimation { duration: Theme.animFast } }
                        }
                        Rectangle {
                            anchors.fill: parent
                            radius: width / 2
                            color: swatch.modelData.color
                            scale: swatch.active ? 1.08 : 1
                            Behavior on scale { NumberAnimation { duration: Theme.animFast } }
                        }
                        MouseArea {
                            anchors.fill: parent
                            onClicked: {
                                swatches.current = swatch.index
                                root.applyColor(swatch.index)
                            }
                        }
                    }
                }
            }
        }

        Text {
            text: root.colors[swatches.current].name
            color: Theme.textPrimary
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontLarge
            font.weight: Font.Light
        }
    }
}
