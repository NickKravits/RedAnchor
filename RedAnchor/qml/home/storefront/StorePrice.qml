import QtQuick
import "Storefront.js" as Storefront

// Цена игры витрины: скидка «−40%», прежняя цена зачёркнута, новая — крупно.
// Купленная игра вместо цены отмечается «В библиотеке» / «Установлена».
Row {
    id: root

    property var item: null
    property var own: ({})
    property int pixelSize: Theme.fontSmall

    readonly property int discount: Storefront.discount(item)
    readonly property bool owned: own && own.owned === true

    spacing: Math.round(pixelSize * 0.5)

    // Куплено
    Icon {
        visible: root.owned
        anchors.verticalCenter: parent.verticalCenter
        name: "check"
        size: root.pixelSize * 1.15
        lineWidth: 2
        color: Theme.success
    }
    Text {
        visible: root.owned
        anchors.verticalCenter: parent.verticalCenter
        text: root.own.installed ? qsTr("Установлена") : qsTr("В библиотеке")
        color: Theme.success
        font.family: Theme.fontFamily
        font.pixelSize: root.pixelSize
        font.weight: Font.Medium
    }

    // Скидка
    Rectangle {
        visible: !root.owned && root.discount > 0
        anchors.verticalCenter: parent.verticalCenter
        width: discountLabel.implicitWidth + root.pixelSize * 0.7
        height: root.pixelSize * 1.45
        radius: Theme.radiusSmall
        color: Theme.accent
        Text {
            id: discountLabel
            anchors.centerIn: parent
            text: "−" + root.discount + "%"
            color: Theme.textPrimary
            font.family: Theme.fontFamily
            font.pixelSize: root.pixelSize * 0.85
            font.weight: Font.DemiBold
            font.features: { "tnum": 1 }
        }
    }
    Text {
        visible: !root.owned && root.discount > 0 && root.item && root.item.price && root.item.price.base !== ""
        anchors.verticalCenter: parent.verticalCenter
        text: root.item && root.item.price ? root.item.price.base : ""
        color: Theme.textMuted
        font.family: Theme.fontFamily
        font.pixelSize: root.pixelSize * 0.9
        font.strikeout: true
    }
    // Раздача Epic: прежняя цена зачёркнута
    Text {
        visible: !root.owned && root.item !== null && root.item.promo === "now" && root.item.price && root.item.price.base !== ""
        anchors.verticalCenter: parent.verticalCenter
        text: root.item && root.item.price ? root.item.price.base : ""
        color: Theme.textMuted
        font.family: Theme.fontFamily
        font.pixelSize: root.pixelSize * 0.9
        font.strikeout: true
    }
    Text {
        visible: !root.owned && text !== ""
        anchors.verticalCenter: parent.verticalCenter
        text: Storefront.price(root.item)
        color: Theme.textPrimary
        font.family: Theme.fontFamily
        font.pixelSize: root.pixelSize
        font.weight: Font.Medium
        font.features: { "tnum": 1 }
    }
}
