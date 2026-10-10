import QtQuick

// Живой фон: светлая стальная гамма, объёмная дымка, лучи света сверху,
// свечение горизонта и едва заметная перспективная сетка. Рисуется фрагментным
// шейдером shaders/aurora.frag — формы строятся из шума и медленно перетекают.
// Поверх — мягкое затемнение переднего плана (dimmed), чтобы белый текст читался.
// Шейдер считается в пониженном разрешении (размытому фону это не вредит)
// и растягивается со сглаживанием, так что фон дешёвый даже для встроенной графики.
Item {
    id: root

    // Остановить анимацию (например, когда поверх запущена игра)
    property bool animated: true
    // Доля разрешения, в которой считается шейдер
    property real quality: 0.5
    // Общая яркость эффектов
    property real intensity: 1.0
    // Затемнение переднего плана: темнее к низу и к краям
    property bool dimmed: true

    // Запасной фон, если шейдер не загрузился
    Rectangle {
        anchors.fill: parent
        gradient: Gradient {
            GradientStop { position: 0.0; color: "#373d46" }
            GradientStop { position: 0.6; color: "#262a31" }
            GradientStop { position: 1.0; color: "#121417" }
        }
    }

    Item {
        anchors.fill: parent
        layer.enabled: true
        layer.smooth: true
        layer.textureSize: Qt.size(Math.max(1, Math.ceil(width * root.quality)),
                                   Math.max(1, Math.ceil(height * root.quality)))

        ShaderEffect {
            id: effect
            anchors.fill: parent

            property real time: 0
            property real intensity: root.intensity
            property size resolution: Qt.size(width * root.quality, height * root.quality)

            fragmentShader: "qrc:/qt/qml/RedAnchor/shaders/aurora.frag.qsb"

            onStatusChanged: if (status === ShaderEffect.Error) console.warn("AppBackground:", log)

            // Время в секундах. Цикл — 6 часов: при перезапуске картинка один раз
            // сменится скачком, чаще это не происходит.
            NumberAnimation on time {
                running: root.animated
                from: 0
                to: 6 * 3600
                duration: 6 * 3600 * 1000
                loops: Animation.Infinite
            }
        }
    }

    // ---------- Затемнение переднего плана ----------
    Item {
        anchors.fill: parent
        opacity: root.dimmed ? 1 : 0
        visible: opacity > 0
        Behavior on opacity { NumberAnimation { duration: Theme.animSlow; easing.type: Easing.InOutQuad } }

        // Общий тон: слабее сверху, заметно сильнее к низу
        Rectangle {
            anchors.fill: parent
            gradient: Gradient {
                GradientStop { position: 0.0; color: Qt.rgba(0, 0, 0, 0.22) }
                GradientStop { position: 0.55; color: Qt.rgba(0, 0, 0, 0.30) }
                GradientStop { position: 1.0; color: Qt.rgba(0, 0, 0, 0.50) }
            }
        }
        // Боковые края
        Rectangle {
            anchors.fill: parent
            gradient: Gradient {
                orientation: Gradient.Horizontal
                GradientStop { position: 0.0; color: Qt.rgba(0, 0, 0, 0.22) }
                GradientStop { position: 0.2; color: Qt.rgba(0, 0, 0, 0.0) }
                GradientStop { position: 0.8; color: Qt.rgba(0, 0, 0, 0.0) }
                GradientStop { position: 1.0; color: Qt.rgba(0, 0, 0, 0.22) }
            }
        }
    }
}
