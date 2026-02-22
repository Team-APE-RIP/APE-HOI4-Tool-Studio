#ifndef FLAGMANAGERTOOL_H
#define FLAGMANAGERTOOL_H

#include <QObject>
#include <QWidget>
#include <QStackedWidget>
#include <QPushButton>
#include <QListWidget>
#include <QTreeWidget>
#include <QGraphicsView>
#include <QGraphicsScene>
#include <QLineEdit>
#include <QSlider>
#include <QLabel>
#include <QMap>
#include <QImage>
#include <QScrollArea>
#include <QGridLayout>
#include <QButtonGroup>
#include "../../ToolInterface.h"

class FlagManagerTool;
class FlagManagerMainWidget;
class FlagListWidget;

// --- Data Structures ---

struct FlagVariant {
    QString name; // base name e.g. "GER_fascism"
    bool hasLarge = false;
    bool hasMedium = false;
    bool hasSmall = false;
    
    bool isComplete() const { return hasLarge && hasMedium && hasSmall; }
};

// --- Converter Widgets ---

class ImagePreviewWidget : public QWidget {
    Q_OBJECT
public:
    ImagePreviewWidget(QWidget* parent = nullptr);
    void setImage(const QImage& img);
    void setCrop(const QRect& crop);
    void setZoom(double zoom);
    QRect getCrop() const { return m_crop; }
    QImage getImage() const { return m_image; }
    QSize sizeHint() const override;

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;

signals:
    void cropChanged(const QRect& rect);
    void zoomRequested(double delta);

private:
    QImage m_image;
    QRect m_crop;
    double m_zoom = 1.0;
    bool m_dragging = false;
    QPoint m_lastMousePos;
};

class FlagConverterWidget : public QWidget {
    Q_OBJECT
public:
    FlagConverterWidget(FlagManagerTool* tool, QWidget* parent = nullptr);
    void addFiles(const QStringList& paths);
    void setSidebarList(QTreeWidget* list);
    void updateTexts();

protected:
    void dragEnterEvent(QDragEnterEvent* event) override;
    void dropEvent(QDropEvent* event) override;

private slots:
    void onFileSelected();
    void onNameChanged(const QString& text);
    void onZoomChanged(int value);
    void onExportCurrent();
    void onExportAll();
    void onImportClicked();

private:
    struct FlagItem {
        QString path;
        QString name;
        QImage image;
        QRect crop;
    };

    void exportItem(const FlagItem& item, const QString& baseDir);

    FlagManagerTool* m_tool;
    QTreeWidget* m_fileList = nullptr;
    QMap<QString, FlagItem> m_items;
    QString m_currentPath;

    ImagePreviewWidget* m_preview;
    QLabel* m_zoomLabel;
    QLabel* m_nameLabel;
    QLabel* m_cropLabel;
    QLabel *m_labelL, *m_labelT, *m_labelR, *m_labelB;
    QPushButton* m_exportBtn;
    QPushButton* m_exportAllBtn;
    QPushButton* m_importBtn;
    QLineEdit* m_nameEdit;
    QLineEdit* m_cropLeft, *m_cropTop, *m_cropRight, *m_cropBottom;
    QSlider* m_zoomSlider;
};

// --- Browser Widgets ---

class FlagBrowserWidget : public QWidget {
    Q_OBJECT
public:
    FlagBrowserWidget(FlagManagerTool* tool, QWidget* parent = nullptr);
    void setSidebarList(QTreeWidget* list);
    void refreshData();
    void updateTexts();

private slots:
    void onTagSelected(QTreeWidgetItem* item, int col);
    void onSizeChanged(int id); // 0=Large, 1=Medium, 2=Small

private:
    void updateFlagDisplay();
    QImage loadTga(const QString& path);

    FlagManagerTool* m_tool;
    QTreeWidget* m_tagList = nullptr;
    QWidget* m_scrollContent;
    QScrollArea* m_scrollArea;
    QLabel* m_placeholder;
    
    QButtonGroup* m_sizeGroup;
    QPushButton* m_sizeBtns[3];
    int m_currentSizeIndex = 0; // 0=Large
    
    QString m_selectedTag;
    QMap<QString, QList<FlagVariant>> m_tagMap; // TAG -> Variants
    QMap<QString, QString> m_flagPaths; // "baseName_sizeIndex" -> absPath
};

// --- Main Container ---

class FlagManagerMainWidget : public QWidget {
    Q_OBJECT
public:
    FlagManagerMainWidget(FlagManagerTool* tool, QWidget* parent = nullptr);
    FlagConverterWidget* getConverter() const { return m_converter; }
    FlagBrowserWidget* getBrowser() const { return m_browser; }
    void updateTexts();

private slots:
    void onModeChanged(int index);

private:
    FlagManagerTool* m_tool;
    QStackedWidget* m_stack;
    FlagConverterWidget* m_converter;
    FlagBrowserWidget* m_browser;
    QWidget* m_tabBar;
    QPushButton* m_browserBtn;
    QPushButton* m_converterBtn;
};

// --- Sidebar ---

class FlagListWidget : public QWidget {
    Q_OBJECT
public:
    FlagListWidget(FlagManagerTool* tool, QWidget* parent = nullptr);
    QTreeWidget* getList() const { return m_list; }
    void setMode(int mode);
    void updateTexts();

private:
    FlagManagerTool* m_tool;
    QTreeWidget* m_list;
    QLabel* m_header;
    int m_currentMode = 0;
};

// --- Tool Class ---

class FlagManagerTool : public QObject, public ToolInterface {
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
    QWidget* createSidebarWidget(QWidget* parent = nullptr) override;
    void loadLanguage(const QString& lang) override;

    void switchMode(int mode);
    QString getString(const QString& key);

private:
    QMap<QString, QString> m_localizedNames;
    QMap<QString, QString> m_localizedDescs;
    QJsonObject m_localizedStrings;
    QString m_currentLang;
    
    QString m_id;
    QString m_version;
    QString m_compatibleVersion;
    QString m_author;

    FlagManagerMainWidget* m_mainWidget = nullptr;
    FlagListWidget* m_listWidget = nullptr;
};

#endif // FLAGMANAGERTOOL_H
