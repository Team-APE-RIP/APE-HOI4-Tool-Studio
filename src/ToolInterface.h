#ifndef TOOLINTERFACE_H
#define TOOLINTERFACE_H

#include <QtPlugin>
#include <QWidget>
#include <QString>
#include <QIcon>
#include <QJsonObject>

class ToolInterface {
public:
    virtual ~ToolInterface() = default;

    // Basic Info
    virtual QString id() const = 0;
    virtual QString name() const = 0;
    virtual QString description() const = 0;
    virtual QString version() const = 0;
    virtual QString compatibleVersion() const = 0; // New: Required main program version
    virtual QString author() const = 0;
    
    // Metadata Injection
    virtual void setMetaData(const QJsonObject& metaData) = 0;

    // Resources
    virtual QIcon icon() const = 0;
    
    // Initialization
    // You might want to pass a context object here in the future
    virtual void initialize() = 0;
    
    // UI
    virtual QWidget* createWidget(QWidget* parent = nullptr) = 0;
    virtual QWidget* createSidebarWidget(QWidget* parent = nullptr) { return nullptr; }
    
    // Localization
    virtual void loadLanguage(const QString& lang) = 0;
    
    // Theme
    virtual void applyTheme() {}
};

#define ToolInterface_iid "com.ape.hoi4toolstudio.ToolInterface"
Q_DECLARE_INTERFACE(ToolInterface, ToolInterface_iid)

#endif // TOOLINTERFACE_H
