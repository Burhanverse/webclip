#include <QtTest/QtTest>
#include <QTemporaryDir>
#include <QTemporaryFile>
#include <QFile>
#include <QFileInfo>
#include <QSettings>
#include <csignal>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <thread>
#include <atomic>
#include <functional>
#include <string>
#include <sstream>
#include <map>
#include <vector>
#include <algorithm>
#include "sync/sync_manager.hpp"
#include "clipboard/clipboard.hpp"
#include "net/file_download_manager.hpp"
#include "net/file_upload_manager.hpp"
#include "net/http_client.hpp"
#include "util/json.hpp"

class FakeHttpServer {
public:
    using Handler = std::function<void(int client_fd, const std::string& method, const std::string& path, const std::map<std::string, std::string>& headers)>;
    using BodyHandler = std::function<void(int client_fd, const std::string& method, const std::string& path, const std::map<std::string, std::string>& headers, const std::string& body)>;
    using RawHandler = std::function<void(int client_fd, const std::string& method, const std::string& path, const std::map<std::string, std::string>& headers, const std::string& initial_body)>;

    FakeHttpServer() = default;
    ~FakeHttpServer() {
        stop();
    }

    bool start(Handler handler) {
        handler_ = std::move(handler);
        return start_server();
    }

    bool start_with_body(BodyHandler handler) {
        body_handler_ = std::move(handler);
        return start_server();
    }

    bool start_raw(RawHandler handler) {
        raw_handler_ = std::move(handler);
        return start_server();
    }

    bool start_server() {
        server_fd_ = ::socket(AF_INET, SOCK_STREAM, 0);
        if (server_fd_ < 0) {
            return false;
        }

        int opt = 1;
        ::setsockopt(server_fd_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

        struct sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        addr.sin_port = 0;

        if (::bind(server_fd_, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr)) < 0) {
            ::close(server_fd_);
            server_fd_ = -1;
            return false;
        }

        socklen_t len = sizeof(addr);
        if (::getsockname(server_fd_, reinterpret_cast<struct sockaddr*>(&addr), &len) < 0) {
            ::close(server_fd_);
            server_fd_ = -1;
            return false;
        }
        port_ = ntohs(addr.sin_port);

        if (::listen(server_fd_, 16) < 0) {
            ::close(server_fd_);
            server_fd_ = -1;
            return false;
        }

        running_.store(true);
        accept_thread_ = std::thread([this]() {
            while (running_.load()) {
                struct sockaddr_in client_addr{};
                socklen_t client_len = sizeof(client_addr);
                int client_fd = ::accept(server_fd_, reinterpret_cast<struct sockaddr*>(&client_addr), &client_len);
                if (client_fd < 0) {
                    if (!running_.load()) {
                        break;
                    }
                    continue;
                }
                std::thread([this, client_fd]() {
                    handle_client(client_fd);
                }).detach();
            }
        });
        return true;
    }

    void stop() {
        if (!running_.exchange(false)) {
            return;
        }
        if (server_fd_ >= 0) {
            ::shutdown(server_fd_, SHUT_RDWR);
            ::close(server_fd_);
            server_fd_ = -1;
        }
        if (accept_thread_.joinable()) {
            accept_thread_.join();
        }
    }

    int port() const {
        return port_;
    }

    static void send_response(int fd, int status, const std::string& status_text, const std::map<std::string, std::string>& headers, const std::string& body) {
        std::string resp = "HTTP/1.1 " + std::to_string(status) + " " + status_text + "\r\n";
        for (const auto& [k, v] : headers) {
            resp += k + ": " + v + "\r\n";
        }
        resp += "Content-Length: " + std::to_string(body.size()) + "\r\n\r\n";
        resp += body;
#if defined(MSG_NOSIGNAL)
        ::send(fd, resp.data(), resp.size(), MSG_NOSIGNAL);
#else
        ::send(fd, resp.data(), resp.size(), 0);
#endif
        ::close(fd);
    }

private:
    void handle_client(int client_fd) {
        std::string raw_req;
        char buf[2048];
        while (raw_req.find("\r\n\r\n") == std::string::npos) {
            ssize_t n = ::recv(client_fd, buf, sizeof(buf), 0);
            if (n <= 0) {
                break;
            }
            raw_req.append(buf, n);
        }

        std::string method;
        std::string path;
        std::map<std::string, std::string> headers;

        size_t header_end = raw_req.find("\r\n\r\n");
        if (header_end != std::string::npos) {
            std::string header_part = raw_req.substr(0, header_end);
            std::istringstream iss(header_part);
            std::string line;
            if (std::getline(iss, line)) {
                if (!line.empty() && line.back() == '\r') {
                    line.pop_back();
                }
                std::istringstream req_line(line);
                req_line >> method >> path;
                size_t q = path.find('?');
                if (q != std::string::npos) {
                    path = path.substr(0, q);
                }
            }
            while (std::getline(iss, line)) {
                if (!line.empty() && line.back() == '\r') {
                    line.pop_back();
                }
                size_t colon = line.find(':');
                if (colon != std::string::npos) {
                    std::string key = line.substr(0, colon);
                    std::string val = line.substr(colon + 1);
                    while (!val.empty() && val.front() == ' ') {
                        val.erase(0, 1);
                    }
                    std::string lower_key = key;
                    std::transform(lower_key.begin(), lower_key.end(), lower_key.begin(), ::tolower);
                    headers[lower_key] = val;
                }
            }
        }

        std::string initial_body = (header_end != std::string::npos) ? raw_req.substr(header_end + 4) : "";

        if (body_handler_) {
            std::string body = initial_body;
            auto it = headers.find("content-length");
            if (it != headers.end()) {
                size_t cl = 0;
                try {
                    cl = std::stoul(it->second);
                } catch (...) {}
                while (body.size() < cl) {
                    ssize_t n = ::recv(client_fd, buf, sizeof(buf), 0);
                    if (n <= 0) break;
                    body.append(buf, n);
                }
            }
            body_handler_(client_fd, method, path, headers, body);
        } else if (raw_handler_) {
            raw_handler_(client_fd, method, path, headers, initial_body);
        } else if (handler_) {
            handler_(client_fd, method, path, headers);
        } else {
            ::close(client_fd);
        }
    }

    int server_fd_ = -1;
    int port_ = 0;
    std::atomic<bool> running_{false};
    std::thread accept_thread_;
    Handler handler_;
    BodyHandler body_handler_;
    RawHandler raw_handler_;
};

class TestFileTransfer : public QObject {
    Q_OBJECT

private slots:
    void initTestCase();
    void testSanitizeFileNameBasics();
    void testSanitizeFileNameWindowsReserved();
    void testSanitizeFileNameLengthCap();
    void testSanitizeFileNameUnicode();
    void testResolveUniquePath();
    void testStorageSpaceCheck();
    void testFileOfferEventParsing();
    void testPartFileHandling();

    void testFullDownload();
    void testResumeFromPartViaRange206();
    void testServerIgnoringRange200TruncatesAndRestarts();
    void test416WithPartEqualsOfferedSizeRenames();
    void test416WithLargerPartDeletesAndRestarts();
    void test503WithRetryAfterThenSuccess();
    void test404Or410NoRetry();
    void testConnectionDroppedMidBodyKeepsPartAndResumes();
    void testStalledBodyLowSpeedAbort();
    void testCancelMidDownload();
    void testFinalSizeMismatchNeverRenamed();
    void testCollisionNaming();

    void testUploadFull();
    void testUploadResumeViaStatusRoute();
    void testUpload409AdoptOffset();
    void testUpload507StorageFull();
    void testUpload500SinkCreationFailed();
    void testUpload503WithRetryAfter();
    void testUpload401PairingError();
    void testUpload403UploadsDisabled();
    void testUploadDropMidBodyAndResume();
    void testUploadStalledBodyLowSpeedAbort();
    void testUploadCancelMidUpload();
    void testUploadNoExpectHeaderSent();
    void testUpload410BatchCancelledStopsBatchImmediately();
    void testUpload429WithRetryAfter();
    void testUploadSourceFileDisappearsOrChangesSizeFailsCleanlyAndBatchContinues();
    void testTransferManagersIsBusyAndCancelAll();
    void testUploadIdempotencyCompleteStatusRouteReturnsSuccessWithoutResending();
    void testUploadZeroByteFile();
    void testDownloadZeroByteFile();
    void testUploadOfferPendingThenAcceptedProceedsToUpload();
    void testUploadOffer409NotificationsDisabledFailsImmediately();
    void testUploadOfferDeclinedAborts();
    void testUploadOfferTimeoutAborts();
    void testUploadOfferPolledDeclinedAborts();
    void testIncomingOfferDebounceGrouping();
    void testSseImageClipWithHasImageFetchesImageAndDoesNotAddText();
    void testSseImageClipFetchFailureLeavesClipboardClean();
    void testClipboardTextWithImagePathPushesImageNotText();
    void testClipboardTextWithNonImageOrMissingFilePushesText();
};

void TestFileTransfer::initTestCase() {
#if defined(SIGPIPE)
    ::signal(SIGPIPE, SIG_IGN);
#endif
}

void TestFileTransfer::testSanitizeFileNameBasics() {
    QCOMPARE(webclip::FileDownloadManager::sanitize_file_name(""), QString("download"));
    QCOMPARE(webclip::FileDownloadManager::sanitize_file_name("   "), QString("download"));
    QCOMPARE(webclip::FileDownloadManager::sanitize_file_name("..."), QString("download"));

    QCOMPARE(webclip::FileDownloadManager::sanitize_file_name("foo/bar.txt"), QString("foo_bar.txt"));
    QCOMPARE(webclip::FileDownloadManager::sanitize_file_name("foo\\bar.txt"), QString("foo_bar.txt"));
    QCOMPARE(webclip::FileDownloadManager::sanitize_file_name("../../etc/passwd"), QString("____etc_passwd"));

    QString withControls = QString("hello") + QChar(1) + QChar(31) + QString("world.txt");
    QCOMPARE(webclip::FileDownloadManager::sanitize_file_name(withControls), QString("helloworld.txt"));

    QCOMPARE(webclip::FileDownloadManager::sanitize_file_name("file<1>:2\"3|4?5*6.png"), QString("file123456.png"));
    QCOMPARE(webclip::FileDownloadManager::sanitize_file_name("   my_document.pdf...   "), QString("my_document.pdf"));
}

void TestFileTransfer::testSanitizeFileNameWindowsReserved() {
    QCOMPARE(webclip::FileDownloadManager::sanitize_file_name("con"), QString("_con"));
    QCOMPARE(webclip::FileDownloadManager::sanitize_file_name("CON.txt"), QString("_CON.txt"));
    QCOMPARE(webclip::FileDownloadManager::sanitize_file_name("prn.pdf"), QString("_prn.pdf"));
    QCOMPARE(webclip::FileDownloadManager::sanitize_file_name("aux.tar.gz"), QString("_aux.tar.gz"));
    QCOMPARE(webclip::FileDownloadManager::sanitize_file_name("NUL.dat"), QString("_NUL.dat"));
    QCOMPARE(webclip::FileDownloadManager::sanitize_file_name("COM1"), QString("_COM1"));
    QCOMPARE(webclip::FileDownloadManager::sanitize_file_name("lpt9.log"), QString("_lpt9.log"));
    QCOMPARE(webclip::FileDownloadManager::sanitize_file_name("normal_file.txt"), QString("normal_file.txt"));
}

void TestFileTransfer::testSanitizeFileNameLengthCap() {
    QString longName(300, 'a');
    longName += ".txt";
    QString sanitized = webclip::FileDownloadManager::sanitize_file_name(longName);
    QVERIFY(sanitized.toUtf8().size() <= 255);
    QVERIFY(sanitized.endsWith(".txt"));
    QCOMPARE(sanitized.size(), 255);
}

void TestFileTransfer::testSanitizeFileNameUnicode() {
    QString unicodeName = QString::fromUtf8("🚀_тест_中文_ملف.png");
    QString sanitized = webclip::FileDownloadManager::sanitize_file_name(unicodeName);
    QCOMPARE(sanitized, unicodeName);
}

void TestFileTransfer::testResolveUniquePath() {
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    QString firstPath = webclip::FileDownloadManager::resolve_unique_path(tempDir.path(), "test.txt");
    QCOMPARE(firstPath, tempDir.path() + "/test.txt");

    QFile file1(firstPath);
    QVERIFY(file1.open(QIODevice::WriteOnly));
    file1.write("data");
    file1.close();

    QString secondPath = webclip::FileDownloadManager::resolve_unique_path(tempDir.path(), "test.txt");
    QCOMPARE(secondPath, tempDir.path() + "/test (1).txt");

    QFile file2(secondPath);
    QVERIFY(file2.open(QIODevice::WriteOnly));
    file2.write("data");
    file2.close();

    QString thirdPath = webclip::FileDownloadManager::resolve_unique_path(tempDir.path(), "test.txt");
    QCOMPARE(thirdPath, tempDir.path() + "/test (2).txt");
}

void TestFileTransfer::testStorageSpaceCheck() {
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    QString errorMsg;
    bool hasSpace = webclip::FileDownloadManager::check_storage_space(tempDir.path(), 1024, &errorMsg);
    QVERIFY(hasSpace);
    QVERIFY(errorMsg.isEmpty());

    int64_t petabyte = 1024LL * 1024LL * 1024LL * 1024LL * 1024LL;
    bool hugeSpace = webclip::FileDownloadManager::check_storage_space(tempDir.path(), petabyte, &errorMsg);
    QVERIFY(!hugeSpace);
    QVERIFY(!errorMsg.isEmpty());
}

void TestFileTransfer::testFileOfferEventParsing() {
    std::string sseData = "{\"token\":\"abcdef0123456789abcdef0123456789\",\"name\":\"photo.jpg\",\"size\":1048576,\"mimeType\":\"image/jpeg\",\"batchId\":\"b123\",\"index\":1,\"total\":1,\"clientId\":\"\"}";
    webclip::JsonValue parsed = webclip::JsonValue::parse(sseData);

    QCOMPARE(QString::fromStdString(parsed.get_string("token")), QString("abcdef0123456789abcdef0123456789"));
    QCOMPARE(QString::fromStdString(parsed.get_string("name")), QString("photo.jpg"));
    QCOMPARE(parsed.get_int64("size"), static_cast<int64_t>(1048576));
    QCOMPARE(QString::fromStdString(parsed.get_string("batchId")), QString("b123"));
    QCOMPARE(static_cast<int>(parsed.get_int64("index")), 1);
    QCOMPARE(static_cast<int>(parsed.get_int64("total")), 1);
    QCOMPARE(QString::fromStdString(parsed.get_string("clientId")), QString(""));
}

void TestFileTransfer::testPartFileHandling() {
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    QString targetPath = tempDir.path() + "/sample.bin";
    QString partPath = targetPath + ".part";

    QFile part(partPath);
    QVERIFY(part.open(QIODevice::WriteOnly));
    QByteArray sampleData(100, 'x');
    part.write(sampleData);
    part.close();

    QFileInfo info(partPath);
    QCOMPARE(info.size(), 100);

    int64_t offeredSize = 100;
    if (info.size() == offeredSize) {
        QVERIFY(QFile::rename(partPath, targetPath));
    }
    QVERIFY(!QFile::exists(partPath));
    QVERIFY(QFile::exists(targetPath));
    QCOMPARE(QFileInfo(targetPath).size(), 100);
}

void TestFileTransfer::testFullDownload() {
    FakeHttpServer server;
    QVERIFY(server.start([](int fd, const std::string&, const std::string&, const auto&) {
        FakeHttpServer::send_response(fd, 200, "OK", {{"Content-Type", "application/octet-stream"}}, std::string(100, 'A'));
    }));

    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());
    QSettings settings("Burhanverse", "WebClip");
    settings.setValue("downloadDirectory", tempDir.path());

    webclip::FileOfferTask task;
    task.token = "full1";
    task.name = "full.bin";
    task.size = 100;
    task.host = "127.0.0.1";
    task.port = server.port();
    task.use_https = false;
    task.insecure = true;
    task.pairing_code = "1234";

    webclip::FileDownloadManager manager;
    bool ok = manager.process_single_task(task);
    QVERIFY(ok);

    QString finalPath = tempDir.path() + "/full.bin";
    QVERIFY(QFile::exists(finalPath));
    QCOMPARE(QFileInfo(finalPath).size(), 100);
    QVERIFY(!QFile::exists(finalPath + ".part"));

    settings.remove("downloadDirectory");
}

void TestFileTransfer::testResumeFromPartViaRange206() {
    FakeHttpServer server;
    QVERIFY(server.start([](int fd, const std::string&, const std::string&, const auto& headers) {
        auto it = headers.find("range");
        if (it != headers.end() && it->second == "bytes=40-") {
            FakeHttpServer::send_response(fd, 206, "Partial Content",
                {{"Content-Range", "bytes 40-99/100"}, {"Content-Type", "application/octet-stream"}},
                std::string(60, 'B'));
        } else {
            FakeHttpServer::send_response(fd, 400, "Bad Request", {}, "Expected Range");
        }
    }));

    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());
    QSettings settings("Burhanverse", "WebClip");
    settings.setValue("downloadDirectory", tempDir.path());

    QString partPath = tempDir.path() + "/resume.bin.part";
    QFile partFile(partPath);
    QVERIFY(partFile.open(QIODevice::WriteOnly));
    partFile.write(QByteArray(40, 'A'));
    partFile.close();

    webclip::FileOfferTask task;
    task.token = "resume1";
    task.name = "resume.bin";
    task.size = 100;
    task.host = "127.0.0.1";
    task.port = server.port();
    task.use_https = false;
    task.insecure = true;
    task.pairing_code = "1234";

    webclip::FileDownloadManager manager;
    bool ok = manager.process_single_task(task);
    QVERIFY(ok);

    QString finalPath = tempDir.path() + "/resume.bin";
    QVERIFY(QFile::exists(finalPath));
    QCOMPARE(QFileInfo(finalPath).size(), 100);
    QVERIFY(!QFile::exists(partPath));

    QFile readBack(finalPath);
    QVERIFY(readBack.open(QIODevice::ReadOnly));
    QByteArray data = readBack.readAll();
    readBack.close();
    QCOMPARE(data.left(40), QByteArray(40, 'A'));
    QCOMPARE(data.mid(40), QByteArray(60, 'B'));

    settings.remove("downloadDirectory");
}

void TestFileTransfer::testServerIgnoringRange200TruncatesAndRestarts() {
    FakeHttpServer server;
    QVERIFY(server.start([](int fd, const std::string&, const std::string&, const auto&) {
        FakeHttpServer::send_response(fd, 200, "OK", {{"Content-Type", "application/octet-stream"}}, std::string(100, 'C'));
    }));

    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());
    QSettings settings("Burhanverse", "WebClip");
    settings.setValue("downloadDirectory", tempDir.path());

    QString partPath = tempDir.path() + "/ignore_range.bin.part";
    QFile partFile(partPath);
    QVERIFY(partFile.open(QIODevice::WriteOnly));
    partFile.write(QByteArray(40, 'Z'));
    partFile.close();

    webclip::FileOfferTask task;
    task.token = "ignore1";
    task.name = "ignore_range.bin";
    task.size = 100;
    task.host = "127.0.0.1";
    task.port = server.port();
    task.use_https = false;
    task.insecure = true;
    task.pairing_code = "1234";

    webclip::FileDownloadManager manager;
    bool ok = manager.process_single_task(task);
    QVERIFY(ok);

    QString finalPath = tempDir.path() + "/ignore_range.bin";
    QVERIFY(QFile::exists(finalPath));
    QCOMPARE(QFileInfo(finalPath).size(), 100);
    QVERIFY(!QFile::exists(partPath));

    QFile readBack(finalPath);
    QVERIFY(readBack.open(QIODevice::ReadOnly));
    QByteArray data = readBack.readAll();
    readBack.close();
    QCOMPARE(data, QByteArray(100, 'C'));

    settings.remove("downloadDirectory");
}

void TestFileTransfer::test416WithPartEqualsOfferedSizeRenames() {
    FakeHttpServer server;
    QVERIFY(server.start([](int fd, const std::string&, const std::string&, const auto&) {
        FakeHttpServer::send_response(fd, 416, "Range Not Satisfiable", {{"Content-Range", "bytes */100"}}, "");
    }));

    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());
    QSettings settings("Burhanverse", "WebClip");
    settings.setValue("downloadDirectory", tempDir.path());

    QString partPath = tempDir.path() + "/done416.bin.part";
    QFile partFile(partPath);
    QVERIFY(partFile.open(QIODevice::WriteOnly));
    partFile.write(QByteArray(100, 'D'));
    partFile.close();

    webclip::FileOfferTask task;
    task.token = "done416";
    task.name = "done416.bin";
    task.size = 100;
    task.host = "127.0.0.1";
    task.port = server.port();
    task.use_https = false;
    task.insecure = true;
    task.pairing_code = "1234";

    webclip::FileDownloadManager manager;
    bool ok = manager.process_single_task(task);
    QVERIFY(ok);

    QString finalPath = tempDir.path() + "/done416.bin";
    QVERIFY(QFile::exists(finalPath));
    QCOMPARE(QFileInfo(finalPath).size(), 100);
    QVERIFY(!QFile::exists(partPath));

    settings.remove("downloadDirectory");
}

void TestFileTransfer::test416WithLargerPartDeletesAndRestarts() {
    FakeHttpServer server;
    QVERIFY(server.start([](int fd, const std::string&, const std::string&, const auto& headers) {
        if (headers.find("range") != headers.end()) {
            FakeHttpServer::send_response(fd, 416, "Range Not Satisfiable", {{"Content-Range", "bytes */100"}}, "");
        } else {
            FakeHttpServer::send_response(fd, 200, "OK", {{"Content-Type", "application/octet-stream"}}, std::string(100, 'E'));
        }
    }));

    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());
    QSettings settings("Burhanverse", "WebClip");
    settings.setValue("downloadDirectory", tempDir.path());

    QString partPath = tempDir.path() + "/corrupt_large.bin.part";
    QFile partFile(partPath);
    QVERIFY(partFile.open(QIODevice::WriteOnly));
    partFile.write(QByteArray(150, 'X'));
    partFile.close();

    webclip::FileOfferTask task;
    task.token = "corrupt1";
    task.name = "corrupt_large.bin";
    task.size = 100;
    task.host = "127.0.0.1";
    task.port = server.port();
    task.use_https = false;
    task.insecure = true;
    task.pairing_code = "1234";

    webclip::FileDownloadManager manager;
    bool ok = manager.process_single_task(task);
    QVERIFY(ok);

    QString finalPath = tempDir.path() + "/corrupt_large.bin";
    QVERIFY(QFile::exists(finalPath));
    QCOMPARE(QFileInfo(finalPath).size(), 100);
    QVERIFY(!QFile::exists(partPath));

    QFile readBack(finalPath);
    QVERIFY(readBack.open(QIODevice::ReadOnly));
    QByteArray data = readBack.readAll();
    readBack.close();
    QCOMPARE(data, QByteArray(100, 'E'));

    settings.remove("downloadDirectory");
}

void TestFileTransfer::test503WithRetryAfterThenSuccess() {
    std::shared_ptr<std::atomic<int>> req_count = std::make_shared<std::atomic<int>>(0);
    FakeHttpServer server;
    QVERIFY(server.start([req_count](int fd, const std::string&, const std::string&, const auto&) {
        int cur = req_count->fetch_add(1);
        if (cur == 0) {
            FakeHttpServer::send_response(fd, 503, "Service Unavailable", {{"Retry-After", "1"}}, "Busy");
        } else {
            FakeHttpServer::send_response(fd, 200, "OK", {{"Content-Type", "application/octet-stream"}}, std::string(100, 'F'));
        }
    }));

    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());
    QSettings settings("Burhanverse", "WebClip");
    settings.setValue("downloadDirectory", tempDir.path());

    webclip::FileOfferTask task;
    task.token = "retry1";
    task.name = "retry503.bin";
    task.size = 100;
    task.host = "127.0.0.1";
    task.port = server.port();
    task.use_https = false;
    task.insecure = true;
    task.pairing_code = "1234";

    webclip::FileDownloadManager manager;
    bool ok = manager.process_single_task(task);
    QVERIFY(ok);
    QVERIFY(req_count->load() >= 2);

    QString finalPath = tempDir.path() + "/retry503.bin";
    QVERIFY(QFile::exists(finalPath));
    QCOMPARE(QFileInfo(finalPath).size(), 100);

    settings.remove("downloadDirectory");
}

void TestFileTransfer::test404Or410NoRetry() {
    std::shared_ptr<std::atomic<int>> req_count = std::make_shared<std::atomic<int>>(0);
    FakeHttpServer server;
    QVERIFY(server.start([req_count](int fd, const std::string&, const std::string&, const auto&) {
        req_count->fetch_add(1);
        FakeHttpServer::send_response(fd, 404, "Not Found", {}, "Not Found");
    }));

    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());
    QSettings settings("Burhanverse", "WebClip");
    settings.setValue("downloadDirectory", tempDir.path());

    webclip::FileOfferTask task;
    task.token = "missing1";
    task.name = "missing404.bin";
    task.size = 100;
    task.host = "127.0.0.1";
    task.port = server.port();
    task.use_https = false;
    task.insecure = true;
    task.pairing_code = "1234";

    webclip::FileDownloadManager manager;
    bool ok = manager.process_single_task(task);
    QVERIFY(!ok);
    QCOMPARE(req_count->load(), 1);

    QString finalPath = tempDir.path() + "/missing404.bin";
    QVERIFY(!QFile::exists(finalPath));

    settings.remove("downloadDirectory");
}

void TestFileTransfer::testConnectionDroppedMidBodyKeepsPartAndResumes() {
    std::shared_ptr<std::atomic<int>> req_count = std::make_shared<std::atomic<int>>(0);
    FakeHttpServer server;
    QVERIFY(server.start([req_count](int fd, const std::string&, const std::string&, const auto& headers) {
        int cur = req_count->fetch_add(1);
        if (cur == 0) {
            std::string resp = "HTTP/1.1 200 OK\r\nContent-Length: 100\r\nContent-Type: application/octet-stream\r\n\r\n";
            resp.append(40, 'M');
#if defined(MSG_NOSIGNAL)
            ::send(fd, resp.data(), resp.size(), MSG_NOSIGNAL);
#else
            ::send(fd, resp.data(), resp.size(), 0);
#endif
            ::close(fd);
        } else {
            auto it = headers.find("range");
            if (it != headers.end() && it->second == "bytes=40-") {
                FakeHttpServer::send_response(fd, 206, "Partial Content",
                    {{"Content-Range", "bytes 40-99/100"}, {"Content-Type", "application/octet-stream"}},
                    std::string(60, 'N'));
            } else {
                FakeHttpServer::send_response(fd, 400, "Bad Request", {}, "Expected Range");
            }
        }
    }));

    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());
    QSettings settings("Burhanverse", "WebClip");
    settings.setValue("downloadDirectory", tempDir.path());

    QString finalPath = tempDir.path() + "/dropped.bin";
    QString partPath = finalPath + ".part";

    webclip::HttpClient client("127.0.0.1", server.port(), "1234", false, true);
    std::atomic<bool> cancel_flag{false};
    int http_code = 0;
    int retry_after = 0;

    bool first_attempt_ok = client.download_file("/files/dropped", partPath.toStdString(), 0, nullptr, cancel_flag, http_code, retry_after);
    QVERIFY(!first_attempt_ok);
    QVERIFY(QFile::exists(partPath));
    QCOMPARE(QFileInfo(partPath).size(), 40);
    QVERIFY(!QFile::exists(finalPath));

    webclip::FileOfferTask task;
    task.token = "dropped";
    task.name = "dropped.bin";
    task.size = 100;
    task.host = "127.0.0.1";
    task.port = server.port();
    task.use_https = false;
    task.insecure = true;
    task.pairing_code = "1234";

    webclip::FileDownloadManager manager;
    bool second_attempt_ok = manager.process_single_task(task);
    QVERIFY(second_attempt_ok);
    QVERIFY(QFile::exists(finalPath));
    QCOMPARE(QFileInfo(finalPath).size(), 100);
    QVERIFY(!QFile::exists(partPath));

    QFile readBack(finalPath);
    QVERIFY(readBack.open(QIODevice::ReadOnly));
    QByteArray data = readBack.readAll();
    readBack.close();
    QCOMPARE(data.left(40), QByteArray(40, 'M'));
    QCOMPARE(data.mid(40), QByteArray(60, 'N'));

    settings.remove("downloadDirectory");
}

void TestFileTransfer::testStalledBodyLowSpeedAbort() {
    FakeHttpServer server;
    QVERIFY(server.start([](int fd, const std::string&, const std::string&, const auto&) {
        std::string resp = "HTTP/1.1 200 OK\r\nContent-Length: 100\r\nContent-Type: application/octet-stream\r\n\r\n";
        resp.append(10, 'S');
#if defined(MSG_NOSIGNAL)
        ::send(fd, resp.data(), resp.size(), MSG_NOSIGNAL);
#else
        ::send(fd, resp.data(), resp.size(), 0);
#endif
        std::this_thread::sleep_for(std::chrono::seconds(2));
        ::close(fd);
    }));

    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());
    QString finalPath = tempDir.path() + "/stalled.bin";
    QString partPath = finalPath + ".part";

    webclip::HttpClient client("127.0.0.1", server.port(), "1234", false, true);
    client.set_low_speed_options(1024L, 1L);
    std::atomic<bool> cancel_flag{false};
    int http_code = 0;
    int retry_after = 0;

    bool ok = client.download_file("/files/stalled", partPath.toStdString(), 0, nullptr, cancel_flag, http_code, retry_after);
    QVERIFY(!ok);
    QVERIFY(!QFile::exists(finalPath));
}

void TestFileTransfer::testCancelMidDownload() {
    FakeHttpServer server;
    QVERIFY(server.start([](int fd, const std::string&, const std::string&, const auto&) {
        std::string resp = "HTTP/1.1 200 OK\r\nContent-Length: 5000000\r\nContent-Type: application/octet-stream\r\n\r\n";
#if defined(MSG_NOSIGNAL)
        if (::send(fd, resp.data(), resp.size(), MSG_NOSIGNAL) <= 0) {
            ::close(fd);
            return;
        }
        for (int i = 0; i < 50; ++i) {
            std::string chunk(1024, 'K');
            if (::send(fd, chunk.data(), chunk.size(), MSG_NOSIGNAL) <= 0) {
                break;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
        }
#else
        if (::send(fd, resp.data(), resp.size(), 0) <= 0) {
            ::close(fd);
            return;
        }
        for (int i = 0; i < 50; ++i) {
            std::string chunk(1024, 'K');
            if (::send(fd, chunk.data(), chunk.size(), 0) <= 0) {
                break;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
        }
#endif
        ::close(fd);
    }));

    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());
    QString finalPath = tempDir.path() + "/cancel.bin";
    QString partPath = finalPath + ".part";

    webclip::HttpClient client("127.0.0.1", server.port(), "1234", false, true);
    std::atomic<bool> cancel_flag{false};
    int http_code = 0;
    int retry_after = 0;

    auto progress_cb = [&cancel_flag](int64_t downloaded, int64_t) {
        if (downloaded > 0) {
            cancel_flag.store(true);
        }
    };

    bool ok = client.download_file("/files/cancel", partPath.toStdString(), 0, progress_cb, cancel_flag, http_code, retry_after);
    QVERIFY(!ok);
    QVERIFY(!QFile::exists(finalPath));
}

void TestFileTransfer::testFinalSizeMismatchNeverRenamed() {
    FakeHttpServer server;
    QVERIFY(server.start([](int fd, const std::string&, const std::string&, const auto& headers) {
        if (headers.find("range") != headers.end()) {
            FakeHttpServer::send_response(fd, 416, "Range Not Satisfiable", {{"Content-Range", "bytes */80"}}, "");
        } else {
            FakeHttpServer::send_response(fd, 200, "OK", {{"Content-Type", "application/octet-stream"}}, std::string(80, 'R'));
        }
    }));

    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());
    QSettings settings("Burhanverse", "WebClip");
    settings.setValue("downloadDirectory", tempDir.path());

    webclip::FileOfferTask task;
    task.token = "mismatch";
    task.name = "mismatch.bin";
    task.size = 100;
    task.host = "127.0.0.1";
    task.port = server.port();
    task.use_https = false;
    task.insecure = true;
    task.pairing_code = "1234";

    webclip::FileDownloadManager manager;
    bool ok = manager.process_single_task(task);
    QVERIFY(!ok);

    QString finalPath = tempDir.path() + "/mismatch.bin";
    QVERIFY(!QFile::exists(finalPath));

    settings.remove("downloadDirectory");
}

void TestFileTransfer::testCollisionNaming() {
    FakeHttpServer server;
    QVERIFY(server.start([](int fd, const std::string&, const std::string&, const auto&) {
        FakeHttpServer::send_response(fd, 200, "OK", {{"Content-Type", "application/octet-stream"}}, std::string(100, 'X'));
    }));

    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());
    QSettings settings("Burhanverse", "WebClip");
    settings.setValue("downloadDirectory", tempDir.path());

    QString existingPath = tempDir.path() + "/collide.txt";
    QFile existingFile(existingPath);
    QVERIFY(existingFile.open(QIODevice::WriteOnly));
    existingFile.write("original");
    existingFile.close();

    webclip::FileOfferTask task;
    task.token = "collide";
    task.name = "collide.txt";
    task.size = 100;
    task.host = "127.0.0.1";
    task.port = server.port();
    task.use_https = false;
    task.insecure = true;
    task.pairing_code = "1234";

    webclip::FileDownloadManager manager;
    bool ok = manager.process_single_task(task);
    QVERIFY(ok);

    QVERIFY(QFile::exists(existingPath));
    QCOMPARE(QFileInfo(existingPath).size(), 8);

    QString collisionPath = tempDir.path() + "/collide (1).txt";
    QVERIFY(QFile::exists(collisionPath));
    QCOMPARE(QFileInfo(collisionPath).size(), 100);

    settings.remove("downloadDirectory");
}

void TestFileTransfer::testUploadFull() {
    std::shared_ptr<std::atomic<bool>> expect_header_found = std::make_shared<std::atomic<bool>>(false);
    std::shared_ptr<std::atomic<bool>> upload_received = std::make_shared<std::atomic<bool>>(false);

    FakeHttpServer server;
    QVERIFY(server.start_with_body([expect_header_found, upload_received](int fd, const std::string& method, const std::string& path, const auto& headers, const std::string& body) {
        if (path == "/files/upload" && method == "POST") {
            if (headers.find("expect") != headers.end()) {
                expect_header_found->store(true);
            }
            auto pc = headers.find("x-pairing-code");
            auto uid = headers.find("x-upload-id");
            auto fn = headers.find("x-file-name");
            auto fs = headers.find("x-file-size");
            auto fo = headers.find("x-file-offset");
            auto bi = headers.find("x-batch-id");
            auto bx = headers.find("x-batch-index");
            auto bt = headers.find("x-batch-total");
            if (pc != headers.end() && pc->second == "1234" &&
                uid != headers.end() && !uid->second.empty() &&
                fn != headers.end() && !fn->second.empty() &&
                fs != headers.end() && fs->second == "100" &&
                fo != headers.end() && fo->second == "0" &&
                bi != headers.end() && bi->second == "b1" &&
                bx != headers.end() && bx->second == "1" &&
                bt != headers.end() && bt->second == "1" &&
                body.size() == 100) {
                upload_received->store(true);
                FakeHttpServer::send_response(fd, 200, "OK", {{"Content-Type", "application/json"}}, "{\"ok\":true,\"status\":\"complete\"}");
                return;
            }
        }
        FakeHttpServer::send_response(fd, 400, "Bad Request", {}, "{}");
    }));

    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());
    QString filePath = tempDir.filePath("test_upload.bin");
    {
        QFile f(filePath);
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write(QByteArray(100, 'Z'));
        f.close();
    }

    webclip::FileUploadManager mgr;
    std::atomic<bool> completed{false};
    int completed_files = 0;
    mgr.set_completion_callback([&completed, &completed_files](const std::string&, int total_files) {
        completed_files = total_files;
        completed.store(true);
    });

    webclip::FileUploadTask task;
    task.upload_id = "up_1";
    task.local_path = filePath.toStdString();
    task.file_name = "test_upload.bin";
    task.file_size = 100;
    task.batch_id = "b1";
    task.index = 1;
    task.total = 1;
    task.host = "127.0.0.1";
    task.port = server.port();
    task.pairing_code = "1234";

    mgr.enqueue_task(task);

    for (int i = 0; i < 50 && !completed.load(); ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    QVERIFY(completed.load());
    QCOMPARE(completed_files, 1);
    QVERIFY(upload_received->load());
    QVERIFY(!expect_header_found->load());
}

void TestFileTransfer::testUploadResumeViaStatusRoute() {
    std::shared_ptr<std::atomic<int>> post_count = std::make_shared<std::atomic<int>>(0);
    std::shared_ptr<std::atomic<bool>> status_called = std::make_shared<std::atomic<bool>>(false);
    std::shared_ptr<std::atomic<bool>> resumed_post_ok = std::make_shared<std::atomic<bool>>(false);

    FakeHttpServer server;
    QVERIFY(server.start_with_body([post_count, status_called, resumed_post_ok](int fd, const std::string& method, const std::string& path, const auto& headers, const std::string& body) {
        if (method == "POST" && path == "/files/upload") {
            int c = post_count->fetch_add(1);
            if (c == 0) {
                FakeHttpServer::send_response(fd, 503, "Service Unavailable", {{"Retry-After", "1"}}, "{\"error\":\"busy\"}");
                return;
            } else {
                auto fo = headers.find("x-file-offset");
                if (fo != headers.end() && fo->second == "40" && body.size() == 60) {
                    resumed_post_ok->store(true);
                    FakeHttpServer::send_response(fd, 200, "OK", {{"Content-Type", "application/json"}}, "{\"ok\":true,\"status\":\"complete\"}");
                    return;
                }
            }
        } else if (method == "GET" && path.rfind("/files/upload/", 0) == 0) {
            status_called->store(true);
            FakeHttpServer::send_response(fd, 200, "OK", {{"Content-Type", "application/json"}}, "{\"offset\":40,\"size\":100}");
            return;
        }
        FakeHttpServer::send_response(fd, 400, "Bad Request", {}, "{}");
    }));

    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());
    QString filePath = tempDir.filePath("test_resume.bin");
    {
        QFile f(filePath);
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write(QByteArray(100, 'R'));
        f.close();
    }

    webclip::FileUploadManager mgr;
    std::atomic<bool> completed{false};
    mgr.set_completion_callback([&completed](const std::string&, int) {
        completed.store(true);
    });

    webclip::FileUploadTask task;
    task.upload_id = "up_resume";
    task.local_path = filePath.toStdString();
    task.file_name = "test_resume.bin";
    task.file_size = 100;
    task.batch_id = "b_res";
    task.index = 1;
    task.total = 1;
    task.host = "127.0.0.1";
    task.port = server.port();
    task.pairing_code = "1234";

    mgr.enqueue_task(task);

    for (int i = 0; i < 60 && !completed.load(); ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    QVERIFY(completed.load());
    QVERIFY(status_called->load());
    QVERIFY(resumed_post_ok->load());
}

void TestFileTransfer::testUpload409AdoptOffset() {
    std::shared_ptr<std::atomic<int>> post_count = std::make_shared<std::atomic<int>>(0);
    std::shared_ptr<std::atomic<bool>> second_post_adopted = std::make_shared<std::atomic<bool>>(false);

    FakeHttpServer server;
    QVERIFY(server.start_with_body([post_count, second_post_adopted](int fd, const std::string& method, const std::string& path, const auto& headers, const std::string& body) {
        if (method == "POST" && path == "/files/upload") {
            int c = post_count->fetch_add(1);
            if (c == 0) {
                FakeHttpServer::send_response(fd, 409, "Conflict", {{"Content-Type", "application/json"}}, "{\"offset\":50}");
                return;
            } else {
                auto fo = headers.find("x-file-offset");
                if (fo != headers.end() && fo->second == "50" && body.size() == 50) {
                    second_post_adopted->store(true);
                    FakeHttpServer::send_response(fd, 200, "OK", {{"Content-Type", "application/json"}}, "{\"ok\":true,\"status\":\"complete\"}");
                    return;
                }
            }
        }
        FakeHttpServer::send_response(fd, 400, "Bad Request", {}, "{}");
    }));

    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());
    QString filePath = tempDir.filePath("test_409.bin");
    {
        QFile f(filePath);
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write(QByteArray(100, 'X'));
        f.close();
    }

    webclip::FileUploadManager mgr;
    std::atomic<bool> completed{false};
    mgr.set_completion_callback([&completed](const std::string&, int) {
        completed.store(true);
    });

    webclip::FileUploadTask task;
    task.upload_id = "up_409";
    task.local_path = filePath.toStdString();
    task.file_name = "test_409.bin";
    task.file_size = 100;
    task.batch_id = "b409";
    task.index = 1;
    task.total = 1;
    task.host = "127.0.0.1";
    task.port = server.port();
    task.pairing_code = "1234";

    mgr.enqueue_task(task);

    for (int i = 0; i < 50 && !completed.load(); ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    QVERIFY(completed.load());
    QVERIFY(second_post_adopted->load());
}

void TestFileTransfer::testUpload507StorageFull() {
    std::shared_ptr<std::atomic<int>> post_count = std::make_shared<std::atomic<int>>(0);

    FakeHttpServer server;
    QVERIFY(server.start_with_body([post_count](int fd, const std::string&, const std::string&, const auto&, const std::string&) {
        post_count->fetch_add(1);
        FakeHttpServer::send_response(fd, 507, "Insufficient Storage", {{"Content-Type", "application/json"}}, "{\"ok\":false,\"error\":\"insufficient_storage\"}");
    }));

    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());
    QString filePath = tempDir.filePath("test_507.bin");
    {
        QFile f(filePath);
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write(QByteArray(100, 'S'));
        f.close();
    }

    webclip::FileUploadManager mgr;
    std::atomic<bool> failed{false};
    std::string fail_reason;
    mgr.set_failure_callback([&failed, &fail_reason](const std::string&, const std::string& reason) {
        fail_reason = reason;
        failed.store(true);
    });

    webclip::FileUploadTask task;
    task.upload_id = "up_507";
    task.local_path = filePath.toStdString();
    task.file_name = "test_507.bin";
    task.file_size = 100;
    task.batch_id = "b507";
    task.index = 1;
    task.total = 1;
    task.host = "127.0.0.1";
    task.port = server.port();
    task.pairing_code = "1234";

    mgr.enqueue_task(task);

    for (int i = 0; i < 30 && !failed.load(); ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    QVERIFY(failed.load());
    QCOMPARE(QString::fromStdString(fail_reason), QString("Phone storage full"));
    QCOMPARE(post_count->load(), 1);
}

void TestFileTransfer::testUpload500SinkCreationFailed() {
    std::shared_ptr<std::atomic<int>> post_count = std::make_shared<std::atomic<int>>(0);

    FakeHttpServer server;
    QVERIFY(server.start_with_body([post_count](int fd, const std::string&, const std::string&, const auto&, const std::string&) {
        post_count->fetch_add(1);
        FakeHttpServer::send_response(fd, 500, "Internal Server Error", {{"Content-Type", "application/json"}}, "{\"ok\":false,\"error\":\"sink_creation_failed\",\"detail\":\"Failed to create MediaStore entry\"}");
    }));

    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());
    QString filePath = tempDir.filePath("test_500.bin");
    {
        QFile f(filePath);
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write(QByteArray(100, 'X'));
        f.close();
    }

    webclip::FileUploadManager mgr;
    std::atomic<bool> failed{false};
    std::string fail_reason;
    mgr.set_failure_callback([&failed, &fail_reason](const std::string&, const std::string& reason) {
        fail_reason = reason;
        failed.store(true);
    });

    webclip::FileUploadTask task;
    task.upload_id = "up_500";
    task.local_path = filePath.toStdString();
    task.file_name = "test_500.bin";
    task.file_size = 100;
    task.batch_id = "b500";
    task.index = 1;
    task.total = 1;
    task.host = "127.0.0.1";
    task.port = server.port();
    task.pairing_code = "1234";

    mgr.enqueue_task(task);

    for (int i = 0; i < 30 && !failed.load(); ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    QVERIFY(failed.load());
    QCOMPARE(QString::fromStdString(fail_reason), QString("Phone failed to create file (Failed to create MediaStore entry)"));
    QCOMPARE(post_count->load(), 1);
}

void TestFileTransfer::testUpload503WithRetryAfter() {
    std::shared_ptr<std::atomic<int>> post_count = std::make_shared<std::atomic<int>>(0);

    FakeHttpServer server;
    QVERIFY(server.start_with_body([post_count](int fd, const std::string& method, const std::string& path, const auto&, const std::string&) {
        if (method == "POST" && path == "/files/upload") {
            int c = post_count->fetch_add(1);
            if (c == 0) {
                FakeHttpServer::send_response(fd, 503, "Service Unavailable", {{"Retry-After", "1"}}, "{\"error\":\"busy\"}");
                return;
            } else {
                FakeHttpServer::send_response(fd, 200, "OK", {{"Content-Type", "application/json"}}, "{\"ok\":true,\"status\":\"complete\"}");
                return;
            }
        } else if (method == "GET") {
            FakeHttpServer::send_response(fd, 404, "Not Found", {}, "{}");
            return;
        }
        FakeHttpServer::send_response(fd, 400, "Bad Request", {}, "{}");
    }));

    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());
    QString filePath = tempDir.filePath("test_503.bin");
    {
        QFile f(filePath);
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write(QByteArray(100, 'W'));
        f.close();
    }

    webclip::FileUploadManager mgr;
    std::atomic<bool> completed{false};
    mgr.set_completion_callback([&completed](const std::string&, int) {
        completed.store(true);
    });

    webclip::FileUploadTask task;
    task.upload_id = "up_503";
    task.local_path = filePath.toStdString();
    task.file_name = "test_503.bin";
    task.file_size = 100;
    task.batch_id = "b503";
    task.index = 1;
    task.total = 1;
    task.host = "127.0.0.1";
    task.port = server.port();
    task.pairing_code = "1234";

    mgr.enqueue_task(task);

    for (int i = 0; i < 60 && !completed.load(); ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    QVERIFY(completed.load());
    QVERIFY(post_count->load() >= 2);
}

void TestFileTransfer::testUpload401PairingError() {
    std::shared_ptr<std::atomic<int>> post_count = std::make_shared<std::atomic<int>>(0);

    FakeHttpServer server;
    QVERIFY(server.start_with_body([post_count](int fd, const std::string&, const std::string&, const auto&, const std::string&) {
        post_count->fetch_add(1);
        FakeHttpServer::send_response(fd, 401, "Unauthorized", {{"Content-Type", "application/json"}}, "{\"ok\":false,\"error\":\"unauthorized\"}");
    }));

    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());
    QString filePath = tempDir.filePath("test_401.bin");
    {
        QFile f(filePath);
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write(QByteArray(100, 'U'));
        f.close();
    }

    webclip::FileUploadManager mgr;
    std::atomic<bool> failed{false};
    std::string fail_reason;
    mgr.set_failure_callback([&failed, &fail_reason](const std::string&, const std::string& reason) {
        fail_reason = reason;
        failed.store(true);
    });

    webclip::FileUploadTask task;
    task.upload_id = "up_401";
    task.local_path = filePath.toStdString();
    task.file_name = "test_401.bin";
    task.file_size = 100;
    task.batch_id = "b401";
    task.index = 1;
    task.total = 1;
    task.host = "127.0.0.1";
    task.port = server.port();
    task.pairing_code = "wrong";

    mgr.enqueue_task(task);

    for (int i = 0; i < 30 && !failed.load(); ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    QVERIFY(failed.load());
    QCOMPARE(QString::fromStdString(fail_reason), QString("Pairing error (HTTP 401)"));
    QCOMPARE(post_count->load(), 1);
}

void TestFileTransfer::testUpload403UploadsDisabled() {
    std::shared_ptr<std::atomic<int>> post_count = std::make_shared<std::atomic<int>>(0);

    FakeHttpServer server;
    QVERIFY(server.start_with_body([post_count](int fd, const std::string&, const std::string&, const auto&, const std::string&) {
        post_count->fetch_add(1);
        FakeHttpServer::send_response(fd, 403, "Forbidden", {{"Content-Type", "application/json"}}, "{\"ok\":false,\"error\":\"uploads_disabled\"}");
    }));

    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());
    QString filePath = tempDir.filePath("test_403.bin");
    {
        QFile f(filePath);
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write(QByteArray(100, 'D'));
        f.close();
    }

    webclip::FileUploadManager mgr;
    std::atomic<bool> failed{false};
    std::string fail_reason;
    mgr.set_failure_callback([&failed, &fail_reason](const std::string&, const std::string& reason) {
        fail_reason = reason;
        failed.store(true);
    });

    webclip::FileUploadTask task;
    task.upload_id = "up_403";
    task.local_path = filePath.toStdString();
    task.file_name = "test_403.bin";
    task.file_size = 100;
    task.batch_id = "b403";
    task.index = 1;
    task.total = 1;
    task.host = "127.0.0.1";
    task.port = server.port();
    task.pairing_code = "1234";

    mgr.enqueue_task(task);

    for (int i = 0; i < 30 && !failed.load(); ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    QVERIFY(failed.load());
    QCOMPARE(QString::fromStdString(fail_reason), QString("Uploads disabled on phone"));
    QCOMPARE(post_count->load(), 1);
}

void TestFileTransfer::testUploadDropMidBodyAndResume() {
    std::shared_ptr<std::atomic<int>> post_count = std::make_shared<std::atomic<int>>(0);
    std::shared_ptr<std::atomic<bool>> resumed_ok = std::make_shared<std::atomic<bool>>(false);

    FakeHttpServer server;
    QVERIFY(server.start_raw([post_count, resumed_ok](int fd, const std::string& method, const std::string& path, const auto& headers, const std::string&) {
        if (method == "POST" && path == "/files/upload") {
            int c = post_count->fetch_add(1);
            if (c == 0) {
                ::close(fd);
                return;
            } else {
                auto fo = headers.find("x-file-offset");
                if (fo != headers.end() && fo->second == "50") {
                    resumed_ok->store(true);
                    FakeHttpServer::send_response(fd, 200, "OK", {{"Content-Type", "application/json"}}, "{\"ok\":true,\"status\":\"complete\"}");
                    return;
                }
            }
        } else if (method == "GET" && path.rfind("/files/upload/", 0) == 0) {
            FakeHttpServer::send_response(fd, 200, "OK", {{"Content-Type", "application/json"}}, "{\"offset\":50,\"size\":100}");
            return;
        }
        FakeHttpServer::send_response(fd, 400, "Bad Request", {}, "{}");
    }));

    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());
    QString filePath = tempDir.filePath("test_drop.bin");
    {
        QFile f(filePath);
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write(QByteArray(100, 'D'));
        f.close();
    }

    webclip::FileUploadManager mgr;
    std::atomic<bool> completed{false};
    mgr.set_completion_callback([&completed](const std::string&, int) {
        completed.store(true);
    });

    webclip::FileUploadTask task;
    task.upload_id = "up_drop";
    task.local_path = filePath.toStdString();
    task.file_name = "test_drop.bin";
    task.file_size = 100;
    task.batch_id = "b_drop";
    task.index = 1;
    task.total = 1;
    task.host = "127.0.0.1";
    task.port = server.port();
    task.pairing_code = "1234";

    mgr.enqueue_task(task);

    for (int i = 0; i < 60 && !completed.load(); ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    QVERIFY(completed.load());
    QVERIFY(resumed_ok->load());
}

void TestFileTransfer::testUploadStalledBodyLowSpeedAbort() {
    std::shared_ptr<std::atomic<bool>> stall_handled = std::make_shared<std::atomic<bool>>(false);

    FakeHttpServer server;
    QVERIFY(server.start_raw([stall_handled](int fd, const std::string& method, const std::string& path, const auto&, const std::string&) {
        if (method == "POST" && path == "/files/upload") {
            stall_handled->store(true);
            std::this_thread::sleep_for(std::chrono::seconds(3));
            ::close(fd);
            return;
        }
        ::close(fd);
    }));

    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());
    QString filePath = tempDir.filePath("test_stall.bin");
    {
        QFile f(filePath);
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write(QByteArray(100000, 'S'));
        f.close();
    }

    webclip::FileUploadManager mgr;
    mgr.set_low_speed_options(1000000L, 1L);

    webclip::FileUploadTask task;
    task.upload_id = "up_stall";
    task.local_path = filePath.toStdString();
    task.file_name = "test_stall.bin";
    task.file_size = 100000;
    task.batch_id = "bstall";
    task.index = 1;
    task.total = 1;
    task.host = "127.0.0.1";
    task.port = server.port();
    task.pairing_code = "1234";

    std::thread runner([&mgr, task]() {
        mgr.process_single_task(task);
    });

    runner.join();
    QVERIFY(stall_handled->load());
}

void TestFileTransfer::testUploadCancelMidUpload() {
    FakeHttpServer server;
    QVERIFY(server.start_raw([](int fd, const std::string& method, const std::string& path, const auto&, const std::string&) {
        if (method == "POST" && path == "/files/upload") {
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
            FakeHttpServer::send_response(fd, 200, "OK", {}, "{}");
            return;
        }
        ::close(fd);
    }));

    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());
    QString filePath = tempDir.filePath("test_cancel.bin");
    {
        QFile f(filePath);
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write(QByteArray(1000000, 'C'));
        f.close();
    }

    webclip::FileUploadManager mgr;
    std::atomic<bool> completed{false};
    mgr.set_completion_callback([&completed](const std::string&, int) {
        completed.store(true);
    });

    webclip::FileUploadTask task;
    task.upload_id = "up_cancel";
    task.local_path = filePath.toStdString();
    task.file_name = "test_cancel.bin";
    task.file_size = 1000000;
    task.batch_id = "bcancel";
    task.index = 1;
    task.total = 1;
    task.host = "127.0.0.1";
    task.port = server.port();
    task.pairing_code = "1234";

    mgr.enqueue_task(task);

    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    mgr.cancel_current();

    for (int i = 0; i < 20; ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }

    QVERIFY(!completed.load());
}

void TestFileTransfer::testUploadNoExpectHeaderSent() {
    std::shared_ptr<std::atomic<bool>> header_checked = std::make_shared<std::atomic<bool>>(false);
    std::shared_ptr<std::atomic<bool>> has_expect = std::make_shared<std::atomic<bool>>(false);

    FakeHttpServer server;
    QVERIFY(server.start_with_body([header_checked, has_expect](int fd, const std::string& method, const std::string& path, const auto& headers, const std::string&) {
        if (method == "POST" && path == "/files/upload") {
            header_checked->store(true);
            if (headers.find("expect") != headers.end()) {
                has_expect->store(true);
            }
            FakeHttpServer::send_response(fd, 200, "OK", {{"Content-Type", "application/json"}}, "{\"ok\":true,\"status\":\"complete\"}");
            return;
        }
        FakeHttpServer::send_response(fd, 400, "Bad Request", {}, "{}");
    }));

    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());
    QString filePath = tempDir.filePath("test_no_expect.bin");
    {
        QFile f(filePath);
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write(QByteArray(2048, 'N'));
        f.close();
    }

    webclip::FileUploadManager mgr;
    std::atomic<bool> completed{false};
    mgr.set_completion_callback([&completed](const std::string&, int) {
        completed.store(true);
    });

    webclip::FileUploadTask task;
    task.upload_id = "up_no_expect";
    task.local_path = filePath.toStdString();
    task.file_name = "test_no_expect.bin";
    task.file_size = 2048;
    task.batch_id = "b_no_exp";
    task.index = 1;
    task.total = 1;
    task.host = "127.0.0.1";
    task.port = server.port();
    task.pairing_code = "1234";

    mgr.enqueue_task(task);

    for (int i = 0; i < 30 && !completed.load(); ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    QVERIFY(completed.load());
    QVERIFY(header_checked->load());
    QVERIFY(!has_expect->load());
}

void TestFileTransfer::testUpload410BatchCancelledStopsBatchImmediately() {
    std::shared_ptr<std::atomic<int>> post_count = std::make_shared<std::atomic<int>>(0);

    FakeHttpServer server;
    QVERIFY(server.start_with_body([post_count](int fd, const std::string& method, const std::string& path, const auto&, const std::string&) {
        if (method == "POST" && path == "/files/upload") {
            post_count->fetch_add(1);
            FakeHttpServer::send_response(fd, 410, "Gone", {{"Content-Type", "application/json"}}, "{\"ok\":false,\"error\":\"batch_cancelled\"}");
            return;
        }
        FakeHttpServer::send_response(fd, 400, "Bad Request", {}, "{}");
    }));

    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());
    QString file1 = tempDir.filePath("file1.bin");
    QString file2 = tempDir.filePath("file2.bin");
    QString file3 = tempDir.filePath("file3.bin");
    for (const auto& fpath : {file1, file2, file3}) {
        QFile f(fpath);
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write(QByteArray(100, 'K'));
        f.close();
    }

    webclip::FileUploadManager mgr;
    std::atomic<bool> cancelled{false};
    std::string cancelled_batch_id;
    mgr.set_batch_cancelled_callback([&cancelled, &cancelled_batch_id](const std::string& batch_id) {
        cancelled_batch_id = batch_id;
        cancelled.store(true);
    });

    for (int i = 1; i <= 3; ++i) {
        webclip::FileUploadTask task;
        task.upload_id = "up_410_" + std::to_string(i);
        task.local_path = (i == 1 ? file1 : (i == 2 ? file2 : file3)).toStdString();
        task.file_name = "file" + std::to_string(i) + ".bin";
        task.file_size = 100;
        task.batch_id = "batch_cancelled_123";
        task.index = i;
        task.total = 3;
        task.host = "127.0.0.1";
        task.port = server.port();
        task.pairing_code = "1234";
        mgr.enqueue_task(task);
    }

    for (int i = 0; i < 30 && !cancelled.load(); ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    QVERIFY(cancelled.load());
    QCOMPARE(QString::fromStdString(cancelled_batch_id), QString("batch_cancelled_123"));
    std::this_thread::sleep_for(std::chrono::milliseconds(300));
    QCOMPARE(post_count->load(), 1);
}

void TestFileTransfer::testUpload429WithRetryAfter() {
    std::shared_ptr<std::atomic<int>> post_count = std::make_shared<std::atomic<int>>(0);

    FakeHttpServer server;
    QVERIFY(server.start_with_body([post_count](int fd, const std::string& method, const std::string& path, const auto&, const std::string&) {
        if (method == "POST" && path == "/files/upload") {
            int c = post_count->fetch_add(1);
            if (c == 0) {
                FakeHttpServer::send_response(fd, 429, "Too Many Requests", {{"Retry-After", "1"}}, "{\"error\":\"rate_limited\"}");
                return;
            } else {
                FakeHttpServer::send_response(fd, 200, "OK", {{"Content-Type", "application/json"}}, "{\"ok\":true,\"status\":\"complete\"}");
                return;
            }
        } else if (method == "GET") {
            FakeHttpServer::send_response(fd, 404, "Not Found", {}, "{}");
            return;
        }
        FakeHttpServer::send_response(fd, 400, "Bad Request", {}, "{}");
    }));

    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());
    QString filePath = tempDir.filePath("test_429.bin");
    {
        QFile f(filePath);
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write(QByteArray(100, 'T'));
        f.close();
    }

    webclip::FileUploadManager mgr;
    std::atomic<bool> completed{false};
    mgr.set_completion_callback([&completed](const std::string&, int) {
        completed.store(true);
    });

    webclip::FileUploadTask task;
    task.upload_id = "up_429";
    task.local_path = filePath.toStdString();
    task.file_name = "test_429.bin";
    task.file_size = 100;
    task.batch_id = "b429";
    task.index = 1;
    task.total = 1;
    task.host = "127.0.0.1";
    task.port = server.port();
    task.pairing_code = "1234";

    auto start_time = std::chrono::steady_clock::now();
    mgr.enqueue_task(task);

    for (int i = 0; i < 60 && !completed.load(); ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start_time).count();
    QVERIFY(completed.load());
    QCOMPARE(post_count->load(), 2);
    QVERIFY(elapsed_ms >= 900);
}

void TestFileTransfer::testUploadSourceFileDisappearsOrChangesSizeFailsCleanlyAndBatchContinues() {
    std::shared_ptr<std::atomic<int>> post_count = std::make_shared<std::atomic<int>>(0);

    FakeHttpServer server;
    QVERIFY(server.start_with_body([post_count](int fd, const std::string& method, const std::string& path, const auto&, const std::string&) {
        if (method == "POST" && path == "/files/upload") {
            post_count->fetch_add(1);
            FakeHttpServer::send_response(fd, 200, "OK", {{"Content-Type", "application/json"}}, "{\"ok\":true,\"status\":\"complete\"}");
            return;
        }
        FakeHttpServer::send_response(fd, 400, "Bad Request", {}, "{}");
    }));

    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());
    QString file1 = tempDir.filePath("file1.bin");
    QString file2 = tempDir.filePath("file2.bin");
    QString file3 = tempDir.filePath("file3.bin");
    {
        QFile f1(file1);
        QVERIFY(f1.open(QIODevice::WriteOnly));
        f1.write(QByteArray(100, '1'));
        f1.close();

        QFile f2(file2);
        QVERIFY(f2.open(QIODevice::WriteOnly));
        f2.write(QByteArray(100, '2'));
        f2.close();

        QFile f3(file3);
        QVERIFY(f3.open(QIODevice::WriteOnly));
        f3.write(QByteArray(100, '3'));
        f3.close();
    }

    webclip::FileUploadManager mgr;
    std::atomic<bool> completed{false};
    int completed_total = 0;
    mgr.set_completion_callback([&completed, &completed_total](const std::string&, int total_files) {
        completed_total = total_files;
        completed.store(true);
    });

    std::vector<std::pair<std::string, std::string>> failures;
    std::mutex fail_mutex;
    mgr.set_failure_callback([&failures, &fail_mutex](const std::string& file_name, const std::string& reason) {
        std::lock_guard<std::mutex> lock(fail_mutex);
        failures.push_back({file_name, reason});
    });

    webclip::FileUploadTask task1;
    task1.upload_id = "task_1";
    task1.local_path = file1.toStdString();
    task1.file_name = "file1.bin";
    task1.file_size = 100;
    task1.batch_id = "b_resilient";
    task1.index = 1;
    task1.total = 3;
    task1.host = "127.0.0.1";
    task1.port = server.port();
    task1.pairing_code = "1234";

    webclip::FileUploadTask task2;
    task2.upload_id = "task_2";
    task2.local_path = file2.toStdString();
    task2.file_name = "file2.bin";
    task2.file_size = 100;
    task2.batch_id = "b_resilient";
    task2.index = 2;
    task2.total = 3;
    task2.host = "127.0.0.1";
    task2.port = server.port();
    task2.pairing_code = "1234";

    webclip::FileUploadTask task3;
    task3.upload_id = "task_3";
    task3.local_path = file3.toStdString();
    task3.file_name = "file3.bin";
    task3.file_size = 100;
    task3.batch_id = "b_resilient";
    task3.index = 3;
    task3.total = 3;
    task3.host = "127.0.0.1";
    task3.port = server.port();
    task3.pairing_code = "1234";

    mgr.enqueue_task(task1);
    mgr.enqueue_task(task2);
    mgr.enqueue_task(task3);

    QFile::remove(file2);

    for (int i = 0; i < 50 && !completed.load(); ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    QVERIFY(completed.load());
    QCOMPARE(completed_total, 2);
    QCOMPARE(post_count->load(), 2);
    {
        std::lock_guard<std::mutex> lock(fail_mutex);
        QCOMPARE(failures.size(), 1);
        QCOMPARE(QString::fromStdString(failures[0].first), QString("file2.bin"));
        QCOMPARE(QString::fromStdString(failures[0].second), QString("File not found"));
    }
}

void TestFileTransfer::testTransferManagersIsBusyAndCancelAll() {
    FakeHttpServer server;
    QVERIFY(server.start_with_body([](int fd, const std::string&, const std::string&, const std::map<std::string, std::string>&, const std::string&) {
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        FakeHttpServer::send_response(fd, 200, "OK", {{"Content-Type", "application/json"}}, "{\"ok\":true,\"status\":\"complete\"}");
    }));

    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());
    QString testFile = tempDir.filePath("busy_test.bin");
    {
        QFile f(testFile);
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write(QByteArray(100, 'Z'));
        f.close();
    }

    webclip::FileUploadManager up_mgr;
    QVERIFY(!up_mgr.is_busy());

    webclip::FileUploadTask up_task;
    up_task.upload_id = "busy_up";
    up_task.local_path = testFile.toStdString();
    up_task.file_name = "busy_test.bin";
    up_task.file_size = 100;
    up_task.batch_id = "b_busy";
    up_task.index = 1;
    up_task.total = 2;
    up_task.host = "127.0.0.1";
    up_task.port = server.port();
    up_task.pairing_code = "1234";

    webclip::FileUploadTask up_task2 = up_task;
    up_task2.upload_id = "busy_up_2";
    up_task2.index = 2;

    up_mgr.enqueue_task(up_task);
    up_mgr.enqueue_task(up_task2);

    QVERIFY(up_mgr.is_busy());
    up_mgr.cancel_all();

    for (int i = 0; i < 50 && up_mgr.is_busy(); ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
    QVERIFY(!up_mgr.is_busy());

    webclip::FileDownloadManager down_mgr;
    QVERIFY(!down_mgr.is_busy());

    webclip::FileOfferTask down_task;
    down_task.token = "busy_down";
    down_task.name = "busy_down.bin";
    down_task.size = 100;
    down_task.batch_id = "b_down_busy";
    down_task.index = 1;
    down_task.total = 2;
    down_task.host = "127.0.0.1";
    down_task.port = server.port();
    down_task.pairing_code = "1234";

    webclip::FileOfferTask down_task2 = down_task;
    down_task2.token = "busy_down_2";
    down_task2.index = 2;

    down_mgr.enqueue_task(down_task);
    down_mgr.enqueue_task(down_task2);

    QVERIFY(down_mgr.is_busy());
    down_mgr.cancel_all();

    for (int i = 0; i < 50 && down_mgr.is_busy(); ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
    QVERIFY(!down_mgr.is_busy());
}

void TestFileTransfer::testUploadIdempotencyCompleteStatusRouteReturnsSuccessWithoutResending() {
    auto post_count = std::make_shared<std::atomic<int>>(0);
    auto status_called = std::make_shared<std::atomic<bool>>(false);
    FakeHttpServer server;
    QVERIFY(server.start_with_body([post_count, status_called](int fd, const std::string& method, const std::string& path, const std::map<std::string, std::string>&, const std::string&) {
        if (method == "POST" && path == "/files/upload") {
            post_count->fetch_add(1);
            FakeHttpServer::send_response(fd, 503, "Service Unavailable", {{"Retry-After", "1"}}, "{\"error\":\"busy\"}");
            return;
        } else if (method == "GET" && path.rfind("/files/upload/", 0) == 0) {
            status_called->store(true);
            FakeHttpServer::send_response(fd, 200, "OK", {{"Content-Type", "application/json"}}, "{\"offset\":100,\"size\":100,\"complete\":true}");
            return;
        }
        FakeHttpServer::send_response(fd, 400, "Bad Request", {}, "{}");
    }));

    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());
    QString filePath = tempDir.filePath("test_idem_desktop.bin");
    {
        QFile f(filePath);
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write(QByteArray(100, 'I'));
        f.close();
    }

    webclip::FileUploadManager mgr;
    std::atomic<bool> completed{false};
    mgr.set_completion_callback([&completed](const std::string&, int) {
        completed.store(true);
    });

    webclip::FileUploadTask task;
    task.upload_id = "up_idem";
    task.local_path = filePath.toStdString();
    task.file_name = "test_idem_desktop.bin";
    task.file_size = 100;
    task.batch_id = "b_idem";
    task.index = 1;
    task.total = 1;
    task.host = "127.0.0.1";
    task.port = server.port();
    task.pairing_code = "1234";

    mgr.enqueue_task(task);

    for (int i = 0; i < 50 && !completed.load(); ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    QVERIFY(completed.load());
    QVERIFY(status_called->load());
    QCOMPARE(post_count->load(), 1);
}

void TestFileTransfer::testUploadZeroByteFile() {
    auto received_post = std::make_shared<std::atomic<bool>>(false);
    auto zero_cl = std::make_shared<std::atomic<bool>>(false);
    auto no_expect = std::make_shared<std::atomic<bool>>(false);

    FakeHttpServer server;
    QVERIFY(server.start_with_body([received_post, zero_cl, no_expect](int fd, const std::string& method, const std::string& path, const std::map<std::string, std::string>& headers, const std::string& body) {
        if (method == "POST" && path == "/files/upload") {
            received_post->store(true);
            auto cl_it = headers.find("content-length");
            if (cl_it != headers.end() && cl_it->second == "0" && body.empty()) {
                zero_cl->store(true);
            }
            if (headers.find("expect") == headers.end()) {
                no_expect->store(true);
            }
            FakeHttpServer::send_response(fd, 200, "OK", {{"Content-Type", "application/json"}}, "{\"ok\":true,\"status\":\"complete\"}");
            return;
        }
        FakeHttpServer::send_response(fd, 400, "Bad Request", {}, "{}");
    }));

    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());
    QString filePath = tempDir.filePath("test_zero.bin");
    {
        QFile f(filePath);
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.close();
    }

    webclip::FileUploadManager mgr;
    std::atomic<bool> completed{false};
    mgr.set_completion_callback([&completed](const std::string&, int) {
        completed.store(true);
    });

    webclip::FileUploadTask task;
    task.upload_id = "up_zero";
    task.local_path = filePath.toStdString();
    task.file_name = "test_zero.bin";
    task.file_size = 0;
    task.batch_id = "b_zero";
    task.index = 1;
    task.total = 1;
    task.host = "127.0.0.1";
    task.port = server.port();
    task.pairing_code = "1234";

    mgr.enqueue_task(task);

    for (int i = 0; i < 50 && !completed.load(); ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    QVERIFY(completed.load());
    QVERIFY(received_post->load());
    QVERIFY(zero_cl->load());
    QVERIFY(no_expect->load());
}

void TestFileTransfer::testDownloadZeroByteFile() {
    FakeHttpServer server;
    QVERIFY(server.start([](int fd, const std::string& method, const std::string& path, const std::map<std::string, std::string>&) {
        if (method == "GET" && path == "/files/token_zero") {
            FakeHttpServer::send_response(fd, 200, "OK", {{"Content-Type", "application/octet-stream"}, {"Content-Length", "0"}}, "");
            return;
        }
        FakeHttpServer::send_response(fd, 404, "Not Found", {}, "{}");
    }));

    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());
    QSettings settings("Burhanverse", "WebClip");
    settings.setValue("downloadDirectory", tempDir.path());

    webclip::FileDownloadManager mgr;
    std::atomic<bool> completed{false};
    mgr.set_completion_callback([&completed](const std::string&, int) {
        completed.store(true);
    });

    webclip::FileOfferTask task;
    task.token = "token_zero";
    task.name = "zero_download.bin";
    task.size = 0;
    task.batch_id = "b_dl_zero";
    task.index = 1;
    task.total = 1;
    task.host = "127.0.0.1";
    task.port = server.port();
    task.pairing_code = "1234";

    mgr.enqueue_task(task);

    for (int i = 0; i < 50 && !completed.load(); ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    QVERIFY(completed.load());
    QString downloadedPath = QDir(tempDir.path()).filePath("zero_download.bin");
    QFileInfo fi(downloadedPath);
    QVERIFY(fi.exists());
    QCOMPARE(fi.size(), 0);

    settings.remove("downloadDirectory");
}

void TestFileTransfer::testUploadOfferPendingThenAcceptedProceedsToUpload() {
    QTemporaryFile tempFile;
    QVERIFY(tempFile.open());
    tempFile.write("consent approved upload payload");
    tempFile.flush();
    const QString filePath = tempFile.fileName();
    const qint64 fileSize = tempFile.size();

    std::atomic<int> getOfferPolls{0};
    FakeHttpServer server;
    QVERIFY(server.start_with_body([&getOfferPolls](int fd, const std::string& method, const std::string& path, const std::map<std::string, std::string>&, const std::string&) {
        if (method == "POST" && path == "/files/offer") {
            FakeHttpServer::send_response(fd, 200, "OK", {{"Content-Type", "application/json"}}, "{\"ok\":true,\"state\":\"pending\"}");
            return;
        }
        if (method == "GET" && path == "/files/offer/b_consent") {
            int poll = ++getOfferPolls;
            if (poll >= 2) {
                FakeHttpServer::send_response(fd, 200, "OK", {{"Content-Type", "application/json"}}, "{\"ok\":true,\"state\":\"accepted\"}");
            } else {
                FakeHttpServer::send_response(fd, 200, "OK", {{"Content-Type", "application/json"}}, "{\"ok\":true,\"state\":\"pending\"}");
            }
            return;
        }
        if (method == "POST" && path == "/files/upload") {
            FakeHttpServer::send_response(fd, 200, "OK", {{"Content-Type", "application/json"}}, "{\"ok\":true}");
            return;
        }
        FakeHttpServer::send_response(fd, 404, "Not Found", {}, "{}");
    }));

    webclip::FileUploadManager mgr;
    std::atomic<bool> completed{false};
    mgr.set_completion_callback([&completed](const std::string&, int) {
        completed.store(true);
    });

    webclip::FileUploadTask task;
    task.upload_id = "up_consent_1";
    task.local_path = filePath.toStdString();
    task.file_name = "consent.bin";
    task.file_size = fileSize;
    task.batch_id = "b_consent";
    task.index = 1;
    task.total = 1;
    task.host = "127.0.0.1";
    task.port = server.port();
    task.pairing_code = "1234";
    task.batch_files = {{task.file_name, task.local_path, task.file_size}};
    task.batch_total_size = task.file_size;

    mgr.enqueue_task(task);

    for (int i = 0; i < 50 && !completed.load(); ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    QVERIFY(completed.load());
    QVERIFY(getOfferPolls.load() >= 2);
}

void TestFileTransfer::testUploadOffer409NotificationsDisabledFailsImmediately() {
    QTemporaryFile tempFile;
    QVERIFY(tempFile.open());
    tempFile.write("offer test payload");
    tempFile.flush();
    const QString filePath = tempFile.fileName();
    const qint64 fileSize = tempFile.size();

    FakeHttpServer server;
    QVERIFY(server.start([](int fd, const std::string& method, const std::string& path, const std::map<std::string, std::string>&) {
        if (method == "POST" && path == "/files/offer") {
            FakeHttpServer::send_response(fd, 409, "Conflict", {{"Content-Type", "application/json"}}, "{\"ok\":false,\"error\":\"notifications_disabled\"}");
            return;
        }
        FakeHttpServer::send_response(fd, 404, "Not Found", {}, "{}");
    }));

    webclip::FileUploadManager mgr;
    std::atomic<bool> failed{false};
    std::string failureReason;
    mgr.set_failure_callback([&failed, &failureReason](const std::string&, const std::string& reason) {
        failed.store(true);
        failureReason = reason;
    });

    webclip::FileUploadTask task;
    task.upload_id = "up_notif_dis";
    task.local_path = filePath.toStdString();
    task.file_name = "notif_dis.bin";
    task.file_size = fileSize;
    task.batch_id = "b_notif_dis";
    task.index = 1;
    task.total = 1;
    task.host = "127.0.0.1";
    task.port = server.port();
    task.pairing_code = "1234";
    task.batch_files = {{task.file_name, task.local_path, task.file_size}};
    task.batch_total_size = task.file_size;

    mgr.enqueue_task(task);

    for (int i = 0; i < 50 && !failed.load(); ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    QVERIFY(failed.load());
    QVERIFY(failureReason.find("notifications disabled") != std::string::npos);
}

void TestFileTransfer::testUploadOfferDeclinedAborts() {
    QTemporaryFile tempFile;
    QVERIFY(tempFile.open());
    tempFile.write("offer test payload");
    tempFile.flush();
    const QString filePath = tempFile.fileName();
    const qint64 fileSize = tempFile.size();

    FakeHttpServer server;
    QVERIFY(server.start([](int fd, const std::string& method, const std::string& path, const std::map<std::string, std::string>&) {
        if (method == "POST" && path == "/files/offer") {
            FakeHttpServer::send_response(fd, 200, "OK", {{"Content-Type", "application/json"}}, "{\"ok\":true,\"state\":\"declined\"}");
            return;
        }
        FakeHttpServer::send_response(fd, 404, "Not Found", {}, "{}");
    }));

    webclip::FileUploadManager mgr;
    std::atomic<bool> failed{false};
    std::string failureReason;
    mgr.set_failure_callback([&failed, &failureReason](const std::string&, const std::string& reason) {
        failed.store(true);
        failureReason = reason;
    });

    webclip::FileUploadTask task;
    task.upload_id = "up_declined";
    task.local_path = filePath.toStdString();
    task.file_name = "declined.bin";
    task.file_size = fileSize;
    task.batch_id = "b_declined";
    task.index = 1;
    task.total = 1;
    task.host = "127.0.0.1";
    task.port = server.port();
    task.pairing_code = "1234";
    task.batch_files = {{task.file_name, task.local_path, task.file_size}};
    task.batch_total_size = task.file_size;

    mgr.enqueue_task(task);

    for (int i = 0; i < 50 && !failed.load(); ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    QVERIFY(failed.load());
    QCOMPARE(QString::fromStdString(failureReason), QStringLiteral("Transfer declined on phone"));
}

void TestFileTransfer::testUploadOfferTimeoutAborts() {
    QTemporaryFile tempFile;
    QVERIFY(tempFile.open());
    tempFile.write("offer test payload");
    tempFile.flush();
    const QString filePath = tempFile.fileName();
    const qint64 fileSize = tempFile.size();

    FakeHttpServer server;
    QVERIFY(server.start([](int fd, const std::string& method, const std::string& path, const std::map<std::string, std::string>&) {
        if (method == "POST" && path == "/files/offer") {
            FakeHttpServer::send_response(fd, 200, "OK", {{"Content-Type", "application/json"}}, "{\"ok\":true,\"state\":\"pending\"}");
            return;
        }
        if (method == "GET" && path.find("/files/offer/") == 0) {
            FakeHttpServer::send_response(fd, 200, "OK", {{"Content-Type", "application/json"}}, "{\"ok\":true,\"state\":\"expired\"}");
            return;
        }
        FakeHttpServer::send_response(fd, 404, "Not Found", {}, "{}");
    }));

    webclip::FileUploadManager mgr;
    std::atomic<bool> failed{false};
    std::string failureReason;
    mgr.set_failure_callback([&failed, &failureReason](const std::string&, const std::string& reason) {
        failed.store(true);
        failureReason = reason;
    });

    webclip::FileUploadTask task;
    task.upload_id = "up_timeout";
    task.local_path = filePath.toStdString();
    task.file_name = "timeout.bin";
    task.file_size = fileSize;
    task.batch_id = "b_timeout";
    task.index = 1;
    task.total = 1;
    task.host = "127.0.0.1";
    task.port = server.port();
    task.pairing_code = "1234";
    task.batch_files = {{task.file_name, task.local_path, task.file_size}};
    task.batch_total_size = task.file_size;

    mgr.enqueue_task(task);

    for (int i = 0; i < 50 && !failed.load(); ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    QVERIFY(failed.load());
    QCOMPARE(QString::fromStdString(failureReason), QStringLiteral("Transfer timed out waiting for approval"));
}

void TestFileTransfer::testUploadOfferPolledDeclinedAborts() {
    QTemporaryFile tempFile;
    QVERIFY(tempFile.open());
    tempFile.write("offer test payload");
    tempFile.flush();
    const QString filePath = tempFile.fileName();
    const qint64 fileSize = tempFile.size();

    FakeHttpServer server;
    QVERIFY(server.start([](int fd, const std::string& method, const std::string& path, const std::map<std::string, std::string>&) {
        if (method == "POST" && path == "/files/offer") {
            FakeHttpServer::send_response(fd, 200, "OK", {{"Content-Type", "application/json"}}, "{\"ok\":true,\"state\":\"pending\"}");
            return;
        }
        if (method == "GET" && path.find("/files/offer/") == 0) {
            FakeHttpServer::send_response(fd, 200, "OK", {{"Content-Type", "application/json"}}, "{\"ok\":true,\"state\":\"declined\"}");
            return;
        }
        FakeHttpServer::send_response(fd, 404, "Not Found", {}, "{}");
    }));

    webclip::FileUploadManager mgr;
    std::atomic<bool> failed{false};
    std::string failureReason;
    mgr.set_failure_callback([&failed, &failureReason](const std::string&, const std::string& reason) {
        failed.store(true);
        failureReason = reason;
    });

    webclip::FileUploadTask task;
    task.upload_id = "up_polled_declined";
    task.local_path = filePath.toStdString();
    task.file_name = "declined2.bin";
    task.file_size = fileSize;
    task.batch_id = "b_polled_declined";
    task.index = 1;
    task.total = 1;
    task.host = "127.0.0.1";
    task.port = server.port();
    task.pairing_code = "1234";
    task.batch_files = {{task.file_name, task.local_path, task.file_size}};
    task.batch_total_size = task.file_size;

    mgr.enqueue_task(task);

    for (int i = 0; i < 50 && !failed.load(); ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    QVERIFY(failed.load());
    QCOMPARE(QString::fromStdString(failureReason), QStringLiteral("Transfer declined on phone"));
}

void TestFileTransfer::testIncomingOfferDebounceGrouping() {
    struct TestDebouncer {
        int expectedTotal = 0;
        std::vector<std::string> files;
        int flushCount = 0;
        QTimer timer;

        TestDebouncer() {
            timer.setSingleShot(true);
            QObject::connect(&timer, &QTimer::timeout, [this]() {
                flush();
            });
        }

        void onFileOffer(const std::string& name, int total) {
            expectedTotal = total;
            files.push_back(name);
            if (expectedTotal > 0 && static_cast<int>(files.size()) >= expectedTotal) {
                timer.stop();
                flush();
            } else {
                timer.start(400);
            }
        }

        void flush() {
            ++flushCount;
        }
    };

    {
        TestDebouncer debouncer;
        debouncer.onFileOffer("f1.txt", 3);
        QCOMPARE(debouncer.flushCount, 0);
        QVERIFY(debouncer.timer.isActive());

        debouncer.onFileOffer("f2.txt", 3);
        QCOMPARE(debouncer.flushCount, 0);
        QVERIFY(debouncer.timer.isActive());

        debouncer.onFileOffer("f3.txt", 3);
        QCOMPARE(debouncer.flushCount, 1);
        QVERIFY(!debouncer.timer.isActive());
        QCOMPARE(static_cast<int>(debouncer.files.size()), 3);
    }

    {
        TestDebouncer debouncer;
        debouncer.onFileOffer("partial.txt", 5);
        QCOMPARE(debouncer.flushCount, 0);
        QVERIFY(debouncer.timer.isActive());

        QEventLoop loop;
        QTimer::singleShot(450, &loop, &QEventLoop::quit);
        loop.exec();

        QCOMPARE(debouncer.flushCount, 1);
        QCOMPARE(static_cast<int>(debouncer.files.size()), 1);
    }
}

class MockClipboard : public webclip::IClipboard {
public:
    std::string text_;
    std::vector<uint8_t> image_data_;
    std::string image_mime_;
    std::atomic<bool> set_image_called{false};
    std::atomic<bool> set_text_called{false};

    std::string get_text() override { return text_; }
    bool set_text(const std::string& text) override {
        text_ = text;
        set_text_called.store(true);
        return true;
    }
    bool has_image() override { return !image_data_.empty(); }
    webclip::ClipboardImage get_image() override {
        webclip::ClipboardImage img;
        img.data = image_data_;
        img.mime_type = image_mime_;
        img.valid = !image_data_.empty();
        return img;
    }
    bool set_image(const std::vector<uint8_t>& data, const std::string& mime_type) override {
        image_data_ = data;
        image_mime_ = mime_type;
        set_image_called.store(true);
        return true;
    }
    std::string get_backend_name() const override { return "MockClipboard"; }
};

void TestFileTransfer::testSseImageClipWithHasImageFetchesImageAndDoesNotAddText() {
    FakeHttpServer server;
    std::atomic<bool> getImageRequested{false};
    std::atomic<bool> getImageHadAuth{false};

    QVERIFY(server.start([&](int fd, const std::string&, const std::string& path, const auto& headers) {
        if (path == "/state") {
            FakeHttpServer::send_response(fd, 200, "OK", {{"Content-Type", "application/json"}},
                                          "{\"ok\":true,\"hasImage\":false,\"text\":\"\"}");
            return;
        }
        if (path == "/events") {
            auto it = headers.find("x-pairing-code");
            std::string code = (it != headers.end()) ? it->second : "";
            if (code != "1234") {
                FakeHttpServer::send_response(fd, 401, "Unauthorized", {{"Content-Type", "application/json"}},
                                              "{\"ok\":false}");
                return;
            }
            std::string sseHeaders = "HTTP/1.1 200 OK\r\nContent-Type: text/event-stream\r\nCache-Control: no-cache\r\n\r\n";
            ::send(fd, sseHeaders.data(), sseHeaders.size(), 0);

            std::string sseData = "event: clipboard\r\ndata: {\"type\":\"text\",\"hasImage\":true,\"imageUrl\":\"/image/latest?source=phone\",\"text\":\"/storage/emulated/0/DCIM/Screenshots/shot.png\"}\r\n\r\n";
            ::send(fd, sseData.data(), sseData.size(), 0);

            while (true) {
                char dummy[64];
                ssize_t n = ::recv(fd, dummy, sizeof(dummy), 0);
                if (n <= 0) break;
            }
            return;
        }
        if (path == "/image/latest") {
            getImageRequested.store(true);
            auto it = headers.find("x-pairing-code");
            if (it != headers.end() && it->second == "1234") {
                getImageHadAuth.store(true);
                std::string pngBytes = "\x89PNG\r\n\x1a\n\x00\x01";
                FakeHttpServer::send_response(fd, 200, "OK", {{"Content-Type", "image/png"}}, pngBytes);
                return;
            }
            FakeHttpServer::send_response(fd, 401, "Unauthorized", {{"Content-Type", "application/json"}},
                                          "{\"ok\":false}");
            return;
        }
        FakeHttpServer::send_response(fd, 404, "Not Found", {{"Content-Type", "application/json"}}, "{\"ok\":false}");
    }));

    auto mockClip = std::make_unique<MockClipboard>();
    MockClipboard* rawClip = mockClip.get();

    webclip::SyncConfig config;
    config.host = "127.0.0.1";
    config.port = server.port();
    config.code = "1234";
    config.client_id = "test-desktop";

    auto syncMgr = std::make_unique<webclip::SyncManager>(config, std::move(mockClip));
    std::thread syncThread([&]() {
        syncMgr->run();
    });

    for (int i = 0; i < 40; ++i) {
        if (rawClip->set_image_called.load() || rawClip->set_text_called.load()) {
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }

    syncMgr->stop();
    if (syncThread.joinable()) {
        syncThread.join();
    }

    QVERIFY(getImageRequested.load());
    QVERIFY(getImageHadAuth.load());
    QVERIFY(rawClip->set_image_called.load());
    QVERIFY(!rawClip->set_text_called.load());
    QVERIFY(rawClip->text_.empty());
}

void TestFileTransfer::testSseImageClipFetchFailureLeavesClipboardClean() {
    FakeHttpServer server;

    QVERIFY(server.start([&](int fd, const std::string&, const std::string& path, const auto&) {
        if (path == "/state") {
            FakeHttpServer::send_response(fd, 200, "OK", {{"Content-Type", "application/json"}},
                                          "{\"ok\":true,\"hasImage\":false,\"text\":\"\"}");
            return;
        }
        if (path == "/events") {
            std::string sseHeaders = "HTTP/1.1 200 OK\r\nContent-Type: text/event-stream\r\nCache-Control: no-cache\r\n\r\n";
            ::send(fd, sseHeaders.data(), sseHeaders.size(), 0);

            std::string sseData = "event: clipboard\r\ndata: {\"type\":\"text\",\"hasImage\":true,\"imageUrl\":\"/image/latest?source=phone\",\"text\":\"/storage/emulated/0/DCIM/Screenshots/shot.png\"}\r\n\r\n";
            ::send(fd, sseData.data(), sseData.size(), 0);

            while (true) {
                char dummy[64];
                ssize_t n = ::recv(fd, dummy, sizeof(dummy), 0);
                if (n <= 0) break;
            }
            return;
        }
        if (path == "/image/latest") {
            FakeHttpServer::send_response(fd, 500, "Internal Server Error", {{"Content-Type", "application/json"}},
                                          "{\"ok\":false}");
            return;
        }
        FakeHttpServer::send_response(fd, 404, "Not Found", {{"Content-Type", "application/json"}}, "{\"ok\":false}");
    }));

    auto mockClip = std::make_unique<MockClipboard>();
    MockClipboard* rawClip = mockClip.get();

    webclip::SyncConfig config;
    config.host = "127.0.0.1";
    config.port = server.port();
    config.code = "1234";
    config.client_id = "test-desktop";

    auto syncMgr = std::make_unique<webclip::SyncManager>(config, std::move(mockClip));
    std::thread syncThread([&]() {
        syncMgr->run();
    });

    for (int i = 0; i < 30; ++i) {
        if (rawClip->set_image_called.load() || rawClip->set_text_called.load()) {
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }

    syncMgr->stop();
    if (syncThread.joinable()) {
        syncThread.join();
    }

    QVERIFY(!rawClip->set_image_called.load());
    QVERIFY(!rawClip->set_text_called.load());
    QVERIFY(rawClip->text_.empty());
}

void TestFileTransfer::testClipboardTextWithImagePathPushesImageNotText() {
    FakeHttpServer server;
    std::atomic<bool> imagePushed{false};
    std::atomic<bool> textPushed{false};

    QVERIFY(server.start_with_body([&](int fd, const std::string&, const std::string& path, const auto&, const std::string& body) {
        if (path.find("/state") == 0) {
            FakeHttpServer::send_response(fd, 200, "OK", {{"Content-Type", "application/json"}},
                                          "{\"ok\":true,\"hasImage\":false,\"text\":\"\"}");
            return;
        }
        if (path.find("/events") == 0) {
            std::string sseHeaders = "HTTP/1.1 200 OK\r\nContent-Type: text/event-stream\r\nCache-Control: no-cache\r\n\r\n";
            ::send(fd, sseHeaders.data(), sseHeaders.size(), 0);
            while (true) {
                std::this_thread::sleep_for(std::chrono::milliseconds(200));
                std::string ping = ": ping\n\n";
                if (::send(fd, ping.data(), ping.size(), MSG_NOSIGNAL) <= 0) break;
            }
            return;
        }
        if (path.find("/clipboard") == 0) {
            webclip::JsonValue val = webclip::JsonValue::parse(body);
            std::string type = val.get_string("type");
            if (type == "image") {
                imagePushed.store(true);
            } else if (type == "text" || !val.get_string("text").empty()) {
                textPushed.store(true);
            }
            FakeHttpServer::send_response(fd, 200, "OK", {{"Content-Type", "application/json"}}, "{\"ok\":true}");
            return;
        }
        FakeHttpServer::send_response(fd, 404, "Not Found", {{"Content-Type", "application/json"}}, "{\"ok\":false}");
    }));

    QTemporaryFile tempPng;
    tempPng.setFileTemplate(QDir::tempPath() + "/webclip_test_XXXXXX.png");
    QVERIFY(tempPng.open());
    const char pngBytes[] = "\x89PNG\r\n\x1a\n\x00\x00\x00\rIHDR\x00\x00\x00\x01\x00\x00\x00\x01\x08\x06\x00\x00\x00\x1f" "\x15" "c4\x00\x00\x00\rIDATx" "\x9c" "c`\x00\x00\x00\x02\x00\x01H" "\xaf" "\xa4" "q\x00\x00\x00\x00IEND" "\xae" "B`\x82";
    tempPng.write(pngBytes, sizeof(pngBytes) - 1);
    tempPng.flush();

    auto mockClip = std::make_unique<MockClipboard>();
    MockClipboard* rawClip = mockClip.get();
    rawClip->text_ = "";

    webclip::SyncConfig config;
    config.host = "127.0.0.1";
    config.port = server.port();
    config.code = "1234";
    config.client_id = "test-desktop";
    config.poll_interval_sec = 0.05;

    auto syncMgr = std::make_unique<webclip::SyncManager>(config, std::move(mockClip));
    std::thread syncThread([&]() {
        syncMgr->run();
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    rawClip->text_ = tempPng.fileName().toStdString();

    for (int i = 0; i < 40; ++i) {
        if (imagePushed.load() || textPushed.load()) {
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }

    syncMgr->stop();
    if (syncThread.joinable()) {
        syncThread.join();
    }

    QVERIFY(imagePushed.load());
    QVERIFY(!textPushed.load());
}

void TestFileTransfer::testClipboardTextWithNonImageOrMissingFilePushesText() {
    FakeHttpServer server;
    std::atomic<bool> imagePushed{false};
    std::atomic<bool> textPushed{false};

    QVERIFY(server.start_with_body([&](int fd, const std::string&, const std::string& path, const auto&, const std::string& body) {
        if (path.find("/state") == 0) {
            FakeHttpServer::send_response(fd, 200, "OK", {{"Content-Type", "application/json"}},
                                          "{\"ok\":true,\"hasImage\":false,\"text\":\"\"}");
            return;
        }
        if (path.find("/events") == 0) {
            std::string sseHeaders = "HTTP/1.1 200 OK\r\nContent-Type: text/event-stream\r\nCache-Control: no-cache\r\n\r\n";
            ::send(fd, sseHeaders.data(), sseHeaders.size(), 0);
            while (true) {
                std::this_thread::sleep_for(std::chrono::milliseconds(200));
                std::string ping = ": ping\n\n";
                if (::send(fd, ping.data(), ping.size(), MSG_NOSIGNAL) <= 0) break;
            }
            return;
        }
        if (path.find("/clipboard") == 0) {
            webclip::JsonValue val = webclip::JsonValue::parse(body);
            std::string type = val.get_string("type");
            if (type == "image") {
                imagePushed.store(true);
            } else if (type == "text" || !val.get_string("text").empty()) {
                textPushed.store(true);
            }
            FakeHttpServer::send_response(fd, 200, "OK", {{"Content-Type", "application/json"}}, "{\"ok\":true}");
            return;
        }
        FakeHttpServer::send_response(fd, 404, "Not Found", {{"Content-Type", "application/json"}}, "{\"ok\":false}");
    }));

    QTemporaryFile tempTxt;
    tempTxt.setFileTemplate(QDir::tempPath() + "/webclip_test_XXXXXX.txt");
    QVERIFY(tempTxt.open());
    tempTxt.write("not an image");
    tempTxt.flush();

    auto mockClip = std::make_unique<MockClipboard>();
    MockClipboard* rawClip = mockClip.get();
    rawClip->text_ = "";

    webclip::SyncConfig config;
    config.host = "127.0.0.1";
    config.port = server.port();
    config.code = "1234";
    config.client_id = "test-desktop";
    config.poll_interval_sec = 0.05;

    auto syncMgr = std::make_unique<webclip::SyncManager>(config, std::move(mockClip));
    std::thread syncThread([&]() {
        syncMgr->run();
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    rawClip->text_ = tempTxt.fileName().toStdString();

    for (int i = 0; i < 40; ++i) {
        if (imagePushed.load() || textPushed.load()) {
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }

    syncMgr->stop();
    if (syncThread.joinable()) {
        syncThread.join();
    }

    QVERIFY(!imagePushed.load());
    QVERIFY(textPushed.load());
}

QTEST_MAIN(TestFileTransfer)
#include "test_file_transfer.moc"
