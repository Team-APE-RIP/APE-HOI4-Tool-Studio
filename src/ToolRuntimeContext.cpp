#include "ToolRuntimeContext.h"

ToolRuntimeContext& ToolRuntimeContext::instance() {
    static ToolRuntimeContext instance;
    return instance;
}

void ToolRuntimeContext::setPluginBinaryPathResolver(PluginBinaryPathResolver resolver) {
    m_pluginBinaryPathResolver = std::move(resolver);
}

bool ToolRuntimeContext::requestAuthorizedPluginBinaryPath(const QString& pluginName, QString* outPath, QString* errorMessage) const {
    if (!m_pluginBinaryPathResolver) {
        if (errorMessage) {
            *errorMessage = "Plugin runtime context resolver is not available.";
        }
        return false;
    }

    return m_pluginBinaryPathResolver(pluginName, outPath, errorMessage);
}