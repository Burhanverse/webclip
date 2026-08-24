#include "md3_theme.hpp"
#include <QGuiApplication>
#include <QStyleHints>
#include <cmath>

namespace webclip {

MD3Theme* MD3Theme::instance() {
    static MD3Theme s_instance;
    return &s_instance;
}

MD3Theme::MD3Theme(QObject* parent)
    : QObject(parent) {
    if (QGuiApplication::styleHints()) {
        connect(QGuiApplication::styleHints(), &QStyleHints::colorSchemeChanged, this, [this]() {
            if (themeMode_ == 0) {
                emit isDarkChanged();
                emit themeChanged();
            }
        });
    }
}

void MD3Theme::setThemeMode(int mode) {
    if (themeMode_ != mode) {
        themeMode_ = mode;
        emit themeModeChanged();
        emit isDarkChanged();
        emit themeChanged();
    }
}

void MD3Theme::setAccentPreset(const QString& preset) {
    if (accentPreset_ != preset) {
        accentPreset_ = preset;
        emit accentPresetChanged();
        emit themeChanged();
    }
}

bool MD3Theme::isDark() const {
    if (themeMode_ == 1) return false;
    if (themeMode_ == 2) return true;
    if (QGuiApplication::styleHints()) {
        return QGuiApplication::styleHints()->colorScheme() == Qt::ColorScheme::Dark;
    }
    return false;
}

QColor MD3Theme::activeSeedColor() const {
    if (accentPreset_ == "blue") return QColor("#2196F3");
    if (accentPreset_ == "teal") return QColor("#009688");
    if (accentPreset_ == "green") return QColor("#4CAF50");
    if (accentPreset_ == "orange") return QColor("#FF9800");
    if (accentPreset_ == "red") return QColor("#F44336");
    if (accentPreset_ == "pink") return QColor("#E91E63");
    return QColor("#6750A4"); // M3 Default Purple
}

// MD3 Tonal Palette derived from active preset
QColor MD3Theme::primary() const {
    QColor seed = activeSeedColor();
    if (isDark()) {
        return QColor::fromHslF(seed.hslHueF(), qBound(0.45, seed.hslSaturationF(), 0.90), 0.76);
    } else {
        return QColor::fromHslF(seed.hslHueF(), qBound(0.55, seed.hslSaturationF(), 0.95), 0.44);
    }
}

QColor MD3Theme::onPrimary() const {
    QColor seed = activeSeedColor();
    return isDark() ? QColor::fromHslF(seed.hslHueF(), 0.7, 0.2) : QColor("#FFFFFF");
}

QColor MD3Theme::primaryContainer() const {
    QColor seed = activeSeedColor();
    if (isDark()) {
        return QColor::fromHslF(seed.hslHueF(), 0.50, 0.28);
    } else {
        return QColor::fromHslF(seed.hslHueF(), 0.70, 0.90);
    }
}

QColor MD3Theme::onPrimaryContainer() const {
    QColor seed = activeSeedColor();
    if (isDark()) {
        return QColor::fromHslF(seed.hslHueF(), 0.65, 0.92);
    } else {
        return QColor::fromHslF(seed.hslHueF(), 0.80, 0.18);
    }
}

QColor MD3Theme::secondary() const {
    QColor seed = activeSeedColor();
    return isDark() ? QColor::fromHslF(seed.hslHueF(), 0.20, 0.78) : QColor::fromHslF(seed.hslHueF(), 0.25, 0.40);
}

QColor MD3Theme::onSecondary() const {
    QColor seed = activeSeedColor();
    return isDark() ? QColor::fromHslF(seed.hslHueF(), 0.2, 0.22) : QColor("#FFFFFF");
}

QColor MD3Theme::secondaryContainer() const {
    QColor seed = activeSeedColor();
    return isDark() ? QColor::fromHslF(seed.hslHueF(), 0.20, 0.28) : QColor::fromHslF(seed.hslHueF(), 0.25, 0.90);
}

QColor MD3Theme::onSecondaryContainer() const {
    QColor seed = activeSeedColor();
    return isDark() ? QColor::fromHslF(seed.hslHueF(), 0.20, 0.92) : QColor::fromHslF(seed.hslHueF(), 0.30, 0.15);
}

QColor MD3Theme::tertiary() const {
    QColor seed = activeSeedColor();
    qreal h = std::fmod(seed.hslHueF() + 0.15, 1.0);
    return isDark() ? QColor::fromHslF(h, 0.45, 0.8) : QColor::fromHslF(h, 0.4, 0.4);
}

QColor MD3Theme::onTertiary() const {
    return isDark() ? QColor("#381E72") : QColor("#FFFFFF");
}

QColor MD3Theme::tertiaryContainer() const {
    QColor seed = activeSeedColor();
    qreal h = std::fmod(seed.hslHueF() + 0.15, 1.0);
    return isDark() ? QColor::fromHslF(h, 0.35, 0.28) : QColor::fromHslF(h, 0.5, 0.92);
}

QColor MD3Theme::onTertiaryContainer() const {
    QColor seed = activeSeedColor();
    qreal h = std::fmod(seed.hslHueF() + 0.15, 1.0);
    return isDark() ? QColor::fromHslF(h, 0.5, 0.92) : QColor::fromHslF(h, 0.6, 0.18);
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
    return isDark() ? QColor("#141218") : QColor("#FEF7FF");
}

QColor MD3Theme::surfaceDim() const {
    return isDark() ? QColor("#141218") : QColor("#DED8E1");
}

QColor MD3Theme::surfaceBright() const {
    return isDark() ? QColor("#3B383E") : QColor("#FEF7FF");
}

QColor MD3Theme::surfaceContainerLowest() const {
    return isDark() ? QColor("#0F0D13") : QColor("#FFFFFF");
}

QColor MD3Theme::surfaceContainerLow() const {
    return isDark() ? QColor("#1D1B20") : QColor("#F7F2FA");
}

QColor MD3Theme::surfaceContainer() const {
    return isDark() ? QColor("#211F26") : QColor("#F3EDF7");
}

QColor MD3Theme::surfaceContainerHigh() const {
    return isDark() ? QColor("#2B2930") : QColor("#ECE6F0");
}

QColor MD3Theme::surfaceContainerHighest() const {
    return isDark() ? QColor("#36343B") : QColor("#E6E0E9");
}

QColor MD3Theme::onSurface() const {
    return isDark() ? QColor("#E6E0E9") : QColor("#1D1B20");
}

QColor MD3Theme::onSurfaceVariant() const {
    return isDark() ? QColor("#CAC4D0") : QColor("#49454F");
}

QColor MD3Theme::outline() const {
    return isDark() ? QColor("#938F99") : QColor("#79747E");
}

QColor MD3Theme::outlineVariant() const {
    return isDark() ? QColor("#49454F") : QColor("#CAC4D0");
}

// Typography
QFont MD3Theme::headlineSmall() const {
    QFont f;
    f.setPixelSize(24);
    f.setWeight(QFont::Normal);
    return f;
}

QFont MD3Theme::titleLarge() const {
    QFont f;
    f.setPixelSize(22);
    f.setWeight(QFont::Normal);
    return f;
}

QFont MD3Theme::titleMedium() const {
    QFont f;
    f.setPixelSize(16);
    f.setWeight(QFont::Medium);
    return f;
}

QFont MD3Theme::titleSmall() const {
    QFont f;
    f.setPixelSize(14);
    f.setWeight(QFont::Medium);
    return f;
}

QFont MD3Theme::bodyLarge() const {
    QFont f;
    f.setPixelSize(16);
    f.setWeight(QFont::Normal);
    return f;
}

QFont MD3Theme::bodyMedium() const {
    QFont f;
    f.setPixelSize(14);
    f.setWeight(QFont::Normal);
    return f;
}

QFont MD3Theme::bodySmall() const {
    QFont f;
    f.setPixelSize(12);
    f.setWeight(QFont::Normal);
    return f;
}

QFont MD3Theme::labelLarge() const {
    QFont f;
    f.setPixelSize(14);
    f.setWeight(QFont::Medium);
    return f;
}

QFont MD3Theme::labelMedium() const {
    QFont f;
    f.setPixelSize(12);
    f.setWeight(QFont::Medium);
    return f;
}

QFont MD3Theme::labelSmall() const {
    QFont f;
    f.setPixelSize(11);
    f.setWeight(QFont::Medium);
    return f;
}

} // namespace webclip
