import QtQuick

// Вход в Steam (через DepotDownloader):
//   по QR-коду — код сканируется в мобильном приложении Steam, логин не нужен;
//   по паролю — логин, пароль и, если попросит Steam, код или подтверждение Steam Guard.
// Пароль не сохраняется — DepotDownloader запоминает вход сам.
FocusScope {
    id: root

    readonly property var steam: StoreManager.stores.steam || ({})
    readonly property bool busy: steam.busy === true
    readonly property bool guardCode: steam.guard === "code"
    readonly property bool guardMobile: steam.guard === "mobile"
    readonly property string qr: steam.qr || ""
    property string mode: "qr"          // "qr" | "password"
    property bool invertQr: false

    implicitHeight: column.implicitHeight

    function checkLogin() {
        if (loginField.text.trim() === "") {
            loginField.errorText = qsTr("Введите логин Steam.")
            loginField.forceActiveFocus()
            return false
        }
        loginField.errorText = ""
        return true
    }

    function signInQr() {
        mode = "qr"
        StoreManager.steamQrLogin()
    }

    function signInPassword() {
        if (mode !== "password") {
            mode = "password"
            loginField.forceActiveFocus()
            if (loginField.text === "")
                Keyboard.open(loginField)
            return
        }
        if (!checkLogin())
            return
        if (passwordField.text === "") {
            passwordField.errorText = qsTr("Введите пароль.")
            passwordField.forceActiveFocus()
            return
        }
        passwordField.errorText = ""
        StoreManager.steamLogin(loginField.text, passwordField.text)
        passwordField.text = ""
    }

    function sendCode() {
        if (codeField.text.trim() === "") {
            codeField.forceActiveFocus()
            return
        }
        StoreManager.steamGuard(codeField.text)
        codeField.text = ""
    }

    // Пока идёт вход, поля отключены — держим фокус на «Отмене», чтобы до неё можно было дойти
    onBusyChanged: {
        if (busy && !guardCode)
            cancelButton.forceActiveFocus()
        else if (!busy && activeFocus)
            (mode === "password" ? loginField : qrButton).forceActiveFocus()
    }

    // Steam попросил код — сразу открываем поле и клавиатуру
    onGuardCodeChanged: {
        if (guardCode) {
            codeField.text = ""
            codeField.forceActiveFocus()
            Keyboard.open(codeField)
        }
    }

    // ○ во время входа — отменить его; △ — инвертировать QR-код
    Keys.onPressed: (event) => {
        if (event.key === Qt.Key_Escape && root.busy) {
            StoreManager.cancelSteamLogin()
            event.accepted = true
        } else if (event.key === Qt.Key_Space && root.qr !== "") {
            root.invertQr = !root.invertQr
            event.accepted = true
        }
    }

    Row {
        id: column
        width: parent.width
        spacing: Theme.px(56)

        Column {
            width: root.qr !== "" ? parent.width - qrColumn.width - parent.spacing : parent.width
            spacing: Theme.px(22)

            Text {
                text: qsTr("Вход в Steam")
                color: Theme.textPrimary
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontLarge
                font.weight: Font.Light
            }

            ConsoleTextField {
                id: loginField
                visible: root.mode === "password" && !root.guardCode
                width: parent.width
                label: qsTr("Логин Steam")
                placeholder: qsTr("Имя аккаунта, не почта")
                inputHint: "nickname"
                enabled: !root.busy
                KeyNavigation.down: passwordField
                onAccepted: {
                    passwordField.forceActiveFocus()
                    if (passwordField.text === "")
                        Keyboard.open(passwordField)
                }
            }
            ConsoleTextField {
                id: passwordField
                visible: root.mode === "password" && !root.guardCode
                width: parent.width
                label: qsTr("Пароль")
                placeholder: qsTr("Пароль не введён")
                password: true
                enabled: !root.busy
                KeyNavigation.up: loginField
                KeyNavigation.down: passwordButton
                onAccepted: root.signInPassword()
            }

            // ---------- Код Steam Guard ----------
            ConsoleTextField {
                id: codeField
                visible: root.guardCode
                width: parent.width
                label: qsTr("Код Steam Guard")
                placeholder: qsTr("Пять символов")
                inputHint: "nickname"
                KeyNavigation.down: sendCodeButton
                onAccepted: root.sendCode()
            }

            Row {
                spacing: Theme.px(20)
                ConsoleButton {
                    id: qrButton
                    focus: true
                    visible: !root.guardCode && !root.busy
                    text: qsTr("Войти по QR-коду")
                    icon: "lock"
                    primary: root.mode === "qr"
                    KeyNavigation.up: root.mode === "password" ? passwordField : null
                    KeyNavigation.right: passwordButton
                    onClicked: root.signInQr()
                }
                ConsoleButton {
                    id: passwordButton
                    visible: !root.guardCode && !root.busy
                    text: root.mode === "password" ? qsTr("Войти") : qsTr("Войти с паролем")
                    icon: "user"
                    primary: root.mode === "password"
                    KeyNavigation.up: root.mode === "password" ? passwordField : null
                    KeyNavigation.left: qrButton
                    onClicked: root.signInPassword()
                }
                ConsoleButton {
                    id: sendCodeButton
                    visible: root.guardCode
                    text: qsTr("Отправить код")
                    primary: true
                    KeyNavigation.up: codeField
                    KeyNavigation.right: cancelButton
                    onClicked: root.sendCode()
                }
                ConsoleButton {
                    id: cancelButton
                    visible: root.busy
                    text: qsTr("Отмена")
                    icon: "close"
                    KeyNavigation.left: root.guardCode ? sendCodeButton : null
                    KeyNavigation.up: root.guardCode ? codeField : null
                    onClicked: StoreManager.cancelSteamLogin()
                }
                Spinner {
                    visible: root.busy && root.qr === "" && !root.guardCode && !root.guardMobile
                    anchors.verticalCenter: parent.verticalCenter
                }
            }

            // Подтверждение в мобильном приложении Steam (Steam Guard)
            Row {
                visible: root.guardMobile
                spacing: Theme.px(18)
                Rectangle {
                    width: Theme.px(64)
                    height: width
                    radius: width / 2
                    color: Qt.rgba(1, 1, 1, 0.08)
                    border.width: 1
                    border.color: Theme.outlineStrong
                    anchors.verticalCenter: parent.verticalCenter
                    Icon {
                        anchors.centerIn: parent
                        name: "lock"
                        size: Theme.px(30)
                        lineWidth: 1.4
                    }
                }
                Column {
                    anchors.verticalCenter: parent.verticalCenter
                    spacing: Theme.px(4)
                    Text {
                        text: qsTr("Подтвердите вход на телефоне")
                        color: Theme.textPrimary
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontBody
                        font.weight: Font.Medium
                    }
                    Text {
                        text: qsTr("Откройте приложение Steam → Steam Guard и одобрите вход.")
                        color: Theme.textSecondary
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontSmall
                    }
                }
            }

            Text {
                visible: text !== "" && !root.guardMobile
                width: parent.width
                wrapMode: Text.WordWrap
                text: root.steam.message || (root.busy || root.mode === "password" ? ""
                                             : qsTr("Нажмите «Войти по QR-коду» и отсканируйте код в мобильном приложении Steam."))
                color: root.busy || !root.steam.message ? Theme.textSecondary : Theme.warning
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontSmall
            }
        }

        // ---------- QR-код ----------
        Column {
            id: qrColumn
            visible: root.qr !== ""
            spacing: Theme.px(16)

            Rectangle {
                id: qrFrame
                width: Theme.px(400)
                height: width
                radius: Theme.px(12)
                color: root.invertQr ? "#000000" : "#ffffff"

                Canvas {
                    id: qrCanvas
                    anchors.fill: parent
                    anchors.margins: Theme.px(22)   // светлое поле вокруг кода
                    onPaint: {
                        const ctx = getContext("2d")
                        ctx.reset()
                        const rows = root.qr.split("\n")
                        if (!rows.length)
                            return
                        // Код из полублоков (▀ ▄): символ — один модуль в ширину и два в высоту;
                        // из полных блоков (██): два символа на модуль, строка — один модуль
                        const half = root.qr.indexOf("▀") >= 0 || root.qr.indexOf("▄") >= 0
                        const columns = Math.max(...rows.map(r => r.length))
                        const modulesX = half ? columns : Math.ceil(columns / 2)
                        // QR квадратный: сетку считаем по большей стороне, рисуем от верхнего левого угла
                        const modules = Math.max(modulesX, half ? rows.length * 2 : rows.length)
                        const cell = Math.floor(Math.min(width, height) / modules)
                        const ox = Math.floor((width - cell * modules) / 2)
                        const oy = Math.floor((height - cell * modules) / 2)
                        ctx.fillStyle = root.invertQr ? "#ffffff" : "#000000"
                        for (let y = 0; y < rows.length; ++y) {
                            for (let x = 0; x < rows[y].length; ++x) {
                                const c = rows[y][x]
                                if (half) {
                                    if (c === "█" || c === "▀")
                                        ctx.fillRect(ox + x * cell, oy + y * 2 * cell, cell, cell)
                                    if (c === "█" || c === "▄")
                                        ctx.fillRect(ox + x * cell, oy + (y * 2 + 1) * cell, cell, cell)
                                } else if (c === "█") {
                                    ctx.fillRect(ox + Math.floor(x / 2) * cell, oy + y * cell, cell, cell)
                                }
                            }
                        }
                    }
                    Connections {
                        target: root
                        function onQrChanged() { qrCanvas.requestPaint() }
                        function onInvertQrChanged() { qrCanvas.requestPaint() }
                    }
                }
            }
            Text {
                width: qrFrame.width
                wrapMode: Text.WordWrap
                text: qsTr("Приложение Steam → Steam Guard → «Сканировать QR-код».")
                color: Theme.textSecondary
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontSmall
            }
            GlyphText {
                width: qrFrame.width
                text: qsTr("Не сканируется? {triangle} — инвертировать.")
                color: Theme.textMuted
                pixelSize: Theme.fontSmall
            }
        }
    }
}
