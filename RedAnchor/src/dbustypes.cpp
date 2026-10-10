#include "dbustypes.h"

#include <QDBusMetaType>

void registerDBusTypes()
{
    qDBusRegisterMetaType<InterfaceMap>();
    qDBusRegisterMetaType<ManagedObjects>();
}
