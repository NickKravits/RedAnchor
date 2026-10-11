import QtQuick
import QtWebEngine

// Отдельное большое окно встроенного браузера для входа в магазин (GOG, Epic Games)
// (на консоли — во весь экран). Управление геймпадом:
//   крестовина — переход между элементами страницы, ✕ — нажать (на поле ввода —
//   экранная клавиатура), △ — отправить форму, ○ — отменить вход,
//   L1 / R1 — прокрутка, правый стик — курсор (только в этом окне), ✕ или R3 — щелчок курсором;
//   щелчок курсором по полю ввода открывает экранную клавиатуру.
// Код авторизации перехватывается автоматически и передаётся в StoreManager:
//   GOG  — из адреса перенаправления embed.gog.com/on_login_success?code=…;
//   Epic — со страницы www.epicgames.com/id/api/redirect: она отдаёт JSON с authorizationCode
//          (так входят legendary и Heroic).
// Браузер приватный: куки и вход не сохраняются и не смешиваются между пользователями.
Window {
    id: win

    property string store                 // "gog" | "epic"
    property bool fullscreen: false
    property bool done: false

    signal finished()

    readonly property string storeTitle: store === "epic" ? "Epic Games" : "GOG"

    title: qsTr("Вход в %1").arg(storeTitle)
    color: Theme.background
    // Как у главного окна: полный экран — только после первого кадра (иначе падает cage)
    property bool shown: false
    onFrameSwapped: if (!shown) shown = true
    visibility: !shown ? Window.Windowed : fullscreen ? Window.FullScreen : Window.Maximized
    flags: fullscreen ? Qt.Window | Qt.FramelessWindowHint : Qt.Window

    // Навигация по странице: выбор элементов по их положению, подсветка, ввод текста
    readonly property string helperScript: "
(function () {
    if (window.__ra) return;
    var style = document.createElement('style');
    style.textContent = '.__ra-focus { outline: 3px solid #f4f5f7 !important; outline-offset: 3px !important; }';
    document.documentElement.appendChild(style);
    var current = null;
    function items() {
        return Array.prototype.filter.call(document.querySelectorAll(
            'a[href], button, input:not([type=hidden]), select, textarea, [role=button], [tabindex]:not([tabindex=\"-1\"])'),
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
                 label: e.getAttribute('aria-label') || e.placeholder || e.name || '' };
    }
    // Щелчок курсором по полю ввода — оболочка откроет экранную клавиатуру.
    // Ловим сам щелчок, а не получение фокуса: страница входа часто уже поставила фокус
    // в поле, и повторный щелчок по нему фокус не меняет.
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
        },
        submit: function () {
            var form = valid() ? current.form : document.querySelector('form');
            if (!form) return;
            var button = form.querySelector('button[type=submit], input[type=submit]');
            if (button) button.click(); else if (form.requestSubmit) form.requestSubmit();
        }
    };
})();"

    function close_() {
        if (done)
            return
        done = true
        GamepadManager.pointerEnabled = false
        GamepadManager.keyEventsEnabled = true
        win.finished()
    }

    function finishWithCode(code) {
        if (done || code === "")
            return
        StoreManager.completeWebLogin(store, code)
        close_()
    }

    // Код со страницы перенаправления
    function checkUrl(url) {
        const text = url.toString()
        if (store === "gog" && text.startsWith("https://embed.gog.com/on_login_success")) {
            const match = text.match(/[?&]code=([^&]+)/)
            if (match)
                finishWithCode(decodeURIComponent(match[1]))
        }
    }

    // Epic: код — в JSON на странице перенаправления (её текст читаем после загрузки)
    function checkPage(url) {
        if (store !== "epic" || !url.toString().startsWith("https://www.epicgames.com/id/api/redirect"))
            return
        view.runJavaScript("document.body ? document.body.innerText : ''", function (text) {
            try {
                const code = JSON.parse(text).authorizationCode
                if (code)
                    win.finishWithCode(code)
            } catch (e) {
                console.warn("Epic login: no JSON on the redirect page")
            }
        })
    }

    function openKeyboard(info) {
        if (!info || info.type !== "text" || Keyboard.active)
            return
        proxy.text = info.value
        proxy.password = info.password
        proxy.inputHint = info.password ? "password" : "nickname"
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
                view.runJavaScript("window.__ra && __ra.submit()")
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
            Text {
                anchors.verticalCenter: parent.verticalCenter
                text: win.title
                color: Theme.textPrimary
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontLarge
                font.weight: Font.Light
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
                { button: "circle", text: qsTr("Отмена") },
                { button: "triangle", text: qsTr("Отправить") },
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
            url: StoreManager.loginUrl(win.store)
            profile: WebEngineProfile {
                offTheRecord: true
            }

            onUrlChanged: win.checkUrl(url)
            onLoadingChanged: (request) => {
                if (request.status === WebEngineView.LoadSucceededStatus) {
                    runJavaScript(win.helperScript)
                    win.checkPage(request.url)
                }
            }

            // Поле выбрали щелчком курсора — открываем экранную клавиатуру
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
