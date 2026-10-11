#pragma once

#include <QColor>
#include <QQuickPaintedItem>
#include <QString>
#include <QtQml/qqmlregistration.h>

#include <vector>

// QR-код для QML: QrCode { text: "https://…" }. Строится библиотекой Nayuki QR Code generator
// (src/third_party/qrcodegen, лицензия MIT) и рисуется квадратами по сетке модулей.
// Код всегда квадратный и вписан в элемент; вокруг — светлое поле (quietZone, в модулях),
// без которого телефон плохо распознаёт код на тёмном фоне.
class QrCode : public QQuickPaintedItem
{
    Q_OBJECT
    QML_ELEMENT

    Q_PROPERTY(QString text READ text WRITE setText NOTIFY textChanged)
    Q_PROPERTY(QColor color READ color WRITE setColor NOTIFY appearanceChanged)
    Q_PROPERTY(QColor background READ background WRITE setBackground NOTIFY appearanceChanged)
    Q_PROPERTY(int quietZone READ quietZone WRITE setQuietZone NOTIFY appearanceChanged)
    // Модулей по стороне (0 — текст не поместился в QR-код)
    Q_PROPERTY(int modules READ modules NOTIFY textChanged)

public:
    explicit QrCode(QQuickItem *parent = nullptr);

    QString text() const { return m_text; }
    void setText(const QString &text);
    QColor color() const { return m_color; }
    void setColor(const QColor &color);
    QColor background() const { return m_background; }
    void setBackground(const QColor &color);
    int quietZone() const { return m_quietZone; }
    void setQuietZone(int modules);
    int modules() const { return m_size; }

    void paint(QPainter *painter) override;

    // Модули QR-кода для текста: size × size, true — тёмный. Пусто — не удалось построить.
    static std::vector<bool> encode(const QString &text, int *size);

signals:
    void textChanged();
    void appearanceChanged();

private:
    QString m_text;
    QColor m_color = Qt::black;
    QColor m_background = Qt::white;
    int m_quietZone = 2;
    int m_size = 0;
    std::vector<bool> m_modules;
};
