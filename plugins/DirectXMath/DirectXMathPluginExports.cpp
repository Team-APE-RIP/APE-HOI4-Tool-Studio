#include "main/DirectXMath.h"

#ifdef _WIN32
#define APE_PLUGIN_EXPORT extern "C" __declspec(dllexport)
#else
#define APE_PLUGIN_EXPORT extern "C"
#endif

APE_PLUGIN_EXPORT const char* APE_DirectXMath_GetPluginName() {
    return "DirectXMath";
}

APE_PLUGIN_EXPORT int APE_DirectXMath_GetVersion() {
    return 1;
}