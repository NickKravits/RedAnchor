#pragma once

#include <QList>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QVariantList>
#include <QVariantMap>
#include <QtQml/qqmlregistration.h>

class QJSEngine;
class QQmlEngine;

// Установленные игры текущего пользователя. У каждого пользователя свои игры:
// они лежат в его профиле (profiles/<id>/games/<магазин>/…), а список — в
// profiles/<id>/library.json. Удаление учётной записи удаляет и игры.
//
// Игра попадает сюда, как только её поставили на загрузку (installed = false), и остаётся
// после установки. Закрепить на главном экране («Мои игры») можно и загружаемую игру.
// Незавершённые загрузки не переживают перезапуск оболочки — такие записи убираются. Ещё класс считает занятое место для
// раздела «Хранилище»: свои игры по отдельности, всё остальное — «Остальное».
class GameLibrary : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

    // [{ id, store, appId, title, installed, installPath, sizeBytes, pinned, installedAt }], id = "<магазин>:<appId>"
    Q_PROPERTY(QVariantList games READ games NOTIFY gamesChanged)
    // Закреплённые на главном экране, в порядке закрепления
    Q_PROPERTY(QVariantList pinned READ pinned NOTIFY gamesChanged)
    Q_PROPERTY(int count READ count NOTIFY gamesChanged)
    // Только установленные (без загружаемых)
    Q_PROPERTY(int installedCount READ installedCount NOTIFY gamesChanged)

    // Диск, на котором лежит профиль
    Q_PROPERTY(qint64 diskTotal READ diskTotal NOTIFY storageChanged)
    Q_PROPERTY(qint64 diskFree READ diskFree NOTIFY storageChanged)
    // Сколько занимают игры текущего пользователя
    Q_PROPERTY(qint64 gamesBytes READ gamesBytes NOTIFY storageChanged)
    // Идёт подсчёт размеров
    Q_PROPERTY(bool measuring READ measuring NOTIFY storageChanged)

public:
    static GameLibrary *instance();
    static GameLibrary *create(QQmlEngine *, QJSEngine *);

    QVariantList games() const;
    QVariantList pinned() const;
    int count() const { return int(m_games.size()); }
    int installedCount() const;
    qint64 diskTotal() const { return m_diskTotal; }
    qint64 diskFree() const { return m_diskFree; }
    qint64 gamesBytes() const;
    bool measuring() const { return m_measuring; }

    // Каталог игр магазина в профиле текущего пользователя (создаётся при надобности)
    QString gamesDir(const QString &store) const;

    // Для StoreManager: установленные игры магазина, библиотекой которого управляет его
    // клиент (Steam). Добавляет новые, обновляет пути, убирает удалённые; незавершённые
    // загрузки остаются, пока они есть в pending.
    struct StoreGame {
        QString appId;
        QString title;
        QString path;
        qint64 sizeBytes = 0;
    };
    void syncStore(const QString &store, const QList<StoreGame> &installed, const QStringList &pending);

    // Для StoreManager: игра поставлена на загрузку / установлена / удалена
    void addPending(const QString &store, const QString &appId, const QString &title);
    void addInstalled(const QString &store, const QString &appId, const QString &title,
                      const QString &installPath, qint64 sizeBytes);
    void removeRecord(const QString &id);

    Q_INVOKABLE bool contains(const QString &id) const;
    Q_INVOKABLE bool isInstalled(const QString &id) const;
    Q_INVOKABLE QVariantMap game(const QString &id) const;
    Q_INVOKABLE bool isPinned(const QString &id) const;
    Q_INVOKABLE void pin(const QString &id);
    Q_INVOKABLE void unpin(const QString &id);
    // Пересчитать размеры игр и свободное место
    Q_INVOKABLE void refreshStorage();

signals:
    void gamesChanged();
    void storageChanged();

private:
    explicit GameLibrary(QObject *parent = nullptr);

    struct Game {
        QString store;
        QString appId;
        QString title;
        bool installed = true;
        QString installPath;
        qint64 sizeBytes = 0;
        bool pinned = false;
        qint64 pinOrder = 0;
        qint64 installedAt = 0;
        QString id() const { return store + QLatin1Char(':') + appId; }
    };

    static QVariantMap toMap(const Game &g);
    int indexOf(const QString &id) const;
    QString libraryFile() const;
    void load();
    void save() const;

    QList<Game> m_games;
    qint64 m_diskTotal = 0;
    qint64 m_diskFree = 0;
    bool m_measuring = false;
    int m_generation = 0;   // защищает от результатов подсчёта для прежнего пользователя
};
