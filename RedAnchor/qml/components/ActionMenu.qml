pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls.Basic

// Всплывающее меню действий: заголовок, пояснение и столбец кнопок. ○ — закрыть.
//   menu.show(title, subtitle, [{ text, icon, run: function }], returnFocusItem)
// Действие выполняется после закрытия меню, когда фокус уже возвращён, —
// поэтому оно может само перевести фокус (например, открыть форму).
Popup {
    id: root

    property string title
    property string subtitle
    property var actions: []
    property Item returnFocus: null
    property var pending: null

    function show(title, subtitle, actions, returnFocus) {
        root.title = title
        root.subtitle = subtitle || ""
        root.actions = actions
        root.returnFocus = returnFocus || null
        root.pending = null
        root.open()
    }

    function choose(action) {
        root.pending = action.run || null
        root.close()
    }

    parent: Overlay.overlay
    anchors.centerIn: parent
    width: Math.min(Theme.px(660), parent ? parent.width - 2 * Theme.px(40) : Theme.px(660))
    padding: Theme.px(44)
    topPadding: Theme.px(40)
    modal: true
    focus: true
    closePolicy: Popup.CloseOnEscape

    // Затемнение под меню
    Overlay.modal: Rectangle { color: Theme.scrim }

    background: GlassPanel {
        strong: true
        radius: Theme.radiusLarge
        // Почти непрозрачное: под меню не должны читаться обложки
        color: Qt.rgba(0.072, 0.078, 0.092, 0.97)
    }

    enter: Transition {
        NumberAnimation { property: "opacity"; from: 0; to: 1; duration: Theme.animNormal; easing.type: Easing.OutCubic }
        NumberAnimation { property: "scale"; from: 0.94; to: 1; duration: Theme.animNormal; easing.type: Easing.OutQuint }
    }
    exit: Transition {
        NumberAnimation { property: "opacity"; to: 0; duration: Theme.animFast }
        NumberAnimation { property: "scale"; to: 0.97; duration: Theme.animFast }
    }

    onOpened: {
        const first = buttons.itemAt(0)
        if (first)
            first.forceActiveFocus()
    }
    onClosed: {
        if (root.returnFocus)
            root.returnFocus.forceActiveFocus()
        const action = root.pending
        root.pending = null
        if (action)
            action()
    }

    contentItem: Column {
        spacing: Theme.px(16)

        Text {
            width: parent.width
            text: root.title
            color: Theme.textPrimary
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontLarge
            font.weight: Font.Normal
            font.letterSpacing: Theme.trackingTight
            wrapMode: Text.WordWrap
        }
        Text {
            visible: text !== ""
            width: parent.width
            text: root.subtitle
            color: Theme.textSecondary
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontSmall
            lineHeight: 1.2
            wrapMode: Text.WordWrap
        }
        Item { width: 1; height: Theme.px(14) }

        Column {
            width: parent.width
            spacing: Theme.px(16)

            Repeater {
                id: buttons
                model: root.actions
                delegate: ConsoleButton {
                    required property var modelData
                    required property int index
                    width: parent.width
                    text: modelData.text
                    icon: modelData.icon || ""
                    primary: index === 0
                    Keys.onUpPressed: {
                        const prev = buttons.itemAt(index - 1)
                        if (prev)
                            prev.forceActiveFocus()
                    }
                    Keys.onDownPressed: {
                        const next = buttons.itemAt(index + 1)
                        if (next)
                            next.forceActiveFocus()
                    }
                    onClicked: root.choose(modelData)
                }
            }
        }
    }
}
