#include "http_client.hpp"
#include "../util/json.hpp"
#include "../version.hpp"
#include <curl/curl.h>
#include <iostream>
#include <thread>
#include <chrono>

namespace webclip {

namespace {

#if defined(_WIN32)
const std::string CLIENT_USER_AGENT = std::string(APP_NAME) + "/" + std::string(VERSION_STRING) + " (Windows; x86_64)";
#else
const std::string CLIENT_USER_AGENT = std::string(APP_NAME) + "/" + std::string(VERSION_STRING) + " (Linux; x86_64)";
#endif

size_t write_string_cb(void* ptr, size_t size, size_t nmemb, void* userdata) {
    size_t total = size * nmemb;
    auto* str = static_cast<std::string*>(userdata);
    str->append(static_cast<const char*>(ptr), total);
    return total;
}

size_t write_binary_cb(void* ptr, size_t size, size_t nmemb, void* userdata) {
    size_t total = size * nmemb;
    auto* vec = static_cast<std::vector<uint8_t>*>(userdata);
    const uint8_t* byte_ptr = static_cast<const uint8_t*>(ptr);
    vec->insert(vec->end(), byte_ptr, byte_ptr + total);
    return total;
}

struct StreamContext {
    SseParser* parser;
    const std::atomic<bool>* stop_flag;
};

size_t stream_write_cb(void* ptr, size_t size, size_t nmemb, void* userdata) {
    auto* ctx = static_cast<StreamContext*>(userdata);
    if (ctx->stop_flag && ctx->stop_flag->load()) {
        return 0;
    }
    size_t total = size * nmemb;
    ctx->parser->feed(static_cast<const char*>(ptr), total);
    return total;
}

int progress_cb(void* clientp, curl_off_t, curl_off_t, curl_off_t, curl_off_t) {
    auto* stop_flag = static_cast<const std::atomic<bool>*>(clientp);
    if (stop_flag && stop_flag->load()) {
        return 1;
    }
    return 0;
}

}

HttpClient::HttpClient(std::string host, int port, std::string code, bool use_https, bool insecure, std::string client_id)
    : host_(std::move(host)),
      port_(port),
      code_(std::move(code)),
      use_https_(use_https),
      insecure_(insecure),
      client_id_(std::move(client_id)) {

    static const bool curl_initialized = []() {
        curl_global_init(CURL_GLOBAL_DEFAULT);
        return true;
    }();
    (void)curl_initialized;

    post_url_ = build_url("/clipboard");
}

HttpClient::~HttpClient() {
    std::lock_guard<std::mutex> guard(post_mutex_);
    if (post_curl_) {
        curl_easy_cleanup(static_cast<CURL*>(post_curl_));
        post_curl_ = nullptr;
    }
    if (post_headers_) {
        curl_slist_free_all(post_headers_);
        post_headers_ = nullptr;
    }
}

std::string HttpClient::get_base_url() const {
    std::string scheme = use_https_ ? "https" : "http";
    return scheme + "://" + host_ + ":" + std::to_string(port_);
}

std::string HttpClient::build_url(const std::string& path, const std::string& extra_query) const {
    std::string url = get_base_url() + path;
    std::string query = "code=" + code_;
    if (!extra_query.empty()) {
        query += "&" + extra_query;
    }
    url += (path.find('?') == std::string::npos ? "?" : "&") + query;
    return url;
}

HttpResponse HttpClient::get_state() {
    HttpResponse resp;
    CURL* curl = curl_easy_init();
    if (!curl) {
        resp.error = "Failed to initialize CURL";
        return resp;
    }

    std::string url = build_url("/state");
    struct curl_slist* headers = nullptr;
    headers = curl_slist_append(headers, ("X-Pairing-Code: " + code_).c_str());
    headers = curl_slist_append(headers, "Accept: application/json");
    headers = curl_slist_append(headers, "Connection: keep-alive");

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_USERAGENT, CLIENT_USER_AGENT.c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_string_cb);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &resp.body);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 6L);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 5L);
    curl_easy_setopt(curl, CURLOPT_TCP_KEEPALIVE, 1L);
    curl_easy_setopt(curl, CURLOPT_TCP_KEEPIDLE, 60L);
    curl_easy_setopt(curl, CURLOPT_TCP_KEEPINTVL, 30L);

    if (insecure_ || use_https_) {
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, insecure_ ? 0L : 1L);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, insecure_ ? 0L : 2L);
    }

    CURLcode res = curl_easy_perform(curl);
    if (res != CURLE_OK) {
        resp.error = curl_easy_strerror(res);
    } else {
        long http_code = 0;
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
        resp.status_code = static_cast<int>(http_code);
    }

    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
    return resp;
}

HttpResponse HttpClient::get_image(const std::string& path_or_url) {
    HttpResponse resp;
    CURL* curl = curl_easy_init();
    if (!curl) {
        resp.error = "Failed to initialize CURL";
        return resp;
    }

    std::string url;
    if (path_or_url.rfind("http://", 0) == 0 || path_or_url.rfind("https://", 0) == 0) {
        url = path_or_url;
    } else {
        std::string path = path_or_url.empty() ? "/image/latest?source=phone" : path_or_url;
        url = build_url(path);
    }

    struct curl_slist* headers = nullptr;
    headers = curl_slist_append(headers, ("X-Pairing-Code: " + code_).c_str());
    headers = curl_slist_append(headers, "Accept: image/*, */*");
    headers = curl_slist_append(headers, "Connection: keep-alive");

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_USERAGENT, CLIENT_USER_AGENT.c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_binary_cb);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &resp.binary_body);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 15L);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 6L);
    curl_easy_setopt(curl, CURLOPT_TCP_KEEPALIVE, 1L);
    curl_easy_setopt(curl, CURLOPT_TCP_KEEPIDLE, 60L);
    curl_easy_setopt(curl, CURLOPT_TCP_KEEPINTVL, 30L);

    if (insecure_ || use_https_) {
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, insecure_ ? 0L : 1L);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, insecure_ ? 0L : 2L);
    }

    CURLcode res = curl_easy_perform(curl);
    if (res != CURLE_OK) {
        resp.error = curl_easy_strerror(res);
    } else {
        long http_code = 0;
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
        resp.status_code = static_cast<int>(http_code);

        char* ct = nullptr;
        curl_easy_getinfo(curl, CURLINFO_CONTENT_TYPE, &ct);
        if (ct) {
            resp.content_type = ct;
        }
    }

    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
    return resp;
}

HttpResponse HttpClient::push_clipboard(const std::string& text, const std::string& clip_id) {
    std::string json_body;
    json_body.reserve(text.size() + client_id_.size() + clip_id.size() + 96);
    json_body += "{\"type\":\"text\",\"text\":\"";
    json_body += JsonValue::escape_string(text);
    json_body += "\",\"clientId\":\"";
    json_body += JsonValue::escape_string(client_id_);
    if (!clip_id.empty()) {
        json_body += "\",\"clipId\":\"";
        json_body += JsonValue::escape_string(clip_id);
    }
    json_body += "\"}";

    return post_json_body(std::move(json_body), 10L, 5L);
}

HttpResponse HttpClient::push_image(const std::vector<uint8_t>& bytes, const std::string& mime_type, const std::string& clip_id) {
    return push_image(bytes.data(), bytes.size(), mime_type, clip_id);
}

HttpResponse HttpClient::push_image(const uint8_t* data, size_t len, const std::string& mime_type, const std::string& clip_id) {
    static const char b64_charset[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string mime = mime_type.empty() ? "image/png" : mime_type;

    const size_t b64_len = ((len + 2) / 3) * 4;
    const size_t prefix_len = 64 + mime.size() * 2 + client_id_.size() + (clip_id.empty() ? 0 : clip_id.size() + 11);
    std::string json_body;
    json_body.reserve(prefix_len + b64_len + 24);

    json_body += "{\"type\":\"image\",\"mimeType\":\"";
    json_body += JsonValue::escape_string(mime);
    json_body += "\",\"data\":\"data:";
    json_body += mime;
    json_body += ";base64,";

    size_t body_start = json_body.size();
    json_body.resize(body_start + b64_len);
    char* out = &json_body[body_start];
    size_t oi = 0;
    for (size_t i = 0; i < len; i += 3) {
        uint32_t b = (static_cast<uint32_t>(data[i]) << 16);
        if (i + 1 < len) b |= (static_cast<uint32_t>(data[i + 1]) << 8);
        if (i + 2 < len) b |= static_cast<uint32_t>(data[i + 2]);

        out[oi++] = b64_charset[(b >> 18) & 0x3F];
        out[oi++] = b64_charset[(b >> 12) & 0x3F];
        out[oi++] = (i + 1 < len) ? b64_charset[(b >> 6) & 0x3F] : '=';
        out[oi++] = (i + 2 < len) ? b64_charset[b & 0x3F] : '=';
    }

    json_body += "\",\"clientId\":\"";
    json_body += JsonValue::escape_string(client_id_);
    if (!clip_id.empty()) {
        json_body += "\",\"clipId\":\"";
        json_body += JsonValue::escape_string(clip_id);
    }
    json_body += "\"}";

    return post_json_body(std::move(json_body), 20L, 6L);
}

HttpResponse HttpClient::push_image_data_url(const std::string& data_url, const std::string& mime_type, const std::string& clip_id) {
    std::string mime = mime_type.empty() ? "image/png" : mime_type;

    std::string json_body;
    json_body.reserve(data_url.size() + mime.size() + client_id_.size() + clip_id.size() + 160);
    json_body += "{\"type\":\"image\",\"mimeType\":\"";
    json_body += JsonValue::escape_string(mime);
    json_body += "\",\"data\":\"";
    json_body += data_url;
    json_body += "\",\"clientId\":\"";
    json_body += JsonValue::escape_string(client_id_);
    if (!clip_id.empty()) {
        json_body += "\",\"clipId\":\"";
        json_body += JsonValue::escape_string(clip_id);
    }
    json_body += "\"}";

    return post_json_body(std::move(json_body), 20L, 6L);
}

HttpResponse HttpClient::post_json_body(std::string json_body, long timeout_s, long connect_timeout_s) {
    std::lock_guard<std::mutex> guard(post_mutex_);
    HttpResponse resp;

    auto t0 = std::chrono::steady_clock::now();

    CURL* curl = static_cast<CURL*>(post_curl_);
    if (!curl) {
        curl = curl_easy_init();
        if (!curl) {
            resp.error = "Failed to initialize CURL";
            return resp;
        }
        if (!post_headers_) {
            post_headers_ = curl_slist_append(post_headers_, "Content-Type: application/json; charset=utf-8");
            post_headers_ = curl_slist_append(post_headers_, ("X-Pairing-Code: " + code_).c_str());
            post_headers_ = curl_slist_append(post_headers_, "Accept: application/json");
            post_headers_ = curl_slist_append(post_headers_, "Connection: keep-alive");
        }
        curl_easy_setopt(curl, CURLOPT_URL, post_url_.c_str());
        curl_easy_setopt(curl, CURLOPT_USERAGENT, CLIENT_USER_AGENT.c_str());
        curl_easy_setopt(curl, CURLOPT_POST, 1L);
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, post_headers_);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_string_cb);
        curl_easy_setopt(curl, CURLOPT_TCP_KEEPALIVE, 1L);
        curl_easy_setopt(curl, CURLOPT_TCP_KEEPIDLE, 60L);
        curl_easy_setopt(curl, CURLOPT_TCP_KEEPINTVL, 30L);

        if (insecure_ || use_https_) {
            curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, insecure_ ? 0L : 1L);
            curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, insecure_ ? 0L : 2L);
        }
        post_curl_ = curl;
    }

    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, json_body.c_str());
    curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, static_cast<long>(json_body.length()));
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &resp.body);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, timeout_s);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, connect_timeout_s);

    CURLcode res = curl_easy_perform(curl);
    if (res != CURLE_OK) {
        resp.error = curl_easy_strerror(res);
        curl_easy_cleanup(curl);
        post_curl_ = nullptr;
    } else {
        long http_code = 0;
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
        resp.status_code = static_cast<int>(http_code);
    }

    static const bool s_perfLog = (std::getenv("WEBCLIP_PERF") != nullptr || std::getenv("WEBCLIP_DEBUG_PERF") != nullptr);
    if (s_perfLog) {
        auto t1 = std::chrono::steady_clock::now();
        auto durMs = std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0).count();
        std::cerr << "[PERF] HttpClient::post_json_body took " << durMs << "ms (size=" << json_body.size() << " bytes, status=" << resp.status_code << ")\n";
    }

    return resp;
}

void HttpClient::stream_events(
    std::function<void(const SseEvent&)> on_event,
    std::function<void(const std::string&)> on_status,
    const std::atomic<bool>& stop_flag
) {
    SseParser parser(std::move(on_event));

    while (!stop_flag.load()) {
        parser.reset();
        CURL* curl = curl_easy_init();
        if (!curl) {
            if (on_status) on_status("Failed to initialize CURL for SSE stream");
            std::this_thread::sleep_for(std::chrono::seconds(2));
            continue;
        }

        std::string url = build_url("/events", "clientId=" + client_id_);
        struct curl_slist* headers = nullptr;
        headers = curl_slist_append(headers, "Accept: text/event-stream");
        headers = curl_slist_append(headers, "Cache-Control: no-cache");
        headers = curl_slist_append(headers, ("X-Pairing-Code: " + code_).c_str());

        StreamContext ctx{&parser, &stop_flag};

        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_USERAGENT, CLIENT_USER_AGENT.c_str());
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, stream_write_cb);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &ctx);
        curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, progress_cb);
        curl_easy_setopt(curl, CURLOPT_XFERINFODATA, &stop_flag);
        curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);
        curl_easy_setopt(curl, CURLOPT_TCP_KEEPALIVE, 1L);

        curl_easy_setopt(curl, CURLOPT_TCP_KEEPIDLE, 15L);
        curl_easy_setopt(curl, CURLOPT_TCP_KEEPINTVL, 5L);
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, 0L);
        curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 6L);

        if (insecure_ || use_https_) {
            curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, insecure_ ? 0L : 1L);
            curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, insecure_ ? 0L : 2L);
        }

        if (on_status) {
            on_status("Connecting to SSE stream...");
        }

        CURLcode res = curl_easy_perform(curl);

        curl_slist_free_all(headers);
        curl_easy_cleanup(curl);

        if (stop_flag.load()) {
            break;
        }

        if (res != CURLE_OK) {
            std::string err = curl_easy_strerror(res);
            if (on_status) {
                on_status("SSE connection dropped (" + err + "); reconnecting...");
            }
            std::this_thread::sleep_for(std::chrono::seconds(2));
        }
    }
}

}
