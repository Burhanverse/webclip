#include "file_upload_manager.hpp"
#include "http_client.hpp"
#include "../util/json.hpp"
#include <QFileInfo>
#include <chrono>

namespace webclip {

FileUploadManager::FileUploadManager()
    : FileUploadManager(nullptr) {
}

FileUploadManager::FileUploadManager(std::shared_ptr<HttpClient> client)
    : client_(std::move(client)) {
    worker_thread_ = std::thread(&FileUploadManager::worker_loop, this);
}

FileUploadManager::~FileUploadManager() {
    stop();
}

void FileUploadManager::set_client(std::shared_ptr<HttpClient> client) {
    std::lock_guard<std::mutex> lock(client_mutex_);
    client_ = std::move(client);
}

void FileUploadManager::enqueue_task(FileUploadTask task) {
    {
        std::lock_guard<std::mutex> lock(queue_mutex_);
        queue_.push(std::move(task));
    }
    queue_cv_.notify_one();
}

void FileUploadManager::cancel_current() {
    cancel_current_flag_.store(true);
}

void FileUploadManager::cancel_all() {
    std::lock_guard<std::mutex> lock(queue_mutex_);
    cancel_current_flag_.store(true);
    std::queue<FileUploadTask> empty;
    std::swap(queue_, empty);
    accepted_batches_.clear();
}

void FileUploadManager::stop() {
    stop_flag_.store(true);
    cancel_current_flag_.store(true);
    queue_cv_.notify_all();
    if (worker_thread_.joinable()) {
        worker_thread_.join();
    }
}

bool FileUploadManager::is_busy() const {
    std::lock_guard<std::mutex> lock(queue_mutex_);
    return is_processing_.load() || !queue_.empty();
}

void FileUploadManager::set_progress_callback(ProgressCallback cb) {
    std::lock_guard<std::mutex> lock(callback_mutex_);
    progress_cb_ = std::move(cb);
}

void FileUploadManager::set_completion_callback(CompletionCallback cb) {
    std::lock_guard<std::mutex> lock(callback_mutex_);
    completion_cb_ = std::move(cb);
}

void FileUploadManager::set_completion_callback(SimpleCompletionCallback cb) {
    std::lock_guard<std::mutex> lock(callback_mutex_);
    simple_completion_cb_ = std::move(cb);
}

void FileUploadManager::set_failure_callback(FailureCallback cb) {
    std::lock_guard<std::mutex> lock(callback_mutex_);
    failure_cb_ = std::move(cb);
}

void FileUploadManager::set_batch_cancelled_callback(BatchCancelledCallback cb) {
    std::lock_guard<std::mutex> lock(callback_mutex_);
    batch_cancelled_cb_ = std::move(cb);
}

void FileUploadManager::set_low_speed_options(long limit_bytes_per_sec, long time_seconds) {
    low_speed_limit_ = limit_bytes_per_sec;
    low_speed_time_ = time_seconds;
}

void FileUploadManager::worker_loop() {
    while (true) {
        FileUploadTask task;
        {
            std::unique_lock<std::mutex> lock(queue_mutex_);
            queue_cv_.wait(lock, [this]() {
                return stop_flag_.load() || !queue_.empty();
            });
            if (stop_flag_.load() && queue_.empty()) {
                break;
            }
            task = std::move(queue_.front());
            queue_.pop();
            cancel_current_flag_.store(false);
            is_processing_.store(true);
        }
        process_single_task(task);
        is_processing_.store(false);
    }
}

bool FileUploadManager::process_single_task(const FileUploadTask& task) {
    if (cancel_current_flag_.load() || stop_flag_.load()) {
        return false;
    }
    std::unique_ptr<HttpClient> client;
    if (!task.host.empty()) {
        client = std::make_unique<HttpClient>(
            task.host,
            task.port,
            task.pairing_code,
            task.use_https,
            task.insecure,
            task.client_id
        );
    } else {
        std::lock_guard<std::mutex> lock(client_mutex_);
        if (client_) {
            client = std::make_unique<HttpClient>(
                client_->get_host(),
                client_->get_port(),
                client_->get_code(),
                client_->get_use_https(),
                client_->get_insecure(),
                client_->get_client_id()
            );
        }
    }
    if (!client) {
        report_failure(task.file_name, "No active connection to phone");
        return false;
    }
    client->set_low_speed_options(low_speed_limit_, low_speed_time_);

    QFileInfo fi(QString::fromStdString(task.local_path));
    if (!fi.exists()) {
        report_failure(task.file_name, "File not found");
        return false;
    }
    if (!fi.isFile() || !fi.isReadable()) {
        report_failure(task.file_name, "File not readable");
        return false;
    }

    int64_t file_size = fi.size();
    if (task.file_size > 0 && file_size != task.file_size) {
        report_failure(task.file_name, "File size changed");
        return false;
    }

    if (!task.batch_id.empty() && !task.batch_files.empty()) {
        bool needs_offer = false;
        {
            std::lock_guard<std::mutex> lock(queue_mutex_);
            needs_offer = (accepted_batches_.find(task.batch_id) == accepted_batches_.end());
        }
        if (needs_offer) {
            std::string offer_body = "{\"batchId\":\"" + JsonValue::escape_string(task.batch_id) + "\",";
            int64_t total_size = task.batch_total_size > 0 ? task.batch_total_size : file_size;
            offer_body += "\"totalSize\":" + std::to_string(total_size) + ",\"files\":[";
            if (!task.batch_files.empty()) {
                for (size_t i = 0; i < task.batch_files.size(); ++i) {
                    if (i > 0) offer_body += ",";
                    offer_body += "{\"name\":\"" + JsonValue::escape_string(task.batch_files[i].name) + "\",";
                    offer_body += "\"size\":" + std::to_string(task.batch_files[i].size) + "}";
                }
            } else {
                offer_body += "{\"name\":\"" + JsonValue::escape_string(task.file_name) + "\",";
                offer_body += "\"size\":" + std::to_string(file_size) + "}";
            }
            offer_body += "]}";

            HttpResponse resp = client->post_json("/files/offer", offer_body, 10L);
            if (resp.status_code == 409) {
                cancel_all();
                report_failure(task.file_name, "Phone notifications disabled (enable in phone settings)");
                return false;
            }
            if (resp.status_code == 403) {
                cancel_all();
                report_failure(task.file_name, "Uploads disabled on phone");
                return false;
            }
            if (resp.status_code == 404) {
                std::lock_guard<std::mutex> lock(queue_mutex_);
                accepted_batches_.insert(task.batch_id);
            } else if (resp.status_code != 200) {
                cancel_all();
                report_failure(task.file_name, "Transfer offer rejected by phone (HTTP " + std::to_string(resp.status_code) + ")");
                return false;
            } else {
                std::string state;
                JsonValue j = JsonValue::parse(resp.body);
                if (j.has("state")) {
                    state = j.get_string("state");
                }

                if (state == "pending") {
                    auto start_time = std::chrono::steady_clock::now();
                    int poll_interval_ms = 1000;
                    bool approved = false;
                    while (!stop_flag_.load() && !cancel_current_flag_.load()) {
                        auto elapsed_s = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now() - start_time).count();
                        if (elapsed_s >= 120) {
                            cancel_all();
                            report_failure(task.file_name, "Transfer timed out waiting for approval");
                            return false;
                        }

                        std::this_thread::sleep_for(std::chrono::milliseconds(poll_interval_ms));
                        if (poll_interval_ms < 3000) poll_interval_ms += 500;

                        if (stop_flag_.load() || cancel_current_flag_.load()) {
                            return false;
                        }

                        HttpResponse poll_resp = client->get_json("/files/offer/" + task.batch_id, 10L);
                        if (poll_resp.status_code == 200) {
                            JsonValue poll_j = JsonValue::parse(poll_resp.body);
                            std::string current_state = poll_j.get_string("state");
                            if (current_state == "accepted") {
                                approved = true;
                                break;
                            }
                            if (current_state == "declined") {
                                cancel_all();
                                report_failure(task.file_name, "Transfer declined on phone");
                                return false;
                            }
                            if (current_state == "expired") {
                                cancel_all();
                                report_failure(task.file_name, "Transfer timed out waiting for approval");
                                return false;
                            }
                        } else if (poll_resp.status_code == 404 || poll_resp.status_code == 408) {
                            cancel_all();
                            report_failure(task.file_name, "Transfer timed out waiting for approval");
                            return false;
                        }
                    }
                    if (!approved) {
                        return false;
                    }
                } else if (state == "accepted") {
                } else if (state == "declined") {
                    cancel_all();
                    report_failure(task.file_name, "Transfer declined on phone");
                    return false;
                } else {
                    cancel_all();
                    report_failure(task.file_name, "Unknown offer state: " + state);
                    return false;
                }

                {
                    std::lock_guard<std::mutex> lock(queue_mutex_);
                    accepted_batches_.insert(task.batch_id);
                }
            }
        }
    }

    int64_t offset = 0;
    const int max_retries = 5;
    int retry_count = 0;
    bool upload_success = false;

    while (!stop_flag_.load() && !cancel_current_flag_.load() && retry_count < max_retries) {
        int http_code = 0;
        int retry_after = 0;
        std::string resp_body;

        bool ok = client->upload_file(
            "/files/upload",
            task.local_path,
            offset,
            file_size,
            task.upload_id,
            task.file_name,
            task.batch_id,
            task.index,
            task.total,
            [this, &task](int64_t uploaded, int64_t total) {
                report_progress(task, uploaded, total);
            },
            cancel_current_flag_,
            http_code,
            retry_after,
            resp_body
        );

        if (ok) {
            upload_success = true;
            break;
        }

        if (cancel_current_flag_.load() || stop_flag_.load()) {
            return false;
        }

        fi.refresh();
        if (!fi.exists()) {
            report_failure(task.file_name, "File not found");
            return false;
        }
        if (!fi.isFile() || !fi.isReadable()) {
            report_failure(task.file_name, "File not readable");
            return false;
        }
        if (fi.size() != file_size) {
            report_failure(task.file_name, "File size changed");
            return false;
        }

        if (http_code == 410) {
            cancel_all();
            report_batch_cancelled(task.batch_id);
            return false;
        }

        if (http_code == 401) {
            cancel_all();
            report_failure(task.file_name, "Pairing error (HTTP 401)");
            return false;
        }

        if (http_code == 403) {
            cancel_all();
            std::string reason = "Uploads forbidden on phone (HTTP 403)";
            JsonValue j = JsonValue::parse(resp_body);
            if (j.has("error")) {
                std::string err = j.get_string("error");
                if (err == "uploads_disabled") {
                    reason = "Uploads disabled on phone";
                } else if (err == "unsupported_android_version") {
                    reason = "Unsupported Android version";
                }
            }
            report_failure(task.file_name, reason);
            return false;
        }

        if (http_code == 507) {
            cancel_all();
            report_failure(task.file_name, "Phone storage full");
            return false;
        }

        if (http_code == 500) {
            cancel_all();
            std::string reason = "Phone error (HTTP 500)";
            JsonValue j = JsonValue::parse(resp_body);
            if (j.has("error")) {
                std::string err = j.get_string("error");
                if (err == "sink_creation_failed") {
                    reason = "Phone failed to create file";
                } else {
                    reason = "Phone error: " + err;
                }
                if (j.has("detail") && !j.get_string("detail").empty()) {
                    reason += " (" + j.get_string("detail") + ")";
                }
            }
            report_failure(task.file_name, reason);
            return false;
        }

        if (http_code == 409) {
            JsonValue j = JsonValue::parse(resp_body);
            if (j.has("offset")) {
                int64_t server_offset = j.get_int64("offset");
                if (server_offset == file_size) {
                    upload_success = true;
                    break;
                }
                if (server_offset >= 0 && server_offset <= file_size) {
                    offset = server_offset;
                }
            } else {
                offset = 0;
            }
            retry_count++;
            continue;
        }

        if (http_code == 429 || http_code == 503) {
            int wait_sec = (retry_after > 0) ? retry_after : (2 * (retry_count + 1));
            if (wait_sec > 60) {
                wait_sec = 60;
            }
            for (int s = 0; s < wait_sec * 10 && !stop_flag_.load() && !cancel_current_flag_.load(); ++s) {
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }
            if (stop_flag_.load() || cancel_current_flag_.load()) {
                return false;
            }
            int64_t status_offset = 0;
            int64_t status_size = 0;
            int status_code = 0;
            int status_retry = 0;
            bool status_complete = false;
            if (client->get_upload_status(task.upload_id, status_offset, status_size, status_code, status_retry, status_complete)) {
                if (status_code == 200) {
                    if (status_complete || (status_size > 0 && status_offset == status_size) || (status_offset == file_size && (file_size > 0 || status_size == 0))) {
                        upload_success = true;
                        break;
                    }
                    if (status_offset >= 0 && status_offset <= file_size) {
                        offset = status_offset;
                    }
                } else if (status_code == 404) {
                    offset = 0;
                } else if (status_code == 410) {
                    cancel_all();
                    report_batch_cancelled(task.batch_id);
                    return false;
                }
            }
            retry_count++;
            continue;
        }

        retry_count++;
        int backoff_ms = (1 << retry_count) * 500;
        if (backoff_ms > 8000) {
            backoff_ms = 8000;
        }
        for (int m = 0; m < backoff_ms / 100 && !stop_flag_.load() && !cancel_current_flag_.load(); ++m) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        if (stop_flag_.load() || cancel_current_flag_.load()) {
            return false;
        }
        int64_t status_offset = 0;
        int64_t status_size = 0;
        int status_code = 0;
        int status_retry = 0;
        bool status_complete = false;
        if (client->get_upload_status(task.upload_id, status_offset, status_size, status_code, status_retry, status_complete)) {
            if (status_code == 200) {
                if (status_complete || (status_size > 0 && status_offset == status_size) || (status_offset == file_size && (file_size > 0 || status_size == 0))) {
                    upload_success = true;
                    break;
                }
                if (status_offset >= 0 && status_offset <= file_size) {
                    offset = status_offset;
                }
            } else if (status_code == 404) {
                offset = 0;
            } else if (status_code == 410) {
                cancel_all();
                report_batch_cancelled(task.batch_id);
                return false;
            }
        }
    }

    if (upload_success) {
        handle_file_completed(task);
        return true;
    }

    if (!cancel_current_flag_.load() && !stop_flag_.load()) {
        report_failure(task.file_name, "Upload failed after retries");
    }
    return false;
}

void FileUploadManager::report_progress(const FileUploadTask& task, int64_t uploaded, int64_t total) {
    int64_t now = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
    if (now - last_progress_report_ms_ < 100L && uploaded < total) {
        return;
    }
    last_progress_report_ms_ = now;

    ProgressCallback cb;
    {
        std::lock_guard<std::mutex> lock(callback_mutex_);
        cb = progress_cb_;
    }
    if (cb) {
        cb(task.batch_id, task.index, (task.total > 0 ? task.total : 1), uploaded, total, task.file_name);
    }
}

void FileUploadManager::report_failure(const std::string& file_name, const std::string& reason) {
    FailureCallback cb;
    {
        std::lock_guard<std::mutex> lock(callback_mutex_);
        cb = failure_cb_;
    }
    if (cb) {
        cb(file_name, reason);
    }
}

void FileUploadManager::report_batch_cancelled(const std::string& batch_id) {
    BatchCancelledCallback cb;
    {
        std::lock_guard<std::mutex> lock(callback_mutex_);
        cb = batch_cancelled_cb_;
    }
    if (cb) {
        cb(batch_id);
    }
}

void FileUploadManager::handle_file_completed(const FileUploadTask& task) {
    if (task.batch_id != current_batch_id_) {
        current_batch_id_ = task.batch_id;
        batch_total_ = (task.total > 0) ? task.total : 1;
        batch_completed_ = 0;
        current_batch_results_.clear();
    }
    batch_completed_++;
    FileUploadResult res{task.file_name, task.local_path, task.file_size};
    current_batch_results_.push_back(res);

    bool batch_done = false;
    if (task.total > 0 && batch_completed_ >= task.total) {
        batch_done = true;
    } else {
        std::lock_guard<std::mutex> lock(queue_mutex_);
        if (queue_.empty() && (task.index >= task.total || task.total <= 1)) {
            batch_done = true;
        }
    }

    if (batch_done) {
        CompletionCallback cb;
        SimpleCompletionCallback simple_cb;
        {
            std::lock_guard<std::mutex> lock(callback_mutex_);
            cb = completion_cb_;
            simple_cb = simple_completion_cb_;
        }
        if (cb) {
            cb(task.batch_id, current_batch_results_);
        }
        if (simple_cb) {
            simple_cb(task.batch_id, batch_completed_);
        }
        current_batch_results_.clear();
        current_batch_id_.clear();
        batch_completed_ = 0;
        batch_total_ = 0;
        {
            std::lock_guard<std::mutex> lock(queue_mutex_);
            accepted_batches_.erase(task.batch_id);
        }
    }
}

}
