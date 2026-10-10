import QtQuick

// Аватар пользователя: круг с первой буквой никнейма. Цвет зависит от никнейма,
// так что у каждого пользователя он свой и не меняется между запусками.
Item {
    id: root

    property string name
    property real size: Theme.px(56)
    property bool highlighted: false

    readonly property var tints: ["#2f6bff", "#7b4dff", "#00a3c4", "#e0457b", "#f08c2e", "#2fbf71", "#c94dff", "#4d7cff"]
    readonly property color tint: {
        let h = 0
        for (let i = 0; i < name.length; ++i)
            h = (h * 31 + name.charCodeAt(i)) >>> 0
        return tints[h % tints.length]
    }

    implicitWidth: size
    implicitHeight: size

    // Ореол выбранного аватара
    Rectangle {
        anchors.centerIn: parent
        width: parent.width + Theme.px(14)
        height: width
        radius: width / 2
        color: "transparent"
        border.width: Math.max(2, root.size * 0.03)
        border.color: Theme.focus
        opacity: root.highlighted ? 1 : 0
        Behavior on opacity { NumberAnimation { duration: Theme.animFast } }
    }

    Rectangle {
        anchors.fill: parent
        radius: width / 2
        gradient: Gradient {
            GradientStop { position: 0.0; color: Qt.lighter(root.tint, 1.25) }
            GradientStop { position: 1.0; color: Qt.darker(root.tint, 1.7) }
        }
    }

    Text {
        anchors.centerIn: parent
        text: root.name.length > 0 ? root.name.charAt(0).toUpperCase() : "?"
        color: "#ffffff"
        font.family: Theme.fontFamily
        font.pixelSize: root.size * 0.44
        font.weight: Font.Medium
    }
}
