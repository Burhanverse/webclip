#include "style_core_font.hpp"
#include "debug_logger.hpp"
#include <QFontDatabase>
#include <QFontInfo>
#include <QDir>
#include <QFileInfo>
#include <QStandardPaths>
#include <QLoggingCategory>
#include <QSettings>

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

double detectSystemFontScale(QString* outDetails) {
    double scale = 1.0;
    QString details;

#if defined(Q_OS_WIN)
    bool detectedAccessibility = false;
    // 1. Check Windows Accessibility TextScaleFactor (100 to 225)
    QSettings accessSettings(QStringLiteral("HKEY_CURRENT_USER\\Software\\Microsoft\\Accessibility"), QSettings::NativeFormat);
    QVariant factorVar = accessSettings.value(QStringLiteral("TextScaleFactor"));
    if (factorVar.isValid()) {
        int textScaleFactor = factorVar.toInt();
        if (textScaleFactor >= 100 && textScaleFactor <= 500) {
            scale = textScaleFactor / 100.0;
            detectedAccessibility = true;
            details = QStringLiteral("Windows Accessibility TextScaleFactor=") + QString::number(textScaleFactor) + QStringLiteral("%");
        }
    }

    if (!detectedAccessibility) {
        NONCLIENTMETRICSW ncm;
        std::memset(&ncm, 0, sizeof(ncm));
        ncm.cbSize = sizeof(NONCLIENTMETRICSW);
        if (SystemParametersInfoW(SPI_GETNONCLIENTMETRICS, sizeof(ncm), &ncm, 0)) {
            // Standard Windows 9pt message font height is -12 at 96 DPI
            int lfHeight = std::abs(ncm.lfMessageFont.lfHeight);
            if (lfHeight > 0) {
                double metricScale = static_cast<double>(lfHeight) / 12.0;
                if (std::abs(metricScale - 1.0) > 0.05) {
                    scale = metricScale;
                    details = QStringLiteral("Windows NonClientMetrics lfHeight=") + QString::number(lfHeight) + QStringLiteral("px");
                }
            }
        }
    }
    if (details.isEmpty()) {
        details = QStringLiteral("Windows Default (100%)");
    }
#else
    QFont sysFont = QFontDatabase::systemFont(QFontDatabase::GeneralFont);
    qreal pt = sysFont.pointSizeF();
    if (pt <= 0.0) {
        pt = sysFont.pointSize();
    }
    if (pt > 0.0) {
        // Standard reference is 10.0pt (standard base across GNOME, KDE, and X11)
        scale = pt / 10.0;
        details = QStringLiteral("System font: '") + sysFont.family() + QStringLiteral("' ") +
                  QString::number(pt, 'f', 1) + QStringLiteral("pt (base 10pt)");
    } else {
        details = QStringLiteral("Default (100%)");
    }
#endif

    scale = std::clamp(scale, 0.75, 2.50);
    if (outDetails) {
        *outDetails = details + QStringLiteral(" -> scale=") + QString::number(scale, 'f', 2) + QStringLiteral("x");
    }
    return scale;
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
