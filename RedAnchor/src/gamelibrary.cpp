#include "gamelibrary.h"

#include "accountmanager.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QFutureWatcher>
#include <QJSEngine>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSet>
#include <QStorageInfo>
#include <QtConcurrent/QtConcurrentRun>

#include <algorithm>

namespace {

qint64 directorySize(const QString &path)
{
    qint64 total = 0;
    QDirIterator it(path, QDir::Files | QDir::Hidden | QDir::System | QDir::NoSymLinks, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        it.next();
        total += it.fileInfo().size();
    }
    return total;
}

} // namespace

GameLibrary *GameLibrary::instance()
{
    static GameLibrary *library = new GameLibrary(qApp);
    return library;
}

GameLibrary *GameLibrary::create(QQmlEngine *, QJSEngine *)
{
    GameLibrary *library = instance();
    QJSEngine::setObjectOwnership(library, QJSEngine::CppOwnership);
    return library;
}

GameLibrary::GameLibrary(QObject *parent)
    : QObject(parent)
{
    connect(AccountManager::instance(), &AccountManager::accountChanged, this, [this] {
        ++m_generation;
        load();
        refreshStorage();
    });
    load();
}

QVariantMap GameLibrary::toMap(const Game &g)
{
    return {
        {QStringLiteral("id"), g.id()},
        {QStringLiteral("store"), g.store},
        {QStringLiteral("appId"), g.appId},
        {QStringLiteral("title"), g.title},
        {QStringLiteral("installed"), g.installed},
        {QStringLiteral("installPath"), g.installPath},
        {QStringLiteral("sizeBytes"), g.sizeBytes},
        {QStringLiteral("pinned"), g.pinned},
        {QStringLiteral("installedAt"), g.installedAt},
    };
}

QVariantList GameLibrary::games() const
{
    // Сначала недавно установленные
    QList<Game> sorted = m_games;
    std::sort(sorted.begin(), sorted.end(), [](const Game &a, const Game &b) { return a.installedAt > b.installedAt; });
    QVariantList list;
    for (const Game &g : sorted)
        list.append(toMap(g));
    return list;
}

QVariantList GameLibrary::pinned() const
{
    QList<Game> sorted;
    for (const Game &g : m_games) {
        if (g.pinned)
            sorted.append(g);
    }
    std::sort(sorted.begin(), sorted.end(), [](const Game &a, const Game &b) { return a.pinOrder < b.pinOrder; });
    QVariantList list;
    for (const Game &g : sorted)
        list.append(toMap(g));
    return list;
}

qint64 GameLibrary::gamesBytes() const
{
    qint64 total = 0;
    for (const Game &g : m_games) {
        if (g.installed)
            total += g.sizeBytes;
    }
    return total;
}

int GameLibrary::installedCount() const
{
    return int(std::count_if(m_games.cbegin(), m_games.cend(), [](const Game &g) { return g.installed; }));
}

bool GameLibrary::isInstalled(const QString &id) const
{
    const int index = indexOf(id);
    return index >= 0 && m_games.at(index).installed;
}

void GameLibrary::addPending(const QString &store, const QString &appId, const QString &title)
{
    Game g;
    g.store = store;
    g.appId = appId;
    if (indexOf(g.id()) >= 0)
        return;
    g.title = title;
    g.installed = false;
    g.installedAt = QDateTime::currentSecsSinceEpoch();
    m_games.append(g);
    save();
    emit gamesChanged();
}

QString GameLibrary::gamesDir(const QString &store) const
{
    const QString profile = AccountManager::instance()->profileDir();
    if (profile.isEmpty())
        return {};
    const QString dir = profile + QStringLiteral("/games/") + store;
    QDir().mkpath(dir);
    return dir;
}

int GameLibrary::indexOf(const QString &id) const
{
    for (int i = 0; i < m_games.size(); ++i) {
        if (m_games.at(i).id() == id)
            return i;
    }
    return -1;
}

bool GameLibrary::contains(const QString &id) const
{
    return indexOf(id) >= 0;
}

QVariantMap GameLibrary::game(const QString &id) const
{
    const int index = indexOf(id);
    return index >= 0 ? toMap(m_games.at(index)) : QVariantMap();
}

bool GameLibrary::isPinned(const QString &id) const
{
    const int index = indexOf(id);
    return index >= 0 && m_games.at(index).pinned;
}

void GameLibrary::pin(const QString &id)
{
    const int index = indexOf(id);
    if (index < 0 || m_games.at(index).pinned)
        return;
    m_games[index].pinned = true;
    m_games[index].pinOrder = QDateTime::currentMSecsSinceEpoch();
    save();
    emit gamesChanged();
}

void GameLibrary::unpin(const QString &id)
{
    const int index = indexOf(id);
    if (index < 0 || !m_games.at(index).pinned)
        return;
    m_games[index].pinned = false;
    save();
    emit gamesChanged();
}

void GameLibrary::addInstalled(const QString &store, const QString &appId, const QString &title,
                               const QString &installPath, qint64 sizeBytes)
{
    Game g;
    g.store = store;
    g.appId = appId;
    const int index = indexOf(g.id());
    if (index >= 0)
        g = m_games.at(index);   // переустановка: сохраняем закрепление
    g.title = title;
    g.installed = true;
    g.installPath = installPath;
    g.sizeBytes = sizeBytes;
    g.installedAt = QDateTime::currentSecsSinceEpoch();
    if (index >= 0)
        m_games[index] = g;
    else
        m_games.append(g);
    save();
    emit gamesChanged();
    emit storageChanged();
    if (sizeBytes <= 0)
        refreshStorage();
}

void GameLibrary::syncStore(const QString &store, const QList<StoreGame> &installed, const QStringList &pending)
{
    if (libraryFile().isEmpty())
        return;
    bool changed = false;
    QSet<QString> present;
    for (const StoreGame &entry : installed) {
        present.insert(entry.appId);
        Game g;
        g.store = store;
        g.appId = entry.appId;
        const int index = indexOf(g.id());
        if (index >= 0) {
            Game &existing = m_games[index];
            if (existing.installed && existing.installPath == entry.path && existing.title == entry.title)
                continue;
            if (!existing.installed)
                existing.installedAt = QDateTime::currentSecsSinceEpoch();
            existing.installed = true;
            existing.installPath = entry.path;
            existing.title = entry.title;
            if (entry.sizeBytes > 0)
                existing.sizeBytes = entry.sizeBytes;
        } else {
            g.title = entry.title;
            g.installed = true;
            g.installPath = entry.path;
            g.sizeBytes = entry.sizeBytes;
            g.installedAt = QDateTime::currentSecsSinceEpoch();
            m_games.append(g);
        }
        changed = true;
    }
    for (int i = int(m_games.size()) - 1; i >= 0; --i) {
        const Game &g = m_games.at(i);
        if (g.store != store || present.contains(g.appId))
            continue;
        // Удалили через клиент магазина или загрузка больше не идёт
        if (g.installed || !pending.contains(g.appId)) {
            m_games.removeAt(i);
            changed = true;
        }
    }
    if (!changed)
        return;
    save();
    emit gamesChanged();
    refreshStorage();
}

void GameLibrary::removeRecord(const QString &id)
{
    const int index = indexOf(id);
    if (index < 0)
        return;
    m_games.removeAt(index);
    save();
    emit gamesChanged();
    refreshStorage();
}

QString GameLibrary::libraryFile() const
{
    const QString profile = AccountManager::instance()->profileDir();
    return profile.isEmpty() ? QString() : profile + QStringLiteral("/library.json");
}

void GameLibrary::load()
{
    m_games.clear();
    QFile file(libraryFile());
    if (file.open(QIODevice::ReadOnly)) {
        const QJsonArray array = QJsonDocument::fromJson(file.readAll()).object().value(QStringLiteral("games")).toArray();
        for (const QJsonValue &value : array) {
            const QJsonObject o = value.toObject();
            Game g;
            g.store = o.value(QStringLiteral("store")).toString();
            g.appId = o.value(QStringLiteral("appId")).toString();
            g.title = o.value(QStringLiteral("title")).toString();
            g.installPath = o.value(QStringLiteral("installPath")).toString();
            g.installed = o.value(QStringLiteral("installed")).toBool(true);
            g.sizeBytes = qint64(o.value(QStringLiteral("sizeBytes")).toDouble());
            g.pinned = o.value(QStringLiteral("pinned")).toBool();
            g.pinOrder = qint64(o.value(QStringLiteral("pinOrder")).toDouble());
            g.installedAt = qint64(o.value(QStringLiteral("installedAt")).toDouble());
            // Незавершённая загрузка (оболочку перезапустили) или папку игры удалили вручную —
            // запись больше не нужна
            // Игр Steam оболочка больше не ведёт (их ставил клиент Steam) — записи пропускаем
            if (!g.store.isEmpty() && !g.appId.isEmpty() && g.installed && QFileInfo::exists(g.installPath)
                && g.store != QLatin1String("steam"))
                m_games.append(g);
        }
    }
    emit gamesChanged();
    emit storageChanged();
}

void GameLibrary::save() const
{
    const QString path = libraryFile();
    if (path.isEmpty())
        return;
    QJsonArray array;
    for (const Game &g : m_games) {
        array.append(QJsonObject{
            {QStringLiteral("store"), g.store},
            {QStringLiteral("appId"), g.appId},
            {QStringLiteral("title"), g.title},
            {QStringLiteral("installed"), g.installed},
            {QStringLiteral("installPath"), g.installPath},
            {QStringLiteral("sizeBytes"), double(g.sizeBytes)},
            {QStringLiteral("pinned"), g.pinned},
            {QStringLiteral("pinOrder"), double(g.pinOrder)},
            {QStringLiteral("installedAt"), double(g.installedAt)},
        });
    }
    QFile file(path);
    if (file.open(QIODevice::WriteOnly | QIODevice::Truncate))
        file.write(QJsonDocument(QJsonObject{{QStringLiteral("games"), array}}).toJson());
}

void GameLibrary::refreshStorage()
{
    const QString profile = AccountManager::instance()->profileDir();
    const QStorageInfo storage(profile.isEmpty() ? QDir::homePath() : profile);
    m_diskTotal = storage.bytesTotal();
    m_diskFree = storage.bytesAvailable();
    emit storageChanged();

    if (m_measuring || installedCount() == 0)
        return;
    m_measuring = true;
    emit storageChanged();

    // Размеры папок считаем в фоне — у больших игр это десятки тысяч файлов
    QStringList paths;
    for (const Game &g : std::as_const(m_games)) {
        if (g.installed)
            paths.append(g.installPath);
    }
    const int generation = m_generation;
    auto *watcher = new QFutureWatcher<QList<qint64>>(this);
    connect(watcher, &QFutureWatcher<QList<qint64>>::finished, this, [this, watcher, paths, generation] {
        watcher->deleteLater();
        m_measuring = false;
        if (generation == m_generation) {
            const QList<qint64> sizes = watcher->result();
            for (int i = 0; i < paths.size() && i < sizes.size(); ++i) {
                for (Game &g : m_games) {
                    if (g.installPath == paths.at(i))
                        g.sizeBytes = sizes.at(i);
                }
            }
            save();
            emit gamesChanged();
        }
        emit storageChanged();
    });
    watcher->setFuture(QtConcurrent::run([paths] {
        QList<qint64> sizes;
        for (const QString &path : paths)
            sizes.append(directorySize(path));
        return sizes;
    }));
}
