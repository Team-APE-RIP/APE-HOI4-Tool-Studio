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
#include <QStyle>
#include <QPropertyAnimation>
#include <QSequentialAnimationGroup>
#include <QEasingCurve>
#include <QEvent>
#include <QShowEvent>
#include <QResizeEvent>

// ============== AnimatedToolCard Implementation ==============

AnimatedToolCard::AnimatedToolCard(int rowIndex, QWidget *parent) 
    : QWidget(parent), m_hoverOffset(0), m_dropOffset(0), m_rowIndex(rowIndex), m_canHover(false) {
    
    // No layout - we manually position the button using geometry
    m_layout = nullptr;
    
    m_button = new QPushButton(this);  // Parent is this widget
    m_button->setObjectName("ToolCard");
    m_button->setCursor(Qt::PointingHandCursor);
    m_button->setFixedSize(200, CARD_HEIGHT);
    m_button->installEventFilter(this);
    
    connect(m_button, &QPushButton::clicked, this, &AnimatedToolCard::clicked);
    
    // Setup opacity effect for drop animation
    m_opacityEffect = new QGraphicsOpacityEffect(m_button);
    m_opacityEffect->setOpacity(1.0);
    m_button->setGraphicsEffect(m_opacityEffect);
    
    // Fixed size for the wrapper - only need space for hover jump (card can go above container)
    setFixedSize(200, CARD_HEIGHT + HOVER_JUMP_HEIGHT);
    
    // Position button at normal position (with space for hover)
    m_button->move(0, HOVER_JUMP_HEIGHT);
}

void AnimatedToolCard::setHoverOffset(int offset) {
    m_hoverOffset = offset;
    updateButtonPosition();
}

void AnimatedToolCard::setDropOffset(int offset) {
    m_dropOffset = offset;
    updateButtonPosition();
}

void AnimatedToolCard::updateButtonPosition() {
    // Button Y position:
    // - Normal: HOVER_JUMP_HEIGHT (at bottom of container)
    // - Hover: 0 (jumped up by HOVER_JUMP_HEIGHT)
    // - Drop animation: uses dropOffset to move from above to normal position
    //   dropOffset > 0 means button is above normal position
    int y = HOVER_JUMP_HEIGHT - m_hoverOffset - m_dropOffset;
    m_button->move(0, y);
}

void AnimatedToolCard::prepareDropAnimation() {
    m_canHover = false;
    // Calculate total drop distance: base drop + extra distance based on row
    // All cards start from the same absolute position (above the first row)
    int totalDropDistance = DROP_HEIGHT + m_rowIndex * ROW_HEIGHT;
    m_dropOffset = totalDropDistance;
    m_opacityEffect->setOpacity(0.0);
    updateButtonPosition();
}

void AnimatedToolCard::playDropAnimation(int delay) {
    m_canHover = false;
    
    // Calculate total drop distance based on row index
    int totalDropDistance = DROP_HEIGHT + m_rowIndex * ROW_HEIGHT;
    
    QTimer::singleShot(delay, this, [this, totalDropDistance]() {
        // Animate dropOffset from totalDropDistance to 0 (button drops to normal position)
        QPropertyAnimation *dropAnim = new QPropertyAnimation(this, "dropOffset");
        // Duration scales with distance for consistent speed feel
        int duration = 600 + m_rowIndex * 100;
        dropAnim->setDuration(duration);
        dropAnim->setStartValue(totalDropDistance);
        dropAnim->setEndValue(0);
        dropAnim->setEasingCurve(QEasingCurve::OutBounce);
        
        // Opacity animation
        QPropertyAnimation *opacityAnim = new QPropertyAnimation(m_opacityEffect, "opacity");
        opacityAnim->setDuration(300);
        opacityAnim->setStartValue(0.0);
        opacityAnim->setEndValue(1.0);
        opacityAnim->setEasingCurve(QEasingCurve::OutQuad);
        
        connect(dropAnim, &QPropertyAnimation::finished, this, [this]() {
            m_canHover = true;
            m_dropOffset = 0;
        });
        
        dropAnim->start(QAbstractAnimation::DeleteWhenStopped);
        opacityAnim->start(QAbstractAnimation::DeleteWhenStopped);
    });
}

void AnimatedToolCard::resetToNormal() {
    m_canHover = true;
    m_opacityEffect->setOpacity(1.0);
    m_hoverOffset = 0;
    m_dropOffset = 0;
    updateButtonPosition();
}

void AnimatedToolCard::playHoverAnimation() {
    if (m_isHoverAnimating || !m_canHover) return;
    m_isHoverAnimating = true;
    
    QPropertyAnimation *anim = new QPropertyAnimation(this, "hoverOffset");
    anim->setDuration(150);
    anim->setStartValue(0);
    anim->setEndValue(HOVER_JUMP_HEIGHT);
    anim->setEasingCurve(QEasingCurve::OutQuad);
    
    anim->start(QAbstractAnimation::DeleteWhenStopped);
}

void AnimatedToolCard::playLeaveAnimation() {
    if (!m_canHover) return;
    
    QPropertyAnimation *anim = new QPropertyAnimation(this, "hoverOffset");
    anim->setDuration(200);
    anim->setStartValue(m_hoverOffset);
    anim->setEndValue(0);
    anim->setEasingCurve(QEasingCurve::OutBounce);
    
    connect(anim, &QPropertyAnimation::finished, this, [this]() {
        m_isHoverAnimating = false;
    });
    
    anim->start(QAbstractAnimation::DeleteWhenStopped);
}

bool AnimatedToolCard::eventFilter(QObject *obj, QEvent *event) {
    if (obj == m_button) {
        if (event->type() == QEvent::Enter) {
            playHoverAnimation();
        } else if (event->type() == QEvent::Leave) {
            playLeaveAnimation();
        }
    }
    return QWidget::eventFilter(obj, event);
}

// ============== ToolsPage Implementation ==============

ToolsPage::ToolsPage(QWidget *parent) : QWidget(parent) {
    setupUi();
    connect(&ToolManager::instance(), &ToolManager::toolsLoaded, this, &ToolsPage::refreshTools);
    connect(&ConfigManager::instance(), &ConfigManager::themeChanged, this, [this](ConfigManager::Theme){ updateTheme(); });

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
    m_scrollArea = new QScrollArea();
    m_scrollArea->setWidgetResizable(true);
    m_scrollArea->setFrameShape(QFrame::NoFrame);
    
    m_contentWidget = new QWidget();
    m_contentWidget->setObjectName("ToolsContent");
    QVBoxLayout *contentLayout = new QVBoxLayout(m_contentWidget);
    contentLayout->setContentsMargins(40, 20, 40, 40);
    
    // Container for card rows
    m_cardsContainer = new QWidget();
    m_rowsLayout = new QVBoxLayout(m_cardsContainer);
    m_rowsLayout->setSpacing(20);
    m_rowsLayout->setContentsMargins(0, 0, 0, 0);
    
    // Disable clipping to allow drop animation to show cards above container bounds
    m_cardsContainer->setAttribute(Qt::WA_TranslucentBackground);
    m_contentWidget->setAttribute(Qt::WA_TranslucentBackground);
    m_scrollArea->viewport()->setAttribute(Qt::WA_TranslucentBackground);

    contentLayout->addWidget(m_cardsContainer);
    contentLayout->addStretch();

    m_scrollArea->setWidget(m_contentWidget);
    layout->addWidget(m_scrollArea);
    
    // Animation layer - covers the entire ToolsPage for drop animations
    m_animationLayer = new QWidget(this);
    m_animationLayer->setAttribute(Qt::WA_TransparentForMouseEvents);
    m_animationLayer->setAttribute(Qt::WA_TranslucentBackground);
    m_animationLayer->setStyleSheet("background: transparent;");
    m_animationLayer->hide();
}

void ToolsPage::resizeEvent(QResizeEvent *event) {
    QWidget::resizeEvent(event);
    // Keep animation layer covering the entire page
    m_animationLayer->setGeometry(0, 0, width(), height());
}

void ToolsPage::showEvent(QShowEvent *event) {
    QWidget::showEvent(event);
    // Trigger drop animations every time the page is shown
    QTimer::singleShot(50, this, &ToolsPage::playDropAnimations);
}

void ToolsPage::refreshTools() {
    Logger::instance().logInfo("ToolsPage", "Refreshing tools...");
    
    // Clear existing rows
    QLayoutItem *rowItem;
    while ((rowItem = m_rowsLayout->takeAt(0)) != nullptr) {
        if (QWidget *rowWidget = rowItem->widget()) {
            QLayout *rowLayout = rowWidget->layout();
            if (rowLayout) {
                QLayoutItem *cardItem;
                while ((cardItem = rowLayout->takeAt(0)) != nullptr) {
                    delete cardItem->widget();
                    delete cardItem;
                }
            }
            delete rowWidget;
        }
        delete rowItem;
    }
    m_toolCards.clear();

    QList<ToolInterface*> tools = ToolManager::instance().getTools();
    Logger::instance().logInfo("ToolsPage", QString("Found %1 tools.").arg(tools.size()));
    
    int index = 0;
    int col = 0;
    int row = 0;
    QWidget *currentRow = nullptr;
    QHBoxLayout *currentRowLayout = nullptr;

    for (ToolInterface* tool : tools) {
        // Create new row if needed
        if (col == 0) {
            currentRow = new QWidget();
            currentRowLayout = new QHBoxLayout(currentRow);
            currentRowLayout->setSpacing(0);
            currentRowLayout->setContentsMargins(0, 0, 0, 0);
            currentRowLayout->addStretch(1); // Left stretch
            m_rowsLayout->addWidget(currentRow);
        }
        
        Logger::instance().logInfo("ToolsPage", "Adding card for tool: " + tool->name());
        QWidget *card = createToolCard(tool, index, row);
        currentRowLayout->addWidget(card);
        currentRowLayout->addStretch(1); // Stretch after each card for equal spacing
        
        col++;
        index++;
        
        if (col >= MAX_COLS) {
            col = 0;
            row++;
        }
    }
    
    updateTexts();
    updateTheme();
}

void ToolsPage::playDropAnimations() {
    if (m_toolCards.isEmpty()) return;
    
    // Ensure layout is complete
    QCoreApplication::processEvents();
    
    // Show animation layer
    m_animationLayer->setGeometry(0, 0, width(), height());
    m_animationLayer->show();
    m_animationLayer->raise();
    m_animationsRunning = m_toolCards.size();
    
    int delay = 0;
    for (int i = 0; i < m_toolCards.size(); i++) {
        auto& cardInfo = m_toolCards[i];
        AnimatedToolCard* card = cardInfo.cardWidget;
        QPushButton* btn = card->button();
        
        // Calculate target position in animation layer coordinates
        QPoint cardPosInPage = card->mapTo(this, QPoint(0, 0));
        // Target position is where the button should end up (with hover offset space)
        QPoint targetPos = cardPosInPage + QPoint(0, AnimatedToolCard::HOVER_JUMP_HEIGHT);
        cardInfo.targetPos = targetPos;
        
        // Move button to animation layer
        btn->setParent(m_animationLayer);
        
        // Start position: above the window (y = negative)
        int startY = -AnimatedToolCard::CARD_HEIGHT - 50;  // Start above the window
        btn->move(targetPos.x(), startY);
        btn->show();
        
        // Setup opacity effect
        QGraphicsOpacityEffect* opacityEffect = qobject_cast<QGraphicsOpacityEffect*>(btn->graphicsEffect());
        if (opacityEffect) {
            opacityEffect->setOpacity(0.0);
        }
        
        // Play animation with delay
        QTimer::singleShot(delay, this, [this, card, btn, targetPos, opacityEffect, i]() {
            // Position animation
            QPropertyAnimation *posAnim = new QPropertyAnimation(btn, "pos");
            int duration = 600 + card->rowIndex() * 100;
            posAnim->setDuration(duration);
            posAnim->setStartValue(btn->pos());
            posAnim->setEndValue(targetPos);
            posAnim->setEasingCurve(QEasingCurve::OutBounce);
            
            // Opacity animation
            QPropertyAnimation *opacityAnim = nullptr;
            if (opacityEffect) {
                opacityAnim = new QPropertyAnimation(opacityEffect, "opacity");
                opacityAnim->setDuration(300);
                opacityAnim->setStartValue(0.0);
                opacityAnim->setEndValue(1.0);
                opacityAnim->setEasingCurve(QEasingCurve::OutQuad);
            }
            
            connect(posAnim, &QPropertyAnimation::finished, this, [this, card]() {
                onDropAnimationFinished(card);
            });
            
            posAnim->start(QAbstractAnimation::DeleteWhenStopped);
            if (opacityAnim) {
                opacityAnim->start(QAbstractAnimation::DeleteWhenStopped);
            }
        });
        
        delay += 80;
    }
}

void ToolsPage::onDropAnimationFinished(AnimatedToolCard* card) {
    // Move button back to its original parent
    QPushButton* btn = card->button();
    btn->setParent(card);
    btn->move(0, AnimatedToolCard::HOVER_JUMP_HEIGHT);
    btn->show();
    
    // Enable hover
    card->resetToNormal();
    
    m_animationsRunning--;
    if (m_animationsRunning <= 0) {
        m_animationLayer->hide();
    }
}

QWidget* ToolsPage::createToolCard(ToolInterface* tool, int index, int rowIndex) {
    Q_UNUSED(index);
    
    AnimatedToolCard *cardWrapper = new AnimatedToolCard(rowIndex);
    QPushButton *card = cardWrapper->button();
    
    QString toolId = tool->id();
    connect(cardWrapper, &AnimatedToolCard::clicked, [this, toolId](){ emit toolSelected(toolId); });

    QVBoxLayout *layout = new QVBoxLayout(card);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    // Cover Area
    QWidget *coverContainer = new QWidget();
    coverContainer->setFixedHeight(250);
    QGridLayout *coverLayout = new QGridLayout(coverContainer);
    coverLayout->setContentsMargins(0, 0, 0, 0);
    coverLayout->setSpacing(0);

    // Cover Image
    QLabel *iconLbl = new QLabel();
    QIcon icon = tool->icon();
    if (!icon.isNull()) {
        iconLbl->setPixmap(icon.pixmap(200, 250));
        iconLbl->setScaledContents(true);
    } else {
        iconLbl->setText("No Image");
        iconLbl->setAlignment(Qt::AlignCenter);
        iconLbl->setStyleSheet("background-color: #333; color: #888;");
    }
    iconLbl->setStyleSheet("border-top-left-radius: 10px; border-top-right-radius: 10px;");
    
    coverLayout->addWidget(iconLbl, 0, 0);

    // Version Check
    QString appVersion = APP_VERSION;
    QString requiredVersion = tool->compatibleVersion();
    bool versionMismatch = (appVersion != requiredVersion);

    if (versionMismatch) {
        QLabel *warningLbl = new QLabel("!");
        warningLbl->setFixedSize(24, 24);
        warningLbl->setStyleSheet("background-color: #FF3B30; color: white; border-radius: 12px; font-weight: bold; qproperty-alignment: AlignCenter; margin: 5px;");
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
    
    QLabel *descLbl = new QLabel(tool->description());
    descLbl->setVisible(false); 

    titleLayout->addWidget(titleLbl);

    layout->addWidget(coverContainer);
    layout->addWidget(titleArea);

    m_toolCards.append({toolId, titleLbl, descLbl, tool, cardWrapper});
    
    return cardWrapper;
}

void ToolsPage::updateTexts() {
    LocalizationManager& loc = LocalizationManager::instance();
    m_titleLabel->setText(loc.getString("ToolsPage", "Title"));
    
    QString currentLang = ConfigManager::instance().getLanguage();
    QString authorLabel = loc.getString("Common", "Author");

    for (const auto& card : m_toolCards) {
        card.tool->loadLanguage(currentLang);
        
        card.titleLabel->setText(card.tool->name());
        card.descLabel->setText(card.tool->description());
        
        QString tooltip = QString("<b>%1</b> (v%2)<br>%3: %4<br><br>%5")
                          .arg(card.tool->name())
                          .arg(card.tool->version())
                          .arg(authorLabel)
                          .arg(card.tool->author())
                          .arg(card.tool->description());
        
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
        
        card.cardWidget->button()->setToolTip(tooltip); 
    }
}

void ToolsPage::updateTheme() {
    ConfigManager::Theme theme = ConfigManager::instance().getTheme();
    bool isDark = (theme == ConfigManager::Theme::Dark);
    
    QString cardBg = isDark ? "#3A3A3C" : "#EEEEEE";
    QString cardBorder = isDark ? "#2C2C2E" : "#F5F5F7";
    QString cardHover = isDark ? "#2C2C2E" : "#F5F5F7";
    QString titleBg = isDark ? "#1C1C1E" : "#FFFFFF";
    QString titleText = isDark ? "#FFFFFF" : "#1D1D1F";
    QString placeholderBg = isDark ? "#3A3A3C" : "#E8E8E8";
    QString placeholderText = isDark ? "#888888" : "#666666";

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
    )").arg(cardBg, cardBorder, cardHover);

    for (const auto& card : m_toolCards) {
        card.cardWidget->button()->setStyleSheet(cardStyle);
        
        QWidget* titleArea = card.cardWidget->button()->findChild<QWidget*>("CardTitleArea");
        if (titleArea) {
            titleArea->setStyleSheet(QString("QWidget#CardTitleArea { background-color: %1; border-bottom-left-radius: 10px; border-bottom-right-radius: 10px; }").arg(titleBg));
            titleArea->style()->unpolish(titleArea);
            titleArea->style()->polish(titleArea);
            titleArea->update();
        }
        
        QLabel* titleLbl = card.cardWidget->button()->findChild<QLabel*>("CardTitle");
        if (titleLbl) {
            titleLbl->setStyleSheet(QString("QLabel#CardTitle { font-size: 14px; font-weight: bold; color: %1; background: transparent; }").arg(titleText));
            titleLbl->style()->unpolish(titleLbl);
            titleLbl->style()->polish(titleLbl);
            titleLbl->update();
        }
        
        QList<QLabel*> labels = card.cardWidget->button()->findChildren<QLabel*>();
        for (QLabel* lbl : labels) {
            if (lbl->objectName() != "CardTitle" && lbl->text() == "No Image") {
                lbl->setStyleSheet(QString("background-color: %1; color: %2; border-top-left-radius: 10px; border-top-right-radius: 10px;").arg(placeholderBg, placeholderText));
            }
        }
        
        card.cardWidget->button()->update();
    }
}
