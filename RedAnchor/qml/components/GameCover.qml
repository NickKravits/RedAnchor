import QtQuick
import QtMultimedia
import "Format.js" as Format

// Квадратная плитка игры: обложка SteamGridDB (или заглушка с названием),
// значок магазина, состояние загрузки. Если на плитке задержаться (active),
// вместо картинки проигрывается видео-обложка — когда она есть.
// Выбранная плитка — белое кольцо с ореолом и лёгкий блик по обложке.
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

    // Оттенок заглушки по названию: приглушённый, в тон стальному фону
    readonly property real hue: {
        let h = 0
        for (let i = 0; i < title.length; ++i)
            h = (h * 31 + title.charCodeAt(i)) >>> 0
        return (h % 360) / 360
    }

    Connections {
        target: GameArt
        function onArtChanged(id) {
            if (id === root.gameId)
                root.art = GameArt.art(root.gameId, root.store, root.appId, root.title)
        }
    }

    // Мягкая тень под плиткой: кольца снаружи, смещённые вниз
    Repeater {
        model: 3
        delegate: Rectangle {
            required property int index
            readonly property real spread: (index + 1) * Theme.px(4)
            x: -spread
            y: -spread + Theme.px(6)
            width: root.width + spread * 2
            height: root.height + spread * 2
            radius: Theme.radiusTile + spread
            color: "transparent"
            border.width: Theme.px(4)
            border.color: Qt.rgba(0, 0, 0, 0.12 - index * 0.035)
            antialiasing: true
        }
    }

    // Кольцо выбранной плитки
    FocusRing {
        shown: root.active
        radius: Theme.radiusTile
    }

    // Обложка со скруглёнными углами — и картинка, и видео, и значки поверх
    RoundedClip {
        id: frame
        anchors.fill: parent
        radius: Theme.radiusTile

        Rectangle {
            anchors.fill: parent
            color: "#14171c"
        }

        // Заглушка: приглушённый градиент по названию, крупная буква и само название
        Rectangle {
            anchors.fill: parent
            visible: cover.status !== Image.Ready
            gradient: Gradient {
                GradientStop { position: 0.0; color: Qt.hsla(root.hue, 0.22, 0.30, 1) }
                GradientStop { position: 1.0; color: Qt.hsla(root.hue, 0.25, 0.11, 1) }
            }
            Text {
                anchors.right: parent.right
                anchors.top: parent.top
                anchors.rightMargin: root.width * 0.06
                anchors.topMargin: -root.width * 0.08
                text: root.title.length > 0 ? root.title.charAt(0).toUpperCase() : ""
                color: Qt.rgba(1, 1, 1, 0.07)
                font.family: Theme.fontFamily
                font.pixelSize: root.width * 0.62
                font.weight: Font.Bold
            }
            Text {
                anchors.fill: parent
                anchors.margins: Math.max(Theme.px(14), root.width * 0.08)
                text: root.title
                color: Theme.textPrimary
                font.family: Theme.fontFamily
                font.pixelSize: Math.max(Theme.px(16), root.width * 0.095)
                font.weight: Font.DemiBold
                font.letterSpacing: Theme.trackingTight
                lineHeight: 1.05
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

        // Блик по выбранной обложке: свет падает сверху слева
        Rectangle {
            anchors.fill: parent
            opacity: root.active ? 1 : 0
            Behavior on opacity { NumberAnimation { duration: Theme.animNormal } }
            gradient: Gradient {
                orientation: Gradient.Vertical
                GradientStop { position: 0.0; color: Qt.rgba(1, 1, 1, 0.16) }
                GradientStop { position: 0.35; color: Qt.rgba(1, 1, 1, 0.0) }
            }
        }

        // Затемнение под значками
        Rectangle {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            height: parent.height * 0.45
            visible: root.status !== "" || (root.showInstalled && root.installed)
            gradient: Gradient {
                GradientStop { position: 0.0; color: "transparent" }
                GradientStop { position: 1.0; color: Qt.rgba(0.02, 0.025, 0.03, 0.85) }
            }
        }

        // ---------- Загрузка ----------
        Column {
            visible: root.status !== ""
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            anchors.margins: Math.max(Theme.px(12), root.width * 0.06)
            spacing: Theme.px(10)

            Badge {
                size: Math.max(Theme.px(24), Math.min(Theme.px(30), root.width * 0.12))
                tone: root.status === "failed" ? "warning" : root.status === "downloading" ? "light" : "dark"
                icon: root.status === "failed" ? "warning" : root.status === "downloading" ? "download" : ""
                text: root.status === "queued" ? qsTr("В очереди")
                    : root.status === "failed" ? qsTr("Ошибка загрузки")
                    : root.progress >= 0 ? Math.round(root.progress * 100) + "%" : qsTr("Подготовка…")
            }
            ProgressBar {
                width: parent.width
                height: Math.max(3, Theme.px(5))
                visible: root.status === "downloading"
                value: root.progress
                trackColor: Qt.rgba(1, 1, 1, 0.22)
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
                lineWidth: 2.2
                color: Theme.textOnFocus
            }
        }

        // Магазин
        Badge {
            visible: root.showStore && root.store !== ""
            anchors.left: parent.left
            anchors.top: parent.top
            anchors.margins: Theme.px(10)
            size: Theme.px(26)
            text: Format.storeName(root.store)
        }

        // Тонкая светлая кромка: плитка не сливается с тёмной обложкой и фоном
        Rectangle {
            anchors.fill: parent
            radius: Theme.radiusTile
            color: "transparent"
            border.width: 1
            border.color: Qt.rgba(1, 1, 1, root.active ? 0.0 : 0.10)
        }
    }
}
