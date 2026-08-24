#include "clipboard_history_model.hpp"
#include <QUuid>

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
        case TextRole: return item.text;
        case PreviewRole: return item.preview();
        case SourceRole: return item.source;
        case TimestampRole: return item.timestamp;
        case TimeFormattedRole: return item.formattedTime();
        case CharCountRole: return item.text.length();
        default: return QVariant();
    }
}

QHash<int, QByteArray> ClipboardHistoryModel::roleNames() const {
    QHash<int, QByteArray> roles;
    roles[IdRole] = "id";
    roles[TextRole] = "text";
    roles[PreviewRole] = "preview";
    roles[SourceRole] = "source";
    roles[TimestampRole] = "timestamp";
    roles[TimeFormattedRole] = "timeFormatted";
    roles[CharCountRole] = "charCount";
    return roles;
}

void ClipboardHistoryModel::addClip(const QString& text, const QString& source) {
    if (text.trimmed().isEmpty()) return;

    // Check if the most recent clip is identical to avoid duplicates
    if (!items_.isEmpty() && items_.first().text == text) {
        return;
    }

    beginInsertRows(QModelIndex(), 0, 0);
    ClipItem item;
    item.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    item.text = text;
    item.source = source;
    item.timestamp = QDateTime::currentMSecsSinceEpoch();
    items_.prepend(item);

    // Limit maximum history in memory to 100 items
    if (items_.size() > 100) {
        items_.removeLast();
    }
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

void ClipboardHistoryModel::clear() {
    if (items_.isEmpty()) return;
    beginResetModel();
    items_.clear();
    endResetModel();
    emit countChanged();
}

QString ClipboardHistoryModel::getClipText(int index) const {
    if (index < 0 || index >= items_.size()) return QString();
    return items_.at(index).text;
}

} // namespace webclip
