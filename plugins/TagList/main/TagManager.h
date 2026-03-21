#ifndef TAGMANAGER_H
#define TAGMANAGER_H

#include <QObject>
#include <QMap>
#include <QString>
#include <QMutex>
#include <QJsonObject>

class TagManager : public QObject {
    Q_OBJECT

public:
    static TagManager& instance();

    QMap<QString, QString> getTags() const;
    void scanTags();

    QJsonObject toJson() const;
    void setFromJson(const QJsonObject& obj);

signals:
    void tagsUpdated();

private:
    TagManager();

    void parseFile(const QString& filePath, QMap<QString, QString>& tags);
    QString removeComments(const QString& content);

    QMap<QString, QString> m_tags;
    mutable QMutex m_mutex;
};

#endif // TAGMANAGER_H