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

    /**
     * Reads current text from the system clipboard.
     * Returns empty string if empty or clipboard unavailable.
     */
    virtual std::string get_text() = 0;

    /**
     * Sets new text into the system clipboard.
     */
    virtual bool set_text(const std::string& text) = 0;

    /**
     * Checks if the system clipboard currently contains an image.
     */
    virtual bool has_image() = 0;

    /**
     * Reads current image binary data from the system clipboard.
     */
    virtual ClipboardImage get_image() = 0;

    /**
     * Sets new image into the system clipboard.
     */
    virtual bool set_image(const std::vector<uint8_t>& data, const std::string& mime_type = "image/png") = 0;

    /**
     * Gets a human-readable name of the backend in use.
     */
    virtual std::string get_backend_name() const = 0;
};

/**
 * Creates the appropriate clipboard backend for the current OS and desktop environment.
 */
std::unique_ptr<IClipboard> create_clipboard();

} // namespace webclip

