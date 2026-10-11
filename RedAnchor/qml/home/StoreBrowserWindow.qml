import QtQuick
import QtWebEngine

// Окно встроенного браузера для сайтов магазинов: покупка игры, страница игры,
// активация ключа (на консоли — во весь экран). Управление геймпадом — как во входе
// в магазин (WebLoginWindow.qml):
//   крестовина — переход между элементами страницы, ✕ — нажать (на поле ввода —
//   экранная клавиатура), △ — назад по страницам, □ — QR-код этой страницы для телефона,
//   ○ — закрыть, L1 / R1 — прокрутка, правый стик — курсор, ✕ или R3 — щелчок курсором.
//
// В отличие от окна входа браузер постоянный: вход на сайт магазина и корзина запоминаются,
// у каждого пользователя консоли — свои (папка профиля: stores/web).
// После покупки («спасибо за покупку», библиотека) и при закрытии окна купленные игры
// магазина обновляются — новая игра появляется в библиотеке.
Window {
    id: win

    property string store                 // "gog" | "epic"
    property url startUrl
    property string heading               // «Покупка: …», «Активация ключа GOG»
    property bool fullscreen: false
    property bool done: false
    // Страница похожа на завершённую покупку или активацию
    property bool purchaseDetected: false

    signal finished(bool purchaseDetected)

    title: heading !== "" ? heading : qsTr("Магазин")
    color: Theme.background
    // Как у главного окна: полный экран — только после первого кадра (иначе падает cage)
    property bool shown: false
    onFrameSwapped: if (!shown) shown = true
    visibility: !shown ? Window.Windowed : fullscreen ? Window.FullScreen : Window.Maximized
    flags: fullscreen ? Qt.Window | Qt.FramelessWindowHint : Qt.Window

    // Навигация по странице: выбор элементов по их положению, подсветка, ввод текста
    // (тот же помощник, что в WebLoginWindow.qml, без отправки формы по △)
    readonly property string helperScript: "
(function () {
    if (window.__ra) return;
    var style = document.createElement('style');
    style.textContent = '.__ra-focus { outline: 3px solid #f4f5f7 !important; outline-offset: 3px !important; border-radius: 4px; }';
    document.documentElement.appendChild(style);
    var current = null;
    function items() {
        return Array.prototype.filter.call(document.querySelectorAll(
            'a[href], button, input:not([type=hidden]), select, textarea, [role=button], [role=link], [role=tab], [tabindex]:not([tabindex=\"-1\"])'),
            function (e) {
                var r = e.getBoundingClientRect(); var s = getComputedStyle(e);
                return r.width > 0 && r.height > 0 && s.visibility !== 'hidden' && s.display !== 'none' && !e.disabled;
            });
    }
    function mark(e) {
        if (current) current.classList.remove('__ra-focus');
        current = e;
        if (!e) return;
        e.classList.add('__ra-focus');
        e.focus({ preventScroll: true });
        e.scrollIntoView({ block: 'center', behavior: 'smooth' });
    }
    function valid() { return current && document.contains(current); }
    function textInfo(e) {
        var tag = e.tagName.toLowerCase(), type = (e.type || '').toLowerCase();
        if (!((tag === 'input' && ['checkbox', 'radio', 'submit', 'button', 'image', 'reset', 'file'].indexOf(type) < 0) || tag === 'textarea'))
            return null;
        return { type: 'text', password: type === 'password', value: e.value || '',
                 email: type === 'email', label: e.getAttribute('aria-label') || e.placeholder || e.name || '' };
    }
    document.addEventListener('click', function (event) {
        var target = event.target && event.target.closest ? event.target.closest('input, textarea') : null;
        var info = target ? textInfo(target) : null;
        if (!info) return;
        mark(target);
        console.log('__ra:text:' + JSON.stringify(info));
    }, true);
    window.__ra = {
        move: function (dir) {
            var list = items();
            if (!list.length) { window.scrollBy(0, dir === 'up' ? -240 : dir === 'down' ? 240 : 0); return; }
            if (!valid()) { mark(list[0]); return; }
            var c = current.getBoundingClientRect(), cx = c.left + c.width / 2, cy = c.top + c.height / 2;
            var best = null, bestScore = 1e9, vertical = dir === 'up' || dir === 'down';
            list.forEach(function (e) {
                if (e === current) return;
                var r = e.getBoundingClientRect(), dx = r.left + r.width / 2 - cx, dy = r.top + r.height / 2 - cy;
                var ok = dir === 'up' ? dy < -4 : dir === 'down' ? dy > 4 : dir === 'left' ? dx < -4 : dx > 4;
                if (!ok) return;
                var score = vertical ? Math.abs(dy) + 2 * Math.abs(dx) : Math.abs(dx) + 2 * Math.abs(dy);
                if (score < bestScore) { bestScore = score; best = e; }
            });
            if (best) mark(best);
            else if (vertical) window.scrollBy(0, dir === 'up' ? -240 : 240);
        },
        activate: function () {
            if (!valid()) { var l = items(); if (l.length) mark(l[0]); return { type: 'none' }; }
            var info = textInfo(current);
            if (info) return info;
            current.click();
            return { type: 'click' };
        },
        setText: function (value) {
            if (!valid()) return;
            var proto = current.tagName === 'TEXTAREA' ? HTMLTextAreaElement.prototype : HTMLInputElement.prototype;
            Object.getOwnPropertyDescriptor(proto, 'value').set.call(current, value);
            current.dispatchEvent(new Event('input', { bubbles: true }));
            current.dispatchEvent(new Event('change', { bubbles: true }));
        }
    };
})();"

    function close_() {
        if (done)
            return
        done = true
        GamepadManager.pointerEnabled = false
        GamepadManager.keyEventsEnabled = true
        win.finished(win.purchaseDetected)
    }

    // Страница «спасибо за покупку», заказ оформлен или ключ активирован.
    // Точных адресов магазины не публикуют — узнаём по словам в адресе.
    function checkUrl(url) {
        const text = url.toString().toLowerCase()
        const success = /(thank[-_]?you|order[-_\/]?(complete|confirm|success)|purchase[-_\/#]*success|checkout\/[^?#]*success|redeem[^?#]*success)/
        if (!purchaseDetected && success.test(text)) {
            purchaseDetected = true
            // Магазину нужно время, чтобы покупка попала в список игр аккаунта
            refreshTimer.restart()
        }
    }
    Timer {
        id: refreshTimer
        interval: 4000
        onTriggered: StoreCatalog.refreshOwned(win.store)
    }

    function openKeyboard(info) {
        if (!info || info.type !== "text" || Keyboard.active)
            return
        proxy.text = info.value
        proxy.password = info.password
        proxy.inputHint = info.password ? "password" : info.email ? "email" : "text"
        proxy.label = info.label
        Keyboard.open(proxy)
    }

    // Внутри WebEngine клавиши забирает себе Chromium, поэтому геймпад управляет
    // страницей сигналами кнопок, а нажатия клавиш на это время выключены
    // (кроме экранной клавиатуры — ей они нужны).
    Binding {
        target: GamepadManager
        property: "keyEventsEnabled"
        value: !win.active || Keyboard.active
        when: !win.done
    }
    Connections {
        target: GamepadManager
        function onButtonPressed(button) {
            if (!win.active || Keyboard.active || win.done)
                return
            // Открыт QR-код страницы: любая кнопка, кроме □, просто закрывает его
            if (phone.visible) {
                phone.visible = false
                return
            }
            switch (button) {
            case "up":
            case "down":
            case "left":
            case "right":
                view.runJavaScript("window.__ra && __ra.move('" + button + "')")
                break
            case "cross":
                // Пока виден курсор, ✕ — это щелчок мышью (его отправляет GamepadManager)
                if (GamepadManager.pointerVisible)
                    break
                view.runJavaScript("window.__ra ? __ra.activate() : null", function (result) { win.openKeyboard(result) })
                break
            case "triangle":
                if (view.canGoBack)
                    view.goBack()
                break
            case "square":
                phone.visible = true
                break
            case "circle":
                win.close_()
                break
            case "l1":
                view.runJavaScript("window.scrollBy(0, -window.innerHeight * 0.8)")
                break
            case "r1":
                view.runJavaScript("window.scrollBy(0, window.innerHeight * 0.8)")
                break
            }
        }
    }

    Component.onCompleted: {
        GamepadManager.pointerEnabled = true
        requestActivate()
        view.forceActiveFocus()
    }
    onClosing: close_()

    // Невидимое поле — посредник между экранной клавиатурой и полем на странице
    ConsoleTextField {
        id: proxy
        visible: false
        onAccepted: view.runJavaScript("window.__ra && window.__ra.setText(" + JSON.stringify(proxy.text) + ")")
    }
    Connections {
        target: Keyboard
        function onClosed(field, accepted) {
            if (field === proxy)
                view.forceActiveFocus()
        }
    }

    // ---------- Заголовок ----------
    Item {
        id: header
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        height: Theme.px(96)

        Row {
            anchors.left: parent.left
            anchors.leftMargin: Theme.px(48)
            anchors.verticalCenter: parent.verticalCenter
            spacing: Theme.px(18)
            Icon {
                name: "lock"
                size: Theme.px(30)
                lineWidth: 1.4
                anchors.verticalCenter: parent.verticalCenter
            }
            Column {
                anchors.verticalCenter: parent.verticalCenter
                spacing: Theme.px(2)
                Text {
                    text: win.title
                    color: Theme.textPrimary
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontLarge
                    font.weight: Font.Light
                    elide: Text.ElideRight
                    width: Math.min(implicitWidth, win.width * 0.4)
                }
                Text {
                    text: view.url.toString().replace(/^https?:\/\//, "").split(/[/?#]/)[0]
                    color: Theme.textMuted
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontCaption + Theme.px(2)
                }
            }
            Spinner {
                visible: view.loading
                size: Theme.px(24)
                anchors.verticalCenter: parent.verticalCenter
            }
        }

        HintBar {
            anchors.right: parent.right
            anchors.rightMargin: Theme.px(48)
            anchors.verticalCenter: parent.verticalCenter
            model: Keyboard.active ? [] : [
                { button: "circle", text: qsTr("Закрыть") },
                { button: "triangle", text: qsTr("Назад") },
                { button: "square", text: qsTr("На телефоне") },
                { button: "cross", text: qsTr("Выбрать") },
                { button: "r3", text: qsTr("Курсор — правый стик") }
            ]
        }
    }

    // ---------- Страница ----------
    Rectangle {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: header.bottom
        anchors.bottom: parent.bottom
        color: "#ffffff"

        WebEngineView {
            id: view
            anchors.fill: parent
            focus: true
            url: win.startUrl
            profile: WebEngineProfile {
                // Свой постоянный профиль у каждого пользователя консоли
                storageName: "RedAnchorStore-" + (AccountManager.id !== "" ? AccountManager.id : "default")
                offTheRecord: false
                persistentStoragePath: AccountManager.profileDir !== "" ? AccountManager.profileDir + "/stores/web" : ""
                persistentCookiesPolicy: WebEngineProfile.ForcePersistentCookies
                httpCacheType: WebEngineProfile.DiskHttpCache
            }

            onUrlChanged: win.checkUrl(url)
            onLoadingChanged: (request) => {
                if (request.status === WebEngineView.LoadSucceededStatus) {
                    runJavaScript(win.helperScript)
                }
            }
            // Ссылки «в новом окне» (оплата, вход) открываем здесь же
            onNewWindowRequested: (request) => request.openIn(view)

            onJavaScriptConsoleMessage: (level, message, lineNumber, sourceID) => {
                if (message.startsWith("__ra:text:"))
                    win.openKeyboard(JSON.parse(message.slice(10)))
            }
        }

        // Загрузка страницы
        Rectangle {
            anchors.fill: parent
            color: Theme.background
            visible: view.loading && view.loadProgress < 60
            Spinner {
                anchors.centerIn: parent
            }
        }

        // Покупка завершена: игра скоро появится в библиотеке
        Rectangle {
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.bottom: parent.bottom
            anchors.bottomMargin: Theme.px(36)
            width: doneRow.implicitWidth + Theme.px(56)
            height: Theme.px(72)
            radius: height / 2
            color: Theme.glassStrong
            border.width: 1
            border.color: Theme.glassBorder
            opacity: win.purchaseDetected ? 1 : 0
            visible: opacity > 0
            Behavior on opacity { NumberAnimation { duration: Theme.animNormal } }

            Row {
                id: doneRow
                anchors.centerIn: parent
                spacing: Theme.px(14)
                Icon {
                    name: "check"
                    size: Theme.px(28)
                    lineWidth: 2
                    color: Theme.success
                    anchors.verticalCenter: parent.verticalCenter
                }
                Text {
                    anchors.verticalCenter: parent.verticalCenter
                    text: qsTr("Готово — игра появится в библиотеке.")
                    color: Theme.textPrimary
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontBody
                }
                ButtonGlyph {
                    button: "circle"
                    size: Theme.px(28)
                    anchors.verticalCenter: parent.verticalCenter
                }
                Text {
                    anchors.verticalCenter: parent.verticalCenter
                    text: qsTr("Вернуться")
                    color: Theme.textSecondary
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontSmall
                }
            }
        }
    }

    // ---------- QR-код этой страницы: продолжить на телефоне ----------
    Rectangle {
        id: phone
        anchors.fill: parent
        z: 15
        visible: false
        color: Theme.glassStrong

        Row {
            anchors.centerIn: parent
            spacing: Theme.px(48)
            Rectangle {
                width: Theme.px(400)
                height: width
                radius: Theme.radiusTile
                color: "#ffffff"
                QrCode {
                    anchors.fill: parent
                    anchors.margins: Theme.px(18)
                    text: phone.visible ? view.url.toString() : ""
                    quietZone: 1
                }
            }
            Column {
                anchors.verticalCenter: parent.verticalCenter
                width: Theme.px(560)
                spacing: Theme.px(16)
                Text {
                    width: parent.width
                    text: qsTr("Продолжить на телефоне")
                    color: Theme.textPrimary
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontLarge
                    font.weight: Font.Light
                }
                Text {
                    width: parent.width
                    text: qsTr("Наведите камеру телефона на код — эта страница откроется на телефоне. После оплаты игра появится в библиотеке консоли.")
                    color: Theme.textSecondary
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontBody
                    lineHeight: 1.25
                    wrapMode: Text.WordWrap
                }
                HintBar {
                    layoutDirection: Qt.LeftToRight
                    model: [{ button: "circle", text: qsTr("Закрыть") }]
                }
            }
        }
    }

    // ---------- Экранная клавиатура этого окна ----------
    OnScreenKeyboard {
        width: parent.width
        z: 10
    }

    // ---------- Курсор на правом стике (в стиле PS5: светлый круг с кольцом) ----------
    Item {
        z: 20
        visible: GamepadManager.pointerVisible && GamepadManager.pointerWindow === win
        x: GamepadManager.pointerPos.x
        y: GamepadManager.pointerPos.y

        Rectangle {
            anchors.centerIn: parent
            width: Theme.px(44)
            height: width
            radius: width / 2
            color: Qt.rgba(1, 1, 1, 0.22)
            border.width: Theme.px(2)
            border.color: Qt.rgba(1, 1, 1, 0.9)
        }
        Rectangle {
            anchors.centerIn: parent
            width: Theme.px(10)
            height: width
            radius: width / 2
            color: "#ffffff"
            border.width: 1
            border.color: Qt.rgba(0, 0, 0, 0.35)
        }
    }
}
