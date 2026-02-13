#include "FileTreeWidget.h"
#include "../../FileManager.h"
#include "../../ConfigManager.h"
#include <QVBoxLayout>
#include <QHeaderView>
#include <QFileInfo>
#include <QDebug>
#include <QPainter>
#include <QPainterPath>

FileTreeWidget::FileTreeWidget(QWidget *parent) : QWidget(parent) {
    setupUi();
    buildTree();
    
    // Connect to ConfigManager for theme changes
    connect(&ConfigManager::instance(), &ConfigManager::themeChanged, this, [this](ConfigManager::Theme){ updateTheme(); });
    updateTheme();
}

void FileTreeWidget::setupUi() {
    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setContentsMargins(10, 10, 10, 10);
    layout->setSpacing(10);

    // Search Bar
    m_searchBox = new QLineEdit();
    m_searchBox->setPlaceholderText("Search files...");
    connect(m_searchBox, &QLineEdit::textChanged, this, &FileTreeWidget::filterChanged);
    layout->addWidget(m_searchBox);

    // Tree
    m_tree = new QTreeWidget();
    m_tree->setHeaderLabels({"Name", "Source", "Path"});
    m_tree->setColumnWidth(0, 300);
    m_tree->setColumnWidth(1, 100);
    m_tree->setAlternatingRowColors(true);
    m_tree->setIndentation(20);
    m_tree->setUniformRowHeights(true);
    m_tree->setRootIsDecorated(true);
    m_tree->setAnimated(true);
    
    connect(m_tree, &QTreeWidget::itemClicked, this, &FileTreeWidget::onItemClicked);
    layout->addWidget(m_tree);

    // Path Label
    m_pathLabel = new QLabel("Select a file to see details");
    m_pathLabel->setWordWrap(true);
    layout->addWidget(m_pathLabel);
}

void FileTreeWidget::updateTheme() {
    ConfigManager::Theme theme = ConfigManager::instance().getTheme();
    bool isDark = (theme == ConfigManager::Theme::Dark);
    
    QString bg = isDark ? "#2C2C2E" : "#FFFFFF";
    QString text = isDark ? "#FFFFFF" : "#1D1D1F";
    QString border = isDark ? "#3A3A3C" : "#D2D2D7";
    QString inputBg = isDark ? "#1C1C1E" : "#FFFFFF";
    QString altRow = isDark ? "#3A3A3C" : "#F5F5F7";
    QString headerBg = isDark ? "#3A3A3C" : "#F5F5F7";
    QString hoverBg = isDark ? "#48484A" : "#E5E5EA";
    QString pathColor = isDark ? "#AAAAAA" : "#888888";

    // Search Box
    m_searchBox->setStyleSheet(QString(R"(
        QLineEdit {
            border: 1px solid %1;
            border-radius: 8px;
            padding: 8px 12px;
            background-color: %2;
            color: %3;
            font-size: 13px;
        }
        QLineEdit:focus {
            border: 1px solid #007AFF;
        }
    )").arg(border, inputBg, text));

    // Tree
    m_tree->setStyleSheet(QString(R"(
        QTreeWidget {
            border: 1px solid %1;
            border-radius: 8px;
            background-color: %2;
            color: %3;
            alternate-background-color: %4;
            font-size: 13px;
        }
        QTreeWidget::item {
            height: 28px;
            padding: 2px;
            border-radius: 4px;
        }
        QTreeWidget::item:selected {
            background-color: #007AFF;
            color: white;
        }
        QTreeWidget::item:hover:!selected {
            background-color: %5;
        }
        QHeaderView::section {
            background-color: %6;
            border: none;
            border-bottom: 1px solid %1;
            padding: 6px;
            font-weight: bold;
            color: %3;
        }
    )").arg(border, inputBg, text, altRow, hoverBg, headerBg));

    // Path Label
    m_pathLabel->setStyleSheet(QString("color: %1; font-style: italic; font-size: 12px; margin-top: 5px;").arg(pathColor));
    
    // Rebuild tree to update icons
    buildTree();
}

QIcon getIcon(bool isFolder, bool isDark) {
    QPixmap pixmap(16, 16);
    pixmap.fill(Qt::transparent);
    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);
    
    QColor color = isDark ? Qt::white : QColor("#1D1D1F");
    
    if (isFolder) {
        // Simple folder icon
        QPainterPath path;
        path.moveTo(1, 3);
        path.lineTo(6, 3);
        path.lineTo(8, 5);
        path.lineTo(15, 5);
        path.lineTo(15, 13);
        path.lineTo(1, 13);
        path.closeSubpath();
        
        painter.setPen(QPen(color, 1));
        painter.setBrush(isDark ? QColor(255, 255, 255, 50) : QColor(0, 0, 0, 20));
        painter.drawPath(path);
    } else {
        // Simple file icon
        QPainterPath path;
        path.moveTo(3, 1);
        path.lineTo(10, 1);
        path.lineTo(13, 4);
        path.lineTo(13, 15);
        path.lineTo(3, 15);
        path.closeSubpath();
        
        // Fold corner
        path.moveTo(10, 1);
        path.lineTo(10, 4);
        path.lineTo(13, 4);
        
        painter.setPen(QPen(color, 1));
        painter.setBrush(Qt::transparent);
        painter.drawPath(path);
    }
    
    return QIcon(pixmap);
}

void FileTreeWidget::buildTree() {
    m_tree->clear();
    
    // Get files from FileManager (which should handle priority/replace)
    QMap<QString, FileDetails> files = FileManager::instance().getEffectiveFiles();
    
    // We need to reconstruct the tree structure from flat relative paths
    // e.g. "common/national_focus/germany.txt" -> common -> national_focus -> germany.txt
    
    // Sort keys to ensure folders come before files or alphabetical order
    QStringList paths = files.keys();
    paths.sort();

    QMap<QString, QTreeWidgetItem*> dirItems; // Path -> Item
    
    ConfigManager::Theme theme = ConfigManager::instance().getTheme();
    bool isDark = (theme == ConfigManager::Theme::Dark);
    QIcon folderIcon = getIcon(true, isDark);
    QIcon fileIcon = getIcon(false, isDark);

    for (const QString& relPath : paths) {
        FileDetails details = files[relPath];
        QString absPath = details.absPath;
        QString source = details.source;
        
        QStringList parts = relPath.split('/');
        QString currentPath = "";
        QTreeWidgetItem *parentItem = nullptr;

        for (int i = 0; i < parts.size(); ++i) {
            QString part = parts[i];
            bool isFile = (i == parts.size() - 1);
            
            if (!currentPath.isEmpty()) currentPath += "/";
            currentPath += part;

            if (isFile) {
                QTreeWidgetItem *item = new QTreeWidgetItem(parentItem ? parentItem : m_tree->invisibleRootItem());
                item->setText(0, part);
                item->setText(1, source);
                item->setText(2, absPath); // Hidden or visible? Visible for now
                item->setData(0, Qt::UserRole, absPath);
                
                // Icon
                item->setIcon(0, fileIcon);
            } else {
                // Directory
                if (!dirItems.contains(currentPath)) {
                    QTreeWidgetItem *item = new QTreeWidgetItem(parentItem ? parentItem : m_tree->invisibleRootItem());
                    item->setText(0, part);
                    item->setIcon(0, folderIcon);
                    dirItems.insert(currentPath, item);
                    parentItem = item;
                } else {
                    parentItem = dirItems[currentPath];
                }
            }
        }
    }
}

void FileTreeWidget::filterChanged(const QString &text) {
    // Simple filter implementation
    // Hide items that don't match
    // This is recursive and might be slow for large trees, but okay for demo
    
    // TODO: Implement proper filtering
}

void FileTreeWidget::onItemClicked(QTreeWidgetItem *item, int column) {
    QString path = item->data(0, Qt::UserRole).toString();
    if (!path.isEmpty()) {
        m_pathLabel->setText(path);
    } else {
        // Directory
        m_pathLabel->setText(item->text(0));
    }
}

void FileTreeWidget::refreshTree() {
    buildTree();
}
