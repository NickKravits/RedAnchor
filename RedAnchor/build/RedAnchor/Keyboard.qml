pragma Singleton
import QtQuick

// Мост между полями ввода и единственной экранной клавиатурой в Main.qml.
// Поле вызывает Keyboard.open(this), клавиатура редактирует target.text.
QtObject {
    id: root

    property ConsoleTextField target: null
    readonly property bool active: target !== null

    signal closed(ConsoleTextField field, bool accepted)

    function open(field: ConsoleTextField) {
        target = field
    }

    function close(accepted) {
        const field = target
        target = null
        closed(field, accepted === true)
    }
}
