pragma ComponentBehavior: Bound
import QtQuick
import "Format.js" as Format

// Настройки → Хранилище: полоса занятости диска (свои игры, «Остальное», свободно)
// и список игр текущего пользователя с размерами. ✕ — удалить игру.
// Игры других пользователей не показываются — они входят в «Остальное».
FocusScope {
    id: root

    readonly property bool focusable: GameLibrary.installedCount > 0
    readonly property var installed: GameLibrary.games.filter(g => g.installed)
    readonly property real used: Math.max(0, GameLibrary.diskTotal - GameLibrary.diskFree)
    readonly property real other: Math.max(0, used - GameLibrary.gamesBytes)

    Component.onCompleted: GameLibrary.refreshStorage()

    function confirmRemove(game) {
        menu.show(qsTr("Удалить «%1»?").arg(game.title),
                  qsTr("Освободится %1. Установить игру снова можно из магазина.").arg(Format.bytes(game.sizeBytes)), [
            { text: qsTr("Удалить"), icon: "trash", run: () => StoreManager.uninstall(game.id) },
            { text: qsTr("Отмена") }
        ], list)
    }

    ActionMenu { id: menu }

    Column {
        id: summary
        width: Math.min(parent.width, Theme.px(1100))
        spacing: Theme.px(18)

        Row {
            spacing: Theme.px(16)
            Text {
                text: qsTr("Свободно %1 из %2").arg(Format.bytes(GameLibrary.diskFree)).arg(Format.bytes(GameLibrary.diskTotal))
                color: Theme.textPrimary
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontLarge
                font.weight: Font.Light
            }
            Spinner {
                visible: GameLibrary.measuring
                size: Theme.px(24)
                anchors.verticalCenter: parent.verticalCenter
            }
        }

        // Полоса занятости диска
        Rectangle {
            width: parent.width
            height: Theme.px(18)
            radius: height / 2
            color: Qt.rgba(1, 1, 1, 0.1)
            clip: true

            Row {
                height: parent.height
                Rectangle {
                    height: parent.height
                    width: GameLibrary.diskTotal > 0 ? summary.width * GameLibrary.gamesBytes / GameLibrary.diskTotal : 0
                    color: Theme.focus
                    Behavior on width { NumberAnimation { duration: Theme.animSlow; easing.type: Easing.OutCubic } }
                }
                Rectangle {
                    height: parent.height
                    width: GameLibrary.diskTotal > 0 ? summary.width * root.other / GameLibrary.diskTotal : 0
                    color: Theme.textMuted
                    Behavior on width { NumberAnimation { duration: Theme.animSlow; easing.type: Easing.OutCubic } }
                }
            }
        }

        // Легенда
        Row {
            spacing: Theme.px(40)
            Repeater {
                model: [
                    { color: Theme.focus, label: qsTr("Мои игры"), value: GameLibrary.gamesBytes },
                    { color: Theme.textMuted, label: qsTr("Остальное"), value: root.other },
                    { color: Qt.rgba(1, 1, 1, 0.1), label: qsTr("Свободно"), value: GameLibrary.diskFree }
                ]
                delegate: Row {
                    id: legend
                    required property var modelData
                    spacing: Theme.px(10)
                    Rectangle {
                        width: Theme.px(14)
                        height: width
                        radius: width / 2
                        color: legend.modelData.color
                        anchors.verticalCenter: parent.verticalCenter
                    }
                    Text {
                        anchors.verticalCenter: parent.verticalCenter
                        text: legend.modelData.label + " · " + Format.bytes(legend.modelData.value)
                        color: Theme.textSecondary
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontSmall
                    }
                }
            }
        }
    }

    EmptyState {
        visible: GameLibrary.installedCount === 0
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.top: summary.bottom
        anchors.topMargin: Theme.px(60)
        icon: "storage"
        title: qsTr("Игр пока нет")
        text: qsTr("Установленные игры появятся здесь с их размерами.")
    }

    ListView {
        id: list
        visible: GameLibrary.installedCount > 0
        focus: true
        anchors.top: summary.bottom
        anchors.topMargin: Theme.px(32)
        anchors.bottom: parent.bottom
        width: summary.width
        clip: true
        spacing: Theme.px(10)
        keyNavigationEnabled: true
        highlightMoveDuration: Theme.animFast
        model: root.installed

        Keys.onReturnPressed: if (currentIndex >= 0) root.confirmRemove(root.installed[currentIndex])
        Keys.onEnterPressed: if (currentIndex >= 0) root.confirmRemove(root.installed[currentIndex])

        delegate: ConsoleListItem {
            id: row
            required property var modelData
            required property int index
            width: ListView.view.width
            implicitHeight: Theme.px(88)
            title: modelData.title
            subtitle: Format.storeName(modelData.store)
            current: ListView.isCurrentItem && list.activeFocus
            onClicked: {
                list.currentIndex = index
                list.forceActiveFocus()
                root.confirmRemove(modelData)
            }

            Text {
                anchors.verticalCenter: parent.verticalCenter
                text: row.modelData.sizeBytes > 0 ? Format.bytes(row.modelData.sizeBytes) : qsTr("Подсчёт…")
                color: Theme.textPrimary
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontBody
            }
            Icon {
                visible: row.current
                name: "trash"
                size: Theme.px(24)
                lineWidth: 1.5
                color: Theme.textSecondary
                anchors.verticalCenter: parent.verticalCenter
            }
        }
    }
}
