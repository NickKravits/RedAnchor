#pragma once

#include <QDateTime>
#include <QList>
#include <QObject>
#include <QString>
#include <QVariantList>
#include <QtQml/qqmlregistration.h>

class QJSEngine;
class QQmlEngine;

// Учётные записи пользователей консоли.
//
// Запись бывает двух видов:
//   "local"  — локальная, создаётся по никнейму, если вход в аккаунт RedAnchor пропущен;
//   "online" — аккаунт RedAnchor (пока вход фиктивный, сервера нет).
// У каждой записи постоянный id (UUID) и своя папка профиля
// ~/.local/share/RedAnchor/RedAnchor Shell/profiles/<id> — туда попадают
// сохранения, настройки и прочие данные пользователя.
//
// Текущий пользователь выбирается при каждом запуске (экран выбора пользователя);
// последний выбранный запоминается, чтобы подсветить его в списке.
// Хранится в общем файле настроек оболочки: массив [Accounts], ключ Session/lastAccount.
class AccountManager : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

    // Все записи: [{ id, type, local, nickname, email, createdAt, profileDir }]
    Q_PROPERTY(QVariantList accounts READ accounts NOTIFY accountsChanged)
    Q_PROPERTY(int count READ count NOTIFY accountsChanged)
    Q_PROPERTY(QString lastAccountId READ lastAccountId NOTIFY accountsChanged)

    // Текущий пользователь
    Q_PROPERTY(bool hasAccount READ hasAccount NOTIFY accountChanged)
    Q_PROPERTY(QString id READ id NOTIFY accountChanged)
    Q_PROPERTY(QString type READ type NOTIFY accountChanged)
    Q_PROPERTY(bool local READ isLocal NOTIFY accountChanged)
    Q_PROPERTY(QString nickname READ nickname NOTIFY accountChanged)
    Q_PROPERTY(QString email READ email NOTIFY accountChanged)
    Q_PROPERTY(QDateTime createdAt READ createdAt NOTIFY accountChanged)
    Q_PROPERTY(QString profileDir READ profileDir NOTIFY accountChanged)

public:
    static constexpr int NicknameMin = 3;
    static constexpr int NicknameMax = 16;

    // Один экземпляр на приложение: им пользуются и QML, и C++ (библиотека игр, магазины)
    static AccountManager *instance();
    static AccountManager *create(QQmlEngine *, QJSEngine *);

    QVariantList accounts() const;
    int count() const { return int(m_accounts.size()); }
    QString lastAccountId() const { return m_lastId; }

    bool hasAccount() const { return m_current >= 0; }
    QString id() const { return current().id; }
    QString type() const { return current().type; }
    bool isLocal() const { return current().type == QLatin1String("local"); }
    QString nickname() const { return current().nickname; }
    QString email() const { return current().email; }
    QDateTime createdAt() const { return current().createdAt; }
    QString profileDir() const { return profileDirFor(current().id); }

    // Пустая строка — никнейм подходит, иначе текст ошибки для пользователя.
    Q_INVOKABLE QString validateNickname(const QString &nickname) const;

    // Создать локальную учётную запись; makeCurrent — сразу сделать её текущей
    // (в мастере настройки), иначе просто добавить (из настроек «Пользователи»).
    // Возвращает false, если никнейм не подходит.
    Q_INVOKABLE bool createLocal(const QString &nickname, bool makeCurrent = true);
    // Вход в аккаунт RedAnchor: такая почта уже есть — выбираем её, иначе добавляем запись.
    Q_INVOKABLE void signInOnline(const QString &email, const QString &displayName);
    // Сделать запись текущей (экран выбора пользователя).
    Q_INVOKABLE bool selectAccount(const QString &id);
    // Удалить запись (не текущую) вместе с папкой профиля — со всеми играми и данными.
    Q_INVOKABLE bool removeAccount(const QString &id);

signals:
    void accountsChanged();
    void accountChanged();

private:
    explicit AccountManager(QObject *parent = nullptr);

    struct Account {
        QString id;
        QString type;
        QString nickname;
        QString email;
        QDateTime createdAt;
    };

    static QString profileDirFor(const QString &id);
    const Account &current() const;
    int indexOf(const QString &id) const;
    void addAccount(const QString &type, const QString &nickname, const QString &email, bool makeCurrent = true);
    void setCurrent(int index);
    void load();
    void save() const;

    QList<Account> m_accounts;
    int m_current = -1;
    QString m_lastId;
};
