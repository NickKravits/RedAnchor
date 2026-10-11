import QtQuick

// Поле ввода для консоли: само по себе не редактируется, по ✕/Enter
// открывает экранную клавиатуру (Keyboard singleton).
FocusScope {
    id: root

    property string label
    property string text
    property string placeholder
    property bool password: false
    // "text" — русская раскладка по умолчанию, "email"/"password" — латиница
    property string inputHint: password ? "password" : "text"
    property string errorText
    readonly property bool editing: Keyboard.target === root

    signal accepted()

    implicitWidth: Theme.px(760)
    implicitHeight: column.implicitHeight

    Keys.onReturnPressed: Keyboard.open(root)
    Keys.onEnterPressed: Keyboard.open(root)

    Connections {
        target: Keyboard
        function onClosed(field, accepted) {
            if (field !== root)
                return
            root.forceActiveFocus()
            if (accepted)
                root.accepted()
        }
    }

    Column {
        id: column
        width: parent.width
        spacing: Theme.px(10)

        Text {
            visible: root.label !== ""
            text: root.label
            color: Theme.textSecondary
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontSmall
        }

        Item {
            width: parent.width
            height: Theme.px(72)

            // Ореол фокуса
            FocusRing {
                shown: root.activeFocus || root.editing
                radius: Theme.radius
                gap: 0
                ringWidth: 0
            }

            Rectangle {
                id: box
                anchors.fill: parent
                radius: Theme.radius
                color: root.editing ? Theme.surfaceFocus : root.activeFocus ? Theme.surfaceRaised : Theme.glass
                border.width: root.activeFocus || root.editing || root.errorText !== "" ? Theme.focusRingWidth - 1 : 1
                border.color: root.errorText !== "" ? Theme.accent
                            : (root.activeFocus || root.editing ? Theme.focus : Theme.glassBorder)
                antialiasing: true
                Behavior on border.color { ColorAnimation { duration: Theme.animFast } }
                Behavior on color { ColorAnimation { duration: Theme.animFast } }
            }

            Row {
                anchors.left: parent.left
                anchors.leftMargin: Theme.px(26)
                anchors.verticalCenter: parent.verticalCenter
                spacing: Theme.px(2)

                Text {
                    anchors.verticalCenter: parent.verticalCenter
                    text: root.text === "" ? root.placeholder
                                           : (root.password ? "•".repeat(root.text.length) : root.text)
                    color: root.text === "" ? Theme.textMuted : Theme.textPrimary
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontBody
                    elide: Text.ElideLeft
                    width: Math.min(implicitWidth, root.width - Theme.px(80))
                }
                Rectangle {
                    // каретка во время ввода
                    anchors.verticalCenter: parent.verticalCenter
                    width: Theme.px(2)
                    height: Theme.px(28)
                    color: Theme.textPrimary
                    visible: root.editing
                    SequentialAnimation on opacity {
                        running: root.editing
                        loops: Animation.Infinite
                        NumberAnimation { to: 0; duration: 450 }
                        NumberAnimation { to: 1; duration: 450 }
                    }
                }
            }
        }

        Text {
            visible: root.errorText !== ""
            text: root.errorText
            color: Theme.accent
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontSmall
        }
    }

    MouseArea {
        anchors.fill: parent
        onClicked: {
            root.forceActiveFocus()
            Keyboard.open(root)
        }
    }
}
