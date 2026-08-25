#include "md3_theme.hpp"
#include "../util/style_core_font.hpp"
#include <QGuiApplication>
#include <QStyleHints>
#include <QPalette>
#include <cmath>

namespace webclip {

MD3Theme* MD3Theme::instance() {
    static MD3Theme s_instance;
    return &s_instance;
}

MD3Theme::MD3Theme(QObject* parent)
    : QObject(parent) {
#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
    if (QGuiApplication::styleHints()) {
        connect(QGuiApplication::styleHints(), &QStyleHints::colorSchemeChanged, this, [this]() {
            if (themeMode_ == 0) {
                emit isDarkChanged();
                emit themeChanged();
            }
        });
    }
#endif
}

void MD3Theme::setThemeMode(int mode) {
    if (themeMode_ != mode) {
        themeMode_ = mode;
        emit themeModeChanged();
        emit isDarkChanged();
        emit isPitchBlackChanged();
        emit themeChanged();
    }
}

void MD3Theme::setAccentPreset(const QString& preset) {
    if (accentPreset_ != preset) {
        accentPreset_ = preset;
        if (preset.startsWith('#')) {
            QColor c(preset);
            if (c.isValid()) customColor_ = c;
        }
        emit accentPresetChanged();
        emit themeChanged();
    }
}

void MD3Theme::setCustomColor(const QColor& color) {
    if (customColor_ != color && color.isValid()) {
        customColor_ = color;
        accentPreset_ = "custom";
        emit customColorChanged();
        emit accentPresetChanged();
        emit themeChanged();
    }
}

bool MD3Theme::isDark() const {
    if (themeMode_ == 1) return false;
    if (themeMode_ == 2 || themeMode_ == 3) return true;
#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
    if (QGuiApplication::styleHints()) {
        return QGuiApplication::styleHints()->colorScheme() == Qt::ColorScheme::Dark;
    }
#else
    // Fallback for Qt < 6.5 (e.g. Qt 6.4 on Ubuntu runners)
    if (QGuiApplication::palette().color(QPalette::Window).value() < 128) {
        return true;
    }
#endif
    return false;
}

QColor MD3Theme::activeSeedColor() const {
    if (accentPreset_ == "blue") return QColor("#2196F3");
    if (accentPreset_ == "teal") return QColor("#009688");
    if (accentPreset_ == "green") return QColor("#4CAF50");
    if (accentPreset_ == "orange") return QColor("#FF9800");
    if (accentPreset_ == "red") return QColor("#F44336");
    if (accentPreset_ == "pink") return QColor("#E91E63");
    // Direct user picks: honor the exact color they chose instead of
    // re-deriving a normalized tonal palette from it
    if (accentPreset_ == "custom" && customColor_.isValid()) return customColor_;
    if (accentPreset_.startsWith('#')) {
        QColor c(accentPreset_);
        if (c.isValid()) return c;
    }
    return QColor("#6750A4"); // M3 Default Purple
}

bool MD3Theme::usesDirectSeedColor() const {
    return accentPreset_ == "custom" || accentPreset_.startsWith('#');
}

static QColor contrastTextColor(const QColor& bg) {
    qreal lum = 0.2126 * bg.redF() + 0.7152 * bg.greenF() + 0.0722 * bg.blueF();
    return lum > 0.5 ? QColor("#1B1B1F") : QColor("#FFFFFF");
}

QColor MD3Theme::primary() const {
    QColor seed = activeSeedColor();
    if (usesDirectSeedColor()) {
        return seed;
    }
    if (isPitchBlack()) {
        return QColor::fromHslF(seed.hslHueF(), qBound(0.60f, seed.hslSaturationF(), 0.98f), 0.72f);
    } else if (isDark()) {
        return QColor::fromHslF(seed.hslHueF(), qBound(0.45f, seed.hslSaturationF(), 0.90f), 0.76f);
    } else {
        return QColor::fromHslF(seed.hslHueF(), qBound(0.55f, seed.hslSaturationF(), 0.95f), 0.44f);
    }
}

QColor MD3Theme::onPrimary() const {
    QColor seed = activeSeedColor();
    if (usesDirectSeedColor()) {
        return contrastTextColor(seed);
    }
    return isDark() ? QColor::fromHslF(seed.hslHueF(), 0.70f, 0.20f) : QColor("#FFFFFF");
}

QColor MD3Theme::primaryContainer() const {
    QColor seed = activeSeedColor();
    if (usesDirectSeedColor()) {
        float h = seed.hslHueF() >= 0.0f ? seed.hslHueF() : 0.0f;
        float s = seed.hslSaturationF();
        float l = seed.lightnessF();
        if (isPitchBlack()) {
            return QColor::fromHslF(h, s * 0.6f, qBound(0.10f, l * 0.45f, 0.22f));
        } else if (isDark()) {
            return QColor::fromHslF(h, s * 0.7f, qBound(0.20f, l * 0.60f, 0.32f));
        }
        return QColor::fromHslF(h, s * 0.55f, qBound(0.86f, l + 0.25f, 0.93f));
    }
    if (isPitchBlack()) {
        return QColor::fromHslF(seed.hslHueF(), 0.60f, 0.22f);
    } else if (isDark()) {
        return QColor::fromHslF(seed.hslHueF(), 0.50f, 0.28f);
    } else {
        return QColor::fromHslF(seed.hslHueF(), 0.70f, 0.90f);
    }
}

QColor MD3Theme::onPrimaryContainer() const {
    QColor seed = activeSeedColor();
    if (usesDirectSeedColor()) {
        return contrastTextColor(primaryContainer());
    }
    if (isPitchBlack()) {
        return QColor::fromHslF(seed.hslHueF(), 0.75f, 0.90f);
    } else if (isDark()) {
        return QColor::fromHslF(seed.hslHueF(), 0.65f, 0.92f);
    } else {
        return QColor::fromHslF(seed.hslHueF(), 0.80f, 0.18f);
    }
}

QColor MD3Theme::secondary() const {
    QColor seed = activeSeedColor();
    return isDark() ? QColor::fromHslF(seed.hslHueF(), 0.20f, 0.78f) : QColor::fromHslF(seed.hslHueF(), 0.25f, 0.40f);
}

QColor MD3Theme::onSecondary() const {
    QColor seed = activeSeedColor();
    return isDark() ? QColor::fromHslF(seed.hslHueF(), 0.20f, 0.22f) : QColor("#FFFFFF");
}

QColor MD3Theme::secondaryContainer() const {
    QColor seed = activeSeedColor();
    if (isPitchBlack()) {
        return QColor::fromHslF(seed.hslHueF(), qBound(0.25f, seed.hslSaturationF() * 0.45f, 0.55f), 0.12f);
    } else if (isDark()) {
        return QColor::fromHslF(seed.hslHueF(), qBound(0.20f, seed.hslSaturationF() * 0.40f, 0.45f), 0.22f);
    } else {
        return QColor::fromHslF(seed.hslHueF(), qBound(0.18f, seed.hslSaturationF() * 0.35f, 0.40f), 0.92f);
    }
}

QColor MD3Theme::onSecondaryContainer() const {
    QColor seed = activeSeedColor();
    if (isPitchBlack()) {
        return QColor::fromHslF(seed.hslHueF(), 0.35f, 0.90f);
    } else if (isDark()) {
        return QColor::fromHslF(seed.hslHueF(), 0.30f, 0.90f);
    } else {
        return QColor::fromHslF(seed.hslHueF(), 0.50f, 0.18f);
    }
}

QColor MD3Theme::tertiary() const {
    QColor seed = activeSeedColor();
    float h = std::fmod(seed.hslHueF() + 0.15f, 1.0f);
    return isDark() ? QColor::fromHslF(h, 0.45f, 0.80f) : QColor::fromHslF(h, 0.40f, 0.40f);
}

QColor MD3Theme::onTertiary() const {
    return isDark() ? QColor("#381E72") : QColor("#FFFFFF");
}

QColor MD3Theme::tertiaryContainer() const {
    QColor seed = activeSeedColor();
    float h = std::fmod(seed.hslHueF() + 0.15f, 1.0f);
    return isDark() ? QColor::fromHslF(h, 0.35f, 0.28f) : QColor::fromHslF(h, 0.50f, 0.92f);
}

QColor MD3Theme::onTertiaryContainer() const {
    QColor seed = activeSeedColor();
    float h = std::fmod(seed.hslHueF() + 0.15f, 1.0f);
    return isDark() ? QColor::fromHslF(h, 0.50f, 0.92f) : QColor::fromHslF(h, 0.60f, 0.18f);
}

QColor MD3Theme::error() const {
    return isDark() ? QColor("#F2B8B5") : QColor("#B3261E");
}

QColor MD3Theme::onError() const {
    return isDark() ? QColor("#601410") : QColor("#FFFFFF");
}

QColor MD3Theme::errorContainer() const {
    return isDark() ? QColor("#8C1D18") : QColor("#F9DEDC");
}

QColor MD3Theme::onErrorContainer() const {
    return isDark() ? QColor("#F9DEDC") : QColor("#410E0B");
}

QColor MD3Theme::surface() const {
    if (isPitchBlack()) return QColor("#000000");
    QColor seed = activeSeedColor();
    float h = seed.hslHueF();
    float s = qBound(0.03f, seed.hslSaturationF() * 0.16f, 0.10f);
    return isDark() ? QColor::fromHslF(h, s, 0.08f) : QColor::fromHslF(h, s * 0.6f, 0.98f);
}

QColor MD3Theme::surfaceDim() const {
    if (isPitchBlack()) return QColor("#000000");
    QColor seed = activeSeedColor();
    float h = seed.hslHueF();
    float s = qBound(0.03f, seed.hslSaturationF() * 0.16f, 0.10f);
    return isDark() ? QColor::fromHslF(h, s, 0.07f) : QColor::fromHslF(h, s, 0.87f);
}

QColor MD3Theme::surfaceBright() const {
    if (isPitchBlack()) return QColor("#161616");
    QColor seed = activeSeedColor();
    float h = seed.hslHueF();
    float s = qBound(0.03f, seed.hslSaturationF() * 0.16f, 0.10f);
    return isDark() ? QColor::fromHslF(h, s, 0.22f) : QColor::fromHslF(h, s * 0.4f, 0.99f);
}

QColor MD3Theme::surfaceContainerLowest() const {
    if (isPitchBlack()) return QColor("#000000");
    QColor seed = activeSeedColor();
    float h = seed.hslHueF();
    float s = qBound(0.03f, seed.hslSaturationF() * 0.16f, 0.10f);
    return isDark() ? QColor::fromHslF(h, s, 0.05f) : QColor("#FFFFFF");
}

QColor MD3Theme::surfaceContainerLow() const {
    if (isPitchBlack()) return QColor("#0A0A0A");
    QColor seed = activeSeedColor();
    float h = seed.hslHueF();
    float s = qBound(0.03f, seed.hslSaturationF() * 0.16f, 0.10f);
    return isDark() ? QColor::fromHslF(h, s, 0.11f) : QColor::fromHslF(h, s, 0.95f);
}

QColor MD3Theme::surfaceContainer() const {
    if (isPitchBlack()) return QColor("#121212");
    QColor seed = activeSeedColor();
    float h = seed.hslHueF();
    float s = qBound(0.03f, seed.hslSaturationF() * 0.16f, 0.10f);
    return isDark() ? QColor::fromHslF(h, s, 0.14f) : QColor::fromHslF(h, s, 0.92f);
}

QColor MD3Theme::surfaceContainerHigh() const {
    if (isPitchBlack()) return QColor("#181818");
    QColor seed = activeSeedColor();
    float h = seed.hslHueF();
    float s = qBound(0.03f, seed.hslSaturationF() * 0.16f, 0.10f);
    return isDark() ? QColor::fromHslF(h, s, 0.18f) : QColor::fromHslF(h, s, 0.89f);
}

QColor MD3Theme::surfaceContainerHighest() const {
    if (isPitchBlack()) return QColor("#222222");
    QColor seed = activeSeedColor();
    float h = seed.hslHueF();
    float s = qBound(0.03f, seed.hslSaturationF() * 0.16f, 0.10f);
    return isDark() ? QColor::fromHslF(h, s, 0.22f) : QColor::fromHslF(h, s, 0.85f);
}

QColor MD3Theme::onSurface() const {
    if (isPitchBlack()) return QColor("#FFFFFF");
    QColor seed = activeSeedColor();
    float h = seed.hslHueF();
    float s = qBound(0.02f, seed.hslSaturationF() * 0.08f, 0.05f);
    return isDark() ? QColor::fromHslF(h, s, 0.92f) : QColor::fromHslF(h, s, 0.12f);
}

QColor MD3Theme::onSurfaceVariant() const {
    if (isPitchBlack()) return QColor("#9E9E9E");
    QColor seed = activeSeedColor();
    float h = seed.hslHueF();
    float s = qBound(0.03f, seed.hslSaturationF() * 0.15f, 0.10f);
    return isDark() ? QColor::fromHslF(h, s, 0.76f) : QColor::fromHslF(h, s, 0.32f);
}

QColor MD3Theme::outline() const {
    if (isPitchBlack()) return QColor("#383838");
    QColor seed = activeSeedColor();
    float h = seed.hslHueF();
    float s = qBound(0.03f, seed.hslSaturationF() * 0.15f, 0.10f);
    return isDark() ? QColor::fromHslF(h, s, 0.55f) : QColor::fromHslF(h, s, 0.50f);
}

QColor MD3Theme::outlineVariant() const {
    if (isPitchBlack()) return QColor("#222222");
    QColor seed = activeSeedColor();
    float h = seed.hslHueF();
    float s = qBound(0.03f, seed.hslSaturationF() * 0.15f, 0.10f);
    return isDark() ? QColor::fromHslF(h, s, 0.25f) : QColor::fromHslF(h, s, 0.82f);
}

QFont MD3Theme::createFont(int pixelSize, QFont::Weight weight, bool italic, bool monospace) const {
    return font::createFont(pixelSize, weight, italic, monospace);
}

QFont MD3Theme::headlineSmall() const {
    return createFont(24, QFont::Normal);
}

QFont MD3Theme::titleLarge() const {
    return createFont(22, QFont::Normal);
}

QFont MD3Theme::titleMedium() const {
    return createFont(16, QFont::Medium);
}

QFont MD3Theme::titleSmall() const {
    return createFont(14, QFont::Medium);
}

QFont MD3Theme::bodyLarge() const {
    return createFont(16, QFont::Normal);
}

QFont MD3Theme::bodyMedium() const {
    return createFont(14, QFont::Normal);
}

QFont MD3Theme::bodySmall() const {
    return createFont(12, QFont::Normal);
}

QFont MD3Theme::labelLarge() const {
    return createFont(14, QFont::Medium);
}

QFont MD3Theme::labelMedium() const {
    return createFont(12, QFont::Medium);
}

QFont MD3Theme::labelSmall() const {
    return createFont(11, QFont::Medium);
}

QFont MD3Theme::codeMedium() const {
    return createFont(13, QFont::Normal, false, true);
}

QFont MD3Theme::codeSmall() const {
    return createFont(11, QFont::Normal, false, true);
}

} // namespace webclip
