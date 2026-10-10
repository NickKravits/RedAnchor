import QtQuick

// Строка списка (сети, языки…). Справа можно положить произвольные элементы.
FocusScope {
    id: root

    property string title
    property string subtitle
    property string icon: ""
    property bool selected: false        // текущий выбор (галочка)
    property bool current: activeFocus    // подсветка навигации
    default property alias trailing: trailingRow.data

    signal clicked()

    implicitHeight: Theme.px(84)
    implicitWidth: Theme.px(760)

    Keys.onReturnPressed: clicked()
    Keys.onEnterPressed: clicked()

    Rectangle {
        anchors.fill: parent
        radius: Theme.radius
        // Выделение должно читаться и на светлых участках живого фона
        color: root.current ? Qt.rgba(1, 1, 1, 0.16) : Qt.rgba(1, 1, 1, 0.035)
        border.width: root.current ? Theme.px(2) : 1
        border.color: root.current ? Theme.focus : Theme.outline
        Behavior on color { ColorAnimation { duration: Theme.animFast } }
        Behavior on border.color { ColorAnimation { duration: Theme.animFast } }
    }

    Row {
        anchors.left: parent.left
        anchors.leftMargin: Theme.px(28)
        anchors.verticalCenter: parent.verticalCenter
        spacing: Theme.px(20)

        Icon {
            visible: root.icon !== ""
            name: root.icon
            anchors.verticalCenter: parent.verticalCenter
            size: Theme.px(28)
            lineWidth: 1.5
            color: root.current ? Theme.textPrimary : Theme.textSecondary
        }

        Column {
            anchors.verticalCenter: parent.verticalCenter
            spacing: Theme.px(4)
            Text {
                text: root.title
                color: Theme.textPrimary
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontBody
                font.weight: Font.Medium
            }
            Text {
                visible: text !== ""
                text: root.subtitle
                color: Theme.textSecondary
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontSmall
            }
        }
    }

    Row {
        id: trailingRow
        anchors.right: parent.right
        anchors.rightMargin: Theme.px(28)
        anchors.verticalCenter: parent.verticalCenter
        spacing: Theme.px(18)

        Icon {
            visible: root.selected
            name: "check"
            size: Theme.px(24)
            color: Theme.textPrimary
            anchors.verticalCenter: parent.verticalCenter
        }
    }

    MouseArea {
        anchors.fill: parent
        onClicked: {
            root.forceActiveFocus()
            root.clicked()
        }
    }
}
