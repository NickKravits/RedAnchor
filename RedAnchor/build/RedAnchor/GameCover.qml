import QtQuick
import QtMultimedia
import "Format.js" as Format

// Квадратная плитка игры: обложка SteamGridDB (или заглушка с названием),
// значок магазина, состояние загрузки. Если на плитке задержаться (active),
// вместо картинки проигрывается видео-обложка — когда она есть.
Item {
    id: root

    property string gameId
    property string store
    property string appId
    property string title
    // "" | "queued" | "downloading" | "failed"
    property string status
    property real progress: -1
    property bool installed: false
    property bool showInstalled: false   // отметка «установлено» (в магазине)
    property bool active: false          // выбрана — подсветка и видео-обложка
    property bool showStore: true

    property var art: GameArt.art(gameId, store, appId, title)

    Connections {
        target: GameArt
        function onArtChanged(id) {
            if (id === root.gameId)
                root.art = GameArt.art(root.gameId, root.store, root.appId, root.title)
        }
    }

    // Мягкий ореол выбранной плитки
    Rectangle {
        anchors.fill: parent
        anchors.margins: -Theme.px(7)
        radius: Theme.px(18)
        color: "transparent"
        border.width: Theme.px(3)
        border.color: Theme.focus
        opacity: root.active ? 1 : 0
        Behavior on opacity { NumberAnimation { duration: Theme.animFast } }
    }

    // Обложка со скруглёнными углами — и картинка, и видео, и значки поверх
    RoundedClip {
        id: frame
        anchors.fill: parent
        radius: Theme.px(14)

        Rectangle {
            anchors.fill: parent
            color: "#101827"
        }

        // Заглушка: цвет по названию и само название
        Rectangle {
            anchors.fill: parent
            visible: cover.status !== Image.Ready
            gradient: Gradient {
                GradientStop { position: 0.0; color: Qt.hsla((root.title.length * 37 % 360) / 360, 0.45, 0.32, 1) }
                GradientStop { position: 1.0; color: Qt.hsla((root.title.length * 37 % 360) / 360, 0.5, 0.14, 1) }
            }
            Text {
                anchors.fill: parent
                anchors.margins: Theme.px(16)
                text: root.title
                color: Theme.textPrimary
                font.family: Theme.fontFamily
                font.pixelSize: Math.max(Theme.px(16), root.width * 0.1)
                font.weight: Font.Medium
                wrapMode: Text.WordWrap
                elide: Text.ElideRight
                verticalAlignment: Text.AlignBottom
            }
        }

        Image {
            id: cover
            anchors.fill: parent
            source: root.art.grid || ""
            fillMode: Image.PreserveAspectCrop
            asynchronous: true
            sourceSize.width: 512
            sourceSize.height: 512
            opacity: status === Image.Ready ? 1 : 0
            Behavior on opacity { NumberAnimation { duration: Theme.animNormal } }
        }

        // ---------- Видео-обложка ----------
        Timer {
            id: videoDelay
            interval: 700
            running: root.active && (root.art.video || "") !== ""
            onTriggered: player.play()
        }
        MediaPlayer {
            id: player
            source: root.active ? (root.art.video || "") : ""
            loops: MediaPlayer.Infinite
            videoOutput: video
            onSourceChanged: if (source.toString() === "") stop()
        }
        VideoOutput {
            id: video
            anchors.fill: parent
            fillMode: VideoOutput.PreserveAspectCrop
            opacity: root.active && player.playbackState === MediaPlayer.PlayingState ? 1 : 0
            Behavior on opacity { NumberAnimation { duration: Theme.animNormal } }
        }

        // Затемнение под значками
        Rectangle {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            height: parent.height * 0.4
            visible: root.status !== "" || (root.showInstalled && root.installed)
            gradient: Gradient {
                GradientStop { position: 0.0; color: "transparent" }
                GradientStop { position: 1.0; color: Qt.rgba(0, 0, 0, 0.75) }
            }
        }

        // ---------- Загрузка ----------
        Column {
            visible: root.status !== ""
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            anchors.margins: Theme.px(12)
            spacing: Theme.px(8)

            Text {
                text: root.status === "queued" ? qsTr("В очереди")
                    : root.status === "failed" ? qsTr("Ошибка загрузки")
                    : root.progress >= 0 ? Math.round(root.progress * 100) + "%" : qsTr("Подготовка…")
                color: root.status === "failed" ? Theme.warning : Theme.textPrimary
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontSmall
                font.weight: Font.Medium
            }
            Rectangle {
                width: parent.width
                height: Theme.px(5)
                radius: height / 2
                color: Qt.rgba(1, 1, 1, 0.2)
                visible: root.status === "downloading"
                Rectangle {
                    width: parent.width * Math.max(0, root.progress)
                    height: parent.height
                    radius: height / 2
                    color: Theme.focus
                    Behavior on width { NumberAnimation { duration: Theme.animNormal } }
                }
            }
        }

        // Установлена (в магазине)
        Rectangle {
            visible: root.showInstalled && root.installed && root.status === ""
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            anchors.margins: Theme.px(10)
            width: Theme.px(34)
            height: width
            radius: width / 2
            color: Theme.focus
            Icon {
                anchors.centerIn: parent
                name: "check"
                size: Theme.px(20)
                lineWidth: 2
                color: Theme.background
            }
        }

        // Магазин
        Rectangle {
            visible: root.showStore && root.store !== ""
            anchors.left: parent.left
            anchors.top: parent.top
            anchors.margins: Theme.px(10)
            width: storeLabel.implicitWidth + Theme.px(16)
            height: Theme.px(26)
            radius: height / 2
            color: Qt.rgba(0, 0, 0, 0.6)
            Text {
                id: storeLabel
                anchors.centerIn: parent
                text: Format.storeName(root.store)
                color: Theme.textPrimary
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontCaption
                font.weight: Font.Medium
            }
        }
    }
}
