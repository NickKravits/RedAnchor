import QtQuick

// Строка списка (сети, языки, разделы настроек…). Справа можно положить произвольные элементы.
// Стеклянная плашка; в фокусе — светлее, с белой кромкой и мягким ореолом.
// Кромка рисуется внутри строки: списки часто обрезаны (clip), и внешнее кольцо срезалось бы.
FocusScope {
    id: root

    property string title
    property string subtitle
    property string icon: ""
    property bool selected: false        // текущий выбор (галочка)
    property bool current: activeFocus    // подсветка навигации
    default property alias trailing: trailingRow.data

    signal clicked()

    implicitHeight: Theme.rowHeight
    implicitWidth: Theme.px(760)

    Keys.onReturnPressed: clicked()
    Keys.onEnterPressed: clicked()

    // Ореол фокуса снаружи (если список обрезан — пропадёт только он, не кромка)
    FocusRing {
        shown: root.current
        radius: Theme.radius
        gap: 0
        ringWidth: 0     // только ореол: кромка — у самой строки
    }

    Rectangle {
        anchors.fill: parent
        radius: Theme.radius
        // Выделение должно читаться и на светлых участках живого фона
        color: root.current ? Theme.surfaceFocus : Theme.glass
        border.width: root.current ? Theme.focusRingWidth : 1
        border.color: root.current ? Theme.focus : Theme.glassBorder
        antialiasing: true
        Behavior on color { ColorAnimation { duration: Theme.animFast } }
        Behavior on border.color { ColorAnimation { duration: Theme.animFast } }

        // Блик по верхнему краю
        Rectangle {
            anchors.fill: parent
            anchors.margins: parent.border.width
            radius: Math.max(0, Theme.radius - parent.border.width)
            gradient: Gradient {
                GradientStop { position: 0.0; color: Theme.glassHighlight }
                GradientStop { position: 0.55; color: "transparent" }
            }
        }
    }

    Row {
        anchors.left: parent.left
        anchors.leftMargin: root.icon !== "" ? Theme.px(18) : Theme.px(28)
        anchors.right: trailingRow.left
        anchors.rightMargin: Theme.px(16)
        anchors.verticalCenter: parent.verticalCenter
        spacing: Theme.px(18)

        // Значок в скруглённой плашке
        Rectangle {
            visible: root.icon !== ""
            anchors.verticalCenter: parent.verticalCenter
            width: Math.min(Theme.px(48), root.height - Theme.px(24))
            height: width
            radius: Theme.radiusSmall + Theme.px(2)
            color: root.current ? Qt.rgba(1, 1, 1, 0.16) : Qt.rgba(1, 1, 1, 0.06)
            border.width: 1
            border.color: Qt.rgba(1, 1, 1, root.current ? 0.20 : 0.08)
            Behavior on color { ColorAnimation { duration: Theme.animFast } }

            Icon {
                anchors.centerIn: parent
                name: root.icon
                size: parent.width * 0.58
                lineWidth: 1.6
                color: root.current ? Theme.textPrimary : Theme.textSecondary
            }
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
                color: root.current ? Qt.rgba(0.85, 0.87, 0.9, 1) : Theme.textSecondary
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
            lineWidth: 2
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
