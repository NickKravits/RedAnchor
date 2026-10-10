/****************************************************************************
** Meta object code from reading C++ file 'accountmanager.h'
**
** Created by: The Qt Meta Object Compiler version 69 (Qt 6.12.0)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "../../../src/accountmanager.h"
#include <QtCore/qmetatype.h>

#include <QtCore/qtmochelpers.h>

#include <memory>


#include <QtCore/qxptype_traits.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'accountmanager.h' doesn't include <QObject>."
#elif Q_MOC_OUTPUT_REVISION != 69
#error "This file was generated using the moc from 6.12.0. It"
#error "cannot be used with the include files from this version of Qt."
#error "(The moc has changed too much.)"
#endif

#ifndef Q_CONSTINIT
#define Q_CONSTINIT
#endif

QT_WARNING_PUSH
QT_WARNING_DISABLE_DEPRECATED
QT_WARNING_DISABLE_GCC("-Wuseless-cast")
namespace {
struct qt_meta_tag_ZN14AccountManagerE_t {};
} // unnamed namespace

template <> constexpr inline auto AccountManager::qt_create_metaobjectdata<qt_meta_tag_ZN14AccountManagerE_t>()
{
    namespace QMC = QtMocConstants;
    QtMocHelpers::StringRefStorage qt_stringData {
        "AccountManager",
        "QML.Element",
        "auto",
        "QML.Singleton",
        "true",
        "accountsChanged",
        "",
        "accountChanged",
        "validateNickname",
        "nickname",
        "createLocal",
        "makeCurrent",
        "signInOnline",
        "email",
        "displayName",
        "selectAccount",
        "id",
        "removeAccount",
        "accounts",
        "QVariantList",
        "count",
        "lastAccountId",
        "hasAccount",
        "type",
        "local",
        "createdAt",
        "profileDir"
    };

    QtMocHelpers::UintData qt_methods {
        // Signal 'accountsChanged'
        QtMocHelpers::SignalData<void()>(5, 6, QMC::AccessPublic, QMetaType::Void),
        // Signal 'accountChanged'
        QtMocHelpers::SignalData<void()>(7, 6, QMC::AccessPublic, QMetaType::Void),
        // Method 'validateNickname'
        QtMocHelpers::MethodData<QString(const QString &) const>(8, 6, QMC::AccessPublic, QMetaType::QString, {{
            { QMetaType::QString, 9 },
        }}),
        // Method 'createLocal'
        QtMocHelpers::MethodData<bool(const QString &, bool)>(10, 6, QMC::AccessPublic, QMetaType::Bool, {{
            { QMetaType::QString, 9 }, { QMetaType::Bool, 11 },
        }}),
        // Method 'createLocal'
        QtMocHelpers::MethodData<bool(const QString &)>(10, 6, QMC::AccessPublic | QMC::MethodCloned, QMetaType::Bool, {{
            { QMetaType::QString, 9 },
        }}),
        // Method 'signInOnline'
        QtMocHelpers::MethodData<void(const QString &, const QString &)>(12, 6, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 13 }, { QMetaType::QString, 14 },
        }}),
        // Method 'selectAccount'
        QtMocHelpers::MethodData<bool(const QString &)>(15, 6, QMC::AccessPublic, QMetaType::Bool, {{
            { QMetaType::QString, 16 },
        }}),
        // Method 'removeAccount'
        QtMocHelpers::MethodData<bool(const QString &)>(17, 6, QMC::AccessPublic, QMetaType::Bool, {{
            { QMetaType::QString, 16 },
        }}),
    };
    QtMocHelpers::UintData qt_properties {
        // property 'accounts'
        QtMocHelpers::PropertyData<QVariantList>(18, 0x80000000 | 19, QMC::DefaultPropertyFlags | QMC::EnumOrFlag, 0),
        // property 'count'
        QtMocHelpers::PropertyData<int>(20, QMetaType::Int, QMC::DefaultPropertyFlags, 0),
        // property 'lastAccountId'
        QtMocHelpers::PropertyData<QString>(21, QMetaType::QString, QMC::DefaultPropertyFlags, 0),
        // property 'hasAccount'
        QtMocHelpers::PropertyData<bool>(22, QMetaType::Bool, QMC::DefaultPropertyFlags, 1),
        // property 'id'
        QtMocHelpers::PropertyData<QString>(16, QMetaType::QString, QMC::DefaultPropertyFlags, 1),
        // property 'type'
        QtMocHelpers::PropertyData<QString>(23, QMetaType::QString, QMC::DefaultPropertyFlags, 1),
        // property 'local'
        QtMocHelpers::PropertyData<bool>(24, QMetaType::Bool, QMC::DefaultPropertyFlags, 1),
        // property 'nickname'
        QtMocHelpers::PropertyData<QString>(9, QMetaType::QString, QMC::DefaultPropertyFlags, 1),
        // property 'email'
        QtMocHelpers::PropertyData<QString>(13, QMetaType::QString, QMC::DefaultPropertyFlags, 1),
        // property 'createdAt'
        QtMocHelpers::PropertyData<QDateTime>(25, QMetaType::QDateTime, QMC::DefaultPropertyFlags, 1),
        // property 'profileDir'
        QtMocHelpers::PropertyData<QString>(26, QMetaType::QString, QMC::DefaultPropertyFlags, 1),
    };
    QtMocHelpers::UintData qt_enums {
    };
    QtMocHelpers::UintData qt_constructors {};
    QtMocHelpers::ClassInfos qt_classinfo({
            {    1,    2 },
            {    3,    4 },
    });
    return QtMocHelpers::metaObjectData<AccountManager, void>(QMC::MetaObjectFlag{}, qt_stringData,
            qt_methods, qt_properties, qt_enums, qt_constructors, qt_classinfo);
}
Q_CONSTINIT const QMetaObject AccountManager::staticMetaObject = { {
    QMetaObject::SuperData::link<QObject::staticMetaObject>(),
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN14AccountManagerE_t>.stringdata,
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN14AccountManagerE_t>.data,
    qt_static_metacall,
    nullptr,
    qt_staticMetaObjectRelocatingContent<qt_meta_tag_ZN14AccountManagerE_t>.metaTypes,
    nullptr
} };

void AccountManager::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    auto *_t = static_cast<AccountManager *>(_o);
    if (_c == QMetaObject::InvokeMetaMethod) {
        switch (_id) {
        case 0: _t->accountsChanged(); break;
        case 1: _t->accountChanged(); break;
        case 2: { QString _r = _t->validateNickname((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1])));
            if (_a[0]) *reinterpret_cast<QString*>(_a[0]) = std::move(_r); }  break;
        case 3: { bool _r = _t->createLocal((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<bool>>(_a[2])));
            if (_a[0]) *reinterpret_cast<bool*>(_a[0]) = std::move(_r); }  break;
        case 4: { bool _r = _t->createLocal((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1])));
            if (_a[0]) *reinterpret_cast<bool*>(_a[0]) = std::move(_r); }  break;
        case 5: _t->signInOnline((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<QString>>(_a[2]))); break;
        case 6: { bool _r = _t->selectAccount((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1])));
            if (_a[0]) *reinterpret_cast<bool*>(_a[0]) = std::move(_r); }  break;
        case 7: { bool _r = _t->removeAccount((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1])));
            if (_a[0]) *reinterpret_cast<bool*>(_a[0]) = std::move(_r); }  break;
        default: ;
        }
    }
    if (_c == QMetaObject::IndexOfMethod) {
        if (QtMocHelpers::indexOfMethod<void (AccountManager::*)()>(_a, &AccountManager::accountsChanged, 0))
            return;
        if (QtMocHelpers::indexOfMethod<void (AccountManager::*)()>(_a, &AccountManager::accountChanged, 1))
            return;
    }
    if (_c == QMetaObject::ReadProperty) {
        void *_v = _a[0];
        switch (_id) {
        case 0: *reinterpret_cast<QVariantList*>(_v) = _t->accounts(); break;
        case 1: *reinterpret_cast<int*>(_v) = _t->count(); break;
        case 2: *reinterpret_cast<QString*>(_v) = _t->lastAccountId(); break;
        case 3: *reinterpret_cast<bool*>(_v) = _t->hasAccount(); break;
        case 4: *reinterpret_cast<QString*>(_v) = _t->id(); break;
        case 5: *reinterpret_cast<QString*>(_v) = _t->type(); break;
        case 6: *reinterpret_cast<bool*>(_v) = _t->isLocal(); break;
        case 7: *reinterpret_cast<QString*>(_v) = _t->nickname(); break;
        case 8: *reinterpret_cast<QString*>(_v) = _t->email(); break;
        case 9: *reinterpret_cast<QDateTime*>(_v) = _t->createdAt(); break;
        case 10: *reinterpret_cast<QString*>(_v) = _t->profileDir(); break;
        default: break;
        }
    }
}

const QMetaObject *AccountManager::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *AccountManager::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_staticMetaObjectStaticContent<qt_meta_tag_ZN14AccountManagerE_t>.strings))
        return static_cast<void*>(this);
    return QObject::qt_metacast(_clname);
}

int AccountManager::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QObject::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 8)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 8;
    }
    if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 8)
            *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType();
        _id -= 8;
    }
    if (_c == QMetaObject::ReadProperty || _c == QMetaObject::WriteProperty
            || _c == QMetaObject::ResetProperty || _c == QMetaObject::BindableProperty
            || _c == QMetaObject::RegisterPropertyMetaType) {
        qt_static_metacall(this, _c, _id, _a);
        _id -= 11;
    }
    return _id;
}

// SIGNAL 0
void AccountManager::accountsChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 0, nullptr);
}

// SIGNAL 1
void AccountManager::accountChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 1, nullptr);
}
QT_WARNING_POP
