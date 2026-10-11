#pragma once

#include <QDateTime>
#include <QJsonObject>
#include <QString>
#include <QVariantList>
#include <QVariantMap>

// Разбор ответов открытых API витрин GOG и Epic Games Store (без входа в магазин).
// Отдельно от StoreCatalog, чтобы разбор можно было проверить без остального приложения.
//
// Каждая игра витрины — словарь одного вида для всех магазинов:
//   store ("gog" | "epic"), id, slug, title, description,
//   cover (широкая обложка 16:9), tall (вертикальная), hero (широкий фон), logo,
//   developer, publisher, releaseDate ("гггг-ММ-дд" или пусто), upcoming (ещё не вышла),
//   platforms ["windows", "mac", "linux"], controller (true — заявлена поддержка геймпада),
//   genres [названия], screenshots [URL],
//   price { known, free, final, base, discount (проценты), currency, amount },
//   url (страница игры), checkoutUrl (оформление покупки; пусто — страница игры),
//   Epic: namespace, items [id предметов каталога], promo ("" | "now" | "upcoming"), promoStart, promoEnd.
namespace StoreCatalogParser {

// GOG: catalog.gog.com/v1/catalog → { products: [...] }
QVariantList gogCatalog(const QJsonObject &root);
QVariantMap gogProduct(const QJsonObject &product);
// GOG: api.gog.com/products/<id>?expand=description,screenshots — дополняет карточку игры
QVariantMap gogDetails(const QJsonObject &product, const QVariantMap &base);

// Epic: GraphQL searchStore и freeGamesPromotions (у обоих data.Catalog.searchStore.elements).
// promotions — разбирать бесплатные раздачи (только из freeGamesPromotions); now — текущее время
QVariantList epicSearch(const QJsonObject &root, bool promotions = false,
                        const QDateTime &now = QDateTime::currentDateTimeUtc());
QVariantMap epicElement(const QJsonObject &element, bool promotions, const QDateTime &now);
// Epic: store-content.ak.epicgames.com/api/<locale>/content/products/<slug> — дополняет карточку
QVariantMap epicDetails(const QJsonObject &product, const QVariantMap &base);
// Ошибки GraphQL ответа (пустой список — ошибок нет). Ошибку 1004 Epic отдаёт всегда — её пропускаем.
QStringList epicErrors(const QJsonObject &root);

// Текст из HTML-описания: без тегов, абзацы — переводами строк
QString plainText(const QString &html);
// Название для сравнения: строчными, только буквы и цифры, без «Standard Edition» и т. п.
QString normalizedTitle(const QString &title);

} // namespace StoreCatalogParser
