#ifndef UPDATE_H
#define UPDATE_H

#include <QWidget>
#include <QLabel>
#include <QProgressBar>
#include <QVBoxLayout>
#include <QPushButton>
#include <QStackedWidget>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QFile>

class Update : public QWidget {
    Q_OBJECT

public:
    explicit Update(QWidget *parent = nullptr);
    ~Update();

    void checkForUpdates();
    void updateTheme();

protected:
    void paintEvent(QPaintEvent *event) override;
    bool eventFilter(QObject *obj, QEvent *event) override;

private slots:
    void onCheckFinished(QNetworkReply *reply);
    void onDownloadProgress(qint64 bytesReceived, qint64 bytesTotal);
    void onDownloadFinished();
    void onDownloadReadyRead();
    void onUpdateClicked();

private:
    void updatePosition();
    void setupUi();
    void showUpdateDialog(const QString& version, const QString& changelog, const QString& downloadUrl);
    void startDownload();

    QWidget *m_container;
    QLabel *m_titleLabel;
    QLabel *m_versionLabel;
    QLabel *m_changelogLabel;
    QProgressBar *m_progressBar;
    QLabel *m_progressTextLabel;
    QPushButton *m_updateBtn;
    QStackedWidget *m_bottomStack;

    QNetworkAccessManager *m_networkManager;
    QNetworkReply *m_downloadReply;
    QFile *m_downloadFile;

    QString m_downloadUrl;
    QString m_tempFilePath;
};

#endif // UPDATE_H
