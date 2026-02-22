#include "ToolsPage.h"
#include "LocalizationManager.h"
#include "ToolManager.h"
#include "ConfigManager.h"
#include "Logger.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QScrollArea>
#include <QFrame>
#include <QDebug>
#include <QToolTip>

ToolsPage::ToolsPage(QWidget *parent) : QWidget(parent) {
    setupUi();
    // Connect to ToolManager to refresh when tools are loaded
    connect(&ToolManager::instance(), &ToolManager::toolsLoaded, this, &ToolsPage::refreshTools);
    
    // Connect to ConfigManager for theme changes
    connect(&ConfigManager::instance(), &ConfigManager::themeChanged, this, [this](ConfigManager::Theme){ updateTheme(); });

    // Initial load (if already loaded)
    refreshTools();
    updateTexts();
    updateTheme();
}

void ToolsPage::setupUi() {
    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    // Header
    QWidget *header = new QWidget();
    header->setObjectName("OverlayHeader");
    header->setFixedHeight(60);
    QHBoxLayout *headerLayout = new QHBoxLayout(header);
    headerLayout->setContentsMargins(20, 0, 20, 0);

    m_titleLabel = new QLabel("Tools");
    m_titleLabel->setObjectName("ToolsTitle");
    m_titleLabel->setStyleSheet("font-size: 18px; font-weight: bold;");
    
    m_closeBtn = new QPushButton("×");
    m_closeBtn->setFixedSize(30, 30);
    m_closeBtn->setCursor(Qt::PointingHandCursor);
    m_closeBtn->setStyleSheet("border: none; font-size: 20px; color: #888;");
    connect(m_closeBtn, &QPushButton::clicked, this, &ToolsPage::closeClicked);

    headerLayout->addWidget(m_titleLabel);
    headerLayout->addStretch();
    headerLayout->addWidget(m_closeBtn);
    layout->addWidget(header);

    // Content
    QScrollArea *scroll = new QScrollArea();
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    
    QWidget *content = new QWidget();
    content->setObjectName("ToolsContent");
    QVBoxLayout *contentLayout = new QVBoxLayout(content);
    contentLayout->setContentsMargins(40, 20, 40, 40);
    
    // Grid for tools
    m_gridLayout = new QGridLayout();
    m_gridLayout->setSpacing(20);

    contentLayout->addLayout(m_gridLayout);
    contentLayout->addStretch();

    scroll->setWidget(content);
    layout->addWidget(scroll);
}

void ToolsPage::refreshTools() {
    Logger::instance().logInfo("ToolsPage", "Refreshing tools...");
    
    // Clear existing items
    QLayoutItem *child;
    while ((child = m_gridLayout->takeAt(0)) != nullptr) {
        delete child->widget();
        delete child;
    }
    m_toolCards.clear();

    QList<ToolInterface*> tools = ToolManager::instance().getTools();
    Logger::instance().logInfo("ToolsPage", QString("Found %1 tools.").arg(tools.size()));
    
    int row = 0;
    int col = 0;
    int maxCols = 4; // Adjust based on width?

    for (ToolInterface* tool : tools) {
        Logger::instance().logInfo("ToolsPage", "Adding card for tool: " + tool->name());
        QWidget *card = createToolCard(tool);
        m_gridLayout->addWidget(card, row, col);
        
        col++;
        if (col >= maxCols) {
            col = 0;
            row++;
        }
    }
    
    // Update texts immediately to ensure correct language
    updateTexts();
    updateTheme();
}

QWidget* ToolsPage::createToolCard(ToolInterface* tool) {
    QPushButton *card = new QPushButton();
    card->setObjectName("ToolCard");
    card->setCursor(Qt::PointingHandCursor);
    card->setFixedSize(200, 300); // Portrait aspect ratio like game covers
    
    QString toolId = tool->id();
    connect(card, &QPushButton::clicked, [this, toolId](){ emit toolSelected(toolId); });

    QVBoxLayout *layout = new QVBoxLayout(card);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    // Cover Area (Container for Image + Warning)
    QWidget *coverContainer = new QWidget();
    coverContainer->setFixedHeight(250);
    QGridLayout *coverLayout = new QGridLayout(coverContainer);
    coverLayout->setContentsMargins(0, 0, 0, 0);
    coverLayout->setSpacing(0);

    // Cover Image
    QLabel *iconLbl = new QLabel();
    QIcon icon = tool->icon();
    if (!icon.isNull()) {
        iconLbl->setPixmap(icon.pixmap(200, 250)); // Assuming cover art size
        iconLbl->setScaledContents(true);
    } else {
        // Placeholder
        iconLbl->setText("No Image");
        iconLbl->setAlignment(Qt::AlignCenter);
        iconLbl->setStyleSheet("background-color: #333; color: #888;");
    }
    iconLbl->setStyleSheet("border-top-left-radius: 10px; border-top-right-radius: 10px;");
    
    // Add image to layout (Layer 0)
    coverLayout->addWidget(iconLbl, 0, 0);

    // Version Check
    QString appVersion = APP_VERSION;
    QString requiredVersion = tool->compatibleVersion();
    bool versionMismatch = (appVersion != requiredVersion);

    if (versionMismatch) {
        QLabel *warningLbl = new QLabel("!");
        warningLbl->setFixedSize(24, 24);
        warningLbl->setStyleSheet("background-color: #FF3B30; color: white; border-radius: 12px; font-weight: bold; qproperty-alignment: AlignCenter; margin: 5px;");
        
        // Tooltip will be set in updateTexts
        
        // Add warning to layout (Layer 1), top-right
        coverLayout->addWidget(warningLbl, 0, 0, Qt::AlignTop | Qt::AlignRight);
    }

    // Title Area
    QWidget *titleArea = new QWidget();
    titleArea->setObjectName("CardTitleArea");
    titleArea->setFixedHeight(50);
    QVBoxLayout *titleLayout = new QVBoxLayout(titleArea);
    titleLayout->setContentsMargins(10, 0, 10, 0);
    titleLayout->setAlignment(Qt::AlignCenter);

    QLabel *titleLbl = new QLabel(tool->name());
    titleLbl->setObjectName("CardTitle");
    titleLbl->setAlignment(Qt::AlignCenter);
    titleLbl->setWordWrap(true);
    
    // Description (Hidden in card, maybe tooltip?)
    QLabel *descLbl = new QLabel(tool->description());
    descLbl->setVisible(false); 

    titleLayout->addWidget(titleLbl);

    layout->addWidget(coverContainer);
    layout->addWidget(titleArea);

    // Store for localization updates
    m_toolCards.append({toolId, titleLbl, descLbl, tool, card});
    
    return card;
}

void ToolsPage::updateTexts() {
    LocalizationManager& loc = LocalizationManager::instance();
    m_titleLabel->setText(loc.getString("ToolsPage", "Title"));
    
    QString currentLang = ConfigManager::instance().getLanguage();
    QString authorLabel = loc.getString("Common", "Author");

    for (const auto& card : m_toolCards) {
        // Ask tool to load language
        card.tool->loadLanguage(currentLang);
        
        // Update UI from tool
        card.titleLabel->setText(card.tool->name());
        card.descLabel->setText(card.tool->description());
        
        // Tooltip with Author, Version and Description
        QString tooltip = QString("<b>%1</b> (v%2)<br>%3: %4<br><br>%5")
                          .arg(card.tool->name())
                          .arg(card.tool->version())
                          .arg(authorLabel)
                          .arg(card.tool->author())
                          .arg(card.tool->description());
        
        // Add version mismatch warning to main tooltip as well
        QString appVersion = APP_VERSION;
        if (card.tool->compatibleVersion() != appVersion) {
            QString mismatchTitle = loc.getString("ToolsPage", "VersionMismatch");
            QString reqApp = loc.getString("ToolsPage", "RequiresApp").arg(card.tool->compatibleVersion());
            QString currApp = loc.getString("ToolsPage", "CurrentApp").arg(appVersion);
            QString warning = loc.getString("ToolsPage", "MismatchWarning");

            tooltip += QString("<br><br><font color='red'><b>%1</b><br>%2<br>%3<br>%4</font>")
                       .arg(mismatchTitle)
                       .arg(reqApp)
                       .arg(currApp)
                       .arg(warning);
        }
        
        card.cardWidget->setToolTip(tooltip); 
    }
}

void ToolsPage::updateTheme() {
    ConfigManager::Theme theme = ConfigManager::instance().getTheme();
    bool isDark = (theme == ConfigManager::Theme::Dark);
    
    QString cardBg = isDark ? "rgba(255, 255, 255, 0.1)" : "rgba(0, 0, 0, 0.05)";
    QString cardBorder = isDark ? "rgba(255, 255, 255, 0.2)" : "rgba(0, 0, 0, 0.1)";
    QString titleBg = isDark ? "rgba(0, 0, 0, 0.6)" : "rgba(255, 255, 255, 0.8)";
    QString titleText = isDark ? "#FFFFFF" : "#1D1D1F";

    // Update cards
    QString cardStyle = QString(R"(
        QPushButton#ToolCard {
            background-color: %1;
            border: 1px solid %2;
            border-radius: 10px;
            text-align: center;
            padding: 0px;
        }
        QPushButton#ToolCard:hover {
            background-color: %3;
            border: 1px solid #007AFF;
        }
    )").arg(cardBg, cardBorder, isDark ? "rgba(255, 255, 255, 0.2)" : "rgba(0, 0, 0, 0.1)");

    for (const auto& card : m_toolCards) {
        card.cardWidget->setStyleSheet(cardStyle);
        
        // Find title area and label
        QWidget* titleArea = card.cardWidget->findChild<QWidget*>("CardTitleArea");
        if (titleArea) {
            titleArea->setStyleSheet(QString("background-color: %1; border-bottom-left-radius: 10px; border-bottom-right-radius: 10px;").arg(titleBg));
        }
        
        QLabel* titleLbl = card.cardWidget->findChild<QLabel*>("CardTitle");
        if (titleLbl) {
            titleLbl->setStyleSheet(QString("font-size: 14px; font-weight: bold; color: %1; background: transparent;").arg(titleText));
        }
    }
}
