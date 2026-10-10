/****************************************************************************
** Meta object code from reading C++ file 'gamepadmanager.h'
**
** Created by: The Qt Meta Object Compiler version 69 (Qt 6.12.0)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "../../../src/gamepadmanager.h"
#include <QtCore/qmetatype.h>

#include <QtCore/qtmochelpers.h>

#include <memory>


#include <QtCore/qxptype_traits.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'gamepadmanager.h' doesn't include <QObject>."
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
struct qt_meta_tag_ZN14GamepadManagerE_t {};
} // unnamed namespace

template <> constexpr inline auto GamepadManager::qt_create_metaobjectdata<qt_meta_tag_ZN14GamepadManagerE_t>()
{
    namespace QMC = QtMocConstants;
    QtMocHelpers::StringRefStorage qt_stringData {
        "GamepadManager",
        "QML.Element",
        "auto",
        "QML.Singleton",
        "true",
        "activeChanged",
        "",
        "gamepadsChanged",
        "gamepadConnected",
        "name",
        "connection",
        "gamepadDisconnected",
        "batteryLow",
        "percent",
        "pointerChanged",
        "keyEventsEnabledChanged",
        "buttonPressed",
        "button",
        "hasBluetoothDualSense",
        "mac",
        "rename",
        "id",
        "setColor",
        "QColor",
        "color",
        "connected",
        "dualSense",
        "controllerName",
        "macAddress",
        "controllerCount",
        "gamepads",
        "QVariantList",
        "pointerEnabled",
        "pointerVisible",
        "keyEventsEnabled",
        "pointerPos",
        "QPointF",
        "pointerWindow"
    };

    QtMocHelpers::UintData qt_methods {
        // Signal 'activeChanged'
        QtMocHelpers::SignalData<void()>(5, 6, QMC::AccessPublic, QMetaType::Void),
        // Signal 'gamepadsChanged'
        QtMocHelpers::SignalData<void()>(7, 6, QMC::AccessPublic, QMetaType::Void),
        // Signal 'gamepadConnected'
        QtMocHelpers::SignalData<void(const QString &, const QString &)>(8, 6, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 9 }, { QMetaType::QString, 10 },
        }}),
        // Signal 'gamepadDisconnected'
        QtMocHelpers::SignalData<void(const QString &, const QString &)>(11, 6, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 9 }, { QMetaType::QString, 10 },
        }}),
        // Signal 'batteryLow'
        QtMocHelpers::SignalData<void(const QString &, int)>(12, 6, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 9 }, { QMetaType::Int, 13 },
        }}),
        // Signal 'pointerChanged'
        QtMocHelpers::SignalData<void()>(14, 6, QMC::AccessPublic, QMetaType::Void),
        // Signal 'keyEventsEnabledChanged'
        QtMocHelpers::SignalData<void()>(15, 6, QMC::AccessPublic, QMetaType::Void),
        // Signal 'buttonPressed'
        QtMocHelpers::SignalData<void(const QString &)>(16, 6, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 17 },
        }}),
        // Method 'hasBluetoothDualSense'
        QtMocHelpers::MethodData<bool(const QString &) const>(18, 6, QMC::AccessPublic, QMetaType::Bool, {{
            { QMetaType::QString, 19 },
        }}),
        // Method 'hasBluetoothDualSense'
        QtMocHelpers::MethodData<bool() const>(18, 6, QMC::AccessPublic | QMC::MethodCloned, QMetaType::Bool),
        // Method 'rename'
        QtMocHelpers::MethodData<void(const QString &, const QString &)>(20, 6, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 21 }, { QMetaType::QString, 9 },
        }}),
        // Method 'setColor'
        QtMocHelpers::MethodData<bool(const QString &, const QColor &)>(22, 6, QMC::AccessPublic, QMetaType::Bool, {{
            { QMetaType::QString, 21 }, { 0x80000000 | 23, 24 },
        }}),
    };
    QtMocHelpers::UintData qt_properties {
        // property 'connected'
        QtMocHelpers::PropertyData<bool>(25, QMetaType::Bool, QMC::DefaultPropertyFlags, 0),
        // property 'dualSense'
        QtMocHelpers::PropertyData<bool>(26, QMetaType::Bool, QMC::DefaultPropertyFlags, 0),
        // property 'controllerName'
        QtMocHelpers::PropertyData<QString>(27, QMetaType::QString, QMC::DefaultPropertyFlags, 0),
        // property 'connection'
        QtMocHelpers::PropertyData<QString>(10, QMetaType::QString, QMC::DefaultPropertyFlags, 0),
        // property 'macAddress'
        QtMocHelpers::PropertyData<QString>(28, QMetaType::QString, QMC::DefaultPropertyFlags, 0),
        // property 'controllerCount'
        QtMocHelpers::PropertyData<int>(29, QMetaType::Int, QMC::DefaultPropertyFlags, 0),
        // property 'gamepads'
        QtMocHelpers::PropertyData<QVariantList>(30, 0x80000000 | 31, QMC::DefaultPropertyFlags | QMC::EnumOrFlag, 1),
        // property 'pointerEnabled'
        QtMocHelpers::PropertyData<bool>(32, QMetaType::Bool, QMC::DefaultPropertyFlags | QMC::Writable | QMC::StdCppSet, 5),
        // property 'pointerVisible'
        QtMocHelpers::PropertyData<bool>(33, QMetaType::Bool, QMC::DefaultPropertyFlags, 5),
        // property 'keyEventsEnabled'
        QtMocHelpers::PropertyData<bool>(34, QMetaType::Bool, QMC::DefaultPropertyFlags | QMC::Writable | QMC::StdCppSet, 6),
        // property 'pointerPos'
        QtMocHelpers::PropertyData<QPointF>(35, 0x80000000 | 36, QMC::DefaultPropertyFlags | QMC::EnumOrFlag, 5),
        // property 'pointerWindow'
        QtMocHelpers::PropertyData<QObject*>(37, QMetaType::QObjectStar, QMC::DefaultPropertyFlags, 5),
    };
    QtMocHelpers::UintData qt_enums {
    };
    QtMocHelpers::UintData qt_constructors {};
    QtMocHelpers::ClassInfos qt_classinfo({
            {    1,    2 },
            {    3,    4 },
    });
    return QtMocHelpers::metaObjectData<GamepadManager, void>(QMC::MetaObjectFlag{}, qt_stringData,
            qt_methods, qt_properties, qt_enums, qt_constructors, qt_classinfo);
}
Q_CONSTINIT const QMetaObject GamepadManager::staticMetaObject = { {
    QMetaObject::SuperData::link<QObject::staticMetaObject>(),
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN14GamepadManagerE_t>.stringdata,
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN14GamepadManagerE_t>.data,
    qt_static_metacall,
    nullptr,
    qt_staticMetaObjectRelocatingContent<qt_meta_tag_ZN14GamepadManagerE_t>.metaTypes,
    nullptr
} };

void GamepadManager::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    auto *_t = static_cast<GamepadManager *>(_o);
    if (_c == QMetaObject::InvokeMetaMethod) {
        switch (_id) {
        case 0: _t->activeChanged(); break;
        case 1: _t->gamepadsChanged(); break;
        case 2: _t->gamepadConnected((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<QString>>(_a[2]))); break;
        case 3: _t->gamepadDisconnected((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<QString>>(_a[2]))); break;
        case 4: _t->batteryLow((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<int>>(_a[2]))); break;
        case 5: _t->pointerChanged(); break;
        case 6: _t->keyEventsEnabledChanged(); break;
        case 7: _t->buttonPressed((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1]))); break;
        case 8: { bool _r = _t->hasBluetoothDualSense((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1])));
            if (_a[0]) *reinterpret_cast<bool*>(_a[0]) = std::move(_r); }  break;
        case 9: { bool _r = _t->hasBluetoothDualSense();
            if (_a[0]) *reinterpret_cast<bool*>(_a[0]) = std::move(_r); }  break;
        case 10: _t->rename((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<QString>>(_a[2]))); break;
        case 11: { bool _r = _t->setColor((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<QColor>>(_a[2])));
            if (_a[0]) *reinterpret_cast<bool*>(_a[0]) = std::move(_r); }  break;
        default: ;
        }
    }
    if (_c == QMetaObject::IndexOfMethod) {
        if (QtMocHelpers::indexOfMethod<void (GamepadManager::*)()>(_a, &GamepadManager::activeChanged, 0))
            return;
        if (QtMocHelpers::indexOfMethod<void (GamepadManager::*)()>(_a, &GamepadManager::gamepadsChanged, 1))
            return;
        if (QtMocHelpers::indexOfMethod<void (GamepadManager::*)(const QString & , const QString & )>(_a, &GamepadManager::gamepadConnected, 2))
            return;
        if (QtMocHelpers::indexOfMethod<void (GamepadManager::*)(const QString & , const QString & )>(_a, &GamepadManager::gamepadDisconnected, 3))
            return;
        if (QtMocHelpers::indexOfMethod<void (GamepadManager::*)(const QString & , int )>(_a, &GamepadManager::batteryLow, 4))
            return;
        if (QtMocHelpers::indexOfMethod<void (GamepadManager::*)()>(_a, &GamepadManager::pointerChanged, 5))
            return;
        if (QtMocHelpers::indexOfMethod<void (GamepadManager::*)()>(_a, &GamepadManager::keyEventsEnabledChanged, 6))
            return;
        if (QtMocHelpers::indexOfMethod<void (GamepadManager::*)(const QString & )>(_a, &GamepadManager::buttonPressed, 7))
            return;
    }
    if (_c == QMetaObject::ReadProperty) {
        void *_v = _a[0];
        switch (_id) {
        case 0: *reinterpret_cast<bool*>(_v) = _t->connected(); break;
        case 1: *reinterpret_cast<bool*>(_v) = _t->dualSense(); break;
        case 2: *reinterpret_cast<QString*>(_v) = _t->controllerName(); break;
        case 3: *reinterpret_cast<QString*>(_v) = _t->connection(); break;
        case 4: *reinterpret_cast<QString*>(_v) = _t->macAddress(); break;
        case 5: *reinterpret_cast<int*>(_v) = _t->controllerCount(); break;
        case 6: *reinterpret_cast<QVariantList*>(_v) = _t->gamepads(); break;
        case 7: *reinterpret_cast<bool*>(_v) = _t->pointerEnabled(); break;
        case 8: *reinterpret_cast<bool*>(_v) = _t->pointerVisible(); break;
        case 9: *reinterpret_cast<bool*>(_v) = _t->keyEventsEnabled(); break;
        case 10: *reinterpret_cast<QPointF*>(_v) = _t->pointerPos(); break;
        case 11: *reinterpret_cast<QObject**>(_v) = _t->pointerWindow(); break;
        default: break;
        }
    }
    if (_c == QMetaObject::WriteProperty) {
        void *_v = _a[0];
        switch (_id) {
        case 7: _t->setPointerEnabled(*reinterpret_cast<bool*>(_v)); break;
        case 9: _t->setKeyEventsEnabled(*reinterpret_cast<bool*>(_v)); break;
        default: break;
        }
    }
}

const QMetaObject *GamepadManager::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *GamepadManager::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_staticMetaObjectStaticContent<qt_meta_tag_ZN14GamepadManagerE_t>.strings))
        return static_cast<void*>(this);
    return QObject::qt_metacast(_clname);
}

int GamepadManager::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QObject::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 12)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 12;
    }
    if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 12)
            *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType();
        _id -= 12;
    }
    if (_c == QMetaObject::ReadProperty || _c == QMetaObject::WriteProperty
            || _c == QMetaObject::ResetProperty || _c == QMetaObject::BindableProperty
            || _c == QMetaObject::RegisterPropertyMetaType) {
        qt_static_metacall(this, _c, _id, _a);
        _id -= 12;
    }
    return _id;
}

// SIGNAL 0
void GamepadManager::activeChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 0, nullptr);
}

// SIGNAL 1
void GamepadManager::gamepadsChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 1, nullptr);
}

// SIGNAL 2
void GamepadManager::gamepadConnected(const QString & _t1, const QString & _t2)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 2, nullptr, _t1, _t2);
}

// SIGNAL 3
void GamepadManager::gamepadDisconnected(const QString & _t1, const QString & _t2)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 3, nullptr, _t1, _t2);
}

// SIGNAL 4
void GamepadManager::batteryLow(const QString & _t1, int _t2)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 4, nullptr, _t1, _t2);
}

// SIGNAL 5
void GamepadManager::pointerChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 5, nullptr);
}

// SIGNAL 6
void GamepadManager::keyEventsEnabledChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 6, nullptr);
}

// SIGNAL 7
void GamepadManager::buttonPressed(const QString & _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 7, nullptr, _t1);
}
QT_WARNING_POP
