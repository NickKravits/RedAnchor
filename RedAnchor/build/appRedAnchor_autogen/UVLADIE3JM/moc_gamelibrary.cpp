/****************************************************************************
** Meta object code from reading C++ file 'gamelibrary.h'
**
** Created by: The Qt Meta Object Compiler version 69 (Qt 6.12.0)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "../../../src/gamelibrary.h"
#include <QtCore/qmetatype.h>

#include <QtCore/qtmochelpers.h>

#include <memory>


#include <QtCore/qxptype_traits.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'gamelibrary.h' doesn't include <QObject>."
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
struct qt_meta_tag_ZN11GameLibraryE_t {};
} // unnamed namespace

template <> constexpr inline auto GameLibrary::qt_create_metaobjectdata<qt_meta_tag_ZN11GameLibraryE_t>()
{
    namespace QMC = QtMocConstants;
    QtMocHelpers::StringRefStorage qt_stringData {
        "GameLibrary",
        "QML.Element",
        "auto",
        "QML.Singleton",
        "true",
        "gamesChanged",
        "",
        "storageChanged",
        "contains",
        "id",
        "isInstalled",
        "game",
        "QVariantMap",
        "isPinned",
        "pin",
        "unpin",
        "refreshStorage",
        "games",
        "QVariantList",
        "pinned",
        "count",
        "installedCount",
        "diskTotal",
        "diskFree",
        "gamesBytes",
        "measuring"
    };

    QtMocHelpers::UintData qt_methods {
        // Signal 'gamesChanged'
        QtMocHelpers::SignalData<void()>(5, 6, QMC::AccessPublic, QMetaType::Void),
        // Signal 'storageChanged'
        QtMocHelpers::SignalData<void()>(7, 6, QMC::AccessPublic, QMetaType::Void),
        // Method 'contains'
        QtMocHelpers::MethodData<bool(const QString &) const>(8, 6, QMC::AccessPublic, QMetaType::Bool, {{
            { QMetaType::QString, 9 },
        }}),
        // Method 'isInstalled'
        QtMocHelpers::MethodData<bool(const QString &) const>(10, 6, QMC::AccessPublic, QMetaType::Bool, {{
            { QMetaType::QString, 9 },
        }}),
        // Method 'game'
        QtMocHelpers::MethodData<QVariantMap(const QString &) const>(11, 6, QMC::AccessPublic, 0x80000000 | 12, {{
            { QMetaType::QString, 9 },
        }}),
        // Method 'isPinned'
        QtMocHelpers::MethodData<bool(const QString &) const>(13, 6, QMC::AccessPublic, QMetaType::Bool, {{
            { QMetaType::QString, 9 },
        }}),
        // Method 'pin'
        QtMocHelpers::MethodData<void(const QString &)>(14, 6, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 9 },
        }}),
        // Method 'unpin'
        QtMocHelpers::MethodData<void(const QString &)>(15, 6, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 9 },
        }}),
        // Method 'refreshStorage'
        QtMocHelpers::MethodData<void()>(16, 6, QMC::AccessPublic, QMetaType::Void),
    };
    QtMocHelpers::UintData qt_properties {
        // property 'games'
        QtMocHelpers::PropertyData<QVariantList>(17, 0x80000000 | 18, QMC::DefaultPropertyFlags | QMC::EnumOrFlag, 0),
        // property 'pinned'
        QtMocHelpers::PropertyData<QVariantList>(19, 0x80000000 | 18, QMC::DefaultPropertyFlags | QMC::EnumOrFlag, 0),
        // property 'count'
        QtMocHelpers::PropertyData<int>(20, QMetaType::Int, QMC::DefaultPropertyFlags, 0),
        // property 'installedCount'
        QtMocHelpers::PropertyData<int>(21, QMetaType::Int, QMC::DefaultPropertyFlags, 0),
        // property 'diskTotal'
        QtMocHelpers::PropertyData<qint64>(22, QMetaType::LongLong, QMC::DefaultPropertyFlags, 1),
        // property 'diskFree'
        QtMocHelpers::PropertyData<qint64>(23, QMetaType::LongLong, QMC::DefaultPropertyFlags, 1),
        // property 'gamesBytes'
        QtMocHelpers::PropertyData<qint64>(24, QMetaType::LongLong, QMC::DefaultPropertyFlags, 1),
        // property 'measuring'
        QtMocHelpers::PropertyData<bool>(25, QMetaType::Bool, QMC::DefaultPropertyFlags, 1),
    };
    QtMocHelpers::UintData qt_enums {
    };
    QtMocHelpers::UintData qt_constructors {};
    QtMocHelpers::ClassInfos qt_classinfo({
            {    1,    2 },
            {    3,    4 },
    });
    return QtMocHelpers::metaObjectData<GameLibrary, void>(QMC::MetaObjectFlag{}, qt_stringData,
            qt_methods, qt_properties, qt_enums, qt_constructors, qt_classinfo);
}
Q_CONSTINIT const QMetaObject GameLibrary::staticMetaObject = { {
    QMetaObject::SuperData::link<QObject::staticMetaObject>(),
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN11GameLibraryE_t>.stringdata,
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN11GameLibraryE_t>.data,
    qt_static_metacall,
    nullptr,
    qt_staticMetaObjectRelocatingContent<qt_meta_tag_ZN11GameLibraryE_t>.metaTypes,
    nullptr
} };

void GameLibrary::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    auto *_t = static_cast<GameLibrary *>(_o);
    if (_c == QMetaObject::InvokeMetaMethod) {
        switch (_id) {
        case 0: _t->gamesChanged(); break;
        case 1: _t->storageChanged(); break;
        case 2: { bool _r = _t->contains((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1])));
            if (_a[0]) *reinterpret_cast<bool*>(_a[0]) = std::move(_r); }  break;
        case 3: { bool _r = _t->isInstalled((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1])));
            if (_a[0]) *reinterpret_cast<bool*>(_a[0]) = std::move(_r); }  break;
        case 4: { QVariantMap _r = _t->game((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1])));
            if (_a[0]) *reinterpret_cast<QVariantMap*>(_a[0]) = std::move(_r); }  break;
        case 5: { bool _r = _t->isPinned((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1])));
            if (_a[0]) *reinterpret_cast<bool*>(_a[0]) = std::move(_r); }  break;
        case 6: _t->pin((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1]))); break;
        case 7: _t->unpin((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1]))); break;
        case 8: _t->refreshStorage(); break;
        default: ;
        }
    }
    if (_c == QMetaObject::IndexOfMethod) {
        if (QtMocHelpers::indexOfMethod<void (GameLibrary::*)()>(_a, &GameLibrary::gamesChanged, 0))
            return;
        if (QtMocHelpers::indexOfMethod<void (GameLibrary::*)()>(_a, &GameLibrary::storageChanged, 1))
            return;
    }
    if (_c == QMetaObject::ReadProperty) {
        void *_v = _a[0];
        switch (_id) {
        case 0: *reinterpret_cast<QVariantList*>(_v) = _t->games(); break;
        case 1: *reinterpret_cast<QVariantList*>(_v) = _t->pinned(); break;
        case 2: *reinterpret_cast<int*>(_v) = _t->count(); break;
        case 3: *reinterpret_cast<int*>(_v) = _t->installedCount(); break;
        case 4: *reinterpret_cast<qint64*>(_v) = _t->diskTotal(); break;
        case 5: *reinterpret_cast<qint64*>(_v) = _t->diskFree(); break;
        case 6: *reinterpret_cast<qint64*>(_v) = _t->gamesBytes(); break;
        case 7: *reinterpret_cast<bool*>(_v) = _t->measuring(); break;
        default: break;
        }
    }
}

const QMetaObject *GameLibrary::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *GameLibrary::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_staticMetaObjectStaticContent<qt_meta_tag_ZN11GameLibraryE_t>.strings))
        return static_cast<void*>(this);
    return QObject::qt_metacast(_clname);
}

int GameLibrary::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QObject::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 9)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 9;
    }
    if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 9)
            *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType();
        _id -= 9;
    }
    if (_c == QMetaObject::ReadProperty || _c == QMetaObject::WriteProperty
            || _c == QMetaObject::ResetProperty || _c == QMetaObject::BindableProperty
            || _c == QMetaObject::RegisterPropertyMetaType) {
        qt_static_metacall(this, _c, _id, _a);
        _id -= 8;
    }
    return _id;
}

// SIGNAL 0
void GameLibrary::gamesChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 0, nullptr);
}

// SIGNAL 1
void GameLibrary::storageChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 1, nullptr);
}
QT_WARNING_POP
