#pragma once

#include <QDialog>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QComboBox>
#include <QProgressBar>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QJsonObject>
#include <QMouseEvent>
#include <QPoint>
#include <QWidget>

class Setup : public QDialog {
    Q_OBJECT

public:
    explicit Setup(QWidget *parent = nullptr);
    ~Setup() override;

protected:
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;

private slots:
    void browseDirectory();
    void startInstall();
    void changeLanguage(int index);
    void closeWindow();

private:
    void setupUi();
    void loadLanguage(const QString& langCode);
    bool extractPayload(const QString& targetDir);
    void saveTempLanguage();
    void applyTheme();
    bool detectSystemDarkMode();

    QJsonObject currentLoc;

    QWidget* m_centralWidget;
    QLabel* titleLabel;
    QLabel* pathLabel;
    QLineEdit* pathEdit;
    QPushButton* browseBtn;
    QComboBox* langCombo;
    QPushButton* installBtn;
    QProgressBar* progressBar;

    QString currentLang;
    bool m_isDarkMode;
    bool m_dragging;
    QPoint m_dragPosition;
    bool m_isAutoSetup;
};
