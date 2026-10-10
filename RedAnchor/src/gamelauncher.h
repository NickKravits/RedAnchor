#pragma once

#include <QElapsedTimer>
#include <QList>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QVariantMap>
#include <QtQml/qqmlregistration.h>

class QProcess;
class QTimer;

// Запуск установленных игр через Proton (GE-Proton из «Настройки → Обновления»).
//   GOG   — gogdl launch с Proton в качестве обёртки;
//   Steam — исполняемый файл версии для Windows (параметры запуска — из данных приложения
//           Steam через redanchor-steam). Игры с защитой Steam без клиента Steam могут не
//           запуститься — клиент Steam в фоне появится позже.
// У каждой игры свой префикс Wine в профиле пользователя: profiles/<id>/prefixes/<магазин>_<appId>.
// Вывод игры пишется в profiles/<id>/logs/<магазин>_<appId>.log.
class GameLauncher : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

    // "idle" | "starting" — экран запуска, ждём процесс игры | "running" — игра запущена | "failed"
    Q_PROPERTY(QString state READ state NOTIFY stateChanged)
    // Запуск идёт дольше обычного (экран запуска показывает подсказку)
    Q_PROPERTY(bool slow READ slow NOTIFY stateChanged)
    // Последние строки журнала игры — при ошибке
    Q_PROPERTY(QString details READ details NOTIFY stateChanged)
    // Запускаемая игра (запись GameLibrary)
    Q_PROPERTY(QVariantMap game READ game NOTIFY stateChanged)
    Q_PROPERTY(QString message READ message NOTIFY stateChanged)
    // Как игра получает геймпад: "native" — DualSense напрямую (игра его поддерживает),
    // "xbox" — виртуальный контроллер Xbox (система раскладки RedAnchor)
    Q_PROPERTY(QString inputMode READ inputMode NOTIFY stateChanged)

public:
    explicit GameLauncher(QObject *parent = nullptr);

    QString state() const { return m_state; }
    QVariantMap game() const { return m_game; }
    QString message() const { return m_message; }
    bool slow() const { return m_slow; }
    QString details() const { return m_details; }
    QString inputMode() const { return m_inputMode; }

    Q_INVOKABLE void launch(const QString &gameId);
    // Закрыть экран ошибки
    Q_INVOKABLE void dismiss();
    // Отменить запуск: завершить процессы игры
    Q_INVOKABLE void cancel();
    // Закрыть запущенную игру (оверлей → игра → «Закрыть игру»)
    Q_INVOKABLE void stop();
    // Закрыть и запустить заново
    Q_INVOKABLE void restart();

private:
    void setState(const QString &state, const QString &message = QString());
    QString protonPath() const;
    void start(const QString &program, const QStringList &args, const QString &workingDir, const QString &appIdForSteam);
    // Процессы самой игры (Wine), а не Proton/gogdl: по папке игры в командной строке
    QList<qint64> gamePids() const;
    void checkStarted();
    void fail(const QString &message);
    void killGame();

signals:
    void stateChanged();

private:

    QString m_state = QStringLiteral("idle");
    QVariantMap m_game;
    QString m_message;
    QString m_details;
    QString m_logFile;
    bool m_slow = false;
    bool m_sawGame = false;
    bool m_stopping = false;
    QString m_relaunch;     // id игры, которую запустить после закрытия (перезапуск)
    QString m_inputMode;
    QProcess *m_process = nullptr;
    QElapsedTimer m_started;
    QTimer *m_watch = nullptr;
};
