#ifndef TAGMANAGER_H
#define TAGMANAGER_H

#include <QObject>
#include <QMap>
#include <QString>
#include <QMutex>

class TagManager : public QObject {
    Q_OBJECT

public:
    static TagManager& instance();

    // Returns a map of TAG -> Path (e.g., "ADR" -> "countries/ADR.txt")
    QMap<QString, QString> getTags() const;
    
    // Manually trigger a scan (usually called automatically after FileManager scan)
    void scanTags();

signals:
    void tagsUpdated();

private slots:
    void onFileScanFinished();

private:
    TagManager();
    
    // Helper to parse a single file
    void parseFile(const QString& filePath, QMap<QString, QString>& tags);
    
    // Helper to remove comments
    QString removeComments(const QString& content);

    QMap<QString, QString> m_tags;
    mutable QMutex m_mutex;
};

#endif // TAGMANAGER_H
