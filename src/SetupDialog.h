#ifndef SETUPDIALOG_H
#define SETUPDIALOG_H

#include <QDialog>
#include <QLineEdit>
#include <QComboBox>
#include <QMouseEvent>
#include <QPoint>
#include <QWidget>

class SetupDialog : public QDialog {
    Q_OBJECT

public:
    explicit SetupDialog(QWidget *parent = nullptr);

    QString getGamePath() const;
    QString getModPath() const;
    QString getLanguage() const;

protected:
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;

private slots:
    void browseGamePath();
    void browseModPath();
    void onLanguageChanged(const QString &lang);
    void onGamePathChanged(const QString &path);
    void onModPathChanged(const QString &path);
    void validateAndAccept();
    void closeWindow();

private:
    void setupUi();
    void updateTexts();
    void applyTheme();
    bool detectSystemDarkMode();

    QWidget *m_centralWidget;
    QLineEdit *m_gamePathEdit;
    QLineEdit *m_modPathEdit;
    QComboBox *m_languageCombo;
    bool m_isDarkMode;
    
    // Dragging support
    bool m_dragging;
    QPoint m_dragPosition;
};

#endif // SETUPDIALOG_H
