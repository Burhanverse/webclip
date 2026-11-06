#include <QCoreApplication>
#include <QSettings>
#include <QString>
#include <QTemporaryDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QtTest>
#include "gui/models/clipboard_history_model.hpp"

int main(int argc, char* argv[]) {
    QCoreApplication app(argc, argv);

    QString instanceName = "WebClip-ABC123";
    {
        QSettings s("Burhanverse", "WebClip");
        s.setValue("mdnsInstanceName", instanceName);
    }
    {
        QSettings s("Burhanverse", "WebClip");
        QString loaded = s.value("mdnsInstanceName", "").toString();
        if (loaded != instanceName) {
            qCritical() << "FAIL: expected" << instanceName << "got" << loaded;
            return 1;
        }
    }

    QTemporaryDir tempDir;
    if (!tempDir.isValid()) {
        qCritical() << "FAIL: temp dir invalid";
        return 1;
    }

    const QString histPath = tempDir.filePath("file_history.json");
    const QString sampleFilePath = tempDir.filePath("report.pdf");
    {
        QFile sampleFile(sampleFilePath);
        if (!sampleFile.open(QIODevice::WriteOnly)) {
            qCritical() << "FAIL: cannot create sample file";
            return 1;
        }
        sampleFile.write("dummy PDF content");
        sampleFile.close();
    }

    {
        webclip::ClipboardHistoryModel model;
        model.setPersistenceFilePath(histPath);
        model.addClip("volatile text", "local");
        model.addClipFile("report.pdf", 1024, sampleFilePath, "application/pdf", "phone", "batch-1", 1, 1, webclip::FileStatus::Completed);

        if (model.rowCount() != 2) {
            qCritical() << "FAIL: expected 2 rows, got" << model.rowCount();
            return 1;
        }
    }

    {
        QFile jsonFile(histPath);
        if (!jsonFile.open(QIODevice::ReadOnly)) {
            qCritical() << "FAIL: history file not created";
            return 1;
        }
        QByteArray data = jsonFile.readAll();
        jsonFile.close();

        QJsonDocument doc = QJsonDocument::fromJson(data);
        if (!doc.isArray()) {
            qCritical() << "FAIL: expected json array";
            return 1;
        }
        QJsonArray arr = doc.array();
        if (arr.size() != 1) {
            qCritical() << "FAIL: expected 1 persisted file, got" << arr.size();
            return 1;
        }
        if (data.contains("volatile text")) {
            qCritical() << "FAIL: ephemeral text was persisted";
            return 1;
        }
    }

    {
        webclip::ClipboardHistoryModel model;
        model.setPersistenceFilePath(histPath);
        model.loadHistory();

        if (model.rowCount() != 1) {
            qCritical() << "FAIL: expected 1 reloaded item, got" << model.rowCount();
            return 1;
        }
        const webclip::ClipItem* item = model.getItem(0);
        if (!item || item->type != webclip::ClipType::File || item->fileName != "report.pdf") {
            qCritical() << "FAIL: loaded file properties mismatch";
            return 1;
        }
    }

    QFile::remove(sampleFilePath);

    {
        webclip::ClipboardHistoryModel model;
        model.setPersistenceFilePath(histPath);
        model.loadHistory();

        if (model.rowCount() != 0) {
            qCritical() << "FAIL: deleted file should be dropped on load, got rowCount" << model.rowCount();
            return 1;
        }
    }

    {
        webclip::ClipboardHistoryModel model;
        model.addClipFile("test_progress.bin", 2000, QString(), "application/octet-stream", "phone", "batch-test", 1, 1, webclip::FileStatus::Pending);
        if (model.rowCount() != 1) {
            qCritical() << "FAIL: expected 1 row after addClipFile, got" << model.rowCount();
            return 1;
        }

        model.updateFileStatusByBatch("batch-test", webclip::FileStatus::Transferring);
        const webclip::ClipItem* item = model.getItem(0);
        if (!item || item->fileStatus != webclip::FileStatus::Transferring || item->bytesTransferred != 0) {
            qCritical() << "FAIL: status not transitioning to transferring";
            return 1;
        }

        model.updateFileProgress("batch-test", 1, "test_progress.bin", 1000, 2000);
        if (model.rowCount() != 1) {
            qCritical() << "FAIL: updateFileProgress duplicated rows, count:" << model.rowCount();
            return 1;
        }
        item = model.getItem(0);
        if (!item || item->bytesTransferred != 1000 || item->fileStatus != webclip::FileStatus::Transferring) {
            qCritical() << "FAIL: progress not updated correctly";
            return 1;
        }

        QString finalPath = tempDir.filePath("test_progress.bin");
        QFile finalFile(finalPath);
        if (finalFile.open(QIODevice::WriteOnly)) {
            finalFile.write("dummy");
            finalFile.close();
        }

        model.updateFileCompleted("batch-test", 1, "test_progress.bin", finalPath, 2000);
        if (model.rowCount() != 1) {
            qCritical() << "FAIL: updateFileCompleted duplicated rows, count:" << model.rowCount();
            return 1;
        }
        item = model.getItem(0);
        if (!item || item->fileStatus != webclip::FileStatus::Completed || item->localFilePath != finalPath || item->bytesTransferred != 2000) {
            qCritical() << "FAIL: completed state mismatch";
            return 1;
        }

        model.addClipFile("test_cancel.bin", 1000, QString(), "application/octet-stream", "phone", "batch-cancel", 1, 1, webclip::FileStatus::Transferring);
        model.cancelActiveTransfers();
        item = model.getItem(1);
        if (!item || item->fileStatus != webclip::FileStatus::Cancelled) {
            qCritical() << "FAIL: cancelActiveTransfers failed to cancel active transfer";
            return 1;
        }
    }

    qDebug() << "PASS: all persistence tests passed";
    return 0;
}
