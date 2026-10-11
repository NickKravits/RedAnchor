#include "storecatalog.h"

#include "gamelibrary.h"
#include "storecatalogparser.h"
#include "storemanager.h"

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QNetworkAccessManager>
#include <QNetworkDiskCache>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QQmlEngine>
#include <QQmlNetworkAccessManagerFactory>
#include <QStandardPaths>
#include <QTimeZone>
#include <QTimer>
#include <QUrlQuery>

#include <algorithm>

namespace {

const QString kGogCatalog = QStringLiteral("https://catalog.gog.com/v1/catalog");
// Витрина Epic: тот же GraphQL, что у клиента Epic (legendary: EPCAPI._store_gql_host,
// Heroic). Сайт store.epicgames.com/graphql закрыт Cloudflare для «не браузеров» —
// он запасной.
const QString kEpicGraphql = QStringLiteral("https://launcher.store.epicgames.com/graphql");
const QString kEpicGraphqlSite = QStringLiteral("https://store.epicgames.com/graphql");
const QString kEpicFree = QStringLiteral("https://store-site-backend-static-ipv4.ak.epicgames.com/freeGamesPromotions");
const QString kEpicContent = QStringLiteral("https://store-content.ak.epicgames.com/api/%1/content/products/%2");
// User-Agent клиента Epic (legendary/api/egs.py, _store_user_agent)
const QByteArray kEpicLauncherAgent = "EpicGamesLauncher/14.0.8-22004686+++Portal+Release-Live";
const QByteArray kBrowserAgent = "Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/124.0 Safari/537.36";

// Сколько хранить ответы: подборки — полчаса, поиск — десять минут, описание игры — сутки
constexpr qint64 HomeTtl = 30 * 60;
constexpr qint64 SearchTtl = 10 * 60;
constexpr qint64 DetailsTtl = 24 * 3600;
constexpr int TimeoutMs = 15000;
// Сетевой кэш картинок витрины
constexpr qint64 ImageCacheBytes = 256ll * 1024 * 1024;

// Запрос searchStore: поля — как в запросах Rare (rare/components/tabs/store/constants.py)
// и epicstore_api (STORE_QUERY); onSale, freeGame, releaseDate — фильтры страницы «Каталог»
const char kEpicSearchQuery[] = R"(query searchStoreQuery($allowCountries: String, $category: String, $count: Int, $country: String!, $keywords: String, $locale: String, $sortBy: String, $sortDir: String, $start: Int, $releaseDate: String, $freeGame: Boolean, $onSale: Boolean, $withPrice: Boolean = true) {
  Catalog {
    searchStore(allowCountries: $allowCountries, category: $category, count: $count, country: $country, keywords: $keywords, locale: $locale, sortBy: $sortBy, sortDir: $sortDir, releaseDate: $releaseDate, start: $start, freeGame: $freeGame, onSale: $onSale) {
      elements {
        title id namespace description effectiveDate releaseDate offerType productSlug urlSlug
        keyImages { type url }
        seller { id name }
        items { id namespace }
        tags { id name groupName }
        customAttributes { key value }
        categories { path }
        catalogNs { mappings(pageType: "productHome") { pageSlug pageType } }
        offerMappings { pageSlug pageType }
        price(country: $country) @include(if: $withPrice) {
          totalPrice {
            discountPrice originalPrice voucherDiscount discount currencyCode
            currencyInfo { decimals }
            fmtPrice(locale: $locale) { originalPrice discountPrice intermediatePrice }
          }
        }
      }
      paging { count total }
    }
  }
})";

// Регион цен: сначала Россия, затем США (как Heroic: нет скидок для страны — берёт US/USD)
struct Region {
    QString country;
    QString currency;
    QString gogLocale;
    QString epicLocale;
};
const QList<Region> kRegions = {
    {QStringLiteral("RU"), QStringLiteral("RUB"), QStringLiteral("ru-RU"), QStringLiteral("ru")},
    {QStringLiteral("US"), QStringLiteral("USD"), QStringLiteral("en-US"), QStringLiteral("en-US")},
};

QJsonObject epicElementsOk(const QJsonObject &root, bool *ok)
{
    const QJsonArray elements = root.value(QStringLiteral("data")).toObject().value(QStringLiteral("Catalog")).toObject()
                                    .value(QStringLiteral("searchStore")).toObject().value(QStringLiteral("elements")).toArray();
    *ok = !elements.isEmpty() && StoreCatalogParser::epicErrors(root).isEmpty();
    return root;
}

QVariantMap rowMap(const QString &key, const QString &title)
{
    return {{QStringLiteral("key"), key}, {QStringLiteral("title"), title},
            {QStringLiteral("items"), QVariantList()}, {QStringLiteral("loading"), true}};
}

// Сетевой доступ движка QML с дисковым кэшем: обложки и скриншоты витрины не качаются
// заново при каждом открытии. Вызывается из потоков загрузки картинок — только создаёт объекты.
class CachedNetworkFactory : public QQmlNetworkAccessManagerFactory
{
public:
    QNetworkAccessManager *create(QObject *parent) override
    {
        auto *manager = new QNetworkAccessManager(parent);
        auto *cache = new QNetworkDiskCache(manager);
        cache->setCacheDirectory(QStandardPaths::writableLocation(QStandardPaths::CacheLocation) + QStringLiteral("/storefront/images"));
        cache->setMaximumCacheSize(ImageCacheBytes);
        manager->setCache(cache);
        manager->setTransferTimeout(TimeoutMs);
        return manager;
    }
};

} // namespace

StoreCatalog *StoreCatalog::instance()
{
    static StoreCatalog *catalog = new StoreCatalog(qApp);
    return catalog;
}

StoreCatalog *StoreCatalog::create(QQmlEngine *engine, QJSEngine *)
{
    StoreCatalog *catalog = instance();
    QJSEngine::setObjectOwnership(catalog, QJSEngine::CppOwnership);
    // Кэш картинок для Image { source: "https://…" }. Ставим, только если своего у движка нет;
    // действует на сетевые объекты, которые движок создаст после этого.
    if (engine && !engine->networkAccessManagerFactory()) {
        static CachedNetworkFactory factory;
        engine->setNetworkAccessManagerFactory(&factory);
    }
    return catalog;
}

StoreCatalog::StoreCatalog(QObject *parent)
    : QObject(parent)
    , m_network(new QNetworkAccessManager(this))
    , m_cacheDir(QStandardPaths::writableLocation(QStandardPaths::CacheLocation) + QStringLiteral("/storefront"))
{
    m_network->setTransferTimeout(TimeoutMs);
    for (const QString &store : stores()) {
        m_home.insert(store, QVariantMap{{QStringLiteral("loading"), false}, {QStringLiteral("error"), QString()},
                                         {QStringLiteral("offline"), false}, {QStringLiteral("rows"), QVariantList()}});
    }
    // Купили или установили игру — отметки на витрине обновляются
    const auto bump = [this] {
        ++m_revision;
        emit ownershipChanged();
    };
    connect(StoreManager::instance(), &StoreManager::ownedChanged, this, bump);
    connect(GameLibrary::instance(), &GameLibrary::gamesChanged, this, bump);
}

QStringList StoreCatalog::stores() const
{
    return {QStringLiteral("gog"), QStringLiteral("epic")};
}

// ---------- Сеть и кэш ----------

void StoreCatalog::fetch(const QList<Request> &chain, qint64 ttlSecs, bool force, const Accept &accept, const Done &done)
{
    if (chain.isEmpty()) {
        done({}, false, false);
        return;
    }
    const Request &first = chain.first();
    const QString cacheKey = QString::fromLatin1(
        QCryptographicHash::hash(first.url.toEncoded() + '\n' + first.body, QCryptographicHash::Sha1).toHex());

    QJsonObject cached;
    QFile file(m_cacheDir + QLatin1Char('/') + cacheKey + QStringLiteral(".json"));
    if (file.open(QIODevice::ReadOnly))
        cached = QJsonDocument::fromJson(file.readAll()).object();
    const qint64 age = QDateTime::currentSecsSinceEpoch() - qint64(cached.value(QStringLiteral("time")).toDouble());
    if (!force && !cached.isEmpty() && age >= 0 && age < ttlSecs) {
        const QJsonObject data = cached.value(QStringLiteral("data")).toObject();
        // Ответ — всегда после возврата из вызова, как и из сети
        QTimer::singleShot(0, this, [done, data] { done(data, true, false); });
        return;
    }
    tryChain(chain, 0, cacheKey, cached.value(QStringLiteral("data")).toObject(), accept, done);
}

void StoreCatalog::tryChain(const QList<Request> &chain, int index, const QString &cacheKey, const QJsonObject &cached,
                            const Accept &accept, const Done &done)
{
    if (index >= chain.size()) {
        // Ни один адрес не ответил — показываем сохранённое ранее
        if (!cached.isEmpty())
            done(cached, true, true);
        else
            done({}, false, false);
        return;
    }
    const Request &r = chain.at(index);
    QNetworkRequest req(r.url);
    req.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("RedAnchor-Shell"));
    req.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
    for (const auto &header : r.headers)
        req.setRawHeader(header.first, header.second);
    QNetworkReply *reply;
    if (r.body.isEmpty()) {
        reply = m_network->get(req);
    } else {
        req.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
        reply = m_network->post(req, r.body);
    }
    connect(reply, &QNetworkReply::finished, this, [this, reply, r, chain, index, cacheKey, cached, accept, done] {
        reply->deleteLater();
        const QByteArray body = reply->readAll();
        const QJsonObject root = QJsonDocument::fromJson(body).object();
        if (reply->error() != QNetworkReply::NoError || root.isEmpty() || !accept(root)) {
            qInfo("storefront: %s — %s", qPrintable(r.url.host() + r.url.path()),
                  reply->error() != QNetworkReply::NoError ? qPrintable(reply->errorString()) : "пустой ответ");
            tryChain(chain, index + 1, cacheKey, cached, accept, done);
            return;
        }
        QDir().mkpath(m_cacheDir);
        QFile file(m_cacheDir + QLatin1Char('/') + cacheKey + QStringLiteral(".json"));
        if (file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            file.write(QJsonDocument(QJsonObject{{QStringLiteral("time"), double(QDateTime::currentSecsSinceEpoch())},
                                                 {QStringLiteral("data"), root}}).toJson(QJsonDocument::Compact));
        }
        done(root, true, false);
    });
}

QList<StoreCatalog::Request> StoreCatalog::gogCatalogChain(const QList<QPair<QString, QString>> &params) const
{
    QList<Request> chain;
    for (const Region &region : kRegions) {
        QUrl url(kGogCatalog);
        QUrlQuery query;
        for (const auto &p : params)
            query.addQueryItem(p.first, p.second);
        query.addQueryItem(QStringLiteral("countryCode"), region.country);
        query.addQueryItem(QStringLiteral("currencyCode"), region.currency);
        query.addQueryItem(QStringLiteral("locale"), region.gogLocale);
        url.setQuery(query);
        chain.append(Request{url, {}, {}});
    }
    return chain;
}

QList<StoreCatalog::Request> StoreCatalog::epicSearchChain(const QVariantMap &variables) const
{
    QList<Request> chain;
    for (const Region &region : kRegions) {
        QVariantMap vars = variables;
        vars.insert(QStringLiteral("country"), region.country);
        vars.insert(QStringLiteral("allowCountries"), region.country);
        vars.insert(QStringLiteral("locale"), region.epicLocale);
        const QByteArray body = QJsonDocument(QJsonObject{
            {QStringLiteral("query"), QString::fromUtf8(kEpicSearchQuery)},
            {QStringLiteral("variables"), QJsonObject::fromVariantMap(vars)},
        }).toJson(QJsonDocument::Compact);
        chain.append(Request{QUrl(kEpicGraphql), body, {{"User-Agent", kEpicLauncherAgent}}});
        chain.append(Request{QUrl(kEpicGraphqlSite), body,
                             {{"User-Agent", kBrowserAgent}, {"Origin", "https://store.epicgames.com"}}});
    }
    return chain;
}

QList<StoreCatalog::Request> StoreCatalog::epicFreeChain() const
{
    QList<Request> chain;
    for (const Region &region : kRegions) {
        QUrl url(kEpicFree);
        QUrlQuery query;
        query.addQueryItem(QStringLiteral("locale"), region.epicLocale);
        query.addQueryItem(QStringLiteral("country"), region.country);
        query.addQueryItem(QStringLiteral("allowCountries"), region.country);
        url.setQuery(query);
        chain.append(Request{url, {}, {}});
    }
    return chain;
}

// ---------- Подборки ----------

void StoreCatalog::beginHome(const QString &store, const QList<QPair<QString, QString>> &rows)
{
    QVariantList list;
    // Уже показанные подборки оставляем, пока грузятся новые
    const QVariantList old = m_home.value(store).toMap().value(QStringLiteral("rows")).toList();
    for (const auto &row : rows) {
        QVariantMap map = rowMap(row.first, row.second);
        for (const QVariant &o : old) {
            const QVariantMap prev = o.toMap();
            if (prev.value(QStringLiteral("key")) == row.first)
                map.insert(QStringLiteral("items"), prev.value(QStringLiteral("items")));
        }
        list.append(map);
    }
    m_homePending.insert(store, int(rows.size()));
    m_home.insert(store, QVariantMap{{QStringLiteral("loading"), true}, {QStringLiteral("error"), QString()},
                                     {QStringLiteral("offline"), false}, {QStringLiteral("rows"), list}});
    emit homeChanged();
}

void StoreCatalog::setRow(const QString &store, const QString &key, const QVariantList &items, bool ok, bool stale)
{
    QVariantMap state = m_home.value(store).toMap();
    QVariantList rows = state.value(QStringLiteral("rows")).toList();
    for (QVariant &r : rows) {
        QVariantMap row = r.toMap();
        if (row.value(QStringLiteral("key")) != key)
            continue;
        if (ok)
            row.insert(QStringLiteral("items"), items);
        row.insert(QStringLiteral("loading"), false);
        r = row;
    }
    state.insert(QStringLiteral("rows"), rows);
    if (stale)
        state.insert(QStringLiteral("offline"), true);
    const int pending = m_homePending.value(store) - 1;
    m_homePending.insert(store, pending);
    if (pending <= 0) {
        state.insert(QStringLiteral("loading"), false);
        bool empty = true;
        for (const QVariant &r : std::as_const(rows)) {
            if (!r.toMap().value(QStringLiteral("items")).toList().isEmpty())
                empty = false;
        }
        state.insert(QStringLiteral("error"), empty ? tr("Магазин недоступен. Проверьте подключение к интернету.") : QString());
    }
    m_home.insert(store, state);
    emit homeChanged();
}

void StoreCatalog::load(const QString &store, bool force)
{
    if (m_homePending.value(store) > 0)
        return;
    if (store == QLatin1String("gog"))
        loadGog(force);
    else if (store == QLatin1String("epic"))
        loadEpic(force);
}

void StoreCatalog::loadGog(bool force)
{
    const QString store = QStringLiteral("gog");
    beginHome(store, {{QStringLiteral("featured"), tr("Популярное")}, {QStringLiteral("discounts"), tr("Скидки")},
                      {QStringLiteral("new"), tr("Новинки")}, {QStringLiteral("free"), tr("Бесплатно")}});
    const auto hasProducts = [](const QJsonObject &root) {
        return !root.value(QStringLiteral("products")).toArray().isEmpty();
    };
    using Params = QList<QPair<QString, QString>>;
    // Параметры — как у Heroic (backend/discounts) и страницы каталога gog.com
    const QList<QPair<QString, Params>> rows = {
        {QStringLiteral("featured"), {{QStringLiteral("limit"), QStringLiteral("10")}, {QStringLiteral("order"), QStringLiteral("desc:trending")},
                                      {QStringLiteral("productType"), QStringLiteral("in:game,pack")}}},
        {QStringLiteral("discounts"), {{QStringLiteral("limit"), QStringLiteral("24")}, {QStringLiteral("order"), QStringLiteral("desc:trending")},
                                       {QStringLiteral("discounted"), QStringLiteral("eq:true")},
                                       {QStringLiteral("productType"), QStringLiteral("in:game,pack")}}},
        {QStringLiteral("new"), {{QStringLiteral("limit"), QStringLiteral("40")}, {QStringLiteral("order"), QStringLiteral("desc:releaseDate")},
                                 {QStringLiteral("productType"), QStringLiteral("in:game,pack")}}},
        {QStringLiteral("free"), {{QStringLiteral("limit"), QStringLiteral("24")}, {QStringLiteral("order"), QStringLiteral("desc:trending")},
                                  {QStringLiteral("price"), QStringLiteral("between:0,0")},
                                  {QStringLiteral("productType"), QStringLiteral("in:game,pack")}}},
    };
    for (const auto &row : rows) {
        const QString key = row.first;
        fetch(gogCatalogChain(row.second), HomeTtl, force, hasProducts, [this, store, key](const QJsonObject &root, bool ok, bool stale) {
            QVariantList items = StoreCatalogParser::gogCatalog(root);
            if (key == QLatin1String("new")) {
                // Предзаказы — не новинки
                items.erase(std::remove_if(items.begin(), items.end(), [](const QVariant &v) {
                    return v.toMap().value(QStringLiteral("upcoming")).toBool();
                }), items.end());
                items = items.mid(0, 24);
            }
            setRow(store, key, items, ok, stale);
        });
    }
}

void StoreCatalog::loadEpic(bool force)
{
    const QString store = QStringLiteral("epic");
    beginHome(store, {{QStringLiteral("featured"), tr("Популярное")}, {QStringLiteral("free"), tr("Бесплатно в Epic")},
                      {QStringLiteral("discounts"), tr("Скидки")}, {QStringLiteral("new"), tr("Новинки")}});
    const auto elementsOk = [](const QJsonObject &root) {
        bool ok = false;
        epicElementsOk(root, &ok);
        return ok;
    };
    // Граница «уже вышли» — конец сегодняшнего дня: запрос (и ключ кэша) меняется раз в сутки
    const QString now = QDate::currentDate().addDays(1).startOfDay(QTimeZone::utc()).toString(Qt::ISODateWithMs);
    const QVariantMap base{
        {QStringLiteral("category"), QStringLiteral("games/edition/base|bundles/games")},
        {QStringLiteral("sortDir"), QStringLiteral("DESC")},
        {QStringLiteral("start"), 0},
    };
    QVariantMap featured = base;
    featured.insert(QStringLiteral("count"), 10);
    featured.insert(QStringLiteral("sortBy"), QStringLiteral("relevancy"));
    QVariantMap discounts = base;
    discounts.insert(QStringLiteral("count"), 24);
    discounts.insert(QStringLiteral("sortBy"), QStringLiteral("relevancy"));
    discounts.insert(QStringLiteral("onSale"), true);
    QVariantMap newest = base;
    newest.insert(QStringLiteral("count"), 24);
    newest.insert(QStringLiteral("sortBy"), QStringLiteral("releaseDate"));
    // Только уже вышедшие: «[, дата]» (формат диапазона дат — как у Rare и сайта магазина)
    newest.insert(QStringLiteral("releaseDate"), QStringLiteral("[,%1]").arg(now));

    const QList<QPair<QString, QVariantMap>> rows = {
        {QStringLiteral("featured"), featured},
        {QStringLiteral("discounts"), discounts},
        {QStringLiteral("new"), newest},
    };
    for (const auto &row : rows) {
        const QString key = row.first;
        fetch(epicSearchChain(row.second), HomeTtl, force, elementsOk, [this, store, key](const QJsonObject &root, bool ok, bool stale) {
            setRow(store, key, StoreCatalogParser::epicSearch(root), ok, stale);
        });
    }
    fetch(epicFreeChain(), HomeTtl, force, elementsOk, [this, store](const QJsonObject &root, bool ok, bool stale) {
        setRow(store, QStringLiteral("free"), StoreCatalogParser::epicSearch(root, true), ok, stale);
    });
}

// ---------- Поиск ----------

void StoreCatalog::search(const QString &text)
{
    const QString term = text.simplified();
    m_searchText = term;
    m_searchResults.clear();
    m_searchByStore.clear();
    m_searchError.clear();
    const int generation = ++m_searchGeneration;
    if (term.isEmpty()) {
        m_searchPending = 0;
        emit searchChanged();
        return;
    }
    m_searchPending = 2;
    emit searchChanged();

    fetch(gogCatalogChain({{QStringLiteral("limit"), QStringLiteral("30")}, {QStringLiteral("order"), QStringLiteral("desc:score")},
                           {QStringLiteral("productType"), QStringLiteral("in:game,pack")},
                           {QStringLiteral("query"), QStringLiteral("like:") + term}}),
          SearchTtl, false, [](const QJsonObject &root) { return root.contains(QStringLiteral("products")); },
          [this, generation](const QJsonObject &root, bool ok, bool) {
              mergeSearch(generation, QStringLiteral("gog"), StoreCatalogParser::gogCatalog(root), ok);
          });

    const QVariantMap vars{
        {QStringLiteral("category"), QStringLiteral("games/edition/base|bundles/games|editors")},
        {QStringLiteral("count"), 30},
        {QStringLiteral("keywords"), term},
        {QStringLiteral("sortDir"), QStringLiteral("DESC")},
        {QStringLiteral("start"), 0},
    };
    fetch(epicSearchChain(vars), SearchTtl, false,
          [](const QJsonObject &root) {
              return root.value(QStringLiteral("data")).toObject().value(QStringLiteral("Catalog")).toObject().contains(QStringLiteral("searchStore"))
                  && StoreCatalogParser::epicErrors(root).isEmpty();
          },
          [this, generation](const QJsonObject &root, bool ok, bool) {
              mergeSearch(generation, QStringLiteral("epic"), StoreCatalogParser::epicSearch(root), ok);
          });
}

void StoreCatalog::mergeSearch(int generation, const QString &store, const QVariantList &items, bool ok)
{
    if (generation != m_searchGeneration)
        return;
    --m_searchPending;
    m_searchByStore.insert(store, items);
    if (!ok)
        m_searchByStore.insert(store + QStringLiteral(":failed"), {});
    // Результаты магазинов — вперемешку: сначала самые подходящие из каждого
    m_searchResults.clear();
    const QVariantList gog = m_searchByStore.value(QStringLiteral("gog"));
    const QVariantList epic = m_searchByStore.value(QStringLiteral("epic"));
    for (int i = 0; i < std::max(gog.size(), epic.size()); ++i) {
        if (i < gog.size())
            m_searchResults.append(gog.at(i));
        if (i < epic.size())
            m_searchResults.append(epic.at(i));
    }
    if (m_searchPending <= 0 && m_searchResults.isEmpty()) {
        const bool failed = m_searchByStore.contains(QStringLiteral("gog:failed")) && m_searchByStore.contains(QStringLiteral("epic:failed"));
        m_searchError = failed ? tr("Магазины недоступны. Проверьте подключение к интернету.") : QString();
    }
    emit searchChanged();
}

void StoreCatalog::clearSearch()
{
    search(QString());
}

// ---------- Страница игры ----------

void StoreCatalog::openDetails(const QVariantMap &item)
{
    const int generation = ++m_detailsGeneration;
    m_details = item;
    const QString store = item.value(QStringLiteral("store")).toString();
    const QString id = item.value(QStringLiteral("id")).toString();
    m_detailsLoading = true;
    emit detailsChanged();

    const auto finish = [this, generation](const QVariantMap &details) {
        if (generation != m_detailsGeneration)
            return;
        m_details = details;
        m_detailsLoading = false;
        emit detailsChanged();
    };

    if (store == QLatin1String("gog") && !id.isEmpty()) {
        QList<Request> chain;
        for (const Region &region : kRegions) {
            QUrl url(QStringLiteral("https://api.gog.com/products/") + id);
            url.setQuery(QStringLiteral("expand=description,screenshots&locale=") + region.gogLocale);
            chain.append(Request{url, {}, {}});
        }
        fetch(chain, DetailsTtl, false,
              [](const QJsonObject &root) { return root.contains(QStringLiteral("id")); },
              [item, finish](const QJsonObject &root, bool, bool) { finish(StoreCatalogParser::gogDetails(root, item)); });
        return;
    }
    const QString slug = item.value(QStringLiteral("slug")).toString();
    if (store == QLatin1String("epic") && !slug.isEmpty()) {
        // Страница игры в CMS магазина (Heroic: getExtraFromAPI, Rare: get_game_config_cms).
        // У части новых игр её там нет — тогда остаётся описание из каталога.
        QList<Request> chain;
        for (const QString &locale : {QStringLiteral("ru"), QStringLiteral("en-US")})
            chain.append(Request{QUrl(kEpicContent.arg(locale, slug)), {}, {}});
        fetch(chain, DetailsTtl, false,
              [](const QJsonObject &root) { return root.contains(QStringLiteral("pages")) || root.contains(QStringLiteral("data")); },
              [item, finish](const QJsonObject &root, bool, bool) { finish(StoreCatalogParser::epicDetails(root, item)); });
        return;
    }
    QTimer::singleShot(0, this, [item, finish] { finish(item); });
}

void StoreCatalog::closeDetails()
{
    ++m_detailsGeneration;
    m_details.clear();
    m_detailsLoading = false;
    emit detailsChanged();
}

// ---------- Купленные и установленные ----------

void StoreCatalog::rebuildOwnedIndex()
{
    if (m_indexRevision == m_revision)
        return;
    m_indexRevision = m_revision;
    m_ownedById.clear();
    m_ownedByTitle.clear();
    const QVariantMap owned = StoreManager::instance()->owned();
    for (const QString &store : stores()) {
        for (const QVariant &v : owned.value(store).toList()) {
            const QVariantMap game = v.toMap();
            // Магазин может хранить разные идентификаторы (Epic: имя приложения legendary,
            // пространство имён, предмет каталога) — запоминаем все, какие есть
            for (const QString &key : {QStringLiteral("appId"), QStringLiteral("namespace"), QStringLiteral("catalogItemId"),
                                       QStringLiteral("offerId"), QStringLiteral("productId")}) {
                const QString id = game.value(key).toString();
                if (!id.isEmpty())
                    m_ownedById[store].insert(id, game);
            }
            const QString title = StoreCatalogParser::normalizedTitle(game.value(QStringLiteral("title")).toString());
            if (!title.isEmpty())
                m_ownedByTitle[store].insert(title, game);
        }
    }
}

QVariantMap StoreCatalog::ownership(const QVariantMap &item)
{
    rebuildOwnedIndex();
    const QString store = item.value(QStringLiteral("store")).toString();
    QVariantMap game;
    QStringList ids{item.value(QStringLiteral("id")).toString(), item.value(QStringLiteral("namespace")).toString()};
    for (const QVariant &v : item.value(QStringLiteral("items")).toList())
        ids.append(v.toString());
    const auto &byId = m_ownedById.value(store);
    for (const QString &id : std::as_const(ids)) {
        if (!id.isEmpty() && byId.contains(id)) {
            game = byId.value(id);
            break;
        }
    }
    // Epic: в библиотеке legendary — имя приложения, а не номер предложения; сверяем по названию
    if (game.isEmpty() && store != QLatin1String("gog")) {
        const QString title = StoreCatalogParser::normalizedTitle(item.value(QStringLiteral("title")).toString());
        if (!title.isEmpty())
            game = m_ownedByTitle.value(store).value(title);
    }
    const QString appId = game.value(QStringLiteral("appId")).toString();
    const QString gameId = game.value(QStringLiteral("gameId")).toString();
    const bool installed = game.value(QStringLiteral("installed")).toBool()
                        || (!gameId.isEmpty() && GameLibrary::instance()->isInstalled(gameId));
    return {
        {QStringLiteral("owned"), !game.isEmpty()},
        {QStringLiteral("installed"), installed},
        {QStringLiteral("status"), game.value(QStringLiteral("status")).toString()},
        {QStringLiteral("progress"), game.value(QStringLiteral("progress"), -1)},
        {QStringLiteral("appId"), appId},
        {QStringLiteral("gameId"), gameId},
        {QStringLiteral("title"), game.value(QStringLiteral("title")).toString()},
    };
}

int StoreCatalog::ownedCount(const QString &store) const
{
    return int(StoreManager::instance()->owned().value(store).toList().size());
}

bool StoreCatalog::signedIn(const QString &store) const
{
    const QVariantMap stores = StoreManager::instance()->stores();
    return stores.contains(store) && stores.value(store).toMap().value(QStringLiteral("signedIn")).toBool();
}

void StoreCatalog::refreshOwned(const QString &store)
{
    // Магазина без входа (или ещё не поддержанного StoreManager) не трогаем
    if (signedIn(store))
        StoreManager::instance()->refresh(store);
}

QString StoreCatalog::redeemUrl(const QString &store) const
{
    if (store == QLatin1String("gog"))
        return QStringLiteral("https://www.gog.com/redeem");
    if (store == QLatin1String("epic"))
        return QStringLiteral("https://www.epicgames.com/redeem");
    return {};
}

QString StoreCatalog::storeUrl(const QString &store) const
{
    if (store == QLatin1String("gog"))
        return QStringLiteral("https://www.gog.com/ru/");
    if (store == QLatin1String("epic"))
        return QStringLiteral("https://store.epicgames.com/ru/");
    return {};
}
