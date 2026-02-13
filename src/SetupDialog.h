#ifndef SETUPDIALOG_H
#define SETUPDIALOG_H

#include <QDialog>
#include <QLineEdit>
#include <QComboBox>

class SetupDialog : public QDialog {
    Q_OBJECT

public:
    explicit SetupDialog(QWidget *parent = nullptr);

    QString getGamePath() const;
    QString getModPath() const;
    QString getLanguage() const;

private slots:
    void browseGamePath();
    void browseModPath();
    void onLanguageChanged(const QString &lang);
    void validateAndAccept();

private:
    void setupUi();
    void updateTexts();

    QLineEdit *m_gamePathEdit;
    QLineEdit *m_modPathEdit;
    QComboBox *m_languageCombo;
    
    // UI Elements that need translation updates
    // Using member variables to update text dynamically
    // In a full app, we'd use retranslateUi()
};

#endif // SETUPDIALOG_H
