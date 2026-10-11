#pragma once

#include <QObject>
#include <QtQml/qqmlregistration.h>

class QProcess;
class QTimer;

// Громкость и питание для оверлея по кнопке PS.
//   Громкость — устройства вывода по умолчанию: через wpctl (PipeWire), если его нет — pactl.
//   Питание — через logind (как кнопка питания на консоли): перезагрузка и выключение
//   разрешены пользователю активной локальной сессии без пароля.
//   Выход из системы — оболочка завершается, сессия возвращает на экран входа.
class SystemControl : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

    // 0..100; -1 — громкость неизвестна (нет звукового сервера)
    Q_PROPERTY(int volume READ volume NOTIFY volumeChanged)
    Q_PROPERTY(bool muted READ muted NOTIFY volumeChanged)

public:
    explicit SystemControl(QObject *parent = nullptr);

    int volume() const { return m_volume; }
    bool muted() const { return m_muted; }

    // Перечитать громкость (оверлей открылся)
    Q_INVOKABLE void refreshVolume();
    Q_INVOKABLE void setVolume(int percent);
    Q_INVOKABLE void setMuted(bool muted);

    Q_INVOKABLE void logout();
    // Сброс к заводским настройкам (кроме обновлений): оболочка закрывается, сбрасывает
    // redanchor-reset, сессия запускает оболочку заново — с первичной настройкой
    Q_INVOKABLE void factoryReset();
    Q_INVOKABLE void reboot();
    Q_INVOKABLE void powerOff();

signals:
    void volumeChanged();

private:
    void applyVolume();
    void callLogind(const QString &method);

    int m_volume = -1;
    bool m_muted = false;
    int m_pendingVolume = -1;
    bool m_hasWpctl = false;
    QProcess *m_setter = nullptr;
    QTimer *m_applyTimer = nullptr;
};
