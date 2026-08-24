#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQuickStyle>
#include <QFont>
#include <QFontDatabase>
#include <QLoggingCategory>
#include "util/icon_image_provider.hpp"

#if defined(__SANITIZE_ADDRESS__) || (defined(__has_feature) && __has_feature(address_sanitizer))
extern "C" const char* __lsan_default_suppressions() {
    return "leak:libfontconfig\n"
           "leak:libexpat\n"
           "leak:libGL\n"
           "leak:libwayland\n"
           "leak:QFontDatabase\n"
           "leak:QFontconfigDatabase\n";
}
#endif

int main(int argc, char* argv[]) {
    qputenv("QT_LOGGING_RULES", "qt.text.font.db*=false;qt.gui.fontdatabase*=false");
    QLoggingCategory::setFilterRules(QStringLiteral("qt.text.font.db*=false\nqt.gui.fontdatabase*=false"));

    QGuiApplication app(argc, argv);

    QFontDatabase::addApplicationFont(QStringLiteral(":/qt/qml/src/gui/resources/fonts/OpenSans-Regular.ttf"));
    QFontDatabase::addApplicationFont(QStringLiteral(":/qt/qml/src/gui/resources/fonts/OpenSans-SemiBold.ttf"));
    QFontDatabase::addApplicationFont(QStringLiteral(":/qt/qml/src/gui/resources/fonts/OpenSans-Bold.ttf"));

    QFont defaultFont(QStringLiteral("Open Sans"));
    defaultFont.setStyleHint(QFont::SansSerif);
    app.setFont(defaultFont);

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
