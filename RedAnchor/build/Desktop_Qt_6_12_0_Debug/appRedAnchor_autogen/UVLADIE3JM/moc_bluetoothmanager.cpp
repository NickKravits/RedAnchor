/****************************************************************************
** Meta object code from reading C++ file 'bluetoothmanager.h'
**
** Created by: The Qt Meta Object Compiler version 69 (Qt 6.12.0)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "../../../../src/bluetoothmanager.h"
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
        "stateChanged",
        "",
        "onInterfacesAdded",
        "QDBusObjectPath",
        "path",
        "InterfaceMap",
        "interfaces",
        "onPropertiesChanged",
        "QDBusMessage",
        "message",
        "startControllerPairing",
        "macHint",
        "cancel",
        "markConnected",
        "state",
        "State",
        "deviceName",
        "deviceAddress",
        "errorString",
        "demo",
        "Idle",
        "Preparing",
        "Searching",
        "Pairing",
        "Connecting",
        "Connected",
        "Failed"
    };

    QtMocHelpers::UintData qt_methods {
        // Signal 'stateChanged'
        QtMocHelpers::SignalData<void()>(5, 6, QMC::AccessPublic, QMetaType::Void),
        // Slot 'onInterfacesAdded'
        QtMocHelpers::SlotData<void(const QDBusObjectPath &, const InterfaceMap &)>(7, 6, QMC::AccessPrivate, QMetaType::Void, {{
            { 0x80000000 | 8, 9 }, { 0x80000000 | 10, 11 },
        }}),
        // Slot 'onPropertiesChanged'
        QtMocHelpers::SlotData<void(const QDBusMessage &)>(12, 6, QMC::AccessPrivate, QMetaType::Void, {{
            { 0x80000000 | 13, 14 },
        }}),
        // Method 'startControllerPairing'
        QtMocHelpers::MethodData<void(const QString &)>(15, 6, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 16 },
        }}),
        // Method 'startControllerPairing'
        QtMocHelpers::MethodData<void()>(15, 6, QMC::AccessPublic | QMC::MethodCloned, QMetaType::Void),
        // Method 'cancel'
        QtMocHelpers::MethodData<void()>(17, 6, QMC::AccessPublic, QMetaType::Void),
        // Method 'markConnected'
        QtMocHelpers::MethodData<void()>(18, 6, QMC::AccessPublic, QMetaType::Void),
    };
    QtMocHelpers::UintData qt_properties {
        // property 'state'
        QtMocHelpers::PropertyData<enum State>(19, 0x80000000 | 20, QMC::DefaultPropertyFlags | QMC::EnumOrFlag, 0),
        // property 'deviceName'
        QtMocHelpers::PropertyData<QString>(21, QMetaType::QString, QMC::DefaultPropertyFlags, 0),
        // property 'deviceAddress'
        QtMocHelpers::PropertyData<QString>(22, QMetaType::QString, QMC::DefaultPropertyFlags, 0),
        // property 'errorString'
        QtMocHelpers::PropertyData<QString>(23, QMetaType::QString, QMC::DefaultPropertyFlags, 0),
        // property 'demo'
        QtMocHelpers::PropertyData<bool>(24, QMetaType::Bool, QMC::DefaultPropertyFlags | QMC::Constant),
    };
    QtMocHelpers::UintData qt_enums {
        // enum 'State'
        QtMocHelpers::EnumData<enum State>(20, 20, QMC::EnumFlags{}).add({
            {   25, State::Idle },
            {   26, State::Preparing },
            {   27, State::Searching },
            {   28, State::Pairing },
            {   29, State::Connecting },
            {   30, State::Connected },
            {   31, State::Failed },
        }),
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
        case 0: _t->stateChanged(); break;
        case 1: _t->onInterfacesAdded((*reinterpret_cast<std::add_pointer_t<QDBusObjectPath>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<InterfaceMap>>(_a[2]))); break;
        case 2: _t->onPropertiesChanged((*reinterpret_cast<std::add_pointer_t<QDBusMessage>>(_a[1]))); break;
        case 3: _t->startControllerPairing((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1]))); break;
        case 4: _t->startControllerPairing(); break;
        case 5: _t->cancel(); break;
        case 6: _t->markConnected(); break;
        default: ;
        }
    }
    if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        switch (_id) {
        default: *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType(); break;
        case 1:
            switch (*reinterpret_cast<int*>(_a[1])) {
            default: *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType(); break;
            case 1:
                *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType::fromType< InterfaceMap >(); break;
            case 0:
                *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType::fromType< QDBusObjectPath >(); break;
            }
            break;
        case 2:
            switch (*reinterpret_cast<int*>(_a[1])) {
            default: *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType(); break;
            case 0:
                *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType::fromType< QDBusMessage >(); break;
            }
            break;
        }
    }
    if (_c == QMetaObject::IndexOfMethod) {
        if (QtMocHelpers::indexOfMethod<void (BluetoothManager::*)()>(_a, &BluetoothManager::stateChanged, 0))
            return;
    }
    if (_c == QMetaObject::ReadProperty) {
        void *_v = _a[0];
        switch (_id) {
        case 0: *reinterpret_cast<enum State*>(_v) = _t->state(); break;
        case 1: *reinterpret_cast<QString*>(_v) = _t->deviceName(); break;
        case 2: *reinterpret_cast<QString*>(_v) = _t->deviceAddress(); break;
        case 3: *reinterpret_cast<QString*>(_v) = _t->errorString(); break;
        case 4: *reinterpret_cast<bool*>(_v) = _t->demo(); break;
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
        if (_id < 7)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 7;
    }
    if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 7)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 7;
    }
    if (_c == QMetaObject::ReadProperty || _c == QMetaObject::WriteProperty
            || _c == QMetaObject::ResetProperty || _c == QMetaObject::BindableProperty
            || _c == QMetaObject::RegisterPropertyMetaType) {
        qt_static_metacall(this, _c, _id, _a);
        _id -= 5;
    }
    return _id;
}

// SIGNAL 0
void BluetoothManager::stateChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 0, nullptr);
}
QT_WARNING_POP
