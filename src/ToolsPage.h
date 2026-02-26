#ifndef TOOLSPAGE_H
#define TOOLSPAGE_H

#include <QWidget>
#include <QPushButton>
#include <QLabel>
#include <QGridLayout>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPropertyAnimation>
#include <QParallelAnimationGroup>
#include <QTimer>
#include <QGraphicsOpacityEffect>
#include <QScrollArea>
#include "ToolInterface.h"

// Custom animated tool card - simple wrapper that handles hover animation
class AnimatedToolCard : public QWidget {
    Q_OBJECT
    Q_PROPERTY(int hoverOffset READ hoverOffset WRITE setHoverOffset)
    Q_PROPERTY(int dropOffset READ dropOffset WRITE setDropOffset)

public:
    explicit AnimatedToolCard(int rowIndex = 0, QWidget *parent = nullptr);
    
    QPushButton* button() const { return m_button; }
    
    int hoverOffset() const { return m_hoverOffset; }
    void setHoverOffset(int offset);
    
    int dropOffset() const { return m_dropOffset; }
    void setDropOffset(int offset);
    
    int rowIndex() const { return m_rowIndex; }
    
    void prepareDropAnimation();
    void playDropAnimation(int delay);
    void resetToNormal();

signals:
    void clicked();

protected:
    bool eventFilter(QObject *obj, QEvent *event) override;

private:
    void playHoverAnimation();
    void playLeaveAnimation();
    void updateButtonPosition();
    
    QPushButton *m_button;
    QVBoxLayout *m_layout;
    QGraphicsOpacityEffect *m_opacityEffect;
    int m_hoverOffset = 0;
    int m_dropOffset = 0;  // 0 = normal position, calculated based on row
    int m_rowIndex = 0;    // Row index for calculating drop start position
    bool m_isHoverAnimating = false;
    bool m_canHover = false;
    
public:
    static const int HOVER_JUMP_HEIGHT = 15;
    static const int DROP_HEIGHT = 350;      // Base drop distance
    static const int ROW_HEIGHT = 320;       // Height of each row (card height + spacing)
    static const int CARD_HEIGHT = 300;
};

class ToolsPage : public QWidget {
    Q_OBJECT

public:
    explicit ToolsPage(QWidget *parent = nullptr);
    void updateTexts();
    void refreshTools();
    void updateTheme();

signals:
    void closeClicked();
    void toolSelected(const QString &toolId);

protected:
    void showEvent(QShowEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

private:
    void setupUi();
    QWidget* createToolCard(ToolInterface* tool, int index, int rowIndex);
    void playDropAnimations();
    void onDropAnimationFinished(AnimatedToolCard* card);

    QLabel *m_titleLabel;
    QPushButton *m_closeBtn;
    QWidget *m_cardsContainer;
    QVBoxLayout *m_rowsLayout;
    QWidget *m_contentWidget; // Reference to scroll content for coordinate calculation
    QWidget *m_animationLayer; // Overlay layer for drop animations
    QScrollArea *m_scrollArea;
    
    struct ToolCardInfo {
        QString id;
        QLabel *titleLabel;
        QLabel *descLabel;
        ToolInterface* tool;
        AnimatedToolCard* cardWidget;
        QPoint targetPos;  // Target position in animation layer coordinates
    };
    QList<ToolCardInfo> m_toolCards;
    int m_animationsRunning = 0;
    
    static const int MAX_COLS = 5;
};

#endif // TOOLSPAGE_H
