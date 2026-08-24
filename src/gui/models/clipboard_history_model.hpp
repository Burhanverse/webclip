#pragma once

#include <QAbstractListModel>
#include <QDateTime>
#include <QList>
#include <QtQml/qqmlregistration.h>

namespace webclip {

struct ClipItem {
    QString id;
    QString text;
    QString source; // "phone", "local", "manual"
    qint64 timestamp = 0;

    QString formattedTime() const {
        QDateTime dt = QDateTime::fromMSecsSinceEpoch(timestamp);
        return dt.toString("hh:mm:ss AP");
    }

    QString preview(int maxLen = 80) const {
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
        TextRole,
        PreviewRole,
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
    Q_INVOKABLE void removeClip(int index);
    Q_INVOKABLE void clear();
    Q_INVOKABLE QString getClipText(int index) const;

signals:
    void countChanged();

private:
    QList<ClipItem> items_;
};

} // namespace webclip
