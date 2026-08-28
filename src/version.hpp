#pragma once

#include <string_view>

namespace webclip {

inline constexpr int VERSION_MAJOR = 1;
inline constexpr int VERSION_MINOR = 4;
inline constexpr int VERSION_PATCH = 0;

inline constexpr std::string_view VERSION_STRING = "1.4.0";
inline constexpr std::string_view APP_NAME = "WebClip";
inline constexpr std::string_view APP_DISPLAY_NAME = "WebClip Sync";
inline constexpr std::string_view APP_ORGANIZATION = "Burhanverse";
inline constexpr std::string_view APP_DOMAIN = "burhanverse.eu.org";
inline constexpr std::string_view APP_DESCRIPTION = "Real-Time Gboard Clipboard Sync";
inline constexpr std::string_view APP_AUTHOR = "Burhanverse";
inline constexpr std::string_view APP_URL = "https://github.com/Burhanverse/webclip";

}
