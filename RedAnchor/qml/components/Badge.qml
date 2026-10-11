import QtQuick

// Небольшая «пилюля» поверх обложек и в строках: значок и/или короткая подпись.
//   Badge { icon: "pin" }   Badge { text: "GOG" }   Badge { text: "62%"; tone: "light" }
// tone: "dark" — тёмное стекло (поверх картинок), "light" — белая (важное состояние),
//       "warning" — предупреждение.
Rectangle {
    id: root

    property string text
    property string icon: ""
    property string tone: "dark"
    property real size: Theme.px(30)

    readonly property color foreground: tone === "light" ? Theme.textOnFocus
                                      : tone === "warning" ? Theme.warning : Theme.textPrimary

    implicitHeight: size
    implicitWidth: text === "" ? size : row.implicitWidth + size * 0.75
    width: implicitWidth
    height: implicitHeight
    radius: height / 2
    color: tone === "light" ? Theme.focus
         : tone === "warning" ? Qt.rgba(0.16, 0.12, 0.05, 0.82) : Qt.rgba(0.05, 0.055, 0.065, 0.72)
    border.width: tone === "light" ? 0 : 1
    border.color: tone === "warning" ? Qt.rgba(0.94, 0.71, 0.3, 0.45) : Qt.rgba(1, 1, 1, 0.14)
    antialiasing: true

    Row {
        id: row
        anchors.centerIn: parent
        spacing: Theme.px(6)

        Icon {
            visible: root.icon !== ""
            name: root.icon
            size: root.size * 0.58
            lineWidth: 1.8
            color: root.foreground
            anchors.verticalCenter: parent.verticalCenter
        }
        Text {
            visible: root.text !== ""
            anchors.verticalCenter: parent.verticalCenter
            text: root.text
            color: root.foreground
            font.family: Theme.fontFamily
            font.pixelSize: Math.round(root.size * 0.47)
            font.weight: Font.DemiBold
            font.letterSpacing: Theme.scale * 0.3
        }
    }
}
