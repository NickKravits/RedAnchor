#include "qrcode.h"

#include "third_party/qrcodegen/qrcodegen.hpp"

#include <QPainter>

#include <cmath>
#include <exception>

QrCode::QrCode(QQuickItem *parent)
    : QQuickPaintedItem(parent)
{
    // Чёткие края модулей: без сглаживания соседние квадраты не дают светлых швов
    setAntialiasing(false);
}

std::vector<bool> QrCode::encode(const QString &text, int *size)
{
    *size = 0;
    if (text.isEmpty())
        return {};
    try {
        // Средний уровень коррекции: код надёжно читается с экрана телевизора
        const qrcodegen::QrCode qr = qrcodegen::QrCode::encodeText(text.toUtf8().constData(), qrcodegen::QrCode::Ecc::MEDIUM);
        const int n = qr.getSize();
        std::vector<bool> modules(size_t(n) * size_t(n));
        for (int y = 0; y < n; ++y) {
            for (int x = 0; x < n; ++x)
                modules[size_t(y) * size_t(n) + size_t(x)] = qr.getModule(x, y);
        }
        *size = n;
        return modules;
    } catch (const std::exception &e) {
        // Слишком длинный текст (qrcodegen::data_too_long)
        qWarning("QrCode: %s", e.what());
        return {};
    }
}

void QrCode::setText(const QString &text)
{
    if (text == m_text)
        return;
    m_text = text;
    m_modules = encode(text, &m_size);
    emit textChanged();
    update();
}

void QrCode::setColor(const QColor &color)
{
    if (color == m_color)
        return;
    m_color = color;
    emit appearanceChanged();
    update();
}

void QrCode::setBackground(const QColor &color)
{
    if (color == m_background)
        return;
    m_background = color;
    emit appearanceChanged();
    update();
}

void QrCode::setQuietZone(int modules)
{
    modules = qMax(0, modules);
    if (modules == m_quietZone)
        return;
    m_quietZone = modules;
    emit appearanceChanged();
    update();
}

void QrCode::paint(QPainter *painter)
{
    const qreal side = qMin(width(), height());
    if (side <= 0)
        return;
    const QRectF area((width() - side) / 2, (height() - side) / 2, side, side);
    painter->fillRect(area, m_background);
    if (m_size <= 0)
        return;
    // Модуль — целое число пикселей (если помещается): код не «плывёт» и уверенно читается камерой
    const int total = m_size + 2 * m_quietZone;
    const qreal whole = std::floor(side / total);
    const qreal cell = whole > 0 ? whole : side / total;
    const qreal offsetX = area.x() + (side - cell * m_size) / 2;
    const qreal offsetY = area.y() + (side - cell * m_size) / 2;
    painter->setPen(Qt::NoPen);
    painter->setBrush(m_color);
    for (int y = 0; y < m_size; ++y) {
        // Соседние тёмные модули строки рисуем одним прямоугольником
        int x = 0;
        while (x < m_size) {
            if (!m_modules[size_t(y) * size_t(m_size) + size_t(x)]) {
                ++x;
                continue;
            }
            const int start = x;
            while (x < m_size && m_modules[size_t(y) * size_t(m_size) + size_t(x)])
                ++x;
            painter->drawRect(QRectF(offsetX + start * cell, offsetY + y * cell, (x - start) * cell, cell));
        }
    }
}
