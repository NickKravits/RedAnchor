pragma ComponentBehavior: Bound
import QtQuick

// Вкладка «Настройки»: слева список разделов, справа выбранный раздел.
// ✕ или → — войти в раздел, ○ или ← — вернуться к списку.
FocusScope {
    id: root

    readonly property bool focusable: true
    readonly property bool inPanel: panel.item !== null && (panel.item as Item).activeFocus

    signal switchUserRequested()

    readonly property var sections: [
        { key: "network", title: qsTr("Сеть"), icon: "globe" },
        { key: "bluetooth", title: qsTr("Bluetooth"), icon: "bluetooth" },
        { key: "users", title: qsTr("Пользователи"), icon: "users" },
        { key: "updates", title: qsTr("Обновления"), icon: "download" },
        { key: "storage", title: qsTr("Хранилище"), icon: "storage" },
        { key: "gamepads", title: qsTr("Геймпады"), icon: "controller" },
        { key: "about", title: qsTr("О системе"), icon: "info" }
    ]

    function enterPanel() {
        // Loader — область фокуса: фокус отдаём самому разделу
        if (panel.item && panel.item.focusable)
            (panel.item as Item).forceActiveFocus()
    }

    Keys.onRightPressed: (event) => {
        if (sectionList.activeFocus) {
            enterPanel()
            event.accepted = true
        } else {
            event.accepted = false
        }
    }
    Keys.onLeftPressed: (event) => {
        if (inPanel) {
            sectionList.forceActiveFocus()
            event.accepted = true
        } else {
            event.accepted = false
        }
    }
    Keys.onEscapePressed: (event) => {
        if (inPanel) {
            sectionList.forceActiveFocus()
            event.accepted = true
        } else {
            event.accepted = false
        }
    }

    // Стеклянная подложка: на живом фоне без неё строки и текст сливаются
    GlassPanel {
        anchors.fill: parent
        anchors.margins: -Theme.px(32)
        radius: Theme.radiusLarge
        color: Qt.rgba(0.055, 0.06, 0.072, 0.80)
    }

    ListView {
        id: sectionList
        focus: true
        anchors.left: parent.left
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        width: Theme.px(470)
        spacing: Theme.px(10)
        keyNavigationEnabled: true
        highlightMoveDuration: Theme.animFast
        model: root.sections

        delegate: ConsoleListItem {
            id: sectionItem
            required property var modelData
            required property int index
            width: ListView.view.width
            implicitHeight: Theme.px(76)
            icon: modelData.icon
            title: modelData.title
            subtitle: modelData.note || ""
            current: ListView.isCurrentItem && sectionList.activeFocus
            opacity: ListView.isCurrentItem || sectionList.activeFocus ? 1 : 0.5
            Behavior on opacity { NumberAnimation { duration: Theme.animFast } }
            onClicked: {
                sectionList.currentIndex = index
                root.enterPanel()
            }

            // Раздел, открытый справа, когда фокус в нём: светлая плашка и полоска слева
            Rectangle {
                visible: sectionItem.ListView.isCurrentItem && !sectionList.activeFocus
                parent: sectionItem
                anchors.fill: parent
                radius: Theme.radius
                color: Theme.selection
                border.width: 1
                border.color: Qt.rgba(1, 1, 1, 0.14)
                z: -1
            }
            Rectangle {
                visible: sectionItem.ListView.isCurrentItem && !sectionList.activeFocus
                parent: sectionItem
                anchors.left: parent.left
                anchors.leftMargin: -Theme.px(14)
                anchors.verticalCenter: parent.verticalCenter
                width: Math.max(3, Theme.px(4))
                height: Theme.px(30)
                radius: width / 2
                color: Theme.focus
            }
        }
    }

    Rectangle {
        id: divider
        anchors.left: sectionList.right
        anchors.leftMargin: Theme.px(48)
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        width: 1
        gradient: Gradient {
            GradientStop { position: 0.0; color: "transparent" }
            GradientStop { position: 0.15; color: Theme.outline }
            GradientStop { position: 0.85; color: Theme.outline }
            GradientStop { position: 1.0; color: "transparent" }
        }
    }

    Item {
        id: panelArea
        anchors.left: divider.right
        anchors.leftMargin: Theme.px(56)
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.bottom: parent.bottom

        Text {
            id: panelTitle
            text: sectionList.currentIndex >= 0 ? root.sections[sectionList.currentIndex].title : ""
            color: Theme.textPrimary
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontHeadline
            font.weight: Font.Normal
            font.letterSpacing: Theme.trackingTight
        }

        Loader {
            id: panel
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: panelTitle.bottom
            anchors.topMargin: Theme.px(32)
            anchors.bottom: parent.bottom
            sourceComponent: {
                switch (sectionList.currentIndex >= 0 ? root.sections[sectionList.currentIndex].key : "") {
                case "network": return networkPanel
                case "bluetooth": return bluetoothPanel
                case "users": return usersPanel
                case "updates": return updatesPanel
                case "storage": return storagePanel
                case "gamepads": return gamepadsPanel
                case "about": return aboutPanel
                default: return null
                }
            }
            onLoaded: fade.restart()
            NumberAnimation { id: fade; target: panel; property: "opacity"; from: 0; to: 1; duration: Theme.animNormal }
        }
    }

    Component { id: networkPanel; NetworkSettings {} }
    Component { id: bluetoothPanel; BluetoothSettings {} }
    Component {
        id: usersPanel
        UsersSettings { onSwitchUserRequested: root.switchUserRequested() }
    }
    Component { id: updatesPanel; UpdatesSettings {} }
    Component { id: storagePanel; StorageSettings {} }
    Component { id: gamepadsPanel; GamepadSettings {} }
    Component { id: aboutPanel; AboutSettings {} }
}
