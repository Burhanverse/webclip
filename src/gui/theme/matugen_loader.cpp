#include "matugen_loader.hpp"

#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>

namespace webclip {

namespace {

QColor parseColorValue(const QJsonValue& val) {
    if (val.isString()) {
        const QString str = val.toString().trimmed();
        if (str.startsWith(QLatin1Char('#')) || QColor::isValidColorName(str)) {
            QColor col(str);
            if (col.isValid()) return col;
        }
    } else if (val.isObject()) {
        const QJsonObject obj = val.toObject();
        if (obj.contains(QLatin1String("color"))) {
            return parseColorValue(obj.value(QLatin1String("color")));
        }
        if (obj.contains(QLatin1String("hex"))) {
            return parseColorValue(obj.value(QLatin1String("hex")));
        }
        if (obj.contains(QLatin1String("value"))) {
            return parseColorValue(obj.value(QLatin1String("value")));
        }
    }
    return QColor();
}

QColor contrastTextColor(const QColor& bg) {
    const qreal lum = 0.2126 * bg.redF() + 0.7152 * bg.greenF() + 0.0722 * bg.blueF();
    return lum > 0.5 ? QColor(QStringLiteral("#1B1B1F")) : QColor(QStringLiteral("#FFFFFF"));
}

void assignRoleColor(MD3Palette& pal, const QString& roleName, const QColor& color) {
    if (!color.isValid()) return;

    const QString lower = roleName.toLower();

    if (lower == QLatin1String("primary")) pal.primary = color;
    else if (lower == QLatin1String("on_primary") || lower == QLatin1String("onprimary")) pal.onPrimary = color;
    else if (lower == QLatin1String("primary_container") || lower == QLatin1String("primarycontainer")) pal.primaryContainer = color;
    else if (lower == QLatin1String("on_primary_container") || lower == QLatin1String("onprimarycontainer")) pal.onPrimaryContainer = color;

    else if (lower == QLatin1String("secondary")) pal.secondary = color;
    else if (lower == QLatin1String("on_secondary") || lower == QLatin1String("onsecondary")) pal.onSecondary = color;
    else if (lower == QLatin1String("secondary_container") || lower == QLatin1String("secondarycontainer")) pal.secondaryContainer = color;
    else if (lower == QLatin1String("on_secondary_container") || lower == QLatin1String("onsecondarycontainer")) pal.onSecondaryContainer = color;

    else if (lower == QLatin1String("tertiary")) pal.tertiary = color;
    else if (lower == QLatin1String("on_tertiary") || lower == QLatin1String("ontertiary")) pal.onTertiary = color;
    else if (lower == QLatin1String("tertiary_container") || lower == QLatin1String("tertiarycontainer")) pal.tertiaryContainer = color;
    else if (lower == QLatin1String("on_tertiary_container") || lower == QLatin1String("ontertiarycontainer")) pal.onTertiaryContainer = color;

    else if (lower == QLatin1String("error")) pal.error = color;
    else if (lower == QLatin1String("on_error") || lower == QLatin1String("onerror")) pal.onError = color;
    else if (lower == QLatin1String("error_container") || lower == QLatin1String("errorcontainer")) pal.errorContainer = color;
    else if (lower == QLatin1String("on_error_container") || lower == QLatin1String("onerrorcontainer")) pal.onErrorContainer = color;

    else if (lower == QLatin1String("surface")) pal.surface = color;
    else if (lower == QLatin1String("surface_dim") || lower == QLatin1String("surfacedim")) pal.surfaceDim = color;
    else if (lower == QLatin1String("surface_bright") || lower == QLatin1String("surfacebright")) pal.surfaceBright = color;
    else if (lower == QLatin1String("surface_container_lowest") || lower == QLatin1String("surfacecontainerlowest")) pal.surfaceContainerLowest = color;
    else if (lower == QLatin1String("surface_container_low") || lower == QLatin1String("surfacecontainerlow")) pal.surfaceContainerLow = color;
    else if (lower == QLatin1String("surface_container") || lower == QLatin1String("surfacecontainer")) pal.surfaceContainer = color;
    else if (lower == QLatin1String("surface_container_high") || lower == QLatin1String("surfacecontainerhigh")) pal.surfaceContainerHigh = color;
    else if (lower == QLatin1String("surface_container_highest") || lower == QLatin1String("surfacecontainerhighest")) pal.surfaceContainerHighest = color;

    else if (lower == QLatin1String("on_surface") || lower == QLatin1String("onsurface")) pal.onSurface = color;
    else if (lower == QLatin1String("on_surface_variant") || lower == QLatin1String("onsurfacevariant")) pal.onSurfaceVariant = color;
    else if (lower == QLatin1String("outline")) pal.outline = color;
    else if (lower == QLatin1String("outline_variant") || lower == QLatin1String("outlinevariant")) pal.outlineVariant = color;
}

void parsePaletteFromMap(const QJsonObject& map, MD3Palette& pal) {
    for (auto it = map.begin(); it != map.end(); ++it) {
        const QColor col = parseColorValue(it.value());
        if (col.isValid()) {
            assignRoleColor(pal, it.key(), col);
        }
    }
}

}

void MatugenLoader::completeFallbacks(MD3Palette& pal, bool isDark) {
    if (!pal.primary.isValid()) {
        pal.primary = isDark ? QColor(QStringLiteral("#D0BCFF")) : QColor(QStringLiteral("#6750A4"));
    }

    if (!pal.onPrimary.isValid()) {
        pal.onPrimary = contrastTextColor(pal.primary);
    }

    if (!pal.primaryContainer.isValid()) {
        pal.primaryContainer = isDark ? pal.primary.darker(200) : pal.primary.lighter(170);
    }

    if (!pal.onPrimaryContainer.isValid()) {
        pal.onPrimaryContainer = contrastTextColor(pal.primaryContainer);
    }

    if (!pal.secondary.isValid()) {
        pal.secondary = isDark ? QColor(QStringLiteral("#CCC2DC")) : QColor(QStringLiteral("#625B71"));
    }

    if (!pal.onSecondary.isValid()) {
        pal.onSecondary = contrastTextColor(pal.secondary);
    }

    if (!pal.secondaryContainer.isValid()) {
        pal.secondaryContainer = isDark ? pal.secondary.darker(180) : pal.secondary.lighter(160);
    }

    if (!pal.onSecondaryContainer.isValid()) {
        pal.onSecondaryContainer = contrastTextColor(pal.secondaryContainer);
    }

    if (!pal.tertiary.isValid()) {
        pal.tertiary = isDark ? QColor(QStringLiteral("#EFB8C8")) : QColor(QStringLiteral("#7D5260"));
    }

    if (!pal.onTertiary.isValid()) {
        pal.onTertiary = contrastTextColor(pal.tertiary);
    }

    if (!pal.tertiaryContainer.isValid()) {
        pal.tertiaryContainer = isDark ? pal.tertiary.darker(180) : pal.tertiary.lighter(160);
    }

    if (!pal.onTertiaryContainer.isValid()) {
        pal.onTertiaryContainer = contrastTextColor(pal.tertiaryContainer);
    }

    if (!pal.error.isValid()) {
        pal.error = isDark ? QColor(QStringLiteral("#FFB4AB")) : QColor(QStringLiteral("#BA1A1A"));
    }

    if (!pal.onError.isValid()) {
        pal.onError = isDark ? QColor(QStringLiteral("#690005")) : QColor(QStringLiteral("#FFFFFF"));
    }

    if (!pal.errorContainer.isValid()) {
        pal.errorContainer = isDark ? QColor(QStringLiteral("#93000A")) : QColor(QStringLiteral("#FFDAD6"));
    }

    if (!pal.onErrorContainer.isValid()) {
        pal.onErrorContainer = isDark ? QColor(QStringLiteral("#FFDAD6")) : QColor(QStringLiteral("#410002"));
    }

    if (!pal.surface.isValid()) {
        pal.surface = isDark ? QColor(QStringLiteral("#141218")) : QColor(QStringLiteral("#FEF7FF"));
    }

    if (!pal.surfaceDim.isValid()) {
        pal.surfaceDim = isDark ? QColor(QStringLiteral("#141218")) : QColor(QStringLiteral("#DED8E1"));
    }

    if (!pal.surfaceBright.isValid()) {
        pal.surfaceBright = isDark ? QColor(QStringLiteral("#3B383E")) : QColor(QStringLiteral("#FEF7FF"));
    }

    if (!pal.surfaceContainerLowest.isValid()) {
        pal.surfaceContainerLowest = isDark ? QColor(QStringLiteral("#0F0D13")) : QColor(QStringLiteral("#FFFFFF"));
    }

    if (!pal.surfaceContainerLow.isValid()) {
        pal.surfaceContainerLow = isDark ? QColor(QStringLiteral("#1D1B20")) : QColor(QStringLiteral("#F7F2FA"));
    }

    if (!pal.surfaceContainer.isValid()) {
        pal.surfaceContainer = isDark ? QColor(QStringLiteral("#211F26")) : QColor(QStringLiteral("#F3EDF7"));
    }

    if (!pal.surfaceContainerHigh.isValid()) {
        pal.surfaceContainerHigh = isDark ? QColor(QStringLiteral("#2B2930")) : QColor(QStringLiteral("#ECE6F0"));
    }

    if (!pal.surfaceContainerHighest.isValid()) {
        pal.surfaceContainerHighest = isDark ? QColor(QStringLiteral("#36343B")) : QColor(QStringLiteral("#E6E0E9"));
    }

    if (!pal.onSurface.isValid()) {
        pal.onSurface = isDark ? QColor(QStringLiteral("#E6E0E9")) : QColor(QStringLiteral("#1D1B20"));
    }

    if (!pal.onSurfaceVariant.isValid()) {
        pal.onSurfaceVariant = isDark ? QColor(QStringLiteral("#CAC4D0")) : QColor(QStringLiteral("#49454F"));
    }

    if (!pal.outline.isValid()) {
        pal.outline = isDark ? QColor(QStringLiteral("#938F99")) : QColor(QStringLiteral("#79747E"));
    }

    if (!pal.outlineVariant.isValid()) {
        pal.outlineVariant = isDark ? QColor(QStringLiteral("#49454F")) : QColor(QStringLiteral("#CAC4D0"));
    }
}

MatugenParseStatus MatugenLoader::parse(const QByteArray& jsonData, MatugenThemes& outThemes) {
    if (jsonData.trimmed().isEmpty()) {
        return MatugenParseStatus::EmptyData;
    }

    QJsonParseError parseError;
    const QJsonDocument doc = QJsonDocument::fromJson(jsonData, &parseError);
    if (parseError.error != QJsonParseError::NoError || !doc.isObject()) {
        return MatugenParseStatus::InvalidJson;
    }

    const QJsonObject root = doc.object();
    outThemes = MatugenThemes();

    if (root.contains(QLatin1String("light")) && root.value(QLatin1String("light")).isObject()) {
        parsePaletteFromMap(root.value(QLatin1String("light")).toObject(), outThemes.light);
        outThemes.hasLight = outThemes.light.primary.isValid() || outThemes.light.surface.isValid();
    }

    if (root.contains(QLatin1String("dark")) && root.value(QLatin1String("dark")).isObject()) {
        parsePaletteFromMap(root.value(QLatin1String("dark")).toObject(), outThemes.dark);
        outThemes.hasDark = outThemes.dark.primary.isValid() || outThemes.dark.surface.isValid();
    }

    if (root.contains(QLatin1String("amoled")) && root.value(QLatin1String("amoled")).isObject()) {
        parsePaletteFromMap(root.value(QLatin1String("amoled")).toObject(), outThemes.amoled);
        outThemes.hasAmoled = outThemes.amoled.primary.isValid() || outThemes.amoled.surface.isValid();
    }

    if (root.contains(QLatin1String("colors")) && root.value(QLatin1String("colors")).isObject()) {
        const QJsonObject colorsObj = root.value(QLatin1String("colors")).toObject();

        if (colorsObj.contains(QLatin1String("dark")) && colorsObj.value(QLatin1String("dark")).isObject()) {
            parsePaletteFromMap(colorsObj.value(QLatin1String("dark")).toObject(), outThemes.dark);
            outThemes.hasDark = outThemes.dark.primary.isValid() || outThemes.dark.surface.isValid();
        }
        if (colorsObj.contains(QLatin1String("light")) && colorsObj.value(QLatin1String("light")).isObject()) {
            parsePaletteFromMap(colorsObj.value(QLatin1String("light")).toObject(), outThemes.light);
            outThemes.hasLight = outThemes.light.primary.isValid() || outThemes.light.surface.isValid();
        }
        if (colorsObj.contains(QLatin1String("amoled")) && colorsObj.value(QLatin1String("amoled")).isObject()) {
            parsePaletteFromMap(colorsObj.value(QLatin1String("amoled")).toObject(), outThemes.amoled);
            outThemes.hasAmoled = outThemes.amoled.primary.isValid() || outThemes.amoled.surface.isValid();
        }

        if (!outThemes.hasLight && !outThemes.hasDark) {
            for (auto it = colorsObj.begin(); it != colorsObj.end(); ++it) {
                const QString role = it.key();
                if (it.value().isObject()) {
                    const QJsonObject roleObj = it.value().toObject();
                    if (roleObj.contains(QLatin1String("light"))) {
                        const QColor col = parseColorValue(roleObj.value(QLatin1String("light")));
                        if (col.isValid()) {
                            assignRoleColor(outThemes.light, role, col);
                            outThemes.hasLight = true;
                        }
                    }
                    if (roleObj.contains(QLatin1String("dark"))) {
                        const QColor col = parseColorValue(roleObj.value(QLatin1String("dark")));
                        if (col.isValid()) {
                            assignRoleColor(outThemes.dark, role, col);
                            outThemes.hasDark = true;
                        }
                    }
                    if (roleObj.contains(QLatin1String("amoled"))) {
                        const QColor col = parseColorValue(roleObj.value(QLatin1String("amoled")));
                        if (col.isValid()) {
                            assignRoleColor(outThemes.amoled, role, col);
                            outThemes.hasAmoled = true;
                        }
                    }
                    if (!roleObj.contains(QLatin1String("light")) && !roleObj.contains(QLatin1String("dark"))) {
                        if (roleObj.contains(QLatin1String("default"))) {
                            const QColor col = parseColorValue(roleObj.value(QLatin1String("default")));
                            if (col.isValid()) {
                                assignRoleColor(outThemes.dark, role, col);
                                assignRoleColor(outThemes.light, role, col);
                                outThemes.hasDark = true;
                                outThemes.hasLight = true;
                            }
                        }
                    }
                } else if (it.value().isString()) {
                    const QColor col = parseColorValue(it.value());
                    if (col.isValid()) {
                        assignRoleColor(outThemes.dark, role, col);
                        assignRoleColor(outThemes.light, role, col);
                        outThemes.hasDark = true;
                        outThemes.hasLight = true;
                    }
                }
            }
        }
    }

    if (!outThemes.hasLight && !outThemes.hasDark) {
        MD3Palette flatPal;
        parsePaletteFromMap(root, flatPal);
        if (flatPal.primary.isValid() || flatPal.surface.isValid()) {
            const bool looksDark = flatPal.surface.isValid() ? (flatPal.surface.value() < 128) : true;
            if (looksDark) {
                outThemes.dark = flatPal;
                outThemes.hasDark = true;
            } else {
                outThemes.light = flatPal;
                outThemes.hasLight = true;
            }
        }
    }

    if (!outThemes.hasLight && outThemes.hasDark) {
        outThemes.light = outThemes.dark;
        completeFallbacks(outThemes.light, false);
        outThemes.hasLight = true;
    } else if (outThemes.hasLight && !outThemes.hasDark) {
        outThemes.dark = outThemes.light;
        completeFallbacks(outThemes.dark, true);
        outThemes.hasDark = true;
    }

    if (!outThemes.hasLight && !outThemes.hasDark) {
        return MatugenParseStatus::MissingColors;
    }

    completeFallbacks(outThemes.light, false);
    completeFallbacks(outThemes.dark, true);

    if (!outThemes.hasAmoled) {
        outThemes.amoled = outThemes.dark;
        outThemes.amoled.surface = QColor(QStringLiteral("#000000"));
        outThemes.amoled.surfaceContainerLowest = QColor(QStringLiteral("#000000"));
        outThemes.amoled.surfaceContainerLow = QColor(QStringLiteral("#0A0A0A"));
        outThemes.amoled.surfaceContainer = QColor(QStringLiteral("#121212"));
        outThemes.amoled.surfaceContainerHigh = QColor(QStringLiteral("#181818"));
        outThemes.amoled.surfaceContainerHighest = QColor(QStringLiteral("#222222"));
        outThemes.hasAmoled = true;
    } else {
        completeFallbacks(outThemes.amoled, true);
        outThemes.amoled.surface = QColor(QStringLiteral("#000000"));
        outThemes.amoled.surfaceContainerLowest = QColor(QStringLiteral("#000000"));
    }

    return MatugenParseStatus::Success;
}

MatugenParseStatus MatugenLoader::loadFromFile(const QString& filePath, MatugenThemes& outThemes) {
    if (filePath.isEmpty()) {
        return MatugenParseStatus::EmptyData;
    }

    QFile file(filePath);
    if (!file.exists() || !file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return MatugenParseStatus::EmptyData;
    }

    const QByteArray data = file.readAll();
    file.close();

    return parse(data, outThemes);
}

}

