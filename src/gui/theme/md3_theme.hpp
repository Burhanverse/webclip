#pragma once

#include <QObject>
#include <QColor>
#include <QFont>
#include <QtQml/qqmlregistration.h>
#include <QQmlEngine>
#include <QJSEngine>

namespace webclip {

class MD3Theme : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

    Q_PROPERTY(int themeMode READ themeMode WRITE setThemeMode NOTIFY themeModeChanged)
    Q_PROPERTY(bool isDark READ isDark NOTIFY isDarkChanged)
    Q_PROPERTY(QString accentPreset READ accentPreset WRITE setAccentPreset NOTIFY accentPresetChanged)

    // MD3 Color Tokens
    Q_PROPERTY(QColor primary READ primary NOTIFY themeChanged)
    Q_PROPERTY(QColor onPrimary READ onPrimary NOTIFY themeChanged)
    Q_PROPERTY(QColor primaryContainer READ primaryContainer NOTIFY themeChanged)
    Q_PROPERTY(QColor onPrimaryContainer READ onPrimaryContainer NOTIFY themeChanged)

    Q_PROPERTY(QColor secondary READ secondary NOTIFY themeChanged)
    Q_PROPERTY(QColor onSecondary READ onSecondary NOTIFY themeChanged)
    Q_PROPERTY(QColor secondaryContainer READ secondaryContainer NOTIFY themeChanged)
    Q_PROPERTY(QColor onSecondaryContainer READ onSecondaryContainer NOTIFY themeChanged)

    Q_PROPERTY(QColor tertiary READ tertiary NOTIFY themeChanged)
    Q_PROPERTY(QColor onTertiary READ onTertiary NOTIFY themeChanged)
    Q_PROPERTY(QColor tertiaryContainer READ tertiaryContainer NOTIFY themeChanged)
    Q_PROPERTY(QColor onTertiaryContainer READ onTertiaryContainer NOTIFY themeChanged)

    Q_PROPERTY(QColor error READ error NOTIFY themeChanged)
    Q_PROPERTY(QColor onError READ onError NOTIFY themeChanged)
    Q_PROPERTY(QColor errorContainer READ errorContainer NOTIFY themeChanged)
    Q_PROPERTY(QColor onErrorContainer READ onErrorContainer NOTIFY themeChanged)

    // Surface Container Tiers
    Q_PROPERTY(QColor surface READ surface NOTIFY themeChanged)
    Q_PROPERTY(QColor surfaceDim READ surfaceDim NOTIFY themeChanged)
    Q_PROPERTY(QColor surfaceBright READ surfaceBright NOTIFY themeChanged)
    Q_PROPERTY(QColor surfaceContainerLowest READ surfaceContainerLowest NOTIFY themeChanged)
    Q_PROPERTY(QColor surfaceContainerLow READ surfaceContainerLow NOTIFY themeChanged)
    Q_PROPERTY(QColor surfaceContainer READ surfaceContainer NOTIFY themeChanged)
    Q_PROPERTY(QColor surfaceContainerHigh READ surfaceContainerHigh NOTIFY themeChanged)
    Q_PROPERTY(QColor surfaceContainerHighest READ surfaceContainerHighest NOTIFY themeChanged)

    Q_PROPERTY(QColor onSurface READ onSurface NOTIFY themeChanged)
    Q_PROPERTY(QColor onSurfaceVariant READ onSurfaceVariant NOTIFY themeChanged)
    Q_PROPERTY(QColor outline READ outline NOTIFY themeChanged)
    Q_PROPERTY(QColor outlineVariant READ outlineVariant NOTIFY themeChanged)

    // Shapes
    Q_PROPERTY(qreal cornerXS READ cornerXS CONSTANT)
    Q_PROPERTY(qreal cornerS READ cornerS CONSTANT)
    Q_PROPERTY(qreal cornerM READ cornerM CONSTANT)
    Q_PROPERTY(qreal cornerL READ cornerL CONSTANT)
    Q_PROPERTY(qreal cornerXL READ cornerXL CONSTANT)
    Q_PROPERTY(qreal cornerFull READ cornerFull CONSTANT)

    // Typography
    Q_PROPERTY(QFont headlineSmall READ headlineSmall CONSTANT)
    Q_PROPERTY(QFont titleLarge READ titleLarge CONSTANT)
    Q_PROPERTY(QFont titleMedium READ titleMedium CONSTANT)
    Q_PROPERTY(QFont titleSmall READ titleSmall CONSTANT)
    Q_PROPERTY(QFont bodyLarge READ bodyLarge CONSTANT)
    Q_PROPERTY(QFont bodyMedium READ bodyMedium CONSTANT)
    Q_PROPERTY(QFont bodySmall READ bodySmall CONSTANT)
    Q_PROPERTY(QFont labelLarge READ labelLarge CONSTANT)
    Q_PROPERTY(QFont labelMedium READ labelMedium CONSTANT)
    Q_PROPERTY(QFont labelSmall READ labelSmall CONSTANT)

public:
    explicit MD3Theme(QObject* parent = nullptr);

    static MD3Theme* create(QQmlEngine*, QJSEngine*) {
        return instance();
    }

    static MD3Theme* instance();

    int themeMode() const { return themeMode_; }
    void setThemeMode(int mode);
    bool isDark() const;

    QString accentPreset() const { return accentPreset_; }
    void setAccentPreset(const QString& preset);

    // Colors
    QColor primary() const;
    QColor onPrimary() const;
    QColor primaryContainer() const;
    QColor onPrimaryContainer() const;

    QColor secondary() const;
    QColor onSecondary() const;
    QColor secondaryContainer() const;
    QColor onSecondaryContainer() const;

    QColor tertiary() const;
    QColor onTertiary() const;
    QColor tertiaryContainer() const;
    QColor onTertiaryContainer() const;

    QColor error() const;
    QColor onError() const;
    QColor errorContainer() const;
    QColor onErrorContainer() const;

    QColor surface() const;
    QColor surfaceDim() const;
    QColor surfaceBright() const;
    QColor surfaceContainerLowest() const;
    QColor surfaceContainerLow() const;
    QColor surfaceContainer() const;
    QColor surfaceContainerHigh() const;
    QColor surfaceContainerHighest() const;

    QColor onSurface() const;
    QColor onSurfaceVariant() const;
    QColor outline() const;
    QColor outlineVariant() const;

    // Shapes
    qreal cornerXS() const { return 4.0; }
    qreal cornerS() const { return 8.0; }
    qreal cornerM() const { return 12.0; }
    qreal cornerL() const { return 16.0; }
    qreal cornerXL() const { return 28.0; }
    qreal cornerFull() const { return 9999.0; }

    // Typography
    QFont headlineSmall() const;
    QFont titleLarge() const;
    QFont titleMedium() const;
    QFont titleSmall() const;
    QFont bodyLarge() const;
    QFont bodyMedium() const;
    QFont bodySmall() const;
    QFont labelLarge() const;
    QFont labelMedium() const;
    QFont labelSmall() const;

    Q_INVOKABLE QColor colorWithAlpha(const QColor& c, qreal alpha) const {
        return QColor::fromRgbF(c.redF(), c.greenF(), c.blueF(), qBound(0.0, alpha, 1.0));
    }

signals:
    void themeModeChanged();
    void isDarkChanged();
    void accentPresetChanged();
    void themeChanged();

private:
    int themeMode_ = 0; // 0: System, 1: Light, 2: Dark
    QString accentPreset_ = "purple"; // purple, blue, teal, green, orange, red, pink

    QColor activeSeedColor() const;
};

} // namespace webclip
