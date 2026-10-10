import QtQuick

// Подложка, пока поверх оболочки открыт Steam Big Picture (вход в Steam, магазин Steam,
// подтверждение установки). Окно Steam встаёт поверх, как игра; эта подложка видна, пока
// окно Steam ещё не появилось (клиент запускается или обновляется) и сразу после него.
// Вернуться в RedAnchor — PS (оверлей → Steam → «Вернуться в RedAnchor»). ○ здесь не
// используем: геймпад в это время управляет Steam, и ○ там — «назад».
FocusScope {
    id: root

    readonly property var steam: StoreManager.stores.steam || ({})
    readonly property string phase: steam.phase || ""
    readonly property bool shown: phase === "signin" || phase === "bigpicture"
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
            StoreManager.closeSteam()
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
            text: root.phase === "signin" ? qsTr("Вход в Steam") : qsTr("Steam Big Picture")
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
                  : root.phase === "signin"
                    ? qsTr("Войдите в аккаунт Steam в открывшемся окне — геймпадом или по QR-коду. После входа вы вернётесь в RedAnchor автоматически.")
                    : qsTr("Steam открыт поверх RedAnchor.")
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
            text: qsTr("{ps} — вернуться в RedAnchor")
            color: Theme.textSecondary
            pixelSize: Theme.fontSmall
        }
    }
}
