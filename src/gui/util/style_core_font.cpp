#include "style_core_font.hpp"
#include "debug_logger.hpp"
#include <QFontDatabase>
#include <QFontInfo>
#include <QDir>
#include <QFileInfo>
#include <QStandardPaths>
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
        QStringLiteral(":/qt/qml/src/gui/resources/fonts/Vazirmatn-UI-NL-SemiBold.ttf"),
        QStringLiteral(":/qt/qml/src/gui/resources/fonts/Twemoji.ttf")
    };

    const QString extractedDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + QStringLiteral("/fonts");
    for (const auto& path : fontFiles) {
        QString diskPath = extractedDir + "/" + QFileInfo(path).fileName();
        QString target = QFileInfo::exists(diskPath) ? diskPath : path;
        int id = QFontDatabase::addApplicationFont(target);
        if (id < 0) {
            WEBCLIP_LOG(QStringLiteral("[Font] Failed to load font: ") + target);
        } else {
            QStringList families = QFontDatabase::applicationFontFamilies(id);
            WEBCLIP_LOG(QStringLiteral("[Font] Loaded font id=") + QString::number(id) +
                        QStringLiteral(" from ") + target +
                        QStringLiteral(" (families: ") + families.join(QStringLiteral(", ")) + QStringLiteral(")"));
        }
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
    WEBCLIP_LOG(QStringLiteral("[Font] Resolved monospace family: ") + g_monospaceFamily);
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
        f.setHintingPreference(QFont::PreferNoHinting);
        f.setStyleStrategy(QFont::PreferAntialias);
        return f;
    }

    QString primaryFamily = italic ? QStringLiteral("Google Sans") : QStringLiteral("Google Sans Flex");
    QFont f(primaryFamily);
    f.setFamilies({ primaryFamily, QStringLiteral("Vazirmatn UI NL"), QStringLiteral("Twemoji") });
    f.setStyleHint(QFont::SansSerif);
    f.setPixelSize(pixelSize);
    f.setWeight(weight);
    f.setItalic(italic);
    f.setHintingPreference(QFont::PreferNoHinting);
    f.setStyleStrategy(QFont::PreferAntialias);
    return f;
}

}
