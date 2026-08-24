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
    if (accentPreset_ == "custom" && customColor_.isValid()) return customColor_;
    if (accentPreset_.startsWith('#')) {
        QColor c(accentPreset_);
        if (c.isValid()) return c;
    }
    return QColor("#6750A4"); // M3 Default Purple
}

// MD3 Tonal Palette derived from active seed
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
QFont MD3Theme::createFont(int pixelSize, QFont::Weight weight) const {
    QFont f(QStringLiteral("Open Sans"));
    f.setStyleHint(QFont::SansSerif);
    f.setPixelSize(pixelSize);
    f.setWeight(weight);
    return f;
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

} // namespace webclip
