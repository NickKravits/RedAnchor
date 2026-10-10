/****************************************************************************
** Meta object code from reading C++ file 'bluetoothmanager.h'
**
** Created by: The Qt Meta Object Compiler version 69 (Qt 6.12.0)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "../../../src/bluetoothmanager.h"
#include <QtCore/qmetatype.h>

#include <QtCore/qtmochelpers.h>

#include <memory>


#include <QtCore/qxptype_traits.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'bluetoothmanager.h' doesn't include <QObject>."
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
struct qt_meta_tag_ZN16BluetoothManagerE_t {};
} // unnamed namespace

template <> constexpr inline auto BluetoothManager::qt_create_metaobjectdata<qt_meta_tag_ZN16BluetoothManagerE_t>()
{
    namespace QMC = QtMocConstants;
    QtMocHelpers::StringRefStorage qt_stringData {
        "BluetoothManager",
        "QML.Element",
        "auto",
        "QML.Singleton",
        "true",
        "adapterChanged",
        "",
        "pairingStateChanged",
        "adapterStateChanged",
        "devicesChanged",
        "deviceError",
        "path",
        "message",
        "onInterfacesAdded",
        "QDBusObjectPath",
        "InterfaceMap",
        "interfaces",
        "onInterfacesRemoved",
        "onPropertiesChanged",
        "QDBusMessage",
        "startDiscovery",
        "stopDiscovery",
        "pairDevice",
        "connectDevice",
        "disconnectDevice",
        "removeDevice",
        "disconnectAddress",
        "mac",
        "isPairedDevice",
        "pairController",
        "adapterAvailable",
        "pairingState",
        "controllerPaired",
        "powered",
        "discovering",
        "devices",
        "QVariantList",
        "audioConnected"
    };

    QtMocHelpers::UintData qt_methods {
        // Signal 'adapterChanged'
        QtMocHelpers::SignalData<void()>(5, 6, QMC::AccessPublic, QMetaType::Void),
        // Signal 'pairingStateChanged'
        QtMocHelpers::SignalData<void()>(7, 6, QMC::AccessPublic, QMetaType::Void),
        // Signal 'adapterStateChanged'
        QtMocHelpers::SignalData<void()>(8, 6, QMC::AccessPublic, QMetaType::Void),
        // Signal 'devicesChanged'
        QtMocHelpers::SignalData<void()>(9, 6, QMC::AccessPublic, QMetaType::Void),
        // Signal 'deviceError'
        QtMocHelpers::SignalData<void(const QString &, const QString &)>(10, 6, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 11 }, { QMetaType::QString, 12 },
        }}),
        // Slot 'onInterfacesAdded'
        QtMocHelpers::SlotData<void(const QDBusObjectPath &, const InterfaceMap &)>(13, 6, QMC::AccessPrivate, QMetaType::Void, {{
            { 0x80000000 | 14, 11 }, { 0x80000000 | 15, 16 },
        }}),
        // Slot 'onInterfacesRemoved'
        QtMocHelpers::SlotData<void(const QDBusObjectPath &, const QStringList &)>(17, 6, QMC::AccessPrivate, QMetaType::Void, {{
            { 0x80000000 | 14, 11 }, { QMetaType::QStringList, 16 },
        }}),
        // Slot 'onPropertiesChanged'
        QtMocHelpers::SlotData<void(const QDBusMessage &)>(18, 6, QMC::AccessPrivate, QMetaType::Void, {{
            { 0x80000000 | 19, 12 },
        }}),
        // Method 'startDiscovery'
        QtMocHelpers::MethodData<void()>(20, 6, QMC::AccessPublic, QMetaType::Void),
        // Method 'stopDiscovery'
        QtMocHelpers::MethodData<void()>(21, 6, QMC::AccessPublic, QMetaType::Void),
        // Method 'pairDevice'
        QtMocHelpers::MethodData<void(const QString &)>(22, 6, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 11 },
        }}),
        // Method 'connectDevice'
        QtMocHelpers::MethodData<void(const QString &)>(23, 6, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 11 },
        }}),
        // Method 'disconnectDevice'
        QtMocHelpers::MethodData<void(const QString &)>(24, 6, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 11 },
        }}),
        // Method 'removeDevice'
        QtMocHelpers::MethodData<void(const QString &)>(25, 6, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 11 },
        }}),
        // Method 'disconnectAddress'
        QtMocHelpers::MethodData<void(const QString &)>(26, 6, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 27 },
        }}),
        // Method 'isPairedDevice'
        QtMocHelpers::MethodData<bool(const QString &) const>(28, 6, QMC::AccessPublic, QMetaType::Bool, {{
            { QMetaType::QString, 27 },
        }}),
        // Method 'pairController'
        QtMocHelpers::MethodData<void(const QString &)>(29, 6, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 27 },
        }}),
    };
    QtMocHelpers::UintData qt_properties {
        // property 'adapterAvailable'
        QtMocHelpers::PropertyData<bool>(30, QMetaType::Bool, QMC::DefaultPropertyFlags, 0),
        // property 'pairingState'
        QtMocHelpers::PropertyData<QString>(31, QMetaType::QString, QMC::DefaultPropertyFlags, 1),
        // property 'controllerPaired'
        QtMocHelpers::PropertyData<bool>(32, QMetaType::Bool, QMC::DefaultPropertyFlags, 1),
        // property 'powered'
        QtMocHelpers::PropertyData<bool>(33, QMetaType::Bool, QMC::DefaultPropertyFlags, 2),
        // property 'discovering'
        QtMocHelpers::PropertyData<bool>(34, QMetaType::Bool, QMC::DefaultPropertyFlags, 2),
        // property 'devices'
        QtMocHelpers::PropertyData<QVariantList>(35, 0x80000000 | 36, QMC::DefaultPropertyFlags | QMC::EnumOrFlag, 3),
        // property 'audioConnected'
        QtMocHelpers::PropertyData<bool>(37, QMetaType::Bool, QMC::DefaultPropertyFlags, 3),
    };
    QtMocHelpers::UintData qt_enums {
    };
    QtMocHelpers::UintData qt_constructors {};
    QtMocHelpers::ClassInfos qt_classinfo({
            {    1,    2 },
            {    3,    4 },
    });
    return QtMocHelpers::metaObjectData<BluetoothManager, void>(QMC::MetaObjectFlag{}, qt_stringData,
            qt_methods, qt_properties, qt_enums, qt_constructors, qt_classinfo);
}
Q_CONSTINIT const QMetaObject BluetoothManager::staticMetaObject = { {
    QMetaObject::SuperData::link<QObject::staticMetaObject>(),
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN16BluetoothManagerE_t>.stringdata,
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN16BluetoothManagerE_t>.data,
    qt_static_metacall,
    nullptr,
    qt_staticMetaObjectRelocatingContent<qt_meta_tag_ZN16BluetoothManagerE_t>.metaTypes,
    nullptr
} };

void BluetoothManager::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    auto *_t = static_cast<BluetoothManager *>(_o);
    if (_c == QMetaObject::InvokeMetaMethod) {
        switch (_id) {
        case 0: _t->adapterChanged(); break;
        case 1: _t->pairingStateChanged(); break;
        case 2: _t->adapterStateChanged(); break;
        case 3: _t->devicesChanged(); break;
        case 4: _t->deviceError((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<QString>>(_a[2]))); break;
        case 5: _t->onInterfacesAdded((*reinterpret_cast<std::add_pointer_t<QDBusObjectPath>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<InterfaceMap>>(_a[2]))); break;
        case 6: _t->onInterfacesRemoved((*reinterpret_cast<std::add_pointer_t<QDBusObjectPath>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<QStringList>>(_a[2]))); break;
        case 7: _t->onPropertiesChanged((*reinterpret_cast<std::add_pointer_t<QDBusMessage>>(_a[1]))); break;
        case 8: _t->startDiscovery(); break;
        case 9: _t->stopDiscovery(); break;
        case 10: _t->pairDevice((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1]))); break;
        case 11: _t->connectDevice((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1]))); break;
        case 12: _t->disconnectDevice((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1]))); break;
        case 13: _t->removeDevice((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1]))); break;
        case 14: _t->disconnectAddress((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1]))); break;
        case 15: { bool _r = _t->isPairedDevice((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1])));
            if (_a[0]) *reinterpret_cast<bool*>(_a[0]) = std::move(_r); }  break;
        case 16: _t->pairController((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1]))); break;
        default: ;
        }
    }
    if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        switch (_id) {
        default: *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType(); break;
        case 5:
            switch (*reinterpret_cast<int*>(_a[1])) {
            default: *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType(); break;
            case 1:
                *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType::fromType< InterfaceMap >(); break;
            case 0:
                *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType::fromType< QDBusObjectPath >(); break;
            }
            break;
        case 6:
            switch (*reinterpret_cast<int*>(_a[1])) {
            default: *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType(); break;
            case 0:
                *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType::fromType< QDBusObjectPath >(); break;
            }
            break;
        case 7:
            switch (*reinterpret_cast<int*>(_a[1])) {
            default: *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType(); break;
            case 0:
                *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType::fromType< QDBusMessage >(); break;
            }
            break;
        }
    }
    if (_c == QMetaObject::IndexOfMethod) {
        if (QtMocHelpers::indexOfMethod<void (BluetoothManager::*)()>(_a, &BluetoothManager::adapterChanged, 0))
            return;
        if (QtMocHelpers::indexOfMethod<void (BluetoothManager::*)()>(_a, &BluetoothManager::pairingStateChanged, 1))
            return;
        if (QtMocHelpers::indexOfMethod<void (BluetoothManager::*)()>(_a, &BluetoothManager::adapterStateChanged, 2))
            return;
        if (QtMocHelpers::indexOfMethod<void (BluetoothManager::*)()>(_a, &BluetoothManager::devicesChanged, 3))
            return;
        if (QtMocHelpers::indexOfMethod<void (BluetoothManager::*)(const QString & , const QString & )>(_a, &BluetoothManager::deviceError, 4))
            return;
    }
    if (_c == QMetaObject::ReadProperty) {
        void *_v = _a[0];
        switch (_id) {
        case 0: *reinterpret_cast<bool*>(_v) = _t->adapterAvailable(); break;
        case 1: *reinterpret_cast<QString*>(_v) = _t->pairingState(); break;
        case 2: *reinterpret_cast<bool*>(_v) = _t->controllerPaired(); break;
        case 3: *reinterpret_cast<bool*>(_v) = _t->powered(); break;
        case 4: *reinterpret_cast<bool*>(_v) = _t->discovering(); break;
        case 5: *reinterpret_cast<QVariantList*>(_v) = _t->devices(); break;
        case 6: *reinterpret_cast<bool*>(_v) = _t->audioConnected(); break;
        default: break;
        }
    }
}

const QMetaObject *BluetoothManager::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *BluetoothManager::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_staticMetaObjectStaticContent<qt_meta_tag_ZN16BluetoothManagerE_t>.strings))
        return static_cast<void*>(this);
    return QObject::qt_metacast(_clname);
}

int BluetoothManager::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QObject::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 17)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 17;
    }
    if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 17)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 17;
    }
    if (_c == QMetaObject::ReadProperty || _c == QMetaObject::WriteProperty
            || _c == QMetaObject::ResetProperty || _c == QMetaObject::BindableProperty
            || _c == QMetaObject::RegisterPropertyMetaType) {
        qt_static_metacall(this, _c, _id, _a);
        _id -= 7;
    }
    return _id;
}

// SIGNAL 0
void BluetoothManager::adapterChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 0, nullptr);
}

// SIGNAL 1
void BluetoothManager::pairingStateChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 1, nullptr);
}

// SIGNAL 2
void BluetoothManager::adapterStateChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 2, nullptr);
}

// SIGNAL 3
void BluetoothManager::devicesChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 3, nullptr);
}

// SIGNAL 4
void BluetoothManager::deviceError(const QString & _t1, const QString & _t2)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 4, nullptr, _t1, _t2);
}
QT_WARNING_POP
