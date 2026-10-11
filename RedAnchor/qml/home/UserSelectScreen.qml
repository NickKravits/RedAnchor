pragma ComponentBehavior: Bound
import QtQuick

// Выбор пользователя при запуске оболочки: все учётные записи консоли
// (или единственная), ←/→ — выбрать, ✕ — войти.
SetupPage {
    id: page

    signal chosen()

    hints: [{ button: "cross", text: qsTr("Войти") }]
    initialFocus: users

    function choose(index) {
        const account = AccountManager.accounts[index]
        if (account && AccountManager.selectAccount(account.id))
            page.chosen()
    }

    Item {
        parent: page
        anchors.fill: parent

        Column {
            anchors.centerIn: parent
            anchors.verticalCenterOffset: -Theme.px(20)
            spacing: Theme.px(80)

            NumberAnimation on opacity { from: 0; to: 1; duration: 900; easing.type: Easing.OutCubic }

            Column {
                anchors.horizontalCenter: parent.horizontalCenter
                spacing: Theme.px(14)
                Text {
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: qsTr("RedAnchor").toUpperCase()
                    color: Theme.textSecondary
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontCaption
                    font.weight: Font.DemiBold
                    font.letterSpacing: Theme.trackingCaps * 2.5
                }
                Text {
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: AccountManager.count > 1 ? qsTr("Кто будет играть?") : qsTr("С возвращением")
                    color: Theme.textPrimary
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontTitle
                    font.weight: Font.Light
                    font.letterSpacing: Theme.trackingTight
                }
            }

            ListView {
                id: users
                readonly property real cardWidth: Theme.px(240)
                anchors.horizontalCenter: parent.horizontalCenter
                width: Math.min(count * cardWidth + Math.max(0, count - 1) * spacing, page.width - 2 * Theme.gutter)
                height: Theme.px(360)
                orientation: ListView.Horizontal
                spacing: Theme.px(56)
                clip: true
                focus: true
                keyNavigationEnabled: true
                highlightMoveDuration: Theme.animNormal
                highlightRangeMode: ListView.ApplyRange
                preferredHighlightBegin: cardWidth
                preferredHighlightEnd: width - cardWidth
                model: AccountManager.accounts

                Component.onCompleted: {
                    // Подсвечиваем последнего, кто играл
                    for (let i = 0; i < AccountManager.accounts.length; ++i) {
                        if (AccountManager.accounts[i].id === AccountManager.lastAccountId)
                            currentIndex = i
                    }
                }

                Keys.onReturnPressed: page.choose(currentIndex)
                Keys.onEnterPressed: page.choose(currentIndex)

                delegate: Item {
                    id: card
                    required property var modelData
                    required property int index
                    readonly property bool active: ListView.isCurrentItem && users.activeFocus

                    width: users.cardWidth
                    height: users.height

                    Column {
                        anchors.horizontalCenter: parent.horizontalCenter
                        anchors.top: parent.top
                        anchors.topMargin: Theme.px(34)
                        spacing: Theme.px(28)

                        Avatar {
                            anchors.horizontalCenter: parent.horizontalCenter
                            name: card.modelData.nickname
                            size: Theme.px(180)
                            highlighted: card.active
                            scale: card.active ? 1.06 : 0.88
                            opacity: card.active ? 1 : 0.62
                            Behavior on scale { NumberAnimation { duration: Theme.animNormal; easing.type: Easing.OutQuint } }
                            Behavior on opacity { NumberAnimation { duration: Theme.animNormal } }
                        }
                        Text {
                            anchors.horizontalCenter: parent.horizontalCenter
                            width: users.cardWidth
                            horizontalAlignment: Text.AlignHCenter
                            elide: Text.ElideRight
                            text: card.modelData.nickname
                            color: card.active ? Theme.textPrimary : Theme.textSecondary
                            font.family: Theme.fontFamily
                            font.pixelSize: Theme.fontLarge
                            font.weight: card.active ? Font.DemiBold : Font.Medium
                        }
                        Text {
                            anchors.horizontalCenter: parent.horizontalCenter
                            text: card.modelData.local ? qsTr("Локальная учётная запись") : qsTr("Аккаунт RedAnchor")
                            color: Theme.textSecondary
                            opacity: card.active ? 1 : 0.7
                            font.family: Theme.fontFamily
                            font.pixelSize: Theme.fontSmall
                        }
                    }

                    MouseArea {
                        anchors.fill: parent
                        onClicked: {
                            users.currentIndex = card.index
                            page.choose(card.index)
                        }
                    }
                }
            }
        }
    }
}
