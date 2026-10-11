.pragma library

// Подписи витрины магазинов: цены, даты, платформы. Игра — словарь из StoreCatalog
// (поля описаны в src/storecatalogparser.h).

// Картинка поменьше для плиток: CDN Epic умеет отдавать уменьшенную копию
function thumb(url, width) {
    if (!url)
        return ""
    if (url.indexOf("cdn1.epicgames.com") >= 0 || url.indexOf("cdn2.unrealengine.com") >= 0)
        return url + (url.indexOf("?") >= 0 ? "&" : "?") + "resize=1&w=" + Math.round(width) + "&quality=medium"
    return url
}

// Дата выхода: «9 мая 2024»
function date(iso) {
    if (!iso)
        return ""
    const d = new Date(iso + "T12:00:00Z")
    if (isNaN(d.getTime()))
        return ""
    return d.toLocaleDateString(Qt.locale("ru_RU"), "d MMMM yyyy")
}

// Дата и время раздачи: «27 августа»
function shortDate(isoTime) {
    if (!isoTime)
        return ""
    const d = new Date(isoTime)
    if (isNaN(d.getTime()))
        return ""
    return d.toLocaleDateString(Qt.locale("ru_RU"), "d MMMM")
}

// Цена для кнопки и плитки: «499 ₽», «Бесплатно», «Скоро»
function price(item) {
    if (!item || !item.price)
        return ""
    if (item.promo === "now")
        return qsTr("Бесплатно")
    if (item.price.free)
        return qsTr("Бесплатно")
    if (item.price.known && item.price.final)
        return item.price.final
    if (item.upcoming)
        return qsTr("Скоро")
    return ""
}

// Раздача Epic: «Бесплатно до 27 августа» / «Бесплатно с 27 августа»
function promo(item) {
    if (!item || !item.promo)
        return ""
    if (item.promo === "now")
        return item.promoEnd ? qsTr("Бесплатно до %1").arg(shortDate(item.promoEnd)) : qsTr("Бесплатно сейчас")
    return item.promoStart ? qsTr("Бесплатно с %1").arg(shortDate(item.promoStart)) : qsTr("Скоро бесплатно")
}

// Скидка в процентах (0 — нет скидки; раздача — тоже не «скидка»)
function discount(item) {
    if (!item || !item.price || item.promo === "now" || item.price.free)
        return 0
    return item.price.discount || 0
}

function platforms(item) {
    if (!item || !item.platforms)
        return ""
    const names = { windows: "Windows", mac: "macOS", linux: "Linux" }
    return item.platforms.map(p => names[p] || p).join(", ")
}

// Строка под названием: «CD PROJEKT RED · 2015»
function meta(item) {
    if (!item)
        return ""
    const parts = []
    if (item.developer)
        parts.push(item.developer)
    if (item.releaseDate)
        parts.push(item.releaseDate.substring(0, 4))
    return parts.join(" · ")
}
