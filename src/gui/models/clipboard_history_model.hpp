#pragma once

#include <QAbstractListModel>
#include <QDateTime>
#include <QList>
#include <QtQml/qqmlregistration.h>

namespace webclip {

struct ClipItem {
    QString id;
    bool isImage = false;
    QString text;
    QString imageData; // Data URL ("data:image/png;base64,...") or file path
    QString mimeType;
    int imageSize = 0;
    QString source; // "phone", "local", "manual"
    qint64 timestamp = 0;

    QString formattedTime() const {
        QDateTime dt = QDateTime::fromMSecsSinceEpoch(timestamp);
        return dt.toString("hh:mm:ss AP");
    }

    QString formattedSize() const {
        if (!isImage) return QString::number(text.length()) + " chars";
        if (imageSize < 1024) return QString::number(imageSize) + " B";
        if (imageSize < 1024 * 1024) return QString::number(imageSize / 1024.0, 'f', 1) + " KB";
        return QString::number(imageSize / (1024.0 * 1024.0), 'f', 1) + " MB";
    }

    QString preview(int maxLen = 80) const {
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
    QML_ELEMENT
    Q_PROPERTY(int count READ rowCount NOTIFY countChanged)

public:
    enum ClipRoles {
        IdRole = Qt::UserRole + 1,
        IsImageRole,
        TextRole,
        PreviewRole,
        ImageDataRole,
        MimeTypeRole,
        ImageSizeRole,
        SizeFormattedRole,
        SourceRole,
        TimestampRole,
        TimeFormattedRole,
        CharCountRole
    };

    explicit ClipboardHistoryModel(QObject* parent = nullptr);

    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    Q_INVOKABLE void addClip(const QString& text, const QString& source);
    Q_INVOKABLE void addClipImage(const QString& imageData, const QString& mimeType, int size, const QString& source);
    Q_INVOKABLE void removeClip(int index);
    Q_INVOKABLE void clear();
    Q_INVOKABLE bool isClipImage(int index) const;
    Q_INVOKABLE QString getClipText(int index) const;
    Q_INVOKABLE QString getClipImageData(int index) const;
    Q_INVOKABLE QString getClipMimeType(int index) const;

    const ClipItem* getItem(int index) const {
        if (index < 0 || index >= items_.size()) return nullptr;
        return &items_.at(index);
    }

signals:
    void countChanged();

private:
    QList<ClipItem> items_;
};

} // namespace webclip
