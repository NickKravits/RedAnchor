import QtQuick
import "Format.js" as Format
import "Storefront.js" as Storefront

// Карточка игры витрины: широкая обложка 16:9 из магазина, под ней название и цена.
// item: null — заглушка загрузки. active — выбрана (ореол и лёгкое увеличение, как у GameCover).
Item {
    id: root

    property var item: null
    property bool active: false
    property bool showStore: false
    readonly property var own: {
        StoreCatalog.revision
        return root.item ? StoreCatalog.ownership(root.item) : ({})
    }
    readonly property real coverHeight: Math.round(width * 9 / 16)

    implicitHeight: coverHeight + caption.height + Theme.px(14)

    Item {
        id: coverArea
        width: parent.width
        height: root.coverHeight
        scale: root.active ? Theme.focusScale : 1
        transformOrigin: Item.Bottom
        Behavior on scale { NumberAnimation { duration: Theme.animFast; easing.type: Theme.easeOut } }

        // Мягкая тень под карточкой (как у GameCover)
        Repeater {
            model: 3
            delegate: Rectangle {
                required property int index
                readonly property real spread: (index + 1) * Theme.px(4)
                x: -spread
                y: -spread + Theme.px(6)
                width: coverArea.width + spread * 2
                height: coverArea.height + spread * 2
                radius: Theme.radiusTile + spread
                color: "transparent"
                border.width: Theme.px(4)
                border.color: Qt.rgba(0, 0, 0, 0.12 - index * 0.035)
                antialiasing: true
            }
        }
        // Кольцо выбранной карточки
        FocusRing {
            shown: root.active
            radius: Theme.radiusTile
        }

        StoreShimmer {
            anchors.fill: parent
            visible: root.item === null || cover.status !== Image.Ready
            running: visible && (root.item === null || cover.status === Image.Loading)
        }

        RoundedClip {
            anchors.fill: parent
            radius: Theme.radiusTile
            visible: root.item !== null

            // Пока нет картинки — название на тёмной плашке
            Text {
                anchors.fill: parent
                anchors.margins: Theme.px(16)
                visible: cover.status !== Image.Ready
                text: root.item ? root.item.title : ""
                color: Theme.textSecondary
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontSmall
                wrapMode: Text.WordWrap
                elide: Text.ElideRight
                verticalAlignment: Text.AlignBottom
            }
            Image {
                id: cover
                anchors.fill: parent
                source: root.item ? Storefront.thumb(root.item.cover || root.item.tall, 640) : ""
                fillMode: Image.PreserveAspectCrop
                asynchronous: true
                cache: true
                sourceSize.width: 640
                opacity: status === Image.Ready ? 1 : 0
                Behavior on opacity { NumberAnimation { duration: Theme.animNormal } }
            }

            // Раздача Epic — плашка в углу
            Badge {
                visible: root.item !== null && (root.item.promo === "now" || root.item.promo === "upcoming")
                anchors.left: parent.left
                anchors.top: parent.top
                anchors.margins: Theme.spacingS
                text: root.item && root.item.promo === "now" ? qsTr("Бесплатно") : qsTr("Скоро бесплатно")
                tone: root.item && root.item.promo === "now" ? "light" : "dark"
            }

            // Магазин (в поиске — по всем магазинам)
            Badge {
                visible: root.showStore && root.item !== null
                anchors.right: parent.right
                anchors.top: parent.top
                anchors.margins: Theme.spacingS
                text: root.item ? Format.storeName(root.item.store) : ""
            }

            // Куплена: установлена — галочка, можно установить — загрузка
            Badge {
                visible: root.own.owned === true
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                anchors.margins: Theme.spacingS
                icon: root.own.installed ? "check" : "download"
                tone: "light"
                size: Theme.px(34)
            }
        }
    }

    // ---------- Название и цена ----------
    Column {
        id: caption
        anchors.top: coverArea.bottom
        anchors.topMargin: Theme.px(14)
        width: parent.width
        spacing: Theme.px(4)

        // Заглушки строк
        StoreShimmer {
            visible: root.item === null
            width: parent.width * 0.7
            height: Theme.px(20)
            radius: Theme.radiusSmall
        }
        StoreShimmer {
            visible: root.item === null
            width: parent.width * 0.35
            height: Theme.px(18)
            radius: Theme.radiusSmall
        }

        Text {
            visible: root.item !== null
            width: parent.width
            text: root.item ? root.item.title : ""
            color: root.active ? Theme.textPrimary : Theme.textSecondary
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontSmall
            font.weight: root.active ? Font.Medium : Font.Normal
            elide: Text.ElideRight
        }
        StorePrice {
            visible: root.item !== null
            item: root.item
            own: root.own
            pixelSize: Theme.fontSmall
            height: Theme.px(28)
        }
    }
}
