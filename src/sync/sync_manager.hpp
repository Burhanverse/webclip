#pragma once

#include <memory>
#include <string>
#include <vector>
#include <mutex>
#include <atomic>
#include <thread>
#include "../clipboard/clipboard.hpp"
#include "../net/http_client.hpp"

namespace webclip {

struct SyncConfig {
    std::string host;
    int port = 8080;
    std::string code;
    bool use_https = false;
    bool insecure = false;
    double poll_interval_sec = 1.0;
    std::string client_id;
};

class SyncManager {
public:
    SyncManager(SyncConfig config, std::unique_ptr<IClipboard> clipboard);
    ~SyncManager();

    void run();

    void stop();

    void request_stop();

    std::atomic<bool>* stop_flag_for_signal() { return &stop_flag_; }

private:
    SyncConfig config_;
    std::unique_ptr<IClipboard> clipboard_;
    std::unique_ptr<HttpClient> client_;

    std::mutex state_lock_;
    std::string last_remote_text_;
    std::string last_local_text_;
    int64_t last_text_time_ms_ = 0;
    std::string last_remote_img_hash_;
    std::string last_local_img_hash_;
    int64_t last_img_time_ms_ = 0;

    std::atomic<bool> stop_flag_{false};
    std::unique_ptr<std::thread> sse_thread_;

    void handle_sse_event(const SseEvent& event);
    static std::string truncate_preview(const std::string& text, size_t max_len = 60);
    static std::string compute_hash(const std::vector<uint8_t>& data);
    static std::string compute_hash(const uint8_t* data, size_t len);
    bool should_suppress_text(const std::string& text, int64_t now_ms, int64_t window_ms = 2500);
    void mark_text_applied(const std::string& text, int64_t now_ms);
    bool should_suppress_image(const std::string& hash, int64_t now_ms, int64_t window_ms = 2500);
    void mark_image_applied(const std::string& hash, int64_t now_ms);
};

}
