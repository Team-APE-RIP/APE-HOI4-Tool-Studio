#ifndef LOCALIZATIONMANAGER_H
#define LOCALIZATIONMANAGER_H

#include <QString>
#include <QJsonObject>
#include <QMap>

class LocalizationManager {
public:
    static LocalizationManager& instance();

    void loadLanguage(const QString& langCode);
    QString getString(const QString& category, const QString& key) const;

private:
    LocalizationManager();
    void loadFile(const QString& category, const QString& path);

    // Map<Category, Map<Key, Value>>
    QMap<QString, QMap<QString, QString>> m_translations;
    QString m_currentLang;
};

#endif // LOCALIZATIONMANAGER_H
