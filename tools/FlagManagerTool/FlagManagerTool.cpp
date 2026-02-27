#include "FlagManagerTool.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QDebug>
#include <QDir>
#include <QCoreApplication>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QFileDialog>
#include <QPainter>
#include <QMouseEvent>
#include <QScrollBar>
#include <QBuffer>
#include <QMimeData>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QScrollArea>
#include <QHeaderView>
#include <QWheelEvent>
#include <QFileInfo>
#include <QImageReader>
#include "../../src/TagManager.h"
#include "../../src/FileManager.h"
#include "../../src/ConfigManager.h"
#include "../../src/Logger.h"
#include "../../src/ToolManager.h"
#include "../../src/CustomMessageBox.h"
#include <QMenu>

#ifdef Q_OS_WIN
#include <windows.h>
#include <wincodec.h>
#include <comdef.h>
#pragma comment(lib, "windowscodecs.lib")
#pragma comment(lib, "ole32.lib")
#endif

// --- ImagePreviewWidget ---

ImagePreviewWidget::ImagePreviewWidget(QWidget* parent) : QWidget(parent), m_scrollArea(nullptr) {
    setMouseTracking(true);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
}

void ImagePreviewWidget::setScrollArea(QScrollArea* sa) {
    m_scrollArea = sa;
}

void ImagePreviewWidget::setImage(const QImage& img) {
    m_image = img;
    m_crop = m_image.isNull() ? QRect() : m_image.rect();
    update();
    updateGeometry();
}

QSize ImagePreviewWidget::sizeHint() const {
    if (m_image.isNull()) {
        if (m_scrollArea) {
            return m_scrollArea->viewport()->size();
        }
        return QSize(400, 300);
    }
    return m_image.size() * m_zoom;
}

QSize ImagePreviewWidget::minimumSizeHint() const {
    return QSize(200, 150);
}

void ImagePreviewWidget::setZoom(double zoom) {
    m_zoom = qBound(0.1, zoom, 10.0);
    if (!m_image.isNull()) {
        QSize newSize = m_image.size() * m_zoom;
        setMinimumSize(newSize);
        resize(newSize);
    }
    update();
}

void ImagePreviewWidget::setCrop(const QRect& crop) {
    m_crop = crop;
    update();
}

void ImagePreviewWidget::paintEvent(QPaintEvent*) {
    QPainter painter(this);
    bool isDark = ConfigManager::instance().getTheme() == ConfigManager::Theme::Dark;
    painter.fillRect(rect(), isDark ? QColor("#1E1E1E") : QColor("#F0F0F0"));

    if (m_image.isNull()) return;

    // 计算图像绘制位置，使其居中
    QSize imgSize(m_image.width() * m_zoom, m_image.height() * m_zoom);
    int x = (width() - imgSize.width()) / 2;
    int y = (height() - imgSize.height()) / 2;
    if (x < 0) x = 0;
    if (y < 0) y = 0;
    
    m_imageOffset = QPoint(x, y);
    QRect target(x, y, imgSize.width(), imgSize.height());
    painter.drawImage(target, m_image);

    if (!m_crop.isNull()) {
        // 绘制裁剪框，确保边线正确显示在坐标位置上
        // m_crop 存储的是 (left, top, width, height)
        // 为了让右边线显示在 left+width-1 的像素右侧，下边线显示在 top+height-1 的像素下方
        // 需要绘制宽度为 width，高度为 height 的矩形
        double cropX = x + m_crop.x() * m_zoom;
        double cropY = y + m_crop.y() * m_zoom;
        double cropW = m_crop.width() * m_zoom;
        double cropH = m_crop.height() * m_zoom;
        painter.setPen(QPen(Qt::green, 2));
        painter.drawRect(QRectF(cropX, cropY, cropW, cropH));
    }
}

void ImagePreviewWidget::mousePressEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton) {
        m_dragging = true;
        m_lastMousePos = event->pos();
        setCursor(Qt::ClosedHandCursor);
    }
}

void ImagePreviewWidget::mouseMoveEvent(QMouseEvent* event) {
    if (m_dragging && m_scrollArea) {
        QPoint delta = event->pos() - m_lastMousePos;
        m_lastMousePos = event->pos();
        m_scrollArea->horizontalScrollBar()->setValue(m_scrollArea->horizontalScrollBar()->value() - delta.x());
        m_scrollArea->verticalScrollBar()->setValue(m_scrollArea->verticalScrollBar()->value() - delta.y());
    }
}

void ImagePreviewWidget::mouseReleaseEvent(QMouseEvent*) {
    m_dragging = false;
    setCursor(Qt::ArrowCursor);
}

void ImagePreviewWidget::wheelEvent(QWheelEvent* event) {
    double delta = event->angleDelta().y() > 0 ? 0.1 : -0.1;
    emit zoomRequested(delta);
}

// --- FlagConverterWidget ---

FlagConverterWidget::FlagConverterWidget(FlagManagerTool* tool, QWidget* parent) 
    : QWidget(parent), m_tool(tool) {
    
    setAcceptDrops(true);
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);
    
    // 预览区使用与上方栏一致的背景色
    m_previewContainer = new QWidget();
    m_previewContainer->setObjectName("PreviewContainer");
    
    QVBoxLayout* previewLayout = new QVBoxLayout(m_previewContainer);
    previewLayout->setContentsMargins(0, 0, 0, 0);
    
    m_preview = new ImagePreviewWidget();
    m_currentZoom = 100;
    connect(m_preview, &ImagePreviewWidget::zoomRequested, [this](double delta){
        m_currentZoom = qBound(10, m_currentZoom + (int)(delta * 100), 500);
        m_preview->setZoom(m_currentZoom / 100.0);
    });

    QScrollArea* scroll = new QScrollArea();
    scroll->setWidget(m_preview);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setAlignment(Qt::AlignCenter);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroll->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_preview->setScrollArea(scroll);
    previewLayout->addWidget(scroll);
    mainLayout->addWidget(m_previewContainer, 1);
    
    // 控制面板
    m_controlPanel = new QWidget();
    m_controlPanel->setObjectName("ControlPanel");
    m_controlPanel->setFixedHeight(50);
    
    // 单行布局：Name | Crop（居中）
    QHBoxLayout* ctrlLayout = new QHBoxLayout(m_controlPanel);
    ctrlLayout->setContentsMargins(16, 8, 16, 8);
    ctrlLayout->setSpacing(12);
    ctrlLayout->addStretch();
    
    m_nameLabel = new QLabel("Name");
    m_nameEdit = new QLineEdit();
    m_nameEdit->setFixedWidth(100);
    m_nameEdit->setPlaceholderText("TAG_suffix");
    connect(m_nameEdit, &QLineEdit::textChanged, this, &FlagConverterWidget::onNameChanged);
    
    ctrlLayout->addWidget(m_nameLabel);
    ctrlLayout->addWidget(m_nameEdit);
    ctrlLayout->addSpacing(16);
    
    m_cropLabel = new QLabel("Crop");
    ctrlLayout->addWidget(m_cropLabel);

    m_labelL = new QLabel("L");
    m_cropLeft = new QLineEdit();
    m_cropLeft->setFixedWidth(40);
    m_cropLeft->setAlignment(Qt::AlignCenter);
    connect(m_cropLeft, &QLineEdit::textChanged, this, &FlagConverterWidget::onCropChanged);
    
    m_labelT = new QLabel("T");
    m_cropTop = new QLineEdit();
    m_cropTop->setFixedWidth(40);
    m_cropTop->setAlignment(Qt::AlignCenter);
    connect(m_cropTop, &QLineEdit::textChanged, this, &FlagConverterWidget::onCropChanged);
    
    m_labelR = new QLabel("R");
    m_cropRight = new QLineEdit();
    m_cropRight->setFixedWidth(40);
    m_cropRight->setAlignment(Qt::AlignCenter);
    connect(m_cropRight, &QLineEdit::textChanged, this, &FlagConverterWidget::onCropChanged);
    
    m_labelB = new QLabel("B");
    m_cropBottom = new QLineEdit();
    m_cropBottom->setFixedWidth(40);
    m_cropBottom->setAlignment(Qt::AlignCenter);
    connect(m_cropBottom, &QLineEdit::textChanged, this, &FlagConverterWidget::onCropChanged);
    
    ctrlLayout->addWidget(m_labelL); ctrlLayout->addWidget(m_cropLeft);
    ctrlLayout->addWidget(m_labelT); ctrlLayout->addWidget(m_cropTop);
    ctrlLayout->addWidget(m_labelR); ctrlLayout->addWidget(m_cropRight);
    ctrlLayout->addWidget(m_labelB); ctrlLayout->addWidget(m_cropBottom);
    ctrlLayout->addStretch();

    mainLayout->addWidget(m_controlPanel);
    
    // 应用初始主题
    applyTheme();
}

void FlagConverterWidget::applyTheme() {
    bool isDark = ConfigManager::instance().getTheme() == ConfigManager::Theme::Dark;
    
    // 预览区背景
    QString previewBg = isDark ? "#252526" : "#F0F0F0";
    m_previewContainer->setStyleSheet(QString("QWidget#PreviewContainer { background-color: %1; }").arg(previewBg));
    
    // 控制面板样式
    QString panelBg = isDark ? "#1E1E1E" : "#FFFFFF";
    QString borderColor = isDark ? "rgba(70,70,75,0.8)" : "rgba(210,210,215,0.8)";
    QString inputBg = isDark ? "#3A3A3C" : "#FFFFFF";
    QString inputBorder = isDark ? "#545456" : "#D1D1D6";
    QString inputText = isDark ? "#FFFFFF" : "#1D1D1F";
    QString labelColor = isDark ? "#98989D" : "#86868B";
    QString sliderGroove = isDark ? "#48484A" : "#D1D1D6";
    
    m_controlPanel->setStyleSheet(QString(R"(
        QWidget#ControlPanel {
            background-color: %1;
            border-top: 1px solid %2;
        }
        QLabel {
            color: %3;
            font-size: 11px;
            font-weight: 500;
            background: transparent;
        }
        QLineEdit {
            background-color: %4;
            border: 1px solid %5;
            border-radius: 4px;
            padding: 3px 6px;
            color: %6;
            font-size: 11px;
        }
        QLineEdit:focus {
            border: 1px solid #007AFF;
        }
        QSlider::groove:horizontal {
            background: %7;
            height: 4px;
            border-radius: 2px;
        }
        QSlider::handle:horizontal {
            background: white;
            width: 14px;
            height: 14px;
            margin: -5px 0;
            border-radius: 7px;
            border: 1px solid rgba(0,0,0,0.15);
        }
        QSlider::sub-page:horizontal {
            background: #007AFF;
            border-radius: 2px;
        }
    )").arg(panelBg, borderColor, labelColor, inputBg, inputBorder, inputText, sliderGroove));
}

void FlagConverterWidget::updateTexts() {
    m_nameLabel->setText(m_tool->getString("FlagName"));
    m_cropLabel->setText(m_tool->getString("Crop"));
    m_labelL->setText(m_tool->getString("L"));
    m_labelT->setText(m_tool->getString("T"));
    m_labelR->setText(m_tool->getString("R"));
    m_labelB->setText(m_tool->getString("B"));
}

void FlagConverterWidget::dragEnterEvent(QDragEnterEvent* event) {
    if (event->mimeData()->hasUrls()) event->acceptProposedAction();
}

void FlagConverterWidget::dropEvent(QDropEvent* event) {
    static const QStringList supportedExts = {"png", "jpg", "jpeg", "tga", "dds", "jxr", "webp"};
    QStringList paths;
    for (const QUrl& url : event->mimeData()->urls()) {
        QString path = url.toLocalFile();
        if (path.isEmpty()) continue;
        QString ext = QFileInfo(path).suffix().toLower();
        if (supportedExts.contains(ext)) paths.append(path);
    }
    if (!paths.isEmpty()) addFiles(paths);
}

void FlagConverterWidget::addFiles(const QStringList& paths) {
    for (const QString& path : paths) {
        if (m_items.contains(path)) continue;
        QImage img = loadImageFile(path);
        if (img.isNull()) continue;
        FlagItem item;
        item.path = path;
        item.name = ""; 
        item.image = img;
        item.crop = img.rect();
        m_items.insert(path, item);
        if (m_fileList) {
            QTreeWidgetItem* listItem = new QTreeWidgetItem(m_fileList);
            listItem->setText(0, "");
            listItem->setText(1, QFileInfo(path).fileName());
            listItem->setData(0, Qt::UserRole, path);
            listItem->setIcon(0, QIcon(QPixmap::fromImage(img.scaled(32, 32, Qt::KeepAspectRatio, Qt::SmoothTransformation))));
        }
    }
}

void FlagConverterWidget::setSidebarList(QTreeWidget* list) {
    if (m_fileList) {
        disconnect(m_fileList, &QTreeWidget::itemSelectionChanged, this, &FlagConverterWidget::onFileSelected);
        disconnect(m_fileList, &QTreeWidget::customContextMenuRequested, this, &FlagConverterWidget::onContextMenuRequested);
    }
    m_fileList = list;
    m_fileList->setColumnCount(2);
    m_fileList->setHeaderHidden(false);
    m_fileList->setContextMenuPolicy(Qt::CustomContextMenu);
    m_fileList->setSelectionMode(QAbstractItemView::ExtendedSelection);
    
    m_fileList->clear();
    for (auto it = m_items.begin(); it != m_items.end(); ++it) {
        QTreeWidgetItem* listItem = new QTreeWidgetItem(m_fileList);
        listItem->setText(0, it.value().name);
        listItem->setText(1, QFileInfo(it.key()).fileName());
        listItem->setData(0, Qt::UserRole, it.key());
        listItem->setIcon(0, QIcon(QPixmap::fromImage(it.value().image.scaled(32, 32, Qt::KeepAspectRatio, Qt::SmoothTransformation))));
    }

    connect(m_fileList, &QTreeWidget::itemSelectionChanged, this, &FlagConverterWidget::onFileSelected);
    connect(m_fileList, &QTreeWidget::customContextMenuRequested, this, &FlagConverterWidget::onContextMenuRequested);
}

void FlagConverterWidget::onFileSelected() {
    QList<QTreeWidgetItem*> sel = m_fileList->selectedItems();
    emit selectionChanged(!sel.isEmpty());
    if (sel.isEmpty()) return;
    QString path = sel[0]->data(0, Qt::UserRole).toString();
    if (m_items.contains(path)) {
        m_currentPath = path;
        const FlagItem& flag = m_items[path];
        m_preview->setImage(flag.image);
        m_preview->setCrop(flag.crop);
        m_nameEdit->setText(flag.name);
        
        // 阻塞信号以避免在设置过程中触发验证
        m_cropLeft->blockSignals(true);
        m_cropTop->blockSignals(true);
        m_cropRight->blockSignals(true);
        m_cropBottom->blockSignals(true);
        
        m_cropLeft->setText(QString::number(flag.crop.left()));
        m_cropTop->setText(QString::number(flag.crop.top()));
        m_cropRight->setText(QString::number(flag.crop.right())); 
        m_cropBottom->setText(QString::number(flag.crop.bottom()));
        
        // 解除阻塞
        m_cropLeft->blockSignals(false);
        m_cropTop->blockSignals(false);
        m_cropRight->blockSignals(false);
        m_cropBottom->blockSignals(false);
    }
}

void FlagConverterWidget::onNameChanged(const QString& text) {
    if (m_currentPath.isEmpty() || !m_items.contains(m_currentPath)) return;
    
    // 检查名字是否与其他旗帜重复
    if (!text.isEmpty()) {
        for (auto it = m_items.begin(); it != m_items.end(); ++it) {
            if (it.key() != m_currentPath && it.value().name == text) {
                // 名字重复，显示错误并恢复原值
                m_nameEdit->blockSignals(true);
                m_nameEdit->setText(m_items[m_currentPath].name);
                m_nameEdit->blockSignals(false);
                m_nameEdit->setStyleSheet(m_nameEdit->styleSheet() + "QLineEdit { border: 1px solid red; }");
                m_nameEdit->setToolTip(m_tool->getString("ErrorDuplicateName"));
                return;
            }
        }
    }
    
    // 名字有效，清除错误样式
    m_nameEdit->setToolTip("");
    
    m_items[m_currentPath].name = text;
    QList<QTreeWidgetItem*> items = m_fileList->findItems(QFileInfo(m_currentPath).fileName(), Qt::MatchExactly, 1);
    for (QTreeWidgetItem* item : items) {
        if (item->data(0, Qt::UserRole).toString() == m_currentPath) {
            item->setText(0, text);
            break;
        }
    }
}

void FlagConverterWidget::onCropChanged() {
    if (m_currentPath.isEmpty() || !m_items.contains(m_currentPath)) return;
    
    const QImage& img = m_items[m_currentPath].image;
    int maxX = img.width() - 1;
    int maxY = img.height() - 1;
    
    bool okL, okT, okR, okB;
    int l = m_cropLeft->text().toInt(&okL);
    int t = m_cropTop->text().toInt(&okT);
    int r = m_cropRight->text().toInt(&okR);
    int b = m_cropBottom->text().toInt(&okB);
    
    // 验证所有条件
    bool valid = okL && okT && okR && okB &&
                 l >= 0 && t >= 0 &&
                 r <= maxX && b <= maxY &&
                 l < r && t < b;
    
    auto setError = [this](QLineEdit* edit, bool hasError) {
        if (hasError) {
            edit->setStyleSheet("QLineEdit { border: 1px solid red; }");
            edit->setToolTip(m_tool->getString("ErrorInvalidCrop"));
        } else {
            edit->setStyleSheet("");
            edit->setToolTip("");
        }
    };
    
    if (!valid) {
        // 标记无效的输入框
        setError(m_cropLeft, !okL || l < 0 || l >= r);
        setError(m_cropTop, !okT || t < 0 || t >= b);
        setError(m_cropRight, !okR || r > maxX || r <= l);
        setError(m_cropBottom, !okB || b > maxY || b <= t);
        
        // 恢复到上一次有效值
        const QRect& oldCrop = m_items[m_currentPath].crop;
        m_cropLeft->blockSignals(true);
        m_cropTop->blockSignals(true);
        m_cropRight->blockSignals(true);
        m_cropBottom->blockSignals(true);
        m_cropLeft->setText(QString::number(oldCrop.left()));
        m_cropTop->setText(QString::number(oldCrop.top()));
        m_cropRight->setText(QString::number(oldCrop.right()));
        m_cropBottom->setText(QString::number(oldCrop.bottom()));
        m_cropLeft->blockSignals(false);
        m_cropTop->blockSignals(false);
        m_cropRight->blockSignals(false);
        m_cropBottom->blockSignals(false);
        return;
    }
    
    // 清除错误样式
    setError(m_cropLeft, false);
    setError(m_cropTop, false);
    setError(m_cropRight, false);
    setError(m_cropBottom, false);
    
    QRect newCrop(l, t, r - l + 1, b - t + 1);
    m_items[m_currentPath].crop = newCrop;
    m_preview->setCrop(newCrop);
}

void FlagConverterWidget::onExportCurrent() {
    if (m_currentPath.isEmpty()) return;
    if (exportItem(m_items[m_currentPath], ConfigManager::instance().getModPath())) {
        QString pathToRemove = m_currentPath;
        removeItem(pathToRemove);
        if (!m_items.isEmpty()) {
            selectFirstItem();
        } else {
            clearPreview();
        }
    }
}

void FlagConverterWidget::onExportAll() {
    QStringList toRemove;
    for (auto it = m_items.begin(); it != m_items.end(); ++it) {
        if (exportItem(it.value(), ConfigManager::instance().getModPath())) {
            toRemove.append(it.key());
        }
    }
    for (const QString& path : toRemove) {
        removeItem(path);
    }
    if (!m_items.isEmpty()) {
        selectFirstItem();
    } else {
        clearPreview();
    }
}

void FlagConverterWidget::removeItem(const QString& path) {
    if (!m_items.contains(path)) return;
    m_items.remove(path);
    if (m_fileList) {
        for (int i = 0; i < m_fileList->topLevelItemCount(); ++i) {
            QTreeWidgetItem* item = m_fileList->topLevelItem(i);
            if (item->data(0, Qt::UserRole).toString() == path) {
                delete m_fileList->takeTopLevelItem(i);
                break;
            }
        }
    }
    if (m_currentPath == path) {
        m_currentPath.clear();
    }
}

void FlagConverterWidget::selectFirstItem() {
    if (!m_fileList || m_fileList->topLevelItemCount() == 0) {
        clearPreview();
        return;
    }
    // 找到名字最靠前的项目
    QTreeWidgetItem* firstItem = nullptr;
    QString firstName;
    for (int i = 0; i < m_fileList->topLevelItemCount(); ++i) {
        QTreeWidgetItem* item = m_fileList->topLevelItem(i);
        QString name = item->text(0);
        if (firstItem == nullptr || (!name.isEmpty() && (firstName.isEmpty() || name < firstName))) {
            firstItem = item;
            firstName = name;
        }
    }
    if (firstItem) {
        m_fileList->setCurrentItem(firstItem);
        onFileSelected();
    } else {
        clearPreview();
    }
}

void FlagConverterWidget::clearPreview() {
    m_currentPath.clear();
    m_preview->setImage(QImage());
    m_nameEdit->clear();
    m_cropLeft->clear();
    m_cropTop->clear();
    m_cropRight->clear();
    m_cropBottom->clear();
}

void FlagConverterWidget::onImportClicked() {
    QStringList files = QFileDialog::getOpenFileNames(this, "Import Images", "", "Images (*.png *.jpg *.jpeg *.tga *.dds *.jxr *.webp)");
    addFiles(files);
}

void FlagConverterWidget::onContextMenuRequested(const QPoint& pos) {
    QTreeWidgetItem* item = m_fileList->itemAt(pos);
    if (!item) return;
    
    QMenu menu(this);
    
    // 简洁的系统风格菜单样式，根据主题切换
    bool isDark = ConfigManager::instance().getTheme() == ConfigManager::Theme::Dark;
    QString menuStyle = isDark ? 
        "QMenu { background-color: #3c3c3c; border: 1px solid #555; padding: 4px 0; }"
        "QMenu::item { color: #e0e0e0; padding: 6px 32px 6px 12px; }"
        "QMenu::item:selected { background-color: #0078d4; color: white; }"
        "QMenu::separator { height: 1px; background: #555; margin: 4px 0; }"
        :
        "QMenu { background-color: #f8f8f8; border: 1px solid #ccc; padding: 4px 0; }"
        "QMenu::item { color: #333; padding: 6px 32px 6px 12px; }"
        "QMenu::item:selected { background-color: #0078d4; color: white; }"
        "QMenu::separator { height: 1px; background: #ccc; margin: 4px 0; }";
    menu.setStyleSheet(menuStyle);
    
    QAction* fillAction = menu.addAction(m_tool->getString("FillName"));
    connect(fillAction, &QAction::triggered, this, &FlagConverterWidget::fillNameFromFileName);
    
    QAction* removeAction = menu.addAction(m_tool->getString("RemoveFromList"));
    connect(removeAction, &QAction::triggered, this, &FlagConverterWidget::removeSelectedFile);
    menu.exec(m_fileList->viewport()->mapToGlobal(pos));
}

void FlagConverterWidget::removeSelectedFile() {
    QList<QTreeWidgetItem*> sel = m_fileList->selectedItems();
    if (sel.isEmpty()) return;
    
    for (QTreeWidgetItem* item : sel) {
        QString path = item->data(0, Qt::UserRole).toString();
        m_items.remove(path);
        if (path == m_currentPath) {
            m_currentPath.clear();
            m_preview->setImage(QImage());
            m_nameEdit->clear();
            m_cropLeft->clear();
            m_cropTop->clear();
            m_cropRight->clear();
            m_cropBottom->clear();
        }
        delete item;
    }
}

bool FlagConverterWidget::hasSelection() const {
    return m_fileList && !m_fileList->selectedItems().isEmpty();
}

void FlagConverterWidget::selectAll() {
    if (!m_fileList) return;
    m_fileList->selectAll();
}

void FlagConverterWidget::deselectAll() {
    if (!m_fileList) return;
    m_fileList->clearSelection();
}

void FlagConverterWidget::fillNameFromFileName() {
    QList<QTreeWidgetItem*> sel = m_fileList->selectedItems();
    if (sel.isEmpty()) return;
    
    // 收集已使用的名字
    QSet<QString> usedNames;
    for (auto it = m_items.begin(); it != m_items.end(); ++it) {
        if (!it.value().name.isEmpty()) {
            usedNames.insert(it.value().name);
        }
    }
    
    for (QTreeWidgetItem* item : sel) {
        QString path = item->data(0, Qt::UserRole).toString();
        if (!m_items.contains(path)) continue;
        
        // 从文件名提取名字（不含扩展名）
        QString fileName = QFileInfo(path).baseName();
        
        // 如果名字已被使用，跳过
        if (usedNames.contains(fileName)) continue;
        
        // 设置名字
        m_items[path].name = fileName;
        usedNames.insert(fileName);
        item->setText(0, fileName);
        
        // 如果是当前选中的项目，更新编辑框
        if (path == m_currentPath) {
            m_nameEdit->blockSignals(true);
            m_nameEdit->setText(fileName);
            m_nameEdit->blockSignals(false);
        }
    }
}

// 使用 Windows WIC 加载图片（支持 DDS, WebP, JXR 等格式）
#ifdef Q_OS_WIN
static QImage loadImageWithWIC(const QString& path) {
    QImage result;
    
    // 初始化 COM
    HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    bool comInitialized = SUCCEEDED(hr) || hr == S_FALSE;
    
    IWICImagingFactory* pFactory = nullptr;
    IWICBitmapDecoder* pDecoder = nullptr;
    IWICBitmapFrameDecode* pFrame = nullptr;
    IWICFormatConverter* pConverter = nullptr;
    
    do {
        // 创建 WIC 工厂
        hr = CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER,
                              IID_IWICImagingFactory, reinterpret_cast<void**>(&pFactory));
        if (FAILED(hr)) break;
        
        // 创建解码器
        std::wstring wpath = path.toStdWString();
        hr = pFactory->CreateDecoderFromFilename(wpath.c_str(), nullptr, GENERIC_READ,
                                                  WICDecodeMetadataCacheOnDemand, &pDecoder);
        if (FAILED(hr)) break;
        
        // 获取第一帧
        hr = pDecoder->GetFrame(0, &pFrame);
        if (FAILED(hr)) break;
        
        // 创建格式转换器
        hr = pFactory->CreateFormatConverter(&pConverter);
        if (FAILED(hr)) break;
        
        // 转换为 32bpp BGRA 格式
        hr = pConverter->Initialize(pFrame, GUID_WICPixelFormat32bppBGRA,
                                    WICBitmapDitherTypeNone, nullptr, 0.0,
                                    WICBitmapPaletteTypeCustom);
        if (FAILED(hr)) break;
        
        // 获取图像尺寸
        UINT width = 0, height = 0;
        hr = pConverter->GetSize(&width, &height);
        if (FAILED(hr)) break;
        
        // 创建 QImage 并复制像素数据
        result = QImage(width, height, QImage::Format_ARGB32);
        UINT stride = width * 4;
        UINT bufferSize = stride * height;
        
        hr = pConverter->CopyPixels(nullptr, stride, bufferSize, result.bits());
        if (FAILED(hr)) {
            result = QImage();
            break;
        }
        
    } while (false);
    
    // 清理资源
    if (pConverter) pConverter->Release();
    if (pFrame) pFrame->Release();
    if (pDecoder) pDecoder->Release();
    if (pFactory) pFactory->Release();
    if (comInitialized) CoUninitialize();
    
    return result;
}
#endif

QImage FlagConverterWidget::loadImageFile(const QString& path) {
    QString ext = QFileInfo(path).suffix().toLower();
    
    // 对于 DDS, WebP, JXR 格式，使用 Windows WIC API
#ifdef Q_OS_WIN
    if (ext == "dds" || ext == "webp" || ext == "jxr" || ext == "wdp" || ext == "hdp") {
        QImage img = loadImageWithWIC(path);
        if (!img.isNull()) return img;
    }
#endif
    
    // 对于 TGA 文件，使用自定义解析器
    if (ext == "tga") {
        QFile file(path);
        if (!file.open(QIODevice::ReadOnly)) return QImage();
        
        QByteArray data = file.readAll();
        if (data.size() < 18) return QImage();
        
        const uchar* ptr = reinterpret_cast<const uchar*>(data.constData());
        
        int idLength = ptr[0];
        int colorMapType = ptr[1];
        int imageType = ptr[2];
        int width = ptr[12] | (ptr[13] << 8);
        int height = ptr[14] | (ptr[15] << 8);
        int bpp = ptr[16];
        int descriptor = ptr[17];
        
        if (colorMapType != 0 || (imageType != 2 && imageType != 10)) {
            QImageReader reader(path);
            if (reader.canRead()) return reader.read();
            return QImage();
        }
        
        if (bpp != 24 && bpp != 32) return QImage();
        
        int bytesPerPixel = bpp / 8;
        int pixelDataOffset = 18 + idLength;
        
        if (data.size() < pixelDataOffset) return QImage();
        
        QImage image(width, height, bpp == 32 ? QImage::Format_ARGB32 : QImage::Format_RGB32);
        
        const uchar* pixelData = ptr + pixelDataOffset;
        int pixelCount = width * height;
        
        if (imageType == 2) {
            for (int y = 0; y < height; ++y) {
                int destY = (descriptor & 0x20) ? y : (height - 1 - y);
                for (int x = 0; x < width; ++x) {
                    int srcIdx = (y * width + x) * bytesPerPixel;
                    if (pixelDataOffset + srcIdx + bytesPerPixel > data.size()) break;
                    
                    uchar b = pixelData[srcIdx];
                    uchar g = pixelData[srcIdx + 1];
                    uchar r = pixelData[srcIdx + 2];
                    uchar a = (bytesPerPixel == 4) ? pixelData[srcIdx + 3] : 255;
                    
                    image.setPixel(x, destY, qRgba(r, g, b, a));
                }
            }
        } else if (imageType == 10) {
            int currentPixel = 0;
            int dataIdx = 0;
            int maxDataSize = data.size() - pixelDataOffset;
            
            while (currentPixel < pixelCount && dataIdx < maxDataSize) {
                uchar header = pixelData[dataIdx++];
                int count = (header & 0x7F) + 1;
                
                if (header & 0x80) {
                    if (dataIdx + bytesPerPixel > maxDataSize) break;
                    uchar b = pixelData[dataIdx];
                    uchar g = pixelData[dataIdx + 1];
                    uchar r = pixelData[dataIdx + 2];
                    uchar a = (bytesPerPixel == 4) ? pixelData[dataIdx + 3] : 255;
                    dataIdx += bytesPerPixel;
                    
                    for (int i = 0; i < count && currentPixel < pixelCount; ++i, ++currentPixel) {
                        int x = currentPixel % width;
                        int y = currentPixel / width;
                        int destY = (descriptor & 0x20) ? y : (height - 1 - y);
                        image.setPixel(x, destY, qRgba(r, g, b, a));
                    }
                } else {
                    for (int i = 0; i < count && currentPixel < pixelCount; ++i, ++currentPixel) {
                        if (dataIdx + bytesPerPixel > maxDataSize) break;
                        uchar b = pixelData[dataIdx];
                        uchar g = pixelData[dataIdx + 1];
                        uchar r = pixelData[dataIdx + 2];
                        uchar a = (bytesPerPixel == 4) ? pixelData[dataIdx + 3] : 255;
                        dataIdx += bytesPerPixel;
                        
                        int x = currentPixel % width;
                        int y = currentPixel / width;
                        int destY = (descriptor & 0x20) ? y : (height - 1 - y);
                        image.setPixel(x, destY, qRgba(r, g, b, a));
                    }
                }
            }
        }
        
        return image;
    }
    
    // 对于其他格式，使用 QImageReader
    QImageReader reader(path);
    if (reader.canRead()) {
        return reader.read();
    }
    
    // 最后尝试 QImage 直接加载
    return QImage(path);
}

// 保存未压缩32位ARGB TGA文件
static bool saveTga32(const QImage& img, const QString& path) {
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly)) return false;
    
    QImage image = img.convertToFormat(QImage::Format_ARGB32);
    int width = image.width();
    int height = image.height();
    
    // TGA Header (18 bytes)
    uchar header[18] = {0};
    header[2] = 2;  // Image type: uncompressed true-color
    header[12] = width & 0xFF;
    header[13] = (width >> 8) & 0xFF;
    header[14] = height & 0xFF;
    header[15] = (height >> 8) & 0xFF;
    header[16] = 32; // Bits per pixel
    header[17] = 0;  // Image descriptor: bottom-to-top
    
    file.write(reinterpret_cast<const char*>(header), 18);
    
    // Pixel data: BGRA order, bottom row first
    for (int y = height - 1; y >= 0; --y) {
        const QRgb* line = reinterpret_cast<const QRgb*>(image.constScanLine(y));
        for (int x = 0; x < width; ++x) {
            QRgb pixel = line[x];
            uchar bgra[4] = {
                static_cast<uchar>(qBlue(pixel)),
                static_cast<uchar>(qGreen(pixel)),
                static_cast<uchar>(qRed(pixel)),
                static_cast<uchar>(qAlpha(pixel))
            };
            file.write(reinterpret_cast<const char*>(bgra), 4);
        }
    }
    
    file.close();
    return true;
}

bool FlagConverterWidget::exportItem(const FlagItem& item, const QString& baseDir) {
    if (item.name.isEmpty() || baseDir.isEmpty()) return false;
    
    // 检查是否有文件会被覆盖
    struct Size { int w; int h; QString suffix; };
    Size sizes[] = {{82, 52, ""}, {41, 26, "medium/"}, {10, 7, "small/"}};
    
    QStringList existingFiles;
    for (const auto& s : sizes) {
        QString filePath = baseDir + "/gfx/flags/" + s.suffix + item.name + ".tga";
        if (QFile::exists(filePath)) {
            existingFiles.append(s.suffix + item.name + ".tga");
        }
    }
    
    // 如果有文件会被覆盖，询问用户
    if (!existingFiles.isEmpty()) {
        QString fileList = existingFiles.join("\n");
        QString message = m_tool->getString("ConfirmOverwrite").arg(fileList);
        
        QMessageBox::StandardButton result = CustomMessageBox::question(
            const_cast<FlagConverterWidget*>(this),
            m_tool->getString("ConfirmOverwriteTitle"),
            message
        );
        
        if (result != QMessageBox::Yes) {
            return false;
        }
    }
    
    QImage cropped = item.image.copy(item.crop);
    for (const auto& s : sizes) {
        QImage resized = cropped.scaled(s.w, s.h, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
        QString dir = baseDir + "/gfx/flags/" + s.suffix;
        QDir().mkpath(dir);
        if (!saveTga32(resized, dir + item.name + ".tga")) return false;
    }
    return true;
}

// --- FlagBrowserWidget ---

FlagBrowserWidget::FlagBrowserWidget(FlagManagerTool* tool, QWidget* parent)
    : QWidget(parent), m_tool(tool) {
    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    m_scrollArea = new QScrollArea();
    m_scrollContent = new QWidget();
    m_scrollArea->setWidget(m_scrollContent);
    m_scrollArea->setWidgetResizable(true);
    m_scrollArea->setFrameShape(QFrame::NoFrame);
    layout->addWidget(m_scrollArea, 1);

    m_placeholder = new QLabel("Select a TAG to view flags.");
    m_placeholder->setAlignment(Qt::AlignCenter);
    layout->addWidget(m_placeholder);
    
    // 连接FileManager的scanFinished信号以实时刷新
    connect(&FileManager::instance(), &FileManager::scanFinished, this, &FlagBrowserWidget::refreshData);
    
    applyTheme();
}

void FlagBrowserWidget::applyTheme() {
    bool isDark = ConfigManager::instance().getTheme() == ConfigManager::Theme::Dark;
    QString textColor = isDark ? "#CCCCCC" : "#666666";
    m_placeholder->setStyleSheet(QString("color: %1; font-size: 18px;").arg(textColor));
}

void FlagBrowserWidget::updateTexts() {
    m_placeholder->setText(m_tool->getString("BrowserPlaceholder"));
}

void FlagBrowserWidget::setSizeIndex(int index) {
    m_currentSizeIndex = index;
    updateFlagDisplay();
}

void FlagBrowserWidget::setSidebarList(QTreeWidget* list) {
    Logger::instance().logInfo("FlagBrowserWidget", "setSidebarList() called");
    if (m_tagList) {
        Logger::instance().logInfo("FlagBrowserWidget", "Disconnecting old m_tagList signal");
        disconnect(m_tagList, &QTreeWidget::itemClicked, this, &FlagBrowserWidget::onTagSelected);
    }
    m_tagList = list;
    Logger::instance().logInfo("FlagBrowserWidget", "Clearing m_tagList");
    m_tagList->clear();
    Logger::instance().logInfo("FlagBrowserWidget", "Setting column count to 1");
    m_tagList->setColumnCount(1);
    Logger::instance().logInfo("FlagBrowserWidget", "Setting header hidden");
    m_tagList->setHeaderHidden(true);
    Logger::instance().logInfo("FlagBrowserWidget", "Connecting itemClicked signal");
    connect(m_tagList, &QTreeWidget::itemClicked, this, &FlagBrowserWidget::onTagSelected);
    Logger::instance().logInfo("FlagBrowserWidget", "setSidebarList() completed");
}

void FlagBrowserWidget::refreshData() {
    Logger::instance().logInfo("FlagBrowserWidget", "refreshData() called");
    
    // 检查 m_tagList 是否已初始化
    if (!m_tagList) {
        Logger::instance().logWarning("FlagBrowserWidget", "m_tagList is null, skipping refresh");
        return;
    }
    
    Logger::instance().logInfo("FlagBrowserWidget", "Clearing m_tagMap and m_flagPaths...");
    m_tagMap.clear();
    m_flagPaths.clear();
    Logger::instance().logInfo("FlagBrowserWidget", "Cleared");
    
    Logger::instance().logInfo("FlagBrowserWidget", "Getting tags from TagManager...");
    QMap<QString, QString> allTags = TagManager::instance().getTags();
    Logger::instance().logInfo("FlagBrowserWidget", QString("Got %1 tags").arg(allTags.size()));
    
    Logger::instance().logInfo("FlagBrowserWidget", "Creating validTags QSet...");
    QSet<QString> validTags;
    for (const QString& key : allTags.keys()) {
        validTags.insert(key);
    }
    Logger::instance().logInfo("FlagBrowserWidget", QString("Created validTags with %1 entries").arg(validTags.size()));
    
    // 首先为所有有效 tag 创建空的 FlagVariant 列表
    Logger::instance().logInfo("FlagBrowserWidget", "Initializing m_tagMap for all tags...");
    for (const QString& tag : validTags) {
        m_tagMap[tag] = QList<FlagVariant>();
    }
    Logger::instance().logInfo("FlagBrowserWidget", "m_tagMap initialized");
    
    Logger::instance().logInfo("FlagBrowserWidget", "Getting effective files from FileManager...");
    QMap<QString, FileDetails> effectiveFiles = FileManager::instance().getEffectiveFiles();
    Logger::instance().logInfo("FlagBrowserWidget", QString("Got %1 effective files").arg(effectiveFiles.size()));
    
    for (auto it = effectiveFiles.begin(); it != effectiveFiles.end(); ++it) {
        const QString& relPath = it.key();
        const FileDetails& details = it.value();
        
        if (!relPath.startsWith("gfx/flags/") || !relPath.endsWith(".tga")) continue;
        
        QString subPath = relPath.mid(10);
        QString baseName;
        int sizeIndex = 0;
        
        if (subPath.startsWith("medium/")) {
            baseName = QFileInfo(subPath.mid(7)).baseName();
            sizeIndex = 1;
        } else if (subPath.startsWith("small/")) {
            baseName = QFileInfo(subPath.mid(6)).baseName();
            sizeIndex = 2;
        } else {
            baseName = QFileInfo(subPath).baseName();
            sizeIndex = 0;
        }
        
        QString tag = baseName.left(3).toUpper();
        bool isCosmetic = !validTags.contains(tag);
        QString finalTag = isCosmetic ? "COSMETIC" : tag;
        
        auto& list = m_tagMap[finalTag];
        FlagVariant* found = nullptr;
        for (auto& v : list) { if (v.name == baseName) { found = &v; break; } }
        if (!found) { FlagVariant v; v.name = baseName; list.append(v); found = &list.last(); }
        
        if (sizeIndex == 0) found->hasLarge = true;
        else if (sizeIndex == 1) found->hasMedium = true;
        else found->hasSmall = true;
        
        QString key = baseName + "_" + QString::number(sizeIndex);
        m_flagPaths[key] = details.absPath;
    }

    m_tagList->clear();
    // 注意：QTreeWidgetItem(parent, ...) 构造函数已自动将 item 添加到 parent
    // 不要再调用 addTopLevelItem，否则会重复添加导致崩溃
    new QTreeWidgetItem(m_tagList, {"COSMETIC"});
    
    Logger::instance().logInfo("FlagBrowserWidget", "Building tag list...");
    
    // 遍历所有有效 tag（包括没有旗帜的）
    for (auto it = m_tagMap.begin(); it != m_tagMap.end(); ++it) {
        if (it.key() == "COSMETIC") continue;
        QTreeWidgetItem* item = new QTreeWidgetItem(m_tagList, {it.key()});
        
        const QList<FlagVariant>& variants = it.value();
        bool hasDefaultFlag = false;  // 是否有 TAG.tga（无后缀旗帜）
        bool allComplete = true;
        
        for (const auto& v : variants) {
            if (v.name.toUpper() == it.key()) hasDefaultFlag = true;
            if (!v.isComplete()) allComplete = false;
        }
        
        // 如果没有无后缀旗帜（TAG.tga），标记为红色
        if (!hasDefaultFlag) {
            item->setForeground(0, Qt::red);
        } else if (!allComplete) {
            item->setForeground(0, QColor(255, 165, 0));
        }
        
        // 尝试加载小图标（TAG_2 对应 small/TAG.tga）
        QString key = it.key() + "_2";
        if (m_flagPaths.contains(key)) {
            QImage img = loadTga(m_flagPaths[key]);
            if (!img.isNull()) {
                item->setIcon(0, QIcon(QPixmap::fromImage(img)));
            }
        }
        // 不再调用 addTopLevelItem，构造函数已自动添加
    }
    
    Logger::instance().logInfo("FlagBrowserWidget", "Tag list built successfully");
}

void FlagBrowserWidget::onTagSelected(QTreeWidgetItem* item, int) {
    m_selectedTag = item->text(0);
    m_placeholder->hide();
    updateFlagDisplay();
}

QImage FlagBrowserWidget::loadTga(const QString& path) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) return QImage();
    
    QByteArray data = file.readAll();
    if (data.size() < 18) return QImage();
    
    const uchar* ptr = reinterpret_cast<const uchar*>(data.constData());
    
    int idLength = ptr[0];
    int colorMapType = ptr[1];
    int imageType = ptr[2];
    int width = ptr[12] | (ptr[13] << 8);
    int height = ptr[14] | (ptr[15] << 8);
    int bpp = ptr[16];
    int descriptor = ptr[17];
    
    if (colorMapType != 0 || (imageType != 2 && imageType != 10)) {
        QImageReader reader(path);
        if (reader.canRead()) return reader.read();
        return QImage();
    }
    
    if (bpp != 24 && bpp != 32) return QImage();
    
    int bytesPerPixel = bpp / 8;
    int pixelDataOffset = 18 + idLength;
    
    if (data.size() < pixelDataOffset) return QImage();
    
    QImage image(width, height, bpp == 32 ? QImage::Format_ARGB32 : QImage::Format_RGB32);
    
    const uchar* pixelData = ptr + pixelDataOffset;
    int pixelCount = width * height;
    
    if (imageType == 2) {
        for (int y = 0; y < height; ++y) {
            int destY = (descriptor & 0x20) ? y : (height - 1 - y);
            for (int x = 0; x < width; ++x) {
                int srcIdx = (y * width + x) * bytesPerPixel;
                if (pixelDataOffset + srcIdx + bytesPerPixel > data.size()) break;
                
                uchar b = pixelData[srcIdx];
                uchar g = pixelData[srcIdx + 1];
                uchar r = pixelData[srcIdx + 2];
                uchar a = (bytesPerPixel == 4) ? pixelData[srcIdx + 3] : 255;
                
                image.setPixel(x, destY, qRgba(r, g, b, a));
            }
        }
    } else if (imageType == 10) {
        int currentPixel = 0;
        int dataIdx = 0;
        int maxDataSize = data.size() - pixelDataOffset;
        
        while (currentPixel < pixelCount && dataIdx < maxDataSize) {
            uchar header = pixelData[dataIdx++];
            int count = (header & 0x7F) + 1;
            
            if (header & 0x80) {
                if (dataIdx + bytesPerPixel > maxDataSize) break;
                uchar b = pixelData[dataIdx];
                uchar g = pixelData[dataIdx + 1];
                uchar r = pixelData[dataIdx + 2];
                uchar a = (bytesPerPixel == 4) ? pixelData[dataIdx + 3] : 255;
                dataIdx += bytesPerPixel;
                
                for (int i = 0; i < count && currentPixel < pixelCount; ++i, ++currentPixel) {
                    int x = currentPixel % width;
                    int y = currentPixel / width;
                    int destY = (descriptor & 0x20) ? y : (height - 1 - y);
                    image.setPixel(x, destY, qRgba(r, g, b, a));
                }
            } else {
                for (int i = 0; i < count && currentPixel < pixelCount; ++i, ++currentPixel) {
                    if (dataIdx + bytesPerPixel > maxDataSize) break;
                    uchar b = pixelData[dataIdx];
                    uchar g = pixelData[dataIdx + 1];
                    uchar r = pixelData[dataIdx + 2];
                    uchar a = (bytesPerPixel == 4) ? pixelData[dataIdx + 3] : 255;
                    dataIdx += bytesPerPixel;
                    
                    int x = currentPixel % width;
                    int y = currentPixel / width;
                    int destY = (descriptor & 0x20) ? y : (height - 1 - y);
                    image.setPixel(x, destY, qRgba(r, g, b, a));
                }
            }
        }
    }
    
    return image;
}

void FlagBrowserWidget::updateFlagDisplay() {
    if (m_scrollContent) {
        QLayout* oldLayout = m_scrollContent->layout();
        if (oldLayout) {
            QLayoutItem* item;
            while ((item = oldLayout->takeAt(0)) != nullptr) {
                if (item->layout()) {
                    QLayoutItem* subItem;
                    while ((subItem = item->layout()->takeAt(0)) != nullptr) {
                        delete subItem->widget();
                        delete subItem;
                    }
                }
                delete item->widget();
                delete item;
            }
            delete oldLayout;
        }
    }

    bool isDark = ConfigManager::instance().getTheme() == ConfigManager::Theme::Dark;
    QString imgBg = isDark ? "#333333" : "#E0E0E0";
    QString imgBorder = isDark ? "#555555" : "#CCCCCC";

    QGridLayout* gl = new QGridLayout(m_scrollContent);
    gl->setContentsMargins(20, 20, 20, 20);
    gl->setSpacing(20);
    
    auto variants = m_tagMap[m_selectedTag];
    QSize dispSize = (m_currentSizeIndex == 1 ? QSize(41, 26) : (m_currentSizeIndex == 2 ? QSize(10, 7) : QSize(82, 52)));

    int row = 0, col = 0;
    int cellWidth = 180;
    
    for (const auto& v : variants) {
        QWidget* cellWidget = new QWidget();
        cellWidget->setFixedWidth(cellWidth);
        QVBoxLayout* cell = new QVBoxLayout(cellWidget);
        cell->setContentsMargins(5, 5, 5, 5);
        cell->setSpacing(5);
        
        QLabel* img = new QLabel();
        img->setFixedSize(dispSize * 2);
        img->setAlignment(Qt::AlignCenter);
        img->setStyleSheet(QString("background: %1; border: 1px solid %2;").arg(imgBg, imgBorder));
        
        QString key = v.name + "_" + QString::number(m_currentSizeIndex);
        QString flagPath = m_flagPaths.value(key);
        
        QImage flag;
        if (!flagPath.isEmpty()) {
            flag = loadTga(flagPath);
        }
        
        if (!flag.isNull()) {
            img->setPixmap(QPixmap::fromImage(flag.scaled(dispSize * 2, Qt::KeepAspectRatio)));
        } else {
            img->setText("MISSING");
        }

        cell->addWidget(img, 0, Qt::AlignCenter);
        QLabel* name = new QLabel(v.name);
        name->setAlignment(Qt::AlignCenter);
        name->setWordWrap(true);
        name->setFixedWidth(cellWidth - 10);
        cell->addWidget(name, 0, Qt::AlignCenter);
        gl->addWidget(cellWidget, row, col);
        if (++col > 5) { col = 0; row++; }
    }
    gl->setRowStretch(row + 1, 1);
    gl->setColumnStretch(6, 1);
}

// --- FlagManagerMainWidget ---

FlagManagerMainWidget::FlagManagerMainWidget(FlagManagerTool* tool, QWidget* parent)
    : QWidget(parent), m_tool(tool) {
    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    
    m_tabBar = new QWidget();
    m_tabBar->setObjectName("FlagTabBar");
    m_tabBar->setFixedHeight(40);
    QHBoxLayout* tabLayout = new QHBoxLayout(m_tabBar);
    tabLayout->setContentsMargins(10, 0, 10, 0);
    tabLayout->setSpacing(8);
    
    m_browserBtn = new QPushButton("Manage");
    m_converterBtn = new QPushButton("New");
    m_browserBtn->setCheckable(true);
    m_converterBtn->setCheckable(true);
    connect(m_browserBtn, &QPushButton::clicked, [this](){ onModeChanged(0); });
    connect(m_converterBtn, &QPushButton::clicked, [this](){ onModeChanged(1); });
    
    tabLayout->addWidget(m_browserBtn);
    tabLayout->addWidget(m_converterBtn);
    tabLayout->addStretch();
    
    // 管理模式的尺寸按钮容器
    m_sizeContainer = new QWidget();
    QHBoxLayout* sizeLayout = new QHBoxLayout(m_sizeContainer);
    sizeLayout->setContentsMargins(0, 0, 0, 0);
    sizeLayout->setSpacing(4);
    
    m_sizeGroup = new QButtonGroup(this);
    QString defaultSizeTexts[] = {"Large", "Medium", "Small"};
    for (int i = 0; i < 3; ++i) {
        m_sizeBtns[i] = new QPushButton(defaultSizeTexts[i]);
        m_sizeBtns[i]->setCheckable(true);
        m_sizeBtns[i]->setFixedHeight(28);
        m_sizeGroup->addButton(m_sizeBtns[i], i);
        sizeLayout->addWidget(m_sizeBtns[i]);
    }
    m_sizeBtns[0]->setChecked(true);
    connect(m_sizeGroup, &QButtonGroup::idClicked, this, &FlagManagerMainWidget::onSizeChanged);
    tabLayout->addWidget(m_sizeContainer);
    
    // 新建模式的导入导出按钮容器
    m_actionContainer = new QWidget();
    QHBoxLayout* actionLayout = new QHBoxLayout(m_actionContainer);
    actionLayout->setContentsMargins(0, 0, 0, 0);
    actionLayout->setSpacing(8);
    
    m_importBtn = new QPushButton("Import");
    m_exportBtn = new QPushButton("Export");
    m_exportAllBtn = new QPushButton("Export All");
    m_selectAllBtn = new QPushButton("Select All");
    m_importBtn->setFixedSize(80, 28);
    m_exportBtn->setFixedSize(80, 28);
    m_exportAllBtn->setFixedSize(80, 28);
    m_selectAllBtn->setFixedSize(80, 28);
    
    actionLayout->addWidget(m_importBtn);
    actionLayout->addWidget(m_exportBtn);
    actionLayout->addWidget(m_exportAllBtn);
    actionLayout->addWidget(m_selectAllBtn);
    tabLayout->addWidget(m_actionContainer);
    m_actionContainer->hide();
    
    layout->addWidget(m_tabBar);
    
    m_stack = new QStackedWidget();
    m_browser = new FlagBrowserWidget(tool);
    m_converter = new FlagConverterWidget(tool);
    m_stack->addWidget(m_browser);
    m_stack->addWidget(m_converter);
    layout->addWidget(m_stack);
    
    // 连接导入导出按钮到 converter
    connect(m_importBtn, &QPushButton::clicked, m_converter, &FlagConverterWidget::onImportClicked);
    connect(m_exportBtn, &QPushButton::clicked, m_converter, &FlagConverterWidget::onExportCurrent);
    connect(m_exportAllBtn, &QPushButton::clicked, m_converter, &FlagConverterWidget::onExportAll);
    
    // 连接全选按钮和选择状态变化信号
    connect(m_selectAllBtn, &QPushButton::clicked, this, &FlagManagerMainWidget::onSelectAllClicked);
    connect(m_converter, &FlagConverterWidget::selectionChanged, this, &FlagManagerMainWidget::onSelectionChanged);
    
    m_stack->setCurrentIndex(0);
    m_browserBtn->setChecked(true);
    m_converterBtn->setChecked(false);
    applyTheme();
    updateButtonStyles(0);
    updateToolbarVisibility(0);
}

void FlagManagerMainWidget::applyTheme() {
    bool isDark = ConfigManager::instance().getTheme() == ConfigManager::Theme::Dark;
    QString tabBarBg = isDark ? "#252526" : "#F0F0F0";
    QString borderColor = isDark ? "#3F3F46" : "#E0E0E0";
    QString textColor = isDark ? "#AAAAAA" : "#666666";
    
    m_tabBar->setStyleSheet(QString("QWidget#FlagTabBar { background: %1; border-bottom: 1px solid %2; padding-bottom: 0px; margin-bottom: 0px; }").arg(tabBarBg, borderColor));
    
    // 设置容器透明背景
    m_sizeContainer->setStyleSheet("background: transparent;");
    m_actionContainer->setStyleSheet("background: transparent;");
    
    // 大中小按钮和导入导出按钮使用与管理/新建按钮一致的风格，根据主题调整文字颜色
    QString inactiveStyle = QString("QPushButton { border: none; background: transparent; color: %1; border-radius: 5px; padding: 5px 15px; } QPushButton:hover { background: rgba(128,128,128,0.15); } QPushButton:checked { background: #007AFF; color: white; font-weight: bold; }").arg(textColor);
    
    for (int i = 0; i < 3; ++i) {
        m_sizeBtns[i]->setStyleSheet(inactiveStyle);
    }
    
    m_importBtn->setStyleSheet(inactiveStyle);
    m_exportBtn->setStyleSheet(inactiveStyle);
    m_exportAllBtn->setStyleSheet(inactiveStyle);
    m_selectAllBtn->setStyleSheet(inactiveStyle);
    
    m_browser->applyTheme();
    m_converter->applyTheme();
}

void FlagManagerMainWidget::updateButtonStyles(int activeIndex) {
    QString active = "QPushButton { border: none; background: #007AFF; color: white; border-radius: 5px; padding: 5px 15px; font-weight: bold; }";
    QString inactive = "QPushButton { border: none; background: transparent; color: gray; border-radius: 5px; padding: 5px 15px; } QPushButton:hover { background: rgba(128,128,128,0.1); }";
    m_browserBtn->setStyleSheet(activeIndex == 0 ? active : inactive);
    m_converterBtn->setStyleSheet(activeIndex == 1 ? active : inactive);
}

void FlagManagerMainWidget::updateToolbarVisibility(int modeIndex) {
    m_sizeContainer->setVisible(modeIndex == 0);
    m_actionContainer->setVisible(modeIndex == 1);
}

void FlagManagerMainWidget::updateTexts() {
    m_browserBtn->setText(m_tool->getString("TabManage"));
    m_converterBtn->setText(m_tool->getString("TabNew"));
    m_sizeBtns[0]->setText(m_tool->getString("SizeLarge"));
    m_sizeBtns[1]->setText(m_tool->getString("SizeMedium"));
    m_sizeBtns[2]->setText(m_tool->getString("SizeSmall"));
    m_importBtn->setText(m_tool->getString("ImportFiles"));
    m_exportBtn->setText(m_tool->getString("Export"));
    m_exportAllBtn->setText(m_tool->getString("ExportAll"));
    updateSelectAllButton(m_hasSelection);
    m_browser->updateTexts();
    m_converter->updateTexts();
}

void FlagManagerMainWidget::onModeChanged(int index) {
    m_stack->setCurrentIndex(index);
    m_browserBtn->setChecked(index == 0);
    m_converterBtn->setChecked(index == 1);
    updateButtonStyles(index);
    updateToolbarVisibility(index);
    m_tool->switchMode(index);
}

void FlagManagerMainWidget::onSizeChanged(int id) {
    m_browser->setSizeIndex(id);
}

void FlagManagerMainWidget::onSelectAllClicked() {
    if (m_hasSelection) {
        m_converter->deselectAll();
    } else {
        m_converter->selectAll();
    }
}

void FlagManagerMainWidget::onSelectionChanged(bool hasSelection) {
    m_hasSelection = hasSelection;
    updateSelectAllButton(hasSelection);
}

void FlagManagerMainWidget::updateSelectAllButton(bool hasSelection) {
    if (hasSelection) {
        m_selectAllBtn->setText(m_tool->getString("DeselectAll"));
    } else {
        m_selectAllBtn->setText(m_tool->getString("SelectAll"));
    }
}

// --- FlagListWidget ---

FlagListWidget::FlagListWidget(FlagManagerTool* tool, QWidget* parent)
    : QWidget(parent), m_tool(tool) {
    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    m_header = new QLabel("TAGs");
    layout->addWidget(m_header);
    m_list = new QTreeWidget();
    m_list->setHeaderHidden(true);
    m_list->setIndentation(0);
    m_list->setFrameShape(QFrame::NoFrame);
    layout->addWidget(m_list);
    applyTheme();
}

void FlagListWidget::applyTheme() {
    bool isDark = ConfigManager::instance().getTheme() == ConfigManager::Theme::Dark;
    QString headerColor = isDark ? "#CCCCCC" : "#666666";
    QString listBg = isDark ? "#2C2C2E" : "#F5F5F7";
    QString listText = isDark ? "#FFFFFF" : "#1D1D1F";
    QString itemHover = isDark ? "#3A3A3C" : "#E8E8E8";
    QString itemSelected = isDark ? "#0A84FF" : "#007AFF";
    
    m_header->setStyleSheet(QString("font-weight: bold; padding: 10px; color: %1;").arg(headerColor));
    m_list->setStyleSheet(QString(R"(
        QTreeWidget {
            background-color: %1; border: none; color: %2;
        }
        QTreeWidget::item {
            padding: 5px;
        }
        QTreeWidget::item:hover {
            background-color: %3;
        }
        QTreeWidget::item:selected {
            background-color: %4; color: white;
        }
        QHeaderView::section {
            background-color: %1; color: %2; border: none; padding: 5px;
        }
    )").arg(listBg, listText, itemHover, itemSelected));
}

void FlagListWidget::updateTexts() {
    m_header->setText(m_currentMode == 0 ? m_tool->getString("Tags") : m_tool->getString("Files"));
    if (m_currentMode == 1) {
        m_list->setHeaderHidden(false);
        m_list->setHeaderLabels({m_tool->getString("ColFlagName"), m_tool->getString("ColFileName")});
    } else m_list->setHeaderHidden(true);
}

void FlagListWidget::setMode(int mode) {
    m_currentMode = mode;
    updateTexts();
}

// --- FlagManagerTool ---

QString FlagManagerTool::name() const { return m_localizedNames.value(m_currentLang, "Flag Manager"); }
QString FlagManagerTool::description() const { return m_localizedDescs.value(m_currentLang, "Flag Manager"); }
void FlagManagerTool::setMetaData(const QJsonObject& metaData) {
    m_id = metaData.value("id").toString();
    m_version = metaData.value("version").toString();
    m_compatibleVersion = metaData.value("compatibleVersion").toString();
    m_author = metaData.value("author").toString();
}
QIcon FlagManagerTool::icon() const {
    // Try to find the tools directory
    QDir appDir(QCoreApplication::applicationDirPath());
    
    QString toolsPath;
    if (appDir.exists("tools")) {
        toolsPath = appDir.filePath("tools");
    } else if (QDir(appDir.filePath("../tools")).exists()) {
        toolsPath = appDir.filePath("../tools");
    }
    
    if (!toolsPath.isEmpty()) {
        QString coverPath = toolsPath + "/FlagManagerTool/cover.png";
        if (QFile::exists(coverPath)) {
            return QIcon(coverPath);
        }
    }
    
    // Fallback
    return QIcon::fromTheme("flag");
}
void FlagManagerTool::initialize() { loadLanguage("English"); }
QWidget* FlagManagerTool::createWidget(QWidget* parent) {
    m_mainWidget = new FlagManagerMainWidget(this, parent);
    return m_mainWidget;
}
QWidget* FlagManagerTool::createSidebarWidget(QWidget* parent) {
    Logger::instance().logInfo("FlagManagerTool", "createSidebarWidget() called");
    m_listWidget = new FlagListWidget(this, parent);
    Logger::instance().logInfo("FlagManagerTool", "FlagListWidget created");
    if (m_mainWidget) {
        Logger::instance().logInfo("FlagManagerTool", "m_mainWidget exists, calling setSidebarList...");
        m_mainWidget->getBrowser()->setSidebarList(m_listWidget->getList());
        Logger::instance().logInfo("FlagManagerTool", "setSidebarList completed, calling refreshData...");
        m_mainWidget->getBrowser()->refreshData();
        Logger::instance().logInfo("FlagManagerTool", "refreshData completed");
    } else {
        Logger::instance().logWarning("FlagManagerTool", "m_mainWidget is null!");
    }
    Logger::instance().logInfo("FlagManagerTool", "createSidebarWidget() returning");
    return m_listWidget;
}
void FlagManagerTool::loadLanguage(const QString& lang) {
    m_currentLang = lang;
    QDir appDir(QCoreApplication::applicationDirPath());
    QString locPath = appDir.filePath("tools/FlagManagerTool/localization");
    QString langFile = (lang == "English" ? "en_US" : (lang == "简体中文" ? "zh_CN" : "zh_TW"));
    QFile file(locPath + "/" + langFile + ".json");
    if (file.open(QIODevice::ReadOnly)) {
        m_localizedStrings = QJsonDocument::fromJson(file.readAll()).object();
        m_localizedNames[lang] = m_localizedStrings["Name"].toString();
        m_localizedDescs[lang] = m_localizedStrings["Description"].toString();
        if (m_mainWidget) m_mainWidget->updateTexts();
        if (m_listWidget) m_listWidget->updateTexts();
    }
}
QString FlagManagerTool::getString(const QString& key) { return m_localizedStrings.value(key).toString(key); }
void FlagManagerTool::applyTheme() {
    if (m_mainWidget) m_mainWidget->applyTheme();
    if (m_listWidget) m_listWidget->applyTheme();
}
void FlagManagerTool::switchMode(int mode) {
    if (m_listWidget) {
        m_listWidget->setMode(mode);
        if (mode == 0 && m_mainWidget) m_mainWidget->getBrowser()->refreshData();
        else if (mode == 1 && m_mainWidget) m_mainWidget->getConverter()->setSidebarList(m_listWidget->getList());
    }
}
