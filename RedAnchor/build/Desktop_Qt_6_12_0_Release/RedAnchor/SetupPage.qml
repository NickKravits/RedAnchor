import QtQuick
import QtQuick.Controls.Basic

// Базовая страница мастера настройки: заголовок, подзаголовок, контент.
// Подсказки (hints) показывает общий HintBar в Main.qml.
FocusScope {
    id: root

    property string title
    property string subtitle
    property var hints: []
    property int step: -1               // индекс в StepIndicator, -1 — не показывать
    property Item initialFocus: null
    default property alias content: contentArea.data

    signal next()
    signal back()

    StackView.onActivated: (initialFocus ? initialFocus : root).forceActiveFocus()

    Column {
        id: header
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.leftMargin: Theme.gutter
        anchors.rightMargin: Theme.gutter
        anchors.topMargin: Theme.px(170)
        spacing: Theme.px(14)

        Text {
            width: parent.width
            text: root.title
            visible: text !== ""
            color: Theme.textPrimary
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontTitle
            font.weight: Font.Bold
            wrapMode: Text.WordWrap
        }
        Text {
            width: Math.min(parent.width, Theme.px(1000))
            text: root.subtitle
            visible: text !== ""
            color: Theme.textSecondary
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontBody
            lineHeight: 1.2
            wrapMode: Text.WordWrap
        }
    }

    Item {
        id: contentArea
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: header.bottom
        anchors.bottom: parent.bottom
        anchors.leftMargin: Theme.gutter
        anchors.rightMargin: Theme.gutter
        anchors.topMargin: Theme.px(48)
        anchors.bottomMargin: Theme.px(130)
    }
}
