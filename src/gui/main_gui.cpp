#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQuickStyle>
#include "util/icon_image_provider.hpp"

int main(int argc, char* argv[]) {
    QGuiApplication app(argc, argv);

    app.setOrganizationName("Burhanverse");
    app.setOrganizationDomain("burhanverse.dev");
    app.setApplicationName("WebClip");
    app.setApplicationDisplayName("WebClip");

    QQuickStyle::setStyle("Basic");

    QQmlApplicationEngine engine;
    engine.addImageProvider(QStringLiteral("icon"), new webclip::IconImageProvider());

    QObject::connect(
        &engine,
        &QQmlApplicationEngine::objectCreationFailed,
        &app,
        []() { QCoreApplication::exit(-1); },
        Qt::QueuedConnection
    );

    const QUrl url(QStringLiteral("qrc:/qt/qml/src/gui/qml/Main.qml"));
    engine.load(url);

    return app.exec();
}
