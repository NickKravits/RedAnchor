import QtQuick

// Подложка, пока поверх оболочки открыт Steam Big Picture для входа в аккаунт (единственный
// раз, когда Steam виден). Окно Steam встаёт поверх, как игра; подложка видна, пока его ещё
// нет (клиент запускается или обновляется). После входа Big Picture закрывается сам.
// Отменить вход — PS → Steam → «Отменить вход». ○ здесь не используем: геймпад в это время
// управляет Steam, и ○ там — «назад».
FocusScope {
    id: root

    readonly property var steam: StoreManager.stores.steam || ({})
    readonly property string phase: steam.phase || ""
    readonly property bool shown: phase === "signin"
    property Item returnFocus: null

    visible: opacity > 0
    opacity: shown ? 1 : 0
    Behavior on opacity { NumberAnimation { duration: Theme.animSlow } }

    onShownChanged: {
        if (shown) {
            returnFocus = Window.activeFocusItem
            forceActiveFocus()
        } else if (returnFocus) {
            returnFocus.forceActiveFocus()
            returnFocus = null
        }
    }
    Keys.onPressed: (event) => {
        if (event.key === Qt.Key_Escape)
            StoreManager.cancelSteamSignIn()
        event.accepted = true
    }

    Rectangle {
        anchors.fill: parent
        gradient: Gradient {
            GradientStop { position: 0.0; color: "#1b1f25" }
            GradientStop { position: 1.0; color: "#0b0c0f" }
        }
    }

    Column {
        anchors.centerIn: parent
        width: Math.min(parent.width - 2 * Theme.gutter, Theme.px(1000))
        spacing: Theme.px(28)

        Icon {
            anchors.horizontalCenter: parent.horizontalCenter
            name: "steam"
            size: Theme.px(120)
            lineWidth: 1.1
        }
        Text {
            width: parent.width
            horizontalAlignment: Text.AlignHCenter
            text: qsTr("Вход в Steam")
            color: Theme.textPrimary
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontTitle
            font.weight: Font.Light
        }
        Text {
            width: parent.width
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.WordWrap
            text: root.steam.phaseProgress >= 0
                  ? qsTr("Steam обновляется: %1%. Первый запуск может занять несколько минут.").arg(Math.round(root.steam.phaseProgress * 100))
                  : qsTr("Войдите в аккаунт Steam в открывшемся окне — геймпадом или по QR-коду. После входа вы вернётесь в RedAnchor автоматически.")
            color: Theme.textSecondary
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontBody
            lineHeight: 1.25
        }
        Item {
            width: parent.width
            height: Theme.px(36)
            Spinner {
                anchors.centerIn: parent
                visible: root.steam.phaseProgress >= 0 || !root.steam.running
                size: Theme.px(32)
            }
        }
        GlyphText {
            anchors.horizontalCenter: parent.horizontalCenter
            text: qsTr("{ps} — отменить вход")
            color: Theme.textSecondary
            pixelSize: Theme.fontSmall
        }
    }
}
