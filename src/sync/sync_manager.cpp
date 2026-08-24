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

void SyncManager::handle_sse_event(const SseEvent& event) {
    if (event.event != "clipboard") {
        return;
    }

    JsonValue data = JsonValue::parse(event.data);
    std::string type = data.get_string("type");
    std::string source = data.get_string("source");

    if (source == "web") {
        // Echo of something pushed by a web/desktop client; ignore
        return;
    }

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
            std::lock_guard<std::mutex> guard(state_lock_);
            if (hash != last_remote_img_hash_) {
                last_remote_img_hash_ = hash;
                last_local_img_hash_ = hash;
                last_remote_text_.clear();
                last_local_text_.clear();
                clipboard_->set_image(image_bytes, mime_type);
                std::cout << "[phone -> local image] " << image_bytes.size() << " bytes (" << mime_type << ")" << std::endl;
            }
        }
        return;
    }

    std::string text = data.get_string("text");
    std::lock_guard<std::mutex> guard(state_lock_);
    if (text != last_remote_text_) {
        last_remote_text_ = text;
        last_remote_img_hash_.clear();
        std::string current_local = clipboard_->get_text();
        if (text != current_local) {
            clipboard_->set_text(text);
            last_local_text_ = text;
            last_local_img_hash_.clear();
            std::cout << "[phone -> local] " << text.size() << " chars: \""
                      << truncate_preview(text) << "\"" << std::endl;
        }
    }
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

        // Check image clipboard first
        if (clipboard_->has_image()) {
            ClipboardImage local_img = clipboard_->get_image();
            if (local_img.valid && !local_img.data.empty()) {
                std::string hash = compute_hash(local_img.data);
                std::lock_guard<std::mutex> guard(state_lock_);
                if (hash != last_local_img_hash_ && hash != last_remote_img_hash_) {
                    HttpResponse push_resp = client_->push_image(local_img.data, local_img.mime_type);
                    if (push_resp.status_code == 200) {
                        last_local_img_hash_ = hash;
                        last_remote_img_hash_ = hash;
                        last_local_text_.clear();
                        last_remote_text_.clear();
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
        {
            std::lock_guard<std::mutex> guard(state_lock_);
            if (!current_local.empty() && current_local != last_local_text_ && current_local != last_remote_text_) {
                HttpResponse push_resp = client_->push_clipboard(current_local);
                if (push_resp.status_code == 200) {
                    last_local_text_ = current_local;
                    last_remote_text_ = current_local;
                    last_local_img_hash_.clear();
                    last_remote_img_hash_.clear();
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
