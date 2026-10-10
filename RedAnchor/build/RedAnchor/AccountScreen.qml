import QtQuick

// Шаг 5: вход в аккаунт RedAnchor. Пока сервера аккаунтов нет —
// вход фиктивный: проверяется только формат данных.
// Если вход пропустить, нужно ввести никнейм: создаётся локальная учётная
// запись (AccountManager), под которой будут храниться данные пользователя.
SetupPage {
    id: page

    property bool busy: false
    property string mode: "signin"      // "signin" | "local"

    // Учётная запись создана (AccountManager уже заполнен)
    signal done()

    step: 4
    title: mode === "local" ? qsTr("Локальная учётная запись") : qsTr("Вход в аккаунт RedAnchor")
    subtitle: mode === "local"
              ? qsTr("Придумайте никнейм — под ним на этой консоли будут храниться ваши игры, сохранения и настройки. Войти в аккаунт RedAnchor можно позже.")
              : qsTr("Аккаунт нужен для синхронизации библиотеки, сохранений и друзей.")
    hints: mode === "local"
           ? [{ button: "circle", text: qsTr("Назад") }, { button: "cross", text: qsTr("Выбрать") }]
           : [{ button: "cross", text: qsTr("Выбрать") }]
    initialFocus: emailField

    Keys.onEscapePressed: if (mode === "local") showSignIn()

    function showLocal() {
        mode = "local"
        nicknameField.errorText = ""
        nicknameField.forceActiveFocus()
        if (nicknameField.text === "")
            Keyboard.open(nicknameField)
    }

    function showSignIn() {
        mode = "signin"
        skipButton.forceActiveFocus()
    }

    function createLocal() {
        nicknameField.errorText = AccountManager.validateNickname(nicknameField.text)
        if (nicknameField.errorText !== "") {
            nicknameField.forceActiveFocus()
            return
        }
        if (AccountManager.createLocal(nicknameField.text))
            page.done()
    }

    function displayNameFor(email) {
        const login = email.split("@")[0].replace(/[._-]+/g, " ").trim()
        return login.length > 0 ? login.charAt(0).toUpperCase() + login.slice(1) : email
    }

    function signIn() {
        const email = emailField.text.trim()
        emailField.errorText = /^[^\s@]+@[^\s@]+\.[^\s@]+$/.test(email) ? "" : qsTr("Введите корректный адрес электронной почты.")
        passwordField.errorText = passwordField.text.length >= 6 ? "" : qsTr("Пароль должен содержать не менее 6 символов.")
        if (emailField.errorText !== "") {
            emailField.forceActiveFocus()
            return
        }
        if (passwordField.errorText !== "") {
            passwordField.forceActiveFocus()
            return
        }
        busy = true
        fakeServer.start()
    }

    Timer {
        id: fakeServer
        interval: 1600
        onTriggered: {
            page.busy = false
            const email = emailField.text.trim()
            AccountManager.signInOnline(email, page.displayNameFor(email))
            page.done()
        }
    }

    Column {
        width: Math.min(parent.width, Theme.px(860))
        spacing: Theme.px(28)
        visible: page.mode === "signin"
        enabled: !page.busy
        opacity: page.busy ? 0.5 : 1

        ConsoleTextField {
            id: emailField
            width: parent.width
            label: qsTr("Электронная почта")
            placeholder: "name@example.com"
            inputHint: "email"
            KeyNavigation.down: passwordField
            onAccepted: {
                passwordField.forceActiveFocus()
                if (passwordField.text === "")
                    Keyboard.open(passwordField)
            }
        }

        ConsoleTextField {
            id: passwordField
            width: parent.width
            label: qsTr("Пароль")
            placeholder: qsTr("Не менее 6 символов")
            password: true
            KeyNavigation.up: emailField
            KeyNavigation.down: signInButton
            onAccepted: page.signIn()
        }

        Row {
            spacing: Theme.px(20)
            topPadding: Theme.px(10)

            ConsoleButton {
                id: signInButton
                text: qsTr("Войти")
                primary: true
                KeyNavigation.up: passwordField
                KeyNavigation.right: skipButton
                onClicked: page.signIn()
            }
            ConsoleButton {
                id: skipButton
                text: qsTr("Пропустить")
                KeyNavigation.up: passwordField
                KeyNavigation.left: signInButton
                onClicked: page.showLocal()
            }
        }

        Text {
            width: parent.width
            text: qsTr("Сервер аккаунтов пока не подключён: вход выполняется локально, данные никуда не отправляются.")
            color: Theme.textMuted
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontSmall
            wrapMode: Text.WordWrap
        }
    }

    // ---------- Локальная учётная запись ----------
    Column {
        width: Math.min(parent.width, Theme.px(860))
        spacing: Theme.px(28)
        visible: page.mode === "local"

        Row {
            spacing: Theme.px(18)
            Icon {
                name: "user"
                size: Theme.px(40)
                lineWidth: 1.3
                anchors.verticalCenter: parent.verticalCenter
            }
            Text {
                anchors.verticalCenter: parent.verticalCenter
                text: nicknameField.text.trim() !== "" ? nicknameField.text.trim() : qsTr("Игрок")
                color: nicknameField.text.trim() !== "" ? Theme.textPrimary : Theme.textMuted
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontLarge
                font.weight: Font.Light
            }
        }

        ConsoleTextField {
            id: nicknameField
            width: parent.width
            label: qsTr("Никнейм")
            placeholder: qsTr("От 3 до 16 символов: буквы, цифры, _ - .")
            inputHint: "nickname"
            KeyNavigation.down: createButton
            onAccepted: page.createLocal()
        }

        Row {
            spacing: Theme.px(20)
            topPadding: Theme.px(10)

            ConsoleButton {
                id: createButton
                text: qsTr("Создать")
                primary: true
                KeyNavigation.up: nicknameField
                KeyNavigation.right: backButton
                onClicked: page.createLocal()
            }
            ConsoleButton {
                id: backButton
                text: qsTr("Назад")
                KeyNavigation.up: nicknameField
                KeyNavigation.left: createButton
                onClicked: page.showSignIn()
            }
        }

        Text {
            width: parent.width
            text: qsTr("Учётная запись хранится только на этой консоли.")
            color: Theme.textMuted
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontSmall
            wrapMode: Text.WordWrap
        }
    }

    Row {
        anchors.centerIn: parent
        visible: page.busy
        spacing: Theme.px(18)
        Spinner { anchors.verticalCenter: parent.verticalCenter }
        Text {
            anchors.verticalCenter: parent.verticalCenter
            text: qsTr("Выполняется вход…")
            color: Theme.textPrimary
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontBody
        }
    }
}
