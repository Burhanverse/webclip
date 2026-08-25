#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <sstream>
#include <stdexcept>
#include <cctype>
#include <cstdint>

namespace webclip {

class JsonValue {
public:
    enum class Type { Null, Boolean, Number, String, Object, Array };

    Type type = Type::Null;
    bool bool_val = false;
    double num_val = 0.0;
    std::string str_val;
    std::unordered_map<std::string, JsonValue> obj_val;
    std::vector<JsonValue> arr_val;

    JsonValue() = default;
    explicit JsonValue(bool v) : type(Type::Boolean), bool_val(v) {}
    explicit JsonValue(double v) : type(Type::Number), num_val(v) {}
    explicit JsonValue(int v) : type(Type::Number), num_val(v) {}
    explicit JsonValue(int64_t v) : type(Type::Number), num_val(static_cast<double>(v)) {}
    explicit JsonValue(const std::string& v) : type(Type::String), str_val(v) {}
    explicit JsonValue(const char* v) : type(Type::String), str_val(v ? v : "") {}

    static JsonValue object() {
        JsonValue v;
        v.type = Type::Object;
        return v;
    }

    static JsonValue array() {
        JsonValue v;
        v.type = Type::Array;
        return v;
    }

    void set(const std::string& key, JsonValue val) {
        if (type != Type::Object) {
            type = Type::Object;
            obj_val.clear();
        }
        obj_val[key] = std::move(val);
    }

    bool has(const std::string& key) const {
        if (type != Type::Object) return false;
        return obj_val.find(key) != obj_val.end();
    }

    std::string get_string(const std::string& key, const std::string& def = "") const {
        if (type != Type::Object) return def;
        auto it = obj_val.find(key);
        if (it != obj_val.end() && it->second.type == Type::String) {
            return it->second.str_val;
        }
        return def;
    }

    int64_t get_int64(const std::string& key, int64_t def = 0) const {
        if (type != Type::Object) return def;
        auto it = obj_val.find(key);
        if (it != obj_val.end() && it->second.type == Type::Number) {
            return static_cast<int64_t>(it->second.num_val);
        }
        return def;
    }

    bool get_bool(const std::string& key, bool def = false) const {
        if (type != Type::Object) return def;
        auto it = obj_val.find(key);
        if (it != obj_val.end() && it->second.type == Type::Boolean) {
            return it->second.bool_val;
        }
        return def;
    }

    static std::string escape_string(const std::string& s) {
        std::string out;
        out.reserve(s.size() + 8);
        for (char c : s) {
            switch (c) {
                case '"':  out += "\\\""; break;
                case '\\': out += "\\\\"; break;
                case '\b': out += "\\b"; break;
                case '\f': out += "\\f"; break;
                case '\n': out += "\\n"; break;
                case '\r': out += "\\r"; break;
                case '\t': out += "\\t"; break;
                default:
                    if (static_cast<unsigned char>(c) < 0x20) {
                        char buf[8];
                        snprintf(buf, sizeof(buf), "\\u%04x", static_cast<unsigned char>(c));
                        out += buf;
                    } else {
                        out += c;
                    }
                    break;
            }
        }
        return out;
    }

    std::string serialize() const {
        switch (type) {
            case Type::Null: return "null";
            case Type::Boolean: return bool_val ? "true" : "false";
            case Type::Number: {
                if (num_val == static_cast<int64_t>(num_val)) {
                    return std::to_string(static_cast<int64_t>(num_val));
                }
                std::ostringstream ss;
                ss << num_val;
                return ss.str();
            }
            case Type::String: return "\"" + escape_string(str_val) + "\"";
            case Type::Object: {
                std::string res = "{";
                bool first = true;
                for (const auto& [k, v] : obj_val) {
                    if (!first) res += ",";
                    first = false;
                    res += "\"" + escape_string(k) + "\":" + v.serialize();
                }
                res += "}";
                return res;
            }
            case Type::Array: {
                std::string res = "[";
                bool first = true;
                for (const auto& item : arr_val) {
                    if (!first) res += ",";
                    first = false;
                    res += item.serialize();
                }
                res += "]";
                return res;
            }
        }
        return "null";
    }

    static JsonValue parse(const std::string& str) {
        size_t idx = 0;
        skip_ws(str, idx);
        return parse_value(str, idx);
    }

private:
    static void skip_ws(const std::string& s, size_t& idx) {
        while (idx < s.size() && (s[idx] == ' ' || s[idx] == '\t' || s[idx] == '\r' || s[idx] == '\n')) {
            idx++;
        }
    }

    static JsonValue parse_value(const std::string& s, size_t& idx) {
        skip_ws(s, idx);
        if (idx >= s.size()) return JsonValue();

        if (s[idx] == '{') return parse_object(s, idx);
        if (s[idx] == '[') return parse_array(s, idx);
        if (s[idx] == '"') return parse_string_val(s, idx);
        if (s[idx] == 't' || s[idx] == 'f') return parse_bool(s, idx);
        if (s[idx] == 'n') return parse_null(s, idx);
        if (s[idx] == '-' || (s[idx] >= '0' && s[idx] <= '9')) return parse_number(s, idx);

        return JsonValue();
    }

    static JsonValue parse_object(const std::string& s, size_t& idx) {
        idx++;
        JsonValue val = JsonValue::object();
        skip_ws(s, idx);
        if (idx < s.size() && s[idx] == '}') {
            idx++;
            return val;
        }

        while (idx < s.size()) {
            skip_ws(s, idx);
            if (idx >= s.size() || s[idx] != '"') break;
            std::string key = parse_raw_string(s, idx);
            skip_ws(s, idx);
            if (idx >= s.size() || s[idx] != ':') break;
            idx++;
            skip_ws(s, idx);
            JsonValue child = parse_value(s, idx);
            val.obj_val[key] = std::move(child);

            skip_ws(s, idx);
            if (idx < s.size() && s[idx] == ',') {
                idx++;
            } else if (idx < s.size() && s[idx] == '}') {
                idx++;
                return val;
            } else {
                break;
            }
        }
        return val;
    }

    static JsonValue parse_array(const std::string& s, size_t& idx) {
        idx++;
        JsonValue val = JsonValue::array();
        skip_ws(s, idx);
        if (idx < s.size() && s[idx] == ']') {
            idx++;
            return val;
        }

        while (idx < s.size()) {
            skip_ws(s, idx);
            JsonValue child = parse_value(s, idx);
            val.arr_val.push_back(std::move(child));

            skip_ws(s, idx);
            if (idx < s.size() && s[idx] == ',') {
                idx++;
            } else if (idx < s.size() && s[idx] == ']') {
                idx++;
                return val;
            } else {
                break;
            }
        }
        return val;
    }

    static std::string parse_raw_string(const std::string& s, size_t& idx) {
        idx++;
        std::string res;
        while (idx < s.size()) {
            char c = s[idx++];
            if (c == '"') {
                return res;
            }
            if (c == '\\' && idx < s.size()) {
                char esc = s[idx++];
                switch (esc) {
                    case '"': res += '"'; break;
                    case '\\': res += '\\'; break;
                    case '/': res += '/'; break;
                    case 'b': res += '\b'; break;
                    case 'f': res += '\f'; break;
                    case 'n': res += '\n'; break;
                    case 'r': res += '\r'; break;
                    case 't': res += '\t'; break;
                    case 'u': {
                        if (idx + 4 <= s.size()) {
                            std::string hex_str = s.substr(idx, 4);
                            idx += 4;
                            try {
                                unsigned int codepoint = std::stoul(hex_str, nullptr, 16);

                                if (codepoint >= 0xD800 && codepoint <= 0xDBFF &&
                                    idx + 6 <= s.size() && s[idx] == '\\' && s[idx + 1] == 'u') {
                                    unsigned int low = std::stoul(s.substr(idx + 2, 4), nullptr, 16);
                                    if (low >= 0xDC00 && low <= 0xDFFF) {
                                        codepoint = 0x10000 + ((codepoint - 0xD800) << 10) + (low - 0xDC00);
                                        idx += 6;
                                    }
                                }
                                if (codepoint < 0x80) {
                                    res += static_cast<char>(codepoint);
                                } else if (codepoint < 0x800) {
                                    res += static_cast<char>(0xC0 | ((codepoint >> 6) & 0x1F));
                                    res += static_cast<char>(0x80 | ((codepoint >> 0) & 0x3F));
                                } else if (codepoint <= 0xFFFF) {
                                    res += static_cast<char>(0xE0 | ((codepoint >> 12) & 0x0F));
                                    res += static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F));
                                    res += static_cast<char>(0x80 | ((codepoint >> 0) & 0x3F));
                                } else if (codepoint <= 0x10FFFF) {
                                    res += static_cast<char>(0xF0 | ((codepoint >> 18) & 0x07));
                                    res += static_cast<char>(0x80 | ((codepoint >> 12) & 0x3F));
                                    res += static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F));
                                    res += static_cast<char>(0x80 | ((codepoint >> 0) & 0x3F));
                                }
                            } catch (...) {}
                        }
                        break;
                    }
                    default: res += esc; break;
                }
            } else {
                res += c;
            }
        }
        return res;
    }

    static JsonValue parse_string_val(const std::string& s, size_t& idx) {
        return JsonValue(parse_raw_string(s, idx));
    }

    static JsonValue parse_bool(const std::string& s, size_t& idx) {
        if (s.substr(idx, 4) == "true") {
            idx += 4;
            return JsonValue(true);
        }
        if (s.substr(idx, 5) == "false") {
            idx += 5;
            return JsonValue(false);
        }
        return JsonValue();
    }

    static JsonValue parse_null(const std::string& s, size_t& idx) {
        if (s.substr(idx, 4) == "null") {
            idx += 4;
        }
        return JsonValue();
    }

    static JsonValue parse_number(const std::string& s, size_t& idx) {
        size_t start = idx;
        if (s[idx] == '-') idx++;
        while (idx < s.size() && (std::isdigit(static_cast<unsigned char>(s[idx])) || s[idx] == '.' || s[idx] == 'e' || s[idx] == 'E' || s[idx] == '+' || s[idx] == '-')) {
            idx++;
        }
        std::string num_str = s.substr(start, idx - start);
        try {
            double val = std::stod(num_str);
            return JsonValue(val);
        } catch (...) {
            return JsonValue(0.0);
        }
    }
};

}
