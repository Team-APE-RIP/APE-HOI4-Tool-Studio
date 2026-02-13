#ifndef TOOLSPAGE_H
#define TOOLSPAGE_H

#include <QWidget>
#include <QPushButton>
#include <QLabel>
#include <QGridLayout>
#include "ToolInterface.h"

class ToolsPage : public QWidget {
    Q_OBJECT

public:
    explicit ToolsPage(QWidget *parent = nullptr);
    void updateTexts();
    void refreshTools(); // Moved to public
    void updateTheme();

signals:
    void closeClicked();
    void toolSelected(const QString &toolId);

private:
    void setupUi();
    QWidget* createToolCard(ToolInterface* tool);

    QLabel *m_titleLabel;
    QPushButton *m_closeBtn;
    QGridLayout *m_gridLayout;
    
    // Keep track of dynamic texts
    struct ToolCardInfo {
        QString id;
        QLabel *titleLabel;
        QLabel *descLabel;
        ToolInterface* tool;
        QWidget* cardWidget; // To update style
    };
    QList<ToolCardInfo> m_toolCards;
};

#endif // TOOLSPAGE_H
