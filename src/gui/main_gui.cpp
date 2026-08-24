#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQuickStyle>

int main(int argc, char* argv[]) {
    QGuiApplication app(argc, argv);

    app.setOrganizationName("Burhanverse");
    app.setOrganizationDomain("burhanverse.dev");
    app.setApplicationName("WebClip");
    app.setApplicationDisplayName("WebClip");

    QQuickStyle::setStyle("Basic");

    QQmlApplicationEngine engine;

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
