#include "CustomMessageBox.h"
#include "ConfigManager.h"
#include "LocalizationManager.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QStyle>
#include <QPainter>
#include <QPainterPath>

CustomMessageBox::CustomMessageBox(QWidget *parent, const QString &title, const QString &message, Type type)
    : QDialog(parent)
{
    setWindowFlags(Qt::FramelessWindowHint | Qt::Dialog);
    setAttribute(Qt::WA_TranslucentBackground);
    setWindowModality(Qt::WindowModal);
    setupUi(title, message, type);
    
    // Theme will be handled in paintEvent and setStyleSheet for children
    ConfigManager::Theme theme = ConfigManager::instance().getTheme();
    bool isDark = (theme == ConfigManager::Theme::Dark);
    QString text = isDark ? "#FFFFFF" : "#1D1D1F";
    
    setStyleSheet(QString(R"(
        QLabel { color: %1; }
        QPushButton {
            background-color: #007AFF; color: white; border: none; border-radius: 6px; padding: 8px 16px; font-weight: bold;
        }
        QPushButton:hover { background-color: #0062CC; }
        QPushButton#CancelBtn {
            background-color: %2; color: %1; border: 1px solid %3;
        }
        QPushButton#CancelBtn:hover { background-color: %4; }
    )").arg(text, isDark ? "#3A3A3C" : "#F5F5F7", isDark ? "#48484A" : "#D2D2D7", isDark ? "#48484A" : "#E5E5EA"));
}

void CustomMessageBox::paintEvent(QPaintEvent *event) {
    Q_UNUSED(event);
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    ConfigManager::Theme theme = ConfigManager::instance().getTheme();
    bool isDark = (theme == ConfigManager::Theme::Dark);
    QColor bg = isDark ? QColor("#2C2C2E") : QColor("#FFFFFF");
    QColor border = isDark ? QColor("#3A3A3C") : QColor("#D2D2D7");

    QPainterPath path;
    path.addRoundedRect(rect(), 10, 10);
    
    painter.fillPath(path, bg);
    painter.setPen(QPen(border, 1));
    painter.drawPath(path);
}

void CustomMessageBox::setupUi(const QString &title, const QString &message, Type type) {
    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setContentsMargins(20, 20, 20, 20);
    layout->setSpacing(20);

    QLabel *titleLabel = new QLabel(title);
    titleLabel->setStyleSheet("font-size: 16px; font-weight: bold;");
    layout->addWidget(titleLabel);

    QLabel *msgLabel = new QLabel(message);
    msgLabel->setWordWrap(true);
    msgLabel->setStyleSheet("font-size: 14px;");
    layout->addWidget(msgLabel);

    QHBoxLayout *btnLayout = new QHBoxLayout();
    btnLayout->addStretch();

    LocalizationManager& loc = LocalizationManager::instance();

    if (type == Question) {
        QPushButton *cancelBtn = new QPushButton(loc.getString("Common", "Cancel"));
        cancelBtn->setObjectName("CancelBtn");
        connect(cancelBtn, &QPushButton::clicked, [this](){ 
            m_result = QMessageBox::No; 
            reject(); 
        });
        btnLayout->addWidget(cancelBtn);

        QPushButton *yesBtn = new QPushButton(loc.getString("Common", "Yes"));
        connect(yesBtn, &QPushButton::clicked, [this](){ 
            m_result = QMessageBox::Yes; 
            accept(); 
        });
        btnLayout->addWidget(yesBtn);
    } else {
        QPushButton *okBtn = new QPushButton(loc.getString("Common", "OK"));
        connect(okBtn, &QPushButton::clicked, [this](){ 
            m_result = QMessageBox::Ok; 
            accept(); 
        });
        btnLayout->addWidget(okBtn);
    }
    
    layout->addLayout(btnLayout);
}

void CustomMessageBox::information(QWidget *parent, const QString &title, const QString &message) {
    CustomMessageBox box(parent, title, message, Information);
    box.adjustSize();
    // Center on parent or screen
    if (parent) {
        QPoint parentCenter = parent->mapToGlobal(parent->rect().center());
        box.move(parentCenter.x() - box.width() / 2, parentCenter.y() - box.height() / 2);
    }
    // Ensure dialog is on top
    box.raise();
    box.activateWindow();
    box.exec();
}

QMessageBox::StandardButton CustomMessageBox::question(QWidget *parent, const QString &title, const QString &message) {
    CustomMessageBox box(parent, title, message, Question);
    box.adjustSize();
    // Center on parent or screen
    if (parent) {
        QPoint parentCenter = parent->mapToGlobal(parent->rect().center());
        box.move(parentCenter.x() - box.width() / 2, parentCenter.y() - box.height() / 2);
    }
    // Ensure dialog is on top
    box.raise();
    box.activateWindow();
    box.exec();
    return box.m_result;
}
