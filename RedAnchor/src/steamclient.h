#pragma once

#include <QElapsedTimer>
#include <QHash>
#include <QList>
#include <QMap>
#include <QObject>
#include <QProcessEnvironment>
#include <QSet>
#include <QString>
#include <QStringList>

#include <functional>

class QProcess;
class QTimer;

// Официальный клиент Steam — по одному аккаунту Steam на пользователя консоли.
//
// У каждого пользователя консоли свой каталог Steam: клиент запускается с HOME =
// ~/.local/share/redanchor/steam/<id профиля>, поэтому вход, библиотека и игры не смешиваются.
// Клиент обновляется сам.
//
// Вход — один раз: оболочка открывает Steam Big Picture (окно Steam встаёт поверх оболочки
// в gamescope, как игра), пользователь входит геймпадом; как только вход удался, Big Picture
// закрывается, и дальше клиент работает в фоне (-silent) и сам входит при запуске.
//
// Библиотека читается из файлов клиента: купленные игры — appcache/packageinfo.vdf
// (пакеты лицензий) и appcache/appinfo.vdf (названия и типы приложений), установленные —
// steamapps/appmanifest_*.acf во всех папках библиотек.
//
// Установка из оболочки: в папку библиотеки кладётся appmanifest игры с флагом «нужно
// обновление» и высоким приоритетом, затем клиент перезапускается в фоне — и сам качает игру.
// Прогресс берётся из того же appmanifest. Удаление и отмена — тоже через перезапуск клиента
// (файлы библиотеки клиент переписывает при выходе, менять их можно только пока он закрыт).
//
// Запуск — steam://rungameid/<appid>: игры идут через Proton (Steam Play для всех игр Windows,
// Proton Experimental — его и среду выполнения клиент ставит и обновляет сам), с защитой от
// копирования, облачными сохранениями и сетевой игрой, как в обычном Steam.
// У каждой установленной игры в параметрах запуска стоит обёртка оболочки: перед запуском
// оболочка кладёт рядом настройки ввода игры (DualSense напрямую или контроллер Xbox).
class SteamClient : public QObject
{
    Q_OBJECT

public:
    struct OwnedGame {
        QString appId;
        QString title;
    };
    struct InstalledGame {
        QString appId;
        QString title;
        QString path;
        qint64 sizeBytes = 0;
    };
    struct Download {
        QString appId;
        QString title;
        QString status;     // "queued" | "downloading" | "failed"
        double progress = -1;
        QString message;
    };

    explicit SteamClient(QObject *parent = nullptr);

    // HOME клиента Steam для профиля (вне каталога профиля — см. homeFor)
    static QString homePath(const QString &profileDir);
    ~SteamClient() override;

    // Каталог профиля пользователя консоли (пусто — пользователя нет). Прежний клиент закрывается.
    void setProfile(const QString &profileDir);

    bool installed() const;
    bool running() const;
    bool signedIn() const { return !m_accountName.isEmpty(); }
    // "" — ничего не происходит | "starting" — клиент запускается или обновляется |
    // "signin" — открыт Big Picture для входа (единственный случай, когда Steam виден) |
    // "stopping" — клиент закрывается
    QString phase() const { return m_phase; }
    double phaseProgress() const { return m_phaseProgress; }
    // Вход в аккаунт сейчас действует (клиент запущен и вошёл)
    bool online() const { return m_online; }
    QString account() const { return m_persona.isEmpty() ? m_accountName : m_persona; }
    QString avatar() const { return m_avatar; }
    bool busy() const { return m_libraryBusy; }
    QString message() const { return m_message; }
    QList<OwnedGame> owned() const { return m_owned; }
    QList<InstalledGame> installedGames() const { return m_installed; }
    QList<Download> downloads() const;
    QString installPath(const QString &appId) const;

    // Войти: Big Picture поверх оболочки; закроется сам после входа. Больше Steam
    // никогда не показывается — всё остальное делает оболочка, клиент работает молча
    void signIn();
    void cancelSignIn();
    void signOut();
    void refreshLibrary();
    void install(const QString &appId, const QString &title);
    void cancel(const QString &appId);
    void uninstall(const QString &appId);

    // Запустить игру. env — настройки ввода для обёртки запуска.
    // done(ok, ошибка) — когда клиент принял команду запуска (или не смог)
    void launch(const QString &appId, const QMap<QString, QString> &env,
                std::function<void(bool ok, const QString &error)> done);
    // Процессы игры (reaper Steam и всё, что запущено с SteamAppId=<appid>)
    static QList<qint64> gameProcesses(const QString &appId);

    // Закрыть клиент при выходе из оболочки
    void shutdown();

signals:
    void stateChanged();
    void libraryChanged();
    void downloadsChanged();
    void installedChanged();
    void gameInstalled(const QString &title);
    void gameRemoved(const QString &title);

private:
    enum class Mode { Silent, BigPicture };

    QString root() const;           // каталог клиента внутри его HOME
    static QString homeFor(const QString &profileDir);   // с переносом из прежнего места
    QString steamExecutable() const;
    QProcessEnvironment environment() const;
    QString wrapperPath() const;
    QString envDir() const;
    QString shimDir() const;
    QStringList libraryFolders() const;
    qint64 pidFromFile() const;
    quint32 activeUser() const;

    void setPhase(const QString &phase, double progress = -1);
    void setMessage(const QString &message);
    void readAccount();
    void startClient(Mode mode);
    bool needsBootstrap() const;
    void finishBootstrap();
    void queueManifest(const QString &appId);
    void stopClient(std::function<void()> then);
    void restartSilent();
    void scheduleRestart();
    void runCommand(const QStringList &args);
    void applyConfig();
    void writeWrapper();
    void writeInstallerShim();
    static void writeScript(const QString &path, const QByteArray &script);
    void poll();
    void scanManifests();
    void onSignedIn();
    void runOffline(std::function<void()> op);

    QString m_profile;
    QString m_home;
    QProcess *m_process = nullptr;
    QTimer *m_poll = nullptr;
    QTimer *m_restartTimer = nullptr;
    QString m_phase;
    double m_phaseProgress = -1;
    QString m_message;
    bool m_online = false;
    bool m_libraryBusy = false;
    int m_generation = 0;

    QString m_accountName;
    QString m_persona;
    QString m_avatar;
    QString m_steamId;

    QList<OwnedGame> m_owned;
    QList<InstalledGame> m_installed;
    QHash<QString, QString> m_installDirs;      // appid → папка установки из appinfo
    QHash<QString, QString> m_manifestDirs;     // appid → папка библиотеки (…/steamapps)

    struct Request {
        QString title;
        qint64 since = 0;       // мс от m_clock, когда клиент запустился с этим запросом
        int retries = 0;        // сколько раз клиент тихо перезапускали, потому что он не начал загрузку
        bool failed = false;
    };
    QMap<QString, Request> m_requests;          // поставленные из оболочки загрузки
    QMap<QString, Download> m_active;           // загрузки по данным appmanifest
    QSet<QString> m_removing;                   // удаляются (ждут закрытия клиента)
    QElapsedTimer m_clock;
    qint64 m_runningSince = -1;                 // мс от m_clock, с какого момента клиент запущен
    qint64 m_onlineWaitSince = 0;               // мс от m_clock, с какого момента ждём входа
    qint64 m_signInStarted = 0;                 // секунды Unix, когда открыли вход
    bool m_signInDone = false;

    QList<std::function<void()>> m_offline;     // действия, пока клиент закрыт
    QList<std::function<void()>> m_afterStop;
    QList<std::function<void(bool)>> m_onOnline;
    bool m_stopping = false;
    bool m_bootstrapping = false;               // первая загрузка клиента на невидимом экране
    Mode m_bootstrapMode = Mode::Silent;
    qint64 m_bootstrapReadySince = -1;
    bool m_wantRunning = false;
};
