import QtQuick

// Вход в Steam — один раз: открывается Steam Big Picture поверх оболочки (как игра),
// входить можно геймпадом или по QR-коду из мобильного приложения Steam. Как только вход
// удался, Big Picture закрывается сам, а клиент Steam дальше работает в фоне.
FocusScope {
    id: root

    readonly property var steam: StoreManager.stores.steam || ({})
    readonly property string phase: steam.phase || ""
    readonly property bool working: phase === "starting" || phase === "signin" || phase === "stopping"

    implicitHeight: column.implicitHeight

    // Пока идёт вход, кнопка «Войти» скрыта — держим фокус на «Отмене», и наоборот
    onWorkingChanged: Qt.callLater(() => {
        if (root.activeFocus || !root.Window.activeFocusItem || !root.Window.activeFocusItem.visible)
            (root.working ? cancelButton : signInButton).forceActiveFocus()
    })

    Column {
        id: column
        width: parent.width
        spacing: Theme.px(26)

        Row {
            spacing: Theme.px(28)

            Rectangle {
                width: Theme.px(96)
                height: width
                radius: width / 2
                color: Qt.rgba(1, 1, 1, 0.06)
                border.width: 1
                border.color: Theme.outlineStrong
                anchors.verticalCenter: parent.verticalCenter
                Icon {
                    anchors.centerIn: parent
                    name: "steam"
                    size: Theme.px(52)
                    lineWidth: 1.3
                }
            }
            Column {
                anchors.verticalCenter: parent.verticalCenter
                spacing: Theme.px(8)
                Text {
                    text: qsTr("Войдите в Steam")
                    color: Theme.textPrimary
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontLarge
                    font.weight: Font.Light
                }
                Text {
                    width: column.width - Theme.px(124)
                    wrapMode: Text.WordWrap
                    text: qsTr("Откроется Steam Big Picture — войдите геймпадом или по QR-коду из мобильного приложения Steam. Входить нужно один раз: дальше Steam работает в фоне, а игры, загрузки и сетевая игра — прямо из RedAnchor.")
                    color: Theme.textSecondary
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontSmall
                    lineHeight: 1.2
                }
            }
        }

        Row {
            spacing: Theme.px(20)
            ConsoleButton {
                id: signInButton
                focus: true
                visible: !root.working
                text: qsTr("Войти в Steam")
                icon: "user"
                primary: true
                onClicked: StoreManager.steamSignIn()
            }
            ConsoleButton {
                id: cancelButton
                visible: root.working
                text: qsTr("Отмена")
                icon: "close"
                onClicked: StoreManager.cancelSteamSignIn()
            }
            Spinner {
                visible: root.working
                size: Theme.px(28)
                anchors.verticalCenter: parent.verticalCenter
            }
            Text {
                visible: root.working
                anchors.verticalCenter: parent.verticalCenter
                text: root.steam.phaseProgress >= 0 ? qsTr("Steam обновляется: %1%").arg(Math.round(root.steam.phaseProgress * 100))
                      : root.phase === "signin" ? qsTr("Ждём входа в Steam…")
                      : root.phase === "stopping" ? qsTr("Сохраняем вход…")
                      : qsTr("Steam запускается…")
                color: Theme.textSecondary
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontSmall
            }
        }

        Text {
            visible: text !== ""
            width: parent.width
            wrapMode: Text.WordWrap
            text: root.steam.message || ""
            color: Theme.warning
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontSmall
        }
    }
}
