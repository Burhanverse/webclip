#include "sync_manager.hpp"
#include "../util/json.hpp"
#include "../util/base64.hpp"
#include <iostream>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <cstring>
#include <fstream>
#include <sys/stat.h>

#ifndef S_ISREG
#if defined(_S_IFMT) && defined(_S_IFREG)
#define S_ISREG(m) (((m) & _S_IFMT) == _S_IFREG)
#elif defined(S_IFMT) && defined(S_IFREG)
#define S_ISREG(m) (((m) & S_IFMT) == S_IFREG)
#endif
#endif

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

    download_manager_ = std::make_unique<FileDownloadManager>();
    download_manager_->set_client(std::shared_ptr<HttpClient>(client_.get(), [](HttpClient*){}));
    download_manager_->set_progress_callback([](const std::string&, int index, int total, int64_t, int64_t, const std::string& name) {
        std::cout << "[file] Downloading (" << index << "/" << total << "): " << name << std::endl;
    });
    download_manager_->set_completion_callback([](const std::string&, int total_files) {
        std::cout << "[file] Received " << total_files << (total_files == 1 ? " file." : " files.") << std::endl;
    });
    download_manager_->set_failure_callback([](const std::string& file_name, const std::string& reason) {
        std::cerr << "[file error] Failed to download " << file_name << ": " << reason << std::endl;
    });
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
    return compute_hash(reinterpret_cast<const uint8_t*>(data.data()), data.size());
}

std::string SyncManager::compute_hash(const uint8_t* data, size_t len) {
    if (len == 0) return "";
    uint64_t hash = 14695981039346656037ULL;

    if (len > 65536) {
        const size_t headLen = 8192;
        for (size_t i = 0; i < headLen; i += 8) {
            uint64_t w;
            std::memcpy(&w, data + i, 8);
            hash ^= w;
            hash *= 1099511628211ULL;
        }
        const size_t tailStart = len - 8192;
        for (size_t i = tailStart; i + 8 <= len; i += 8) {
            uint64_t w;
            std::memcpy(&w, data + i, 8);
            hash ^= w;
            hash *= 1099511628211ULL;
        }
        size_t midSpan = tailStart - headLen;
        if (midSpan > 0) {
            size_t step = std::max<size_t>(8, (midSpan / 1024) & ~7ULL);
            for (size_t i = headLen; i + 8 <= tailStart; i += step) {
                uint64_t w;
                std::memcpy(&w, data + i, 8);
                hash ^= w;
                hash *= 1099511628211ULL;
            }
        }
    } else {
        size_t i = 0;
        for (; i + 8 <= len; i += 8) {
            uint64_t w;
            std::memcpy(&w, data + i, 8);
            hash ^= w;
            hash *= 1099511628211ULL;
        }
        for (; i < len; ++i) {
            hash ^= data[i];
            hash *= 1099511628211ULL;
        }
    }

    std::stringstream ss;
    ss << std::hex << std::setw(16) << std::setfill('0') << hash << "-" << len;
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
    if (event.event == "file") {
        JsonValue data = JsonValue::parse(event.data);
        std::string client_id = data.get_string("clientId");
        if (!client_id.empty() && client_id != client_->get_client_id()) {
            return;
        }
        std::string token = data.get_string("token");
        std::string name = data.get_string("name");
        int64_t size = data.get_int64("size");
        std::string mime_type = data.get_string("mimeType");
        std::string batch_id = data.get_string("batchId");
        int index = static_cast<int>(data.get_int64("index"));
        int total = static_cast<int>(data.get_int64("total"));

        if (token.empty() || name.empty()) {
            return;
        }

        FileOfferTask task;
        task.token = std::move(token);
        task.name = std::move(name);
        task.size = size;
        task.mime_type = std::move(mime_type);
        task.batch_id = std::move(batch_id);
        task.index = index;
        task.total = total;
        task.client_id = std::move(client_id);
        task.host = config_.host;
        task.port = config_.port;
        task.use_https = config_.use_https;
        task.insecure = config_.insecure;
        task.pairing_code = config_.code;

        if (download_manager_) {
            download_manager_->enqueue_task(std::move(task));
        }
        return;
    }

    if (event.event != "clipboard") {
        return;
    }

    JsonValue data = JsonValue::parse(event.data);
    std::string type = data.get_string("type");
    std::string source = data.get_string("source");
    std::string client_id = data.get_string("clientId");

    if (source == "web" || (!client_id.empty() && client_id == client_->get_client_id())) {

        return;
    }

    auto now = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();

    bool has_image = (type == "image") || data.get_bool("hasImage");
    if (has_image) {
        std::string mime_type = data.get_string("mimeType");
        if (mime_type.empty()) mime_type = "image/png";
        std::string inline_data = data.take_string("data");
        std::vector<uint8_t> image_bytes;

        if (!inline_data.empty()) {
            size_t comma = inline_data.find(',');
            std::string_view b64_part = (comma != std::string::npos)
                ? std::string_view(inline_data).substr(comma + 1)
                : std::string_view(inline_data);
            image_bytes = base64::decode(b64_part);
            std::string().swap(inline_data);
        } else {
            std::string image_url = data.get_string("imageUrl");
            HttpResponse img_resp = client_->get_image(image_url);
            if (img_resp.status_code == 200 && !img_resp.binary_body.empty()) {
                image_bytes = std::move(img_resp.binary_body);
            } else {
                std::cerr << "[image error] Failed to fetch image from phone: HTTP " << img_resp.status_code << std::endl;
            }
        }

        if (!image_bytes.empty()) {
            std::string hash = compute_hash(image_bytes);
            {
                std::lock_guard<std::mutex> guard(state_lock_);
                last_local_img_hash_ = hash;
                last_remote_img_hash_ = hash;
                last_local_text_.clear();
                last_remote_text_.clear();
                last_img_time_ms_ = now;
            }
            clipboard_->set_image(image_bytes, mime_type);
            std::cout << "[phone -> local image] " << image_bytes.size() << " bytes (" << mime_type << ")" << std::endl;
        }
        return;
    }

    std::string text = data.take_string("text");
    if (text.empty()) return;
    {
        std::lock_guard<std::mutex> guard(state_lock_);
        last_local_text_ = text;
        last_remote_text_ = text;
        last_local_img_hash_.clear();
        last_remote_img_hash_.clear();
        last_text_time_ms_ = now;
    }

    clipboard_->set_text(text);
    std::cout << "[phone -> local] " << text.size() << " chars: \""
              << truncate_preview(text) << "\"" << std::endl;
}

void SyncManager::run() {
    std::cout << "Connecting to Web Clipboard portal at " << config_.host << ":" << config_.port << std::endl;

    std::cout << "Using clipboard backend: " << clipboard_->get_backend_name() << std::endl;
    std::cout << "Client ID: " << config_.client_id << std::endl;

    client_ = std::make_unique<HttpClient>(
        config_.host,
        config_.port,
        config_.code,
        config_.use_https,
        config_.insecure,
        config_.client_id
    );

    download_manager_ = std::make_unique<FileDownloadManager>();
    download_manager_->set_client(std::shared_ptr<HttpClient>(client_.get(), [](HttpClient*){}));
    download_manager_->set_progress_callback([](const std::string&, int index, int total, int64_t, int64_t, const std::string& name) {
        std::cout << "[file] Downloading (" << index << "/" << total << "): " << name << std::endl;
    });
    download_manager_->set_completion_callback([](const std::string&, int total_files) {
        std::cout << "[file] Received " << total_files << (total_files == 1 ? " file." : " files.") << std::endl;
    });
    download_manager_->set_failure_callback([](const std::string& file_name, const std::string& reason) {
        std::cerr << "[file error] Failed to download " << file_name << ": " << reason << std::endl;
    });

    HttpResponse initial_state = client_->get_state();
    if (initial_state.status_code == 200) {
        JsonValue state_json = JsonValue::parse(initial_state.body);
        std::string instance_name = state_json.get_string("instanceName");
        if (instance_name.empty()) instance_name = state_json.get_string("deviceId");
        if (instance_name.empty()) instance_name = state_json.get_string("instance");
        if (!instance_name.empty()) {
            config_.instance_name = instance_name;
            std::cout << "[pairing] Captured device instance name: " << instance_name << std::endl;
        }
    }

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

            if (!remote_text.empty() && local_text != remote_text) {
                clipboard_->set_text(remote_text);
                std::lock_guard<std::mutex> guard(state_lock_);
                last_remote_text_ = remote_text;
                last_local_text_ = remote_text;
                std::cout << "Bootstrapped initial clipboard from phone: \""
                          << truncate_preview(remote_text) << "\"" << std::endl;
            } else if (!local_text.empty()) {
                std::lock_guard<std::mutex> guard(state_lock_);
                last_local_text_ = local_text;
            }
        }
    } else if (!initial_state.error.empty()) {
        std::cerr << "[warn] Initial state fetch failed: " << initial_state.error << " (will retry via SSE)" << std::endl;
    } else if (initial_state.status_code == 401) {
        std::cerr << "[error] Authentication failed: invalid pairing code." << std::endl;
    }

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

        if (clipboard_->has_image()) {
            ClipboardImage local_img = clipboard_->get_image();
            if (local_img.valid && !local_img.data.empty()) {
                std::string hash = compute_hash(local_img.data);
                bool should_push = false;
                {
                    std::lock_guard<std::mutex> guard(state_lock_);
                    if (hash != last_local_img_hash_ && hash != last_remote_img_hash_) {
                        last_local_img_hash_ = hash;
                        last_local_text_.clear();
                        last_img_time_ms_ = now;
                        should_push = true;
                    }
                }
                if (should_push) {
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

            const uint8_t* u = reinterpret_cast<const uint8_t*>(current_local.data());
            size_t len = current_local.size();
            bool is_png = (len >= 4 && u[0] == 0x89 && u[1] == 0x50 && u[2] == 0x4E && u[3] == 0x47);
            bool is_jpg = (len >= 3 && u[0] == 0xFF && u[1] == 0xD8 && u[2] == 0xFF);
            bool is_gif = (len >= 4 && u[0] == 0x47 && u[1] == 0x49 && u[2] == 0x46 && u[3] == 0x38);
            bool is_webp = (len >= 12 && u[0] == 'R' && u[1] == 'I' && u[2] == 'F' && u[3] == 'F' && u[8] == 'W' && u[9] == 'E' && u[10] == 'B' && u[11] == 'P');

            if (is_png || is_jpg || is_gif || is_webp) {
                std::string mime = is_png ? "image/png" : (is_jpg ? "image/jpeg" : (is_gif ? "image/gif" : "image/webp"));
                std::string hash = compute_hash(reinterpret_cast<const uint8_t*>(current_local.data()), current_local.size());
                bool should_push = false;
                {
                    std::lock_guard<std::mutex> guard(state_lock_);
                    if (hash != last_local_img_hash_ && hash != last_remote_img_hash_) {
                        last_local_img_hash_ = hash;
                        last_local_text_.clear();
                        last_img_time_ms_ = now;
                        should_push = true;
                    }
                }
                if (should_push) {
                    HttpResponse push_resp = client_->push_image(
                        reinterpret_cast<const uint8_t*>(current_local.data()), current_local.size(), mime);
                    if (push_resp.status_code == 200) {
                        std::cout << "[local -> phone image] " << current_local.size() << " bytes (" << mime << ")" << std::endl;
                    }
                }
                continue;
            }

            std::string candidate_path = current_local;
            if (candidate_path.rfind("file://", 0) == 0) {
                candidate_path = candidate_path.substr(7);
            }
            while (!candidate_path.empty() && (candidate_path.back() == '\n' || candidate_path.back() == '\r' || candidate_path.back() == ' ')) {
                candidate_path.pop_back();
            }
            if (candidate_path.size() >= 3 && candidate_path[0] == '/' && std::isalpha(static_cast<unsigned char>(candidate_path[1])) && candidate_path[2] == ':') {
                candidate_path = candidate_path.substr(1);
            }
            bool is_abs_path = (!candidate_path.empty() && candidate_path[0] == '/') ||
                               (candidate_path.size() >= 3 && std::isalpha(static_cast<unsigned char>(candidate_path[0])) && candidate_path[1] == ':' && (candidate_path[2] == '\\' || candidate_path[2] == '/'));
            if (candidate_path.find('\n') == std::string::npos && candidate_path.find('\r') == std::string::npos && is_abs_path) {
                struct stat st;
                if (stat(candidate_path.c_str(), &st) == 0 && S_ISREG(st.st_mode) && st.st_size > 0 && st.st_size <= 20 * 1024 * 1024) {
                    std::ifstream f(candidate_path, std::ios::binary);
                    if (f.is_open()) {
                        std::vector<uint8_t> file_bytes((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
                        if (file_bytes.size() >= 4) {
                            const uint8_t* fu = file_bytes.data();
                            bool file_is_png = (fu[0] == 0x89 && fu[1] == 0x50 && fu[2] == 0x4E && fu[3] == 0x47);
                            bool file_is_jpg = (fu[0] == 0xFF && fu[1] == 0xD8 && fu[2] == 0xFF);
                            bool file_is_gif = (fu[0] == 0x47 && fu[1] == 0x49 && fu[2] == 0x46 && fu[3] == 0x38);
                            bool file_is_webp = (file_bytes.size() >= 12 && fu[0] == 'R' && fu[1] == 'I' && fu[2] == 'F' && fu[3] == 'F' && fu[8] == 'W' && fu[9] == 'E' && fu[10] == 'B' && fu[11] == 'P');
                            if (file_is_png || file_is_jpg || file_is_gif || file_is_webp) {
                                std::string mime = file_is_png ? "image/png" : (file_is_jpg ? "image/jpeg" : (file_is_gif ? "image/gif" : "image/webp"));
                                std::string hash = compute_hash(file_bytes);
                                bool should_push = false;
                                {
                                    std::lock_guard<std::mutex> guard(state_lock_);
                                    if (hash != last_local_img_hash_ && hash != last_remote_img_hash_) {
                                        last_local_img_hash_ = hash;
                                        last_local_text_ = current_local;
                                        last_img_time_ms_ = now;
                                        should_push = true;
                                    }
                                }
                                if (should_push) {
                                    HttpResponse push_resp = client_->push_image(file_bytes, mime);
                                    if (push_resp.status_code == 200) {
                                        std::cout << "[local -> phone image] " << file_bytes.size() << " bytes (" << mime << ")" << std::endl;
                                    }
                                }
                                continue;
                            }
                        }
                    }
                }
            }

            bool should_push_text = false;
            {
                std::lock_guard<std::mutex> guard(state_lock_);
                if (current_local != last_local_text_ && current_local != last_remote_text_) {
                    last_local_text_ = current_local;
                    last_local_img_hash_.clear();
                    last_text_time_ms_ = now;
                    should_push_text = true;
                }
            }

            if (should_push_text) {
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
    stop_flag_.store(true);
    if (download_manager_) {
        download_manager_->cancel_all();
        download_manager_->stop();
    }
    if (sse_thread_ && sse_thread_->joinable()) {
        if (sse_thread_->get_id() == std::this_thread::get_id()) {

            sse_thread_->detach();
            return;
        }
        sse_thread_->join();
    }
}

void SyncManager::request_stop() {
    stop_flag_.store(true);
}

}
