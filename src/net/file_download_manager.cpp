#include "file_download_manager.hpp"
#include "http_client.hpp"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QStorageInfo>
#include <QSettings>
#include <QStandardPaths>
#include <chrono>

namespace webclip {

FileDownloadManager::FileDownloadManager()
    : FileDownloadManager(nullptr) {
}

FileDownloadManager::FileDownloadManager(std::shared_ptr<HttpClient> client)
    : client_(std::move(client)) {
    worker_thread_ = std::thread(&FileDownloadManager::worker_loop, this);
}

FileDownloadManager::~FileDownloadManager() {
    stop();
}

void FileDownloadManager::set_client(std::shared_ptr<HttpClient> client) {
    std::lock_guard<std::mutex> lock(client_mutex_);
    client_ = std::move(client);
}

void FileDownloadManager::enqueue_task(FileOfferTask task) {
    {
        std::lock_guard<std::mutex> lock(queue_mutex_);
        queue_.push(std::move(task));
    }
    queue_cv_.notify_one();
}

void FileDownloadManager::cancel_current() {
    cancel_current_flag_.store(true);
}

void FileDownloadManager::cancel_all() {
    std::lock_guard<std::mutex> lock(queue_mutex_);
    cancel_current_flag_.store(true);
    std::queue<FileOfferTask> empty;
    std::swap(queue_, empty);
}

void FileDownloadManager::stop() {
    stop_flag_.store(true);
    cancel_current_flag_.store(true);
    queue_cv_.notify_all();
    if (worker_thread_.joinable()) {
        worker_thread_.join();
    }
}

bool FileDownloadManager::is_busy() const {
    std::lock_guard<std::mutex> lock(queue_mutex_);
    return is_processing_.load() || !queue_.empty();
}

void FileDownloadManager::set_progress_callback(ProgressCallback cb) {
    std::lock_guard<std::mutex> lock(callback_mutex_);
    progress_cb_ = std::move(cb);
}

void FileDownloadManager::set_completion_callback(CompletionCallback cb) {
    std::lock_guard<std::mutex> lock(callback_mutex_);
    completion_cb_ = std::move(cb);
}

void FileDownloadManager::set_completion_callback(SimpleCompletionCallback cb) {
    std::lock_guard<std::mutex> lock(callback_mutex_);
    simple_completion_cb_ = std::move(cb);
}

void FileDownloadManager::set_failure_callback(FailureCallback cb) {
    std::lock_guard<std::mutex> lock(callback_mutex_);
    failure_cb_ = std::move(cb);
}

void FileDownloadManager::set_low_speed_options(long limit_bytes_per_sec, long time_seconds) {
    low_speed_limit_ = limit_bytes_per_sec;
    low_speed_time_ = time_seconds;
}

void FileDownloadManager::worker_loop() {
    while (true) {
        FileOfferTask task;
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

bool FileDownloadManager::process_single_task(const FileOfferTask& task) {
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
        report_failure(task.name, "No active connection to phone");
        return false;
    }
    client->set_low_speed_options(low_speed_limit_, low_speed_time_);

    QString download_dir = get_download_directory();
    QDir().mkpath(download_dir);

    QString space_err;
    if (!check_storage_space(download_dir, task.size, &space_err)) {
        report_failure(task.name, space_err.toStdString());
        return false;
    }

    QString clean_name = sanitize_file_name(QString::fromStdString(task.name));
    QString target_path = resolve_unique_path(download_dir, clean_name);
    QString part_path = target_path + QStringLiteral(".part");

    QFileInfo part_info(part_path);
    int64_t offset = 0;
    if (part_info.exists()) {
        int64_t part_size = part_info.size();
        if (part_size == task.size) {
            if (QFile::rename(part_path, target_path)) {
                handle_file_completed(task, target_path);
                return true;
            }
        } else if (part_size > task.size) {
            QFile::remove(part_path);
            offset = 0;
        } else {
            offset = part_size;
        }
    }

    const int max_retries = 5;
    int retry_count = 0;
    bool download_success = false;

    while (!stop_flag_.load() && !cancel_current_flag_.load() && retry_count < max_retries) {
        int http_code = 0;
        int retry_after = 0;

        bool ok = client->download_file(
            "/files/" + task.token,
            part_path.toStdString(),
            offset,
            [this, &task, &clean_name](int64_t downloaded, int64_t total) {
                report_progress(task, clean_name, downloaded, total);
            },
            cancel_current_flag_,
            http_code,
            retry_after
        );

        if (ok) {
            QFileInfo fi(part_path);
            if (fi.size() == task.size) {
                if (QFile::rename(part_path, target_path)) {
                    download_success = true;
                    break;
                } else {
                    report_failure(clean_name.toStdString(), "Failed to rename .part file");
                    return false;
                }
            }
        }

        if (cancel_current_flag_.load() || stop_flag_.load()) {
            return false;
        }

        if (http_code == 404 || http_code == 410) {
            QFile::remove(part_path);
            report_failure(clean_name.toStdString(), "Offer expired or not found (HTTP " + std::to_string(http_code) + ")");
            return false;
        }

        if (http_code == 416) {
            QFileInfo fi(part_path);
            if (fi.size() == task.size) {
                if (QFile::rename(part_path, target_path)) {
                    download_success = true;
                    break;
                }
            }
            QFile::remove(part_path);
            offset = 0;
            retry_count++;
            continue;
        }

        if (http_code == 503) {
            int wait_sec = (retry_after > 0) ? retry_after : (2 * (retry_count + 1));
            if (wait_sec > 60) wait_sec = 60;
            for (int s = 0; s < wait_sec * 10 && !stop_flag_.load() && !cancel_current_flag_.load(); ++s) {
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }
            retry_count++;
            QFileInfo fi(part_path);
            offset = fi.exists() ? fi.size() : 0;
            continue;
        }

        retry_count++;
        int backoff_ms = (1 << retry_count) * 500;
        if (backoff_ms > 8000) backoff_ms = 8000;
        for (int m = 0; m < backoff_ms / 100 && !stop_flag_.load() && !cancel_current_flag_.load(); ++m) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        QFileInfo fi(part_path);
        offset = fi.exists() ? fi.size() : 0;
    }

    if (download_success) {
        handle_file_completed(task, target_path);
        return true;
    }

    if (!cancel_current_flag_.load() && !stop_flag_.load()) {
        report_failure(clean_name.toStdString(), "Download failed after retries");
    }
    return false;
}

void FileDownloadManager::report_progress(const FileOfferTask& task, const QString& clean_name, int64_t downloaded, int64_t total) {
    int64_t now = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
    if (now - last_progress_report_ms_ < 100L && downloaded < total) {
        return;
    }
    last_progress_report_ms_ = now;

    ProgressCallback cb;
    {
        std::lock_guard<std::mutex> lock(callback_mutex_);
        cb = progress_cb_;
    }
    if (cb) {
        cb(task.batch_id, task.index, (task.total > 0 ? task.total : 1), downloaded, total, clean_name.toStdString());
    }
}

void FileDownloadManager::report_failure(const std::string& file_name, const std::string& reason) {
    FailureCallback cb;
    {
        std::lock_guard<std::mutex> lock(callback_mutex_);
        cb = failure_cb_;
    }
    if (cb) {
        cb(file_name, reason);
    }
}

void FileDownloadManager::handle_file_completed(const FileOfferTask& task, const QString& local_path) {
    if (task.batch_id != current_batch_id_) {
        current_batch_id_ = task.batch_id;
        batch_total_ = (task.total > 0) ? task.total : 1;
        batch_completed_ = 0;
        current_batch_results_.clear();
    }
    batch_completed_++;
    FileDownloadResult res{task.name, local_path.toStdString(), task.size, task.mime_type};
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
    }
}

QString FileDownloadManager::sanitize_file_name(const QString& raw_name) {
    if (raw_name.isEmpty()) {
        return QStringLiteral("download");
    }

    QString s = raw_name.trimmed();
    while (s.endsWith(QLatin1Char('.')) || s.endsWith(QLatin1Char(' '))) {
        s.chop(1);
    }
    if (s.isEmpty()) {
        return QStringLiteral("download");
    }

    s.replace(QLatin1Char('/'), QLatin1Char('_'));
    s.replace(QLatin1Char('\\'), QLatin1Char('_'));

    while (s.contains(QLatin1String(".."))) {
        s.replace(QLatin1String(".."), QLatin1String("_"));
    }

    QString filtered;
    filtered.reserve(s.size());
    for (int i = 0; i < s.size(); ++i) {
        QChar ch = s.at(i);
        ushort u = ch.unicode();
        if (u < 32 || u == 127) {
            continue;
        }
        if (ch == QLatin1Char('<') || ch == QLatin1Char('>') || ch == QLatin1Char(':') ||
            ch == QLatin1Char('"') || ch == QLatin1Char('|') || ch == QLatin1Char('?') ||
            ch == QLatin1Char('*')) {
            continue;
        }
        filtered.append(ch);
    }
    s = filtered;

    while (s.endsWith(QLatin1Char('.')) || s.endsWith(QLatin1Char(' '))) {
        s.chop(1);
    }
    while (s.startsWith(QLatin1Char(' '))) {
        s.remove(0, 1);
    }

    bool all_placeholder = true;
    for (int i = 0; i < s.size(); ++i) {
        QChar ch = s.at(i);
        if (ch != QLatin1Char('_') && ch != QLatin1Char('.') && ch != QLatin1Char(' ')) {
            all_placeholder = false;
            break;
        }
    }
    if (all_placeholder || s.isEmpty()) {
        return QStringLiteral("download");
    }

    int dot_idx = s.lastIndexOf(QLatin1Char('.'));
    QString stem = (dot_idx > 0) ? s.left(dot_idx) : s;
    QString ext = (dot_idx > 0) ? s.mid(dot_idx) : QString();

    int first_dot = s.indexOf(QLatin1Char('.'));
    QString base_prefix = (first_dot > 0) ? s.left(first_dot) : stem;

    QString upper_stem = stem.toUpper();
    QString upper_base = base_prefix.toUpper();
    static const QStringList reserved = {
        QStringLiteral("CON"), QStringLiteral("PRN"), QStringLiteral("AUX"), QStringLiteral("NUL"),
        QStringLiteral("COM1"), QStringLiteral("COM2"), QStringLiteral("COM3"), QStringLiteral("COM4"),
        QStringLiteral("COM5"), QStringLiteral("COM6"), QStringLiteral("COM7"), QStringLiteral("COM8"),
        QStringLiteral("COM9"), QStringLiteral("LPT1"), QStringLiteral("LPT2"), QStringLiteral("LPT3"),
        QStringLiteral("LPT4"), QStringLiteral("LPT5"), QStringLiteral("LPT6"), QStringLiteral("LPT7"),
        QStringLiteral("LPT8"), QStringLiteral("LPT9")
    };
    if (reserved.contains(upper_stem) || reserved.contains(upper_base)) {
        stem = QStringLiteral("_") + stem;
        s = QStringLiteral("_") + s;
    }

    QByteArray ext_utf8 = ext.toUtf8();
    QByteArray stem_utf8 = stem.toUtf8();
    const int max_bytes = 255;

    if (ext_utf8.size() + stem_utf8.size() > max_bytes) {
        if (ext_utf8.size() < max_bytes) {
            int allowed_stem_bytes = max_bytes - ext_utf8.size();
            while (!stem.isEmpty() && stem.toUtf8().size() > allowed_stem_bytes) {
                stem.chop(1);
            }
            s = stem + ext;
        } else {
            while (!s.isEmpty() && s.toUtf8().size() > max_bytes) {
                s.chop(1);
            }
        }
    }

    if (s.isEmpty()) {
        return QStringLiteral("download");
    }

    return s;
}

QString FileDownloadManager::resolve_unique_path(const QString& target_dir, const QString& sanitized_name) {
    QDir dir(target_dir);
    QString base_path = dir.filePath(sanitized_name);
    if (!QFile::exists(base_path)) {
        return base_path;
    }

    int dot_idx = sanitized_name.lastIndexOf(QLatin1Char('.'));
    QString stem = (dot_idx > 0) ? sanitized_name.left(dot_idx) : sanitized_name;
    QString ext = (dot_idx > 0) ? sanitized_name.mid(dot_idx) : QString();

    for (int i = 1; i <= 99999; ++i) {
        QString candidate_name = QStringLiteral("%1 (%2)%3").arg(stem).arg(i).arg(ext);
        QString candidate_path = dir.filePath(candidate_name);
        if (!QFile::exists(candidate_path)) {
            return candidate_path;
        }
    }
    return base_path;
}

bool FileDownloadManager::check_storage_space(const QString& target_dir, int64_t needed_bytes, QString* error_msg) {
    QStorageInfo storage(target_dir);
    if (!storage.isValid() || !storage.isReady()) {
        storage.setPath(QDir::homePath());
    }
    qint64 available = storage.bytesAvailable();
    const qint64 safety_buffer = 50LL * 1024LL * 1024LL;
    if (available < static_cast<qint64>(needed_bytes) + safety_buffer) {
        if (error_msg) {
            *error_msg = QStringLiteral("Insufficient disk space on target device");
        }
        return false;
    }
    return true;
}

QString FileDownloadManager::get_download_directory() {
    QSettings settings(QStringLiteral("Burhanverse"), QStringLiteral("WebClip"));
    QString dir = settings.value(QStringLiteral("downloadDirectory"), QString()).toString();
    if (dir.isEmpty()) {
        QString dl_loc = QStandardPaths::writableLocation(QStandardPaths::DownloadLocation);
        if (dl_loc.isEmpty()) {
            dl_loc = QDir::homePath() + QStringLiteral("/Downloads");
        }
        dir = dl_loc + QStringLiteral("/WebClip");
    }
    return dir;
}

}
