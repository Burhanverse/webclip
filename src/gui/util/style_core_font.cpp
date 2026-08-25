#include "style_core_font.hpp"
#include <QFontDatabase>
#include <QFontInfo>
#include <QDir>
#include <QLoggingCategory>

namespace webclip::font {

namespace {

QString g_monospaceFamily;

QString resolveMonospaceFont() {
    const QStringList tryFirst = {
        QStringLiteral("Cascadia Mono"),
        QStringLiteral("Consolas"),
        QStringLiteral("Liberation Mono"),
        QStringLiteral("Menlo"),
        QStringLiteral("Fira Code"),
        QStringLiteral("JetBrains Mono"),
        QStringLiteral("DejaVu Sans Mono"),
        QStringLiteral("Courier New")
    };

    for (const auto& family : tryFirst) {
        const auto resolved = QFontInfo(QFont(family)).family();
        if (resolved.trimmed().startsWith(family, Qt::CaseInsensitive)) {
            return family;
        }
    }

    return QFontDatabase::systemFont(QFontDatabase::FixedFont).family();
}

}

void initFonts() {
    const QStringList fontFiles = {
        QStringLiteral(":/qt/qml/src/gui/resources/fonts/GoogleSansFlexRegular.ttf"),
        QStringLiteral(":/qt/qml/src/gui/resources/fonts/GoogleSansFlexMedium.ttf"),
        QStringLiteral(":/qt/qml/src/gui/resources/fonts/GoogleSansItalic.ttf"),
        QStringLiteral(":/qt/qml/src/gui/resources/fonts/GoogleSansMediumItalic.ttf"),
        QStringLiteral(":/qt/qml/src/gui/resources/fonts/Vazirmatn-UI-NL-Regular.ttf"),
        QStringLiteral(":/qt/qml/src/gui/resources/fonts/Vazirmatn-UI-NL-SemiBold.ttf")
    };

    for (const auto& path : fontFiles) {
        QFontDatabase::addApplicationFont(path);
    }

    const QString googleSansFlex = QStringLiteral("Google Sans Flex");
    const QString googleSans = QStringLiteral("Google Sans");
    const QString vazirmatn = QStringLiteral("Vazirmatn UI NL");

    QFont::insertSubstitution(googleSansFlex, vazirmatn);
    QFont::insertSubstitution(googleSans, vazirmatn);

#ifdef Q_OS_WIN
    const QStringList winFallbacks = { QStringLiteral("Ebrima"), QStringLiteral("Nirmala UI"), QStringLiteral("Segoe UI Symbol") };
    QFont::insertSubstitutions(googleSansFlex, winFallbacks);
    QFont::insertSubstitutions(googleSans, winFallbacks);
#elif defined(Q_OS_MAC)
    const QStringList macFallbacks = { QStringLiteral("STIXGeneral"), QStringLiteral(".SF NS Text"), QStringLiteral("Helvetica Neue"), QStringLiteral("Lucida Grande") };
    QFont::insertSubstitutions(googleSansFlex, macFallbacks);
    QFont::insertSubstitutions(googleSans, macFallbacks);
#endif

    g_monospaceFamily = resolveMonospaceFont();
}

QString monospaceFontFamily() {
    if (g_monospaceFamily.isEmpty()) {
        g_monospaceFamily = resolveMonospaceFont();
    }
    return g_monospaceFamily;
}

QFont createFont(int pixelSize, QFont::Weight weight, bool italic, bool monospace) {
    if (monospace) {
        QFont f(monospaceFontFamily());
        f.setStyleHint(QFont::Monospace);
        f.setPixelSize(pixelSize);
        f.setWeight(weight);
        f.setItalic(italic);
        return f;
    }

    QString primaryFamily = italic ? QStringLiteral("Google Sans") : QStringLiteral("Google Sans Flex");
    QFont f(primaryFamily);
    f.setFamilies({ primaryFamily, QStringLiteral("Vazirmatn UI NL") });
    f.setStyleHint(QFont::SansSerif);
    f.setPixelSize(pixelSize);
    f.setWeight(weight);
    f.setItalic(italic);
    return f;
}

}
