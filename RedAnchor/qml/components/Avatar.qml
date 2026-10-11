import QtQuick

// Аватар пользователя: круг с первой буквой никнейма. Цвет зависит от никнейма,
// так что у каждого пользователя он свой и не меняется между запусками.
// Оттенки приглушённые — аватар не спорит с монохромным фоном.
Item {
    id: root

    property string name
    property real size: Theme.px(56)
    property bool highlighted: false

    readonly property var tints: ["#4a72d6", "#7660d0", "#2a90a8", "#c45a82", "#cf8740", "#3c9c70", "#a463cc", "#5a7bd0"]
    readonly property color tint: {
        let h = 0
        for (let i = 0; i < name.length; ++i)
            h = (h * 31 + name.charCodeAt(i)) >>> 0
        return tints[h % tints.length]
    }

    implicitWidth: size
    implicitHeight: size

    // Ореол выбранного аватара: кольцо и мягкое свечение
    FocusRing {
        shown: root.highlighted
        radius: root.size / 2
        gap: Math.max(Theme.px(3), root.size * 0.04)
        ringWidth: Math.max(2, root.size * 0.025)
        glow: root.size > Theme.px(80)
    }

    Rectangle {
        anchors.fill: parent
        radius: width / 2
        antialiasing: true
        gradient: Gradient {
            GradientStop { position: 0.0; color: Qt.lighter(root.tint, 1.22) }
            GradientStop { position: 1.0; color: Qt.darker(root.tint, 1.8) }
        }

        // Блик сверху и тонкая кромка — «стеклянный» объём
        Rectangle {
            anchors.fill: parent
            radius: width / 2
            color: "transparent"
            border.width: 1
            border.color: Qt.rgba(1, 1, 1, 0.22)
            gradient: Gradient {
                GradientStop { position: 0.0; color: Qt.rgba(1, 1, 1, 0.18) }
                GradientStop { position: 0.5; color: "transparent" }
            }
        }
    }

    Text {
        anchors.centerIn: parent
        text: root.name.length > 0 ? root.name.charAt(0).toUpperCase() : "?"
        color: "#ffffff"
        font.family: Theme.fontFamily
        font.pixelSize: root.size * 0.42
        font.weight: Font.DemiBold
    }
}
