pragma ComponentBehavior: Bound
import QtQuick

// Полка витрины: заголовок («Скидки», «Новинки»…) и ряд карточек игр.
// ←/→ — по карточкам, ✕ — открыть игру. Пока подборка грузится — заглушки.
FocusScope {
    id: root

    property string title
    property var items: []
    property bool loading: false
    property bool showStore: false
    property real tileWidth: Theme.px(380)
    readonly property var current: shown.length > 0 ? shown[list.currentIndex] || null : null

    signal activated(var item)

    // Показанный список меняем, только если данные правда изменились:
    // иначе каждое обновление витрины сбрасывало бы выбранную карточку
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
        list.positionViewAtIndex(list.currentIndex, ListView.Contain)
    }
    onItemsChanged: update()
    Component.onCompleted: update()

    implicitHeight: heading.height + Theme.px(18) + list.height

    Text {
        id: heading
        x: Theme.px(22)
        text: root.title
        color: root.activeFocus ? Theme.textPrimary : Theme.textSecondary
        font.family: Theme.fontFamily
        font.pixelSize: Theme.fontBody
        font.weight: Font.Medium
        Behavior on color { ColorAnimation { duration: Theme.animFast } }
    }

    ListView {
        id: list
        anchors.top: heading.bottom
        anchors.topMargin: Theme.px(6)
        // Место сверху и снизу под ореол и увеличение выбранной карточки
        anchors.left: parent.left
        anchors.right: parent.right
        height: Math.round(root.tileWidth * 9 / 16) + Theme.px(128)
        focus: true
        orientation: ListView.Horizontal
        spacing: Theme.px(28)
        clip: true
        leftMargin: Theme.px(22)
        rightMargin: Theme.px(22)
        topMargin: Theme.px(30)
        keyNavigationEnabled: true
        highlightMoveDuration: Theme.animNormal
        highlightRangeMode: ListView.ApplyRange
        preferredHighlightBegin: Theme.px(22)
        preferredHighlightEnd: Math.max(preferredHighlightBegin + root.tileWidth, width * 0.55)
        boundsBehavior: Flickable.StopAtBounds
        model: root.shown.length > 0 ? root.shown : (root.loading ? 6 : 0)

        Keys.onReturnPressed: if (root.current) root.activated(root.current)
        Keys.onEnterPressed: if (root.current) root.activated(root.current)

        delegate: StoreTile {
            id: tile
            required property int index
            required property var modelData
            width: root.tileWidth
            item: typeof modelData === "object" ? modelData : null
            showStore: root.showStore
            active: ListView.isCurrentItem && list.activeFocus

            MouseArea {
                anchors.fill: parent
                onClicked: {
                    list.currentIndex = tile.index
                    list.forceActiveFocus()
                    if (tile.item)
                        root.activated(tile.item)
                }
            }
        }
    }
}
