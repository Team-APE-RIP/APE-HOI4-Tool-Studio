#ifndef CUSTOMMESSAGEBOX_H
#define CUSTOMMESSAGEBOX_H

#include <QDialog>
#include <QLabel>
#include <QPushButton>
#include <QPaintEvent>
#include <QMessageBox>

class CustomMessageBox : public QDialog {
    Q_OBJECT

public:
    enum Type { Information, Question };

    explicit CustomMessageBox(QWidget *parent, const QString &title, const QString &message, Type type = Information);
    
    static void information(QWidget *parent, const QString &title, const QString &message);
    static QMessageBox::StandardButton question(QWidget *parent, const QString &title, const QString &message);

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    void setupUi(const QString &title, const QString &message, Type type);
    QMessageBox::StandardButton m_result = QMessageBox::NoButton;
};

#endif // CUSTOMMESSAGEBOX_H
