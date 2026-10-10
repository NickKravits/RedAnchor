#pragma once

#include <QObject>
#include <QString>
#include <QtQml/qqmlregistration.h>

// Сведения о системе для раздела настроек «О системе».
// Читаются один раз при создании из /etc/os-release и /proc.
class SystemInfo : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

    Q_PROPERTY(QString osName MEMBER m_osName CONSTANT)
    Q_PROPERTY(QString shellVersion MEMBER m_shellVersion CONSTANT)
    Q_PROPERTY(QString kernel MEMBER m_kernel CONSTANT)
    Q_PROPERTY(QString architecture MEMBER m_architecture CONSTANT)
    Q_PROPERTY(QString hostName MEMBER m_hostName CONSTANT)
    Q_PROPERTY(QString cpu MEMBER m_cpu CONSTANT)
    Q_PROPERTY(int cpuThreads MEMBER m_cpuThreads CONSTANT)
    Q_PROPERTY(QString memory MEMBER m_memory CONSTANT)
    Q_PROPERTY(QString qtVersion MEMBER m_qtVersion CONSTANT)

public:
    explicit SystemInfo(QObject *parent = nullptr);

private:
    QString m_osName;
    QString m_shellVersion;
    QString m_kernel;
    QString m_architecture;
    QString m_hostName;
    QString m_cpu;
    int m_cpuThreads = 0;
    QString m_memory;
    QString m_qtVersion;
};
