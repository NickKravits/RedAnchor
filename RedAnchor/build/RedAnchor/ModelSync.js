.pragma library

// Обновляет ListModel по массиву объектов, сохраняя совпадающие (по ключу key)
// элементы на месте: список не сбрасывает текущий элемент и фокус при каждом
// обновлении данных (устройства Bluetooth, геймпады).
function sync(model, items, key) {
    for (let i = model.count - 1; i >= 0; --i) {
        const k = model.get(i)[key]
        if (!items.some(item => item[key] === k))
            model.remove(i)
    }
    for (let i = 0; i < items.length; ++i) {
        let found = -1
        for (let j = i; j < model.count; ++j) {
            if (model.get(j)[key] === items[i][key]) {
                found = j
                break
            }
        }
        if (found < 0) {
            model.insert(i, items[i])
        } else {
            if (found !== i)
                model.move(found, i, 1)
            model.set(i, items[i])
        }
    }
}
