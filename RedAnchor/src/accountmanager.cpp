#include "accountmanager.h"

#include <QCoreApplication>
#include <QDir>
#include <QJSEngine>
#include <QRegularExpression>
#include <QSettings>
#include <QStandardPaths>
#include <QUuid>

namespace {

const QString kArray = QStringLiteral("Accounts");
const QString kLastKey = QStringLiteral("Session/lastAccount");

} // namespace

AccountManager *AccountManager::instance()
{
    static AccountManager *manager = new AccountManager(qApp);
    return manager;
}

AccountManager *AccountManager::create(QQmlEngine *, QJSEngine *)
{
    AccountManager *manager = instance();
    QJSEngine::setObjectOwnership(manager, QJSEngine::CppOwnership);
    return manager;
}

AccountManager::AccountManager(QObject *parent)
    : QObject(parent)
{
    load();
}

QString AccountManager::profileDirFor(const QString &id)
{
    if (id.isEmpty())
        return {};
    return QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + QStringLiteral("/profiles/") + id;
}

const AccountManager::Account &AccountManager::current() const
{
    static const Account none;
    return m_current >= 0 ? m_accounts.at(m_current) : none;
}

int AccountManager::indexOf(const QString &id) const
{
    for (int i = 0; i < m_accounts.size(); ++i) {
        if (m_accounts.at(i).id == id)
            return i;
    }
    return -1;
}

QVariantList AccountManager::accounts() const
{
    QVariantList list;
    for (const Account &a : m_accounts) {
        list.append(QVariantMap{
            {QStringLiteral("id"), a.id},
            {QStringLiteral("type"), a.type},
            {QStringLiteral("local"), a.type == QLatin1String("local")},
            {QStringLiteral("nickname"), a.nickname},
            {QStringLiteral("email"), a.email},
            {QStringLiteral("createdAt"), a.createdAt},
            {QStringLiteral("profileDir"), profileDirFor(a.id)},
        });
    }
    return list;
}

QString AccountManager::validateNickname(const QString &nickname) const
{
    const QString nick = nickname.trimmed();
    if (nick.isEmpty())
        return tr("Введите никнейм.");
    if (nick.size() < NicknameMin)
        return tr("Никнейм должен быть не короче %1 символов.").arg(NicknameMin);
    if (nick.size() > NicknameMax)
        return tr("Никнейм должен быть не длиннее %1 символов.").arg(NicknameMax);
    // Буквы любого алфавита, цифры, «_», «-» и «.»
    static const QRegularExpression allowed(QStringLiteral("^[\\p{L}\\p{N}_.\\-]+$"));
    if (!allowed.match(nick).hasMatch())
        return tr("Можно использовать только буквы, цифры и символы _ - .");
    static const QRegularExpression letter(QStringLiteral("\\p{L}"));
    if (!letter.match(nick).hasMatch())
        return tr("Никнейм должен содержать хотя бы одну букву.");
    for (const Account &a : m_accounts) {
        if (a.nickname.compare(nick, Qt::CaseInsensitive) == 0)
            return tr("Пользователь с таким никнеймом уже есть.");
    }
    return {};
}

bool AccountManager::createLocal(const QString &nickname, bool makeCurrent)
{
    if (!validateNickname(nickname).isEmpty())
        return false;
    addAccount(QStringLiteral("local"), nickname.trimmed(), QString(), makeCurrent);
    return true;
}

void AccountManager::signInOnline(const QString &email, const QString &displayName)
{
    const QString mail = email.trimmed();
    for (int i = 0; i < m_accounts.size(); ++i) {
        if (m_accounts.at(i).type == QLatin1String("online")
            && m_accounts.at(i).email.compare(mail, Qt::CaseInsensitive) == 0) {
            setCurrent(i);
            return;
        }
    }
    addAccount(QStringLiteral("online"), displayName.trimmed(), mail);
}

bool AccountManager::selectAccount(const QString &id)
{
    const int index = indexOf(id);
    if (index < 0)
        return false;
    setCurrent(index);
    return true;
}

bool AccountManager::removeAccount(const QString &id)
{
    const int index = indexOf(id);
    if (index < 0 || index == m_current)
        return false;
    const QString currentId = current().id;
    // Вместе с записью удаляются все её игры и данные
    const QString dir = profileDirFor(id);
    if (!dir.isEmpty() && !QDir(dir).removeRecursively())
        qWarning("Failed to remove profile directory %s", qPrintable(dir));
    m_accounts.removeAt(index);
    m_current = indexOf(currentId);
    if (m_lastId == id)
        m_lastId = currentId;
    save();
    emit accountsChanged();
    return true;
}

void AccountManager::addAccount(const QString &type, const QString &nickname, const QString &email, bool makeCurrent)
{
    Account a;
    a.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    a.type = type;
    a.nickname = nickname;
    a.email = email;
    a.createdAt = QDateTime::currentDateTimeUtc();
    if (!QDir().mkpath(profileDirFor(a.id)))
        qWarning("Failed to create profile directory %s", qPrintable(profileDirFor(a.id)));
    m_accounts.append(a);
    qInfo("Account created: %s (%s, %s)", qPrintable(a.nickname), qPrintable(a.type), qPrintable(a.id));
    if (makeCurrent) {
        setCurrent(int(m_accounts.size()) - 1);
    } else {
        save();
        emit accountsChanged();
    }
}

void AccountManager::setCurrent(int index)
{
    m_current = index;
    m_lastId = current().id;
    save();
    emit accountsChanged();
    emit accountChanged();
}

void AccountManager::load()
{
    QSettings settings;
    const int size = settings.beginReadArray(kArray);
    for (int i = 0; i < size; ++i) {
        settings.setArrayIndex(i);
        Account a;
        a.id = settings.value(QStringLiteral("id")).toString();
        a.type = settings.value(QStringLiteral("type")).toString();
        a.nickname = settings.value(QStringLiteral("nickname")).toString();
        a.email = settings.value(QStringLiteral("email")).toString();
        a.createdAt = settings.value(QStringLiteral("createdAt")).toDateTime();
        if (!a.id.isEmpty())
            m_accounts.append(a);
    }
    settings.endArray();
    m_lastId = settings.value(kLastKey).toString();

    if (!m_accounts.isEmpty())
        return;

    // Записи из прежних версий: одна запись в группе [Account] или имя/почта в [Setup].
    Account a;
    a.id = settings.value(QStringLiteral("Account/id")).toString();
    a.type = settings.value(QStringLiteral("Account/type")).toString();
    a.nickname = settings.value(QStringLiteral("Account/nickname")).toString();
    a.email = settings.value(QStringLiteral("Account/email")).toString();
    a.createdAt = settings.value(QStringLiteral("Account/createdAt")).toDateTime();
    if (a.nickname.isEmpty()) {
        a.nickname = settings.value(QStringLiteral("Setup/accountName")).toString();
        a.email = settings.value(QStringLiteral("Setup/accountEmail")).toString();
        a.type = a.email.isEmpty() ? QStringLiteral("local") : QStringLiteral("online");
    }
    if (a.nickname.isEmpty())
        return;
    if (a.id.isEmpty()) {
        a.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
        a.createdAt = QDateTime::currentDateTimeUtc();
        QDir().mkpath(profileDirFor(a.id));
    }
    m_accounts.append(a);
    m_lastId = a.id;
    settings.remove(QStringLiteral("Account"));
    settings.remove(QStringLiteral("Setup/accountName"));
    settings.remove(QStringLiteral("Setup/accountEmail"));
    save();
}

void AccountManager::save() const
{
    QSettings settings;
    settings.remove(kArray);
    settings.beginWriteArray(kArray, int(m_accounts.size()));
    for (int i = 0; i < m_accounts.size(); ++i) {
        const Account &a = m_accounts.at(i);
        settings.setArrayIndex(i);
        settings.setValue(QStringLiteral("id"), a.id);
        settings.setValue(QStringLiteral("type"), a.type);
        settings.setValue(QStringLiteral("nickname"), a.nickname);
        settings.setValue(QStringLiteral("email"), a.email);
        settings.setValue(QStringLiteral("createdAt"), a.createdAt);
    }
    settings.endArray();
    settings.setValue(kLastKey, m_lastId);
}
