/****************************************************************************
** Generated QML type registration code
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include <QtQml/qqml.h>
#include <QtQml/qqmlmoduleregistration.h>

#if __has_include(<accountmanager.h>)
#  include <accountmanager.h>
#endif
#if __has_include(<bluetoothmanager.h>)
#  include <bluetoothmanager.h>
#endif
#if __has_include(<componentsmanager.h>)
#  include <componentsmanager.h>
#endif
#if __has_include(<gameart.h>)
#  include <gameart.h>
#endif
#if __has_include(<gamelauncher.h>)
#  include <gamelauncher.h>
#endif
#if __has_include(<gamelibrary.h>)
#  include <gamelibrary.h>
#endif
#if __has_include(<gamepadmanager.h>)
#  include <gamepadmanager.h>
#endif
#if __has_include(<networkbackend.h>)
#  include <networkbackend.h>
#endif
#if __has_include(<storemanager.h>)
#  include <storemanager.h>
#endif
#if __has_include(<systeminfo.h>)
#  include <systeminfo.h>
#endif


#if !defined(QT_STATIC)
#define Q_QMLTYPE_EXPORT Q_DECL_EXPORT
#else
#define Q_QMLTYPE_EXPORT
#endif
Q_QMLTYPE_EXPORT void qml_register_types_RedAnchor()
{
    qmlRegisterModule("RedAnchor", 254, 0);
    QT_WARNING_PUSH QT_WARNING_DISABLE_DEPRECATED
    qmlRegisterTypesAndRevisions<AccountManager>("RedAnchor", 254);
    qmlRegisterTypesAndRevisions<BluetoothManager>("RedAnchor", 254);
    qmlRegisterTypesAndRevisions<ComponentsManager>("RedAnchor", 254);
    qmlRegisterTypesAndRevisions<GameArt>("RedAnchor", 254);
    qmlRegisterTypesAndRevisions<GameLauncher>("RedAnchor", 254);
    qmlRegisterTypesAndRevisions<GameLibrary>("RedAnchor", 254);
    qmlRegisterTypesAndRevisions<GamepadManager>("RedAnchor", 254);
    qmlRegisterTypesAndRevisions<NetworkBackend>("RedAnchor", 254);
    qmlRegisterEnum<NetworkBackend::ConnectState>("NetworkBackend::ConnectState");
    qmlRegisterTypesAndRevisions<StoreManager>("RedAnchor", 254);
    qmlRegisterTypesAndRevisions<SystemInfo>("RedAnchor", 254);
    QT_WARNING_POP
    qmlRegisterModule("RedAnchor", 254, 254);
}

static const QQmlModuleRegistration redAnchorRegistration("RedAnchor", qml_register_types_RedAnchor);
