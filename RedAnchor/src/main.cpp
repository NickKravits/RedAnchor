#include <QCommandLineParser>
#include <QGuiApplication>
#include <QLocale>
#include <QQmlApplicationEngine>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QtWebEngineQuick/qtwebenginequickglobal.h>

#include "dbustypes.h"

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
        // Под cage каждое окно и так на весь экран; просьба о полном экране ему только мешает
        {QStringLiteral("kiosk"), qEnvironmentVariable("REDANCHOR_SESSION_COMPOSITOR") == QLatin1String("cage")},
    });
    engine.loadFromModule("RedAnchor", "Main");

    return QGuiApplication::exec();
}
