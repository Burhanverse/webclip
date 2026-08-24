#include "i18n.hpp"
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QDir>
#include <QDebug>

namespace webclip {

I18n* I18n::instance() {
    static I18n s_instance;
    return &s_instance;
}

I18n::I18n(QObject* parent)
    : QObject(parent) {
    loadLanguage("en");
}

void I18n::setLanguage(const QString& langCode) {
    if (currentLanguage_ != langCode) {
        currentLanguage_ = langCode;
        loadLanguage(langCode);
        emit languageChanged();
    }
}

QStringList I18n::availableLanguages() const {
    QStringList langs;
    langs << "en";

    // Scan QRC resources for extra language files
    QDir qrcDir(":/qt/qml/src/gui/resources/langs");
    if (qrcDir.exists()) {
        const auto files = qrcDir.entryList(QStringList() << "*.json", QDir::Files);
        for (const auto& file : files) {
            QString code = file.section('.', 0, 0);
            if (!langs.contains(code)) {
                langs.append(code);
            }
        }
    }
    return langs;
}

void I18n::loadLanguage(const QString& langCode) {
    translations_.clear();

    QString path = QStringLiteral(":/qt/qml/src/gui/resources/langs/%1.json").arg(langCode);
    QFile file(path);
    if (!file.exists()) {
        // Fallback to English if requested language not found
        path = QStringLiteral(":/qt/qml/src/gui/resources/langs/en.json");
        file.setFileName(path);
    }

    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qWarning() << "I18n: Could not open language file:" << path;
        return;
    }

    QByteArray data = file.readAll();
    file.close();

    QJsonParseError err;
    QJsonDocument doc = QJsonDocument::fromJson(data, &err);
    if (err.error != QJsonParseError::NoError || !doc.isObject()) {
        qWarning() << "I18n: JSON parse error in language file:" << err.errorString();
        return;
    }

    flattenJson(QString(), doc.object());
}

void I18n::flattenJson(const QString& prefix, const QJsonObject& obj) {
    for (auto it = obj.begin(); it != obj.end(); ++it) {
        QString key = prefix.isEmpty() ? it.key() : (prefix + "." + it.key());
        if (it.value().isObject()) {
            flattenJson(key, it.value().toObject());
        } else if (it.value().isString()) {
            translations_.insert(key, it.value().toString());
        }
    }
}

QString I18n::t(const QString& key, const QString& defaultVal) const {
    if (translations_.contains(key)) {
        return translations_.value(key);
    }
    return defaultVal.isEmpty() ? key : defaultVal;
}

} // namespace webclip
