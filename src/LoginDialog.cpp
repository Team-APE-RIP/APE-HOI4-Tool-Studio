//-------------------------------------------------------------------------------------
// LoginDialog.cpp -- Part of APE HOI4 Tool Studio
//
// Copyright (C) 2026 Team APE:RIP. All rights reserved.
// Licensed under the Team APE:RIP Source Code License Agreement.
//
// https://github.com/Team-APE-RIP/APE-HOI4-Tool-Studio/
//-------------------------------------------------------------------------------------
#include "LoginDialog.h"
#include "AuthManager.h"
#include "ConfigManager.h"
#include "LocalizationManager.h"
#include "SslConfig.h"
#include <QPainter>
#include <QPainterPath>
#include <QGraphicsDropShadowEffect>
#include <QEvent>
#include <QShortcut>
#include <QKeySequence>
#include <QToolButton>
#include <QHBoxLayout>
#include <QLinearGradient>
#include <QFontMetrics>

#include <QNetworkRequest>
#include <QNetworkReply>
#include <QNetworkAccessManager>
#include <QSslError>
#include <QJsonDocument>
#include <QJsonObject>

namespace {
class MarqueeStatusLabel : public QWidget {
public:
    explicit MarqueeStatusLabel(QWidget *parent = nullptr)
        : QWidget(parent)
        , m_timer(new QTimer(this))
        , m_emphasized(false)
        , m_offset(0.0)
        , m_textWidth(0.0)
        , m_tickCount(0)
    {
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        setMinimumHeight(28);
        m_timer->setInterval(80);
        connect(m_timer, &QTimer::timeout, this, [this]() {
            updateAnimation();
            update();
        });
    }

    void setContent(const QString& text) {
        if (m_text == text) {
            recalculateMetrics();
            updateAnimationState();
            update();
            return;
        }

        m_text = text;
        m_offset = 0.0;
        m_tickCount = 0;
        recalculateMetrics();
        updateAnimationState();
        update();
    }

    void setTextColor(const QColor& color) {
        m_textColor = color;
        update();
    }

    void setEmphasized(bool emphasized) {
        if (m_emphasized == emphasized) {
            recalculateMetrics();
            updateAnimationState();
            update();
            return;
        }

        m_emphasized = emphasized;
        recalculateMetrics();
        updateAnimationState();
        update();
    }

protected:
    void resizeEvent(QResizeEvent *event) override {
        QWidget::resizeEvent(event);
        recalculateMetrics();
        updateAnimationState();
    }

    void paintEvent(QPaintEvent *event) override {
        Q_UNUSED(event);

        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);
        painter.setClipRect(rect());

        QFont drawFont = font();
        drawFont.setBold(m_emphasized);
        painter.setFont(drawFont);

        if (m_text.isEmpty()) {
            return;
        }

        const QRect textRect = rect().adjusted(6, 0, -6, 0);
        const QFontMetrics metrics(drawFont);
        const int baselineY = textRect.y() + ((textRect.height() - metrics.height()) / 2) + metrics.ascent();
        const qreal availableWidth = qMax(1, textRect.width());
        const qreal fadeWidth = qMin<qreal>(18.0, availableWidth * 0.12);

        if (m_textWidth <= availableWidth) {
            painter.setPen(m_textColor);
            painter.drawText(textRect, Qt::AlignCenter, m_text);
            return;
        }

        auto buildGradientBrush = [&](qreal left, qreal drawWidth) {
            QLinearGradient gradient(left, 0.0, left + drawWidth, 0.0);
            const qreal fadeRatio = drawWidth > 0.0 ? qBound(0.0, fadeWidth / drawWidth, 0.25) : 0.0;
            gradient.setColorAt(0.0, QColor(m_textColor.red(), m_textColor.green(), m_textColor.blue(), 0));
            gradient.setColorAt(fadeRatio, m_textColor);
            gradient.setColorAt(1.0 - fadeRatio, m_textColor);
            gradient.setColorAt(1.0, QColor(m_textColor.red(), m_textColor.green(), m_textColor.blue(), 0));
            return QBrush(gradient);
        };

        auto drawTextPath = [&](qreal x) {
            QPainterPath path;
            path.addText(QPointF(x, baselineY), drawFont, m_text);
            painter.fillPath(path, buildGradientBrush(x, m_textWidth));
        };

        const qreal gap = 48.0;
        const qreal startX = textRect.x() - m_offset;
        drawTextPath(startX);
        drawTextPath(startX + m_textWidth + gap);
    }

private:
    void recalculateMetrics() {
        QFont drawFont = font();
        drawFont.setBold(m_emphasized);
        m_textWidth = QFontMetrics(drawFont).horizontalAdvance(m_text);
    }

    void updateAnimationState() {
        if (!m_text.isEmpty() && m_textWidth > width()) {
            if (!m_timer->isActive()) {
                m_timer->start();
            }
        } else {
            m_timer->stop();
            m_offset = 0.0;
            m_tickCount = 0;
        }
    }

    void updateAnimation() {
        if (m_textWidth <= width()) {
            m_offset = 0.0;
            m_tickCount = 0;
            return;
        }

        const qreal gap = 48.0;
        const qreal speed = 1.0;
        const qint64 holdTicks = 24;

        if (m_tickCount < holdTicks) {
            m_tickCount += 1;
            return;
        }

        m_offset += speed;
        if (m_offset > m_textWidth + gap) {
            m_offset = 0.0;
            m_tickCount = 0;
        }
    }

    QTimer *m_timer;
    QString m_text;
    QColor m_textColor = QColor(255, 59, 48);
    bool m_emphasized;
    qreal m_offset;
    qreal m_textWidth;
    qint64 m_tickCount;
};

QString sanitizeSensitiveAuthEndpointText(const QString& text) {
    return AuthManager::sanitizeSensitiveApiText(text);
}

QString mapNetworkErrorToDisplayMessage(const QString& errorCode, const QString& errorDetail) {
    LocalizationManager& loc = LocalizationManager::instance();

    if (errorCode == "1") {
        return QString("%1 (%2)")
            .arg(loc.getString("LoginDialog", "UnknownError").arg("ConnectionRefusedError"))
            .arg(errorDetail.isEmpty() ? QStringLiteral("Connection refused") : errorDetail);
    }

    if (errorCode == "2") {
        return QString("%1 (%2)")
            .arg(loc.getString("LoginDialog", "UnknownError").arg("RemoteHostClosedError"))
            .arg(errorDetail.isEmpty() ? QStringLiteral("Remote host closed the connection") : errorDetail);
    }

    if (errorCode == "3") {
        return QString("%1 (%2)")
            .arg(loc.getString("LoginDialog", "UnknownError").arg("HostNotFoundError"))
            .arg(errorDetail.isEmpty() ? QStringLiteral("Host not found") : errorDetail);
    }

    if (errorCode == "4") {
        return QString("%1 (%2)")
            .arg(loc.getString("LoginDialog", "UnknownError").arg("TimeoutError"))
            .arg(errorDetail.isEmpty() ? QStringLiteral("Connection timed out") : errorDetail);
    }

    if (errorCode == "6") {
        return QString("%1 (%2)")
            .arg(loc.getString("LoginDialog", "UnknownError").arg("SslHandshakeFailedError"))
            .arg(errorDetail.isEmpty() ? QStringLiteral("SSL handshake failed") : errorDetail);
    }

    if (!errorDetail.isEmpty()) {
        return QString("%1 (%2)")
            .arg(loc.getString("LoginDialog", "UnknownError").arg(errorCode))
            .arg(errorDetail);
    }

    return loc.getString("LoginDialog", "UnknownError").arg(errorCode);
}
}

LoginDialog::LoginDialog(QWidget *parent)
    : QWidget(parent)
    , m_isLoginMode(true)
    , m_accountActionCountdownTimer(new QTimer(this))
{
    setAttribute(Qt::WA_TransparentForMouseEvents, false);
    setAttribute(Qt::WA_TranslucentBackground);

    setupUi();
    updateTheme();
    updateTexts();

    hide();

    if (parent) {
        parent->installEventFilter(this);
    }

    connect(&ConfigManager::instance(), &ConfigManager::languageChanged, this, &LoginDialog::updateTexts);
    // Use old-style connect for cross-DLL signals to avoid MOC index mismatch issues
    connect(&AuthManager::instance(), SIGNAL(loginSuccess()), this, SLOT(onLoginSuccess()));
    connect(&AuthManager::instance(), SIGNAL(loginFailed(QString)), this, SLOT(onLoginFailed(QString)));
    connect(m_accountActionCountdownTimer, &QTimer::timeout, this, &LoginDialog::onAccountActionCountdownTick);
    m_accountActionCountdownTimer->setInterval(1000);
}

LoginDialog::~LoginDialog() {
}

void LoginDialog::setupUi() {
    m_container = new QWidget(this);
    m_container->setObjectName("LoginContainer");
    m_container->setFixedSize(400, 380);
    
    QGraphicsDropShadowEffect *shadow = new QGraphicsDropShadowEffect(this);
    shadow->setBlurRadius(15);
    shadow->setColor(QColor(0, 0, 0, 80));
    shadow->setOffset(0, 0);
    m_container->setGraphicsEffect(shadow);

    QVBoxLayout *mainLayout = new QVBoxLayout(m_container);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    
    m_stackedWidget = new QStackedWidget(m_container);
    mainLayout->addWidget(m_stackedWidget);
    
    // --- Page 0: Form Page ---
    m_formPage = new QWidget();
    QVBoxLayout *formLayout = new QVBoxLayout(m_formPage);
    formLayout->setContentsMargins(30, 30, 30, 30);
    formLayout->setSpacing(15);

    m_titleLabel = new QLabel(m_formPage);
    m_titleLabel->setObjectName("LoginTitle");
    m_titleLabel->setAlignment(Qt::AlignCenter);
    QFont titleFont = m_titleLabel->font();
    titleFont.setPointSize(18);
    titleFont.setBold(true);
    m_titleLabel->setFont(titleFont);

    m_subtitleLabel = new QLabel(m_formPage);
    m_subtitleLabel->setObjectName("LoginSubtitle");
    m_subtitleLabel->setAlignment(Qt::AlignCenter);

    m_usernameInput = new QLineEdit(m_formPage);
    m_usernameInput->setObjectName("LoginInput");
    m_usernameInput->setAlignment(Qt::AlignCenter);
    m_usernameInput->setMinimumHeight(40);

    m_passwordInput = new QLineEdit(m_formPage);
    m_passwordInput->setObjectName("LoginInput");
    m_passwordInput->setEchoMode(QLineEdit::Password);
    m_passwordInput->setAlignment(Qt::AlignCenter);
    m_passwordInput->setMinimumHeight(40);
    m_passwordInput->installEventFilter(this);
    
    m_togglePasswordBtn = new QToolButton(m_passwordInput);
    m_togglePasswordBtn->setIcon(QIcon(":/icons/eye.svg"));
    m_togglePasswordBtn->setCursor(Qt::PointingHandCursor);
    m_togglePasswordBtn->setStyleSheet("QToolButton { border: none; background: transparent; padding: 0px; }");
    m_togglePasswordBtn->setFixedSize(30, 30);
    connect(m_togglePasswordBtn, &QToolButton::clicked, this, &LoginDialog::onTogglePasswordVisibility);

    m_actionBtn = new QPushButton(m_formPage);
    m_actionBtn->setObjectName("LoginBtn");
    m_actionBtn->setMinimumHeight(40);
    m_actionBtn->setCursor(Qt::PointingHandCursor);
    connect(m_actionBtn, &QPushButton::clicked, this, &LoginDialog::onLoginClicked);

    m_toggleModeBtn = new QPushButton(m_formPage);
    m_toggleModeBtn->setObjectName("ToggleModeBtn");
    m_toggleModeBtn->setCursor(Qt::PointingHandCursor);
    m_toggleModeBtn->setFlat(true);
    connect(m_toggleModeBtn, &QPushButton::clicked, this, &LoginDialog::onToggleModeClicked);

    m_statusContainer = new QWidget(m_formPage);
    QVBoxLayout *statusLayout = new QVBoxLayout(m_statusContainer);
    statusLayout->setContentsMargins(0, 0, 0, 0);
    statusLayout->setSpacing(8);

    m_statusLabel = new QLabel("", m_statusContainer);
    m_statusLabel->setObjectName("LoginStatus");
    m_statusLabel->setAlignment(Qt::AlignCenter);
    m_statusLabel->setWordWrap(true);
    m_statusLabel->hide();

    m_accountActionStatusWidget = new QWidget(m_statusContainer);
    QVBoxLayout *accountActionStatusLayout = new QVBoxLayout(m_accountActionStatusWidget);
    accountActionStatusLayout->setContentsMargins(0, 0, 0, 0);
    accountActionStatusLayout->setSpacing(3);

    MarqueeStatusLabel *accountActionLine1 = new MarqueeStatusLabel(m_accountActionStatusWidget);
    accountActionLine1->setObjectName("LoginAccountActionLine1");
    accountActionLine1->setEmphasized(true);
    m_accountActionLine1 = accountActionLine1;

    MarqueeStatusLabel *accountActionLine2 = new MarqueeStatusLabel(m_accountActionStatusWidget);
    accountActionLine2->setObjectName("LoginAccountActionLine2");
    m_accountActionLine2 = accountActionLine2;

    MarqueeStatusLabel *accountActionLine3 = new MarqueeStatusLabel(m_accountActionStatusWidget);
    accountActionLine3->setObjectName("LoginAccountActionLine3");
    m_accountActionLine3 = accountActionLine3;

    accountActionStatusLayout->addWidget(accountActionLine1);
    accountActionStatusLayout->addWidget(accountActionLine2);
    accountActionStatusLayout->addWidget(accountActionLine3);
    m_accountActionStatusWidget->hide();

    statusLayout->addWidget(m_statusLabel);
    statusLayout->addWidget(m_accountActionStatusWidget);

    formLayout->addWidget(m_titleLabel);
    formLayout->addWidget(m_subtitleLabel);
    formLayout->addStretch();
    formLayout->addWidget(m_statusContainer);
    formLayout->addWidget(m_usernameInput);
    formLayout->addWidget(m_passwordInput);
    formLayout->addWidget(m_actionBtn);
    formLayout->addWidget(m_toggleModeBtn);
    
    m_stackedWidget->addWidget(m_formPage);
    
    // --- Page 1: Loading Page ---
    m_loadingPage = new QWidget();
    QVBoxLayout *loadingLayout = new QVBoxLayout(m_loadingPage);
    loadingLayout->setContentsMargins(30, 25, 30, 25);
    loadingLayout->setSpacing(15);
    loadingLayout->setAlignment(Qt::AlignCenter);
    
    m_loadingIconLabel = new QLabel(m_loadingPage);
    m_loadingIconLabel->setPixmap(QPixmap(":/app.ico"));
    m_loadingIconLabel->setAlignment(Qt::AlignCenter);
    loadingLayout->addWidget(m_loadingIconLabel);
    
    m_loadingMessageLabel = new QLabel(m_loadingPage);
    m_loadingMessageLabel->setObjectName("LoadingMessage");
    m_loadingMessageLabel->setAlignment(Qt::AlignCenter);
    m_loadingMessageLabel->setWordWrap(true);
    loadingLayout->addWidget(m_loadingMessageLabel);
    
    m_loadingProgressBar = new QProgressBar(m_loadingPage);
    m_loadingProgressBar->setObjectName("LoadingProgressBar");
    m_loadingProgressBar->setTextVisible(false);
    m_loadingProgressBar->setFixedHeight(6);
    m_loadingProgressBar->setRange(0, 0); // Indeterminate
    loadingLayout->addWidget(m_loadingProgressBar);
    
    m_stackedWidget->addWidget(m_loadingPage);
    
    // Setup global Enter key shortcut for the dialog
    QShortcut *enterShortcut = new QShortcut(QKeySequence(Qt::Key_Return), this);
    enterShortcut->setContext(Qt::WidgetWithChildrenShortcut);
    connect(enterShortcut, &QShortcut::activated, m_actionBtn, &QPushButton::click);
    
    QShortcut *enterNumShortcut = new QShortcut(QKeySequence(Qt::Key_Enter), this);
    enterNumShortcut->setContext(Qt::WidgetWithChildrenShortcut);
    connect(enterNumShortcut, &QShortcut::activated, m_actionBtn, &QPushButton::click);
}

void LoginDialog::setMode(bool isLogin) {
    m_isLoginMode = isLogin;
    stopAccountActionCountdown();
    showAccountActionStatusWidget(false);
    m_statusLabel->hide();
    
    if (isLogin) {
        disconnect(m_actionBtn, &QPushButton::clicked, nullptr, nullptr);
        connect(m_actionBtn, &QPushButton::clicked, this, &LoginDialog::onLoginClicked);
    } else {
        disconnect(m_actionBtn, &QPushButton::clicked, nullptr, nullptr);
        connect(m_actionBtn, &QPushButton::clicked, this, &LoginDialog::onRegisterClicked);
    }
    
    updateTexts();
}

void LoginDialog::updateTexts() {
    LocalizationManager& loc = LocalizationManager::instance();
    
    m_titleLabel->setText(loc.getString("LoginDialog", "Title"));
    m_usernameInput->setPlaceholderText(loc.getString("LoginDialog", "UsernamePlaceholder"));
    m_passwordInput->setPlaceholderText(loc.getString("LoginDialog", "PasswordPlaceholder"));
    
    if (m_isLoginMode) {
        m_subtitleLabel->setText(loc.getString("LoginDialog", "SubtitleLogin"));
        m_actionBtn->setText(loc.getString("LoginDialog", "LoginBtn"));
        m_toggleModeBtn->setText(loc.getString("LoginDialog", "ToggleToRegister"));
    } else {
        m_subtitleLabel->setText(loc.getString("LoginDialog", "SubtitleRegister"));
        m_actionBtn->setText(loc.getString("LoginDialog", "RegisterBtn"));
        m_toggleModeBtn->setText(loc.getString("LoginDialog", "ToggleToLogin"));
    }
    
    m_loadingMessageLabel->setText(loc.getString("LoginDialog", "LoggingIn"));
}

void LoginDialog::onToggleModeClicked() {
    setMode(!m_isLoginMode);
}

void LoginDialog::onTogglePasswordVisibility() {
    if (m_passwordInput->echoMode() == QLineEdit::Password) {
        m_passwordInput->setEchoMode(QLineEdit::Normal);
        m_togglePasswordBtn->setIcon(QIcon(":/icons/eye-off.svg"));
    } else {
        m_passwordInput->setEchoMode(QLineEdit::Password);
        m_togglePasswordBtn->setIcon(QIcon(":/icons/eye.svg"));
    }
}

void LoginDialog::updateTheme() {
    bool isDark = ConfigManager::instance().isCurrentThemeDark();
    
    QString containerBg = isDark ? "#2C2C2E" : "#FFFFFF";
    QString textColor = isDark ? "#FFFFFF" : "#1D1D1F";
    QString secondaryTextColor = isDark ? "#8E8E93" : "#86868B";
    QString inputBg = isDark ? "#1C1C1E" : "#F2F2F7";
    QString inputBorder = isDark ? "#3A3A3C" : "#E5E5EA";
    QString btnBg = "#007AFF";
    QString btnHoverBg = "#0062CC";
    QString progressBg = isDark ? "#3A3A3C" : "#E5E5EA";
    QString progressChunk = "#007AFF";
    
    setStyleSheet(QString(
        "QWidget#LoginContainer {"
        "  background-color: %1;"
        "  border-radius: 12px;"
        "}"
        "QLabel#LoginTitle {"
        "  color: %2;"
        "}"
        "QLabel#LoginSubtitle {"
        "  color: %3;"
        "}"
        "QLineEdit#LoginInput {"
        "  background-color: %4;"
        "  color: %2;"
        "  border: 1px solid %5;"
        "  border-radius: 6px;"
        "  font-size: 14px;"
        "  font-family: monospace;"
        "}"
        "QLineEdit#LoginInput:focus {"
        "  border: 1px solid %6;"
        "}"
        "QPushButton#LoginBtn {"
        "  background-color: %6;"
        "  color: #FFFFFF;"
        "  border: none;"
        "  border-radius: 6px;"
        "  font-weight: bold;"
        "  font-size: 14px;"
        "}"
        "QPushButton#LoginBtn:hover {"
        "  background-color: %7;"
        "}"
        "QPushButton#LoginBtn:disabled {"
        "  background-color: %3;"
        "}"
        "QPushButton#ToggleModeBtn {"
        "  color: %6;"
        "  border: none;"
        "  background: transparent;"
        "}"
        "QPushButton#ToggleModeBtn:hover {"
        "  text-decoration: underline;"
        "}"
        "QLabel#LoginStatus {"
        "  color: #FF3B30;"
        "  font-size: 13px;"
        "  font-weight: 500;"
        "}"
        "QLabel#LoadingMessage {"
        "  color: %2;"
        "  font-size: 14px;"
        "  font-weight: 500;"
        "}"
        "QProgressBar#LoadingProgressBar {"
        "  background-color: %8;"
        "  border: none;"
        "  border-radius: 3px;"
        "}"
        "QProgressBar#LoadingProgressBar::chunk {"
        "  background-color: %9;"
        "  border-radius: 3px;"
        "}"
    ).arg(containerBg, textColor, secondaryTextColor, inputBg, inputBorder, btnBg, btnHoverBg, progressBg, progressChunk));
}

void LoginDialog::onLoginClicked() {
    QString username = m_usernameInput->text().trimmed();
    QString password = m_passwordInput->text();
    
    if (username.isEmpty() || password.isEmpty()) {
        setPlainStatusMessage(LocalizationManager::instance().getString("LoginDialog", "auth.login.missing_credentials"), "#FF3B30");
        return;
    }

    m_actionBtn->setEnabled(false);
    m_actionBtn->setText(LocalizationManager::instance().getString("LoginDialog", "Connecting"));
    m_usernameInput->setEnabled(false);
    m_passwordInput->setEnabled(false);
    m_toggleModeBtn->setEnabled(false);
    showAccountActionStatusWidget(false);
    m_statusLabel->hide();

    AuthManager::instance().login(username, password);
}

void LoginDialog::onRegisterClicked() {
    QString username = m_usernameInput->text().trimmed();
    QString password = m_passwordInput->text();

    if (username.isEmpty() || password.isEmpty()) {
        setPlainStatusMessage(LocalizationManager::instance().getString("LoginDialog", "auth.register.missing_fields"), "#FF3B30");
        return;
    }

    m_actionBtn->setEnabled(false);
    m_actionBtn->setText(LocalizationManager::instance().getString("LoginDialog", "Registering"));
    m_usernameInput->setEnabled(false);
    m_passwordInput->setEnabled(false);
    m_toggleModeBtn->setEnabled(false);
    showAccountActionStatusWidget(false);
    m_statusLabel->hide();

    QJsonObject json;
    json["username"] = username;
    json["password"] = password;
    json["hwid"] = AuthManager::instance().getHWID();

    HttpRequestOptions options = HttpClient::createJsonPost(
        QUrl(AuthManager::buildApiUrl("/api/v1/auth/register")),
        QJsonDocument(json).toJson(QJsonDocument::Compact)
    );
    HttpClient::addOrReplaceHeader(options, "User-Agent", "Mozilla/5.0 (Windows NT 10.0; Win64; x64) APEHOI4ToolStudio/1.0");
    HttpClient::addOrReplaceHeader(options, "Accept", "application/json, text/plain, */*");
    HttpClient::addOrReplaceHeader(options, "Accept-Language", "zh-CN,zh;q=0.9,en;q=0.8");
    options.category = HttpRequestCategory::Auth;
    options.timeoutMs = 20000;
    options.connectTimeoutMs = 5000;
    options.maxRetries = 1;
    options.retryOnHttp5xx = true;
    options.allowHttp11Fallback = true;
    options.allowIpv4Fallback = true;

    HttpClient::instance().send(options, this, [this](const HttpResponse& response) {
        onRegisterReply(response);
    });
}

void LoginDialog::onRegisterReply(const HttpResponse& response) {
    m_actionBtn->setEnabled(true);
    m_actionBtn->setText(LocalizationManager::instance().getString("LoginDialog", "RegisterBtn"));
    m_usernameInput->setEnabled(true);
    m_passwordInput->setEnabled(true);
    m_toggleModeBtn->setEnabled(true);

    if (!response.success) {
        const QString errorCode = response.statusCode > 0
            ? QString::number(response.statusCode)
            : QStringLiteral("0");
        const QString sanitizedErrorDetail = sanitizeSensitiveAuthEndpointText(response.errorMessage);
        const QString errorMsg = mapNetworkErrorToDisplayMessage(errorCode, sanitizedErrorDetail);

        setPlainStatusMessage(errorMsg, "#FF3B30");
        return;
    }

    const QJsonDocument doc = QJsonDocument::fromJson(response.body);
    QJsonObject obj = doc.object();
    const AuthResult authResult = AuthManager::instance().parseAuthResult(obj);
    const QString localizedMessage = AuthManager::instance().localizeAuthResultMessage(authResult);

    if (authResult.success) {
        setMode(true);

        setPlainStatusMessage(localizedMessage, "#34C759");

        // Auto login after register
        onLoginClicked();
    } else {
        setPlainStatusMessage(localizedMessage, "#FF3B30");
    }
}

void LoginDialog::onLoginSuccess() {
    stopAccountActionCountdown();
    emit loginSuccessful();
    hideLogin();
}

void LoginDialog::showLogin() {
    m_stackedWidget->setCurrentWidget(m_formPage);
    m_container->setFixedSize(400, 380);
    update(); // Trigger repaint for background alpha
    
    // Reset UI state
    m_actionBtn->setEnabled(true);
    m_usernameInput->setEnabled(true);
    m_passwordInput->setEnabled(true);
    m_toggleModeBtn->setEnabled(true);
    m_passwordInput->clear();
    stopAccountActionCountdown();
    showAccountActionStatusWidget(false);
    m_statusLabel->hide();
    
    // Restore button text based on current mode
    if (m_isLoginMode) {
        m_actionBtn->setText(LocalizationManager::instance().getString("LoginDialog", "LoginBtn"));
    } else {
        m_actionBtn->setText(LocalizationManager::instance().getString("LoginDialog", "RegisterBtn"));
    }

    if (parentWidget()) {
        raise();
        updatePosition();
    }
    show();
}

void LoginDialog::showAutoLoggingIn() {
    m_stackedWidget->setCurrentWidget(m_loadingPage);
    m_container->setFixedSize(350, 400);
    update(); // Trigger repaint for background alpha
    
    if (parentWidget()) {
        raise();
        updatePosition();
    }
    show();
}

void LoginDialog::hideLogin() {
    hide();
}

void LoginDialog::paintEvent(QPaintEvent *event) {
    Q_UNUSED(event);
    
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    
    QPainterPath path;
    QRectF r = rect();
    qreal radius = 9;
    
    path.addRoundedRect(r, radius, radius);
    
    int alpha = (m_stackedWidget->currentWidget() == m_loadingPage) ? 120 : 150;
    painter.fillPath(path, QColor(0, 0, 0, alpha));
}

bool LoginDialog::eventFilter(QObject *obj, QEvent *event) {
    if (obj == parentWidget() && event->type() == QEvent::Resize) {
        updatePosition();
    } else if (obj == m_passwordInput && event->type() == QEvent::Resize) {
        int btnWidth = m_togglePasswordBtn->width();
        int btnHeight = m_togglePasswordBtn->height();
        int inputHeight = m_passwordInput->height();
        int inputWidth = m_passwordInput->width();
        m_togglePasswordBtn->move(inputWidth - btnWidth - 5, (inputHeight - btnHeight) / 2);
    }
    return QWidget::eventFilter(obj, event);
}

void LoginDialog::updatePosition() {
    if (parentWidget()) {
        setGeometry(parentWidget()->rect());
        m_container->move(
            (width() - m_container->width()) / 2,
            (height() - m_container->height()) / 2
        );
    }
}

void LoginDialog::onLoginFailed(const QString& errorMsg) {
    // If we were in auto-login mode, switch back to form
    if (m_stackedWidget->currentWidget() == m_loadingPage) {
        m_stackedWidget->setCurrentWidget(m_formPage);
    }
    
    m_actionBtn->setEnabled(true);
    m_actionBtn->setText(LocalizationManager::instance().getString("LoginDialog", "LoginBtn"));
    m_usernameInput->setEnabled(true);
    m_passwordInput->setEnabled(true);
    m_toggleModeBtn->setEnabled(true);
    
    QString displayMsg = errorMsg;
    if (errorMsg.startsWith("NETWORK_ERROR:")) {
        const QString payload = errorMsg.mid(14); // Length of "NETWORK_ERROR:"
        const int separatorIndex = payload.indexOf(':');
        const QString errorCode = separatorIndex >= 0 ? payload.left(separatorIndex) : payload;
        const QString errorDetail = separatorIndex >= 0 ? payload.mid(separatorIndex + 1) : QString();
        displayMsg = mapNetworkErrorToDisplayMessage(errorCode, sanitizeSensitiveAuthEndpointText(errorDetail));
        stopAccountActionCountdown();
    } else {
        const AccountActionInfo accountActionInfo = AuthManager::instance().getAccountActionInfo();
        if (accountActionInfo.blocking) {
            startAccountActionCountdown(accountActionInfo);
            displayMsg = AuthManager::instance().localizeAccountActionMessage(m_pendingAccountActionInfo, "LoginDialog");
        } else {
            stopAccountActionCountdown();
        }
    }
    
    if (m_pendingAccountActionInfo.blocking) {
        refreshAccountActionStatusText();
    } else {
        setPlainStatusMessage(displayMsg, "#FF3B30");
    }
}

void LoginDialog::setPlainStatusMessage(const QString& message, const QString& color) {
    showAccountActionStatusWidget(false);
    m_statusLabel->setText(message);
    m_statusLabel->setStyleSheet(QString("color: %1;").arg(color));
    m_statusLabel->setVisible(!message.isEmpty());
}

void LoginDialog::showAccountActionStatusWidget(bool show) {
    m_accountActionStatusWidget->setVisible(show);
    if (show) {
        m_statusLabel->hide();
    }
}

void LoginDialog::startAccountActionCountdown(const AccountActionInfo& info) {
    m_pendingAccountActionInfo = info;
    if (!info.permanent && info.remainingSeconds > 0) {
        m_accountActionExpireAtUtc = QDateTime::currentDateTimeUtc().addSecs(info.remainingSeconds);
        if (!m_accountActionCountdownTimer->isActive()) {
            m_accountActionCountdownTimer->start();
        }
    } else {
        m_accountActionExpireAtUtc = QDateTime();
        m_accountActionCountdownTimer->stop();
    }
}

void LoginDialog::stopAccountActionCountdown() {
    m_accountActionCountdownTimer->stop();
    m_pendingAccountActionInfo = AccountActionInfo();
    m_accountActionExpireAtUtc = QDateTime();
}

void LoginDialog::refreshAccountActionStatusText() {
    if (!m_pendingAccountActionInfo.blocking) {
        return;
    }

    if (!m_pendingAccountActionInfo.permanent && m_accountActionExpireAtUtc.isValid()) {
        const qint64 remainingSeconds = qMax<qint64>(0, QDateTime::currentDateTimeUtc().secsTo(m_accountActionExpireAtUtc));
        m_pendingAccountActionInfo.remainingSeconds = remainingSeconds;
        if (remainingSeconds <= 0) {
            m_accountActionCountdownTimer->stop();
        }
    }

    const QString localizedText = AuthManager::instance().localizeAccountActionMessage(m_pendingAccountActionInfo, "LoginDialog");
    const QStringList lines = localizedText.split('\n');

    MarqueeStatusLabel *line1 = static_cast<MarqueeStatusLabel *>(m_accountActionLine1);
    MarqueeStatusLabel *line2 = static_cast<MarqueeStatusLabel *>(m_accountActionLine2);
    MarqueeStatusLabel *line3 = static_cast<MarqueeStatusLabel *>(m_accountActionLine3);

    if (line1) {
        line1->setTextColor(QColor(255, 59, 48));
        line1->setContent(lines.value(0));
    }
    if (line2) {
        line2->setTextColor(QColor(255, 59, 48));
        line2->setContent(lines.value(1));
    }
    if (line3) {
        line3->setTextColor(QColor(255, 59, 48));
        line3->setContent(lines.value(2));
    }

    showAccountActionStatusWidget(true);
}

void LoginDialog::onAccountActionCountdownTick() {
    refreshAccountActionStatusText();
}