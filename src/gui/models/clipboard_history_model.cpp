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
    : QAbstractListModel(parent) {}

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

void ClipboardHistoryModel::removeClip(int index) {
    if (index < 0 || index >= items_.size()) return;
    removeSpillFileLocked(items_.at(index).id);
    beginRemoveRows(QModelIndex(), index, index);
    items_.removeAt(index);
    endRemoveRows();
    emit countChanged();
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
