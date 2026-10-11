pragma Singleton
import QtQuick

// Общая палитра и метрики оболочки. Все размеры задаются для 1080p
// и умножаются на scale, который Main.qml выставляет по размеру окна.
// Стиль: графит и сталь, «стеклянные» панели с тонкой светлой кромкой,
// фокус — белым (заливка или кольцо с мягким ореолом). Акцентный красный —
// только точечно (ошибки, индикатор текущего шага).
QtObject {
    property real scale: 1.0
    function px(v) { return Math.round(v * scale) }

    // ---------- Палитра ----------
    readonly property color background: "#08090b"
    readonly property color backgroundAlt: "#0f1114"
    readonly property color surface: Qt.rgba(1, 1, 1, 0.05)
    readonly property color surfaceRaised: Qt.rgba(1, 1, 1, 0.09)
    readonly property color outline: Qt.rgba(1, 1, 1, 0.10)
    readonly property color outlineStrong: Qt.rgba(1, 1, 1, 0.24)
    readonly property color accent: "#e5383b"
    readonly property color focus: "#f4f5f7"
    readonly property color textPrimary: "#f4f5f7"
    readonly property color textSecondary: "#a9aeb8"
    readonly property color textMuted: "#6c717b"
    readonly property color success: "#5ad19a"
    readonly property color warning: "#f0b44c"

    // Текст и значки на светлой заливке фокуса
    readonly property color textOnFocus: "#0c0e11"
    // Поверхность в фокусе (строки списков, плитки) — светлее обычной
    readonly property color surfaceFocus: Qt.rgba(1, 1, 1, 0.15)
    // Мягкий ореол вокруг элемента в фокусе
    readonly property color focusGlow: Qt.rgba(1, 1, 1, 0.16)
    // Подсветка выбранного, но не сфокусированного (вкладка, пункт меню)
    readonly property color selection: Qt.rgba(1, 1, 1, 0.13)

    // «Стекло»: полупрозрачные тёмные панели поверх живого фона
    readonly property color glass: Qt.rgba(0.075, 0.082, 0.095, 0.62)       // карточки, строки
    readonly property color glassStrong: Qt.rgba(0.062, 0.068, 0.080, 0.92) // меню, оверлеи, клавиатура
    readonly property color glassBorder: Qt.rgba(1, 1, 1, 0.09)             // тонкая кромка
    readonly property color glassHighlight: Qt.rgba(1, 1, 1, 0.07)          // блик по верхнему краю
    readonly property color shadow: Qt.rgba(0, 0, 0, 0.30)                  // мягкая тень под панелями
    readonly property color scrim: Qt.rgba(0.02, 0.022, 0.026, 0.66)        // затемнение под модальными окнами

    // Полосы прогресса
    readonly property color progressTrack: Qt.rgba(1, 1, 1, 0.16)
    readonly property color progressFill: "#f4f5f7"

    // Иллюстрации в стиле технического чертежа (белые линии)
    readonly property color sketchLine: "#e9ebef"
    readonly property color sketchDim: Qt.rgba(0.91, 0.92, 0.94, 0.32)
    readonly property color sketchFaint: Qt.rgba(0.91, 0.92, 0.94, 0.12)
    readonly property color lightbarBlue: "#4c9bff"

    // ---------- Типографика ----------
    // Шкала: Display 88 / Huge 64 / Title 52 / Headline 40 / Large 30 / Body 23 / Small 18 / Caption 14
    readonly property string fontFamily: "Inter"   // при отсутствии fontconfig подставит системный sans
    readonly property int fontDisplay: px(88)
    readonly property int fontHuge: px(64)
    readonly property int fontTitle: px(52)
    readonly property int fontHeadline: px(40)
    readonly property int fontLarge: px(30)
    readonly property int fontBody: px(23)
    readonly property int fontSmall: px(18)
    readonly property int fontCaption: px(14)
    // Разрядка подписей капсом («МАГАЗИН», «ШАГ 2»)
    readonly property real trackingCaps: 1.6 * scale
    // Лёгкое сжатие крупных заголовков
    readonly property real trackingTight: -0.6 * scale

    // ---------- Метрики ----------
    // Скругления: мелкие значки и клавиши / строки, кнопки, поля / плитки игр / панели и меню
    readonly property int radiusSmall: px(8)
    readonly property int radius: px(14)
    readonly property int radiusTile: px(18)
    readonly property int radiusLarge: px(24)
    readonly property int gutter: px(112)

    // Ритм отступов (шаг 4 px при 1080p)
    readonly property int spacingXs: px(8)
    readonly property int spacingS: px(12)
    readonly property int spacingM: px(20)
    readonly property int spacingL: px(32)
    readonly property int spacingXl: px(48)

    // Высоты элементов управления
    readonly property int controlHeight: px(64)    // кнопки
    readonly property int rowHeight: px(84)        // строки списков

    // Фокус: кольцо снаружи элемента и лёгкое увеличение
    readonly property int focusRingWidth: Math.max(2, px(3))
    readonly property int focusRingGap: px(5)
    readonly property real focusScale: 1.03

    // ---------- Анимации ----------
    readonly property int animFast: 140
    readonly property int animNormal: 280
    readonly property int animSlow: 600
    readonly property int easeOut: Easing.OutCubic
    readonly property int easeEmphasized: Easing.OutQuint
}
