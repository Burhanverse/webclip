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

struct StreamContext {
    SseParser* parser;
    const std::atomic<bool>* stop_flag;
};

size_t stream_write_cb(void* ptr, size_t size, size_t nmemb, void* userdata) {
    auto* ctx = static_cast<StreamContext*>(userdata);
    if (ctx->stop_flag && ctx->stop_flag->load()) {
        return 0; // Aborts curl transfer
    }
    size_t total = size * nmemb;
    ctx->parser->feed(static_cast<const char*>(ptr), total);
    return total;
}

int progress_cb(void* clientp, curl_off_t, curl_off_t, curl_off_t, curl_off_t) {
    auto* stop_flag = static_cast<const std::atomic<bool>*>(clientp);
    if (stop_flag && stop_flag->load()) {
        return 1; // Non-zero aborts transfer
    }
    return 0;
}

} // namespace

HttpClient::HttpClient(std::string host, int port, std::string code, bool use_https, bool insecure, std::string client_id)
    : host_(std::move(host)),
      port_(port),
      code_(std::move(code)),
      use_https_(use_https),
      insecure_(insecure),
      client_id_(std::move(client_id)) {
    curl_global_init(CURL_GLOBAL_DEFAULT);
}

HttpClient::~HttpClient() {
    curl_global_cleanup();
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
    url += "?" + query;
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

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_USERAGENT, CLIENT_USER_AGENT.c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_string_cb);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &resp.body);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 6L);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 5L);

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

HttpResponse HttpClient::push_clipboard(const std::string& text) {
    HttpResponse resp;
    CURL* curl = curl_easy_init();
    if (!curl) {
        resp.error = "Failed to initialize CURL";
        return resp;
    }

    std::string url = build_url("/clipboard");
    JsonValue json = JsonValue::object();
    json.set("text", JsonValue(text));
    json.set("clientId", JsonValue(client_id_));
    std::string json_body = json.serialize();

    struct curl_slist* headers = nullptr;
    headers = curl_slist_append(headers, "Content-Type: application/json; charset=utf-8");
    headers = curl_slist_append(headers, ("X-Pairing-Code: " + code_).c_str());
    headers = curl_slist_append(headers, "Accept: application/json");

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_USERAGENT, CLIENT_USER_AGENT.c_str());
    curl_easy_setopt(curl, CURLOPT_POST, 1L);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, json_body.c_str());
    curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, static_cast<long>(json_body.length()));
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_string_cb);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &resp.body);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 6L);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 5L);

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
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, 0L); // Infinite stream
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

} // namespace webclip
