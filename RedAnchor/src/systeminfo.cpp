#include "systeminfo.h"

#include <QCoreApplication>
#include <QFile>
#include <QSysInfo>
#include <QThread>

namespace {

// Значение поля "key: value" или KEY="value" из текстового файла
QString field(const QString &path, const QString &key, QChar separator)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return {};
    while (!file.atEnd()) {
        const QString line = QString::fromUtf8(file.readLine());
        const qsizetype pos = line.indexOf(separator);
        if (pos <= 0 || line.left(pos).trimmed() != key)
            continue;
        QString value = line.mid(pos + 1).trimmed();
        if (value.size() >= 2 && value.startsWith(QLatin1Char('"')) && value.endsWith(QLatin1Char('"')))
            value = value.mid(1, value.size() - 2);
        return value;
    }
    return {};
}

} // namespace

SystemInfo::SystemInfo(QObject *parent)
    : QObject(parent)
{
    m_osName = field(QStringLiteral("/etc/os-release"), QStringLiteral("PRETTY_NAME"), QLatin1Char('='));
    if (m_osName.isEmpty())
        m_osName = QSysInfo::prettyProductName();
    m_shellVersion = QCoreApplication::applicationVersion();
    m_kernel = QSysInfo::kernelVersion();
    m_architecture = QSysInfo::currentCpuArchitecture();
    m_hostName = QSysInfo::machineHostName();
    m_cpu = field(QStringLiteral("/proc/cpuinfo"), QStringLiteral("model name"), QLatin1Char(':'));
    m_cpuThreads = QThread::idealThreadCount();

    // MemTotal: 16318764 kB
    const QString mem = field(QStringLiteral("/proc/meminfo"), QStringLiteral("MemTotal"), QLatin1Char(':'));
    const double kb = mem.section(QLatin1Char(' '), 0, 0).toDouble();
    if (kb > 0)
        m_memory = tr("%1 ГБ").arg(QString::number(kb / 1024.0 / 1024.0, 'f', 1));
    m_qtVersion = QString::fromLatin1(qVersion());
}
