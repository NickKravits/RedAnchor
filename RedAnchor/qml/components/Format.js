.pragma library

// Размер в байтах для людей: «512 МБ», «12,3 ГБ»
function bytes(value) {
    const v = Number(value) || 0
    const units = ["Б", "КБ", "МБ", "ГБ", "ТБ"]
    let i = 0
    let n = v
    while (n >= 1024 && i < units.length - 1) {
        n /= 1024
        ++i
    }
    const digits = n >= 100 || i === 0 ? 0 : 1
    return n.toFixed(digits).replace(".", ",") + " " + units[i]
}

// Название магазина по ключу
function storeName(key) {
    switch (key) {
    case "steam": return "Steam"
    case "epic": return "Epic Games"
    case "gog": return "GOG"
    case "redstore": return "RedStore"
    default: return key
    }
}
