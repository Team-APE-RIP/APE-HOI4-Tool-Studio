#ifndef SETTINGSPAGE_H
#define SETTINGSPAGE_H

#include <QWidget>
#include <QComboBox>
#include <QCheckBox>
#include <QLabel>
#include <QPushButton>
#include <QSpinBox>

class SettingsPage : public QWidget {
    Q_OBJECT

public:
    explicit SettingsPage(QWidget *parent = nullptr);
    void updateTexts();
    void updateTheme();

signals:
    void closeClicked();
    void themeChanged();
    void languageChanged();
    void debugModeChanged(bool enabled);
    void sidebarCompactChanged(bool enabled);

private slots:
    void openUrl(const QString &url);
    void toggleOpenSource();
    void openLogDir();
    void createStartMenuShortcut();
    void clearAppCache();

private:
    void setupUi();
    QWidget* createSettingRow(const QString &id, const QString &icon, const QString &title, const QString &desc, QWidget *control);
    QWidget* createGroup(const QString &title, QLayout *contentLayout);

    QComboBox *m_themeCombo;
    QComboBox *m_languageCombo;
    QCheckBox *m_debugCheck;
    QCheckBox *m_sidebarCompactCheck;
    QSpinBox *m_maxLogFilesSpin;
    QLabel *m_versionLabel;
    QWidget *m_openSourceArea;
    QPushButton *m_openSourceToggleBtn;
    QPushButton *m_openLogBtn;
    QPushButton *m_pinToStartBtn;
    QPushButton *m_clearCacheBtn;
};

#endif // SETTINGSPAGE_H
