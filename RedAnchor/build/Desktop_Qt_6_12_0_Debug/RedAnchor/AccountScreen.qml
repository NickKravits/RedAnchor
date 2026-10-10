import QtQuick

// Шаг 5: вход в аккаунт RedAnchor. Пока сервера аккаунтов нет —
// вход фиктивный: проверяется только формат данных.
SetupPage {
    id: page

    property bool busy: false

    signal signedIn(string email, string displayName)
    signal skipped()

    step: 3
    title: qsTr("Вход в аккаунт RedAnchor")
    subtitle: qsTr("Аккаунт нужен для синхронизации библиотеки, сохранений и друзей.")
    hints: [{ button: "cross", text: qsTr("Выбрать") }]
    initialFocus: emailField

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
            page.signedIn(email, page.displayNameFor(email))
        }
    }

    Column {
        width: Math.min(parent.width, Theme.px(860))
        spacing: Theme.px(28)
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
                onClicked: page.skipped()
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
