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
    width: Theme.px(640)
    padding: Theme.px(44)
    modal: true
    focus: true
    closePolicy: Popup.CloseOnEscape

    Overlay.modal: Rectangle { color: Qt.rgba(0.02, 0.022, 0.026, 0.72) }

    background: Rectangle {
        radius: Theme.radius * 1.5
        color: "#1b1e24"
        border.width: 1
        border.color: Theme.outline
    }

    enter: Transition {
        NumberAnimation { property: "opacity"; from: 0; to: 1; duration: Theme.animNormal }
        NumberAnimation { property: "scale"; from: 0.96; to: 1; duration: Theme.animNormal; easing.type: Easing.OutCubic }
    }
    exit: Transition {
        NumberAnimation { property: "opacity"; to: 0; duration: Theme.animFast }
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
            font.weight: Font.Light
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
        Item { width: 1; height: Theme.px(10) }

        Column {
            width: parent.width
            spacing: Theme.px(14)

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
