/****************************************************************************
** Meta object code from reading C++ file 'networkbackend.h'
**
** Created by: The Qt Meta Object Compiler version 69 (Qt 6.12.0)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "../../../../src/networkbackend.h"
#include <QtCore/qmetatype.h>

#include <QtCore/qtmochelpers.h>

#include <memory>


#include <QtCore/qxptype_traits.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'networkbackend.h' doesn't include <QObject>."
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
struct qt_meta_tag_ZN14NetworkBackendE_t {};
} // unnamed namespace

template <> constexpr inline auto NetworkBackend::qt_create_metaobjectdata<qt_meta_tag_ZN14NetworkBackendE_t>()
{
    namespace QMC = QtMocConstants;
    QtMocHelpers::StringRefStorage qt_stringData {
        "NetworkBackend",
        "QML.Element",
        "auto",
        "QML.Singleton",
        "true",
        "statusChanged",
        "",
        "networksChanged",
        "scanningChanged",
        "connectStateChanged",
        "refresh",
        "scan",
        "connectToNetwork",
        "ssid",
        "password",
        "resetConnectState",
        "demo",
        "online",
        "wiredConnected",
        "wifiAvailable",
        "activeSsid",
        "networks",
        "QVariantList",
        "scanning",
        "connectState",
        "ConnectState",
        "errorString",
        "Idle",
        "Connecting",
        "Connected",
        "Failed"
    };

    QtMocHelpers::UintData qt_methods {
        // Signal 'statusChanged'
        QtMocHelpers::SignalData<void()>(5, 6, QMC::AccessPublic, QMetaType::Void),
        // Signal 'networksChanged'
        QtMocHelpers::SignalData<void()>(7, 6, QMC::AccessPublic, QMetaType::Void),
        // Signal 'scanningChanged'
        QtMocHelpers::SignalData<void()>(8, 6, QMC::AccessPublic, QMetaType::Void),
        // Signal 'connectStateChanged'
        QtMocHelpers::SignalData<void()>(9, 6, QMC::AccessPublic, QMetaType::Void),
        // Slot 'refresh'
        QtMocHelpers::SlotData<void()>(10, 6, QMC::AccessPublic, QMetaType::Void),
        // Method 'scan'
        QtMocHelpers::MethodData<void()>(11, 6, QMC::AccessPublic, QMetaType::Void),
        // Method 'connectToNetwork'
        QtMocHelpers::MethodData<void(const QString &, const QString &)>(12, 6, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 13 }, { QMetaType::QString, 14 },
        }}),
        // Method 'connectToNetwork'
        QtMocHelpers::MethodData<void(const QString &)>(12, 6, QMC::AccessPublic | QMC::MethodCloned, QMetaType::Void, {{
            { QMetaType::QString, 13 },
        }}),
        // Method 'resetConnectState'
        QtMocHelpers::MethodData<void()>(15, 6, QMC::AccessPublic, QMetaType::Void),
    };
    QtMocHelpers::UintData qt_properties {
        // property 'demo'
        QtMocHelpers::PropertyData<bool>(16, QMetaType::Bool, QMC::DefaultPropertyFlags | QMC::Constant),
        // property 'online'
        QtMocHelpers::PropertyData<bool>(17, QMetaType::Bool, QMC::DefaultPropertyFlags, 0),
        // property 'wiredConnected'
        QtMocHelpers::PropertyData<bool>(18, QMetaType::Bool, QMC::DefaultPropertyFlags, 0),
        // property 'wifiAvailable'
        QtMocHelpers::PropertyData<bool>(19, QMetaType::Bool, QMC::DefaultPropertyFlags, 0),
        // property 'activeSsid'
        QtMocHelpers::PropertyData<QString>(20, QMetaType::QString, QMC::DefaultPropertyFlags, 0),
        // property 'networks'
        QtMocHelpers::PropertyData<QVariantList>(21, 0x80000000 | 22, QMC::DefaultPropertyFlags | QMC::EnumOrFlag, 1),
        // property 'scanning'
        QtMocHelpers::PropertyData<bool>(23, QMetaType::Bool, QMC::DefaultPropertyFlags, 2),
        // property 'connectState'
        QtMocHelpers::PropertyData<enum ConnectState>(24, 0x80000000 | 25, QMC::DefaultPropertyFlags | QMC::EnumOrFlag, 3),
        // property 'errorString'
        QtMocHelpers::PropertyData<QString>(26, QMetaType::QString, QMC::DefaultPropertyFlags, 3),
    };
    QtMocHelpers::UintData qt_enums {
        // enum 'ConnectState'
        QtMocHelpers::EnumData<enum ConnectState>(25, 25, QMC::EnumFlags{}).add({
            {   27, ConnectState::Idle },
            {   28, ConnectState::Connecting },
            {   29, ConnectState::Connected },
            {   30, ConnectState::Failed },
        }),
    };
    QtMocHelpers::UintData qt_constructors {};
    QtMocHelpers::ClassInfos qt_classinfo({
            {    1,    2 },
            {    3,    4 },
    });
    return QtMocHelpers::metaObjectData<NetworkBackend, void>(QMC::MetaObjectFlag{}, qt_stringData,
            qt_methods, qt_properties, qt_enums, qt_constructors, qt_classinfo);
}
Q_CONSTINIT const QMetaObject NetworkBackend::staticMetaObject = { {
    QMetaObject::SuperData::link<QObject::staticMetaObject>(),
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN14NetworkBackendE_t>.stringdata,
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN14NetworkBackendE_t>.data,
    qt_static_metacall,
    nullptr,
    qt_staticMetaObjectRelocatingContent<qt_meta_tag_ZN14NetworkBackendE_t>.metaTypes,
    nullptr
} };

void NetworkBackend::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    auto *_t = static_cast<NetworkBackend *>(_o);
    if (_c == QMetaObject::InvokeMetaMethod) {
        switch (_id) {
        case 0: _t->statusChanged(); break;
        case 1: _t->networksChanged(); break;
        case 2: _t->scanningChanged(); break;
        case 3: _t->connectStateChanged(); break;
        case 4: _t->refresh(); break;
        case 5: _t->scan(); break;
        case 6: _t->connectToNetwork((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<QString>>(_a[2]))); break;
        case 7: _t->connectToNetwork((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1]))); break;
        case 8: _t->resetConnectState(); break;
        default: ;
        }
    }
    if (_c == QMetaObject::IndexOfMethod) {
        if (QtMocHelpers::indexOfMethod<void (NetworkBackend::*)()>(_a, &NetworkBackend::statusChanged, 0))
            return;
        if (QtMocHelpers::indexOfMethod<void (NetworkBackend::*)()>(_a, &NetworkBackend::networksChanged, 1))
            return;
        if (QtMocHelpers::indexOfMethod<void (NetworkBackend::*)()>(_a, &NetworkBackend::scanningChanged, 2))
            return;
        if (QtMocHelpers::indexOfMethod<void (NetworkBackend::*)()>(_a, &NetworkBackend::connectStateChanged, 3))
            return;
    }
    if (_c == QMetaObject::ReadProperty) {
        void *_v = _a[0];
        switch (_id) {
        case 0: *reinterpret_cast<bool*>(_v) = _t->demo(); break;
        case 1: *reinterpret_cast<bool*>(_v) = _t->online(); break;
        case 2: *reinterpret_cast<bool*>(_v) = _t->wiredConnected(); break;
        case 3: *reinterpret_cast<bool*>(_v) = _t->wifiAvailable(); break;
        case 4: *reinterpret_cast<QString*>(_v) = _t->activeSsid(); break;
        case 5: *reinterpret_cast<QVariantList*>(_v) = _t->networks(); break;
        case 6: *reinterpret_cast<bool*>(_v) = _t->scanning(); break;
        case 7: *reinterpret_cast<enum ConnectState*>(_v) = _t->connectState(); break;
        case 8: *reinterpret_cast<QString*>(_v) = _t->errorString(); break;
        default: break;
        }
    }
}

const QMetaObject *NetworkBackend::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *NetworkBackend::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_staticMetaObjectStaticContent<qt_meta_tag_ZN14NetworkBackendE_t>.strings))
        return static_cast<void*>(this);
    return QObject::qt_metacast(_clname);
}

int NetworkBackend::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
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
        _id -= 9;
    }
    return _id;
}

// SIGNAL 0
void NetworkBackend::statusChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 0, nullptr);
}

// SIGNAL 1
void NetworkBackend::networksChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 1, nullptr);
}

// SIGNAL 2
void NetworkBackend::scanningChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 2, nullptr);
}

// SIGNAL 3
void NetworkBackend::connectStateChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 3, nullptr);
}
QT_WARNING_POP
