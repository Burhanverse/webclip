#pragma once

#include <string>
#include <vector>
#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <functional>
#include <memory>
#include <QString>

namespace webclip {

class HttpClient;

struct FileOfferTask {
    std::string token;
    std::string name;
    int64_t size = 0;
    std::string mime_type;
    std::string batch_id;
    int index = 0;
    int total = 0;
    std::string client_id;
    std::string host;
    int port = 8080;
    bool use_https = false;
    bool insecure = false;
    std::string pairing_code;
};

struct FileDownloadResult {
    std::string name;
    std::string local_path;
    int64_t size = 0;
    std::string mime_type;
};

class FileDownloadManager {
public:
    using ProgressCallback = std::function<void(const std::string& batch_id, int index, int total, int64_t downloaded_bytes, int64_t total_bytes, const std::string& file_name)>;
    using CompletionCallback = std::function<void(const std::string& batch_id, const std::vector<FileDownloadResult>& files)>;
    using SimpleCompletionCallback = std::function<void(const std::string& batch_id, int total_files)>;
    using FailureCallback = std::function<void(const std::string& file_name, const std::string& reason)>;

    FileDownloadManager();
    explicit FileDownloadManager(std::shared_ptr<HttpClient> client);
    ~FileDownloadManager();

    void set_client(std::shared_ptr<HttpClient> client);
    void enqueue_task(FileOfferTask task);
    void cancel_current();
    void cancel_all();
    void stop();
    bool is_busy() const;

    void set_progress_callback(ProgressCallback cb);
    void set_completion_callback(CompletionCallback cb);
    void set_completion_callback(SimpleCompletionCallback cb);
    void set_failure_callback(FailureCallback cb);
    void set_low_speed_options(long limit_bytes_per_sec, long time_seconds);

    static QString sanitize_file_name(const QString& raw_name);
    static QString resolve_unique_path(const QString& target_dir, const QString& sanitized_name);
    static bool check_storage_space(const QString& target_dir, int64_t needed_bytes, QString* error_msg = nullptr);
    static QString get_download_directory();
    bool process_single_task(const FileOfferTask& task);

private:
    void worker_loop();
    void report_progress(const FileOfferTask& task, const QString& clean_name, int64_t downloaded, int64_t total);
    void report_failure(const std::string& file_name, const std::string& reason);
    void handle_file_completed(const FileOfferTask& task, const QString& clean_name);

    std::shared_ptr<HttpClient> client_;
    std::mutex client_mutex_;

    std::queue<FileOfferTask> queue_;
    mutable std::mutex queue_mutex_;
    std::condition_variable queue_cv_;

    std::atomic<bool> stop_flag_{false};
    std::atomic<bool> cancel_current_flag_{false};
    std::atomic<bool> is_processing_{false};
    std::thread worker_thread_;

    ProgressCallback progress_cb_;
    CompletionCallback completion_cb_;
    SimpleCompletionCallback simple_completion_cb_;
    FailureCallback failure_cb_;
    std::mutex callback_mutex_;

    std::string current_batch_id_;
    int batch_total_ = 0;
    int batch_completed_ = 0;
    std::vector<FileDownloadResult> current_batch_results_;
    int64_t last_progress_report_ms_ = 0;
    long low_speed_limit_ = 1024L;
    long low_speed_time_ = 30L;
};

}
