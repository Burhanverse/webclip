#pragma once

#include "style_core_font.hpp"
#include <QFont>
#include <algorithm>
#include <cmath>

namespace webclip::scale {

inline double& globalScaleRef() {
    static double s_scale = 1.0;
    return s_scale;
}

inline void set(double scale) {
    if (scale <= 0.0) {
        scale = font::detectSystemFontScale();
    }
    scale = (std::clamp)(scale, 0.75, 2.50);
    globalScaleRef() = scale;
}

[[nodiscard]] inline double current() {
    return globalScaleRef();
}

[[nodiscard]] inline int px(int basePx) {
    return (std::max)(1, qRound(basePx * globalScaleRef()));
}

[[nodiscard]] inline qreal pxF(qreal basePx) {
    return basePx * globalScaleRef();
}

[[nodiscard]] inline QFont font(int basePx, QFont::Weight w = QFont::Normal, bool italic = false, bool mono = false) {
    return font::createFont(px(basePx), w, italic, mono);
}

} // namespace webclip::scale
