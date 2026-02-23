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

// --- ImagePreviewWidget ---

ImagePreviewWidget::ImagePreviewWidget(QWidget* parent) : QWidget(parent) {
    setMouseTracking(true);
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
}

void ImagePreviewWidget::setImage(const QImage& img) {
    m_image = img;
    m_crop = m_image.isNull() ? QRect() : m_image.rect();
    update();
    updateGeometry();
}

QSize ImagePreviewWidget::sizeHint() const {
    if (m_image.isNull()) return QSize(200, 200);
    return m_image.size() * m_zoom;
}

void ImagePreviewWidget::setZoom(double zoom) {
    m_zoom = qBound(0.1, zoom, 10.0);
    update();
    updateGeometry();
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

    QRect target(0, 0, m_image.width() * m_zoom, m_image.height() * m_zoom);
    painter.drawImage(target, m_image);

    if (!m_crop.isNull()) {
        QRect sc(m_crop.x() * m_zoom, m_crop.y() * m_zoom, m_crop.width() * m_zoom, m_crop.height() * m_zoom);
        painter.setPen(QPen(Qt::green, 2));
        painter.drawRect(sc);
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
    if (m_dragging) {
        QPoint delta = event->pos() - m_lastMousePos;
        QScrollArea* sa = qobject_cast<QScrollArea*>(parent()->parent());
        if (sa) {
            sa->horizontalScrollBar()->setValue(sa->horizontalScrollBar()->value() - delta.x());
            sa->verticalScrollBar()->setValue(sa->verticalScrollBar()->value() - delta.y());
        }
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
    
    m_preview = new ImagePreviewWidget();
    connect(m_preview, &ImagePreviewWidget::zoomRequested, [this](double delta){
        int newVal = m_zoomSlider->value() + (delta * 100);
        m_zoomSlider->setValue(qBound(10, newVal, 500));
    });

    QScrollArea* scroll = new QScrollArea();
    scroll->setWidget(m_preview);
    scroll->setWidgetResizable(false);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setAlignment(Qt::AlignCenter);
    mainLayout->addWidget(scroll, 1);
    
    QWidget* controlPanel = new QWidget();
    controlPanel->setObjectName("ControlPanel");
    controlPanel->setFixedHeight(140);
    controlPanel->setStyleSheet("QWidget#ControlPanel { background-color: rgba(128,128,128,0.05); border-top: 1px solid rgba(128,128,128,0.2); }");
    QVBoxLayout* ctrlLayout = new QVBoxLayout(controlPanel);
    ctrlLayout->setContentsMargins(20, 10, 20, 10);
    ctrlLayout->setSpacing(8);

    // Row 1: TAG and Zoom
    QHBoxLayout* row1 = new QHBoxLayout();
    m_nameLabel = new QLabel("Flag Name:");
    m_nameEdit = new QLineEdit();
    m_nameEdit->setFixedWidth(120);
    connect(m_nameEdit, &QLineEdit::textChanged, this, &FlagConverterWidget::onNameChanged);
    
    m_zoomLabel = new QLabel("Zoom 100%");
    m_zoomSlider = new QSlider(Qt::Horizontal);
    m_zoomSlider->setRange(10, 500);
    m_zoomSlider->setValue(100);
    m_zoomSlider->setFixedWidth(150);
    connect(m_zoomSlider, &QSlider::valueChanged, this, &FlagConverterWidget::onZoomChanged);
    
    row1->addWidget(m_nameLabel);
    row1->addWidget(m_nameEdit);
    row1->addSpacing(30);
    row1->addWidget(m_zoomLabel);
    row1->addWidget(m_zoomSlider);
    row1->addStretch();
    
    m_importBtn = new QPushButton("Import");
    m_importBtn->setFixedWidth(100);
    connect(m_importBtn, &QPushButton::clicked, this, &FlagConverterWidget::onImportClicked);
    row1->addWidget(m_importBtn);
    
    ctrlLayout->addLayout(row1);

    // Row 2: Crop
    QHBoxLayout* row2 = new QHBoxLayout();
    m_cropLabel = new QLabel("Crop:");
    row2->addWidget(m_cropLabel);

    m_labelL = new QLabel("L"); m_cropLeft = new QLineEdit(); m_cropLeft->setFixedWidth(50);
    m_labelT = new QLabel("T"); m_cropTop = new QLineEdit(); m_cropTop->setFixedWidth(50);
    m_labelR = new QLabel("R"); m_cropRight = new QLineEdit(); m_cropRight->setFixedWidth(50);
    m_labelB = new QLabel("B"); m_cropBottom = new QLineEdit(); m_cropBottom->setFixedWidth(50);
    
    row2->addWidget(m_labelL); row2->addWidget(m_cropLeft);
    row2->addWidget(m_labelT); row2->addWidget(m_cropTop);
    row2->addWidget(m_labelR); row2->addWidget(m_cropRight);
    row2->addWidget(m_labelB); row2->addWidget(m_cropBottom);
    row2->addStretch();
    
    m_exportBtn = new QPushButton("Export Current");
    m_exportAllBtn = new QPushButton("Export All");
    connect(m_exportBtn, &QPushButton::clicked, this, &FlagConverterWidget::onExportCurrent);
    connect(m_exportAllBtn, &QPushButton::clicked, this, &FlagConverterWidget::onExportAll);
    
    row2->addWidget(m_exportBtn);
    row2->addWidget(m_exportAllBtn);
    ctrlLayout->addLayout(row2);

    mainLayout->addWidget(controlPanel);
}

void FlagConverterWidget::updateTexts() {
    m_nameLabel->setText(m_tool->getString("FlagName"));
    m_cropLabel->setText(m_tool->getString("Crop"));
    m_exportBtn->setText(m_tool->getString("ExportCurrent"));
    m_exportAllBtn->setText(m_tool->getString("ExportAll"));
    m_importBtn->setText(m_tool->getString("ImportFiles"));
    m_zoomLabel->setText(QString("%1 %2%").arg(m_tool->getString("Zoom")).arg(m_zoomSlider->value()));
    m_labelL->setText(m_tool->getString("L"));
    m_labelT->setText(m_tool->getString("T"));
    m_labelR->setText(m_tool->getString("R"));
    m_labelB->setText(m_tool->getString("B"));
}

void FlagConverterWidget::dragEnterEvent(QDragEnterEvent* event) {
    if (event->mimeData()->hasUrls()) event->acceptProposedAction();
}

void FlagConverterWidget::dropEvent(QDropEvent* event) {
    QStringList paths;
    for (const QUrl& url : event->mimeData()->urls()) {
        QString path = url.toLocalFile();
        if (!path.isEmpty()) paths.append(path);
    }
    if (!paths.isEmpty()) addFiles(paths);
}

void FlagConverterWidget::addFiles(const QStringList& paths) {
    for (const QString& path : paths) {
        if (m_items.contains(path)) continue;
        QImage img(path);
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
    }
    m_fileList = list;
    m_fileList->setColumnCount(2);
    m_fileList->setHeaderHidden(false);
    
    m_fileList->clear();
    for (auto it = m_items.begin(); it != m_items.end(); ++it) {
        QTreeWidgetItem* listItem = new QTreeWidgetItem(m_fileList);
        listItem->setText(0, it.value().name);
        listItem->setText(1, QFileInfo(it.key()).fileName());
        listItem->setData(0, Qt::UserRole, it.key());
        listItem->setIcon(0, QIcon(QPixmap::fromImage(it.value().image.scaled(32, 32, Qt::KeepAspectRatio, Qt::SmoothTransformation))));
    }

    connect(m_fileList, &QTreeWidget::itemSelectionChanged, this, &FlagConverterWidget::onFileSelected);
}

void FlagConverterWidget::onFileSelected() {
    QList<QTreeWidgetItem*> sel = m_fileList->selectedItems();
    if (sel.isEmpty()) return;
    QString path = sel[0]->data(0, Qt::UserRole).toString();
    if (m_items.contains(path)) {
        m_currentPath = path;
        const FlagItem& flag = m_items[path];
        m_preview->setImage(flag.image);
        m_preview->setCrop(flag.crop);
        m_nameEdit->setText(flag.name);
        m_cropLeft->setText(QString::number(flag.crop.left()));
        m_cropTop->setText(QString::number(flag.crop.top()));
        m_cropRight->setText(QString::number(flag.crop.right())); 
        m_cropBottom->setText(QString::number(flag.crop.bottom()));
    }
}

void FlagConverterWidget::onNameChanged(const QString& text) {
    if (!m_currentPath.isEmpty() && m_items.contains(m_currentPath)) {
        m_items[m_currentPath].name = text;
        QList<QTreeWidgetItem*> items = m_fileList->findItems(QFileInfo(m_currentPath).fileName(), Qt::MatchExactly, 1);
        for (QTreeWidgetItem* item : items) {
            if (item->data(0, Qt::UserRole).toString() == m_currentPath) {
                item->setText(0, text);
                break;
            }
        }
    }
}

void FlagConverterWidget::onZoomChanged(int value) {
    double zoom = value / 100.0;
    m_zoomLabel->setText(QString("%1 %2%").arg(m_tool->getString("Zoom")).arg(value));
    m_preview->setZoom(zoom);
}

void FlagConverterWidget::onExportCurrent() {
    if (m_currentPath.isEmpty()) return;
    exportItem(m_items[m_currentPath], ConfigManager::instance().getModPath());
}

void FlagConverterWidget::onExportAll() {
    for (const auto& item : m_items) exportItem(item, ConfigManager::instance().getModPath());
}

void FlagConverterWidget::onImportClicked() {
    QStringList files = QFileDialog::getOpenFileNames(this, "Import Images", "", "Images (*.png *.jpg *.jpeg *.tga *.dds)");
    addFiles(files);
}

void FlagConverterWidget::exportItem(const FlagItem& item, const QString& baseDir) {
    if (item.name.isEmpty() || baseDir.isEmpty()) return;
    QImage cropped = item.image.copy(item.crop);
    struct Size { int w; int h; QString suffix; };
    Size sizes[] = {{82, 52, ""}, {41, 26, "medium/"}, {10, 7, "small/"}};
    for (const auto& s : sizes) {
        QImage resized = cropped.scaled(s.w, s.h, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
        QString dir = baseDir + "/gfx/flags/" + s.suffix;
        QDir().mkpath(dir);
        resized.convertTo(QImage::Format_ARGB32);
        resized.save(dir + item.name + ".tga", "TGA");
    }
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

    m_toolbar = new QWidget();
    m_toolbar->setObjectName("FlagToolbar");
    m_toolbar->setFixedHeight(40);
    QHBoxLayout* tbLayout = new QHBoxLayout(m_toolbar);
    tbLayout->setContentsMargins(10, 0, 10, 0);
    
    tbLayout->addStretch();
    m_sizeGroup = new QButtonGroup(this);
    QString defaultSizeTexts[] = {"Large", "Medium", "Small"};
    for (int i = 0; i < 3; ++i) {
        m_sizeBtns[i] = new QPushButton(defaultSizeTexts[i]);
        m_sizeBtns[i]->setCheckable(true);
        m_sizeGroup->addButton(m_sizeBtns[i], i);
        tbLayout->addWidget(m_sizeBtns[i]);
    }
    m_sizeBtns[0]->setChecked(true);
    connect(m_sizeGroup, &QButtonGroup::idClicked, this, &FlagBrowserWidget::onSizeChanged);
    tbLayout->addStretch();
    layout->addWidget(m_toolbar);
    
    applyTheme();
}

void FlagBrowserWidget::applyTheme() {
    bool isDark = ConfigManager::instance().getTheme() == ConfigManager::Theme::Dark;
    QString toolbarBg = isDark ? "#2D2D30" : "#F5F5F5";
    QString borderColor = isDark ? "#3F3F46" : "#E0E0E0";
    QString textColor = isDark ? "#CCCCCC" : "#666666";
    QString btnText = isDark ? "#FFFFFF" : "#333333";
    QString btnBg = isDark ? "#3C3C3C" : "#E8E8E8";
    QString btnHover = isDark ? "#4A4A4A" : "#D0D0D0";
    
    m_toolbar->setStyleSheet(QString("QWidget#FlagToolbar { background: %1; border-bottom: 1px solid %2; }").arg(toolbarBg, borderColor));
    m_placeholder->setStyleSheet(QString("color: %1; font-size: 18px;").arg(textColor));
    
    QString btnStyle = QString(R"(
        QPushButton {
            background-color: %1; color: %2; border: 1px solid %3; border-radius: 4px; padding: 5px 12px;
        }
        QPushButton:hover {
            background-color: %4;
        }
        QPushButton:checked {
            background-color: #007AFF; color: white; border: 1px solid #007AFF;
        }
    )").arg(btnBg, btnText, borderColor, btnHover);
    
    for (int i = 0; i < 3; ++i) {
        m_sizeBtns[i]->setStyleSheet(btnStyle);
    }
}

void FlagBrowserWidget::updateTexts() {
    m_placeholder->setText(m_tool->getString("BrowserPlaceholder"));
    m_sizeBtns[0]->setText(m_tool->getString("SizeLarge"));
    m_sizeBtns[1]->setText(m_tool->getString("SizeMedium"));
    m_sizeBtns[2]->setText(m_tool->getString("SizeSmall"));
}

void FlagBrowserWidget::setSidebarList(QTreeWidget* list) {
    if (m_tagList) {
        disconnect(m_tagList, &QTreeWidget::itemClicked, this, &FlagBrowserWidget::onTagSelected);
    }
    m_tagList = list;
    m_tagList->clear();
    m_tagList->setColumnCount(1);
    m_tagList->setHeaderHidden(true);
    connect(m_tagList, &QTreeWidget::itemClicked, this, &FlagBrowserWidget::onTagSelected);
}

void FlagBrowserWidget::refreshData() {
    m_tagMap.clear();
    m_flagPaths.clear();
    
    QList<QString> tagKeys = TagManager::instance().getTags().keys();
    QSet<QString> validTags(tagKeys.begin(), tagKeys.end());
    
    QMap<QString, FileDetails> effectiveFiles = FileManager::instance().getEffectiveFiles();
    
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
    QTreeWidgetItem* cosItem = new QTreeWidgetItem(m_tagList, {"COSMETIC"});
    m_tagList->addTopLevelItem(cosItem);
    
    for (auto it = m_tagMap.begin(); it != m_tagMap.end(); ++it) {
        if (it.key() == "COSMETIC") continue;
        QTreeWidgetItem* item = new QTreeWidgetItem(m_tagList, {it.key()});
        bool hasDefault = false;
        bool allComplete = true;
        for (const auto& v : it.value()) {
            if (v.name.toUpper() == it.key()) hasDefault = true;
            if (!v.isComplete()) allComplete = false;
        }
        if (!hasDefault) item->setForeground(0, Qt::red);
        else if (!allComplete) item->setForeground(0, QColor(255, 165, 0));
        
        QString key = it.key() + "_2";
        if (m_flagPaths.contains(key)) {
            QImage img = loadTga(m_flagPaths[key]);
            if (!img.isNull()) item->setIcon(0, QIcon(QPixmap::fromImage(img)));
        }
        m_tagList->addTopLevelItem(item);
    }
}

void FlagBrowserWidget::onTagSelected(QTreeWidgetItem* item, int) {
    m_selectedTag = item->text(0);
    m_placeholder->hide();
    updateFlagDisplay();
}

void FlagBrowserWidget::onSizeChanged(int id) {
    m_currentSizeIndex = id;
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
    
    m_browserBtn = new QPushButton("Manage");
    m_converterBtn = new QPushButton("New");
    m_browserBtn->setCheckable(true);
    m_converterBtn->setCheckable(true);
    connect(m_browserBtn, &QPushButton::clicked, [this](){ onModeChanged(0); });
    connect(m_converterBtn, &QPushButton::clicked, [this](){ onModeChanged(1); });
    
    tabLayout->addWidget(m_browserBtn);
    tabLayout->addWidget(m_converterBtn);
    tabLayout->addStretch();
    layout->addWidget(m_tabBar);
    
    m_stack = new QStackedWidget();
    m_browser = new FlagBrowserWidget(tool);
    m_converter = new FlagConverterWidget(tool);
    m_stack->addWidget(m_browser);
    m_stack->addWidget(m_converter);
    layout->addWidget(m_stack);
    
    m_stack->setCurrentIndex(0);
    m_browserBtn->setChecked(true);
    m_converterBtn->setChecked(false);
    applyTheme();
    updateButtonStyles(0);
}

void FlagManagerMainWidget::applyTheme() {
    bool isDark = ConfigManager::instance().getTheme() == ConfigManager::Theme::Dark;
    QString tabBarBg = isDark ? "#252526" : "#F0F0F0";
    m_tabBar->setStyleSheet(QString("QWidget#FlagTabBar { background: %1; }").arg(tabBarBg));
    m_browser->applyTheme();
}

void FlagManagerMainWidget::updateButtonStyles(int activeIndex) {
    QString active = "QPushButton { border: none; background: #007AFF; color: white; border-radius: 5px; padding: 5px 15px; font-weight: bold; }";
    QString inactive = "QPushButton { border: none; background: transparent; color: gray; border-radius: 5px; padding: 5px 15px; } QPushButton:hover { background: rgba(128,128,128,0.1); }";
    m_browserBtn->setStyleSheet(activeIndex == 0 ? active : inactive);
    m_converterBtn->setStyleSheet(activeIndex == 1 ? active : inactive);
}

void FlagManagerMainWidget::updateTexts() {
    m_browserBtn->setText(m_tool->getString("TabManage"));
    m_converterBtn->setText(m_tool->getString("TabNew"));
    m_browser->updateTexts();
    m_converter->updateTexts();
}

void FlagManagerMainWidget::onModeChanged(int index) {
    m_stack->setCurrentIndex(index);
    m_browserBtn->setChecked(index == 0);
    m_converterBtn->setChecked(index == 1);
    updateButtonStyles(index);
    m_tool->switchMode(index);
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
QIcon FlagManagerTool::icon() const { return QIcon::fromTheme("flag"); }
void FlagManagerTool::initialize() { loadLanguage("English"); }
QWidget* FlagManagerTool::createWidget(QWidget* parent) {
    m_mainWidget = new FlagManagerMainWidget(this, parent);
    return m_mainWidget;
}
QWidget* FlagManagerTool::createSidebarWidget(QWidget* parent) {
    m_listWidget = new FlagListWidget(this, parent);
    if (m_mainWidget) {
        m_mainWidget->getBrowser()->setSidebarList(m_listWidget->getList());
        m_mainWidget->getConverter()->setSidebarList(m_listWidget->getList());
        m_mainWidget->getBrowser()->refreshData();
    }
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
