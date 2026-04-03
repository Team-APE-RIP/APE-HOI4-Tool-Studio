//-------------------------------------------------------------------------------------
// HttpClient.h -- Part of APE HOI4 Tool Studio
//
// Copyright (C) 2026 Team APE:RIP. All rights reserved.
// Licensed under the Team APE:RIP Source Code License Agreement.
//
// https://github.com/Team-APE-RIP/APE-HOI4-Tool-Studio/
//-------------------------------------------------------------------------------------
#ifndef HTTPCLIENT_H
#define HTTPCLIENT_H

#include <QObject>
#include <QByteArray>
#include <QHash>
#include <QList>
#include <QPointer>
#include <QQueue>
#include <QString>
#include <QUrl>
#include <QVariantMap>
#include <functional>
#include <memory>
#include <mutex>
#include <thread>

struct HttpHeader {
    QByteArray name;
    QByteArray value;
};

enum class HttpTrustMode {
    SystemFirstThenBundleFallback,
    SystemOnly,
    BundleOnly
};

enum class HttpBackendPreference {
    Auto,
    QtNetwork,
    Libcurl
};

enum class HttpVersionPolicy {
    Automatic,
    PreferHttp2,
    ForceHttp2,
    ForceHttp11
};

enum class HttpIpResolvePolicy {
    Any,
    PreferIpv4,
    PreferIpv6,
    ForceIpv4,
    ForceIpv6
};

enum class HttpRequestCategory {
    General,
    Auth,
    Ping,
    Manifest,
    AdvertisementImage,
    UpdateDownload,
    LargeDownload
};

struct HttpRequestOptions {
    QUrl url;
    QByteArray method = "GET";
    QList<HttpHeader> headers;
    QByteArray body;

    int timeoutMs = 30000;
    int connectTimeoutMs = 10000;
    int lowSpeedLimitBytesPerSecond = 0;
    int lowSpeedTimeSeconds = 0;

    int maxRetries = 0;
    int retryBackoffMs = 500;
    bool retryOnTimeout = true;
    bool retryOnTransientSslFailure = true;
    bool retryOnHttp5xx = false;

    bool followRedirects = true;
    bool applyPinnedSslForQt = true;
    HttpTrustMode trustMode = HttpTrustMode::SystemFirstThenBundleFallback;
    HttpBackendPreference backendPreference = HttpBackendPreference::Auto;
    HttpVersionPolicy httpVersionPolicy = HttpVersionPolicy::PreferHttp2;
    HttpIpResolvePolicy ipResolvePolicy = HttpIpResolvePolicy::Any;
    HttpRequestCategory category = HttpRequestCategory::General;

    bool allowHttp11Fallback = true;
    bool allowIpv4Fallback = true;
    bool enableTcpKeepAlive = true;
    bool enableDnsCache = true;
    bool enableConnectionReuse = true;
    bool collectTimingInfo = true;
};

struct HttpResponse {
    bool success = false;
    int statusCode = 0;
    QByteArray body;
    QList<HttpHeader> headers;
    QString errorMessage;
    QString backendName;
    QVariantMap diagnostics;
};

class HttpClient : public QObject {
    Q_OBJECT

public:
    using ResponseHandler = std::function<void(const HttpResponse&)>;
    using ProgressHandler = std::function<void(qint64, qint64)>;

    static HttpClient& instance();

    void send(const HttpRequestOptions& options,
              QObject* context,
              const ResponseHandler& handler,
              const ProgressHandler& progressHandler = ProgressHandler());

    void downloadToFile(const HttpRequestOptions& options,
                        const QString& filePath,
                        QObject* context,
                        const ResponseHandler& handler,
                        const ProgressHandler& progressHandler = ProgressHandler());

    void refreshCaBundleInBackground(const QUrl& metadataUrl,
                                     const QUrl& downloadUrl,
                                     QObject* context = nullptr,
                                     const ResponseHandler& handler = ResponseHandler());
    void warmUpConnection(const QUrl& url,
                          QObject* context = nullptr,
                          const ResponseHandler& handler = ResponseHandler());
    void shutdown();
#ifdef APE_USE_LIBCURL
    void stopCurlWorker();
#endif

    static HttpRequestOptions createGet(const QUrl& url);
    static HttpRequestOptions createJsonPost(const QUrl& url, const QByteArray& jsonBody);
    static void addOrReplaceHeader(HttpRequestOptions& options, const QByteArray& name, const QByteArray& value);
    static QString activeBackendName();

private:
    struct RequestTask {
        qint64 id = 0;
        HttpRequestOptions options;
        QString filePath;
        bool writeToFile = false;
        QPointer<QObject> context;
        ResponseHandler handler;
        ProgressHandler progressHandler;
    };

    explicit HttpClient(QObject* parent = nullptr);
    ~HttpClient() override;

    HttpClient(const HttpClient&) = delete;
    HttpClient& operator=(const HttpClient&) = delete;

    void sendWithQt(const HttpRequestOptions& options,
                    QObject* context,
                    const ResponseHandler& handler,
                    const ProgressHandler& progressHandler);

    void downloadWithQt(const HttpRequestOptions& options,
                        const QString& filePath,
                        QObject* context,
                        const ResponseHandler& handler,
                        const ProgressHandler& progressHandler);

#ifdef APE_USE_LIBCURL
    void sendWithCurl(const HttpRequestOptions& options,
                      QObject* context,
                      const ResponseHandler& handler,
                      const ProgressHandler& progressHandler);

    void downloadWithCurl(const HttpRequestOptions& options,
                          const QString& filePath,
                          QObject* context,
                          const ResponseHandler& handler,
                          const ProgressHandler& progressHandler);

    void enqueueCurlTask(RequestTask task);
    void ensureCurlWorkerStarted();
    void curlWorkerLoop();

    class CurlRuntime;
    std::unique_ptr<CurlRuntime> m_curlRuntime;
    std::thread m_curlWorkerThread;
    std::mutex m_curlQueueMutex;
    QQueue<RequestTask> m_pendingCurlTasks;
    bool m_curlWorkerRunning = false;
    bool m_curlStopRequested = false;
    qint64 m_nextRequestId = 0;
#endif

    class QNetworkAccessManager* m_qtNetworkManager;
    bool m_shutdownRequested = false;
};

#endif // HTTPCLIENT_H