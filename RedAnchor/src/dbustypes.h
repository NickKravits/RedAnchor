#pragma once

#include <QDBusObjectPath>
#include <QMap>
#include <QMetaType>
#include <QVariantMap>

// a{sa{sv}} — интерфейсы объекта (BlueZ) или настройки соединения (NetworkManager)
using InterfaceMap = QMap<QString, QVariantMap>;
// a{oa{sa{sv}}} — результат org.freedesktop.DBus.ObjectManager.GetManagedObjects
using ManagedObjects = QMap<QDBusObjectPath, InterfaceMap>;

Q_DECLARE_METATYPE(InterfaceMap)
Q_DECLARE_METATYPE(ManagedObjects)

void registerDBusTypes();
