#pragma once

#include <string>
#include <vector>
#include <cstdint>
#include <functional>
#include <atomic>
#include "sse_parser.hpp"

namespace webclip {

struct HttpResponse {
    int status_code = 0;
    std::string body;
    std::vector<uint8_t> binary_body;
    std::string content_type;
    std::string error;
};

class HttpClient {
public:
    HttpClient(std::string host, int port, std::string code, bool use_https, bool insecure, std::string client_id);
    ~HttpClient();

    std::string get_base_url() const;
    const std::string& get_client_id() const { return client_id_; }

    /**
     * Fetches current remote state via GET /state
     */
    HttpResponse get_state();

    /**
     * Fetches raw binary image data (e.g., from GET /image/latest?source=phone)
     */
    HttpResponse get_image(const std::string& path_or_url = "/image/latest?source=phone");

    /**
     * Pushes local text to remote portal via POST /clipboard
     */
    HttpResponse push_clipboard(const std::string& text);

    /**
     * Pushes local binary image data to remote portal via POST /clipboard
     */
    HttpResponse push_image(const std::vector<uint8_t>& bytes, const std::string& mime_type = "image/png");

    /**
     * Pushes data URL / base64 image data to remote portal via POST /clipboard
     */
    HttpResponse push_image_data_url(const std::string& data_url, const std::string& mime_type = "image/png");

    /**
     * Streams SSE events continuously from GET /events.
     * Blocks until stop_flag is true or unrecoverable error occurs.
     * Automatically reconnects on network drop.
     */
    void stream_events(
        std::function<void(const SseEvent&)> on_event,
        std::function<void(const std::string&)> on_status,
        const std::atomic<bool>& stop_flag
    );

private:
    std::string host_;
    int port_;
    std::string code_;
    bool use_https_;
    bool insecure_;
    std::string client_id_;

    std::string build_url(const std::string& path, const std::string& extra_query = "") const;
};

} // namespace webclip
