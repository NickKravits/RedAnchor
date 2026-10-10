pragma ComponentBehavior: Bound
import QtQuick

// Временная заглушка главного экрана оболочки.
SetupPage {
    id: page

    property string userName

    signal resetRequested()

    title: page.userName !== "" ? qsTr("Привет, %1").arg(page.userName) : qsTr("Главный экран")
    subtitle: qsTr("Здесь появятся библиотека игр, магазин и настройки.")
    hints: [{ button: "options", text: qsTr("Пройти настройку заново") }]

    Keys.onPressed: (event) => {
        if (event.key === Qt.Key_Menu || event.key === Qt.Key_F12) {
            page.resetRequested()
            event.accepted = true
        }
    }

    Row {
        spacing: Theme.px(28)
        Repeater {
            model: [qsTr("Игры"), qsTr("Медиа"), qsTr("Магазин"), qsTr("Настройки")]
            Rectangle {
                required property string modelData
                width: Theme.px(300)
                height: Theme.px(300)
                radius: Theme.radius
                color: Theme.surface
                border.color: Theme.outline
                border.width: Math.max(1, Theme.px(1.5))
                Text {
                    anchors.left: parent.left
                    anchors.bottom: parent.bottom
                    anchors.margins: Theme.px(24)
                    text: parent.modelData
                    color: Theme.textSecondary
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontLarge
                }
            }
        }
    }
}
