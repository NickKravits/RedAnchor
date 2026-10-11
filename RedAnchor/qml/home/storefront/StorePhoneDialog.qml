import QtQuick
import QtQuick.Controls.Basic
import "Format.js" as Format

// «Купить на телефоне»: QR-код страницы магазина. Пока окно открыто, купленные игры
// магазина периодически обновляются; как только игра появилась в библиотеке
// (или — при активации ключа — стало больше игр), окно сообщает об этом и закрывается.
//   dialog.show(store, url, item, returnFocusItem)   // item: null — активация ключа
Popup {
    id: root

    property string store
    property string url
    property var item: null
    property Item returnFocus: null
    property int ownedAtStart: 0
    property bool success: false

    signal finished(bool purchased)

    function show(store, url, item, returnFocus) {
        root.store = store
        root.url = url
        root.item = item || null
        root.returnFocus = returnFocus || null
        root.ownedAtStart = StoreCatalog.ownedCount(store)
        root.success = false
        root.open()
    }

    function check() {
        if (!root.opened || root.success)
            return
        const done = root.item ? StoreCatalog.ownership(root.item).owned === true
                               : StoreCatalog.ownedCount(root.store) > root.ownedAtStart
        if (done) {
            root.success = true
            closeTimer.restart()
        }
    }

    parent: Overlay.overlay
    anchors.centerIn: parent
    width: Theme.px(1080)
    padding: Theme.px(48)
    modal: true
    focus: true
    closePolicy: Popup.CloseOnEscape

    Overlay.modal: Rectangle { color: Theme.scrim }
    background: GlassPanel {
        radius: Theme.radiusLarge
        strong: true
    }
    enter: Transition {
        NumberAnimation { property: "opacity"; from: 0; to: 1; duration: Theme.animNormal }
        NumberAnimation { property: "scale"; from: 0.96; to: 1; duration: Theme.animNormal; easing.type: Easing.OutCubic }
    }
    exit: Transition {
        NumberAnimation { property: "opacity"; to: 0; duration: Theme.animFast }
    }

    onOpened: {
        doneButton.forceActiveFocus()
        StoreCatalog.refreshOwned(root.store)
    }
    onClosed: {
        if (root.returnFocus)
            root.returnFocus.forceActiveFocus()
        // Последняя проверка: покупку могли закончить перед самым закрытием
        StoreCatalog.refreshOwned(root.store)
        root.finished(root.success)
    }

    // Купленные игры магазина обновляются, пока открыт QR-код
    Timer {
        interval: 15000
        repeat: true
        running: root.opened && !root.success
        onTriggered: StoreCatalog.refreshOwned(root.store)
    }
    Timer {
        id: closeTimer
        interval: 2200
        onTriggered: root.close()
    }
    Connections {
        target: StoreCatalog
        function onOwnershipChanged() { root.check() }
    }

    contentItem: Row {
        spacing: Theme.px(48)

        // QR-код на белой подложке (иначе камера телефона не распознает его на тёмном фоне)
        Rectangle {
            id: qrFrame
            width: Theme.px(400)
            height: width
            radius: Theme.radiusTile
            color: "#ffffff"

            QrCode {
                anchors.fill: parent
                anchors.margins: Theme.px(18)
                text: root.url
                color: "#000000"
                background: "#ffffff"
                quietZone: 1
                opacity: root.success ? 0.08 : 1
                Behavior on opacity { NumberAnimation { duration: Theme.animNormal } }
            }
            // Готово — галочка поверх кода
            Rectangle {
                anchors.centerIn: parent
                width: Theme.px(140)
                height: width
                radius: width / 2
                color: Theme.success
                scale: root.success ? 1 : 0.4
                opacity: root.success ? 1 : 0
                Behavior on scale { NumberAnimation { duration: Theme.animNormal; easing.type: Easing.OutBack } }
                Behavior on opacity { NumberAnimation { duration: Theme.animFast } }
                Icon {
                    anchors.centerIn: parent
                    name: "check"
                    size: Theme.px(80)
                    lineWidth: 2.4
                    color: "#ffffff"
                }
            }
        }

        Column {
            width: root.availableWidth - qrFrame.width - parent.spacing
            anchors.verticalCenter: parent.verticalCenter
            spacing: Theme.px(18)

            Text {
                width: parent.width
                text: root.success ? (root.item ? qsTr("Игра в библиотеке") : qsTr("Ключ активирован"))
                    : root.item ? qsTr("Купить на телефоне") : qsTr("Активировать ключ на телефоне")
                color: Theme.textPrimary
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontLarge
                font.weight: Font.Light
                wrapMode: Text.WordWrap
            }
            Text {
                width: parent.width
                visible: root.item !== null
                text: root.item ? root.item.title : ""
                color: Theme.textSecondary
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontBody
                elide: Text.ElideRight
            }
            Item { width: 1; height: Theme.px(4) }

            // Шаги: номер и текст
            Repeater {
                model: [
                    qsTr("Наведите камеру телефона на QR-код."),
                    root.item ? qsTr("Оплатите игру на сайте %1, войдя в свой аккаунт.").arg(Format.storeName(root.store))
                              : qsTr("Войдите в аккаунт %1 и введите ключ.").arg(Format.storeName(root.store)),
                    StoreCatalog.signedIn(root.store)
                        ? qsTr("Игра сама появится в библиотеке консоли — её можно будет сразу установить.")
                        : qsTr("Войдите в %1 на консоли (Магазин → %1), чтобы игра появилась в библиотеке.").arg(Format.storeName(root.store))
                ]
                delegate: Row {
                    id: step
                    required property string modelData
                    required property int index
                    spacing: Theme.px(18)
                    Item {
                        width: Theme.px(36)
                        height: stepText.implicitHeight > 0 ? Theme.px(30) : 0
                        Text {
                            visible: !root.success
                            text: "0" + (step.index + 1)
                            color: Theme.textMuted
                            font.family: Theme.fontFamily
                            font.pixelSize: Theme.fontSmall
                            font.weight: Font.Medium
                            font.features: { "tnum": 1 }
                        }
                        Icon {
                            visible: root.success
                            name: "check"
                            size: Theme.px(24)
                            lineWidth: 1.6
                            color: Theme.success
                        }
                    }
                    Text {
                        id: stepText
                        width: (step.parent ? step.parent.width : 0) - Theme.px(54)
                        text: step.modelData
                        color: Theme.textPrimary
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontSmall + Theme.px(2)
                        lineHeight: 1.15
                        wrapMode: Text.WordWrap
                    }
                }
            }

            Row {
                visible: StoreCatalog.signedIn(root.store) && !root.success
                spacing: Theme.px(12)
                Spinner {
                    size: Theme.px(24)
                    anchors.verticalCenter: parent.verticalCenter
                }
                Text {
                    anchors.verticalCenter: parent.verticalCenter
                    text: root.item ? qsTr("Ждём покупку…") : qsTr("Ждём активацию…")
                    color: Theme.textSecondary
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontSmall
                }
            }
            Item { width: 1; height: Theme.px(4) }
            ConsoleButton {
                id: doneButton
                text: qsTr("Готово")
                icon: "check"
                primary: true
                onClicked: root.close()
            }
        }
    }
}
