//-------------------------------------------------------------------------------------
// HttpClient.cpp -- Part of APE HOI4 Tool Studio
//
// Copyright (C) 2026 Team APE:RIP. All rights reserved.
// Licensed under the Team APE:RIP Source Code License Agreement.
//
// https://github.com/Team-APE-RIP/APE-HOI4-Tool-Studio/
//-------------------------------------------------------------------------------------
#include "HttpClient.h"

#include "AuthManager.h"
#include "Logger.h"
#include "SslConfig.h"

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QEventLoop>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMetaObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPointer>
#include <QThread>
#include <QTimer>

#include <chrono>
#include <condition_variable>
#include <limits>
#include <memory>
#include <thread>
#include <unordered_map>
#include <utility>

#ifdef APE_USE_LIBCURL
#include <curl/curl.h>
#endif

namespace {
QNetworkRequest buildQtRequest(const HttpRequestOptions& options) {
    QNetworkRequest request(options.url);
    if (options.followRedirects) {
        request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
    } else {
        request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::ManualRedirectPolicy);
    }

    for (const HttpHeader& header : options.headers) {
        request.setRawHeader(header.name, header.value);
    }

    if (options.applyPinnedSslForQt && options.url.scheme().compare("https", Qt::CaseInsensitive) == 0) {
        SslConfig::applyPinnedConfiguration(request);
    }

    return request;
}

QString bundledCaPath() {
    const QString appDir = QCoreApplication::applicationDirPath();
    return QDir(appDir).filePath("etc/ssl/certs/ca-bundle.crt");
}

QByteArray resolveBundledCaPath() {
    const QString path = bundledCaPath();
    if (QFileInfo::exists(path)) {
        return QDir::toNativeSeparators(path).toUtf8();
    }

    return QByteArray();
}

QByteArray fileSha256(const QString& filePath) {
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        return QByteArray();
    }

    QCryptographicHash hash(QCryptographicHash::Sha256);
    while (!file.atEnd()) {
        const QByteArray chunk = file.read(64 * 1024);
        if (chunk.isEmpty() && file.error() != QFile::NoError) {
            return QByteArray();
        }
        hash.addData(chunk);
    }

    return hash.result().toHex();
}

bool isSslFailureMessage(const QString& errorMessage) {
    const QString lowered = errorMessage.toLower();
    return lowered.contains("ssl")
        || lowered.contains("tls")
        || lowered.contains("certificate")
        || lowered.contains("trust anchor")
        || lowered.contains("ca")
        || lowered.contains("verify")
        || lowered.contains("handshake")
        || lowered.contains("peer")
        || lowered.contains("schannel");
}

bool isLikelyTimeoutMessage(const QString& errorMessage) {
    const QString lowered = errorMessage.toLower();
    return lowered.contains("timed out")
        || lowered.contains("timeout")
        || lowered.contains("operation too slow")
        || lowered.contains("connection timeout");
}

HttpResponse buildResponseFromReply(QNetworkReply* reply, const QByteArray& body) {
    HttpResponse response;
    response.backendName = HttpClient::activeBackendName();

    if (!reply) {
        response.errorMessage = QStringLiteral("Null network reply");
        return response;
    }

    response.statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    response.body = body;
    response.success = (reply->error() == QNetworkReply::NoError);

    const QList<QByteArray> headerNames = reply->rawHeaderList();
    for (const QByteArray& name : headerNames) {
        response.headers.append(HttpHeader{name, reply->rawHeader(name)});
    }

    if (!response.success) {
        response.errorMessage = reply->errorString();
    }

    return response;
}

void invokeHandler(QObject* context,
                   const HttpClient::ResponseHandler& handler,
                   const HttpResponse& response) {
    if (!handler) {
        return;
    }

    QPointer<QObject> safeContext(context);
    if (safeContext) {
        QMetaObject::invokeMethod(
            safeContext,
            [safeContext, handler, response]() {
                if (!safeContext) {
                    return;
                }
                handler(response);
            },
            Qt::QueuedConnection
        );
        return;
    }

    handler(response);
}

void invokeProgress(QObject* context,
                    const HttpClient::ProgressHandler& progressHandler,
                    qint64 received,
                    qint64 total) {
    if (!progressHandler) {
        return;
    }

    QPointer<QObject> safeContext(context);
    if (safeContext) {
        QMetaObject::invokeMethod(
            safeContext,
            [safeContext, progressHandler, received, total]() {
                if (!safeContext) {
                    return;
                }
                progressHandler(received, total);
            },
            Qt::QueuedConnection
        );
        return;
    }

    progressHandler(received, total);
}

QString determineTrustSourceName(HttpTrustMode trustMode) {
    switch (trustMode) {
    case HttpTrustMode::SystemOnly:
        return QStringLiteral("system");
    case HttpTrustMode::BundleOnly:
        return QStringLiteral("bundle");
    case HttpTrustMode::SystemFirstThenBundleFallback:
        return QStringLiteral("system-first-fallback-bundle");
    }
    return QStringLiteral("unknown");
}

QString determineCategoryName(HttpRequestCategory category) {
    switch (category) {
    case HttpRequestCategory::General:
        return QStringLiteral("general");
    case HttpRequestCategory::Auth:
        return QStringLiteral("auth");
    case HttpRequestCategory::Ping:
        return QStringLiteral("ping");
    case HttpRequestCategory::Manifest:
        return QStringLiteral("manifest");
    case HttpRequestCategory::AdvertisementImage:
        return QStringLiteral("advertisement_image");
    case HttpRequestCategory::UpdateDownload:
        return QStringLiteral("update_download");
    case HttpRequestCategory::LargeDownload:
        return QStringLiteral("large_download");
    }
    return QStringLiteral("general");
}

bool isHeavyTransferCategory(HttpRequestCategory category) {
    switch (category) {
    case HttpRequestCategory::AdvertisementImage:
    case HttpRequestCategory::UpdateDownload:
    case HttpRequestCategory::LargeDownload:
        return true;
    case HttpRequestCategory::General:
    case HttpRequestCategory::Auth:
    case HttpRequestCategory::Ping:
    case HttpRequestCategory::Manifest:
        return false;
    }
    return false;
}

int requestPriorityForCategory(HttpRequestCategory category) {
    switch (category) {
    case HttpRequestCategory::Ping:
        return 600;
    case HttpRequestCategory::Auth:
        return 550;
    case HttpRequestCategory::Manifest:
        return 500;
    case HttpRequestCategory::AdvertisementImage:
        return 250;
    case HttpRequestCategory::UpdateDownload:
        return 200;
    case HttpRequestCategory::LargeDownload:
        return 150;
    case HttpRequestCategory::General:
    default:
        return 50;
    }
}

QString normalizedHostKey(const QUrl& url) {
    if (!url.isValid()) {
        return QStringLiteral("<invalid-host>");
    }

    const QString scheme = url.scheme().toLower();
    const QString host = url.host().toLower();
    const int defaultPort = scheme.compare("https", Qt::CaseInsensitive) == 0 ? 443 : 80;
    const int port = url.port(defaultPort);

    return QStringLiteral("%1://%2:%3")
        .arg(scheme.isEmpty() ? QStringLiteral("http") : scheme,
             host.isEmpty() ? QStringLiteral("<no-host>") : host,
             QString::number(port));
}

qint64 currentSteadyTimeMs() {
    return static_cast<qint64>(std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count());
}

QString redactUrlForLogs(const QUrl& url) {
    if (!url.isValid()) {
        return QStringLiteral("<invalid-url>");
    }

    const QString path = url.path().isEmpty() ? QStringLiteral("/") : url.path();
    if (path.startsWith("/api/", Qt::CaseInsensitive)) {
        return QStringLiteral("<redacted-api>%1").arg(path);
    }
    if (path.startsWith("/ads/", Qt::CaseInsensitive) || path.startsWith("/versions/", Qt::CaseInsensitive)) {
        return QStringLiteral("<redacted-static>%1").arg(path);
    }

    return QStringLiteral("<redacted-url>%1").arg(path);
}

QString sanitizeErrorMessageForLogs(const QString& message) {
    return AuthManager::sanitizeSensitiveApiText(message);
}

QUrl determineCaDownloadUrl(const QJsonObject& metaObject, const QUrl& defaultUrl) {
    const QString downloadUrlValue = metaObject.value("download_url").toString();
    if (!downloadUrlValue.isEmpty()) {
        const QUrl candidate(downloadUrlValue);
        if (candidate.isValid() && candidate.isRelative()) {
            return defaultUrl.resolved(candidate);
        }
        if (candidate.isValid()) {
            return candidate;
        }
    }

    return defaultUrl;
}

#ifdef APE_USE_LIBCURL
void applyCurlTrustMode(CURL* curl, HttpTrustMode trustMode) {
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 2L);

#ifdef CURLSSLOPT_NATIVE_CA
    if (trustMode == HttpTrustMode::SystemOnly || trustMode == HttpTrustMode::SystemFirstThenBundleFallback) {
        curl_easy_setopt(curl, CURLOPT_SSL_OPTIONS, CURLSSLOPT_NATIVE_CA);
    }
#endif

    const QByteArray bundledPath = resolveBundledCaPath();
    if (trustMode == HttpTrustMode::BundleOnly && !bundledPath.isEmpty()) {
        curl_easy_setopt(curl, CURLOPT_CAINFO, bundledPath.constData());
    }
}

void applyCurlHttpVersionPolicy(CURL* curl, HttpVersionPolicy policy) {
#ifdef CURL_HTTP_VERSION_2TLS
    if (policy == HttpVersionPolicy::PreferHttp2) {
        curl_easy_setopt(curl, CURLOPT_HTTP_VERSION, CURL_HTTP_VERSION_2TLS);
        return;
    }
#endif
#ifdef CURL_HTTP_VERSION_2_0
    if (policy == HttpVersionPolicy::ForceHttp2) {
        curl_easy_setopt(curl, CURLOPT_HTTP_VERSION, CURL_HTTP_VERSION_2_0);
        return;
    }
#endif
#ifdef CURL_HTTP_VERSION_1_1
    if (policy == HttpVersionPolicy::ForceHttp11) {
        curl_easy_setopt(curl, CURLOPT_HTTP_VERSION, CURL_HTTP_VERSION_1_1);
        return;
    }
#endif
#ifdef CURL_HTTP_VERSION_NONE
    curl_easy_setopt(curl, CURLOPT_HTTP_VERSION, CURL_HTTP_VERSION_NONE);
#else
    Q_UNUSED(policy);
#endif
}

void applyCurlIpResolvePolicy(CURL* curl, HttpIpResolvePolicy policy) {
#ifdef CURLOPT_IPRESOLVE
    switch (policy) {
    case HttpIpResolvePolicy::PreferIpv4:
    case HttpIpResolvePolicy::ForceIpv4:
        curl_easy_setopt(curl, CURLOPT_IPRESOLVE, CURL_IPRESOLVE_V4);
        break;
    case HttpIpResolvePolicy::PreferIpv6:
    case HttpIpResolvePolicy::ForceIpv6:
        curl_easy_setopt(curl, CURLOPT_IPRESOLVE, CURL_IPRESOLVE_V6);
        break;
    case HttpIpResolvePolicy::Any:
    default:
        curl_easy_setopt(curl, CURLOPT_IPRESOLVE, CURL_IPRESOLVE_WHATEVER);
        break;
    }
#else
    Q_UNUSED(policy);
#endif
}

QString httpVersionName(long httpVersion) {
    switch (httpVersion) {
#ifdef CURL_HTTP_VERSION_1_0
    case CURL_HTTP_VERSION_1_0:
        return QStringLiteral("1.0");
#endif
#ifdef CURL_HTTP_VERSION_1_1
    case CURL_HTTP_VERSION_1_1:
        return QStringLiteral("1.1");
#endif
#ifdef CURL_HTTP_VERSION_2_0
    case CURL_HTTP_VERSION_2_0:
        return QStringLiteral("2");
#endif
#ifdef CURL_HTTP_VERSION_2TLS
    case CURL_HTTP_VERSION_2TLS:
        return QStringLiteral("2tls");
#endif
#ifdef CURL_HTTP_VERSION_3
    case CURL_HTTP_VERSION_3:
        return QStringLiteral("3");
#endif
    default:
        return QStringLiteral("unknown");
    }
}

QString classifyCurlError(CURLcode code, long statusCode, const QString& errorMessage) {
    if (code == CURLE_OK && (statusCode == 0 || statusCode < 400)) {
        return QStringLiteral("success");
    }

    if (statusCode >= 500 && statusCode <= 599) {
        return QStringLiteral("http_5xx");
    }

    switch (code) {
    case CURLE_OPERATION_TIMEDOUT:
        return QStringLiteral("timeout");
    case CURLE_COULDNT_RESOLVE_HOST:
        return QStringLiteral("dns");
    case CURLE_COULDNT_CONNECT:
        return QStringLiteral("connect");
    case CURLE_PEER_FAILED_VERIFICATION:
    case CURLE_SSL_CONNECT_ERROR:
    case CURLE_SSL_CERTPROBLEM:
    case CURLE_SSL_CIPHER:
    case CURLE_SSL_ISSUER_ERROR:
    case CURLE_USE_SSL_FAILED:
        return QStringLiteral("ssl");
#ifdef CURLE_HTTP2
    case CURLE_HTTP2:
        return QStringLiteral("http2");
#endif
    default:
        break;
    }

    if (isSslFailureMessage(errorMessage)) {
        return QStringLiteral("ssl");
    }
    if (isLikelyTimeoutMessage(errorMessage)) {
        return QStringLiteral("timeout");
    }
    return QStringLiteral("network");
}

void collectCurlTimings(CURL* curl, QVariantMap& diagnostics) {
    if (!curl) {
        return;
    }

    curl_off_t nameLookupUs = 0;
    curl_off_t connectUs = 0;
    curl_off_t appConnectUs = 0;
    curl_off_t startTransferUs = 0;
    curl_off_t totalUs = 0;
    curl_off_t redirectUs = 0;
    long responseCode = 0;
    long httpVersion = 0;
    char* primaryIp = nullptr;
    char* localIp = nullptr;

#ifdef CURLINFO_NAMELOOKUP_TIME_T
    curl_easy_getinfo(curl, CURLINFO_NAMELOOKUP_TIME_T, &nameLookupUs);
#endif
#ifdef CURLINFO_CONNECT_TIME_T
    curl_easy_getinfo(curl, CURLINFO_CONNECT_TIME_T, &connectUs);
#endif
#ifdef CURLINFO_APPCONNECT_TIME_T
    curl_easy_getinfo(curl, CURLINFO_APPCONNECT_TIME_T, &appConnectUs);
#endif
#ifdef CURLINFO_STARTTRANSFER_TIME_T
    curl_easy_getinfo(curl, CURLINFO_STARTTRANSFER_TIME_T, &startTransferUs);
#endif
#ifdef CURLINFO_TOTAL_TIME_T
    curl_easy_getinfo(curl, CURLINFO_TOTAL_TIME_T, &totalUs);
#endif
#ifdef CURLINFO_REDIRECT_TIME_T
    curl_easy_getinfo(curl, CURLINFO_REDIRECT_TIME_T, &redirectUs);
#endif
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &responseCode);
#ifdef CURLINFO_HTTP_VERSION
    curl_easy_getinfo(curl, CURLINFO_HTTP_VERSION, &httpVersion);
#endif
#ifdef CURLINFO_PRIMARY_IP
    curl_easy_getinfo(curl, CURLINFO_PRIMARY_IP, &primaryIp);
#endif
#ifdef CURLINFO_LOCAL_IP
    curl_easy_getinfo(curl, CURLINFO_LOCAL_IP, &localIp);
#endif

    diagnostics.insert(QStringLiteral("dns_ms"), static_cast<qlonglong>(nameLookupUs / 1000));
    diagnostics.insert(QStringLiteral("connect_ms"), static_cast<qlonglong>(connectUs / 1000));
    diagnostics.insert(QStringLiteral("tls_ms"), static_cast<qlonglong>(appConnectUs / 1000));
    diagnostics.insert(QStringLiteral("first_byte_ms"), static_cast<qlonglong>(startTransferUs / 1000));
    diagnostics.insert(QStringLiteral("total_ms"), static_cast<qlonglong>(totalUs / 1000));
    diagnostics.insert(QStringLiteral("redirect_ms"), static_cast<qlonglong>(redirectUs / 1000));
    diagnostics.insert(QStringLiteral("status_code"), static_cast<int>(responseCode));
    diagnostics.insert(QStringLiteral("http_version"), httpVersionName(httpVersion));
    diagnostics.insert(QStringLiteral("primary_ip"), primaryIp ? QString::fromUtf8(primaryIp) : QString());
    diagnostics.insert(QStringLiteral("local_ip"), localIp ? QString::fromUtf8(localIp) : QString());
}

bool shouldRetryResponse(const HttpRequestOptions& options,
                         CURLcode code,
                         long statusCode,
                         const QString& errorMessage,
                         int attemptIndex) {
    if (attemptIndex >= options.maxRetries) {
        return false;
    }

    if (statusCode >= 500 && statusCode <= 599) {
        return options.retryOnHttp5xx;
    }

    if (code == CURLE_OPERATION_TIMEDOUT) {
        return options.retryOnTimeout;
    }

#ifdef CURLE_HTTP2
    if (code == CURLE_HTTP2) {
        return true;
    }
#endif

    if (code == CURLE_COULDNT_CONNECT || code == CURLE_SEND_ERROR || code == CURLE_RECV_ERROR) {
        return true;
    }

    if (options.retryOnTransientSslFailure && isSslFailureMessage(errorMessage)) {
        return true;
    }

    return false;
}
#endif
}

#ifdef APE_USE_LIBCURL
class HttpClient::CurlRuntime {
public:
    explicit CurlRuntime(HttpClient* owner)
        : m_owner(owner) {
        m_multiHandle = curl_multi_init();
        m_shareHandle = curl_share_init();

#ifdef CURL_LOCK_DATA_DNS
        if (m_shareHandle) {
            curl_share_setopt(m_shareHandle, CURLSHOPT_SHARE, CURL_LOCK_DATA_DNS);
        }
#endif
#ifdef CURL_LOCK_DATA_SSL_SESSION
        if (m_shareHandle) {
            curl_share_setopt(m_shareHandle, CURLSHOPT_SHARE, CURL_LOCK_DATA_SSL_SESSION);
        }
#endif
#ifdef CURL_LOCK_DATA_CONNECT
        if (m_shareHandle) {
            curl_share_setopt(m_shareHandle, CURLSHOPT_SHARE, CURL_LOCK_DATA_CONNECT);
        }
#endif

        m_worker = std::thread([this]() {
            workerLoop();
        });
    }

    ~CurlRuntime() {
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_stopRequested = true;
            m_pendingRequests.clear();
        }
        m_cv.notify_all();
        wakeupMulti();

        if (m_worker.joinable()) {
            m_worker.join();
        }

        for (auto& pair : m_activeRequests) {
            if (m_multiHandle && pair.first) {
                curl_multi_remove_handle(m_multiHandle, pair.first);
            }
            cleanupActiveRequest(*pair.second, true);
        }
        m_activeRequests.clear();

        if (m_multiHandle) {
            curl_multi_cleanup(m_multiHandle);
            m_multiHandle = nullptr;
        }
        if (m_shareHandle) {
            curl_share_cleanup(m_shareHandle);
            m_shareHandle = nullptr;
        }
    }

    void enqueue(HttpClient::RequestTask task) {
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_pendingRequests.enqueue(std::move(task));
        }
        m_cv.notify_one();
        wakeupMulti();
    }

private:
    struct ActiveRequest {
        HttpClient::RequestTask task;
        CURL* easyHandle = nullptr;
        curl_slist* headerList = nullptr;
        QByteArray responseBody;
        QList<HttpHeader> responseHeaders;
        QFile outputFile;
        char errorBuffer[CURL_ERROR_SIZE] = {0};

        QString hostKey;
        HttpTrustMode currentTrustMode = HttpTrustMode::SystemOnly;
        HttpVersionPolicy currentHttpPolicy = HttpVersionPolicy::PreferHttp2;
        HttpIpResolvePolicy currentIpPolicy = HttpIpResolvePolicy::Any;
        bool usedHttp11Fallback = false;
        bool usedIpv4Fallback = false;
        int attemptIndex = 0;
    };

    struct HostHealthStats {
        int totalRequests = 0;
        int successfulRequests = 0;
        int timeoutFailures = 0;
        int connectFailures = 0;
        int dnsFailures = 0;
        int sslFailures = 0;
        int http5xxFailures = 0;
        int consecutiveFailures = 0;
        qint64 rollingTotalMs = 0;
        qint64 cooldownUntilMs = 0;
        QString lastErrorClass;
    };

    static size_t writeToBuffer(char* ptr, size_t size, size_t nmemb, void* userdata) {
        const size_t totalBytes = size * nmemb;
        ActiveRequest* request = static_cast<ActiveRequest*>(userdata);
        if (!request) {
            return 0;
        }

        request->responseBody.append(ptr, static_cast<int>(totalBytes));
        return totalBytes;
    }

    static size_t writeToFile(char* ptr, size_t size, size_t nmemb, void* userdata) {
        const size_t totalBytes = size * nmemb;
        ActiveRequest* request = static_cast<ActiveRequest*>(userdata);
        if (!request || !request->outputFile.isOpen()) {
            return 0;
        }

        const qint64 written = request->outputFile.write(ptr, static_cast<qint64>(totalBytes));
        if (written < 0) {
            return 0;
        }

        return static_cast<size_t>(written);
    }

    static size_t headerCallback(char* buffer, size_t size, size_t nitems, void* userdata) {
        const size_t totalBytes = size * nitems;
        ActiveRequest* request = static_cast<ActiveRequest*>(userdata);
        if (!request) {
            return totalBytes;
        }

        QByteArray rawLine(buffer, static_cast<int>(totalBytes));
        rawLine = rawLine.trimmed();

        if (rawLine.isEmpty() || rawLine.startsWith("HTTP/")) {
            return totalBytes;
        }

        const int separatorIndex = rawLine.indexOf(':');
        if (separatorIndex <= 0) {
            return totalBytes;
        }

        const QByteArray name = rawLine.left(separatorIndex).trimmed();
        const QByteArray value = rawLine.mid(separatorIndex + 1).trimmed();
        request->responseHeaders.append(HttpHeader{name, value});

        return totalBytes;
    }

    static int progressCallback(void* clientp,
                                curl_off_t dltotal,
                                curl_off_t dlnow,
                                curl_off_t,
                                curl_off_t) {
        ActiveRequest* request = static_cast<ActiveRequest*>(clientp);
        if (!request) {
            return 0;
        }

        invokeProgress(request->task.context,
                       request->task.progressHandler,
                       static_cast<qint64>(dlnow),
                       static_cast<qint64>(dltotal));
        return 0;
    }

    int taskPriority(const HttpClient::RequestTask& task) const {
        int priority = requestPriorityForCategory(task.options.category);
        const QByteArray method = task.options.method.trimmed().toUpper();
        if (method == "HEAD") {
            priority -= 20;
        }
        if (task.writeToFile) {
            priority -= 10;
        }
        return priority;
    }

    QString hostKeyForTask(const HttpClient::RequestTask& task) const {
        return normalizedHostKey(task.options.url);
    }

    int activeRequestsForHost(const QString& hostKey) const {
        int count = 0;
        for (const auto& pair : m_activeRequests) {
            if (hostKeyForTask(pair.second->task) == hostKey) {
                ++count;
            }
        }
        return count;
    }

    int activeHeavyTransfers() const {
        int count = 0;
        for (const auto& pair : m_activeRequests) {
            if (isHeavyTransferCategory(pair.second->task.options.category)) {
                ++count;
            }
        }
        return count;
    }

    int activeInteractiveRequests() const {
        int count = 0;
        for (const auto& pair : m_activeRequests) {
            if (isInteractiveCategory(pair.second->task.options.category)) {
                ++count;
            }
        }
        return count;
    }

    int activeBulkRequests() const {
        int count = 0;
        for (const auto& pair : m_activeRequests) {
            if (isHeavyTransferCategory(pair.second->task.options.category)) {
                ++count;
            }
        }
        return count;
    }

    int hostConcurrencyLimitForTask(const HttpClient::RequestTask& task) const {
        return isHeavyTransferCategory(task.options.category)
            ? m_maxHeavyRequestsPerHost
            : m_maxInteractiveRequestsPerHost;
    }

    bool isInteractiveCategory(HttpRequestCategory category) const {
        return !isHeavyTransferCategory(category);
    }

    bool hasPendingInteractiveTask() const {
        for (const HttpClient::RequestTask& task : m_pendingRequests) {
            if (isInteractiveCategory(task.options.category)) {
                return true;
            }
        }
        return false;
    }

    const HostHealthStats* hostStatsForTask(const HttpClient::RequestTask& task) const {
        const QString hostKey = hostKeyForTask(task);
        auto it = m_hostHealthStats.find(hostKey);
        if (it == m_hostHealthStats.end()) {
            return nullptr;
        }
        return &it->second;
    }

    int hostHealthScore(const HostHealthStats& stats) const {
        int score = 100;
        score -= stats.timeoutFailures * 6;
        score -= stats.connectFailures * 5;
        score -= stats.dnsFailures * 5;
        score -= stats.sslFailures * 4;
        score -= stats.http5xxFailures * 2;
        score -= stats.consecutiveFailures * 10;

        if (stats.rollingTotalMs > 8000) {
            score -= 12;
        } else if (stats.rollingTotalMs > 4000) {
            score -= 6;
        } else if (stats.rollingTotalMs > 1500) {
            score -= 2;
        }

        return qBound(0, score, 100);
    }

    int adaptiveHostConcurrencyLimitForTask(const HttpClient::RequestTask& task) const {
        int limit = hostConcurrencyLimitForTask(task);
        const HostHealthStats* stats = hostStatsForTask(task);
        if (!stats || stats->totalRequests < 3) {
            return limit;
        }

        const int score = hostHealthScore(*stats);
        if (score < 25) {
            limit = 1;
        } else if (score < 50) {
            limit = qMax(1, limit / 2);
        } else if (score < 70) {
            limit = qMax(1, limit - 1);
        }

        return limit;
    }

    bool isHostCoolingDownForTask(const HttpClient::RequestTask& task) const {
        const HostHealthStats* stats = hostStatsForTask(task);
        if (!stats) {
            return false;
        }

        if (stats->cooldownUntilMs <= currentSteadyTimeMs()) {
            return false;
        }

        return isHeavyTransferCategory(task.options.category);
    }

    bool shouldReserveSlotForInteractiveTraffic(const HttpClient::RequestTask& task) const {
        return isHeavyTransferCategory(task.options.category)
            && hasPendingInteractiveTask()
            && m_activeRequests.size() >= (m_maxConcurrentRequests - 1);
    }

    bool exceedsLaneLimit(const HttpClient::RequestTask& task) const {
        if (isInteractiveCategory(task.options.category)) {
            return activeInteractiveRequests() >= m_maxInteractiveLaneRequests;
        }

        return activeBulkRequests() >= m_maxBulkLaneRequests;
    }

    bool canScheduleTask(const HttpClient::RequestTask& task) const {
        const QString hostKey = hostKeyForTask(task);
        if (activeRequestsForHost(hostKey) >= adaptiveHostConcurrencyLimitForTask(task)) {
            return false;
        }

        if (isHeavyTransferCategory(task.options.category)
            && activeHeavyTransfers() >= m_maxHeavyTransfers) {
            return false;
        }

        if (isHostCoolingDownForTask(task)) {
            return false;
        }

        if (exceedsLaneLimit(task)) {
            return false;
        }

        if (shouldReserveSlotForInteractiveTraffic(task)) {
            return false;
        }

        return true;
    }

    int selectNextPendingTaskIndex() const {
        int bestIndex = -1;
        int bestPriority = std::numeric_limits<int>::min();
        qint64 bestTaskId = std::numeric_limits<qint64>::max();

        for (int i = 0; i < m_pendingRequests.size(); ++i) {
            const HttpClient::RequestTask& candidate = m_pendingRequests.at(i);
            if (!canScheduleTask(candidate)) {
                continue;
            }

            const int priority = taskPriority(candidate);
            if (bestIndex < 0
                || priority > bestPriority
                || (priority == bestPriority && candidate.id < bestTaskId)) {
                bestIndex = i;
                bestPriority = priority;
                bestTaskId = candidate.id;
            }
        }

        return bestIndex;
    }

    bool shouldPreferHttp11ForHost(const QString& hostKey) const {
        auto it = m_hostHealthStats.find(hostKey);
        if (it == m_hostHealthStats.end()) {
            return false;
        }

        const HostHealthStats& stats = it->second;
        const int score = hostHealthScore(stats);
        return stats.lastErrorClass == QStringLiteral("http2")
            || stats.timeoutFailures >= 2
            || score < 55;
    }

    bool shouldPreferIpv4ForHost(const QString& hostKey) const {
        auto it = m_hostHealthStats.find(hostKey);
        if (it == m_hostHealthStats.end()) {
            return false;
        }

        const HostHealthStats& stats = it->second;
        return stats.connectFailures >= 2
            || stats.dnsFailures >= 2
            || stats.lastErrorClass == QStringLiteral("dns")
            || stats.consecutiveFailures >= 3;
    }

    void applyAdaptivePolicies(ActiveRequest& request) {
        request.hostKey = hostKeyForTask(request.task);

        if (request.currentHttpPolicy == HttpVersionPolicy::PreferHttp2
            && shouldPreferHttp11ForHost(request.hostKey)) {
            request.currentHttpPolicy = HttpVersionPolicy::ForceHttp11;
            request.usedHttp11Fallback = true;
        }

        if (request.currentIpPolicy == HttpIpResolvePolicy::Any
            && shouldPreferIpv4ForHost(request.hostKey)) {
            request.currentIpPolicy = HttpIpResolvePolicy::PreferIpv4;
            request.usedIpv4Fallback = true;
        }
    }

    void updateHostHealthStats(const ActiveRequest& request, const HttpResponse& response) {
        HostHealthStats& stats = m_hostHealthStats[request.hostKey];
        stats.totalRequests++;

        const QString errorClass = response.diagnostics.value(QStringLiteral("error_class")).toString();
        stats.lastErrorClass = errorClass;

        auto decayCounter = [](int& value, int amount) {
            value = qMax(0, value - amount);
        };

        if (response.success || errorClass == QStringLiteral("success")) {
            stats.successfulRequests++;
            stats.consecutiveFailures = 0;
            stats.cooldownUntilMs = 0;
            decayCounter(stats.timeoutFailures, 1);
            decayCounter(stats.connectFailures, 1);
            decayCounter(stats.dnsFailures, 1);
            decayCounter(stats.sslFailures, 1);
            decayCounter(stats.http5xxFailures, 1);

            const qint64 totalMs = response.diagnostics.value(QStringLiteral("total_ms")).toLongLong();
            if (totalMs > 0) {
                if (stats.rollingTotalMs <= 0) {
                    stats.rollingTotalMs = totalMs;
                } else {
                    stats.rollingTotalMs = ((stats.rollingTotalMs * 3) + totalMs) / 4;
                }
            }
            return;
        }

        stats.consecutiveFailures++;

        if (errorClass == QStringLiteral("timeout")) {
            stats.timeoutFailures++;
        } else if (errorClass == QStringLiteral("connect")) {
            stats.connectFailures++;
        } else if (errorClass == QStringLiteral("dns")) {
            stats.dnsFailures++;
        } else if (errorClass == QStringLiteral("ssl")) {
            stats.sslFailures++;
        } else if (errorClass == QStringLiteral("http_5xx")) {
            stats.http5xxFailures++;
        }

        if (stats.consecutiveFailures >= 3
            && (errorClass == QStringLiteral("timeout")
                || errorClass == QStringLiteral("connect")
                || errorClass == QStringLiteral("dns"))) {
            stats.cooldownUntilMs = currentSteadyTimeMs() + 15000;
        }
    }

    void wakeupMulti() {
#ifdef CURLMOPT_SOCKETFUNCTION
        if (m_multiHandle) {
            curl_multi_wakeup(m_multiHandle);
        }
#endif
    }

    void workerLoop() {
        while (true) {
            bool stopRequested = false;
            {
                std::unique_lock<std::mutex> lock(m_mutex);
                if (!m_stopRequested && m_pendingRequests.isEmpty() && m_activeRequests.empty()) {
                    m_cv.wait(lock, [this]() {
                        return m_stopRequested || !m_pendingRequests.isEmpty() || !m_activeRequests.empty();
                    });
                }
                stopRequested = m_stopRequested;
                if (stopRequested) {
                    m_pendingRequests.clear();
                }
            }

            if (stopRequested) {
                for (auto& pair : m_activeRequests) {
                    if (m_multiHandle && pair.first) {
                        curl_multi_remove_handle(m_multiHandle, pair.first);
                    }
                    cleanupActiveRequest(*pair.second, true);
                }
                m_activeRequests.clear();
                break;
            }

            schedulePendingRequests();

            int runningHandles = 0;
            if (m_multiHandle) {
                curl_multi_perform(m_multiHandle, &runningHandles);
            }

            processCompletedMessages();
            schedulePendingRequests();

            {
                std::unique_lock<std::mutex> lock(m_mutex);
                if (m_stopRequested) {
                    m_pendingRequests.clear();
                    stopRequested = true;
                }
            }

            if (stopRequested) {
                for (auto& pair : m_activeRequests) {
                    if (m_multiHandle && pair.first) {
                        curl_multi_remove_handle(m_multiHandle, pair.first);
                    }
                    cleanupActiveRequest(*pair.second, true);
                }
                m_activeRequests.clear();
                break;
            }

            if (!m_activeRequests.empty()) {
                int numFds = 0;
                curl_multi_poll(m_multiHandle, nullptr, 0, 100, &numFds);
                curl_multi_perform(m_multiHandle, &runningHandles);
                processCompletedMessages();
            } else {
                std::this_thread::sleep_for(std::chrono::milliseconds(20));
            }
        }
    }

    void schedulePendingRequests() {
        while (true) {
            HttpClient::RequestTask task;
            {
                std::lock_guard<std::mutex> lock(m_mutex);
                if (m_pendingRequests.isEmpty() || m_activeRequests.size() >= m_maxConcurrentRequests) {
                    return;
                }

                const int nextIndex = selectNextPendingTaskIndex();
                if (nextIndex < 0) {
                    return;
                }

                task = m_pendingRequests.takeAt(nextIndex);
            }

            auto request = std::make_unique<ActiveRequest>();
            request->task = std::move(task);
            request->currentTrustMode = request->task.options.trustMode == HttpTrustMode::SystemFirstThenBundleFallback
                ? HttpTrustMode::SystemOnly
                : request->task.options.trustMode;
            request->currentHttpPolicy = request->task.options.httpVersionPolicy;
            request->currentIpPolicy = request->task.options.ipResolvePolicy;
            request->attemptIndex = 0;
            applyAdaptivePolicies(*request);

            if (!prepareAttempt(*request)) {
                HttpResponse response;
                response.backendName = HttpClient::activeBackendName();
                response.errorMessage = QStringLiteral("Failed to prepare libcurl request");
                response.diagnostics.insert(QStringLiteral("url"), redactUrlForLogs(request->task.options.url));
                invokeHandler(request->task.context, request->task.handler, response);
                continue;
            }

            CURL* easyHandle = request->easyHandle;
            m_activeRequests.emplace(easyHandle, std::move(request));
            curl_multi_add_handle(m_multiHandle, easyHandle);
        }
    }

    bool prepareAttempt(ActiveRequest& request) {
        cleanupEasyHandle(request);

        request.responseBody.clear();
        request.responseHeaders.clear();
        memset(request.errorBuffer, 0, sizeof(request.errorBuffer));

        request.easyHandle = curl_easy_init();
        if (!request.easyHandle) {
            return false;
        }

        if (request.task.writeToFile) {
            QFile::remove(request.task.filePath);
            request.outputFile.setFileName(request.task.filePath);
            if (!request.outputFile.open(QIODevice::WriteOnly)) {
                cleanupEasyHandle(request);
                return false;
            }
        }

        const HttpRequestOptions& options = request.task.options;
        const QByteArray encodedUrl = options.url.toString(QUrl::FullyEncoded).toUtf8();
        const QByteArray method = options.method.trimmed().toUpper();

        curl_easy_setopt(request.easyHandle, CURLOPT_URL, encodedUrl.constData());
        curl_easy_setopt(request.easyHandle, CURLOPT_NOSIGNAL, 1L);
        curl_easy_setopt(request.easyHandle, CURLOPT_ERRORBUFFER, request.errorBuffer);
        curl_easy_setopt(request.easyHandle, CURLOPT_FOLLOWLOCATION, options.followRedirects ? 1L : 0L);
        curl_easy_setopt(request.easyHandle, CURLOPT_TIMEOUT_MS, static_cast<long>(options.timeoutMs));
        curl_easy_setopt(request.easyHandle, CURLOPT_CONNECTTIMEOUT_MS, static_cast<long>(options.connectTimeoutMs > 0 ? options.connectTimeoutMs : qMin(options.timeoutMs, 10000)));
        curl_easy_setopt(request.easyHandle, CURLOPT_ACCEPT_ENCODING, "");
        curl_easy_setopt(request.easyHandle, CURLOPT_MAXREDIRS, 8L);
        curl_easy_setopt(request.easyHandle, CURLOPT_PRIVATE, &request);

#ifdef CURLOPT_HAPPY_EYEBALLS_TIMEOUT_MS
        curl_easy_setopt(request.easyHandle, CURLOPT_HAPPY_EYEBALLS_TIMEOUT_MS, 200L);
#endif
#ifdef CURLOPT_DNS_CACHE_TIMEOUT
        curl_easy_setopt(request.easyHandle, CURLOPT_DNS_CACHE_TIMEOUT, options.enableDnsCache ? 300L : 0L);
#endif
#ifdef CURLOPT_TCP_KEEPALIVE
        curl_easy_setopt(request.easyHandle, CURLOPT_TCP_KEEPALIVE, options.enableTcpKeepAlive ? 1L : 0L);
#endif
#ifdef CURLOPT_TCP_KEEPIDLE
        curl_easy_setopt(request.easyHandle, CURLOPT_TCP_KEEPIDLE, 30L);
#endif
#ifdef CURLOPT_TCP_KEEPINTVL
        curl_easy_setopt(request.easyHandle, CURLOPT_TCP_KEEPINTVL, 15L);
#endif
#ifdef CURLOPT_PIPEWAIT
        curl_easy_setopt(request.easyHandle, CURLOPT_PIPEWAIT, 1L);
#endif
#ifdef CURLOPT_SSL_ENABLE_ALPN
        curl_easy_setopt(request.easyHandle, CURLOPT_SSL_ENABLE_ALPN, 1L);
#endif
#ifdef CURLOPT_SSL_ENABLE_NPN
        curl_easy_setopt(request.easyHandle, CURLOPT_SSL_ENABLE_NPN, 1L);
#endif
#ifdef CURLOPT_SSL_SESSIONID_CACHE
        curl_easy_setopt(request.easyHandle, CURLOPT_SSL_SESSIONID_CACHE, 1L);
#endif
#ifdef CURLOPT_SSLVERSION
        curl_easy_setopt(request.easyHandle, CURLOPT_SSLVERSION, CURL_SSLVERSION_TLSv1_2);
#endif
#ifdef CURLOPT_DEFAULT_PROTOCOL
        curl_easy_setopt(request.easyHandle, CURLOPT_DEFAULT_PROTOCOL, "https");
#endif
#ifdef CURLOPT_SUPPRESS_CONNECT_HEADERS
        curl_easy_setopt(request.easyHandle, CURLOPT_SUPPRESS_CONNECT_HEADERS, 1L);
#endif

        if (options.lowSpeedLimitBytesPerSecond > 0 && options.lowSpeedTimeSeconds > 0) {
            curl_easy_setopt(request.easyHandle, CURLOPT_LOW_SPEED_LIMIT, static_cast<long>(options.lowSpeedLimitBytesPerSecond));
            curl_easy_setopt(request.easyHandle, CURLOPT_LOW_SPEED_TIME, static_cast<long>(options.lowSpeedTimeSeconds));
        }

        applyCurlTrustMode(request.easyHandle, request.currentTrustMode);
        applyCurlHttpVersionPolicy(request.easyHandle, request.currentHttpPolicy);
        applyCurlIpResolvePolicy(request.easyHandle, request.currentIpPolicy);

        if (options.enableConnectionReuse && m_shareHandle) {
            curl_easy_setopt(request.easyHandle, CURLOPT_SHARE, m_shareHandle);
        }

        for (const HttpHeader& header : options.headers) {
            const QByteArray line = header.name + ": " + header.value;
            request.headerList = curl_slist_append(request.headerList, line.constData());
        }

        if (request.headerList) {
            curl_easy_setopt(request.easyHandle, CURLOPT_HTTPHEADER, request.headerList);
        }

        curl_easy_setopt(request.easyHandle, CURLOPT_HEADERFUNCTION, &CurlRuntime::headerCallback);
        curl_easy_setopt(request.easyHandle, CURLOPT_HEADERDATA, &request);

        if (request.task.writeToFile) {
            curl_easy_setopt(request.easyHandle, CURLOPT_WRITEFUNCTION, &CurlRuntime::writeToFile);
            curl_easy_setopt(request.easyHandle, CURLOPT_WRITEDATA, &request);
        } else {
            curl_easy_setopt(request.easyHandle, CURLOPT_WRITEFUNCTION, &CurlRuntime::writeToBuffer);
            curl_easy_setopt(request.easyHandle, CURLOPT_WRITEDATA, &request);
        }

        if (request.task.progressHandler) {
            curl_easy_setopt(request.easyHandle, CURLOPT_XFERINFOFUNCTION, &CurlRuntime::progressCallback);
            curl_easy_setopt(request.easyHandle, CURLOPT_XFERINFODATA, &request);
            curl_easy_setopt(request.easyHandle, CURLOPT_NOPROGRESS, 0L);
        } else {
            curl_easy_setopt(request.easyHandle, CURLOPT_NOPROGRESS, 1L);
        }

        if (method == "GET") {
            curl_easy_setopt(request.easyHandle, CURLOPT_HTTPGET, 1L);
        } else if (method == "HEAD") {
            curl_easy_setopt(request.easyHandle, CURLOPT_NOBODY, 1L);
            curl_easy_setopt(request.easyHandle, CURLOPT_CUSTOMREQUEST, "HEAD");
        } else if (method == "POST") {
            curl_easy_setopt(request.easyHandle, CURLOPT_POST, 1L);
            curl_easy_setopt(request.easyHandle, CURLOPT_POSTFIELDS, options.body.constData());
            curl_easy_setopt(request.easyHandle, CURLOPT_POSTFIELDSIZE_LARGE, static_cast<curl_off_t>(options.body.size()));
        } else {
            curl_easy_setopt(request.easyHandle, CURLOPT_CUSTOMREQUEST, method.constData());
            if (!options.body.isEmpty()) {
                curl_easy_setopt(request.easyHandle, CURLOPT_POSTFIELDS, options.body.constData());
                curl_easy_setopt(request.easyHandle, CURLOPT_POSTFIELDSIZE_LARGE, static_cast<curl_off_t>(options.body.size()));
            }
        }

        return true;
    }

    void cleanupEasyHandle(ActiveRequest& request) {
        if (request.headerList) {
            curl_slist_free_all(request.headerList);
            request.headerList = nullptr;
        }

        if (request.outputFile.isOpen()) {
            request.outputFile.flush();
            request.outputFile.close();
        }

        if (request.easyHandle) {
            curl_easy_cleanup(request.easyHandle);
            request.easyHandle = nullptr;
        }
    }

    void cleanupActiveRequest(ActiveRequest& request, bool removeFileOnFailure) {
        if (request.outputFile.isOpen()) {
            request.outputFile.flush();
            request.outputFile.close();
        }
        cleanupEasyHandle(request);

        if (removeFileOnFailure && request.task.writeToFile) {
            QFile::remove(request.task.filePath);
        }
    }

    HttpResponse buildResponse(ActiveRequest& request, CURLcode code) {
        HttpResponse response;
        response.backendName = HttpClient::activeBackendName();
        response.body = request.responseBody;
        response.headers = request.responseHeaders;

        long statusCode = 0;
        curl_easy_getinfo(request.easyHandle, CURLINFO_RESPONSE_CODE, &statusCode);
        response.statusCode = static_cast<int>(statusCode);
        response.success = (code == CURLE_OK) && (statusCode < 400 || statusCode == 0);

        const QString curlError = (request.errorBuffer[0] != '\0')
            ? QString::fromUtf8(request.errorBuffer)
            : QString::fromUtf8(curl_easy_strerror(code));

        if (!response.success) {
            if (code != CURLE_OK) {
                response.errorMessage = sanitizeErrorMessageForLogs(curlError);
            } else if (statusCode >= 400) {
                response.errorMessage = QStringLiteral("HTTP %1").arg(statusCode);
            }
        }

        response.diagnostics.insert(QStringLiteral("attempt"), request.attemptIndex + 1);
        response.diagnostics.insert(QStringLiteral("curl_code"), static_cast<int>(code));
        response.diagnostics.insert(QStringLiteral("category"), determineCategoryName(request.task.options.category));
        response.diagnostics.insert(QStringLiteral("trust_source"), determineTrustSourceName(request.currentTrustMode));
        response.diagnostics.insert(QStringLiteral("used_http11_fallback"), request.usedHttp11Fallback);
        response.diagnostics.insert(QStringLiteral("used_ipv4_fallback"), request.usedIpv4Fallback);
        response.diagnostics.insert(QStringLiteral("url"), redactUrlForLogs(request.task.options.url));
        response.diagnostics.insert(QStringLiteral("error_class"), classifyCurlError(code, statusCode, curlError));

        if (request.task.options.collectTimingInfo) {
            collectCurlTimings(request.easyHandle, response.diagnostics);
        }

        return response;
    }

    bool restartForFallbackOrRetry(ActiveRequest& request, const HttpResponse& response) {
        const QString errorClass = response.diagnostics.value(QStringLiteral("error_class")).toString();
        const HttpRequestOptions& options = request.task.options;

        if (options.trustMode == HttpTrustMode::SystemFirstThenBundleFallback
            && request.currentTrustMode == HttpTrustMode::SystemOnly
            && errorClass == QStringLiteral("ssl")
            && QFileInfo::exists(bundledCaPath())) {
            request.currentTrustMode = HttpTrustMode::BundleOnly;
            return prepareAttempt(request);
        }

        if (options.allowHttp11Fallback
            && !request.usedHttp11Fallback
            && request.currentHttpPolicy != HttpVersionPolicy::ForceHttp11
            && (errorClass == QStringLiteral("http2") || errorClass == QStringLiteral("timeout"))) {
            request.currentHttpPolicy = HttpVersionPolicy::ForceHttp11;
            request.usedHttp11Fallback = true;
            return prepareAttempt(request);
        }

        if (options.allowIpv4Fallback
            && !request.usedIpv4Fallback
            && request.currentIpPolicy != HttpIpResolvePolicy::ForceIpv4
            && request.currentIpPolicy != HttpIpResolvePolicy::PreferIpv4
            && (errorClass == QStringLiteral("connect") || errorClass == QStringLiteral("timeout") || errorClass == QStringLiteral("dns"))) {
            request.currentIpPolicy = HttpIpResolvePolicy::ForceIpv4;
            request.usedIpv4Fallback = true;
            return prepareAttempt(request);
        }

        if (!shouldRetryResponse(options,
                                 static_cast<CURLcode>(response.diagnostics.value(QStringLiteral("curl_code")).toInt()),
                                 response.statusCode,
                                 response.errorMessage,
                                 request.attemptIndex)) {
            return false;
        }

        request.attemptIndex++;
        if (options.retryBackoffMs > 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(options.retryBackoffMs * (request.attemptIndex)));
        }
        return prepareAttempt(request);
    }

    void processCompletedMessages() {
        int messagesLeft = 0;
        while (CURLMsg* message = curl_multi_info_read(m_multiHandle, &messagesLeft)) {
            if (message->msg != CURLMSG_DONE) {
                continue;
            }

            CURL* completedEasy = message->easy_handle;
            auto it = m_activeRequests.find(completedEasy);
            if (it == m_activeRequests.end()) {
                continue;
            }

            std::unique_ptr<ActiveRequest> request = std::move(it->second);
            m_activeRequests.erase(it);

            HttpResponse response = buildResponse(*request, message->data.result);
            updateHostHealthStats(*request, response);

            curl_multi_remove_handle(m_multiHandle, completedEasy);

            if (restartForFallbackOrRetry(*request, response)) {
                CURL* restartedEasy = request->easyHandle;
                m_activeRequests.emplace(restartedEasy, std::move(request));
                curl_multi_add_handle(m_multiHandle, restartedEasy);
                continue;
            }

            const bool removeFileOnFailure = !response.success;
            cleanupActiveRequest(*request, removeFileOnFailure);

            const HostHealthStats& hostStats = m_hostHealthStats[request->hostKey];
            Logger::instance().logInfo(
                "HttpClient",
                QString("%1 %2 backend=%3 category=%4 status=%5 total=%6ms connect=%7ms tls=%8ms firstByte=%9ms ip=%10 hostAvg=%11ms hostOk=%12/%13 hostErr=%14 hostScore=%15 cooldownMs=%16")
                    .arg(request->task.writeToFile ? QStringLiteral("DOWNLOAD") : QStringLiteral("HTTP"))
                    .arg(redactUrlForLogs(request->task.options.url))
                    .arg(response.backendName)
                    .arg(determineCategoryName(request->task.options.category))
                    .arg(response.statusCode)
                    .arg(response.diagnostics.value("total_ms").toLongLong())
                    .arg(response.diagnostics.value("connect_ms").toLongLong())
                    .arg(response.diagnostics.value("tls_ms").toLongLong())
                    .arg(response.diagnostics.value("first_byte_ms").toLongLong())
                    .arg(response.diagnostics.value("primary_ip").toString())
                    .arg(hostStats.rollingTotalMs)
                    .arg(hostStats.successfulRequests)
                    .arg(hostStats.totalRequests)
                    .arg(hostStats.lastErrorClass)
                    .arg(hostHealthScore(hostStats))
                    .arg(qMax<qint64>(0, hostStats.cooldownUntilMs - currentSteadyTimeMs()))
            );

            invokeHandler(request->task.context, request->task.handler, response);
        }
    }

    HttpClient* m_owner = nullptr;
    CURLM* m_multiHandle = nullptr;
    CURLSH* m_shareHandle = nullptr;
    std::thread m_worker;
    std::mutex m_mutex;
    std::condition_variable m_cv;
    QQueue<HttpClient::RequestTask> m_pendingRequests;
    std::unordered_map<CURL*, std::unique_ptr<ActiveRequest>> m_activeRequests;
    std::unordered_map<QString, HostHealthStats> m_hostHealthStats;
    bool m_stopRequested = false;
    const std::size_t m_maxConcurrentRequests = 8;
    const int m_maxHeavyTransfers = 4;
    const int m_maxHeavyRequestsPerHost = 4;
    const int m_maxInteractiveRequestsPerHost = 6;
    const int m_maxInteractiveLaneRequests = 4;
    const int m_maxBulkLaneRequests = 4;
};
#endif

HttpClient& HttpClient::instance() {
    static HttpClient instance;
    return instance;
}

HttpClient::HttpClient(QObject* parent)
    : QObject(parent)
    , m_qtNetworkManager(new QNetworkAccessManager(this)) {
#ifdef APE_USE_LIBCURL
    curl_global_init(CURL_GLOBAL_DEFAULT);
    m_curlRuntime = std::make_unique<CurlRuntime>(this);
#endif
}

HttpClient::~HttpClient() {
#ifdef APE_USE_LIBCURL
    m_curlRuntime.reset();
    curl_global_cleanup();
#endif
}

void HttpClient::send(const HttpRequestOptions& options,
                      QObject* context,
                      const ResponseHandler& handler,
                      const ProgressHandler& progressHandler) {
    if (m_shutdownRequested) {
        HttpResponse response;
        response.backendName = activeBackendName();
        response.errorMessage = QStringLiteral("HttpClient is shutting down");
        invokeHandler(context, handler, response);
        return;
    }

#ifdef APE_USE_LIBCURL
    Q_UNUSED(options.backendPreference);
    sendWithCurl(options, context, handler, progressHandler);
#else
    sendWithQt(options, context, handler, progressHandler);
#endif
}

void HttpClient::downloadToFile(const HttpRequestOptions& options,
                                const QString& filePath,
                                QObject* context,
                                const ResponseHandler& handler,
                                const ProgressHandler& progressHandler) {
    if (m_shutdownRequested) {
        HttpResponse response;
        response.backendName = activeBackendName();
        response.errorMessage = QStringLiteral("HttpClient is shutting down");
        invokeHandler(context, handler, response);
        return;
    }

#ifdef APE_USE_LIBCURL
    Q_UNUSED(options.backendPreference);
    downloadWithCurl(options, filePath, context, handler, progressHandler);
#else
    downloadWithQt(options, filePath, context, handler, progressHandler);
#endif
}

void HttpClient::refreshCaBundleInBackground(const QUrl& metadataUrl,
                                             const QUrl& downloadUrl,
                                             QObject* context,
                                             const ResponseHandler& handler) {
    if (m_shutdownRequested) {
        HttpResponse response;
        response.backendName = activeBackendName();
        response.errorMessage = QStringLiteral("HttpClient is shutting down");
        invokeHandler(context, handler, response);
        return;
    }

    struct CaRefreshState {
        QUrl metadataUrl;
        QUrl defaultDownloadUrl;
        QObject* context = nullptr;
        HttpClient::ResponseHandler handler;
        QString targetPath;
        QString tempPath;
        QString backupPath;
        QString remoteSha256;
        qint64 remoteSize = -1;
    };

    auto state = std::make_shared<CaRefreshState>();
    state->metadataUrl = metadataUrl;
    state->defaultDownloadUrl = downloadUrl;
    state->context = context;
    state->handler = handler;
    state->targetPath = bundledCaPath();
    state->tempPath = state->targetPath + ".download";
    state->backupPath = state->targetPath + ".bak";

    auto finishWithResponse = [state](const HttpResponse& response) {
        invokeHandler(state->context, state->handler, response);
    };

    auto finishWithError = [state, finishWithResponse](const QString& errorMessage) {
        HttpResponse response;
        response.backendName = HttpClient::activeBackendName();
        response.errorMessage = errorMessage;
        finishWithResponse(response);
    };

    if (!state->metadataUrl.isValid() || !state->defaultDownloadUrl.isValid()) {
        finishWithError(QStringLiteral("Invalid CA bundle refresh URLs"));
        return;
    }

    HttpRequestOptions metaOptions = createGet(state->metadataUrl);
    metaOptions.timeoutMs = 15000;
    metaOptions.connectTimeoutMs = 5000;
    metaOptions.trustMode = HttpTrustMode::SystemOnly;
    metaOptions.category = HttpRequestCategory::Manifest;
    metaOptions.maxRetries = 1;
    metaOptions.retryOnHttp5xx = true;
    metaOptions.backendPreference = HttpBackendPreference::Libcurl;

    send(metaOptions, this, [this, state, finishWithResponse, finishWithError](const HttpResponse& metaResponse) {
        if (!metaResponse.success) {
            finishWithResponse(metaResponse);
            return;
        }

        const QJsonDocument metaDoc = QJsonDocument::fromJson(metaResponse.body);
        if (!metaDoc.isObject()) {
            finishWithError(QStringLiteral("Invalid CA bundle metadata response"));
            return;
        }

        const QJsonObject metaObject = metaDoc.object();
        state->remoteSha256 = metaObject.value("sha256").toString().trimmed().toLower();
        state->remoteSize = static_cast<qint64>(metaObject.value("size").toDouble(-1));
        const QUrl resolvedDownloadUrl = determineCaDownloadUrl(metaObject, state->defaultDownloadUrl);

        if (state->remoteSha256.isEmpty()) {
            finishWithError(QStringLiteral("Missing CA bundle hash in metadata"));
            return;
        }

        const QByteArray localSha256 = fileSha256(state->targetPath);
        const QFileInfo currentInfo(state->targetPath);
        if (!localSha256.isEmpty()
            && QString::fromUtf8(localSha256).compare(state->remoteSha256, Qt::CaseInsensitive) == 0
            && (state->remoteSize < 0 || currentInfo.size() == state->remoteSize)) {
            HttpResponse response;
            response.backendName = HttpClient::activeBackendName();
            response.success = true;
            response.statusCode = 200;
            response.body = QByteArray("up-to-date");
            finishWithResponse(response);
            return;
        }

        QDir().mkpath(QFileInfo(state->targetPath).absolutePath());
        QFile::remove(state->tempPath);

        HttpRequestOptions downloadOptions = createGet(resolvedDownloadUrl);
        downloadOptions.timeoutMs = 60000;
        downloadOptions.connectTimeoutMs = 8000;
        downloadOptions.lowSpeedLimitBytesPerSecond = 1024;
        downloadOptions.lowSpeedTimeSeconds = 30;
        downloadOptions.trustMode = HttpTrustMode::SystemOnly;
        downloadOptions.category = HttpRequestCategory::LargeDownload;
        downloadOptions.maxRetries = 2;
        downloadOptions.retryOnHttp5xx = true;
        downloadOptions.backendPreference = HttpBackendPreference::Libcurl;

        downloadToFile(downloadOptions, state->tempPath, this, [state, finishWithResponse, finishWithError](const HttpResponse& downloadResponse) {
            if (!downloadResponse.success) {
                QFile::remove(state->tempPath);
                finishWithResponse(downloadResponse);
                return;
            }

            const QByteArray downloadedSha256 = fileSha256(state->tempPath);
            if (downloadedSha256.isEmpty()
                || QString::fromUtf8(downloadedSha256).compare(state->remoteSha256, Qt::CaseInsensitive) != 0) {
                QFile::remove(state->tempPath);
                finishWithError(QStringLiteral("CA bundle hash verification failed"));
                return;
            }

            if (state->remoteSize >= 0) {
                const QFileInfo downloadedInfo(state->tempPath);
                if (downloadedInfo.size() != state->remoteSize) {
                    QFile::remove(state->tempPath);
                    finishWithError(QStringLiteral("CA bundle size verification failed"));
                    return;
                }
            }

            QFile::remove(state->backupPath);
            if (QFile::exists(state->targetPath) && !QFile::rename(state->targetPath, state->backupPath)) {
                QFile::remove(state->tempPath);
                finishWithError(QStringLiteral("Failed to back up local CA bundle"));
                return;
            }

            if (!QFile::rename(state->tempPath, state->targetPath)) {
                if (QFile::exists(state->backupPath)) {
                    QFile::rename(state->backupPath, state->targetPath);
                }
                QFile::remove(state->tempPath);
                finishWithError(QStringLiteral("Failed to replace local CA bundle"));
                return;
            }

            QFile::remove(state->backupPath);

            HttpResponse response;
            response.backendName = HttpClient::activeBackendName();
            response.success = true;
            response.statusCode = 200;
            response.body = QByteArray("updated");
            finishWithResponse(response);
        }, ProgressHandler());
    }, ProgressHandler());
}

void HttpClient::warmUpConnection(const QUrl& url,
                                  QObject* context,
                                  const ResponseHandler& handler) {
    if (m_shutdownRequested) {
        HttpResponse response;
        response.backendName = activeBackendName();
        response.errorMessage = QStringLiteral("HttpClient is shutting down");
        invokeHandler(context, handler, response);
        return;
    }

    if (!url.isValid()) {
        HttpResponse response;
        response.backendName = activeBackendName();
        response.errorMessage = QStringLiteral("Invalid warm-up URL");
        invokeHandler(context, handler, response);
        return;
    }

    HttpRequestOptions options = createGet(url);
    options.method = "HEAD";
    options.timeoutMs = 8000;
    options.connectTimeoutMs = 3000;
    options.maxRetries = 0;
    options.retryOnTimeout = false;
    options.retryOnTransientSslFailure = false;
    options.retryOnHttp5xx = false;
    options.followRedirects = false;
    options.category = HttpRequestCategory::General;
    options.backendPreference = HttpBackendPreference::Libcurl;
    options.collectTimingInfo = false;

    send(options, context, handler, ProgressHandler());
}

HttpRequestOptions HttpClient::createGet(const QUrl& url) {
    HttpRequestOptions options;
    options.url = url;
    options.method = "GET";
    return options;
}

HttpRequestOptions HttpClient::createJsonPost(const QUrl& url, const QByteArray& jsonBody) {
    HttpRequestOptions options;
    options.url = url;
    options.method = "POST";
    options.body = jsonBody;
    addOrReplaceHeader(options, "Content-Type", "application/json");
    return options;
}

void HttpClient::addOrReplaceHeader(HttpRequestOptions& options, const QByteArray& name, const QByteArray& value) {
    for (HttpHeader& header : options.headers) {
        if (header.name.compare(name, Qt::CaseInsensitive) == 0) {
            header.value = value;
            return;
        }
    }

    options.headers.append(HttpHeader{name, value});
}

QString HttpClient::activeBackendName() {
#ifdef APE_USE_LIBCURL
    return QStringLiteral("libcurl");
#else
    return QStringLiteral("QtNetwork");
#endif
}

void HttpClient::sendWithQt(const HttpRequestOptions& options,
                            QObject* context,
                            const ResponseHandler& handler,
                            const ProgressHandler& progressHandler) {
    QNetworkRequest request = buildQtRequest(options);
    QNetworkReply* reply = nullptr;

    const QByteArray method = options.method.trimmed().toUpper();
    if (method == "GET") {
        reply = m_qtNetworkManager->get(request);
    } else if (method == "POST") {
        reply = m_qtNetworkManager->post(request, options.body);
    } else {
        reply = m_qtNetworkManager->sendCustomRequest(request, method, options.body);
    }

    if (!reply) {
        HttpResponse response;
        response.backendName = activeBackendName();
        response.errorMessage = QStringLiteral("Failed to create Qt network reply");
        invokeHandler(context, handler, response);
        return;
    }

    if (options.url.scheme().compare("https", Qt::CaseInsensitive) == 0) {
        connect(reply, &QNetworkReply::sslErrors, this, [reply](const QList<QSslError>& errors) {
            SslConfig::handleSslErrors(reply, errors);
        });
    }

    QTimer* timeoutTimer = nullptr;
    if (options.timeoutMs > 0) {
        timeoutTimer = new QTimer(reply);
        timeoutTimer->setSingleShot(true);
        connect(timeoutTimer, &QTimer::timeout, reply, [reply]() {
            if (reply->isRunning()) {
                reply->abort();
            }
        });
        timeoutTimer->start(options.timeoutMs);
    }

    if (progressHandler) {
        connect(reply, &QNetworkReply::downloadProgress, this, [context, progressHandler](qint64 received, qint64 total) {
            invokeProgress(context, progressHandler, received, total);
        });
    }

    connect(reply, &QNetworkReply::finished, this, [context, handler, timeoutTimer, reply]() {
        if (timeoutTimer) {
            timeoutTimer->stop();
        }

        const QByteArray body = reply->readAll();
        const HttpResponse response = buildResponseFromReply(reply, body);
        invokeHandler(context, handler, response);
        reply->deleteLater();
    });
}

void HttpClient::downloadWithQt(const HttpRequestOptions& options,
                                const QString& filePath,
                                QObject* context,
                                const ResponseHandler& handler,
                                const ProgressHandler& progressHandler) {
    QFile* outputFile = new QFile(filePath);
    if (!outputFile->open(QIODevice::WriteOnly)) {
        HttpResponse response;
        response.backendName = activeBackendName();
        response.errorMessage = QStringLiteral("Failed to open output file: %1").arg(filePath);
        invokeHandler(context, handler, response);
        delete outputFile;
        return;
    }

    QNetworkRequest request = buildQtRequest(options);
    QNetworkReply* reply = m_qtNetworkManager->get(request);

    if (options.url.scheme().compare("https", Qt::CaseInsensitive) == 0) {
        connect(reply, &QNetworkReply::sslErrors, this, [reply](const QList<QSslError>& errors) {
            SslConfig::handleSslErrors(reply, errors);
        });
    }

    QTimer* timeoutTimer = nullptr;
    if (options.timeoutMs > 0) {
        timeoutTimer = new QTimer(reply);
        timeoutTimer->setSingleShot(true);
        connect(timeoutTimer, &QTimer::timeout, reply, [reply]() {
            if (reply->isRunning()) {
                reply->abort();
            }
        });
        timeoutTimer->start(options.timeoutMs);
    }

    connect(reply, &QNetworkReply::readyRead, this, [reply, outputFile]() {
        outputFile->write(reply->readAll());
    });

    if (progressHandler) {
        connect(reply, &QNetworkReply::downloadProgress, this, [context, progressHandler](qint64 received, qint64 total) {
            invokeProgress(context, progressHandler, received, total);
        });
    }

    connect(reply, &QNetworkReply::finished, this, [context, handler, timeoutTimer, reply, outputFile]() {
        if (timeoutTimer) {
            timeoutTimer->stop();
        }

        outputFile->write(reply->readAll());
        outputFile->flush();
        outputFile->close();

        HttpResponse response = buildResponseFromReply(reply, QByteArray());
        if (!response.success) {
            QFile::remove(outputFile->fileName());
        }

        invokeHandler(context, handler, response);

        outputFile->deleteLater();
        reply->deleteLater();
    });
}

void HttpClient::shutdown() {
    if (m_shutdownRequested) {
        return;
    }

    m_shutdownRequested = true;

#ifdef APE_USE_LIBCURL
    stopCurlWorker();
#endif
}

#ifdef APE_USE_LIBCURL
void HttpClient::sendWithCurl(const HttpRequestOptions& options,
                              QObject* context,
                              const ResponseHandler& handler,
                              const ProgressHandler& progressHandler) {
    RequestTask task;
    task.id = ++m_nextRequestId;
    task.options = options;
    task.context = context;
    task.handler = handler;
    task.progressHandler = progressHandler;
    task.writeToFile = false;

    enqueueCurlTask(std::move(task));
}

void HttpClient::downloadWithCurl(const HttpRequestOptions& options,
                                  const QString& filePath,
                                  QObject* context,
                                  const ResponseHandler& handler,
                                  const ProgressHandler& progressHandler) {
    RequestTask task;
    task.id = ++m_nextRequestId;
    task.options = options;
    task.context = context;
    task.handler = handler;
    task.progressHandler = progressHandler;
    task.writeToFile = true;
    task.filePath = filePath;

    enqueueCurlTask(std::move(task));
}

void HttpClient::enqueueCurlTask(RequestTask task) {
    if (m_shutdownRequested) {
        HttpResponse response;
        response.backendName = activeBackendName();
        response.errorMessage = QStringLiteral("HttpClient is shutting down");
        invokeHandler(task.context, task.handler, response);
        return;
    }

    ensureCurlWorkerStarted();
    if (m_curlRuntime) {
        m_curlRuntime->enqueue(std::move(task));
    }
}

void HttpClient::ensureCurlWorkerStarted() {
    if (m_shutdownRequested) {
        return;
    }

    if (!m_curlRuntime) {
        m_curlRuntime = std::make_unique<CurlRuntime>(this);
    }
}

void HttpClient::stopCurlWorker() {
    m_curlRuntime.reset();
}

void HttpClient::curlWorkerLoop() {
}
#endif