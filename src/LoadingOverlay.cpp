#include "LoadingOverlay.h"
#include "ConfigManager.h"
#include <QPainter>
#include <QPainterPath>
#include <QApplication>
#include <QScreen>
#include <QDebug>

LoadingOverlay::LoadingOverlay(QWidget *parent)
    : QWidget(parent)
{
    setAttribute(Qt::WA_TransparentForMouseEvents, false);
    setAttribute(Qt::WA_TranslucentBackground);
    // Don't set FramelessWindowHint - it makes this a separate window instead of a child widget
    
    // Container for content
    m_container = new QWidget(this);
    m_container->setObjectName("LoadingContainer");
    m_container->setFixedSize(300, 150);
    
    QVBoxLayout *layout = new QVBoxLayout(m_container);
    layout->setContentsMargins(30, 25, 30, 25);
    layout->setSpacing(15);
    layout->setAlignment(Qt::AlignCenter);
    
    // Icon
    m_iconLabel = new QLabel(m_container);
    m_iconLabel->setPixmap(QIcon(":/app.ico").pixmap(48, 48));
    m_iconLabel->setAlignment(Qt::AlignCenter);
    layout->addWidget(m_iconLabel);
    
    // Message
    m_messageLabel = new QLabel(m_container);
    m_messageLabel->setObjectName("LoadingMessage");
    m_messageLabel->setAlignment(Qt::AlignCenter);
    m_messageLabel->setWordWrap(true);
    layout->addWidget(m_messageLabel);
    
    // Progress bar
    m_progressBar = new QProgressBar(m_container);
    m_progressBar->setObjectName("LoadingProgressBar");
    m_progressBar->setTextVisible(false);
    m_progressBar->setFixedHeight(6);
    m_progressBar->setRange(0, 0); // Indeterminate by default
    layout->addWidget(m_progressBar);
    
    // Apply theme-aware styling
    ConfigManager::Theme theme = ConfigManager::instance().getTheme();
    bool isDark = (theme == ConfigManager::Theme::Dark);
    
    QString containerBg = isDark ? "#2C2C2E" : "#FFFFFF";
    QString textColor = isDark ? "#FFFFFF" : "#1D1D1F";
    QString borderColor = isDark ? "#3A3A3C" : "#D2D2D7";
    QString progressBg = isDark ? "#3A3A3C" : "#E5E5EA";
    QString progressChunk = "#007AFF";
    
    m_container->setStyleSheet(QString(
        "QWidget#LoadingContainer {"
        "  background-color: %1;"
        "  border: 1px solid %2;"
        "  border-radius: 12px;"
        "}"
    ).arg(containerBg, borderColor));
    
    m_messageLabel->setStyleSheet(QString(
        "QLabel#LoadingMessage {"
        "  color: %1;"
        "  font-size: 14px;"
        "  font-weight: 500;"
        "}"
    ).arg(textColor));
    
    m_progressBar->setStyleSheet(QString(
        "QProgressBar#LoadingProgressBar {"
        "  background-color: %1;"
        "  border: none;"
        "  border-radius: 3px;"
        "}"
        "QProgressBar#LoadingProgressBar::chunk {"
        "  background-color: %2;"
        "  border-radius: 3px;"
        "}"
    ).arg(progressBg, progressChunk));
    
    hide();
    
    if (parent) {
        parent->installEventFilter(this);
    }
}

void LoadingOverlay::setMessage(const QString& message) {
    m_messageLabel->setText(message);
}

void LoadingOverlay::setProgress(int value) {
    if (value < 0) {
        m_progressBar->setRange(0, 0); // Indeterminate
    } else {
        m_progressBar->setRange(0, 100);
        m_progressBar->setValue(value);
    }
}

void LoadingOverlay::showOverlay() {
    if (parentWidget()) {
        raise();
        updatePosition();
    }
    show();
}

void LoadingOverlay::hideOverlay() {
    qDebug() << "LoadingOverlay::hideOverlay() called, isVisible:" << isVisible();
    hide();
    qDebug() << "LoadingOverlay::hideOverlay() after hide(), isVisible:" << isVisible();
}

void LoadingOverlay::paintEvent(QPaintEvent *event) {
    Q_UNUSED(event);
    
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    
    // Semi-transparent background with rounded corners only on the left side
    // Left side matches sidebar's rounded corners, right side is straight
    QPainterPath path;
    QRectF r = rect();
    qreal radius = 10;
    
    // Draw a shape with rounded corners only on the left side
    // Top-left corner (rounded)
    path.moveTo(r.left() + radius, r.top());
    // Top edge to top-right corner (straight)
    path.lineTo(r.right(), r.top());
    // Right edge (straight)
    path.lineTo(r.right(), r.bottom());
    // Bottom edge to bottom-left corner
    path.lineTo(r.left() + radius, r.bottom());
    // Bottom-left corner (rounded)
    path.arcTo(r.left(), r.bottom() - 2 * radius, 2 * radius, 2 * radius, 270, -90);
    // Left edge
    path.lineTo(r.left(), r.top() + radius);
    // Top-left corner (rounded)
    path.arcTo(r.left(), r.top(), 2 * radius, 2 * radius, 180, -90);
    path.closeSubpath();
    
    painter.fillPath(path, QColor(0, 0, 0, 120));
}

bool LoadingOverlay::eventFilter(QObject *obj, QEvent *event) {
    if (obj == parentWidget() && event->type() == QEvent::Resize) {
        updatePosition();
    }
    return QWidget::eventFilter(obj, event);
}

void LoadingOverlay::updatePosition() {
    if (parentWidget()) {
        setGeometry(parentWidget()->rect());
        m_container->move(
            (width() - m_container->width()) / 2,
            (height() - m_container->height()) / 2
        );
    }
}
