#pragma once

#include <QAbstractListModel>
#include <QDateTime>
#include <QList>

namespace webclip {

enum class ClipType {
    Text,
    Image,
    File
};

enum class FileStatus {
    Pending,
    Transferring,
    Completed,
    Failed,
    Cancelled
};

struct ClipItem {
    QString id;
    ClipType type = ClipType::Text;
    bool isImage = false;
    QString text;
    QString imageData;
    QString mimeType;
    int imageSize = 0;
    int imgWidth = 0;
    int imgHeight = 0;
    QString source;
    qint64 timestamp = 0;
    bool textSpilled = false;
    qint64 fullChars = 0;

    QString fileName;
    qint64 fileSize = 0;
    QString localFilePath;
    QString batchId;
    int batchIndex = 1;
    int batchTotal = 1;
    FileStatus fileStatus = FileStatus::Completed;
    qint64 bytesTransferred = 0;

    qint64 charCount() const {
        if (type == ClipType::File) return fileSize;
        return isImage ? imageSize : (textSpilled ? fullChars : text.length());
    }

    QString formattedTime() const {
        QDateTime dt = QDateTime::fromMSecsSinceEpoch(timestamp);
        return dt.toString("hh:mm:ss AP");
    }

    QString formattedSize() const {
        if (type == ClipType::File) {
            if (fileSize < 1024) return QString::number(fileSize) + " B";
            if (fileSize < 1024 * 1024) return QString::number(fileSize / 1024.0, 'f', 1) + " KB";
            return QString::number(fileSize / (1024.0 * 1024.0), 'f', 1) + " MB";
        }
        if (!isImage) return QString::number(charCount()) + " chars";
        if (imageSize < 1024) return QString::number(imageSize) + " B";
        if (imageSize < 1024 * 1024) return QString::number(imageSize / 1024.0, 'f', 1) + " KB";
        return QString::number(imageSize / (1024.0 * 1024.0), 'f', 1) + " MB";
    }

    QString preview(int maxLen = 80) const {
        if (type == ClipType::File) {
            return "[File • " + fileName + " (" + formattedSize() + ")]";
        }
        if (isImage) return "[Image • " + formattedSize() + "]";
        QString clean = text;
        clean.replace('\r', ' ').replace('\n', ' ').replace('\t', ' ');
        if (clean.length() > maxLen) {
            return clean.left(maxLen) + "...";
        }
        return clean;
    }
};

class ClipboardHistoryModel : public QAbstractListModel {
    Q_OBJECT
    Q_PROPERTY(int count READ rowCount NOTIFY countChanged)

public:
    enum ClipRoles {
        IdRole = Qt::UserRole + 1,
        IsImageRole,
        TextRole,
        HeadTextRole,
        PreviewRole,
        ImageDataRole,
        MimeTypeRole,
        ImageSizeRole,
        ImageWRole,
        ImageHRole,
        SizeFormattedRole,
        SourceRole,
        TimestampRole,
        TimeFormattedRole,
        CharCountRole,
        ClipTypeRole,
        FileNameRole,
        FileSizeRole,
        LocalFilePathRole,
        BatchIdRole,
        BatchIndexRole,
        BatchTotalRole,
        FileStatusRole,
        BytesTransferredRole
    };

    explicit ClipboardHistoryModel(QObject* parent = nullptr);

    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    Q_INVOKABLE void addClip(const QString& text, const QString& source);
    Q_INVOKABLE void addClipImage(const QString& imageData, const QString& mimeType, int size, const QString& source);
    Q_INVOKABLE void addClipFile(const QString& fileName, qint64 fileSize, const QString& localFilePath, const QString& mimeType, const QString& source, const QString& batchId = "", int batchIndex = 1, int batchTotal = 1, FileStatus status = FileStatus::Completed);
    Q_INVOKABLE void updateFileStatus(const QString& id, FileStatus status, qint64 bytesTransferred = 0);
    Q_INVOKABLE void updateFileStatusByBatch(const QString& batchId, FileStatus status);
    Q_INVOKABLE void updateFileProgress(const QString& batchId, int index, const QString& fileName, qint64 bytesTransferred, qint64 totalBytes = 0);
    Q_INVOKABLE void updateFileCompleted(const QString& batchId, int index, const QString& fileName, const QString& localFilePath, qint64 fileSize = 0, const QString& mimeType = QString());
    Q_INVOKABLE void updateFileFailed(const QString& batchId, const QString& fileName);
    Q_INVOKABLE void cancelActiveTransfers();
    Q_INVOKABLE void removeClip(int index);
    Q_INVOKABLE void removeClipById(const QString& clipId);
    Q_INVOKABLE void clear();
    Q_INVOKABLE bool isClipImage(int index) const;
    Q_INVOKABLE QString getClipText(int index) const;
    Q_INVOKABLE QString getClipImageData(int index) const;
    Q_INVOKABLE QString getClipMimeType(int index) const;

    void setPersistenceFilePath(const QString& path);
    QString persistenceFilePath() const;
    void loadHistory();
    void saveHistory() const;

    const ClipItem* getItem(int index) const {
        if (index < 0 || index >= items_.size()) return nullptr;
        return &items_.at(index);
    }

signals:
    void countChanged();

private:
    QString fullTextAt(int index) const;
    void removeSpillFileLocked(const QString& clipId) const;

    QList<ClipItem> items_;
    QString customPersistencePath_;
};

}
