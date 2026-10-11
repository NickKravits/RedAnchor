#include "gameart.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QUrl>

namespace {

const QString kApi = QStringLiteral("https://www.steamgriddb.com/api/v2");
constexpr int MaxParallel = 2;
// Не нашли обложек — пробуем снова не раньше чем через три дня
constexpr qint64 RetryAfterSecs = 3 * 24 * 3600;

QString apiKey()
{
    return QStringLiteral(REDANCHOR_STEAMGRIDDB_KEY);
}

} // namespace

GameArt::GameArt(QObject *parent)
    : QObject(parent)
    , m_network(new QNetworkAccessManager(this))
{
}

QString GameArt::cacheDir(const QString &gameId) const
{
    QString name = gameId;
    name.replace(QRegularExpression(QStringLiteral("[^A-Za-z0-9_.-]")), QStringLiteral("_"));
    return QStandardPaths::writableLocation(QStandardPaths::CacheLocation) + QStringLiteral("/art/") + name;
}

QVariantMap GameArt::localArt(const QString &gameId) const
{
    QVariantMap art{{QStringLiteral("grid"), QString()}, {QStringLiteral("hero"), QString()},
                    {QStringLiteral("video"), QString()}, {QStringLiteral("logo"), QString()}};
    const QDir dir(cacheDir(gameId));
    for (const QString &kind : {QStringLiteral("grid"), QStringLiteral("hero"), QStringLiteral("video"), QStringLiteral("logo")}) {
        const QStringList files = dir.entryList({kind + QStringLiteral(".*")}, QDir::Files);
        if (!files.isEmpty())
            art.insert(kind, QUrl::fromLocalFile(dir.filePath(files.first())).toString());
    }
    return art;
}

QVariantMap GameArt::art(const QString &gameId, const QString &store, const QString &appId, const QString &title)
{
    const QVariantMap local = localArt(gameId);
    if (apiKey().isEmpty() || m_known.contains(gameId))
        return local;

    // Уже обрабатывали в прошлых запусках?
    QFile meta(cacheDir(gameId) + QStringLiteral("/meta.json"));
    if (meta.open(QIODevice::ReadOnly)) {
        const QJsonObject o = QJsonDocument::fromJson(meta.readAll()).object();
        const qint64 age = QDateTime::currentSecsSinceEpoch() - qint64(o.value(QStringLiteral("time")).toDouble());
        const int sgdbId = o.value(QStringLiteral("sgdbId")).toInt();
        // Обложки скачаны до появления логотипов — докачиваем только логотип
        if (o.value(QStringLiteral("ok")).toBool() && !o.value(QStringLiteral("logo")).toBool() && sgdbId > 0) {
            m_known.insert(gameId);
            Job job;
            job.gameId = gameId;
            job.store = store;
            job.appId = appId;
            job.title = title;
            job.dir = cacheDir(gameId);
            job.sgdbId = sgdbId;
            job.pending = {QStringLiteral("logo")};
            ++m_running;
            fetchNext(job);
            return local;
        }
        if (o.value(QStringLiteral("ok")).toBool() || age < RetryAfterSecs) {
            m_known.insert(gameId);
            return local;
        }
    }

    m_known.insert(gameId);
    Job job;
    job.gameId = gameId;
    job.store = store;
    job.appId = appId;
    job.title = title;
    job.dir = cacheDir(gameId);
    m_queue.append(job);
    startNext();
    return local;
}

QNetworkReply *GameArt::api(const QString &path)
{
    QNetworkRequest request(QUrl(kApi + path));
    request.setRawHeader("Authorization", "Bearer " + apiKey().toLatin1());
    request.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("RedAnchor-Shell"));
    return m_network->get(request);
}

void GameArt::startNext()
{
    while (m_running < MaxParallel && !m_queue.isEmpty()) {
        ++m_running;
        resolveGame(m_queue.takeFirst());
    }
}

void GameArt::resolveGame(Job job)
{
    // Игра ищется по названию
    const QString path = QStringLiteral("/search/autocomplete/") + QString::fromLatin1(QUrl::toPercentEncoding(job.title));
    QNetworkReply *reply = api(path);
    connect(reply, &QNetworkReply::finished, this, [this, reply, job]() mutable {
        reply->deleteLater();
        const QJsonValue data = QJsonDocument::fromJson(reply->readAll()).object().value(QStringLiteral("data"));
        const QJsonArray found = data.toArray();
        const QJsonObject game = data.isArray() ? (found.isEmpty() ? QJsonObject() : found.first().toObject())
                                                : data.toObject();
        job.sgdbId = game.value(QStringLiteral("id")).toInt();
        if (reply->error() != QNetworkReply::NoError || job.sgdbId <= 0) {
            finish(job, false);
            return;
        }
        QDir().mkpath(job.dir);
        job.pending = {QStringLiteral("grid"), QStringLiteral("video"), QStringLiteral("hero"), QStringLiteral("logo")};
        fetchNext(job);
    });
}

void GameArt::fetchNext(Job job)
{
    if (job.pending.isEmpty()) {
        finish(job, true);
        return;
    }
    const QString kind = job.pending.takeFirst();
    const QString id = QString::number(job.sgdbId);
    QString path;
    if (kind == QLatin1String("grid"))
        path = QStringLiteral("/grids/game/%1?dimensions=1024x1024,512x512&types=static&nsfw=false&humor=false").arg(id);
    else if (kind == QLatin1String("gridTall"))   // квадратной нет — берём вертикальную
        path = QStringLiteral("/grids/game/%1?dimensions=600x900&types=static&nsfw=false&humor=false").arg(id);
    else if (kind == QLatin1String("video"))
        path = QStringLiteral("/grids/game/%1?dimensions=1024x1024,512x512&types=animated&nsfw=false&humor=false").arg(id);
    else if (kind == QLatin1String("logo"))
        path = QStringLiteral("/logos/game/%1?types=static&nsfw=false&humor=false").arg(id);
    else
        path = QStringLiteral("/heroes/game/%1?types=static&nsfw=false&humor=false").arg(id);

    QNetworkReply *reply = api(path);
    connect(reply, &QNetworkReply::finished, this, [this, reply, job, kind]() mutable {
        reply->deleteLater();
        const QJsonArray items = QJsonDocument::fromJson(reply->readAll()).object().value(QStringLiteral("data")).toArray();
        QString url;
        for (const QJsonValue &value : items) {
            const QString candidate = value.toObject().value(QStringLiteral("url")).toString();
            // Видео — только WebM (его проигрывает Qt Multimedia)
            if (kind == QLatin1String("video") && !candidate.endsWith(QLatin1String(".webm"), Qt::CaseInsensitive))
                continue;
            url = candidate;
            break;
        }
        if (url.isEmpty()) {
            if (kind == QLatin1String("grid"))
                job.pending.prepend(QStringLiteral("gridTall"));
            fetchNext(job);
            return;
        }
        download(job, kind == QLatin1String("gridTall") ? QStringLiteral("grid") : kind, url);
    });
}

void GameArt::download(Job job, const QString &kind, const QString &url)
{
    QNetworkRequest request{QUrl(url)};
    request.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("RedAnchor-Shell"));
    QNetworkReply *reply = m_network->get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply, job, kind, url] {
        reply->deleteLater();
        if (reply->error() == QNetworkReply::NoError) {
            const QString ext = QFileInfo(QUrl(url).path()).suffix().toLower();
            QFile file(job.dir + QLatin1Char('/') + kind + QLatin1Char('.') + (ext.isEmpty() ? QStringLiteral("png") : ext));
            if (file.open(QIODevice::WriteOnly | QIODevice::Truncate))
                file.write(reply->readAll());
            // Картинки появляются по мере загрузки, не дожидаясь остальных
            emit artChanged(job.gameId);
        }
        fetchNext(job);
    });
}

void GameArt::finish(const Job &job, bool ok)
{
    QDir().mkpath(job.dir);
    QFile meta(job.dir + QStringLiteral("/meta.json"));
    if (meta.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        meta.write(QJsonDocument(QJsonObject{
            {QStringLiteral("ok"), ok},
            {QStringLiteral("sgdbId"), job.sgdbId},
            {QStringLiteral("logo"), ok},   // логотип уже искали
            {QStringLiteral("time"), double(QDateTime::currentSecsSinceEpoch())},
        }).toJson());
    }
    --m_running;
    emit artChanged(job.gameId);
    startNext();
}
