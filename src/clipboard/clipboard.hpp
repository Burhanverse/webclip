#pragma once

#include <string>
#include <vector>
#include <memory>
#include <cstdint>

namespace webclip {

struct ClipboardImage {
    std::vector<uint8_t> data;
    std::string mime_type = "image/png";
    bool valid = false;
};

class IClipboard {
public:
    virtual ~IClipboard() = default;

    virtual std::string get_text() = 0;

    virtual bool set_text(const std::string& text) = 0;

    virtual bool has_image() = 0;

    virtual ClipboardImage get_image() = 0;

    virtual bool set_image(const std::vector<uint8_t>& data, const std::string& mime_type = "image/png") = 0;

    virtual std::string get_backend_name() const = 0;
};

std::unique_ptr<IClipboard> create_clipboard();

}
