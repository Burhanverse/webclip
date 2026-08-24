#pragma once

#include <string>
#include <memory>

namespace webclip {

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
     * Gets a human-readable name of the backend in use.
     */
    virtual std::string get_backend_name() const = 0;
};

/**
 * Creates the appropriate clipboard backend for the current OS and desktop environment.
 */
std::unique_ptr<IClipboard> create_clipboard();

} // namespace webclip
