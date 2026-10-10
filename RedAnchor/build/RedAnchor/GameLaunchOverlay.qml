import QtQuick
import "Compat.js" as Compat

// Экран запуска игры: фон игры, название, «Игра запускается…». Если игра официально
// не поддерживает DualSense — рисунок DualSense и предупреждение о системе ввода RedAnchor.
// Пока экран виден, он забирает управление; при ошибке ○ или ✕ закрывают его.
FocusScope {
    id: root

    readonly property bool shown: GameLauncher.state === "starting" || GameLauncher.state === "failed"
    readonly property bool failed: GameLauncher.state === "failed"
    readonly property var game: GameLauncher.game

    property int compatRevision: 0
    property int artRevision: 0
    readonly property var compat: {
        root.compatRevision
        return game && game.store ? Compat.describe(StoreManager.compatibility(game.store, game.appId), GamepadManager.dualSense)
                                  : Compat.describe(null, false)
    }
    readonly property var art: {
        root.artRevision
        return game && game.id ? GameArt.art(game.id, game.store, game.appId, game.title) : ({})
    }
    readonly property bool warn: compat.level === "remap" || compat.level === "keyboard"
    property Item returnFocus: null

    visible: opacity > 0
    opacity: shown ? 1 : 0
    Behavior on opacity { NumberAnimation { duration: Theme.animSlow } }

    // Пока виден — перехватываем управление, потом возвращаем фокус туда, где он был
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
        if (root.failed && (event.key === Qt.Key_Escape || event.key === Qt.Key_Return || event.key === Qt.Key_Enter))
            GameLauncher.dismiss()
        else if (!root.failed && event.key === Qt.Key_Escape)
            GameLauncher.cancel()
        event.accepted = true
    }

    Connections {
        target: StoreManager
        function onCompatibilityChanged(id) { root.compatRevision++ }
    }
    Connections {
        target: GameArt
        function onArtChanged(id) {
            if (root.game && id === root.game.id)
                root.artRevision++
        }
    }

    Rectangle {
        anchors.fill: parent
        color: "#05070d"
    }
    Image {
        anchors.fill: parent
        source: root.art.hero || ""
        fillMode: Image.PreserveAspectCrop
        asynchronous: true
        opacity: status === Image.Ready ? 1 : 0
        Behavior on opacity { NumberAnimation { duration: Theme.animSlow } }
    }
    Rectangle {
        anchors.fill: parent
        gradient: Gradient {
            orientation: Gradient.Horizontal
            GradientStop { position: 0.0; color: Qt.rgba(0.01, 0.02, 0.05, 0.92) }
            GradientStop { position: 0.6; color: Qt.rgba(0.01, 0.02, 0.05, 0.6) }
            GradientStop { position: 1.0; color: Qt.rgba(0.01, 0.02, 0.05, 0.85) }
        }
    }

    // ---------- Название и состояние ----------
    Column {
        anchors.left: parent.left
        anchors.leftMargin: Theme.gutter
        anchors.bottom: parent.bottom
        anchors.bottomMargin: Theme.px(150)
        width: root.warn ? parent.width * 0.48 : parent.width - 2 * Theme.gutter
        spacing: Theme.px(24)

        Text {
            width: parent.width
            text: root.game ? (root.game.title || "") : ""
            color: Theme.textPrimary
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontDisplay
            font.weight: Font.Light
            wrapMode: Text.WordWrap
        }

        Row {
            spacing: Theme.px(16)
            Spinner {
                visible: !root.failed
                size: Theme.px(28)
                anchors.verticalCenter: parent.verticalCenter
            }
            Icon {
                visible: root.failed
                name: "warning"
                size: Theme.px(28)
                lineWidth: 1.5
                color: Theme.warning
                anchors.verticalCenter: parent.verticalCenter
            }
            Text {
                anchors.verticalCenter: parent.verticalCenter
                width: Math.min(implicitWidth, root.width * 0.45)
                text: GameLauncher.message
                color: root.failed ? Theme.warning : Theme.textSecondary
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontLarge
                font.weight: Font.Light
                wrapMode: Text.WordWrap
            }
        }

        // Предупреждение о поддержке геймпада
        Rectangle {
            visible: root.warn
            width: parent.width
            height: warningText.implicitHeight + Theme.px(40)
            radius: Theme.radius
            color: Qt.rgba(0.94, 0.71, 0.3, 0.12)
            border.width: 1
            border.color: Qt.rgba(0.94, 0.71, 0.3, 0.45)

            Row {
                anchors.fill: parent
                anchors.margins: Theme.px(20)
                spacing: Theme.px(16)
                Icon {
                    name: root.compat.level === "keyboard" ? "warning" : "controller"
                    size: Theme.px(28)
                    lineWidth: 1.5
                    color: Theme.warning
                }
                Text {
                    id: warningText
                    width: parent.width - Theme.px(44)
                    text: root.compat.level === "remap"
                          ? qsTr("Игра официально не поддерживает DualSense. Будет использована система ввода RedAnchor: управление может быть неточным, а значки кнопок — отображаться некорректно.")
                          : root.compat.text
                    color: Theme.textPrimary
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontBody
                    lineHeight: 1.2
                    wrapMode: Text.WordWrap
                }
            }
        }

        // Последние строки журнала игры — чтобы было видно, что пошло не так
        Text {
            visible: root.failed && GameLauncher.details !== ""
            width: parent.width
            text: GameLauncher.details
            color: Theme.textSecondary
            font.family: "monospace"
            font.pixelSize: Theme.fontCaption
            wrapMode: Text.WrapAnywhere
            maximumLineCount: 8
            elide: Text.ElideRight
        }

        Text {
            visible: !root.failed && GameLauncher.slow
            width: parent.width
            wrapMode: Text.WordWrap
            text: qsTr("Запуск идёт дольше обычного: первый запуск игры через Proton может занять пару минут.")
            color: Theme.textSecondary
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontSmall
        }

        GlyphText {
            text: root.failed ? qsTr("{circle} — закрыть") : qsTr("{circle} — отменить запуск")
            color: Theme.textSecondary
            pixelSize: Theme.fontSmall
        }
    }

    // ---------- DualSense: игра официально его не поддерживает ----------
    DualSense {
        visible: root.warn
        anchors.right: parent.right
        anchors.rightMargin: Theme.gutter
        anchors.verticalCenter: parent.verticalCenter
        width: parent.width * 0.36
        height: width * implicitHeight / implicitWidth
        showDimensions: false
        lightbarMode: "breathe"
        lightbarColor: Theme.warning
        playerLeds: 1
        opacity: 0.9
    }
}
