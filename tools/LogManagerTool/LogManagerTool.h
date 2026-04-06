#ifndef LOGMANAGERTOOL_H
#define LOGMANAGERTOOL_H

#include <QObject>
#include <QWidget>
#include <QTreeWidget>
#include <QTableWidget>
#include <QPushButton>
#include <QLineEdit>
#include <QLabel>
#include <QMap>
#include <QList>
#include <QJsonObject>
#include <QIcon>
#include <QColor>
#include <QPoint>
#include <QRegularExpression>
#include "../../src/ToolInterface.h"
#include "../../src/ConfigManager.h"
#include "../../src/Logger.h"

class QStackedWidget;
class QButtonGroup;
class QTreeWidgetItem;
class QTableWidgetItem;
class ToolLoadingOverlay;

class LogManagerTool;
class LogManagerMainWidget;
class LogFileSidebarWidget;

enum class LogMode {
    ErrorLog = 0
};

enum class LogSortMode {
    ByTime = 0,
    ByCategory = 1
};

struct LogEntry {
    QString systemTime;
    QString gameTime;
    QString category;
    QString message;
    QString normalizedKey;
    bool isHighPriority = false;
    int originalIndex = -1;
};

struct LogFileRecord {
    QString displayName;
    QString sourcePath;
    LogMode mode = LogMode::ErrorLog;
    bool isLatest = false;
    bool isLoaded = false;
    QList<LogEntry> entries;
};

struct CompareRow {
    QString normalizedKey;
    QString category;
    bool isHighPriority = false;
    bool hasLeft = false;
    bool hasRight = false;
    LogEntry leftEntry;
    LogEntry rightEntry;
};

class LogManagerMainWidget : public QWidget {
public:
    explicit LogManagerMainWidget(LogManagerTool* tool, QWidget* parent = nullptr);

    void updateTexts();
    void applyTheme();
    void setAvailableModes(const QList<LogMode>& modes);
    void setCurrentMode(LogMode mode);
    void setCurrentFile(const LogFileRecord& fileRecord);
    void setCompareFile(const LogFileRecord& compareRecord);
    void clearCompareMode();
    void setSortMode(LogSortMode sortMode);
    void setSearchText(const QString& text);
    void showLoadingOverlay(const QString& message);
    void hideLoadingOverlay();

private:
    void handleModeButtonClicked(int id);
    void handleSortButtonClicked(int id);
    void handleSearchTextChanged(const QString& text);
    void handleMainListContextMenuRequested(const QPoint& pos);
    void handleCompareTableContextMenuRequested(const QPoint& pos);

    void rebuildModeButtons();
    void refreshView();
    QList<LogEntry> buildFilteredEntries() const;
    QList<LogEntry> buildSortedEntries(const QList<LogEntry>& entries) const;
    QList<CompareRow> buildCompareRows() const;
    void populateMainTable(const QList<LogEntry>& entries);
    void populateCompareTable(const QList<CompareRow>& rows);
    QString buildPreviewText(const QString& text) const;
    bool matchesSearch(const LogEntry& entry) const;
    bool compareRowsMatchSearch(const CompareRow& row) const;
    void updateButtonStyles();
    QColor priorityTextColor() const;
    QColor normalTextColor() const;
    void updateHeadersOnly();
    QString formatEntryForClipboard(const LogEntry& entry) const;

    LogManagerTool* m_tool = nullptr;

    QWidget* m_topBar = nullptr;
    QWidget* m_modeContainer = nullptr;
    QWidget* m_sortContainer = nullptr;
    QLineEdit* m_searchEdit = nullptr;
    QStackedWidget* m_contentStack = nullptr;
    QTreeWidget* m_mainTable = nullptr;
    QTableWidget* m_compareTable = nullptr;

    QButtonGroup* m_modeGroup = nullptr;
    QButtonGroup* m_sortGroup = nullptr;
    QList<QPushButton*> m_modeButtons;
    QPushButton* m_sortByTimeButton = nullptr;
    QPushButton* m_sortByCategoryButton = nullptr;
    QLabel* m_compareHeaderLabel = nullptr;
    ToolLoadingOverlay* m_loadingOverlay = nullptr;

    QList<LogMode> m_availableModes;
    LogMode m_currentMode = LogMode::ErrorLog;
    LogSortMode m_sortMode = LogSortMode::ByTime;
    QString m_searchText;
    LogFileRecord m_currentFile;
    bool m_hasCurrentFile = false;
    LogFileRecord m_compareFile;
    bool m_isCompareMode = false;
};

class LogFileSidebarWidget : public QWidget {
public:
    explicit LogFileSidebarWidget(LogManagerTool* tool, QWidget* parent = nullptr);

    void setState(const QList<LogFileRecord>& files, const QString& currentFileName, const QString& compareFileName);
    void updateTexts();
    void applyTheme();

private:
    void handleItemClicked(QTreeWidgetItem* item, int column);
    void handleContextMenuRequested(const QPoint& pos);
    void refreshItems();

    LogManagerTool* m_tool = nullptr;
    QLabel* m_headerLabel = nullptr;
    QTreeWidget* m_fileTable = nullptr;
    QList<LogFileRecord> m_files;
    QString m_currentFileName;
    QString m_compareFileName;
};

class LogManagerTool : public QObject, public ToolInterface {
    Q_OBJECT
    Q_PLUGIN_METADATA(IID "com.ape.hoi4toolstudio.ToolInterface")
    Q_INTERFACES(ToolInterface)

signals:
    void loadingStarted();
    void loadingFinished();

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
    QWidget* createSidebarWidget(QWidget* parent = nullptr) override;
    void loadLanguage(const QString& lang) override;
    void applyTheme() override;

    QString getString(const QString& key) const;
    QString getGameDocsPath() const;
    QList<LogMode> availableModes() const;
    QList<LogFileRecord> filesForMode(LogMode mode) const;
    bool hasCurrentFile() const;
    QString displayNameForUi(const LogFileRecord& fileRecord) const;

    QList<LogFileRecord> discoverLogFiles(LogMode mode) const;
    QList<LogEntry> parseLogContent(const QString& content) const;
    QString normalizeLogEntryKey(const QString& category, const QString& message) const;
    LogFileRecord findFileByDisplayName(LogMode mode, const QString& displayName, bool* ok = nullptr) const;

public slots:
    void handleModeChanged(int mode);
    void handleFileActivated(const QString& displayName);
    void handleCompareRequested(const QString& displayName);
    void handleCompareCleared();

private:
    QString readTextFile(const QString& path) const;
    void beginLoading(const QString& message);
    void endLoading();
    void refreshDiscoveredFiles();
    void syncSidebarState();
    bool ensureFileLoaded(LogMode mode, const QString& displayName, LogFileRecord* outRecord = nullptr);
    int indexOfFile(LogMode mode, const QString& displayName) const;
    void openInitialFileIfAvailable();
    void pushCurrentFileToView();
    void pushCompareFileToView();
    void startInitialLoadAsync();
    void startCurrentFileLoadAsync(const QString& displayName);
    void startCompareFileLoadAsync(const QString& displayName);
    void setLoadingState(bool loading, const QString& message = QString());
    void releaseAllLoadedLogs();
    QString normalizeLanguageCode(const QString& lang) const;
    QString localisationRootPath() const;
    QMap<QString, QString> parseSimpleYamlFile(const QString& filePath) const;

    QMap<QString, QString> m_localizedNames;
    QMap<QString, QString> m_localizedDescs;
    QMap<QString, QString> m_localizedStrings;
    QString m_currentLang;

    QString m_id;
    QString m_version;
    QString m_compatibleVersion;
    QString m_author;

    LogManagerMainWidget* m_mainWidget = nullptr;
    LogFileSidebarWidget* m_sidebarWidget = nullptr;
    QMap<int, QList<LogFileRecord>> m_discoveredFilesByMode;
    LogMode m_currentMode = LogMode::ErrorLog;
    QString m_currentFileName;
    QString m_compareFileName;
    quint64 m_loadRequestSerial = 0;
    bool m_isLoading = false;
};

#endif // LOGMANAGERTOOL_H