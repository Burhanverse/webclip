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

    HttpResponse get_state();

    HttpResponse get_image(const std::string& path_or_url = "/image/latest?source=phone");

    HttpResponse push_clipboard(const std::string& text, const std::string& clip_id = "");

    HttpResponse push_image(const std::vector<uint8_t>& bytes, const std::string& mime_type = "image/png", const std::string& clip_id = "");

    HttpResponse push_image(const uint8_t* data, size_t len, const std::string& mime_type = "image/png", const std::string& clip_id = "");

    HttpResponse push_image_data_url(const std::string& data_url, const std::string& mime_type = "image/png", const std::string& clip_id = "");

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
    HttpResponse post_json_body(std::string json_body, long timeout_s, long connect_timeout_s);
};

}
