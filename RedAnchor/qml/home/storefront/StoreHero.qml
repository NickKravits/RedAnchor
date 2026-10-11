pragma ComponentBehavior: Bound
import QtQuick
import "Storefront.js" as Storefront

// Большая карусель витрины: широкая картинка игры, название, цена.
// ←/→ — листать (по кругу), ✕ — открыть игру. Без фокуса листается сама.
FocusScope {
    id: root

    property var items: []
    property bool loading: false
    property string eyebrow                     // подпись над названием («Популярное в GOG»)
    readonly property var current: shown[list.currentIndex] || null

    signal activated(var item)

    property var shown: []
    property string shownJson: ""
    function update() {
        const json = JSON.stringify(items || [])
        if (json === shownJson)
            return
        const index = list.currentIndex
        shownJson = json
        shown = items || []
        list.currentIndex = Math.max(0, Math.min(index, shown.length - 1))
    }
    onItemsChanged: update()
    Component.onCompleted: update()

    // Кольцо фокуса
    FocusRing {
        shown: root.activeFocus
        radius: Theme.radiusLarge
    }

    StoreShimmer {
        anchors.fill: parent
        radius: Theme.radiusLarge
        visible: root.shown.length === 0
        running: visible && root.loading
    }

    RoundedClip {
        anchors.fill: parent
        radius: Theme.radiusLarge
        visible: root.shown.length > 0

        ListView {
            id: list
            anchors.fill: parent
            focus: true
            orientation: ListView.Horizontal
            snapMode: ListView.SnapOneItem
            highlightRangeMode: ListView.StrictlyEnforceRange
            preferredHighlightBegin: 0
            preferredHighlightEnd: width
            highlightMoveDuration: Theme.animSlow
            keyNavigationEnabled: true
            keyNavigationWraps: true
            boundsBehavior: Flickable.StopAtBounds
            model: root.shown

            Keys.onReturnPressed: if (root.current) root.activated(root.current)
            Keys.onEnterPressed: if (root.current) root.activated(root.current)

            delegate: Item {
                id: slide
                required property int index
                required property var modelData
                readonly property bool isCurrent: ListView.isCurrentItem
                width: list.width
                height: list.height

                Rectangle {
                    anchors.fill: parent
                    color: Theme.backgroundAlt
                }
                Image {
                    id: art
                    anchors.fill: parent
                    source: Storefront.thumb(slide.modelData.hero || slide.modelData.cover, 1920)
                    fillMode: Image.PreserveAspectCrop
                    asynchronous: true
                    sourceSize.width: 1920
                    opacity: status === Image.Ready ? 1 : 0
                    // Лёгкое «дыхание» картинки на текущем слайде
                    scale: slide.isCurrent && root.activeFocus ? 1.03 : 1
                    Behavior on opacity { NumberAnimation { duration: Theme.animSlow } }
                    Behavior on scale { NumberAnimation { duration: 4000; easing.type: Theme.easeOut } }
                }
                // Затемнение слева и снизу: текст поверх картинки должен читаться
                Rectangle {
                    anchors.fill: parent
                    gradient: Gradient {
                        orientation: Gradient.Horizontal
                        GradientStop { position: 0.0; color: Qt.rgba(Theme.background.r, Theme.background.g, Theme.background.b, 0.9) }
                        GradientStop { position: 0.45; color: Qt.rgba(Theme.background.r, Theme.background.g, Theme.background.b, 0.55) }
                        GradientStop { position: 0.8; color: Qt.rgba(Theme.background.r, Theme.background.g, Theme.background.b, 0.0) }
                    }
                }
                Rectangle {
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.bottom: parent.bottom
                    height: parent.height * 0.5
                    gradient: Gradient {
                        GradientStop { position: 0.0; color: "transparent" }
                        GradientStop { position: 1.0; color: Qt.rgba(Theme.background.r, Theme.background.g, Theme.background.b, 0.75) }
                    }
                }

                Column {
                    anchors.left: parent.left
                    anchors.bottom: parent.bottom
                    anchors.leftMargin: Theme.px(56)
                    anchors.bottomMargin: Theme.px(48)
                    width: Math.min(parent.width * 0.55, Theme.px(900))
                    spacing: Theme.px(14)
                    // Текст выезжает вместе со сменой слайда
                    opacity: slide.isCurrent ? 1 : 0
                    transform: Translate { x: slide.isCurrent ? 0 : Theme.px(40); Behavior on x { NumberAnimation { duration: Theme.animSlow; easing.type: Theme.easeEmphasized } } }
                    Behavior on opacity { NumberAnimation { duration: Theme.animSlow } }

                    Text {
                        text: Storefront.promo(slide.modelData) || root.eyebrow
                        color: slide.modelData.promo === "now" ? Theme.focus : Theme.textSecondary
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontSmall
                        font.weight: Font.Medium
                        font.capitalization: Font.AllUppercase
                        font.letterSpacing: Theme.trackingCaps
                    }
                    Text {
                        width: parent.width
                        text: slide.modelData.title
                        color: Theme.textPrimary
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontTitle
                        font.weight: Font.Light
                        font.letterSpacing: Theme.trackingTight
                        wrapMode: Text.WordWrap
                        maximumLineCount: 2
                        elide: Text.ElideRight
                    }
                    Text {
                        width: parent.width
                        visible: text !== ""
                        text: slide.modelData.description || Storefront.meta(slide.modelData)
                        color: Theme.textSecondary
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontSmall
                        lineHeight: 1.2
                        wrapMode: Text.WordWrap
                        maximumLineCount: 2
                        elide: Text.ElideRight
                    }
                    Item { width: 1; height: Theme.px(2) }
                    Row {
                        spacing: Theme.px(28)
                        StorePrice {
                            anchors.verticalCenter: parent.verticalCenter
                            item: slide.modelData
                            own: {
                                StoreCatalog.revision
                                return StoreCatalog.ownership(slide.modelData)
                            }
                            pixelSize: Theme.fontBody
                        }
                        Row {
                            anchors.verticalCenter: parent.verticalCenter
                            spacing: Theme.px(10)
                            opacity: root.activeFocus ? 1 : 0
                            Behavior on opacity { NumberAnimation { duration: Theme.animFast } }
                            ButtonGlyph {
                                button: "cross"
                                size: Theme.px(28)
                                anchors.verticalCenter: parent.verticalCenter
                            }
                            Text {
                                anchors.verticalCenter: parent.verticalCenter
                                text: qsTr("Подробнее")
                                color: Theme.textPrimary
                                font.family: Theme.fontFamily
                                font.pixelSize: Theme.fontSmall
                            }
                        }
                    }
                }

                MouseArea {
                    anchors.fill: parent
                    onClicked: {
                        list.currentIndex = slide.index
                        list.forceActiveFocus()
                        root.activated(slide.modelData)
                    }
                }
            }
        }

        // Номер слайда — точками
        Row {
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            anchors.rightMargin: Theme.px(44)
            anchors.bottomMargin: Theme.px(52)
            spacing: Theme.px(10)
            Repeater {
                model: root.shown.length
                delegate: Rectangle {
                    required property int index
                    readonly property bool selected: index === list.currentIndex
                    anchors.verticalCenter: parent.verticalCenter
                    width: selected ? Theme.px(30) : Theme.px(10)
                    height: Theme.px(10)
                    radius: height / 2
                    color: selected ? Theme.progressFill : Theme.progressTrack
                    Behavior on width { NumberAnimation { duration: Theme.animNormal; easing.type: Theme.easeOut } }
                }
            }
        }
    }

    // Сама листается, пока на ней нет фокуса
    Timer {
        interval: 7000
        repeat: true
        running: root.visible && !root.activeFocus && root.shown.length > 1
        onTriggered: list.currentIndex = (list.currentIndex + 1) % root.shown.length
    }
}
