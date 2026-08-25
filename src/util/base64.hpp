#pragma once

#include <string>
#include <vector>
#include <cstdint>
#include <string_view>

namespace webclip {

namespace base64 {

inline std::string encode(const uint8_t* data, size_t len) {
    static const char charset[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string result;
    result.reserve(((len + 2) / 3) * 4);

    for (size_t i = 0; i < len; i += 3) {
        uint32_t b = (static_cast<uint32_t>(data[i]) << 16);
        if (i + 1 < len) b |= (static_cast<uint32_t>(data[i + 1]) << 8);
        if (i + 2 < len) b |= static_cast<uint32_t>(data[i + 2]);

        result.push_back(charset[(b >> 18) & 0x3F]);
        result.push_back(charset[(b >> 12) & 0x3F]);
        if (i + 1 < len) {
            result.push_back(charset[(b >> 6) & 0x3F]);
        } else {
            result.push_back('=');
        }
        if (i + 2 < len) {
            result.push_back(charset[b & 0x3F]);
        } else {
            result.push_back('=');
        }
    }
    return result;
}

inline std::string encode(const std::vector<uint8_t>& bytes) {
    return encode(bytes.data(), bytes.size());
}

inline std::string encode(std::string_view str) {
    return encode(reinterpret_cast<const uint8_t*>(str.data()), str.size());
}

inline std::vector<uint8_t> decode(std::string_view input) {
    std::vector<uint8_t> result;
    if (input.empty()) return result;

    std::vector<int> T(256, -1);
    for (int i = 0; i < 64; ++i) {
        T[static_cast<unsigned char>("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/"[i])] = i;
    }

    int val = 0, valb = -8;
    for (unsigned char c : input) {
        if (c == '=') break;
        if (c == '\r' || c == '\n' || c == ' ' || c == '\t') continue;
        if (T[c] == -1) continue;
        val = (val << 6) + T[c];
        valb += 6;
        if (valb >= 0) {
            result.push_back(static_cast<uint8_t>((val >> valb) & 0xFF));
            valb -= 8;
        }
    }
    return result;
}

}

}
