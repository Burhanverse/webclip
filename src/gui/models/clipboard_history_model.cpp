#include "clipboard_history_model.hpp"
#include <QUuid>
#include <QStandardPaths>
#include <QDir>
#include <QFile>
#include <QUrl>
#include <QByteArray>

namespace webclip {

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
        case TextRole: return item.text;
        case PreviewRole: return item.preview();
        case ImageDataRole: return item.imageData;
        case MimeTypeRole: return item.mimeType;
        case ImageSizeRole: return item.imageSize;
        case SizeFormattedRole: return item.formattedSize();
        case SourceRole: return item.source;
        case TimestampRole: return item.timestamp;
        case TimeFormattedRole: return item.formattedTime();
        case CharCountRole: return item.isImage ? item.imageSize : item.text.length();
        default: return QVariant();
    }
}

QHash<int, QByteArray> ClipboardHistoryModel::roleNames() const {
    QHash<int, QByteArray> roles;
    roles[IdRole] = "id";
    roles[IsImageRole] = "isImage";
    roles[TextRole] = "text";
    roles[PreviewRole] = "preview";
    roles[ImageDataRole] = "imageData";
    roles[MimeTypeRole] = "mimeType";
    roles[ImageSizeRole] = "imageSize";
    roles[SizeFormattedRole] = "sizeFormatted";
    roles[SourceRole] = "source";
    roles[TimestampRole] = "timestamp";
    roles[TimeFormattedRole] = "timeFormatted";
    roles[CharCountRole] = "charCount";
    return roles;
}

void ClipboardHistoryModel::addClip(const QString& text, const QString& source) {
    if (text.trimmed().isEmpty()) return;

    if (!items_.isEmpty() && !items_.last().isImage && items_.last().text == text) {
        return;
    }

    if (items_.size() >= 100) {
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
    items_.append(item);
    endInsertRows();
    emit countChanged();
}

void ClipboardHistoryModel::addClipImage(const QString& imageData, const QString& mimeType, int size, const QString& source) {
    if (imageData.isEmpty()) return;

    if (!items_.isEmpty() && items_.last().isImage && items_.last().imageData == imageData) {
        return;
    }

    if (items_.size() >= 100) {
        beginRemoveRows(QModelIndex(), 0, 0);
        items_.removeFirst();
        endRemoveRows();
    }

    QString finalImageUrl = imageData;
    int imgSize = size > 0 ? size : imageData.length();

    if (imageData.startsWith("data:")) {
        int commaIdx = imageData.indexOf(',');
        if (commaIdx >= 0) {
            QByteArray bytes = QByteArray::fromBase64(imageData.mid(commaIdx + 1).toLatin1());
            if (!bytes.isEmpty()) {
                imgSize = bytes.size();
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
    }

    int newIndex = items_.size();
    beginInsertRows(QModelIndex(), newIndex, newIndex);
    ClipItem item;
    item.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    item.isImage = true;
    item.imageData = finalImageUrl;
    item.mimeType = mimeType.isEmpty() ? "image/png" : mimeType;
    item.imageSize = imgSize;
    item.source = source;
    item.timestamp = QDateTime::currentMSecsSinceEpoch();
    items_.append(item);
    endInsertRows();
    emit countChanged();
}

void ClipboardHistoryModel::removeClip(int index) {
    if (index < 0 || index >= items_.size()) return;
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
    return items_.at(index).text;
}

QString ClipboardHistoryModel::getClipImageData(int index) const {
    if (index < 0 || index >= items_.size()) return QString();
    return items_.at(index).imageData;
}

QString ClipboardHistoryModel::getClipMimeType(int index) const {
    if (index < 0 || index >= items_.size()) return QString();
    return items_.at(index).mimeType;
}

}
