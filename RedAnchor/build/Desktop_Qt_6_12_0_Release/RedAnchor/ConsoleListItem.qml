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

    implicitHeight: Theme.px(88)
    implicitWidth: Theme.px(760)

    Keys.onReturnPressed: clicked()
    Keys.onEnterPressed: clicked()

    Rectangle {
        anchors.fill: parent
        radius: Theme.radius
        color: root.current ? Theme.surfaceRaised : Qt.rgba(1, 1, 1, 0.035)
        border.width: root.current ? Theme.px(3) : 0
        border.color: Theme.textPrimary
        Behavior on color { ColorAnimation { duration: Theme.animFast } }
    }

    Rectangle {
        // красная метка слева у активного элемента
        anchors.left: parent.left
        anchors.verticalCenter: parent.verticalCenter
        anchors.leftMargin: Theme.px(10)
        width: Theme.px(4)
        height: parent.height * 0.5
        radius: width / 2
        color: Theme.accent
        opacity: root.current ? 1 : 0
        Behavior on opacity { NumberAnimation { duration: Theme.animFast } }
    }

    Row {
        anchors.left: parent.left
        anchors.leftMargin: Theme.px(32)
        anchors.verticalCenter: parent.verticalCenter
        spacing: Theme.px(20)

        Icon {
            visible: root.icon !== ""
            name: root.icon
            anchors.verticalCenter: parent.verticalCenter
            size: Theme.px(32)
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
                font.weight: Font.DemiBold
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
        anchors.rightMargin: Theme.px(32)
        anchors.verticalCenter: parent.verticalCenter
        spacing: Theme.px(18)

        Icon {
            visible: root.selected
            name: "check"
            color: Theme.success
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
