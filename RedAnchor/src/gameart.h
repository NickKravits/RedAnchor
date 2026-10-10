#pragma once

#include <QHash>
#include <QObject>
#include <QSet>
#include <QString>
#include <QStringList>
#include <QVariantMap>
#include <QtQml/qqmlregistration.h>

class QNetworkAccessManager;
class QNetworkReply;

// Обложки игр из SteamGridDB (ключ API задаётся при сборке: REDANCHOR_STEAMGRIDDB_KEY).
// Для каждой игры загружаются:
//   grid  — квадратная обложка (плитки библиотеки, магазина, «Мои игры»);
//   hero  — широкая картинка для фона главного экрана;
//   video — анимированная обложка WebM, если есть (проигрывается при наведении);
//   logo  — логотип игры с прозрачным фоном (иконка игры в оверлее по кнопке PS).
// Всё кэшируется в ~/.cache/RedAnchor/RedAnchor Shell/art/<игра>/ и повторно не скачивается.
class GameArt : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

public:
    explicit GameArt(QObject *parent = nullptr);

    // { grid, hero, video, logo } — file:// URL готовых картинок (или пустые строки).
    // Если обложек ещё нет — запускает загрузку, по готовности придёт artChanged(gameId).
    Q_INVOKABLE QVariantMap art(const QString &gameId, const QString &store, const QString &appId, const QString &title);

signals:
    void artChanged(const QString &gameId);

private:
    struct Job {
        QString gameId;
        QString store;
        QString appId;
        QString title;
        QString dir;
        int sgdbId = 0;
        QStringList pending;    // какие виды ещё не обработаны: grid, video, hero
    };

    QString cacheDir(const QString &gameId) const;
    QVariantMap localArt(const QString &gameId) const;
    QNetworkReply *api(const QString &path);
    void startNext();
    void resolveGame(Job job);
    void fetchNext(Job job);
    void download(Job job, const QString &kind, const QString &url);
    void finish(const Job &job, bool ok);

    QNetworkAccessManager *m_network = nullptr;
    QList<Job> m_queue;
    QSet<QString> m_known;      // поставлены в очередь или уже обработаны в этом запуске
    int m_running = 0;
};
