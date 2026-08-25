#pragma once

#include <QString>
#include <QFont>
#include <QStringList>

namespace webclip::font {

void initFonts();

[[nodiscard]] QString monospaceFontFamily();

[[nodiscard]] QFont createFont(int pixelSize, QFont::Weight weight = QFont::Normal, bool italic = false, bool monospace = false);

} // namespace webclip::font
