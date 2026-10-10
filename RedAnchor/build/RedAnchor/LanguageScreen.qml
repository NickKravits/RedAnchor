pragma ComponentBehavior: Bound
import QtQuick

// Шаг 3: выбор языка интерфейса (пока доступен только русский).
SetupPage {
    id: page

    property string language: "ru"

    step: 1
    title: qsTr("Выберите язык")
    subtitle: qsTr("Язык интерфейса можно будет изменить позже в настройках.")
    hints: [{ button: "cross", text: qsTr("Выбрать") }]
    initialFocus: languages

    ListView {
        id: languages
        width: Math.min(parent.width, Theme.px(820))
        height: parent.height
        spacing: Theme.px(14)
        focus: true
        keyNavigationEnabled: true
        model: [
            { code: "ru", title: "Русский", subtitle: "Russian" }
        ]
        delegate: ConsoleListItem {
            required property var modelData
            required property int index
            width: ListView.view.width
            icon: "globe"
            title: modelData.title
            subtitle: modelData.subtitle
            selected: page.language === modelData.code
            current: ListView.isCurrentItem && languages.activeFocus
            onClicked: languages.choose(index)
        }

        function choose(i) {
            page.language = model[i].code
            page.next()
        }
        Keys.onReturnPressed: choose(currentIndex)
        Keys.onEnterPressed: choose(currentIndex)
    }
}
