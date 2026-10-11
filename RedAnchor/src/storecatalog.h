#pragma once

#include <QHash>
#include <QJsonObject>
#include <QList>
#include <QObject>
#include <QPair>
#include <QString>
#include <QStringList>
#include <QUrl>
#include <QVariantList>
#include <QVariantMap>
#include <QtQml/qqmlregistration.h>

#include <functional>

class QJSEngine;
class QNetworkAccessManager;
class QQmlEngine;

// Витрина магазинов GOG и Epic Games Store: подборки, поиск и страница игры.
//
// Данные — из открытых API магазинов, вход не нужен (теми же адресами пользуются
// Heroic Games Launcher, Rare, legendary, Playnite):
//   GOG  — catalog.gog.com/v1/catalog (подборки, поиск, цены), api.gog.com/products/<id> (описание);
//   Epic — GraphQL searchStore на launcher.store.epicgames.com (запасной — store.epicgames.com),
//          freeGamesPromotions (бесплатные раздачи), store-content.ak.epicgames.com (описание).
// Цены — для России (RU, рубли); если магазин для России ничего не отдаёт — для США.
//
// Ответы кэшируются в ~/.cache/RedAnchor/RedAnchor Shell/storefront: свежие показываются
// без запроса, а без сети — последние сохранённые (с пометкой offline). Картинки витрины
// кэширует сетевой кэш движка QML (его ставит create()).
//
// Купить игру через API нельзя: покупка — на сайте магазина (StoreBrowserWindow.qml или
// QR-код для телефона). Купленные и установленные игры отмечаются по StoreManager.owned
// и GameLibrary (ownership()).
class StoreCatalog : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

    // Магазины витрины: ["gog", "epic"]
    Q_PROPERTY(QStringList stores READ stores CONSTANT)
    // { gog|epic: { loading, error, offline, rows: [{ key, title, items: [игра], loading }] } }
    // Игра — словарь, описанный в storecatalogparser.h
    Q_PROPERTY(QVariantMap home READ home NOTIFY homeChanged)
    // Поиск сразу по всем магазинам
    Q_PROPERTY(QString searchText READ searchText NOTIFY searchChanged)
    Q_PROPERTY(QVariantList searchResults READ searchResults NOTIFY searchChanged)
    Q_PROPERTY(bool searching READ searching NOTIFY searchChanged)
    Q_PROPERTY(QString searchError READ searchError NOTIFY searchChanged)
    // Открытая игра (дополняется описанием и скриншотами по мере загрузки)
    Q_PROPERTY(QVariantMap details READ details NOTIFY detailsChanged)
    Q_PROPERTY(bool detailsLoading READ detailsLoading NOTIFY detailsChanged)
    // Меняется, когда меняются купленные или установленные игры: QML перечитывает ownership()
    Q_PROPERTY(int revision READ revision NOTIFY ownershipChanged)

public:
    static StoreCatalog *instance();
    static StoreCatalog *create(QQmlEngine *engine, QJSEngine *);

    QStringList stores() const;
    QVariantMap home() const { return m_home; }
    QString searchText() const { return m_searchText; }
    QVariantList searchResults() const { return m_searchResults; }
    bool searching() const { return m_searchPending > 0; }
    QString searchError() const { return m_searchError; }
    QVariantMap details() const { return m_details; }
    bool detailsLoading() const { return m_detailsLoading; }
    int revision() const { return m_revision; }

    // Подборки магазина (force — не брать свежий кэш)
    Q_INVOKABLE void load(const QString &store, bool force = false);
    Q_INVOKABLE void search(const QString &text);
    Q_INVOKABLE void clearSearch();
    // Открыть страницу игры: сразу показываем то, что уже известно из подборки, и догружаем описание
    Q_INVOKABLE void openDetails(const QVariantMap &item);
    Q_INVOKABLE void closeDetails();

    // { owned, installed, status ("" | queued | downloading | failed), progress, appId, gameId }
    Q_INVOKABLE QVariantMap ownership(const QVariantMap &item);
    // Сколько игр куплено в магазине (по StoreManager.owned)
    Q_INVOKABLE int ownedCount(const QString &store) const;
    // Вошёл ли пользователь в магазин в оболочке (тогда покупки попадают в библиотеку)
    Q_INVOKABLE bool signedIn(const QString &store) const;
    // Обновить купленные игры после покупки на сайте (если в магазин выполнен вход)
    Q_INVOKABLE void refreshOwned(const QString &store);
    // Официальная страница активации ключа
    Q_INVOKABLE QString redeemUrl(const QString &store) const;
    // Главная страница магазина (для «Открыть сайт»)
    Q_INVOKABLE QString storeUrl(const QString &store) const;

signals:
    void homeChanged();
    void searchChanged();
    void detailsChanged();
    void ownershipChanged();

private:
    explicit StoreCatalog(QObject *parent = nullptr);

    struct Request {
        QUrl url;
        QByteArray body;                               // непустое — POST с JSON
        QList<QPair<QByteArray, QByteArray>> headers;
    };
    // Ответ подошёл (иначе пробуем следующий запрос цепочки)
    using Accept = std::function<bool(const QJsonObject &)>;
    // ok — есть данные; stale — сеть недоступна, данные из старого кэша
    using Done = std::function<void(const QJsonObject &root, bool ok, bool stale)>;

    // Запросы цепочки пробуются по очереди (другой адрес, другая страна);
    // результат кэшируется под ключом первого запроса
    void fetch(const QList<Request> &chain, qint64 ttlSecs, bool force, const Accept &accept, const Done &done);
    void tryChain(const QList<Request> &chain, int index, const QString &cacheKey, const QJsonObject &cached,
                  const Accept &accept, const Done &done);

    QList<Request> gogCatalogChain(const QList<QPair<QString, QString>> &params) const;
    QList<Request> epicSearchChain(const QVariantMap &variables) const;
    QList<Request> epicFreeChain() const;

    void loadGog(bool force);
    void loadEpic(bool force);
    void setRow(const QString &store, const QString &key, const QVariantList &items, bool ok, bool stale);
    void beginHome(const QString &store, const QList<QPair<QString, QString>> &rows);
    void mergeSearch(int generation, const QString &store, const QVariantList &items, bool ok);

    void rebuildOwnedIndex();

    QNetworkAccessManager *m_network = nullptr;
    QString m_cacheDir;

    QVariantMap m_home;
    QHash<QString, int> m_homePending;

    QString m_searchText;
    QVariantList m_searchResults;
    QHash<QString, QVariantList> m_searchByStore;
    QString m_searchError;
    int m_searchPending = 0;
    int m_searchGeneration = 0;

    QVariantMap m_details;
    bool m_detailsLoading = false;
    int m_detailsGeneration = 0;

    int m_revision = 0;
    int m_indexRevision = -1;
    // Купленные игры: магазин → (appId или название для сравнения → запись StoreManager.owned)
    QHash<QString, QHash<QString, QVariantMap>> m_ownedById;
    QHash<QString, QHash<QString, QVariantMap>> m_ownedByTitle;
};
