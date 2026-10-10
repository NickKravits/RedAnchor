import QtQuick

// Первый экран при запуске: пульсирующий логотип, затем приветствие — всё по центру.
SetupPage {
    id: page

    property bool finished: false

    hints: [{ button: "cross", text: qsTr("Начать") }]

    function finish() {
        if (finished)
            return
        finished = true
        page.next()
    }

    Keys.onPressed: (event) => {
        // Любая кнопка геймпада или клавиша — начать
        event.accepted = true
        if (introAnim.running)
            return
        page.finish()
    }

    Timer {
        interval: 7500
        running: true
        onTriggered: page.finish()
    }

    SequentialAnimation {
        id: introAnim
        running: true
        PauseAnimation { duration: 300 }
        ParallelAnimation {
            NumberAnimation { target: logo; property: "opacity"; to: 1; duration: 900; easing.type: Easing.OutCubic }
            NumberAnimation { target: logo; property: "appear"; from: 0.85; to: 1; duration: 1100; easing.type: Easing.OutCubic }
        }
        PauseAnimation { duration: 700 }
        ParallelAnimation {
            NumberAnimation { target: greeting; property: "opacity"; to: 1; duration: 900; easing.type: Easing.OutCubic }
            NumberAnimation { target: greetingShift; property: "y"; from: Theme.px(24); to: 0; duration: 900; easing.type: Easing.OutCubic }
        }
        ParallelAnimation {
            NumberAnimation { target: tagline; property: "opacity"; to: 1; duration: 800; easing.type: Easing.OutCubic }
            NumberAnimation { target: taglineShift; property: "y"; from: Theme.px(16); to: 0; duration: 800; easing.type: Easing.OutCubic }
        }
    }

    // Содержимое живёт поверх всей страницы, а не в области контента SetupPage
    Item {
        parent: page
        anchors.fill: parent

        Column {
            anchors.centerIn: parent
            anchors.verticalCenterOffset: -Theme.px(20)
            spacing: Theme.px(26)

            // ---------- Логотип ----------
            Item {
                id: logo

                property real appear: 0.85
                property real pulse

                anchors.horizontalCenter: parent.horizontalCenter
                width: Theme.px(150)
                height: width
                opacity: 0

                // Мягкое «дыхание» логотипа
                SequentialAnimation on pulse {
                    loops: Animation.Infinite
                    NumberAnimation { from: 1.0; to: 1.06; duration: 1100; easing.type: Easing.InOutSine }
                    NumberAnimation { to: 1.0; duration: 1100; easing.type: Easing.InOutSine }
                }

                // Расходящиеся кольца в такт пульсу
                Repeater {
                    model: 2
                    Rectangle {
                        id: ring
                        required property int index
                        anchors.centerIn: parent
                        width: parent.width * 1.05
                        height: width
                        radius: width / 2
                        color: "transparent"
                        border.width: Theme.px(1.5)
                        border.color: Theme.accent
                        opacity: 0
                        SequentialAnimation {
                            running: true
                            loops: Animation.Infinite
                            PauseAnimation { duration: ring.index * 1100 }
                            ParallelAnimation {
                                NumberAnimation { target: ring; property: "scale"; from: 0.8; to: 1.6; duration: 2200; easing.type: Easing.OutCubic }
                                NumberAnimation { target: ring; property: "opacity"; from: 0.55; to: 0; duration: 2200; easing.type: Easing.OutCubic }
                            }
                            PauseAnimation { duration: (1 - ring.index) * 1100 }
                        }
                    }
                }

                Icon {
                    anchors.centerIn: parent
                    name: "anchor"
                    size: parent.width * 0.62
                    lineWidth: 1.1
                    color: Theme.textPrimary
                    scale: logo.appear * logo.pulse
                }
            }

            Item { width: 1; height: Theme.px(14) }

            // ---------- Приветствие ----------
            Text {
                id: greeting
                anchors.horizontalCenter: parent.horizontalCenter
                opacity: 0
                transform: Translate { id: greetingShift }
                text: qsTr("Добро пожаловать в RedAnchor!")
                color: Theme.textPrimary
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontHuge
                font.weight: Font.Light
                font.letterSpacing: Theme.px(-0.5)
            }

            Text {
                id: tagline
                anchors.horizontalCenter: parent.horizontalCenter
                opacity: 0
                transform: Translate { id: taglineShift }
                text: qsTr("Давайте всё настроим для комфортной игры.")
                color: Theme.textSecondary
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontLarge
                font.weight: Font.Light
            }
        }
    }
}
