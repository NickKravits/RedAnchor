import QtQuick

// Живой фон: «графитовый атриум» — светлая стальная гамма, мягкие поля света,
// слоистая дымка у горизонта, широкие объёмные лучи из высокого окна за кадром
// и редкие пылинки в них. Рисуется фрагментным шейдером shaders/aurora.frag —
// формы строятся из шума и очень медленно перетекают.
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
    // Плавный уровень затемнения, 0..1 (меняется вместе с dimmed)
    property real dimLevel: dimmed ? 1 : 0
    Behavior on dimLevel { NumberAnimation { duration: Theme.animSlow; easing.type: Easing.InOutQuad } }

    // Запасной фон, если шейдер не загрузился (или нет аппаратной отрисовки):
    // та же гамма — сталь сверху, графит снизу, светлая полоса у горизонта
    Rectangle {
        anchors.fill: parent
        gradient: Gradient {
            GradientStop { position: 0.0; color: "#40454d" }
            GradientStop { position: 0.5; color: "#30343b" }
            GradientStop { position: 0.66; color: "#383d45" }
            GradientStop { position: 0.8; color: "#24272c" }
            GradientStop { position: 1.0; color: "#16181b" }
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
            property real dim: root.dimLevel

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
    // Лёгкое: фон должен оставаться светлым, затемнение лишь помогает тексту.
    // С шейдером его считает сам шейдер (uniform dim) — без полос на градиентах;
    // без шейдера — простая полупрозрачная заливка поверх запасного фона.
    Rectangle {
        anchors.fill: parent
        // Программная отрисовка шейдеры не поддерживает — статус тогда не Compiled
        visible: effect.status !== ShaderEffect.Compiled
        color: "black"
        opacity: 0.22 * root.dimLevel
    }
}
