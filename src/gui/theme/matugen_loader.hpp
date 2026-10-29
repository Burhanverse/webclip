#pragma once

#include <QColor>
#include <QString>
#include <QByteArray>

namespace webclip {

struct MD3Palette {
    QColor primary;
    QColor onPrimary;
    QColor primaryContainer;
    QColor onPrimaryContainer;

    QColor secondary;
    QColor onSecondary;
    QColor secondaryContainer;
    QColor onSecondaryContainer;

    QColor tertiary;
    QColor onTertiary;
    QColor tertiaryContainer;
    QColor onTertiaryContainer;

    QColor error;
    QColor onError;
    QColor errorContainer;
    QColor onErrorContainer;

    QColor surface;
    QColor surfaceDim;
    QColor surfaceBright;
    QColor surfaceContainerLowest;
    QColor surfaceContainerLow;
    QColor surfaceContainer;
    QColor surfaceContainerHigh;
    QColor surfaceContainerHighest;

    QColor onSurface;
    QColor onSurfaceVariant;
    QColor outline;
    QColor outlineVariant;

    bool isValid() const {
        return primary.isValid() && surface.isValid();
    }
};

struct MatugenThemes {
    MD3Palette light;
    MD3Palette dark;
    MD3Palette amoled;
    bool hasLight = false;
    bool hasDark = false;
    bool hasAmoled = false;

    bool isValid() const {
        return (hasLight && light.isValid()) || (hasDark && dark.isValid()) || (hasAmoled && amoled.isValid());
    }
};

enum class MatugenParseStatus {
    Success,
    EmptyData,
    InvalidJson,
    MissingColors
};

class MatugenLoader {
public:
    static MatugenParseStatus parse(const QByteArray& jsonData, MatugenThemes& outThemes);
    static MatugenParseStatus loadFromFile(const QString& filePath, MatugenThemes& outThemes);
    static void completeFallbacks(MD3Palette& palette, bool isDark);
};

}

