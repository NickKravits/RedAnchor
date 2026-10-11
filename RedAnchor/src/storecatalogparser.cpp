#include "storecatalogparser.h"

#include <QJsonArray>
#include <QJsonValue>
#include <QLocale>
#include <QRegularExpression>
#include <QSet>
#include <QUrl>

#include <cmath>

namespace StoreCatalogParser {

namespace {

// Адреса картинок GOG иногда приходят без схемы: «//images-3.gog.com/…»
QString absoluteUrl(const QString &url)
{
    if (url.startsWith(QLatin1String("//")))
        return QStringLiteral("https:") + url;
    return url;
}

QStringList stringList(const QJsonValue &value)
{
    QStringList list;
    for (const QJsonValue &v : value.toArray()) {
        const QString s = v.toString().trimmed();
        if (!s.isEmpty())
            list.append(s);
    }
    return list;
}

QStringList namesOf(const QJsonValue &value)
{
    QStringList list;
    for (const QJsonValue &v : value.toArray()) {
        const QString name = v.toObject().value(QStringLiteral("name")).toString().trimmed();
        if (!name.isEmpty())
            list.append(name);
    }
    return list;
}

// Дата «2024.05.09», «2024-05-09T…» или «2008-11-25T06:00:00+0200» → «2024-05-09»
QString isoDate(const QString &text)
{
    static const QRegularExpression re(QStringLiteral("^(\\d{4})[.-](\\d{2})[.-](\\d{2})"));
    const QRegularExpressionMatch m = re.match(text.trimmed());
    if (!m.hasMatch())
        return {};
    return m.captured(1) + QLatin1Char('-') + m.captured(2) + QLatin1Char('-') + m.captured(3);
}

bool isFuture(const QString &date, const QDateTime &now)
{
    const QDate d = QDate::fromString(date, Qt::ISODate);
    return d.isValid() && d > now.date();
}

QVariantMap priceMap(bool known, bool free, const QString &finalText, const QString &baseText, int discount,
                     const QString &currency, double amount)
{
    return {
        {QStringLiteral("known"), known},
        {QStringLiteral("free"), free},
        {QStringLiteral("final"), finalText},
        {QStringLiteral("base"), baseText},
        {QStringLiteral("discount"), discount},
        {QStringLiteral("currency"), currency},
        {QStringLiteral("amount"), amount},
    };
}

// Цена для людей, если магазин не прислал готовую строку: «1 299 ₽», «$24.99»
QString formatMoney(double amount, const QString &currency)
{
    if (currency == QLatin1String("RUB"))
        return QLocale(QLocale::Russian, QLocale::Russia).toString(amount, 'f', std::floor(amount) == amount ? 0 : 2)
               + QStringLiteral(" ₽");
    const QLocale locale(QLocale::Russian, QLocale::Russia);
    return locale.toString(amount, 'f', 2) + QChar(0x00a0) + currency;
}

QDateTime parseTime(const QString &text)
{
    return QDateTime::fromString(text, Qt::ISODateWithMs);
}

} // namespace

// ---------- Общее ----------

QString plainText(const QString &html)
{
    QString text = html;
    static const QRegularExpression breaks(QStringLiteral("<\\s*(br|/p|/li|/h\\d|/div)\\s*/?\\s*>"),
                                           QRegularExpression::CaseInsensitiveOption);
    static const QRegularExpression items(QStringLiteral("<\\s*li[^>]*>"), QRegularExpression::CaseInsensitiveOption);
    static const QRegularExpression tags(QStringLiteral("<[^>]*>"));
    // Markdown в описаниях Epic: **жирный**, ## заголовки, ![картинки](…), [ссылки](…)
    static const QRegularExpression mdImages(QStringLiteral("!\\[[^\\]]*\\]\\([^)]*\\)"));
    static const QRegularExpression mdLinks(QStringLiteral("\\[([^\\]]*)\\]\\([^)]*\\)"));
    static const QRegularExpression mdMarks(QStringLiteral("(\\*\\*|__|^#+\\s*)"), QRegularExpression::MultilineOption);
    static const QRegularExpression spaces(QStringLiteral("[ \\t\\x{00a0}]+"));
    static const QRegularExpression blankLines(QStringLiteral("\\n\\s*\\n\\s*(\\n\\s*)+"));
    text.replace(items, QStringLiteral("• "));
    text.replace(breaks, QStringLiteral("\n"));
    text.replace(tags, QString());
    text.replace(mdImages, QString());
    text.replace(mdLinks, QStringLiteral("\\1"));
    text.replace(mdMarks, QString());
    text.replace(QLatin1String("&nbsp;"), QLatin1String(" "));
    text.replace(QLatin1String("&quot;"), QLatin1String("\""));
    text.replace(QLatin1String("&#39;"), QLatin1String("'"));
    text.replace(QLatin1String("&lt;"), QLatin1String("<"));
    text.replace(QLatin1String("&gt;"), QLatin1String(">"));
    text.replace(QLatin1String("&amp;"), QLatin1String("&"));
    text.replace(QLatin1String("\r"), QString());
    text.replace(spaces, QStringLiteral(" "));
    text.replace(blankLines, QStringLiteral("\n\n"));
    QStringList lines = text.split(QLatin1Char('\n'));
    for (QString &line : lines)
        line = line.trimmed();
    return lines.join(QLatin1Char('\n')).trimmed();
}

QString normalizedTitle(const QString &title)
{
    QString t = title.toLower();
    static const QRegularExpression editions(QStringLiteral(
        "\\b(standard|digital|base|game of the year|goty|definitive|complete|deluxe)\\s+edition\\b"));
    static const QRegularExpression marks(QStringLiteral("[™®©]"));
    static const QRegularExpression other(QStringLiteral("[^\\p{L}\\p{N}]+"));
    t.replace(marks, QString());
    t.replace(editions, QString());
    t.replace(other, QString());
    return t;
}

// ---------- GOG ----------

QVariantMap gogProduct(const QJsonObject &p)
{
    const QString id = p.value(QStringLiteral("id")).toVariant().toString();
    const QString title = p.value(QStringLiteral("title")).toString().trimmed();
    if (id.isEmpty() || title.isEmpty())
        return {};
    const QString slug = p.value(QStringLiteral("slug")).toString();

    // Цена: готовые строки (final, base, discount «-40%») и суммы (finalMoney, baseMoney)
    const QJsonObject price = p.value(QStringLiteral("price")).toObject();
    const QJsonObject finalMoney = price.value(QStringLiteral("finalMoney")).toObject();
    const QJsonObject baseMoney = price.value(QStringLiteral("baseMoney")).toObject();
    const bool known = !price.isEmpty() && finalMoney.contains(QStringLiteral("amount"));
    const double finalAmount = finalMoney.value(QStringLiteral("amount")).toString().toDouble();
    const double baseAmount = baseMoney.value(QStringLiteral("amount")).toString().toDouble();
    const QString currency = finalMoney.value(QStringLiteral("currency")).toString();
    int discount = 0;
    {
        static const QRegularExpression digits(QStringLiteral("(\\d+)"));
        const QRegularExpressionMatch m = digits.match(price.value(QStringLiteral("discount")).toString());
        if (m.hasMatch())
            discount = m.captured(1).toInt();
        else if (baseAmount > 0 && finalAmount < baseAmount)
            discount = int(std::lround((baseAmount - finalAmount) * 100.0 / baseAmount));
    }
    QString finalText = price.value(QStringLiteral("final")).toString();
    QString baseText = price.value(QStringLiteral("base")).toString();
    if (known && finalText.isEmpty())
        finalText = formatMoney(finalAmount, currency);
    if (known && baseText.isEmpty())
        baseText = formatMoney(baseAmount, currency);

    QVariantList screenshots;
    for (const QString &s : stringList(p.value(QStringLiteral("screenshots")))) {
        QString url = absoluteUrl(s);
        url.replace(QLatin1String("{formatter}"), QLatin1String("ggvgl"));
        screenshots.append(url);
    }

    QVariantList platforms;
    for (const QString &os : stringList(p.value(QStringLiteral("operatingSystems")))) {
        if (os == QLatin1String("windows"))
            platforms.append(QStringLiteral("windows"));
        else if (os == QLatin1String("osx") || os == QLatin1String("mac"))
            platforms.append(QStringLiteral("mac"));
        else if (os == QLatin1String("linux"))
            platforms.append(QStringLiteral("linux"));
    }
    bool controller = false;
    for (const QJsonValue &f : p.value(QStringLiteral("features")).toArray()) {
        const QString featureSlug = f.toObject().value(QStringLiteral("slug")).toString();
        if (featureSlug == QLatin1String("controller_support") || featureSlug == QLatin1String("controller"))
            controller = true;
    }

    const QString releaseDate = isoDate(p.value(QStringLiteral("releaseDate")).toString());
    const QString state = p.value(QStringLiteral("productState")).toString();
    const QStringList developers = stringList(p.value(QStringLiteral("developers")));
    const QStringList publishers = stringList(p.value(QStringLiteral("publishers")));
    const QString cover = absoluteUrl(p.value(QStringLiteral("coverHorizontal")).toString());
    const QString hero = absoluteUrl(p.value(QStringLiteral("galaxyBackgroundImage")).toString());

    QString url = p.value(QStringLiteral("storeLink")).toString();
    if (!slug.isEmpty())
        url = QStringLiteral("https://www.gog.com/ru/game/") + slug;

    return {
        {QStringLiteral("store"), QStringLiteral("gog")},
        {QStringLiteral("id"), id},
        {QStringLiteral("slug"), slug},
        {QStringLiteral("title"), title},
        {QStringLiteral("description"), QString()},
        {QStringLiteral("cover"), cover},
        {QStringLiteral("tall"), absoluteUrl(p.value(QStringLiteral("coverVertical")).toString())},
        {QStringLiteral("hero"), hero.isEmpty() ? cover : hero},
        {QStringLiteral("logo"), absoluteUrl(p.value(QStringLiteral("logo")).toString())},
        {QStringLiteral("developer"), developers.join(QStringLiteral(", "))},
        {QStringLiteral("publisher"), publishers.join(QStringLiteral(", "))},
        {QStringLiteral("releaseDate"), releaseDate},
        {QStringLiteral("upcoming"), state == QLatin1String("coming-soon") || isFuture(releaseDate, QDateTime::currentDateTimeUtc())},
        {QStringLiteral("platforms"), platforms},
        {QStringLiteral("controller"), controller},
        {QStringLiteral("genres"), namesOf(p.value(QStringLiteral("genres")))},
        {QStringLiteral("screenshots"), screenshots},
        {QStringLiteral("price"), priceMap(known, known && finalAmount <= 0, finalText, baseText, discount, currency, finalAmount)},
        {QStringLiteral("url"), url},
        {QStringLiteral("checkoutUrl"), QString()},
        {QStringLiteral("productType"), p.value(QStringLiteral("productType")).toString()},
    };
}

QVariantList gogCatalog(const QJsonObject &root)
{
    QVariantList list;
    QSet<QString> seen;
    for (const QJsonValue &value : root.value(QStringLiteral("products")).toArray()) {
        const QVariantMap item = gogProduct(value.toObject());
        const QString id = item.value(QStringLiteral("id")).toString();
        if (id.isEmpty() || seen.contains(id))
            continue;
        seen.insert(id);
        list.append(item);
    }
    return list;
}

QVariantMap gogDetails(const QJsonObject &product, const QVariantMap &base)
{
    QVariantMap item = base;
    if (product.isEmpty())
        return item;
    const QJsonObject description = product.value(QStringLiteral("description")).toObject();
    QString text = plainText(description.value(QStringLiteral("lead")).toString());
    if (text.isEmpty())
        text = plainText(description.value(QStringLiteral("full")).toString());
    if (!text.isEmpty())
        item.insert(QStringLiteral("description"), text);
    const QString full = plainText(description.value(QStringLiteral("full")).toString());
    if (!full.isEmpty())
        item.insert(QStringLiteral("fullDescription"), full);

    if (item.value(QStringLiteral("screenshots")).toList().isEmpty()) {
        QVariantList screenshots;
        for (const QJsonValue &s : product.value(QStringLiteral("screenshots")).toArray()) {
            QString url = absoluteUrl(s.toObject().value(QStringLiteral("formatter_template_url")).toString());
            url.replace(QLatin1String("{formatter}"), QLatin1String("ggvgl"));
            if (!url.isEmpty())
                screenshots.append(url);
        }
        item.insert(QStringLiteral("screenshots"), screenshots);
    }
    const QJsonObject images = product.value(QStringLiteral("images")).toObject();
    const QString background = absoluteUrl(images.value(QStringLiteral("background")).toString());
    if (item.value(QStringLiteral("hero")).toString().isEmpty() && !background.isEmpty())
        item.insert(QStringLiteral("hero"), background);
    if (item.value(QStringLiteral("releaseDate")).toString().isEmpty())
        item.insert(QStringLiteral("releaseDate"), isoDate(product.value(QStringLiteral("release_date")).toString()));
    if (item.value(QStringLiteral("title")).toString().isEmpty())
        item.insert(QStringLiteral("title"), product.value(QStringLiteral("title")).toString());
    // Ссылка оформления покупки (links.purchase_link). Покупаем всё же через страницу игры:
    // адрес оформления GOG не документирован и менялся, а со страницы игры покупка работает всегда
    const QString purchase = product.value(QStringLiteral("links")).toObject().value(QStringLiteral("purchase_link")).toString();
    if (!purchase.isEmpty())
        item.insert(QStringLiteral("purchaseLink"), purchase);
    if (item.value(QStringLiteral("platforms")).toList().isEmpty()) {
        const QJsonObject os = product.value(QStringLiteral("content_system_compatibility")).toObject();
        QVariantList platforms;
        if (os.value(QStringLiteral("windows")).toBool())
            platforms.append(QStringLiteral("windows"));
        if (os.value(QStringLiteral("osx")).toBool())
            platforms.append(QStringLiteral("mac"));
        if (os.value(QStringLiteral("linux")).toBool())
            platforms.append(QStringLiteral("linux"));
        item.insert(QStringLiteral("platforms"), platforms);
    }
    return item;
}

// ---------- Epic Games Store ----------

QStringList epicErrors(const QJsonObject &root)
{
    QStringList errors;
    for (const QJsonValue &value : root.value(QStringLiteral("errors")).toArray()) {
        const QJsonObject e = value.toObject();
        const QString service = e.value(QStringLiteral("serviceResponse")).toString();
        // 1004 — «нет доступа к промо-предложению»: приходит всегда, сам магазин её не замечает
        if (service.contains(QLatin1String("\"numericErrorCode\":1004")))
            continue;
        errors.append(e.value(QStringLiteral("message")).toString());
    }
    return errors;
}

QVariantMap epicElement(const QJsonObject &e, bool promotions, const QDateTime &now)
{
    const QString id = e.value(QStringLiteral("id")).toString();
    const QString title = e.value(QStringLiteral("title")).toString().trimmed();
    const QString ns = e.value(QStringLiteral("namespace")).toString();
    if (id.isEmpty() || title.isEmpty())
        return {};

    // Картинки: непрозрачные «коробки» (логотипы с прозрачным фоном не годятся для обложки)
    QString wide, tall, thumb, logo;
    for (const QJsonValue &value : e.value(QStringLiteral("keyImages")).toArray()) {
        const QJsonObject img = value.toObject();
        const QString type = img.value(QStringLiteral("type")).toString();
        const QString url = img.value(QStringLiteral("url")).toString();
        if (url.isEmpty())
            continue;
        if (type == QLatin1String("DieselStoreFrontWide") || type == QLatin1String("DieselGameBox"))
            wide = url;
        else if (type == QLatin1String("OfferImageWide") && wide.isEmpty())
            wide = url;
        else if (type == QLatin1String("OfferImageTall") || type == QLatin1String("DieselGameBoxTall")
                 || type == QLatin1String("DieselStoreFrontTall"))
            tall = tall.isEmpty() ? url : tall;
        else if (type == QLatin1String("Thumbnail") && thumb.isEmpty())
            thumb = url;
        else if (type == QLatin1String("ProductLogo") || type == QLatin1String("DieselGameBoxLogo"))
            logo = url;
    }
    // Раздача-«загадка» (Mystery Game) без страницы и картинок — не показываем
    if (wide.isEmpty() && tall.isEmpty() && thumb.isEmpty())
        return {};

    // Страница игры: адрес из сопоставлений каталога, иначе productSlug без «/home»
    QString slug;
    for (const QString &key : {QStringLiteral("catalogNs"), QStringLiteral("offerMappings")}) {
        const QJsonValue v = e.value(key);
        const QJsonArray mappings = v.isArray() ? v.toArray() : v.toObject().value(QStringLiteral("mappings")).toArray();
        for (const QJsonValue &m : mappings) {
            const QJsonObject map = m.toObject();
            if (slug.isEmpty() && map.value(QStringLiteral("pageType")).toString() == QLatin1String("productHome"))
                slug = map.value(QStringLiteral("pageSlug")).toString();
        }
    }
    if (slug.isEmpty()) {
        slug = e.value(QStringLiteral("productSlug")).toString();
        if (slug.endsWith(QLatin1String("/home")))
            slug.chop(5);
        if (slug == QLatin1String("[]"))
            slug.clear();
    }

    QString developer, publisher;
    for (const QJsonValue &value : e.value(QStringLiteral("customAttributes")).toArray()) {
        const QJsonObject attr = value.toObject();
        const QString key = attr.value(QStringLiteral("key")).toString();
        if (key == QLatin1String("developerName"))
            developer = attr.value(QStringLiteral("value")).toString();
        else if (key == QLatin1String("publisherName"))
            publisher = attr.value(QStringLiteral("value")).toString();
    }
    const QString seller = e.value(QStringLiteral("seller")).toObject().value(QStringLiteral("name")).toString();
    if (developer.isEmpty())
        developer = seller;
    if (publisher.isEmpty())
        publisher = seller;

    // Метки каталога: 9547 — Windows, 9548 — macOS, 9549 — поддержка контроллеров
    // (те же номера использует фильтр магазина в Rare)
    QVariantList platforms;
    QVariantList genres;
    bool controller = false;
    for (const QJsonValue &value : e.value(QStringLiteral("tags")).toArray()) {
        const QJsonObject tag = value.toObject();
        const QString tagId = tag.value(QStringLiteral("id")).toVariant().toString();
        if (tagId == QLatin1String("9547"))
            platforms.append(QStringLiteral("windows"));
        else if (tagId == QLatin1String("9548"))
            platforms.append(QStringLiteral("mac"));
        else if (tagId == QLatin1String("9549"))
            controller = true;
        else if (tag.value(QStringLiteral("groupName")).toString() == QLatin1String("genre")) {
            const QString name = tag.value(QStringLiteral("name")).toString();
            if (!name.isEmpty())
                genres.append(name);
        }
    }

    // Цена: суммы в минимальных единицах (копейки, центы) и готовые строки fmtPrice
    const QJsonObject total = e.value(QStringLiteral("price")).toObject().value(QStringLiteral("totalPrice")).toObject();
    const bool known = total.contains(QStringLiteral("originalPrice"));
    const double original = total.value(QStringLiteral("originalPrice")).toDouble();
    const double discounted = total.value(QStringLiteral("discountPrice")).toDouble(original);
    const int decimals = total.value(QStringLiteral("currencyInfo")).toObject().value(QStringLiteral("decimals")).toInt(2);
    const double divisor = std::pow(10.0, decimals);
    const QString currency = total.value(QStringLiteral("currencyCode")).toString();
    const QJsonObject fmt = total.value(QStringLiteral("fmtPrice")).toObject();
    QString finalText = fmt.value(QStringLiteral("discountPrice")).toString();
    QString baseText = fmt.value(QStringLiteral("originalPrice")).toString();
    if (known && (finalText.isEmpty() || finalText == QLatin1String("0")))
        finalText = discounted <= 0 ? QString() : formatMoney(discounted / divisor, currency);
    if (known && (baseText.isEmpty() || baseText == QLatin1String("0")))
        baseText = original <= 0 ? QString() : formatMoney(original / divisor, currency);
    const int discount = known && original > 0 && discounted < original
                       ? int(std::lround((original - discounted) * 100.0 / original)) : 0;

    // Бесплатные раздачи: текущее или будущее предложение со скидкой до 0 (discountPercentage == 0)
    QString promo, promoStart, promoEnd;
    if (promotions) {
        const QJsonObject promos = e.value(QStringLiteral("promotions")).toObject();
        const auto scan = [&](const QString &key, const QString &kind) {
            for (const QJsonValue &group : promos.value(key).toArray()) {
                for (const QJsonValue &offer : group.toObject().value(QStringLiteral("promotionalOffers")).toArray()) {
                    const QJsonObject o = offer.toObject();
                    const QJsonObject setting = o.value(QStringLiteral("discountSetting")).toObject();
                    if (setting.value(QStringLiteral("discountPercentage")).toInt(-1) != 0 || !promo.isEmpty())
                        continue;
                    const QDateTime start = parseTime(o.value(QStringLiteral("startDate")).toString());
                    const QDateTime end = parseTime(o.value(QStringLiteral("endDate")).toString());
                    if (kind == QLatin1String("now") && ((start.isValid() && start > now) || (end.isValid() && end < now)))
                        continue;
                    promo = kind;
                    promoStart = o.value(QStringLiteral("startDate")).toString();
                    promoEnd = o.value(QStringLiteral("endDate")).toString();
                }
            }
        };
        scan(QStringLiteral("promotionalOffers"), QStringLiteral("now"));
        scan(QStringLiteral("upcomingPromotionalOffers"), QStringLiteral("upcoming"));
    }

    QString releaseDate = isoDate(e.value(QStringLiteral("releaseDate")).toString());
    if (releaseDate.isEmpty())
        releaseDate = isoDate(e.value(QStringLiteral("effectiveDate")).toString());
    // Дату 2099-01-01 Epic ставит играм без даты выхода
    const bool noDate = releaseDate.startsWith(QLatin1String("2099"));
    if (noDate)
        releaseDate.clear();

    QVariantList items;
    for (const QJsonValue &value : e.value(QStringLiteral("items")).toArray()) {
        const QString itemId = value.toObject().value(QStringLiteral("id")).toString();
        if (!itemId.isEmpty())
            items.append(itemId);
    }

    const QString url = slug.isEmpty() ? QStringLiteral("https://store.epicgames.com/ru/free-games")
                                       : QStringLiteral("https://store.epicgames.com/ru/p/") + slug;
    // Оформление покупки сразу (тот же адрес открывает кнопка «Получить» на сайте)
    const QString checkout = ns.isEmpty() ? QString()
        : QStringLiteral("https://store.epicgames.com/purchase?offers=1-%1-%2#/").arg(ns, id);
    const QString cover = wide.isEmpty() ? thumb : wide;

    return {
        {QStringLiteral("store"), QStringLiteral("epic")},
        {QStringLiteral("id"), id},
        {QStringLiteral("namespace"), ns},
        {QStringLiteral("items"), items},
        {QStringLiteral("slug"), slug},
        {QStringLiteral("title"), title},
        {QStringLiteral("description"), plainText(e.value(QStringLiteral("description")).toString())},
        {QStringLiteral("cover"), cover},
        {QStringLiteral("tall"), tall.isEmpty() ? thumb : tall},
        {QStringLiteral("hero"), cover},
        {QStringLiteral("logo"), logo},
        {QStringLiteral("developer"), developer},
        {QStringLiteral("publisher"), publisher},
        {QStringLiteral("releaseDate"), releaseDate},
        {QStringLiteral("upcoming"), noDate || isFuture(releaseDate, now)},
        {QStringLiteral("platforms"), platforms},
        {QStringLiteral("controller"), controller},
        {QStringLiteral("genres"), genres},
        {QStringLiteral("screenshots"), QVariantList()},
        {QStringLiteral("price"), priceMap(known, known && discounted <= 0, finalText, baseText, discount, currency, discounted / divisor)},
        {QStringLiteral("url"), url},
        {QStringLiteral("checkoutUrl"), checkout},
        {QStringLiteral("productType"), e.value(QStringLiteral("offerType")).toString()},
        {QStringLiteral("promo"), promo},
        {QStringLiteral("promoStart"), promoStart},
        {QStringLiteral("promoEnd"), promoEnd},
    };
}

QVariantList epicSearch(const QJsonObject &root, bool promotions, const QDateTime &now)
{
    const QJsonArray elements = root.value(QStringLiteral("data")).toObject().value(QStringLiteral("Catalog")).toObject()
                                    .value(QStringLiteral("searchStore")).toObject().value(QStringLiteral("elements")).toArray();
    QVariantList list;
    QSet<QString> seen;
    for (const QJsonValue &value : elements) {
        const QVariantMap item = epicElement(value.toObject(), promotions, now);
        const QString id = item.value(QStringLiteral("id")).toString();
        if (id.isEmpty() || seen.contains(id))
            continue;
        // В ленте раздач много обычных игр — оставляем только сами раздачи
        if (promotions && item.value(QStringLiteral("promo")).toString().isEmpty())
            continue;
        seen.insert(id);
        list.append(item);
    }
    if (promotions) {
        // Сначала то, что можно забрать сейчас, затем будущие раздачи
        std::stable_sort(list.begin(), list.end(), [](const QVariant &a, const QVariant &b) {
            return a.toMap().value(QStringLiteral("promo")).toString() == QLatin1String("now")
                && b.toMap().value(QStringLiteral("promo")).toString() != QLatin1String("now");
        });
    }
    return list;
}

QVariantMap epicDetails(const QJsonObject &product, const QVariantMap &base)
{
    QVariantMap item = base;
    if (product.isEmpty())
        return item;
    // Страница игры в CMS: pages[] с типом productHome, у старых игр — данные в корне
    QJsonObject page;
    for (const QJsonValue &value : product.value(QStringLiteral("pages")).toArray()) {
        const QJsonObject p = value.toObject();
        if (p.value(QStringLiteral("type")).toString() == QLatin1String("productHome")
            || p.value(QStringLiteral("_slug")).toString() == QLatin1String("home")) {
            page = p;
            break;
        }
    }
    if (page.isEmpty())
        page = product;
    const QJsonObject data = page.value(QStringLiteral("data")).toObject();
    const QJsonObject about = data.value(QStringLiteral("about")).toObject();
    const QString shortText = plainText(about.value(QStringLiteral("shortDescription")).toString());
    const QString fullText = plainText(about.value(QStringLiteral("description")).toString());
    if (!shortText.isEmpty())
        item.insert(QStringLiteral("description"), shortText);
    else if (!fullText.isEmpty() && item.value(QStringLiteral("description")).toString().isEmpty())
        item.insert(QStringLiteral("description"), fullText);
    if (!fullText.isEmpty())
        item.insert(QStringLiteral("fullDescription"), fullText);
    const QString developer = about.value(QStringLiteral("developerAttribution")).toString();
    const QString publisher = about.value(QStringLiteral("publisherAttribution")).toString();
    if (!developer.isEmpty())
        item.insert(QStringLiteral("developer"), developer);
    if (!publisher.isEmpty())
        item.insert(QStringLiteral("publisher"), publisher);
    const QString heroImage = data.value(QStringLiteral("hero")).toObject().value(QStringLiteral("backgroundImageUrl")).toString();
    if (!heroImage.isEmpty())
        item.insert(QStringLiteral("hero"), heroImage);

    // Скриншоты: _images_ страницы или корня (без логотипов и мелких картинок не разобрать — берём все)
    QVariantList screenshots;
    for (const QJsonValue &value : page.value(QStringLiteral("_images_")).toArray().isEmpty()
                                       ? product.value(QStringLiteral("_images_")).toArray()
                                       : page.value(QStringLiteral("_images_")).toArray()) {
        const QString url = value.toString();
        if (!url.isEmpty() && !url.contains(QLatin1String("logo"), Qt::CaseInsensitive)
            && !url.endsWith(QLatin1String(".svg"), Qt::CaseInsensitive)
            && screenshots.size() < 12)
            screenshots.append(url);
    }
    if (!screenshots.isEmpty())
        item.insert(QStringLiteral("screenshots"), screenshots);
    return item;
}

} // namespace StoreCatalogParser
