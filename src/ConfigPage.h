#ifndef CONFIGPAGE_H
#define CONFIGPAGE_H

#include <QWidget>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

class ConfigPage : public QWidget {
    Q_OBJECT

public:
    explicit ConfigPage(QWidget *parent = nullptr);
    void updateTexts();
    void updateTheme();

signals:
    void closeClicked();
    void gamePathChanged();
    void modClosed();

private slots:
    void browseGamePath();
    void closeCurrentMod();

private:
    void setupUi();
    QWidget* createGroup(const QString &title, QLayout *contentLayout);
    QWidget* createSettingRow(const QString &id, const QString &icon, const QString &title, const QString &desc, QWidget *valueWidget, QWidget *control);

    QLabel *m_gamePathValue;
    QLabel *m_modPathValue;
};

#endif // CONFIGPAGE_H
