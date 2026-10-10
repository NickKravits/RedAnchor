pragma ComponentBehavior: Bound
import QtQuick
import "ModelSync.js" as ModelSync

// Настройки → Bluetooth: поиск и сопряжение новых устройств, подключение,
// отключение и удаление. Геймпады здесь не показываются — для них есть раздел
// «Геймпады». Выключить Bluetooth нельзя: иначе отвалятся беспроводные геймпады.
FocusScope {
    id: root

    readonly property bool focusable: BluetoothManager.adapterAvailable
    property string error

    // Копия элемента модели: модель может обновиться, пока открыто меню
    function entry(i) {
        const e = devices.get(i)
        return { path: e.path, name: e.name, paired: e.paired, connected: e.connected, busy: e.busy }
    }

    function activate(d) {
        error = ""
        if (d.busy)
            return
        if (!d.paired) {
            BluetoothManager.pairDevice(d.path)
            return
        }
        menu.show(d.name, d.connected ? qsTr("Подключено") : qsTr("Сопряжено"), [
            d.connected ? { text: qsTr("Отключить"), icon: "power", run: () => BluetoothManager.disconnectDevice(d.path) }
                        : { text: qsTr("Подключить"), icon: "bluetooth", run: () => BluetoothManager.connectDevice(d.path) },
            { text: qsTr("Удалить устройство"), icon: "trash", run: () => root.confirmRemove(d) },
            { text: qsTr("Отмена") }
        ], deviceList)
    }

    function confirmRemove(d) {
        menu.show(qsTr("Удалить «%1»?").arg(d.name),
                  qsTr("Чтобы снова пользоваться устройством, понадобится новое сопряжение."), [
            { text: qsTr("Удалить"), icon: "trash", run: () => BluetoothManager.removeDevice(d.path) },
            { text: qsTr("Отмена") }
        ], deviceList)
    }

    Component.onCompleted: ModelSync.sync(devices, BluetoothManager.devices, "path")
    Component.onDestruction: BluetoothManager.stopDiscovery()

    Connections {
        target: BluetoothManager
        function onDevicesChanged() { ModelSync.sync(devices, BluetoothManager.devices, "path") }
        function onDeviceError(path, message) { root.error = message }
    }

    ListModel { id: devices }
    ActionMenu { id: menu }

    EmptyState {
        visible: !BluetoothManager.adapterAvailable
        anchors.centerIn: parent
        icon: "bluetooth"
        title: qsTr("Bluetooth-модуль не найден")
        text: qsTr("Подключите USB-адаптер Bluetooth — он появится здесь автоматически.")
    }

    Column {
        id: head
        visible: BluetoothManager.adapterAvailable
        width: Math.min(parent.width, Theme.px(1000))
        spacing: Theme.px(24)

        Row {
            visible: BluetoothManager.powered
            spacing: Theme.px(20)

            ConsoleButton {
                id: scanButton
                focus: true
                text: BluetoothManager.discovering ? qsTr("Остановить поиск") : qsTr("Найти устройства")
                icon: "search"
                KeyNavigation.down: deviceList.count > 0 ? deviceList : null
                onClicked: {
                    root.error = ""
                    if (BluetoothManager.discovering)
                        BluetoothManager.stopDiscovery()
                    else
                        BluetoothManager.startDiscovery()
                }
            }
            Spinner {
                visible: BluetoothManager.discovering
                size: Theme.px(26)
                anchors.verticalCenter: parent.verticalCenter
            }
            Text {
                visible: BluetoothManager.discovering
                anchors.verticalCenter: parent.verticalCenter
                text: qsTr("Переведите устройство в режим сопряжения")
                color: Theme.textSecondary
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontSmall
            }
        }

        Text {
            visible: root.error !== ""
            text: root.error
            color: Theme.accent
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontSmall
        }
    }

    ListView {
        id: deviceList
        visible: BluetoothManager.adapterAvailable && BluetoothManager.powered
        anchors.top: head.bottom
        anchors.topMargin: Theme.px(28)
        anchors.bottom: parent.bottom
        width: head.width
        clip: true
        spacing: Theme.px(12)
        keyNavigationEnabled: true
        highlightMoveDuration: Theme.animFast
        model: devices

        KeyNavigation.up: scanButton
        KeyNavigation.priority: KeyNavigation.AfterItem

        Keys.onReturnPressed: if (currentIndex >= 0) root.activate(root.entry(currentIndex))
        Keys.onEnterPressed: if (currentIndex >= 0) root.activate(root.entry(currentIndex))

        delegate: ConsoleListItem {
            id: item
            required property int index
            required property string path
            required property string name
            required property bool paired
            required property bool connected
            required property bool busy
            width: ListView.view.width
            icon: "bluetooth"
            title: name
            subtitle: busy ? (paired ? qsTr("Подождите…") : qsTr("Сопряжение…"))
                    : connected ? qsTr("Подключено")
                    : paired ? qsTr("Сопряжено")
                    : qsTr("Найдено")
            selected: connected
            current: ListView.isCurrentItem && deviceList.activeFocus
            onClicked: {
                deviceList.currentIndex = index
                deviceList.forceActiveFocus()
                root.activate(root.entry(index))
            }

            Spinner {
                visible: item.busy
                size: Theme.px(26)
                anchors.verticalCenter: parent.verticalCenter
            }
            // Подсказка на выбранном найденном устройстве: [✕] Сопряжение
            Row {
                visible: !item.paired && !item.busy && item.current
                spacing: Theme.px(10)
                anchors.verticalCenter: parent.verticalCenter
                ButtonGlyph {
                    button: "cross"
                    size: Theme.px(28)
                    anchors.verticalCenter: parent.verticalCenter
                }
                Text {
                    anchors.verticalCenter: parent.verticalCenter
                    text: qsTr("Сопряжение")
                    color: Theme.textPrimary
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontSmall
                }
            }
        }

        Text {
            anchors.centerIn: parent
            width: parent.width
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.WordWrap
            visible: deviceList.count === 0
            text: BluetoothManager.discovering ? qsTr("Ищем устройства…")
                                               : qsTr("Нет сопряжённых устройств. Нажмите «Найти устройства».")
            color: Theme.textMuted
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontBody
        }
    }
}
