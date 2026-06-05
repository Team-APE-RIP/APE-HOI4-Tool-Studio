#include "main/TagManager.h"
#include "../../src/PluginAbi.h"

#include <QByteArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QString>

#include <cstdlib>
#include <cstdint>
#include <cstring>

#ifdef _WIN32
#define APE_PLUGIN_EXPORT extern "C" __declspec(dllexport)
#else
#define APE_PLUGIN_EXPORT extern "C"
#endif

namespace {
QByteArray g_tagListJsonBuffer;

void clearAbiResponse(ApePluginAbiResponse* response) {
    if (!response) {
        return;
    }
    response->abiVersion = APE_PLUGIN_ABI_VERSION;
    response->status = APE_PLUGIN_ABI_STATUS_INTERNAL_ERROR;
    response->contentType = APE_PLUGIN_ABI_CONTENT_NONE;
    response->flags = 0;
    response->payload = nullptr;
    response->payloadSize = 0;
    response->errorUtf8 = nullptr;
}

char* allocateAbiString(const QByteArray& text) {
    char* data = static_cast<char*>(std::malloc(static_cast<std::size_t>(text.size() + 1)));
    if (!data) {
        return nullptr;
    }
    std::memcpy(data, text.constData(), static_cast<std::size_t>(text.size()));
    data[text.size()] = '\0';
    return data;
}

bool setAbiPayload(ApePluginAbiResponse* response, const QByteArray& payload) {
    if (!response) {
        return false;
    }
    response->contentType = APE_PLUGIN_ABI_CONTENT_JSON_UTF8;
    response->payloadSize = static_cast<std::uint64_t>(payload.size());
    if (payload.isEmpty()) {
        response->payload = nullptr;
        return true;
    }
    auto* bytes = static_cast<std::uint8_t*>(std::malloc(static_cast<std::size_t>(payload.size())));
    if (!bytes) {
        return false;
    }
    std::memcpy(bytes, payload.constData(), static_cast<std::size_t>(payload.size()));
    response->payload = bytes;
    return true;
}

void setAbiError(ApePluginAbiResponse* response, std::uint32_t status, const QString& message) {
    if (!response) {
        return;
    }
    response->status = status;
    response->errorUtf8 = allocateAbiString(message.toUtf8());
}
}

APE_PLUGIN_EXPORT const char* APE_TagList_GetTagsJson() {
    TagManager::instance().scanTags();
    const QJsonObject json = TagManager::instance().toJson();
    g_tagListJsonBuffer = QJsonDocument(json).toJson(QJsonDocument::Compact);
    return g_tagListJsonBuffer.constData();
}

APE_PLUGIN_EXPORT int APE_TagList_GetTagCount() {
    TagManager::instance().scanTags();
    return TagManager::instance().getTags().size();
}

APE_PLUGIN_EXPORT const char* APE_TagList_GetPluginName() {
    return "TagList";
}

APE_PLUGIN_ABI_EXPORT const char* APE_Plugin_GetName(void) {
    return "TagList";
}

APE_PLUGIN_ABI_EXPORT std::uint32_t APE_Plugin_GetAbiVersion(void) {
    return APE_PLUGIN_ABI_VERSION;
}

APE_PLUGIN_ABI_EXPORT void APE_Plugin_FreeResponse(ApePluginAbiResponse* response) {
    if (!response) {
        return;
    }
    std::free(response->payload);
    std::free(response->errorUtf8);
    clearAbiResponse(response);
}

APE_PLUGIN_ABI_EXPORT int APE_Plugin_Invoke(const ApePluginAbiRequest* request, ApePluginAbiResponse* response) {
    clearAbiResponse(response);
    if (!request || !response || !request->operationUtf8) {
        setAbiError(response, APE_PLUGIN_ABI_STATUS_INVALID_ARGUMENT, QStringLiteral("Invalid TagList ABI request."));
        return 1;
    }
    if (request->abiVersion != APE_PLUGIN_ABI_VERSION) {
        setAbiError(response, APE_PLUGIN_ABI_STATUS_UNSUPPORTED_ABI, QStringLiteral("Unsupported TagList ABI version."));
        return 1;
    }

    const QString operation = QString::fromUtf8(request->operationUtf8);
    if (operation != QStringLiteral("tagList.getTags")) {
        setAbiError(response, APE_PLUGIN_ABI_STATUS_UNSUPPORTED_OPERATION, QStringLiteral("Unsupported TagList operation."));
        return 1;
    }

    const char* json = APE_TagList_GetTagsJson();
    if (!setAbiPayload(response, QByteArray(json ? json : ""))) {
        setAbiError(response, APE_PLUGIN_ABI_STATUS_INTERNAL_ERROR, QStringLiteral("Failed to allocate TagList response."));
        return 1;
    }
    response->status = APE_PLUGIN_ABI_STATUS_OK;
    return 0;
}
