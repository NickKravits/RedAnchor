import QtQuick

// Финал мастера: всё готово, переход на главный экран.
SetupPage {
    id: page

    property string userName
    property bool finished: false

    hints: [{ button: "cross", text: qsTr("Продолжить") }]

    function finish() {
        if (finished)
            return
        finished = true
        page.next()
    }

    Keys.onReturnPressed: page.finish()
    Keys.onEnterPressed: page.finish()

    Item {
        parent: page
        anchors.fill: parent

        Column {
            id: column
            anchors.left: parent.left
            anchors.leftMargin: Theme.gutter
            anchors.verticalCenter: parent.verticalCenter
            spacing: Theme.px(26)

            NumberAnimation on opacity { from: 0; to: 1; duration: 900; easing.type: Easing.OutCubic }

            Text {
                text: qsTr("Всё готово")
                color: Theme.textSecondary
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontLarge
                font.weight: Font.Light
            }
            Text {
                text: page.userName !== "" ? qsTr("Привет, %1").arg(page.userName) : qsTr("Приятной игры")
                color: Theme.textPrimary
                font.family: Theme.fontFamily
                font.pixelSize: Theme.px(110)
                font.weight: Font.ExtraLight
                font.letterSpacing: Theme.px(-1)
            }
            Text {
                text: qsTr("Открываем главный экран…")
                color: Theme.textSecondary
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontBody
            }

            Rectangle {
                width: Theme.px(420)
                height: Math.max(2, Theme.px(4))
                radius: height / 2
                color: Theme.progressTrack
                Rectangle {
                    height: parent.height
                    radius: height / 2
                    color: Theme.progressFill
                    NumberAnimation on width {
                        from: 0; to: Theme.px(420); duration: 3500
                        easing.type: Easing.InOutQuad
                        onFinished: page.finish()
                    }
                }
            }
        }
    }
}
