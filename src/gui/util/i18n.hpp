#pragma once

#include <QObject>
#include <QString>
#include <QHash>
#include <QStringList>
#include <QtQml/qqmlregistration.h>
#include <QQmlEngine>
#include <QJSEngine>

namespace webclip {

class I18n : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

    Q_PROPERTY(QString currentLanguage READ currentLanguage WRITE setLanguage NOTIFY languageChanged)
    Q_PROPERTY(QStringList availableLanguages READ availableLanguages CONSTANT)

public:
    explicit I18n(QObject* parent = nullptr);

    static I18n* create(QQmlEngine*, QJSEngine*) {
        return instance();
    }

    static I18n* instance();

    QString currentLanguage() const { return currentLanguage_; }
    void setLanguage(const QString& langCode);

    QStringList availableLanguages() const;

    Q_INVOKABLE QString t(const QString& key, const QString& defaultVal = QString()) const;
    Q_INVOKABLE QString tr(const QString& key) const { return t(key); }

signals:
    void languageChanged();

private:
    void loadLanguage(const QString& langCode);
    void flattenJson(const QString& prefix, const class QJsonObject& obj);

    QString currentLanguage_ = "en";
    QHash<QString, QString> translations_;
};

} // namespace webclip
