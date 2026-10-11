#include <QCommandLineParser>
#include <QFile>
#include <QTimer>
#include <QGuiApplication>
#include <QLocale>
#include <QQmlApplicationEngine>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QtWebEngineQuick/qtwebenginequickglobal.h>

#include "dbustypes.h"

#include <utility>

int main(int argc, char *argv[])
{
    // В сессии (--fullscreen) окнам не нужны рамки. Без этого Qt рисует свою рамку с заголовком
    // (под cage нет серверных рамок), и при запуске на мгновение видны «очертания окна».
    for (int i = 1; i < argc; ++i) {
        if (qstrcmp(argv[i], "--fullscreen") == 0)
            qputenv("QT_WAYLAND_DISABLE_WINDOWDECORATION", "1");
    }
    // Оверлей по кнопке PS — прозрачное окно поверх игры
    QQuickWindow::setDefaultAlphaBuffer(true);
    // Встроенный браузер (вход в GOG) должен инициализироваться до приложения
    QtWebEngineQuick::initialize();
    QGuiApplication app(argc, argv);
    QGuiApplication::setOrganizationName(QStringLiteral("RedAnchor"));
    QGuiApplication::setApplicationName(QStringLiteral("RedAnchor Shell"));
    QGuiApplication::setApplicationVersion(QStringLiteral(PROJECT_VERSION_STRING));

    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("Оболочка RedAnchor"));
    parser.addHelpOption();
    parser.addVersionOption();
    QCommandLineOption fullscreenOpt(QStringLiteral("fullscreen"), QStringLiteral("Полноэкранный режим (сессия)."));
    QCommandLineOption resetOpt(QStringLiteral("reset-setup"), QStringLiteral("Заново пройти первичную настройку."));
    parser.addOptions({fullscreenOpt, resetOpt});
    parser.process(app);

    QLocale::setDefault(QLocale(QLocale::Russian, QLocale::Russia));
    QQuickStyle::setStyle(QStringLiteral("Basic"));
    registerDBusTypes();

    QQmlApplicationEngine engine;
    QObject::connect(
        &engine,
        &QQmlApplicationEngine::objectCreationFailed,
        &app,
        []() { QCoreApplication::exit(-1); },
        Qt::QueuedConnection);
    engine.setInitialProperties({
        {QStringLiteral("fullscreen"), parser.isSet(fullscreenOpt)},
        {QStringLiteral("resetSetup"), parser.isSet(resetOpt)},
        // Под cage и Weston (kiosk-shell) каждое окно и так на весь экран; просьба о полном
        // экране им только мешает
        {QStringLiteral("kiosk"), qEnvironmentVariable("REDANCHOR_SESSION_COMPOSITOR") == QLatin1String("cage")
                                      || qEnvironmentVariable("REDANCHOR_SESSION_COMPOSITOR") == QLatin1String("weston")},
    });
    engine.loadFromModule("RedAnchor", "Main");

    // Отметка для скрипта сессии: оболочка запустилась и показывает кадры. Если композитор
    // упадёт после этого, сессия перезапустится, а не вернёт на экран входа. Ставим её через
    // несколько секунд после первого кадра: если не работает отрисовка (видеокарта, драйвер),
    // скрипт должен перейти к программной, а не перезапускать то же самое
    const QString marker = qEnvironmentVariable("REDANCHOR_STARTED_FILE");
    auto *window = engine.rootObjects().isEmpty() ? nullptr : qobject_cast<QQuickWindow *>(engine.rootObjects().constFirst());
    if (!marker.isEmpty() && window) {
        QObject::connect(window, &QQuickWindow::frameSwapped, &app, [marker] {
            static bool once = false;
            if (std::exchange(once, true))
                return;
            QTimer::singleShot(3000, [marker] {
                QFile file(marker);
                if (file.open(QIODevice::WriteOnly))
                    file.write(QByteArray::number(QCoreApplication::applicationPid()));
            });
        });
    }

    return QGuiApplication::exec();
}
