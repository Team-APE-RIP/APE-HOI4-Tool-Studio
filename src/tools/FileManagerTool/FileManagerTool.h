#ifndef FILEMANAGERTOOL_H
#define FILEMANAGERTOOL_H

#include <QObject>
#include "../../ToolInterface.h"

class FileManagerTool : public QObject, public ToolInterface {
    Q_OBJECT
    Q_PLUGIN_METADATA(IID "com.ape.hoi4toolstudio.ToolInterface" FILE "metadata.json")
    Q_INTERFACES(ToolInterface)

public:
    QString id() const override { return m_id; }
    QString name() const override;
    QString description() const override;
    QString version() const override { return m_version; }
    QString compatibleVersion() const override { return m_compatibleVersion; }
    QString author() const override { return m_author; }
    
    void setMetaData(const QJsonObject& metaData) override;

    QIcon icon() const override;
    
    void initialize() override;
    QWidget* createWidget(QWidget* parent = nullptr) override;
    void loadLanguage(const QString& lang) override;

private:
    QMap<QString, QString> m_localizedNames;
    QMap<QString, QString> m_localizedDescs;
    QString m_currentLang;
    QString m_toolPath; // Store path to find resources

    // Metadata
    QString m_id;
    QString m_version;
    QString m_compatibleVersion;
    QString m_author;
};

#endif // FILEMANAGERTOOL_H
