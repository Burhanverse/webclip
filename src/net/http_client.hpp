#pragma once

#include <string>
#include <vector>
#include <cstdint>
#include <functional>
#include <atomic>
#include "sse_parser.hpp"

#include <mutex>

struct curl_slist;

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
    HttpClient(std::string host, int port, std::string code, bool use_https, bool insecure, std::string client_id = "");
    ~HttpClient();

    std::string get_base_url() const;
    const std::string& get_client_id() const { return client_id_; }
    const std::string& get_host() const { return host_; }
    int get_port() const { return port_; }
    const std::string& get_code() const { return code_; }
    bool get_use_https() const { return use_https_; }
    bool get_insecure() const { return insecure_; }
    void reset_connections();
    void update_endpoint(const std::string& host, int port, const std::string& code, bool use_https, bool insecure);
    void set_low_speed_options(long limit_bytes_per_sec, long time_seconds);

    HttpResponse get_state();

    HttpResponse get_image(const std::string& path_or_url = "/image/latest?source=phone");

    HttpResponse push_clipboard(const std::string& text, const std::string& clip_id = "");

    HttpResponse push_image(const std::vector<uint8_t>& bytes, const std::string& mime_type = "image/png", const std::string& clip_id = "");

    HttpResponse push_image(const uint8_t* data, size_t len, const std::string& mime_type = "image/png", const std::string& clip_id = "");

    HttpResponse push_image_data_url(const std::string& data_url, const std::string& mime_type = "image/png", const std::string& clip_id = "");

    HttpResponse post_json(const std::string& path, const std::string& json_body, long timeout_s = 10L);
    HttpResponse get_json(const std::string& path, long timeout_s = 10L);

    bool download_file(
        const std::string& path,
        const std::string& target_part_path,
        int64_t offset,
        std::function<void(int64_t downloaded, int64_t total)> progress_cb,
        const std::atomic<bool>& cancel_flag,
        int& out_http_code,
        int& out_retry_after
    );

    bool upload_file(
        const std::string& path,
        const std::string& source_file_path,
        int64_t offset,
        int64_t total_size,
        const std::string& upload_id,
        const std::string& file_name,
        const std::string& batch_id,
        int batch_index,
        int batch_total,
        std::function<void(int64_t uploaded, int64_t total)> progress_cb,
        const std::atomic<bool>& cancel_flag,
        int& out_http_code,
        int& out_retry_after,
        std::string& out_response_body
    );

    bool get_upload_status(
        const std::string& upload_id,
        int64_t& out_offset,
        int64_t& out_size,
        int& out_http_code,
        int& out_retry_after,
        bool& out_complete
    );

    bool get_upload_status(
        const std::string& upload_id,
        int64_t& out_offset,
        int64_t& out_size,
        int& out_http_code,
        int& out_retry_after
    );

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
    long low_speed_limit_ = 1024L;
    long low_speed_time_ = 30L;

    std::string post_url_;
    std::mutex post_mutex_;
    void* post_curl_ = nullptr;
    struct curl_slist* post_headers_ = nullptr;

    std::string build_url(const std::string& path, const std::string& extra_query = "") const;
    HttpResponse post_json_body(std::string json_body, long timeout_s, long connect_timeout_s);
};

}
