#include "FlagManagerTool.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QDebug>
#include <QDir>
#include <QCoreApplication>
#include <QFile>
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
#include <QLibrary>
#include <vector>
#include "../../src/FileManager.h"
#include "../../src/ConfigManager.h"
#include "../../src/Logger.h"
#include "../../src/ToolManager.h"
#include "../../src/CustomMessageBox.h"
#include "../../src/ToolDescriptorParser.h"
#include "../../src/ToolRuntimeContext.h"
#include <QMenu>

#ifdef Q_OS_WIN
#include <windows.h>
#include <wincodec.h>
#pragma comment(lib, "windowscodecs.lib")
#pragma comment(lib, "ole32.lib")
#endif

namespace {
using APEHOI4ParserGetCountryTagEntryCountFn = uint32_t (*)(uint32_t);
using APEHOI4ParserCopyCountryTagEntriesFn = uint32_t (*)(void*, uint32_t, uint32_t);

struct APEHOI4ParserSourceRangeBridge {
    uint32_t startOffset;
    uint32_t endOffset;
    uint32_t startLine;
    uint32_t startColumn;
    uint32_t endLine;
    uint32_t endColumn;
};

struct APEHOI4ParserTagEntryBridge {
    const char* tagUtf8;
    const char* targetPathUtf8;
    uint32_t isDynamic;
    APEHOI4ParserSourceRangeBridge range;
};

#ifdef Q_OS_WIN
using DirectXTexLoadDDSFn = int (*)(const unsigned char*, int, unsigned char**, int*, int*, int*);
using DirectXTexFreeImageFn = void (*)(unsigned char*);
using DirectXTexGetLastErrorFn = const char* (*)();
#endif

QString findPluginBinaryPath(const QString& pluginName) {
    QString libraryPath;
    QString errorMessage;
    if (!ToolRuntimeContext::instance().requestAuthorizedPluginBinaryPath(pluginName, &libraryPath, &errorMessage)) {
        Logger::instance().logWarning(
            "FlagManagerTool",
            QString("Failed to resolve authorized plugin path for %1: %2").arg(pluginName, errorMessage)
        );
        return {};
    }

    return libraryPath;
}

QMap<QString, QString> loadTagsFromPluginRuntime() {
    QMap<QString, QString> tags;

    const QString libraryPath = findPluginBinaryPath("APEHOI4Parser");
    if (libraryPath.isEmpty()) {
        Logger::instance().logWarning("FlagManagerTool", "APEHOI4Parser plugin binary not found.");
        return tags;
    }

    QLibrary library(libraryPath);
    if (!library.load()) {
        Logger::instance().logWarning("FlagManagerTool", "Failed to load APEHOI4Parser plugin: " + library.errorString());
        return tags;
    }

    auto getEntryCount = reinterpret_cast<APEHOI4ParserGetCountryTagEntryCountFn>(
        library.resolve("APE_HOI4Parser_GetCountryTagEntryCount")
    );
    auto copyEntries = reinterpret_cast<APEHOI4ParserCopyCountryTagEntriesFn>(
        library.resolve("APE_HOI4Parser_CopyCountryTagEntries")
    );

    if (!getEntryCount || !copyEntries) {
        Logger::instance().logWarning(
            "FlagManagerTool",
            "Failed to resolve APE_HOI4Parser country tag ABI exports."
        );
        return tags;
    }

    const uint32_t queryFlags = 0;
    const uint32_t entryCount = getEntryCount(queryFlags);
    if (entryCount == 0) {
        return tags;
    }

    std::vector<APEHOI4ParserTagEntryBridge> entries(static_cast<size_t>(entryCount));
    const uint32_t copiedCount = copyEntries(entries.data(), entryCount, queryFlags);

    for (uint32_t i = 0; i < copiedCount; ++i) {
        const APEHOI4ParserTagEntryBridge& entry = entries[static_cast<size_t>(i)];
        if (entry.tagUtf8 == nullptr || entry.targetPathUtf8 == nullptr) {
            continue;
        }
        if (entry.isDynamic != 0) {
            continue;
        }

        tags.insert(
            QString::fromUtf8(entry.tagUtf8),
            QString::fromUtf8(entry.targetPathUtf8)
        );
    }

    return tags;
}

static QImage loadTgaFromData(const QByteArray& data) {
    if (data.size() < 18) {
        return QImage();
    }

    const uchar* ptr = reinterpret_cast<const uchar*>(data.constData());

    const int idLength = ptr[0];
    const int colorMapType = ptr[1];
    const int imageType = ptr[2];
    const int width = ptr[12] | (ptr[13] << 8);
    const int height = ptr[14] | (ptr[15] << 8);
    const int bpp = ptr[16];
    const int descriptor = ptr[17];

    if (colorMapType != 0 || (imageType != 2 && imageType != 10)) {
        return QImage::fromData(data);
    }

    if (bpp != 24 && bpp != 32 || width <= 0 || height <= 0) {
        return QImage();
    }

    const int bytesPerPixel = bpp / 8;
    const int pixelDataOffset = 18 + idLength;

    if (data.size() < pixelDataOffset) {
        return QImage();
    }

    QImage image(width, height, bpp == 32 ? QImage::Format_ARGB32 : QImage::Format_RGB32);

    const uchar* pixelData = ptr + pixelDataOffset;
    const int pixelCount = width * height;

    if (imageType == 2) {
        for (int y = 0; y < height; ++y) {
            const int destY = (descriptor & 0x20) ? y : (height - 1 - y);
            for (int x = 0; x < width; ++x) {
                const int srcIdx = (y * width + x) * bytesPerPixel;
                if (pixelDataOffset + srcIdx + bytesPerPixel > data.size()) {
                    break;
                }

                const uchar b = pixelData[srcIdx];
                const uchar g = pixelData[srcIdx + 1];
                const uchar r = pixelData[srcIdx + 2];
                const uchar a = (bytesPerPixel == 4) ? pixelData[srcIdx + 3] : 255;

                image.setPixel(x, destY, qRgba(r, g, b, a));
            }
        }
    } else {
        int currentPixel = 0;
        int dataIdx = 0;
        const int maxDataSize = data.size() - pixelDataOffset;

        while (currentPixel < pixelCount && dataIdx < maxDataSize) {
            const uchar header = pixelData[dataIdx++];
            const int count = (header & 0x7F) + 1;

            if (header & 0x80) {
                if (dataIdx + bytesPerPixel > maxDataSize) {
                    break;
                }

                const uchar b = pixelData[dataIdx];
                const uchar g = pixelData[dataIdx + 1];
                const uchar r = pixelData[dataIdx + 2];
                const uchar a = (bytesPerPixel == 4) ? pixelData[dataIdx + 3] : 255;
                dataIdx += bytesPerPixel;

                for (int i = 0; i < count && currentPixel < pixelCount; ++i, ++currentPixel) {
                    const int x = currentPixel % width;
                    const int y = currentPixel / width;
                    const int destY = (descriptor & 0x20) ? y : (height - 1 - y);
                    image.setPixel(x, destY, qRgba(r, g, b, a));
                }
            } else {
                for (int i = 0; i < count && currentPixel < pixelCount; ++i, ++currentPixel) {
                    if (dataIdx + bytesPerPixel > maxDataSize) {
                        break;
                    }

                    const uchar b = pixelData[dataIdx];
                    const uchar g = pixelData[dataIdx + 1];
                    const uchar r = pixelData[dataIdx + 2];
                    const uchar a = (bytesPerPixel == 4) ? pixelData[dataIdx + 3] : 255;
                    dataIdx += bytesPerPixel;

                    const int x = currentPixel % width;
                    const int y = currentPixel / width;
                    const int destY = (descriptor & 0x20) ? y : (height - 1 - y);
                    image.setPixel(x, destY, qRgba(r, g, b, a));
                }
            }
        }
    }

    return image;
}

static QByteArray saveTga32ToBytes(const QImage& img) {
    QByteArray output;
    QBuffer buffer(&output);
    if (!buffer.open(QIODevice::WriteOnly)) {
        return {};
    }

    const QImage image = img.convertToFormat(QImage::Format_ARGB32);
    const int width = image.width();
    const int height = image.height();

    uchar header[18] = {0};
    header[2] = 2;
    header[12] = width & 0xFF;
    header[13] = (width >> 8) & 0xFF;
    header[14] = height & 0xFF;
    header[15] = (height >> 8) & 0xFF;
    header[16] = 32;
    header[17] = 0;

    buffer.write(reinterpret_cast<const char*>(header), 18);

    for (int y = height - 1; y >= 0; --y) {
        const QRgb* line = reinterpret_cast<const QRgb*>(image.constScanLine(y));
        for (int x = 0; x < width; ++x) {
            const QRgb pixel = line[x];
            const uchar bgra[4] = {
                static_cast<uchar>(qBlue(pixel)),
                static_cast<uchar>(qGreen(pixel)),
                static_cast<uchar>(qRed(pixel)),
                static_cast<uchar>(qAlpha(pixel))
            };
            buffer.write(reinterpret_cast<const char*>(bgra), 4);
        }
    }

    return output;
}

#ifdef Q_OS_WIN
static QImage loadDdsWithDirectXTex(const QByteArray& content) {
    QImage result;

    if (content.isEmpty()) {
        Logger::instance().logWarning("FlagManagerTool", "DDS content is empty.");
        return result;
    }

    const QString libraryPath = findPluginBinaryPath("DirectXTex");
    if (libraryPath.isEmpty()) {
        Logger::instance().logWarning("FlagManagerTool", "DirectXTex plugin binary not found.");
        return result;
    }

    QLibrary library(libraryPath);
    if (!library.load()) {
        Logger::instance().logWarning("FlagManagerTool", "Failed to load DirectXTex plugin: " + library.errorString());
        return result;
    }

    auto loadDdsImage = reinterpret_cast<DirectXTexLoadDDSFn>(library.resolve("APE_DirectXTex_LoadDDSImage"));
    auto freeImage = reinterpret_cast<DirectXTexFreeImageFn>(library.resolve("APE_DirectXTex_FreeImage"));
    auto getLastError = reinterpret_cast<DirectXTexGetLastErrorFn>(library.resolve("APE_DirectXTex_GetLastError"));

    if (!loadDdsImage || !freeImage) {
        Logger::instance().logWarning("FlagManagerTool", "DirectXTex plugin exports are incomplete.");
        return result;
    }

    unsigned char* bytes = nullptr;
    int width = 0;
    int height = 0;
    int stride = 0;

    const int success = loadDdsImage(
        reinterpret_cast<const unsigned char*>(content.constData()),
        content.size(),
        &bytes,
        &width,
        &height,
        &stride
    );
    if (!success || !bytes || width <= 0 || height <= 0 || stride <= 0) {
        const QString errorMessage = getLastError ? QString::fromUtf8(getLastError()) : QString("Unknown DirectXTex plugin error.");
        Logger::instance().logWarning("FlagManagerTool", "DirectXTex plugin failed: " + errorMessage);
        return result;
    }

    QImage image(bytes, width, height, stride, QImage::Format_ARGB32);
    result = image.copy();
    freeImage(bytes);
    return result;
}
#endif
}

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
            border-bottom-right-radius: 10px;
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
    m_fileList->clear();
    m_fileList->setColumnCount(2);
    m_fileList->setHeaderHidden(false);
    m_fileList->setContextMenuPolicy(Qt::CustomContextMenu);
    m_fileList->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_fileList->setSelectionBehavior(QAbstractItemView::SelectRows);

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
    if (exportItem(m_items[m_currentPath], QString())) {
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
        if (exportItem(it.value(), QString())) {
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

// 使用 Windows WIC 加载图片（支持 WebP, JXR 等格式）
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
    const QString ext = QFileInfo(path).suffix().toLower();

#ifdef Q_OS_WIN
    if (ext == "dds") {
        QFile file(path);
        if (!file.open(QIODevice::ReadOnly)) {
            return QImage();
        }

        const QImage img = loadDdsWithDirectXTex(file.readAll());
        if (!img.isNull()) return img;
    }

    if (ext == "webp" || ext == "jxr" || ext == "wdp" || ext == "hdp") {
        QImage img = loadImageWithWIC(path);
        if (!img.isNull()) return img;
    }
#endif

    if (ext == "tga") {
        QFile file(path);
        if (!file.open(QIODevice::ReadOnly)) return QImage();
        return loadTgaFromData(file.readAll());
    }

    QImageReader reader(path);
    if (reader.canRead()) {
        return reader.read();
    }

    return QImage(path);
}

bool FlagConverterWidget::exportItem(const FlagItem& item, const QString& baseDir) {
    Q_UNUSED(baseDir);

    if (item.name.isEmpty()) return false;

    struct Size { int w; int h; QString suffix; };
    Size sizes[] = {{82, 52, ""}, {41, 26, "medium/"}, {10, 7, "small/"}};

    QStringList existingFiles;
    for (const auto& s : sizes) {
        const QString relativePath = "gfx/flags/" + s.suffix + item.name + ".tga";
        const ToolRuntimeContext::FileReadResult existingResult =
            ToolRuntimeContext::instance().readFile(ToolRuntimeContext::FileRoot::Mod, relativePath);
        if (existingResult.success) {
            existingFiles.append(s.suffix + item.name + ".tga");
        }
    }

    if (!existingFiles.isEmpty()) {
        const QString fileList = existingFiles.join("\n");
        const QString message = m_tool->getString("ConfirmOverwrite").arg(fileList);

        const QMessageBox::StandardButton result = CustomMessageBox::question(
            const_cast<FlagConverterWidget*>(this),
            m_tool->getString("ConfirmOverwriteTitle"),
            message
        );

        if (result != QMessageBox::Yes) {
            return false;
        }
    }

    const ToolRuntimeContext::FileWriteResult ensureRootResult =
        ToolRuntimeContext::instance().ensureDirectory(ToolRuntimeContext::FileRoot::Mod, "gfx/flags");
    if (!ensureRootResult.success) {
        Logger::instance().logWarning("FlagManagerTool", "Failed to ensure flags directory: " + ensureRootResult.errorMessage);
        return false;
    }

    const QImage cropped = item.image.copy(item.crop);
    for (const auto& s : sizes) {
        const QImage resized = cropped.scaled(s.w, s.h, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
        const QString relativeDir = "gfx/flags/" + s.suffix;
        const QString relativePath = relativeDir + item.name + ".tga";

        const ToolRuntimeContext::FileWriteResult ensureDirResult =
            ToolRuntimeContext::instance().ensureDirectory(ToolRuntimeContext::FileRoot::Mod, relativeDir);
        if (!ensureDirResult.success) {
            Logger::instance().logWarning("FlagManagerTool", "Failed to ensure export directory: " + ensureDirResult.errorMessage);
            return false;
        }

        const QByteArray encoded = saveTga32ToBytes(resized);
        if (encoded.isEmpty()) {
            Logger::instance().logWarning("FlagManagerTool", "Failed to encode TGA image for export.");
            return false;
        }

        const ToolRuntimeContext::FileWriteResult writeResult =
            ToolRuntimeContext::instance().writeFile(ToolRuntimeContext::FileRoot::Mod, relativePath, encoded);
        if (!writeResult.success) {
            Logger::instance().logWarning("FlagManagerTool", "Failed to export flag: " + writeResult.errorMessage);
            return false;
        }
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
    m_scrollArea->setStyleSheet("QScrollArea { background: transparent; border: none; border-bottom-right-radius: 10px; } QScrollArea > QWidget > QWidget { background: transparent; }");
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
    m_tagList->setContextMenuPolicy(Qt::NoContextMenu);
    m_tagList->setSelectionMode(QAbstractItemView::SingleSelection);
    m_tagList->setSelectionBehavior(QAbstractItemView::SelectRows);
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
    
    Logger::instance().logInfo("FlagBrowserWidget", "Getting tags from APEHOI4Parser plugin...");
    QMap<QString, QString> allTags = loadTagsFromPluginRuntime();
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
        m_flagPaths[key] = {ToolRuntimeContext::FileRoot::Unknown, relPath};
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

QImage FlagBrowserWidget::loadTga(const FlagFileRef& fileRef) {
    if (fileRef.relativePath.isEmpty()) {
        return QImage();
    }

    const ToolRuntimeContext::FileReadResult readResult =
        ToolRuntimeContext::instance().readEffectiveFile(fileRef.relativePath);
    if (!readResult.success) {
        Logger::instance().logWarning("FlagBrowserWidget", "Failed to read effective flag file: " + readResult.errorMessage);
        return QImage();
    }

    return loadTgaFromData(readResult.content);
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
        const FlagFileRef fileRef = m_flagPaths.value(key);

        QImage flag;
        if (!fileRef.relativePath.isEmpty()) {
            flag = loadTga(fileRef);
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
    setObjectName("FlagManagerMainWidget");
    setAttribute(Qt::WA_StyledBackground, true);
    
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
    
    actionLayout->addWidget(m_importBtn);
    actionLayout->addWidget(m_exportBtn);
    actionLayout->addWidget(m_exportAllBtn);
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
    
    // Keep selection state synced for host-managed select-all action
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
    QString mainBg = isDark ? "#1E1E1E" : "#FFFFFF";
    
    // Main widget with rounded corners
    setStyleSheet(QString("QWidget#FlagManagerMainWidget { background-color: %1; border-top-right-radius: 10px; border-bottom-right-radius: 10px; }").arg(mainBg));
    
    m_tabBar->setStyleSheet(QString("QWidget#FlagTabBar { background: %1; border-bottom: 1px solid %2; padding-bottom: 0px; margin-bottom: 0px; border-top-right-radius: 10px; }").arg(tabBarBg, borderColor));
    
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
    Q_UNUSED(hasSelection);
}

// --- FlagListWidget ---

FlagListWidget::FlagListWidget(FlagManagerTool* tool, QWidget* parent)
    : QWidget(parent), m_tool(tool) {
    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    m_header = new QLabel("TAGs", this);
    m_header->hide();
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
            background-color: %1; border: none; color: %2; border-bottom-right-radius: 0px;
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

QString FlagListWidget::currentTitle() const {
    return m_currentMode == 0 ? m_tool->getString("Tags") : m_tool->getString("Files");
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
    m_dependencies = ToolDescriptorParser::extractDependencies(metaData);
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
void FlagManagerTool::initialize() { loadLanguage("en_US"); }

QWidget* FlagManagerTool::createWidget(QWidget* parent) {
    m_mainWidget = new FlagManagerMainWidget(this, parent);
    m_currentMode = 0;
    m_searchModeActive = false;
    return m_mainWidget;
}

QWidget* FlagManagerTool::createSidebarWidget(QWidget* parent) {
    Logger::instance().logInfo("FlagManagerTool", "createSidebarWidget() called");
    m_listWidget = new FlagListWidget(this, parent);
    m_listWidget->setMode(m_currentMode);
    Logger::instance().logInfo("FlagManagerTool", "FlagListWidget created");
    if (m_mainWidget) {
        Logger::instance().logInfo("FlagManagerTool", "m_mainWidget exists, calling setSidebarList...");
        m_mainWidget->getBrowser()->setSidebarList(m_listWidget->getList());
        Logger::instance().logInfo("FlagManagerTool", "setSidebarList completed, calling refreshData...");
        m_mainWidget->getBrowser()->refreshData();
        if (m_currentMode == 1) {
            m_mainWidget->getConverter()->setSidebarList(m_listWidget->getList());
        }
        Logger::instance().logInfo("FlagManagerTool", "refreshData completed");
    } else {
        Logger::instance().logWarning("FlagManagerTool", "m_mainWidget is null!");
    }
    Logger::instance().logInfo("FlagManagerTool", "createSidebarWidget() returning");
    emit rightSidebarStateChanged();
    return m_listWidget;
}

QList<ToolRightSidebarButtonDefinition> FlagManagerTool::rightSidebarButtons() const {
    return {};
}

ToolRightSidebarState FlagManagerTool::rightSidebarState() const {
    ToolRightSidebarState state;
    state.title = m_listWidget ? m_listWidget->currentTitle() : (m_currentMode == 0 ? getString("Tags") : getString("Files"));
    state.orderedButtonKeys = {QString::fromUtf8("__default__"), QString::fromUtf8("__search__")};
    state.activeButtonKey = m_searchModeActive ? QString::fromUtf8("__search__") : QString::fromUtf8("__default__");
    state.listVisible = true;
    state.searchModeAvailable = true;
    state.searchModeActive = m_searchModeActive;

    if (m_currentMode == 1) {
        state.searchableColumns = {0, 1};
        state.searchableColumnLabels = {getString("ColFlagName"), getString("ColFileName")};
        state.showSelectAllButton = true;
    } else {
        state.searchableColumns = {0};
        state.searchableColumnLabels = {getString("Tags")};
        state.showSelectAllButton = false;
    }

    return state;
}

QTreeWidget* FlagManagerTool::rightSidebarListWidget() const {
    return m_listWidget ? m_listWidget->getList() : nullptr;
}

void FlagManagerTool::handleRightSidebarButton(const QString& key) {
    if (key == QString::fromUtf8("__search__")) {
        m_searchModeActive = !m_searchModeActive;
    } else {
        m_searchModeActive = false;
    }
    emit rightSidebarStateChanged();
}

void FlagManagerTool::loadLanguage(const QString& lang) {
    const QString normalizedLang = normalizeLanguageCode(lang);
    const QString rootPath = localisationRootPath();

    m_currentLang = normalizedLang;
    m_localizedStrings.clear();

    const QMap<QString, QString> englishStrings = parseSimpleYamlFile(rootPath + "/en_US/strings.yml");
    for (auto it = englishStrings.constBegin(); it != englishStrings.constEnd(); ++it) {
        m_localizedStrings.insert(it.key(), it.value());
    }

    if (normalizedLang != "en_US") {
        const QMap<QString, QString> selectedStrings =
            parseSimpleYamlFile(rootPath + "/" + normalizedLang + "/strings.yml");
        for (auto it = selectedStrings.constBegin(); it != selectedStrings.constEnd(); ++it) {
            m_localizedStrings.insert(it.key(), it.value());
        }
    }

    m_localizedNames["en_US"] = englishStrings.value("Name", "Flag Manager");
    m_localizedDescs["en_US"] = englishStrings.value("Description", "Flag Manager");

    if (normalizedLang != "en_US") {
        m_localizedNames[normalizedLang] = m_localizedStrings.value("Name", m_localizedNames.value("en_US"));
        m_localizedDescs[normalizedLang] = m_localizedStrings.value("Description", m_localizedDescs.value("en_US"));
    }

    if (m_mainWidget) {
        m_mainWidget->updateTexts();
    }
    if (m_listWidget) {
        m_listWidget->updateTexts();
    }
}
QString FlagManagerTool::getString(const QString& key) const { return m_localizedStrings.value(key, key); }

void FlagManagerTool::applyTheme() {
    if (m_mainWidget) m_mainWidget->applyTheme();
    if (m_listWidget) m_listWidget->applyTheme();
}

void FlagManagerTool::switchMode(int mode) {
    m_currentMode = mode;
    m_searchModeActive = false;

    if (m_listWidget) {
        m_listWidget->setMode(mode);
        if (mode == 0 && m_mainWidget) {
            m_mainWidget->getBrowser()->setSidebarList(m_listWidget->getList());
            m_mainWidget->getBrowser()->refreshData();
        } else if (mode == 1 && m_mainWidget) {
            m_mainWidget->getConverter()->setSidebarList(m_listWidget->getList());
        }
    }

    emit rightSidebarStateChanged();
}

QString FlagManagerTool::normalizeLanguageCode(const QString& lang) const {
    const QString normalized = lang.trimmed();
    const QString rootPath = localisationRootPath();
    QDir rootDir(rootPath);
    
    if (!rootDir.exists()) {
        return "en_US";
    }

    const QStringList languageDirectories = rootDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
    
    // Exact match
    if (languageDirectories.contains(normalized)) {
        return normalized;
    }
    
    // Scan meta.htsl for text match
    for (const QString& dirName : languageDirectories) {
        const QString metaPath = rootDir.filePath(dirName + "/meta.htsl");
        QFile file(metaPath);
        if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            QTextStream stream(&file);
            while (!stream.atEnd()) {
                QString line = stream.readLine().trimmed();
                if (line.startsWith("text=") || line.startsWith("text =")) {
                    QString text = line.mid(line.indexOf('=') + 1).trimmed();
                    if (text.startsWith('"') && text.endsWith('"')) {
                        text = text.mid(1, text.length() - 2);
                    }
                    if (text == normalized) {
                        return dirName;
                    }
                }
            }
        }
    }

    return "en_US";
}

QString FlagManagerTool::localisationRootPath() const {
    QDir appDir(QCoreApplication::applicationDirPath());

    if (QDir(appDir.filePath("tools/FlagManagerTool/localisation")).exists()) {
        return appDir.filePath("tools/FlagManagerTool/localisation");
    }
    if (QDir(appDir.filePath("../tools/FlagManagerTool/localisation")).exists()) {
        return appDir.filePath("../tools/FlagManagerTool/localisation");
    }
    return appDir.filePath("tools/FlagManagerTool/localisation");
}

QMap<QString, QString> FlagManagerTool::parseSimpleYamlFile(const QString& filePath) const {
    QMap<QString, QString> parsed;
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return parsed;
    }

    QTextStream stream(&file);
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    stream.setEncoding(QStringConverter::Utf8);
#endif

    bool inLanguageBlock = false;
    while (!stream.atEnd()) {
        QString line = stream.readLine();
        if (line.trimmed().isEmpty()) {
            continue;
        }

        if (!line.startsWith(' ') && !line.startsWith('\t')) {
            if (line.trimmed().startsWith("l_") && line.trimmed().endsWith(':')) {
                inLanguageBlock = true;
            }
            continue;
        }

        if (!inLanguageBlock) {
            continue;
        }

        const QString trimmed = line.trimmed();
        if (trimmed.startsWith('#')) {
            continue;
        }

        const int colonIndex = trimmed.indexOf(':');
        if (colonIndex <= 0) {
            continue;
        }

        const QString key = trimmed.left(colonIndex).trimmed();
        QString value = trimmed.mid(colonIndex + 1).trimmed();

        if ((value.startsWith('"') && value.endsWith('"')) || (value.startsWith('\'') && value.endsWith('\''))) {
            value = value.mid(1, value.length() - 2);
        }

        value.replace("\\n", "\n");
        parsed.insert(key, value);
    }

    return parsed;
}