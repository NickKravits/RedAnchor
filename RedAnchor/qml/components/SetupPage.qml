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
    property bool showProfile: false    // показать аватар и никнейм в верхней панели
    property bool immersive: false      // на весь экран: без верхней панели, однотонный фон
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
        anchors.topMargin: Theme.px(190)
        spacing: Theme.px(14)

        Text {
            width: parent.width
            text: root.title
            visible: text !== ""
            color: Theme.textPrimary
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontTitle
            font.weight: Font.Light
            font.letterSpacing: Theme.trackingTight
            wrapMode: Text.WordWrap
        }
        Text {
            width: Math.min(parent.width, Theme.px(1000))
            text: root.subtitle
            visible: text !== "" && text.indexOf("{") < 0
            color: Theme.textSecondary
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontBody
            lineHeight: 1.3
            wrapMode: Text.WordWrap
        }
        // Подзаголовок со значками кнопок: «Нажмите {cross}, чтобы продолжить»
        GlyphText {
            width: Math.min(parent.width, Theme.px(1000))
            visible: root.subtitle.indexOf("{") >= 0
            text: root.subtitle
            color: Theme.textSecondary
            pixelSize: Theme.fontBody
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
