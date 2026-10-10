import QtQuick
import QtWebEngine

// Встроенный браузер для входа в Epic Games и GOG, управляемый геймпадом:
//   крестовина — переход между элементами страницы, ✕ — нажать (на поле ввода —
//   экранная клавиатура), △ — отправить форму, ○ — отменить вход.
// Код авторизации перехватывается автоматически и передаётся в StoreManager.
// Браузер приватный: куки и вход не сохраняются и не смешиваются между пользователями.
SetupPage {
    id: page

    property string store                 // "epic" | "gog"
    property bool done: false

    signal finished()

    readonly property string storeTitle: store === "epic" ? "Epic Games" : "GOG"

    title: qsTr("Вход в %1").arg(storeTitle)
    subtitle: qsTr("Крестовина — выбор, {cross} — нажать, {triangle} — отправить, {circle} — отмена.")
    hints: [{ button: "circle", text: qsTr("Отмена") }, { button: "triangle", text: qsTr("Отправить") },
            { button: "cross", text: qsTr("Выбрать") }]
    initialFocus: view

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
            var tag = current.tagName.toLowerCase(), type = (current.type || '').toLowerCase();
            if ((tag === 'input' && ['checkbox', 'radio', 'submit', 'button', 'image', 'reset'].indexOf(type) < 0) || tag === 'textarea')
                return { type: 'text', password: type === 'password', value: current.value || '',
                         label: current.getAttribute('aria-label') || current.placeholder || current.name || '' };
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

    function finishWithCode(code) {
        if (done || code === "")
            return
        done = true
        StoreManager.completeWebLogin(store, code)
        page.finished()
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

    function checkEpicCode() {
        // Epic показывает JSON с authorizationCode на странице перенаправления
        if (store !== "epic" || !view.url.toString().startsWith("https://www.epicgames.com/id/api/redirect"))
            return
        view.runJavaScript("document.body ? document.body.innerText : ''", function (text) {
            try {
                page.finishWithCode(JSON.parse(text).authorizationCode || "")
            } catch (e) {
                console.warn("WebLogin: no authorization code on redirect page")
            }
        })
    }

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

    Rectangle {
        anchors.fill: parent
        radius: Theme.radius
        color: "#ffffff"
        clip: true

        WebEngineView {
            id: view
            anchors.fill: parent
            focus: true
            url: StoreManager.loginUrl(page.store)
            profile: WebEngineProfile {
                offTheRecord: true
            }

            onUrlChanged: page.checkUrl(url)
            onLoadingChanged: (request) => {
                if (request.status === WebEngineView.LoadSucceededStatus) {
                    runJavaScript(page.helperScript)
                    page.checkEpicCode()
                }
            }

            // Геймпад (через клавиши) перехватываем до страницы
            Keys.priority: Keys.BeforeItem
            Keys.onPressed: (event) => {
                switch (event.key) {
                case Qt.Key_Up: view.runJavaScript("window.__ra && __ra.move('up')"); break
                case Qt.Key_Down: view.runJavaScript("window.__ra && __ra.move('down')"); break
                case Qt.Key_Left: view.runJavaScript("window.__ra && __ra.move('left')"); break
                case Qt.Key_Right: view.runJavaScript("window.__ra && __ra.move('right')"); break
                case Qt.Key_Space: view.runJavaScript("window.__ra && __ra.submit()"); break
                case Qt.Key_Escape: page.finished(); break
                case Qt.Key_Return:
                case Qt.Key_Enter:
                    view.runJavaScript("window.__ra ? __ra.activate() : null", function (result) {
                        if (!result || result.type !== "text")
                            return
                        proxy.text = result.value
                        proxy.password = result.password
                        proxy.inputHint = result.password ? "password" : "nickname"
                        proxy.label = result.label
                        Keyboard.open(proxy)
                    })
                    break
                default:
                    return   // остальные клавиши (печать с клавиатуры) — странице
                }
                event.accepted = true
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
}
