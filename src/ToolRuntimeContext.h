#ifndef TOOLRUNTIMECONTEXT_H
#define TOOLRUNTIMECONTEXT_H

#include <QString>
#include <functional>

class ToolRuntimeContext {
public:
    using PluginBinaryPathResolver = std::function<bool(const QString&, QString*, QString*)>;

    static ToolRuntimeContext& instance();

    void setPluginBinaryPathResolver(PluginBinaryPathResolver resolver);
    bool requestAuthorizedPluginBinaryPath(const QString& pluginName, QString* outPath, QString* errorMessage = nullptr) const;

private:
    ToolRuntimeContext() = default;

    PluginBinaryPathResolver m_pluginBinaryPathResolver;
};

#endif // TOOLRUNTIMECONTEXT_H