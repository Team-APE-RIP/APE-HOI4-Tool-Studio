#include "main/TagManager.h"

#include <QByteArray>
#include <QJsonDocument>
#include <QJsonObject>

#ifdef _WIN32
#define APE_PLUGIN_EXPORT extern "C" __declspec(dllexport)
#else
#define APE_PLUGIN_EXPORT extern "C"
#endif

namespace {
QByteArray g_tagListJsonBuffer;
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