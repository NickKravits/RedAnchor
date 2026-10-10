import QtQuick

// Финал мастера: приветствие и переход на главный экран.
SetupPage {
    id: page

    property string userName

    title: ""
    hints: [{ button: "cross", text: qsTr("Продолжить") }]

    property bool finished: false
    function finish() {
        if (finished)
            return
        finished = true
        page.next()
    }

    Keys.onReturnPressed: page.finish()
    Keys.onEnterPressed: page.finish()

    Column {
        anchors.centerIn: parent
        anchors.verticalCenterOffset: -Theme.px(120)
        spacing: Theme.px(28)

        Item {
            anchors.horizontalCenter: parent.horizontalCenter
            width: Theme.px(150)
            height: width

            Rectangle {
                id: halo
                anchors.fill: parent
                radius: width / 2
                color: "transparent"
                border.width: Theme.px(4)
                border.color: Theme.accent
                SequentialAnimation on scale {
                    loops: Animation.Infinite
                    NumberAnimation { from: 1; to: 1.35; duration: 1600; easing.type: Easing.OutCubic }
                }
                SequentialAnimation on opacity {
                    loops: Animation.Infinite
                    NumberAnimation { from: 0.9; to: 0; duration: 1600; easing.type: Easing.OutCubic }
                }
            }
            Rectangle {
                anchors.fill: parent
                radius: width / 2
                color: Theme.accent
            }
            Icon {
                anchors.centerIn: parent
                name: "anchor"
                size: Theme.px(84)
                lineWidth: 1.6
            }
            NumberAnimation on scale { from: 0.4; to: 1; duration: 700; easing.type: Easing.OutBack }
        }

        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            text: page.userName !== "" ? qsTr("Добро пожаловать, %1!").arg(page.userName) : qsTr("Добро пожаловать!")
            color: Theme.textPrimary
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontHuge
            font.weight: Font.Bold
        }
        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            text: qsTr("RedAnchor настроен. Открываем главный экран…")
            color: Theme.textSecondary
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontBody
        }

        Rectangle {
            anchors.horizontalCenter: parent.horizontalCenter
            width: Theme.px(420)
            height: Theme.px(6)
            radius: height / 2
            color: Qt.rgba(1, 1, 1, 0.12)
            Rectangle {
                height: parent.height
                radius: parent.radius
                color: Theme.accent
                NumberAnimation on width {
                    from: 0; to: Theme.px(420); duration: 3500
                    easing.type: Easing.InOutQuad
                    onFinished: page.finish()
                }
            }
        }
    }
}
