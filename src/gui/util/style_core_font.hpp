#pragma once

#include <QString>
#include <QFont>
#include <QStringList>

namespace webclip::font {

void initFonts();

[[nodiscard]] QString monospaceFontFamily();
[[nodiscard]] double detectSystemFontScale(QString* outDetails = nullptr);

[[nodiscard]] QFont createFont(int pixelSize, QFont::Weight weight = QFont::Normal, bool italic = false, bool monospace = false);

}
