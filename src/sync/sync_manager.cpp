#include "sync_manager.hpp"
#include "../util/json.hpp"
#include "../util/base64.hpp"
#include <iostream>
#include <chrono>
#include <iomanip>
#include <sstream>

namespace webclip {

SyncManager::SyncManager(SyncConfig config, std::unique_ptr<IClipboard> clipboard)
    : config_(std::move(config)),
      clipboard_(std::move(clipboard)) {
    client_ = std::make_unique<HttpClient>(
        config_.host,
        config_.port,
        config_.code,
        config_.use_https,
        config_.insecure,
        config_.client_id
    );
}

SyncManager::~SyncManager() {
    stop();
}

std::string SyncManager::truncate_preview(const std::string& text, size_t max_len) {
    std::string preview;
    for (char c : text) {
        if (c == '\r' || c == '\n' || c == '\t') {
            preview += ' ';
        } else {
            preview += c;
        }
        if (preview.size() >= max_len) {
            preview += "...";
            break;
        }
    }
    return preview;
}

std::string SyncManager::compute_hash(const std::vector<uint8_t>& data) {
    if (data.empty()) return "";
    uint64_t hash = 14695981039346656037ULL;
    for (uint8_t byte : data) {
        hash ^= byte;
        hash *= 1099511628211ULL;
    }
    std::stringstream ss;
    ss << std::hex << std::setw(16) << std::setfill('0') << hash << "-" << data.size();
    return ss.str();
}

bool SyncManager::should_suppress_text(const std::string& text, int64_t now_ms, int64_t window_ms) {
    std::lock_guard<std::mutex> guard(state_lock_);
    if (text == last_local_text_ && (now_ms - last_text_time_ms_) < window_ms) {
        return true;
    }
    if (text == last_remote_text_ && (now_ms - last_text_time_ms_) < window_ms) {
        return true;
    }
    return false;
}

void SyncManager::mark_text_applied(const std::string& text, int64_t now_ms) {
    std::lock_guard<std::mutex> guard(state_lock_);
    last_local_text_ = text;
    last_remote_text_ = text;
    last_text_time_ms_ = now_ms;
    last_local_img_hash_.clear();
    last_remote_img_hash_.clear();
}

bool SyncManager::should_suppress_image(const std::string& hash, int64_t now_ms, int64_t window_ms) {
    if (hash.empty()) return true;
    std::lock_guard<std::mutex> guard(state_lock_);
    if (hash == last_local_img_hash_ && (now_ms - last_img_time_ms_) < window_ms) {
        return true;
    }
    if (hash == last_remote_img_hash_ && (now_ms - last_img_time_ms_) < window_ms) {
        return true;
    }
    return false;
}

void SyncManager::mark_image_applied(const std::string& hash, int64_t now_ms) {
    std::lock_guard<std::mutex> guard(state_lock_);
    last_local_img_hash_ = hash;
    last_remote_img_hash_ = hash;
    last_img_time_ms_ = now_ms;
    last_local_text_.clear();
    last_remote_text_.clear();
}

void SyncManager::handle_sse_event(const SseEvent& event) {
    if (event.event != "clipboard") {
        return;
    }

    JsonValue data = JsonValue::parse(event.data);
    std::string type = data.get_string("type");
    std::string source = data.get_string("source");
    std::string client_id = data.get_string("clientId");

    if (source == "web" || (!client_id.empty() && client_id == client_->get_client_id())) {
        // Echo of something pushed by this client; ignore
        return;
    }

    auto now = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();

    if (type == "image") {
        std::string mime_type = data.get_string("mimeType");
        if (mime_type.empty()) mime_type = "image/png";
        std::string inline_data = data.get_string("data");
        std::vector<uint8_t> image_bytes;

        if (!inline_data.empty()) {
            size_t comma = inline_data.find(',');
            std::string_view b64_part = (comma != std::string::npos)
                ? std::string_view(inline_data).substr(comma + 1)
                : std::string_view(inline_data);
            image_bytes = base64::decode(b64_part);
        } else {
            std::string image_url = data.get_string("imageUrl");
            HttpResponse img_resp = client_->get_image(image_url);
            if (img_resp.status_code == 200 && !img_resp.binary_body.empty()) {
                image_bytes = std::move(img_resp.binary_body);
            }
        }

        if (!image_bytes.empty()) {
            std::string hash = compute_hash(image_bytes);
            if (should_suppress_image(hash, now)) return;
            mark_image_applied(hash, now);
            clipboard_->set_image(image_bytes, mime_type);
            std::cout << "[phone -> local image] " << image_bytes.size() << " bytes (" << mime_type << ")" << std::endl;
        }
        return;
    }

    std::string text = data.get_string("text");
    if (text.empty()) return;
    if (should_suppress_text(text, now)) return;
    mark_text_applied(text, now);

    clipboard_->set_text(text);
    std::cout << "[phone -> local] " << text.size() << " chars: \""
              << truncate_preview(text) << "\"" << std::endl;
}

void SyncManager::run() {
    std::cout << "Connecting to Web Clipboard portal at " << client_->get_base_url() << std::endl;
    std::cout << "Using clipboard backend: " << clipboard_->get_backend_name() << std::endl;
    std::cout << "Client ID: " << client_->get_client_id() << std::endl;

    // Fetch initial state
    HttpResponse initial_state = client_->get_state();
    if (initial_state.status_code == 200) {
        JsonValue state_json = JsonValue::parse(initial_state.body);
        std::string type = state_json.get_string("type");
        bool has_image = (type == "image") || state_json.get_bool("hasImage");

        if (has_image) {
            std::string image_url = state_json.get_string("imageUrl");
            HttpResponse img_resp = client_->get_image(image_url);
            if (img_resp.status_code == 200 && !img_resp.binary_body.empty()) {
                std::string hash = compute_hash(img_resp.binary_body);
                std::lock_guard<std::mutex> guard(state_lock_);
                last_remote_img_hash_ = hash;
                last_local_img_hash_ = hash;
                clipboard_->set_image(img_resp.binary_body, img_resp.content_type.empty() ? "image/png" : img_resp.content_type);
                std::cout << "Bootstrapped initial image clipboard from phone (" << img_resp.binary_body.size() << " bytes)" << std::endl;
            }
        } else {
            std::string remote_text = state_json.get_string("text");
            std::string local_text = clipboard_->get_text();

            std::lock_guard<std::mutex> guard(state_lock_);
            last_remote_text_ = remote_text;
            last_local_text_ = local_text;

            if (!remote_text.empty() && remote_text != local_text) {
                std::cout << "Bootstrapped initial clipboard from phone (" << remote_text.size() << " chars)" << std::endl;
                clipboard_->set_text(remote_text);
                last_local_text_ = remote_text;
            }
        }
    } else if (!initial_state.error.empty()) {
        std::cerr << "[warn] Initial state fetch failed: " << initial_state.error << " (will retry via SSE)" << std::endl;
    } else if (initial_state.status_code == 401) {
        std::cerr << "[error] Authentication failed: invalid pairing code." << std::endl;
    }

    // Start background SSE listener
    sse_thread_ = std::make_unique<std::thread>([this]() {
        client_->stream_events(
            [this](const SseEvent& ev) { handle_sse_event(ev); },
            [](const std::string& status) { std::cout << "[sse] " << status << std::endl; },
            stop_flag_
        );
    });

    std::cout << "Watching local clipboard for changes (polling every "
              << config_.poll_interval_sec << "s). Press Ctrl+C to stop." << std::endl;

    auto poll_interval = std::chrono::duration<double>(config_.poll_interval_sec);

    while (!stop_flag_.load()) {
        std::this_thread::sleep_for(poll_interval);
        if (stop_flag_.load()) break;

        auto now = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();

        // Check image clipboard first
        if (clipboard_->has_image()) {
            ClipboardImage local_img = clipboard_->get_image();
            if (local_img.valid && !local_img.data.empty()) {
                std::string hash = compute_hash(local_img.data);
                if (!should_suppress_image(hash, now)) {
                    mark_image_applied(hash, now);
                    HttpResponse push_resp = client_->push_image(local_img.data, local_img.mime_type);
                    if (push_resp.status_code == 200) {
                        std::cout << "[local -> phone image] " << local_img.data.size() << " bytes (" << local_img.mime_type << ")" << std::endl;
                    } else {
                        std::cerr << "[warn] Push image to phone failed (HTTP " << push_resp.status_code
                                  << (push_resp.error.empty() ? "" : ": " + push_resp.error)
                                  << ")" << std::endl;
                    }
                    continue;
                }
            }
        }

        std::string current_local = clipboard_->get_text();
        if (!current_local.empty()) {
            // Guard: Check if get_text returned raw binary image bytes (PNG / JPEG / GIF / WEBP)
            const uint8_t* u = reinterpret_cast<const uint8_t*>(current_local.data());
            size_t len = current_local.size();
            bool is_png = (len >= 4 && u[0] == 0x89 && u[1] == 0x50 && u[2] == 0x4E && u[3] == 0x47);
            bool is_jpg = (len >= 3 && u[0] == 0xFF && u[1] == 0xD8 && u[2] == 0xFF);
            bool is_gif = (len >= 4 && u[0] == 0x47 && u[1] == 0x49 && u[2] == 0x46 && u[3] == 0x38);
            bool is_webp = (len >= 12 && u[0] == 'R' && u[1] == 'I' && u[2] == 'F' && u[3] == 'F' && u[8] == 'W' && u[9] == 'E' && u[10] == 'B' && u[11] == 'P');

            if (is_png || is_jpg || is_gif || is_webp) {
                std::string mime = is_png ? "image/png" : (is_jpg ? "image/jpeg" : (is_gif ? "image/gif" : "image/webp"));
                std::vector<uint8_t> raw_bytes(u, u + len);
                std::string hash = compute_hash(raw_bytes);
                if (!should_suppress_image(hash, now)) {
                    mark_image_applied(hash, now);
                    HttpResponse push_resp = client_->push_image(raw_bytes, mime);
                    if (push_resp.status_code == 200) {
                        std::cout << "[local -> phone image] " << raw_bytes.size() << " bytes (" << mime << ")" << std::endl;
                    }
                }
                continue;
            }

            if (!should_suppress_text(current_local, now)) {
                mark_text_applied(current_local, now);
                HttpResponse push_resp = client_->push_clipboard(current_local);
                if (push_resp.status_code == 200) {
                    std::cout << "[local -> phone] " << current_local.size() << " chars: \""
                              << truncate_preview(current_local) << "\"" << std::endl;
                } else {
                    std::cerr << "[warn] Push to phone failed (HTTP " << push_resp.status_code
                              << (push_resp.error.empty() ? "" : ": " + push_resp.error)
                              << ")" << std::endl;
                }
            }
        }
    }
}

void SyncManager::stop() {
    if (!stop_flag_.exchange(true)) {
        if (sse_thread_ && sse_thread_->joinable()) {
            sse_thread_->join();
        }
    }
}

} // namespace webclip
