/****************************************************************************
** Meta object code from reading C++ file 'storemanager.h'
**
** Created by: The Qt Meta Object Compiler version 69 (Qt 6.12.0)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "../../../src/storemanager.h"
#include <QtCore/qmetatype.h>

#include <QtCore/qtmochelpers.h>

#include <memory>


#include <QtCore/qxptype_traits.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'storemanager.h' doesn't include <QObject>."
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
struct qt_meta_tag_ZN12StoreManagerE_t {};
} // unnamed namespace

template <> constexpr inline auto StoreManager::qt_create_metaobjectdata<qt_meta_tag_ZN12StoreManagerE_t>()
{
    namespace QMC = QtMocConstants;
    QtMocHelpers::StringRefStorage qt_stringData {
        "StoreManager",
        "QML.Element",
        "auto",
        "QML.Singleton",
        "true",
        "storesChanged",
        "",
        "ownedChanged",
        "downloadsChanged",
        "gameInstalled",
        "title",
        "gameRemoved",
        "compatibilityChanged",
        "gameId",
        "refreshTools",
        "loginUrl",
        "store",
        "completeWebLogin",
        "code",
        "steamQrLogin",
        "steamLogin",
        "user",
        "password",
        "steamGuard",
        "cancelSteamLogin",
        "signOut",
        "refresh",
        "searchSteam",
        "term",
        "install",
        "appId",
        "cancel",
        "uninstall",
        "compatibility",
        "QVariantMap",
        "stores",
        "owned",
        "downloads",
        "QVariantList",
        "activeDownloads",
        "downloadProgress",
        "downloadTitle"
    };

    QtMocHelpers::UintData qt_methods {
        // Signal 'storesChanged'
        QtMocHelpers::SignalData<void()>(5, 6, QMC::AccessPublic, QMetaType::Void),
        // Signal 'ownedChanged'
        QtMocHelpers::SignalData<void()>(7, 6, QMC::AccessPublic, QMetaType::Void),
        // Signal 'downloadsChanged'
        QtMocHelpers::SignalData<void()>(8, 6, QMC::AccessPublic, QMetaType::Void),
        // Signal 'gameInstalled'
        QtMocHelpers::SignalData<void(const QString &)>(9, 6, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 10 },
        }}),
        // Signal 'gameRemoved'
        QtMocHelpers::SignalData<void(const QString &)>(11, 6, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 10 },
        }}),
        // Signal 'compatibilityChanged'
        QtMocHelpers::SignalData<void(const QString &)>(12, 6, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 13 },
        }}),
        // Method 'refreshTools'
        QtMocHelpers::MethodData<void()>(14, 6, QMC::AccessPublic, QMetaType::Void),
        // Method 'loginUrl'
        QtMocHelpers::MethodData<QString(const QString &) const>(15, 6, QMC::AccessPublic, QMetaType::QString, {{
            { QMetaType::QString, 16 },
        }}),
        // Method 'completeWebLogin'
        QtMocHelpers::MethodData<void(const QString &, const QString &)>(17, 6, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 16 }, { QMetaType::QString, 18 },
        }}),
        // Method 'steamQrLogin'
        QtMocHelpers::MethodData<void()>(19, 6, QMC::AccessPublic, QMetaType::Void),
        // Method 'steamLogin'
        QtMocHelpers::MethodData<void(const QString &, const QString &)>(20, 6, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 21 }, { QMetaType::QString, 22 },
        }}),
        // Method 'steamGuard'
        QtMocHelpers::MethodData<void(const QString &)>(23, 6, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 18 },
        }}),
        // Method 'cancelSteamLogin'
        QtMocHelpers::MethodData<void()>(24, 6, QMC::AccessPublic, QMetaType::Void),
        // Method 'signOut'
        QtMocHelpers::MethodData<void(const QString &)>(25, 6, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 16 },
        }}),
        // Method 'refresh'
        QtMocHelpers::MethodData<void(const QString &)>(26, 6, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 16 },
        }}),
        // Method 'searchSteam'
        QtMocHelpers::MethodData<void(const QString &)>(27, 6, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 28 },
        }}),
        // Method 'install'
        QtMocHelpers::MethodData<void(const QString &, const QString &, const QString &)>(29, 6, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 16 }, { QMetaType::QString, 30 }, { QMetaType::QString, 10 },
        }}),
        // Method 'cancel'
        QtMocHelpers::MethodData<void(const QString &)>(31, 6, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 13 },
        }}),
        // Method 'uninstall'
        QtMocHelpers::MethodData<void(const QString &)>(32, 6, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 13 },
        }}),
        // Method 'compatibility'
        QtMocHelpers::MethodData<QVariantMap(const QString &, const QString &)>(33, 6, QMC::AccessPublic, 0x80000000 | 34, {{
            { QMetaType::QString, 16 }, { QMetaType::QString, 30 },
        }}),
    };
    QtMocHelpers::UintData qt_properties {
        // property 'stores'
        QtMocHelpers::PropertyData<QVariantMap>(35, 0x80000000 | 34, QMC::DefaultPropertyFlags | QMC::EnumOrFlag, 0),
        // property 'owned'
        QtMocHelpers::PropertyData<QVariantMap>(36, 0x80000000 | 34, QMC::DefaultPropertyFlags | QMC::EnumOrFlag, 1),
        // property 'downloads'
        QtMocHelpers::PropertyData<QVariantList>(37, 0x80000000 | 38, QMC::DefaultPropertyFlags | QMC::EnumOrFlag, 2),
        // property 'activeDownloads'
        QtMocHelpers::PropertyData<int>(39, QMetaType::Int, QMC::DefaultPropertyFlags, 2),
        // property 'downloadProgress'
        QtMocHelpers::PropertyData<double>(40, QMetaType::Double, QMC::DefaultPropertyFlags, 2),
        // property 'downloadTitle'
        QtMocHelpers::PropertyData<QString>(41, QMetaType::QString, QMC::DefaultPropertyFlags, 2),
    };
    QtMocHelpers::UintData qt_enums {
    };
    QtMocHelpers::UintData qt_constructors {};
    QtMocHelpers::ClassInfos qt_classinfo({
            {    1,    2 },
            {    3,    4 },
    });
    return QtMocHelpers::metaObjectData<StoreManager, void>(QMC::MetaObjectFlag{}, qt_stringData,
            qt_methods, qt_properties, qt_enums, qt_constructors, qt_classinfo);
}
Q_CONSTINIT const QMetaObject StoreManager::staticMetaObject = { {
    QMetaObject::SuperData::link<QObject::staticMetaObject>(),
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN12StoreManagerE_t>.stringdata,
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN12StoreManagerE_t>.data,
    qt_static_metacall,
    nullptr,
    qt_staticMetaObjectRelocatingContent<qt_meta_tag_ZN12StoreManagerE_t>.metaTypes,
    nullptr
} };

void StoreManager::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    auto *_t = static_cast<StoreManager *>(_o);
    if (_c == QMetaObject::InvokeMetaMethod) {
        switch (_id) {
        case 0: _t->storesChanged(); break;
        case 1: _t->ownedChanged(); break;
        case 2: _t->downloadsChanged(); break;
        case 3: _t->gameInstalled((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1]))); break;
        case 4: _t->gameRemoved((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1]))); break;
        case 5: _t->compatibilityChanged((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1]))); break;
        case 6: _t->refreshTools(); break;
        case 7: { QString _r = _t->loginUrl((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1])));
            if (_a[0]) *reinterpret_cast<QString*>(_a[0]) = std::move(_r); }  break;
        case 8: _t->completeWebLogin((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<QString>>(_a[2]))); break;
        case 9: _t->steamQrLogin(); break;
        case 10: _t->steamLogin((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<QString>>(_a[2]))); break;
        case 11: _t->steamGuard((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1]))); break;
        case 12: _t->cancelSteamLogin(); break;
        case 13: _t->signOut((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1]))); break;
        case 14: _t->refresh((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1]))); break;
        case 15: _t->searchSteam((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1]))); break;
        case 16: _t->install((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<QString>>(_a[2])),(*reinterpret_cast<std::add_pointer_t<QString>>(_a[3]))); break;
        case 17: _t->cancel((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1]))); break;
        case 18: _t->uninstall((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1]))); break;
        case 19: { QVariantMap _r = _t->compatibility((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<QString>>(_a[2])));
            if (_a[0]) *reinterpret_cast<QVariantMap*>(_a[0]) = std::move(_r); }  break;
        default: ;
        }
    }
    if (_c == QMetaObject::IndexOfMethod) {
        if (QtMocHelpers::indexOfMethod<void (StoreManager::*)()>(_a, &StoreManager::storesChanged, 0))
            return;
        if (QtMocHelpers::indexOfMethod<void (StoreManager::*)()>(_a, &StoreManager::ownedChanged, 1))
            return;
        if (QtMocHelpers::indexOfMethod<void (StoreManager::*)()>(_a, &StoreManager::downloadsChanged, 2))
            return;
        if (QtMocHelpers::indexOfMethod<void (StoreManager::*)(const QString & )>(_a, &StoreManager::gameInstalled, 3))
            return;
        if (QtMocHelpers::indexOfMethod<void (StoreManager::*)(const QString & )>(_a, &StoreManager::gameRemoved, 4))
            return;
        if (QtMocHelpers::indexOfMethod<void (StoreManager::*)(const QString & )>(_a, &StoreManager::compatibilityChanged, 5))
            return;
    }
    if (_c == QMetaObject::ReadProperty) {
        void *_v = _a[0];
        switch (_id) {
        case 0: *reinterpret_cast<QVariantMap*>(_v) = _t->stores(); break;
        case 1: *reinterpret_cast<QVariantMap*>(_v) = _t->owned(); break;
        case 2: *reinterpret_cast<QVariantList*>(_v) = _t->downloads(); break;
        case 3: *reinterpret_cast<int*>(_v) = _t->activeDownloads(); break;
        case 4: *reinterpret_cast<double*>(_v) = _t->downloadProgress(); break;
        case 5: *reinterpret_cast<QString*>(_v) = _t->downloadTitle(); break;
        default: break;
        }
    }
}

const QMetaObject *StoreManager::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *StoreManager::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_staticMetaObjectStaticContent<qt_meta_tag_ZN12StoreManagerE_t>.strings))
        return static_cast<void*>(this);
    return QObject::qt_metacast(_clname);
}

int StoreManager::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QObject::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 20)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 20;
    }
    if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 20)
            *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType();
        _id -= 20;
    }
    if (_c == QMetaObject::ReadProperty || _c == QMetaObject::WriteProperty
            || _c == QMetaObject::ResetProperty || _c == QMetaObject::BindableProperty
            || _c == QMetaObject::RegisterPropertyMetaType) {
        qt_static_metacall(this, _c, _id, _a);
        _id -= 6;
    }
    return _id;
}

// SIGNAL 0
void StoreManager::storesChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 0, nullptr);
}

// SIGNAL 1
void StoreManager::ownedChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 1, nullptr);
}

// SIGNAL 2
void StoreManager::downloadsChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 2, nullptr);
}

// SIGNAL 3
void StoreManager::gameInstalled(const QString & _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 3, nullptr, _t1);
}

// SIGNAL 4
void StoreManager::gameRemoved(const QString & _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 4, nullptr, _t1);
}

// SIGNAL 5
void StoreManager::compatibilityChanged(const QString & _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 5, nullptr, _t1);
}
QT_WARNING_POP
