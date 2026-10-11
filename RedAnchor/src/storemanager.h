#pragma once

#include <QHash>
#include <QList>
#include <QObject>
#include <QProcessEnvironment>
#include <QSet>
#include <QString>
#include <QStringList>
#include <QUrl>
#include <QVariantList>
#include <QVariantMap>
#include <QtQml/qqmlregistration.h>

#include <functional>

class QJSEngine;
class QNetworkAccessManager;
class QProcess;
class QQmlEngine;

// Магазины игр: GOG (gogdl) и Epic Games Store (legendary).
//
// Инструменты общие для всех пользователей, лежат в ~/.local/share/RedAnchor/RedAnchor Shell/tools,
// их ставит ComponentsManager («Настройки → Обновления»).
// Всё остальное своё у каждого пользователя консоли (папка профиля):
//   stores/<магазин>/  — вход в магазин, кэш списка игр, настройки инструмента
//                        (у legendary — stores/epic/legendary: вход, installed.json, метаданные);
//   games/<магазин>/   — установленные игры.
//
// Вход в оба магазина — через страницу входа во встроенном браузере (WebLoginWindow.qml
// передаёт код авторизации): GOG — gogdl auth --code, Epic — legendary auth --code.
// Установленные игры Epic ведёт сам legendary (installed.json) — библиотека сверяется с ним.
//
// Поставленная на загрузку игра сразу появляется в библиотеке (GameLibrary::addPending).
// Загрузки идут по очереди, по одной игре.
class StoreManager : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

    // { gog|epic: { toolInstalled, tool (имя загрузчика), signedIn, account, avatar (URL), busy, message, listTitle } }
    Q_PROPERTY(QVariantMap stores READ stores NOTIFY storesChanged)
    // { gog|epic: [{ gameId, appId, title, installed, updateAvailable, status ("" | queued | downloading | failed),
    //               progress }] }
    Q_PROPERTY(QVariantMap owned READ owned NOTIFY ownedChanged)
    // [{ id, store, appId, title, status, progress, message }]
    Q_PROPERTY(QVariantList downloads READ downloads NOTIFY downloadsChanged)
    // Идущие и ждущие загрузки (значок в строке состояния)
    Q_PROPERTY(int activeDownloads READ activeDownloads NOTIFY downloadsChanged)
    // Прогресс текущей загрузки 0..1 (или -1)
    Q_PROPERTY(double downloadProgress READ downloadProgress NOTIFY downloadsChanged)
    Q_PROPERTY(QString downloadTitle READ downloadTitle NOTIFY downloadsChanged)

public:
    // Один экземпляр: им пользуются QML и запуск игр (GameLauncher)
    static StoreManager *instance();
    static StoreManager *create(QQmlEngine *, QJSEngine *);

    QVariantMap stores() const;
    QVariantMap owned() const;
    QVariantList downloads() const;
    int activeDownloads() const;
    double downloadProgress() const;
    QString downloadTitle() const;

    // Проверить, установлены ли инструменты магазинов (их ставит «Настройки → Обновления»)
    Q_INVOKABLE void refreshTools();
    // Страница входа для встроенного браузера
    Q_INVOKABLE QString loginUrl(const QString &store) const;
    // Код авторизации со страницы входа
    Q_INVOKABLE void completeWebLogin(const QString &store, const QString &code);
    Q_INVOKABLE void signOut(const QString &store);
    // Обновить список купленных игр
    Q_INVOKABLE void refresh(const QString &store);
    Q_INVOKABLE void install(const QString &store, const QString &appId, const QString &title);
    // Обновить установленную игру (Epic: legendary install --update-only) — через ту же очередь загрузок
    Q_INVOKABLE void update(const QString &gameId);
    Q_INVOKABLE void cancel(const QString &gameId);
    Q_INVOKABLE void uninstall(const QString &gameId);

    // Поддержка геймпада по данным магазина:
    //   { checked, failed, controller (любой геймпад), ps5 (DualSense), ps4 (DualShock 4) }
    // Пока сведений нет — запрашивает их; по готовности придёт compatibilityChanged(gameId).
    Q_INVOKABLE QVariantMap compatibility(const QString &store, const QString &appId);

    // Пути для запуска игр (GameLauncher)
    QString gogdlExecutable() const { return gogdlPath(); }
    QString gogAuthConfig() const { return gogAuthFile(); }
    QString legendaryExecutable() const { return legendaryPath(); }
    QProcessEnvironment storeEnvironment(const QString &store) const { return environment(store); }

signals:
    void storesChanged();
    void ownedChanged();
    void downloadsChanged();
    void gameInstalled(const QString &title);
    void gameRemoved(const QString &title);
    void compatibilityChanged(const QString &gameId);

private:
    explicit StoreManager(QObject *parent = nullptr);

    QString compatFile() const;
    void saveCompatibility() const;

    struct OwnedGame {
        QString appId;
        QString title;
        QString type;
    };
    struct Store {
        QString key;
        bool toolInstalled = false;
        bool toolBusy = false;
        double toolProgress = -1;
        QString toolMessage;
        bool signedIn = false;
        QString account;
        bool busy = false;
        QString message;
        QString listTitle;
        QList<OwnedGame> owned;
        QSet<QString> updates;   // appId установленных игр, для которых вышло обновление
    };
    struct Download {
        QString store;
        QString appId;
        QString title;
        QString status = QStringLiteral("queued");
        double progress = -1;
        QString message;
        bool update = false;     // обновление установленной игры
        QString id() const { return store + QLatin1Char(':') + appId; }
    };

    using LineHandler = std::function<void(const QString &)>;
    using DoneHandler = std::function<void(int exitCode, const QString &stdoutText)>;

    Store &store(const QString &key);
    const Store *findStore(const QString &key) const;
    QString toolsDir() const;
    QString storeDir(const QString &key) const;
    QString gogdlPath() const;
    QString gogAuthFile() const;
    QString legendaryPath() const;
    QString legendaryConfigDir() const;
    QString toolPath(const QString &key) const;
    QProcessEnvironment environment(const QString &key) const;

    // pty — запустить в псевдотерминале (script из util-linux): инструмент выводит
    // запросы ввода и прогресс сразу, как в терминале
    QProcess *run(const QString &program, const QStringList &args, const QProcessEnvironment &env,
                  LineHandler onLine, DoneHandler onDone, bool pty = false);
    void downloadFile(const QUrl &url, const QString &path, std::function<void(double)> onProgress,
                      std::function<void(bool ok, const QString &error)> onDone);
    void gogToken(std::function<void(const QString &token)> onToken);

    void reload();
    void loadOwned(const QString &key);
    void saveOwned(const QString &key) const;
    void setBusy(const QString &key, bool busy, const QString &message = QString());
    void refreshGog();
    void fetchGogProducts(QStringList ids, int index);
    void refreshEpic();
    void loadEpicUpdates();
    void syncEpicLibrary();
    void startGogDownload(const QString &appId, const QString &title);
    void startEpicDownload(const QString &appId, const QString &title, bool update);
    void stopActiveProcess();

    Download *findDownload(const QString &id);
    void processDownloads();
    void setDownloadProgress(double progress, const QString &message);
    void finishDownload(bool ok, const QString &message = QString());
    void cancelAllDownloads();

    QHash<QString, Store> m_stores;
    QList<Download> m_downloads;
    QProcess *m_activeProcess = nullptr;
    QString m_activeDownload;
    QNetworkAccessManager *m_network = nullptr;
    QHash<QString, QVariantMap> m_compat;   // gameId -> поддержка геймпада
    QSet<QString> m_compatPending;
};
