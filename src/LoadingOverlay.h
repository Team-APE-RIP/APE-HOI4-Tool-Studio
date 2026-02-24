#ifndef LOADINGOVERLAY_H
#define LOADINGOVERLAY_H

#include <QWidget>
#include <QLabel>
#include <QProgressBar>
#include <QVBoxLayout>

class LoadingOverlay : public QWidget {
    Q_OBJECT

public:
    explicit LoadingOverlay(QWidget *parent = nullptr);
    
    void setMessage(const QString& message);
    void setProgress(int value); // 0-100, -1 for indeterminate
    void showOverlay();
    void hideOverlay();

protected:
    void paintEvent(QPaintEvent *event) override;
    bool eventFilter(QObject *obj, QEvent *event) override;

private:
    void updatePosition();
    
    QWidget *m_container;
    QLabel *m_iconLabel;
    QLabel *m_messageLabel;
    QProgressBar *m_progressBar;
};

#endif // LOADINGOVERLAY_H
