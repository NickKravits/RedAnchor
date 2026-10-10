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

        Rectangle {
            width: parent.width
            height: Theme.px(76)
            radius: Theme.radius
            color: root.activeFocus || root.editing ? Theme.surfaceRaised : Theme.surface
            border.width: root.activeFocus || root.editing ? Theme.px(3) : Math.max(1, Theme.px(1.5))
            border.color: root.errorText !== "" ? Theme.accent
                        : (root.activeFocus || root.editing ? Theme.textPrimary : Theme.outline)

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
                    height: Theme.px(30)
                    color: Theme.accent
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
            color: Theme.accentSoft
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
