#pragma once

#include <QByteArray>
#include <QHash>
#include <QList>
#include <QString>
#include <QStringList>

#include <functional>

// Форматы файлов клиента Steam.
//
// Текстовый VDF (KeyValues): loginusers.vdf, libraryfolders.vdf, appmanifest_*.acf,
// config.vdf, registry.vdf, localconfig.vdf. Узел — либо строка, либо объект с детьми;
// ключи в Steam сравниваются без учёта регистра, порядок детей сохраняется при записи.
//
// Двоичный кэш клиента: appcache/appinfo.vdf (сведения о приложениях: название, тип,
// папка установки) и appcache/packageinfo.vdf (пакеты лицензий → приложения).
namespace Vdf {

struct Node {
    QString key;
    QString value;
    QList<Node> children;
    bool object = false;

    bool isNull() const { return key.isEmpty() && !object && value.isEmpty(); }
    // Дочерний узел по ключу (без учёта регистра); пустой узел, если его нет
    const Node &operator[](const QString &key) const;
    const Node *find(const QString &key) const;
    Node *find(const QString &key);
    // Путь через «/»: "Software/Valve/Steam/apps"
    const Node &at(const QString &path) const;
    QString str(const QString &key, const QString &fallback = QString()) const;
    qint64 integer(const QString &key, qint64 fallback = 0) const;

    // Найти или создать дочерний объект (или цепочку объектов через «/»)
    Node &child(const QString &path);
    void set(const QString &key, const QString &value);
    bool remove(const QString &key);
};

// Текстовый VDF. Корень — безымянный объект, его дети — узлы верхнего уровня.
Node parse(const QByteArray &text);
Node parseFile(const QString &path, bool *ok = nullptr);
QByteArray serialize(const Node &root);
// Запись через временный файл: Steam не должен увидеть половину файла
bool writeFile(const QString &path, const Node &root);

// ---------- Двоичный кэш ----------

struct AppInfo {
    quint32 appId = 0;
    QString name;
    QString type;        // "game", "application", "tool", "dlc", … (в нижнем регистре)
    QString installDir;  // config/installdir
    QString osList;      // common/oslist: "windows,linux,macos"
    QString controllerSupport;   // common/controller_support: "full", "partial", ""
};

// Читает appinfo.vdf. Если wanted не пуст — только эти приложения (остальные пропускаются
// без разбора, это быстро даже для кэша в сотни мегабайт).
bool readAppInfo(const QString &path, const QHash<quint32, bool> &wanted, QHash<quint32, AppInfo> *out);
// Читает packageinfo.vdf: id пакета → приложения пакета
bool readPackageInfo(const QString &path, QHash<quint32, QList<quint32>> *out);

} // namespace Vdf
