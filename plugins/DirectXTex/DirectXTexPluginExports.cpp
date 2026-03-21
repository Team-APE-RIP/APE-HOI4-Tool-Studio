#include "main/DirectXTex.h"

#include <QByteArray>
#include <QImage>
#include <QString>

#ifdef _WIN32
#define APE_PLUGIN_EXPORT extern "C" __declspec(dllexport)
#else
#define APE_PLUGIN_EXPORT extern "C"
#endif

namespace {
QByteArray g_errorBuffer;

bool loadImageFromDDS(const QString& path, QImage& outImage) {
    const std::wstring wpath = path.toStdWString();

    DirectX::TexMetadata metadata;
    DirectX::ScratchImage scratchImage;

    HRESULT hr = DirectX::LoadFromDDSFile(wpath.c_str(), DirectX::DDS_FLAGS_NONE, &metadata, scratchImage);
    if (FAILED(hr)) {
        g_errorBuffer = QByteArray("Failed to load DDS file.");
        return false;
    }

    DirectX::ScratchImage decompressedImage;
    const DirectX::Image* srcImage = scratchImage.GetImage(0, 0, 0);

    if (DirectX::IsCompressed(metadata.format)) {
        hr = DirectX::Decompress(*srcImage, DXGI_FORMAT_R8G8B8A8_UNORM, decompressedImage);
        if (FAILED(hr)) {
            g_errorBuffer = QByteArray("Failed to decompress DDS image.");
            return false;
        }
        srcImage = decompressedImage.GetImage(0, 0, 0);
    }

    DirectX::ScratchImage convertedImage;
    if (srcImage->format != DXGI_FORMAT_R8G8B8A8_UNORM && srcImage->format != DXGI_FORMAT_B8G8R8A8_UNORM) {
        hr = DirectX::Convert(*srcImage, DXGI_FORMAT_R8G8B8A8_UNORM, DirectX::TEX_FILTER_DEFAULT, DirectX::TEX_THRESHOLD_DEFAULT, convertedImage);
        if (FAILED(hr)) {
            g_errorBuffer = QByteArray("Failed to convert DDS image.");
            return false;
        }
        srcImage = convertedImage.GetImage(0, 0, 0);
    }

    const int width = static_cast<int>(srcImage->width);
    const int height = static_cast<int>(srcImage->height);

    outImage = QImage(width, height, QImage::Format_ARGB32);
    for (int y = 0; y < height; ++y) {
        const uint8_t* srcRow = srcImage->pixels + y * srcImage->rowPitch;
        QRgb* destRow = reinterpret_cast<QRgb*>(outImage.scanLine(y));
        if (srcImage->format == DXGI_FORMAT_B8G8R8A8_UNORM) {
            memcpy(destRow, srcRow, width * 4);
        } else {
            for (int x = 0; x < width; ++x) {
                const uint8_t r = srcRow[x * 4 + 0];
                const uint8_t g = srcRow[x * 4 + 1];
                const uint8_t b = srcRow[x * 4 + 2];
                const uint8_t a = srcRow[x * 4 + 3];
                destRow[x] = qRgba(r, g, b, a);
            }
        }
    }

    g_errorBuffer.clear();
    return true;
}
}

APE_PLUGIN_EXPORT int APE_DirectXTex_LoadDDSImage(const wchar_t* path, unsigned char** outBytes, int* outWidth, int* outHeight, int* outStride) {
    if (!path || !outBytes || !outWidth || !outHeight || !outStride) {
        g_errorBuffer = QByteArray("Invalid DirectXTex arguments.");
        return 0;
    }

    QImage image;
    if (!loadImageFromDDS(QString::fromWCharArray(path), image)) {
        return 0;
    }

    const QImage argbImage = image.convertToFormat(QImage::Format_ARGB32);
    const int byteCount = static_cast<int>(argbImage.sizeInBytes());

    unsigned char* buffer = new unsigned char[byteCount];
    memcpy(buffer, argbImage.constBits(), byteCount);

    *outBytes = buffer;
    *outWidth = argbImage.width();
    *outHeight = argbImage.height();
    *outStride = argbImage.bytesPerLine();
    return 1;
}

APE_PLUGIN_EXPORT void APE_DirectXTex_FreeImage(unsigned char* bytes) {
    delete[] bytes;
}

APE_PLUGIN_EXPORT const char* APE_DirectXTex_GetLastError() {
    return g_errorBuffer.constData();
}

APE_PLUGIN_EXPORT const char* APE_DirectXTex_GetPluginName() {
    return "DirectXTex";
}