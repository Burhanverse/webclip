#include "clipboard_history_model.hpp"
#include <QUuid>
#include <QBuffer>
#include <QImageReader>
#include <QStandardPaths>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QStringList>
#include <QUrl>
#include <QByteArray>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QtGlobal>

namespace webclip {

namespace {
int maxClips() {
    static const int cached = []() {
        int v = qEnvironmentVariableIntValue("WEBCLIP_MAX_CLIPS");
        return (v >= 10 && v <= 1000) ? v : 100;
    }();
    return cached;
}

int textSpillThresholdBytes() {
    static const int cached = []() {
        int v = qEnvironmentVariableIntValue("WEBCLIP_TEXT_SPILL_KB");
        return (v >= 4 && v <= 4096) ? v * 1024 : 16 * 1024;
    }();
    return cached;
}

constexpr int kSpillHeadChars = 4096;

QString spillDirPath() {
    return QStandardPaths::writableLocation(QStandardPaths::CacheLocation) + "/clips/text";
}

QString spillFilePath(const QString& clipId) {
    return spillDirPath() + "/" + clipId + ".txt";
}

void pruneTextSpillDir(int maxFiles) {
    QDir dir(spillDirPath());
    if (!dir.exists()) return;
    const QFileInfoList entries = dir.entryInfoList(QStringList() << "*.txt", QDir::Files | QDir::NoDotAndDotDot, QDir::Time);
    for (int i = maxFiles; i < entries.size(); ++i) {
        QFile::remove(entries.at(i).absoluteFilePath());
    }
}
}

ClipboardHistoryModel::ClipboardHistoryModel(QObject* parent)
    : QAbstractListModel(parent) {
    loadHistory();
}

int ClipboardHistoryModel::rowCount(const QModelIndex& parent) const {
    if (parent.isValid()) return 0;
    return items_.size();
}

QVariant ClipboardHistoryModel::data(const QModelIndex& index, int role) const {
    if (!index.isValid() || index.row() < 0 || index.row() >= items_.size()) {
        return QVariant();
    }

    const auto& item = items_.at(index.row());
    switch (role) {
        case IdRole: return item.id;
        case IsImageRole: return item.isImage;
        case TextRole: return fullTextAt(index.row());
        case HeadTextRole: return item.text;
        case PreviewRole: return item.preview();
        case ImageDataRole: return item.imageData;
        case MimeTypeRole: return item.mimeType;
        case ImageSizeRole: return item.imageSize;
        case ImageWRole: return item.imgWidth;
        case ImageHRole: return item.imgHeight;
        case SizeFormattedRole: return item.formattedSize();
        case SourceRole: return item.source;
        case TimestampRole: return item.timestamp;
        case TimeFormattedRole: return item.formattedTime();
        case CharCountRole: return QVariant::fromValue<qint64>(item.charCount());
        case ClipTypeRole: return static_cast<int>(item.type);
        case FileNameRole: return item.fileName;
        case FileSizeRole: return item.fileSize;
        case LocalFilePathRole: return item.localFilePath;
        case BatchIdRole: return item.batchId;
        case BatchIndexRole: return item.batchIndex;
        case BatchTotalRole: return item.batchTotal;
        case FileStatusRole: return static_cast<int>(item.fileStatus);
        case BytesTransferredRole: return item.bytesTransferred;
        default: return QVariant();
    }
}

QHash<int, QByteArray> ClipboardHistoryModel::roleNames() const {
    QHash<int, QByteArray> roles;
    roles[IdRole] = "id";
    roles[IsImageRole] = "isImage";
    roles[TextRole] = "text";
    roles[HeadTextRole] = "headText";
    roles[PreviewRole] = "preview";
    roles[ImageDataRole] = "imageData";
    roles[MimeTypeRole] = "mimeType";
    roles[ImageSizeRole] = "imageSize";
    roles[ImageWRole] = "imgWidth";
    roles[ImageHRole] = "imgHeight";
    roles[SizeFormattedRole] = "sizeFormatted";
    roles[SourceRole] = "source";
    roles[TimestampRole] = "timestamp";
    roles[TimeFormattedRole] = "timeFormatted";
    roles[CharCountRole] = "charCount";
    roles[ClipTypeRole] = "clipType";
    roles[FileNameRole] = "fileName";
    roles[FileSizeRole] = "fileSize";
    roles[LocalFilePathRole] = "localFilePath";
    roles[BatchIdRole] = "batchId";
    roles[BatchIndexRole] = "batchIndex";
    roles[BatchTotalRole] = "batchTotal";
    roles[FileStatusRole] = "fileStatus";
    roles[BytesTransferredRole] = "bytesTransferred";
    return roles;
}

void ClipboardHistoryModel::addClip(const QString& text, const QString& source) {
    if (text.trimmed().isEmpty()) return;

    if (!items_.isEmpty() && !items_.last().isImage) {
        const ClipItem& last = items_.last();
        const bool sameSize = last.charCount() == text.length();
        const bool sameHead = (last.text == text.left(last.text.length()));
        if (sameSize && sameHead && (!last.textSpilled || text.startsWith(last.text))) {
            return;
        }
    }

    if (items_.size() >= maxClips()) {
        removeSpillFileLocked(items_.first().id);
        beginRemoveRows(QModelIndex(), 0, 0);
        items_.removeFirst();
        endRemoveRows();
    }

    int newIndex = items_.size();
    beginInsertRows(QModelIndex(), newIndex, newIndex);
    ClipItem item;
    item.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    item.type = ClipType::Text;
    item.isImage = false;
    item.text = text;
    item.source = source;
    item.timestamp = QDateTime::currentMSecsSinceEpoch();

    QByteArray utf8 = text.toUtf8();
    if (utf8.size() > textSpillThresholdBytes()) {
        QString dir = spillDirPath();
        QDir().mkpath(dir);
        QFile file(spillFilePath(item.id));
        if (file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            file.write(utf8);
            file.close();
            item.textSpilled = true;
            item.fullChars = text.length();
            item.text = text.left(kSpillHeadChars);
            pruneTextSpillDir(maxClips());
        }
    }
    utf8 = QByteArray();

    items_.append(item);
    endInsertRows();
    emit countChanged();
}

void ClipboardHistoryModel::addClipImage(const QString& imageData, const QString& mimeType, int size, const QString& source) {
    if (imageData.isEmpty()) return;

    if (!items_.isEmpty() && items_.last().isImage && items_.last().imageData == imageData) {
        return;
    }

    if (items_.size() >= maxClips()) {
        beginRemoveRows(QModelIndex(), 0, 0);
        items_.removeFirst();
        endRemoveRows();
    }

    QString finalImageUrl = imageData;
    int imgSize = size > 0 ? size : imageData.length();
    QSize nativeSize;

    if (imageData.startsWith("data:")) {
        int commaIdx = imageData.indexOf(',');
        if (commaIdx >= 0) {
            QByteArray bytes = QByteArray::fromBase64(imageData.mid(commaIdx + 1).toLatin1());
            if (!bytes.isEmpty()) {
                imgSize = bytes.size();
                QBuffer buf(&bytes);
                buf.open(QIODevice::ReadOnly);
                QImageReader headerReader(&buf);
                nativeSize = headerReader.size();

                QString cacheDir = QStandardPaths::writableLocation(QStandardPaths::CacheLocation) + "/clips";
                QDir().mkpath(cacheDir);
                QString clipId = QUuid::createUuid().toString(QUuid::WithoutBraces);
                QString ext = mimeType.contains("jpeg") ? ".jpg" : (mimeType.contains("webp") ? ".webp" : ".png");
                QString filePath = cacheDir + "/" + clipId + ext;
                QFile file(filePath);
                if (file.open(QIODevice::WriteOnly)) {
                    file.write(bytes);
                    file.close();
                    finalImageUrl = QUrl::fromLocalFile(filePath).toString();
                }
            }
        }
    } else if (finalImageUrl.startsWith("file://") || finalImageUrl.startsWith("/")) {
        QString localPath = finalImageUrl.startsWith("file://") ? QUrl(finalImageUrl).toLocalFile() : finalImageUrl;
        QImageReader headerReader(localPath);
        nativeSize = headerReader.size();
    }

    int newIndex = items_.size();
    beginInsertRows(QModelIndex(), newIndex, newIndex);
    ClipItem item;
    item.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    item.type = ClipType::Image;
    item.isImage = true;
    item.imageData = finalImageUrl;
    item.mimeType = mimeType.isEmpty() ? "image/png" : mimeType;
    item.imageSize = imgSize;
    item.imgWidth = nativeSize.width();
    item.imgHeight = nativeSize.height();
    item.source = source;
    item.timestamp = QDateTime::currentMSecsSinceEpoch();
    items_.append(item);
    endInsertRows();
    emit countChanged();
}

void ClipboardHistoryModel::addClipFile(const QString& fileName, qint64 fileSize, const QString& localFilePath, const QString& mimeType, const QString& source, const QString& batchId, int batchIndex, int batchTotal, FileStatus status) {
    if (fileName.isEmpty()) return;

    if (items_.size() >= maxClips()) {
        removeSpillFileLocked(items_.first().id);
        beginRemoveRows(QModelIndex(), 0, 0);
        items_.removeFirst();
        endRemoveRows();
    }

    int newIndex = items_.size();
    beginInsertRows(QModelIndex(), newIndex, newIndex);
    ClipItem item;
    item.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    item.type = ClipType::File;
    item.isImage = false;
    item.fileName = fileName;
    item.fileSize = fileSize;
    item.localFilePath = localFilePath;
    item.mimeType = mimeType.isEmpty() ? QStringLiteral("application/octet-stream") : mimeType;
    item.source = source;
    item.timestamp = QDateTime::currentMSecsSinceEpoch();
    item.batchId = batchId;
    item.batchIndex = batchIndex;
    item.batchTotal = batchTotal;
    item.fileStatus = status;
    item.bytesTransferred = (status == FileStatus::Completed) ? fileSize : 0;
    items_.append(item);
    endInsertRows();
    emit countChanged();
    saveHistory();
}

void ClipboardHistoryModel::updateFileStatus(const QString& id, FileStatus status, qint64 bytesTransferred) {
    if (id.isEmpty()) return;
    for (int i = 0; i < items_.size(); ++i) {
        if (items_[i].id == id) {
            items_[i].fileStatus = status;
            if (bytesTransferred > 0) {
                items_[i].bytesTransferred = bytesTransferred;
            }
            emit dataChanged(index(i), index(i));
            if (status == FileStatus::Completed) {
                saveHistory();
            }
            return;
        }
    }
}

void ClipboardHistoryModel::updateFileStatusByBatch(const QString& batchId, FileStatus status) {
    if (batchId.isEmpty()) return;
    for (int i = 0; i < items_.size(); ++i) {
        if (items_[i].batchId == batchId) {
            items_[i].fileStatus = status;
            emit dataChanged(index(i), index(i));
        }
    }
    if (status == FileStatus::Completed) {
        saveHistory();
    }
}

void ClipboardHistoryModel::updateFileProgress(const QString& batchId, int index, const QString& fileName, qint64 bytesTransferred, qint64 totalBytes) {
    for (int i = 0; i < items_.size(); ++i) {
        auto& item = items_[i];
        if (item.type != ClipType::File) continue;
        bool match = false;
        if (!batchId.isEmpty() && item.batchId == batchId) {
            if (index > 0 && item.batchIndex == index) {
                match = true;
            } else if (!fileName.isEmpty() && item.fileName == fileName) {
                match = true;
            } else if (item.batchTotal == 1) {
                match = true;
            }
        } else if (batchId.isEmpty() && !fileName.isEmpty() && item.fileName == fileName) {
            match = true;
        }
        if (match) {
            item.fileStatus = FileStatus::Transferring;
            item.bytesTransferred = bytesTransferred;
            if (totalBytes > 0 && item.fileSize <= 0) {
                item.fileSize = totalBytes;
            }
            emit dataChanged(this->index(i), this->index(i));
            return;
        }
    }
    addClipFile(fileName, totalBytes > 0 ? totalBytes : bytesTransferred, QString(), QString(), QStringLiteral("phone"), batchId, index > 0 ? index : 1, 1, FileStatus::Transferring);
    if (!items_.isEmpty()) {
        items_.last().bytesTransferred = bytesTransferred;
        emit dataChanged(this->index(items_.size() - 1), this->index(items_.size() - 1));
    }
}

void ClipboardHistoryModel::updateFileCompleted(const QString& batchId, int index, const QString& fileName, const QString& localFilePath, qint64 fileSize, const QString& mimeType) {
    for (int i = 0; i < items_.size(); ++i) {
        auto& item = items_[i];
        if (item.type != ClipType::File) continue;
        bool match = false;
        if (!batchId.isEmpty() && item.batchId == batchId) {
            if (index > 0 && item.batchIndex == index) {
                match = true;
            } else if (!fileName.isEmpty() && item.fileName == fileName) {
                match = true;
            } else if (item.batchTotal == 1) {
                match = true;
            }
        } else if (batchId.isEmpty() && !fileName.isEmpty() && item.fileName == fileName) {
            match = true;
        }
        if (match) {
            item.fileStatus = FileStatus::Completed;
            item.localFilePath = localFilePath;
            if (fileSize > 0) {
                item.fileSize = fileSize;
            }
            item.bytesTransferred = item.fileSize;
            if (!mimeType.isEmpty()) {
                item.mimeType = mimeType;
            }
            emit dataChanged(this->index(i), this->index(i));
            saveHistory();
            return;
        }
    }
    addClipFile(fileName, fileSize, localFilePath, mimeType, QStringLiteral("phone"), batchId, index > 0 ? index : 1, (index > 0 ? index : 1), FileStatus::Completed);
}

void ClipboardHistoryModel::updateFileFailed(const QString& batchId, const QString& fileName) {
    bool changed = false;
    for (int i = 0; i < items_.size(); ++i) {
        auto& item = items_[i];
        if (item.type != ClipType::File) continue;
        if (item.fileStatus == FileStatus::Completed || item.fileStatus == FileStatus::Cancelled) continue;
        bool match = false;
        if (!batchId.isEmpty() && item.batchId == batchId) {
            if (fileName.isEmpty() || item.fileName == fileName) {
                match = true;
            }
        } else if (batchId.isEmpty() && !fileName.isEmpty() && item.fileName == fileName) {
            match = true;
        } else if (batchId.isEmpty() && fileName.isEmpty()) {
            match = true;
        }
        if (match) {
            item.fileStatus = FileStatus::Failed;
            emit dataChanged(this->index(i), this->index(i));
            changed = true;
        }
    }
    if (changed) {
        saveHistory();
    }
}

void ClipboardHistoryModel::cancelActiveTransfers() {
    bool changed = false;
    for (int i = 0; i < items_.size(); ++i) {
        auto& item = items_[i];
        if (item.type != ClipType::File) continue;
        if (item.fileStatus == FileStatus::Pending || item.fileStatus == FileStatus::Transferring) {
            item.fileStatus = FileStatus::Cancelled;
            emit dataChanged(this->index(i), this->index(i));
            changed = true;
        }
    }
    if (changed) {
        saveHistory();
    }
}

void ClipboardHistoryModel::removeClip(int index) {
    if (index < 0 || index >= items_.size()) return;
    bool wasFile = items_.at(index).type == ClipType::File;
    removeSpillFileLocked(items_.at(index).id);
    beginRemoveRows(QModelIndex(), index, index);
    items_.removeAt(index);
    endRemoveRows();
    emit countChanged();
    if (wasFile) {
        saveHistory();
    }
}

void ClipboardHistoryModel::removeClipById(const QString& clipId) {
    if (clipId.isEmpty()) return;
    for (int i = 0; i < items_.size(); ++i) {
        if (items_.at(i).id == clipId) {
            removeClip(i);
            return;
        }
    }
}

void ClipboardHistoryModel::clear() {
    if (items_.isEmpty()) return;
    for (const ClipItem& item : items_) {
        removeSpillFileLocked(item.id);
    }
    beginResetModel();
    items_.clear();
    endResetModel();
    emit countChanged();
    saveHistory();
}

void ClipboardHistoryModel::setPersistenceFilePath(const QString& path) {
    customPersistencePath_ = path;
}

QString ClipboardHistoryModel::persistenceFilePath() const {
    if (!customPersistencePath_.isEmpty()) {
        return customPersistencePath_;
    }
    QString dir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    return dir + QStringLiteral("/file_history.json");
}

void ClipboardHistoryModel::loadHistory() {
    QString path = persistenceFilePath();
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        return;
    }
    QByteArray data = file.readAll();
    file.close();

    QJsonDocument doc = QJsonDocument::fromJson(data);
    if (!doc.isArray()) {
        return;
    }
    QJsonArray arr = doc.array();
    QList<ClipItem> loadedItems;
    for (const QJsonValue& val : arr) {
        if (!val.isObject()) continue;
        QJsonObject obj = val.toObject();
        QString localPath = obj.value(QStringLiteral("localFilePath")).toString();
        if (localPath.isEmpty() || !QFileInfo::exists(localPath)) {
            continue;
        }
        ClipItem item;
        item.id = obj.value(QStringLiteral("id")).toString();
        if (item.id.isEmpty()) {
            item.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
        }
        item.type = ClipType::File;
        item.isImage = false;
        item.fileName = obj.value(QStringLiteral("fileName")).toString();
        item.fileSize = obj.value(QStringLiteral("fileSize")).toVariant().toLongLong();
        item.localFilePath = localPath;
        item.mimeType = obj.value(QStringLiteral("mimeType")).toString();
        item.source = obj.value(QStringLiteral("source")).toString();
        item.timestamp = obj.value(QStringLiteral("timestamp")).toVariant().toLongLong();
        item.batchId = obj.value(QStringLiteral("batchId")).toString();
        item.batchIndex = obj.value(QStringLiteral("batchIndex")).toInt(1);
        item.batchTotal = obj.value(QStringLiteral("batchTotal")).toInt(1);
        item.fileStatus = FileStatus::Completed;
        item.bytesTransferred = item.fileSize;
        loadedItems.append(item);
        if (loadedItems.size() >= 500) {
            break;
        }
    }

    if (!loadedItems.isEmpty()) {
        beginInsertRows(QModelIndex(), items_.size(), items_.size() + loadedItems.size() - 1);
        items_.append(loadedItems);
        endInsertRows();
        emit countChanged();
    }
}

void ClipboardHistoryModel::saveHistory() const {
    QString path = persistenceFilePath();
    QFileInfo fi(path);
    QDir().mkpath(fi.absolutePath());

    QJsonArray arr;
    int count = 0;
    for (const ClipItem& item : items_) {
        if (item.type != ClipType::File) continue;
        if (item.fileStatus != FileStatus::Completed) continue;
        if (item.localFilePath.isEmpty() || !QFileInfo::exists(item.localFilePath)) continue;

        QJsonObject obj;
        obj.insert(QStringLiteral("id"), item.id);
        obj.insert(QStringLiteral("fileName"), item.fileName);
        obj.insert(QStringLiteral("fileSize"), item.fileSize);
        obj.insert(QStringLiteral("localFilePath"), item.localFilePath);
        obj.insert(QStringLiteral("mimeType"), item.mimeType);
        obj.insert(QStringLiteral("source"), item.source);
        obj.insert(QStringLiteral("timestamp"), item.timestamp);
        obj.insert(QStringLiteral("batchId"), item.batchId);
        obj.insert(QStringLiteral("batchIndex"), item.batchIndex);
        obj.insert(QStringLiteral("batchTotal"), item.batchTotal);
        arr.append(obj);
        count++;
        if (count >= 500) {
            break;
        }
    }

    QJsonDocument doc(arr);
    QFile file(path);
    if (file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        file.write(doc.toJson(QJsonDocument::Compact));
        file.close();
    }
}

bool ClipboardHistoryModel::isClipImage(int index) const {
    if (index < 0 || index >= items_.size()) return false;
    return items_.at(index).isImage;
}

QString ClipboardHistoryModel::getClipText(int index) const {
    if (index < 0 || index >= items_.size()) return QString();
    return fullTextAt(index);
}

QString ClipboardHistoryModel::getClipImageData(int index) const {
    if (index < 0 || index >= items_.size()) return QString();
    return items_.at(index).imageData;
}

QString ClipboardHistoryModel::getClipMimeType(int index) const {
    if (index < 0 || index >= items_.size()) return QString();
    return items_.at(index).mimeType;
}

QString ClipboardHistoryModel::fullTextAt(int index) const {
    const ClipItem& item = items_.at(index);
    if (!item.textSpilled) return item.text;
    QFile file(spillFilePath(item.id));
    if (file.open(QIODevice::ReadOnly)) {
        QByteArray raw = file.readAll();
        file.close();
        return QString::fromUtf8(raw);
    }
    return item.text + QStringLiteral("\n…");
}

void ClipboardHistoryModel::removeSpillFileLocked(const QString& clipId) const {
    if (!clipId.isEmpty()) {
        QFile::remove(spillFilePath(clipId));
    }
}

}
