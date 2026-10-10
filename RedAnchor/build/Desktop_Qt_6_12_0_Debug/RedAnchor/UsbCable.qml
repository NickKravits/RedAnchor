import QtQuick

// USB-C кабель в координатах макета DualSense (единицы 600×400).
// Точка (0,0) — верх разгрузочного колпачка; кончик штекера заканчивается на y = 84.
Item {
    id: root

    readonly property real tipBottom: 84

    width: 40
    height: tipBottom

    // Провод уходит далеко вверх за пределы экрана
    Rectangle {
        x: 14
        y: -900
        width: 12
        height: 910
        gradient: Gradient {
            orientation: Gradient.Horizontal
            GradientStop { position: 0.0; color: "#1a1b20" }
            GradientStop { position: 0.45; color: "#3a3d47" }
            GradientStop { position: 1.0; color: "#16171b" }
        }
    }

    // Разгрузочный колпачок
    Rectangle {
        x: 10
        y: 0
        width: 20
        height: 26
        radius: 5
        gradient: Gradient {
            orientation: Gradient.Horizontal
            GradientStop { position: 0.0; color: "#1d1e23" }
            GradientStop { position: 0.5; color: "#41444f" }
            GradientStop { position: 1.0; color: "#1b1c21" }
        }
    }

    // Корпус штекера
    Rectangle {
        x: 2
        y: 22
        width: 36
        height: 48
        radius: 8
        gradient: Gradient {
            orientation: Gradient.Horizontal
            GradientStop { position: 0.0; color: "#202127" }
            GradientStop { position: 0.4; color: "#4a4e5a" }
            GradientStop { position: 1.0; color: "#1c1d22" }
        }
        Rectangle {
            // маленький логотип-полоска
            anchors.horizontalCenter: parent.horizontalCenter
            y: 16
            width: 12
            height: 2
            radius: 1
            color: "#6d7280"
        }
    }

    // Металлический наконечник USB-C
    Rectangle {
        x: 9
        y: 69
        width: 22
        height: 15
        radius: 4
        gradient: Gradient {
            orientation: Gradient.Horizontal
            GradientStop { position: 0.0; color: "#8b909c" }
            GradientStop { position: 0.5; color: "#e3e6ec" }
            GradientStop { position: 1.0; color: "#8a8f9a" }
        }
    }
}
