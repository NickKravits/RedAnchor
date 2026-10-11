#include "systemcontrol.h"

#include <QCoreApplication>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QProcess>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QTimer>

#include <cmath>

SystemControl::SystemControl(QObject *parent)
    : QObject(parent)
    , m_hasWpctl(!QStandardPaths::findExecutable(QStringLiteral("wpctl")).isEmpty())
    , m_applyTimer(new QTimer(this))
{
    // Ползунок двигают быстро — отправляем только последнее значение
    m_applyTimer->setSingleShot(true);
    m_applyTimer->setInterval(60);
    connect(m_applyTimer, &QTimer::timeout, this, &SystemControl::applyVolume);
    refreshVolume();
}

void SystemControl::refreshVolume()
{
    auto *process = new QProcess(this);
    connect(process, &QProcess::finished, this, [this, process](int exitCode) {
        process->deleteLater();
        const QString out = QString::fromUtf8(process->readAllStandardOutput());
        int volume = -1;
        bool muted = false;
        if (exitCode == 0 && m_hasWpctl) {
            // «Volume: 0.40» или «Volume: 0.40 [MUTED]»
            const QRegularExpressionMatch m = QRegularExpression(QStringLiteral("Volume:\\s*([0-9.]+)")).match(out);
            if (m.hasMatch())
                volume = int(std::lround(m.captured(1).toDouble() * 100));
            muted = out.contains(QLatin1String("MUTED"));
        } else if (exitCode == 0) {
            // pactl: «Volume: front-left: 26214 /  40% / …» и «Mute: yes»
            const QRegularExpressionMatch m = QRegularExpression(QStringLiteral("(\\d+)%")).match(out);
            if (m.hasMatch())
                volume = m.captured(1).toInt();
            muted = out.contains(QLatin1String("Mute: yes"));
        }
        if (m_pendingVolume >= 0)
            return;   // пользователь уже двигает ползунок
        if (volume != m_volume || muted != m_muted) {
            m_volume = qBound(-1, volume, 100);
            m_muted = muted;
            emit volumeChanged();
        }
    });
    if (m_hasWpctl) {
        process->start(QStringLiteral("wpctl"), {QStringLiteral("get-volume"), QStringLiteral("@DEFAULT_AUDIO_SINK@")});
    } else {
        // Громкость и выключение звука — одной командой оболочки
        process->start(QStringLiteral("sh"), {QStringLiteral("-c"),
                                              QStringLiteral("pactl get-sink-volume @DEFAULT_SINK@ && pactl get-sink-mute @DEFAULT_SINK@")});
    }
}

void SystemControl::setVolume(int percent)
{
    percent = qBound(0, percent, 100);
    if (percent == m_volume && !m_muted)
        return;
    m_volume = percent;
    m_muted = false;
    m_pendingVolume = percent;
    emit volumeChanged();
    if (!m_setter)
        m_applyTimer->start();
}

void SystemControl::applyVolume()
{
    if (m_pendingVolume < 0)
        return;
    const int percent = m_pendingVolume;
    m_setter = new QProcess(this);
    connect(m_setter, &QProcess::finished, this, [this, percent] {
        m_setter->deleteLater();
        m_setter = nullptr;
        if (m_pendingVolume == percent)
            m_pendingVolume = -1;
        else
            m_applyTimer->start();   // пока применяли, ползунок сдвинули ещё
    });
    if (m_hasWpctl) {
        // Звук выключали — громкость его возвращает, как на консоли
        m_setter->start(QStringLiteral("sh"), {QStringLiteral("-c"),
                                               QStringLiteral("wpctl set-mute @DEFAULT_AUDIO_SINK@ 0; wpctl set-volume -l 1.0 @DEFAULT_AUDIO_SINK@ %1")
                                                   .arg(percent / 100.0, 0, 'f', 2)});
    } else {
        m_setter->start(QStringLiteral("sh"), {QStringLiteral("-c"),
                                               QStringLiteral("pactl set-sink-mute @DEFAULT_SINK@ 0; pactl set-sink-volume @DEFAULT_SINK@ %1%").arg(percent)});
    }
}

void SystemControl::setMuted(bool muted)
{
    if (muted == m_muted)
        return;
    m_muted = muted;
    emit volumeChanged();
    const QString value = muted ? QStringLiteral("1") : QStringLiteral("0");
    if (m_hasWpctl)
        QProcess::startDetached(QStringLiteral("wpctl"), {QStringLiteral("set-mute"), QStringLiteral("@DEFAULT_AUDIO_SINK@"), value});
    else
        QProcess::startDetached(QStringLiteral("pactl"), {QStringLiteral("set-sink-mute"), QStringLiteral("@DEFAULT_SINK@"), value});
}

void SystemControl::factoryReset()
{
    qInfo("Factory reset requested");
    if (!qEnvironmentVariableIsEmpty("REDANCHOR_SESSION_COMPOSITOR")) {
        // Код 42 — redanchor-session сам выполнит сброс, когда оболочка закроется
        QCoreApplication::exit(42);
        return;
    }
    // Оболочка запущена не из сессии: сбрасываем, как только она закроется
    const QString script = QStandardPaths::findExecutable(QStringLiteral("redanchor-reset"));
    if (script.isEmpty()) {
        qWarning("Factory reset: redanchor-reset not found");
        return;
    }
    QProcess::startDetached(QStringLiteral("sh"), {QStringLiteral("-c"),
        QStringLiteral("while pgrep -u \"$(id -u)\" -x redanchor-shell >/dev/null; do sleep 0.5; done; exec \"$0\" --yes"),
        script});
    QCoreApplication::exit(0);
}

void SystemControl::logout()
{
    qInfo("Logout requested");
    // Код 0 — redanchor-session считает сессию законченной и возвращает на экран входа
    QCoreApplication::exit(0);
}

void SystemControl::callLogind(const QString &method)
{
    qInfo("Power: %s", qPrintable(method));
    QDBusMessage message = QDBusMessage::createMethodCall(QStringLiteral("org.freedesktop.login1"),
                                                          QStringLiteral("/org/freedesktop/login1"),
                                                          QStringLiteral("org.freedesktop.login1.Manager"), method);
    message << false;   // interactive: без запроса пароля
    auto *watcher = new QDBusPendingCallWatcher(QDBusConnection::systemBus().asyncCall(message), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this, [method](QDBusPendingCallWatcher *w) {
        w->deleteLater();
        const QDBusPendingReply<> reply = *w;
        if (reply.isError())
            qWarning("Power %s failed: %s", qPrintable(method), qPrintable(reply.error().message()));
    });
}

void SystemControl::reboot()
{
    callLogind(QStringLiteral("Reboot"));
}

void SystemControl::powerOff()
{
    callLogind(QStringLiteral("PowerOff"));
}
