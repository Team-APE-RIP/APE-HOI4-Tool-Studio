//-------------------------------------------------------------------------------------
// AcrylicScreenCapture.cpp -- Part of APE HOI4 Tool Studio
//
// Copyright (C) 2026 Team APE:RIP. All rights reserved.
// Licensed under the Team APE:RIP Source Code License Agreement.
//
// https://github.com/Team-APE-RIP/APE-HOI4-Tool-Studio/
//-------------------------------------------------------------------------------------
#include "AcrylicScreenCapture.h"

#include <QMutexLocker>
#include <QtGlobal>

#include <algorithm>
#include <cstring>

#ifdef Q_OS_WIN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <d3d11.h>
#include <dxgi1_2.h>
#include <windows.h>
#endif

namespace {
#ifdef Q_OS_WIN
template <typename T>
void releaseCom(T*& object) {
    if (object) {
        object->Release();
        object = nullptr;
    }
}

template <typename T>
T* comPtr(void* value) {
    return static_cast<T*>(value);
}

QRect rectFromDxgiCoordinates(const RECT& rect) {
    return QRect(
        rect.left,
        rect.top,
        std::max<LONG>(0, rect.right - rect.left),
        std::max<LONG>(0, rect.bottom - rect.top)
    );
}

bool isBgraFormat(int format) {
    switch (static_cast<DXGI_FORMAT>(format)) {
    case DXGI_FORMAT_B8G8R8A8_UNORM:
    case DXGI_FORMAT_B8G8R8A8_UNORM_SRGB:
    case DXGI_FORMAT_B8G8R8X8_UNORM:
    case DXGI_FORMAT_B8G8R8X8_UNORM_SRGB:
        return true;
    default:
        return false;
    }
}

bool isRgbaFormat(int format) {
    switch (static_cast<DXGI_FORMAT>(format)) {
    case DXGI_FORMAT_R8G8B8A8_UNORM:
    case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB:
        return true;
    default:
        return false;
    }
}

bool isSupportedDesktopFormat(int format) {
    return isBgraFormat(format) || isRgbaFormat(format);
}

bool looksLikeBlueFlashFrame(const QImage& image) {
    if (image.isNull() || image.width() < 4 || image.height() < 4) {
        return false;
    }

    const int stepX = std::max(1, image.width() / 24);
    const int stepY = std::max(1, image.height() / 18);
    int samples = 0;
    int electricBlueSamples = 0;
    qint64 redTotal = 0;
    qint64 greenTotal = 0;
    qint64 blueTotal = 0;

    for (int y = 0; y < image.height(); y += stepY) {
        const auto* line = reinterpret_cast<const QRgb*>(image.constScanLine(y));
        for (int x = 0; x < image.width(); x += stepX) {
            const QRgb pixel = line[x];
            const int red = qRed(pixel);
            const int green = qGreen(pixel);
            const int blue = qBlue(pixel);
            ++samples;
            redTotal += red;
            greenTotal += green;
            blueTotal += blue;
            if (blue >= 220 && red <= 48 && green <= 96) {
                ++electricBlueSamples;
            }
        }
    }

    if (samples <= 0) {
        return false;
    }

    const int blueRatio = electricBlueSamples * 100 / samples;
    const int averageRed = static_cast<int>(redTotal / samples);
    const int averageGreen = static_cast<int>(greenTotal / samples);
    const int averageBlue = static_cast<int>(blueTotal / samples);
    return blueRatio >= 82
        && averageBlue >= 205
        && averageBlue - std::max(averageRed, averageGreen) >= 145;
}
#endif
}

AcrylicScreenCapture& AcrylicScreenCapture::instance() {
    static AcrylicScreenCapture capture;
    return capture;
}

AcrylicScreenCapture::AcrylicScreenCapture() = default;

AcrylicScreenCapture::~AcrylicScreenCapture() {
    resetDevice();
}

bool AcrylicScreenCapture::isAvailable() const {
    return m_available;
}

QImage AcrylicScreenCapture::captureGrid(const QPoint& logicalTopLeft,
                                         const QSize& logicalSize,
                                         qreal devicePixelRatio,
                                         const QSize& gridSize) {
#ifndef Q_OS_WIN
    Q_UNUSED(logicalTopLeft);
    Q_UNUSED(logicalSize);
    Q_UNUSED(devicePixelRatio);
    Q_UNUSED(gridSize);
    return QImage();
#else
    if (logicalSize.width() <= 0 || logicalSize.height() <= 0
        || gridSize.width() <= 0 || gridSize.height() <= 0) {
        return QImage();
    }

    QMutexLocker locker(&m_mutex);
    if (!ensureDevice()) {
        return QImage();
    }

    const qreal dpr = devicePixelRatio > 0.0 ? devicePixelRatio : 1.0;
    const QRect physicalRect(
        qRound(logicalTopLeft.x() * dpr),
        qRound(logicalTopLeft.y() * dpr),
        std::max(1, qRound(logicalSize.width() * dpr)),
        std::max(1, qRound(logicalSize.height() * dpr))
    );

    if (!ensureDuplicationForRect(physicalRect)) {
        return QImage();
    }
    if (!acquireLatestFrame()) {
        return QImage();
    }

    return stableGridImage(copyCurrentFrameRegion(physicalRect, gridSize), physicalRect, gridSize);
#endif
}

bool AcrylicScreenCapture::ensureDevice() {
#ifndef Q_OS_WIN
    return false;
#else
    if (m_initialized) {
        return m_available;
    }

    m_initialized = true;

    static const D3D_FEATURE_LEVEL featureLevels[] = {
        D3D_FEATURE_LEVEL_11_1,
        D3D_FEATURE_LEVEL_11_0,
        D3D_FEATURE_LEVEL_10_1,
        D3D_FEATURE_LEVEL_10_0
    };

    ID3D11Device* device = nullptr;
    ID3D11DeviceContext* context = nullptr;
    D3D_FEATURE_LEVEL selectedFeatureLevel = D3D_FEATURE_LEVEL_11_0;
    HRESULT hr = D3D11CreateDevice(
        nullptr,
        D3D_DRIVER_TYPE_HARDWARE,
        nullptr,
        D3D11_CREATE_DEVICE_BGRA_SUPPORT,
        featureLevels,
        ARRAYSIZE(featureLevels),
        D3D11_SDK_VERSION,
        &device,
        &selectedFeatureLevel,
        &context
    );
    if (FAILED(hr) || !device || !context) {
        releaseCom(device);
        releaseCom(context);
        return false;
    }

    IDXGIDevice* dxgiDevice = nullptr;
    hr = device->QueryInterface(__uuidof(IDXGIDevice), reinterpret_cast<void**>(&dxgiDevice));
    if (FAILED(hr) || !dxgiDevice) {
        releaseCom(device);
        releaseCom(context);
        return false;
    }

    IDXGIAdapter* adapter = nullptr;
    hr = dxgiDevice->GetAdapter(&adapter);
    releaseCom(dxgiDevice);
    if (FAILED(hr) || !adapter) {
        releaseCom(device);
        releaseCom(context);
        return false;
    }

    m_device = device;
    m_context = context;
    m_adapter = adapter;
    m_available = true;
    return true;
#endif
}

bool AcrylicScreenCapture::ensureDuplicationForRect(const QRect& physicalRect) {
#ifndef Q_OS_WIN
    Q_UNUSED(physicalRect);
    return false;
#else
    IDXGIAdapter* adapter = comPtr<IDXGIAdapter>(m_adapter);
    ID3D11Device* device = comPtr<ID3D11Device>(m_device);
    if (!adapter || !device) {
        return false;
    }

    const QPoint center = physicalRect.center();
    int selectedIndex = -1;
    QRect selectedRect;
    IDXGIOutput* selectedOutput = nullptr;

    for (int index = 0;; ++index) {
        IDXGIOutput* output = nullptr;
        HRESULT hr = adapter->EnumOutputs(static_cast<UINT>(index), &output);
        if (hr == DXGI_ERROR_NOT_FOUND) {
            break;
        }
        if (FAILED(hr) || !output) {
            continue;
        }

        DXGI_OUTPUT_DESC desc{};
        if (SUCCEEDED(output->GetDesc(&desc))) {
            const QRect outputRect = rectFromDxgiCoordinates(desc.DesktopCoordinates);
            if (outputRect.contains(center) || outputRect.intersects(physicalRect)) {
                selectedIndex = index;
                selectedRect = outputRect;
                selectedOutput = output;
                break;
            }
        }
        releaseCom(output);
    }

    if (!selectedOutput) {
        return false;
    }

    if (m_duplication && m_outputIndex == selectedIndex && m_outputRect == selectedRect) {
        releaseCom(selectedOutput);
        return true;
    }

    resetDuplication();

    IDXGIOutput1* output1 = nullptr;
    HRESULT hr = selectedOutput->QueryInterface(__uuidof(IDXGIOutput1), reinterpret_cast<void**>(&output1));
    releaseCom(selectedOutput);
    if (FAILED(hr) || !output1) {
        return false;
    }

    IDXGIOutputDuplication* duplication = nullptr;
    hr = output1->DuplicateOutput(device, &duplication);
    releaseCom(output1);
    if (FAILED(hr) || !duplication) {
        return false;
    }

    m_duplication = duplication;
    m_outputIndex = selectedIndex;
    m_outputRect = selectedRect;
    return true;
#endif
}

bool AcrylicScreenCapture::acquireLatestFrame() {
#ifndef Q_OS_WIN
    return false;
#else
    auto* duplication = comPtr<IDXGIOutputDuplication>(m_duplication);
    auto* context = comPtr<ID3D11DeviceContext>(m_context);
    if (!duplication || !context) {
        return false;
    }

    DXGI_OUTDUPL_FRAME_INFO frameInfo{};
    IDXGIResource* resource = nullptr;
    HRESULT hr = duplication->AcquireNextFrame(0, &frameInfo, &resource);
    if (hr == DXGI_ERROR_WAIT_TIMEOUT) {
        return m_desktopCopy != nullptr;
    }
    if (hr == DXGI_ERROR_ACCESS_LOST || hr == DXGI_ERROR_INVALID_CALL) {
        resetDuplication();
        return false;
    }
    if (FAILED(hr) || !resource) {
        return m_desktopCopy != nullptr;
    }

    ID3D11Texture2D* frameTexture = nullptr;
    hr = resource->QueryInterface(__uuidof(ID3D11Texture2D), reinterpret_cast<void**>(&frameTexture));
    releaseCom(resource);
    if (FAILED(hr) || !frameTexture) {
        duplication->ReleaseFrame();
        return false;
    }

    D3D11_TEXTURE2D_DESC desc{};
    frameTexture->GetDesc(&desc);
    const int frameFormat = static_cast<int>(desc.Format);
    if (isSupportedDesktopFormat(frameFormat)
        && ensureDesktopCopyTexture(static_cast<int>(desc.Width),
                                    static_cast<int>(desc.Height),
                                    frameFormat)) {
        context->CopyResource(comPtr<ID3D11Texture2D>(m_desktopCopy), frameTexture);
    }

    releaseCom(frameTexture);
    duplication->ReleaseFrame();
    return m_desktopCopy != nullptr;
#endif
}

QImage AcrylicScreenCapture::copyCurrentFrameRegion(const QRect& physicalRect, const QSize& gridSize) {
#ifndef Q_OS_WIN
    Q_UNUSED(physicalRect);
    Q_UNUSED(gridSize);
    return QImage();
#else
    auto* desktopCopy = comPtr<ID3D11Texture2D>(m_desktopCopy);
    auto* context = comPtr<ID3D11DeviceContext>(m_context);
    if (!desktopCopy || !context || m_outputRect.isEmpty()) {
        return QImage();
    }
    if (!isSupportedDesktopFormat(m_desktopCopyFormat)) {
        return QImage();
    }

    QRect sourceRect = physicalRect.translated(-m_outputRect.topLeft());
    sourceRect = sourceRect.intersected(QRect(0, 0, m_desktopCopyWidth, m_desktopCopyHeight));
    if (sourceRect.width() <= 0 || sourceRect.height() <= 0) {
        return QImage();
    }

    if (!ensureStagingTexture(sourceRect.width(), sourceRect.height(), m_desktopCopyFormat)) {
        return QImage();
    }

    D3D11_BOX sourceBox{};
    sourceBox.left = static_cast<UINT>(sourceRect.left());
    sourceBox.top = static_cast<UINT>(sourceRect.top());
    sourceBox.front = 0;
    sourceBox.right = static_cast<UINT>(sourceRect.right() + 1);
    sourceBox.bottom = static_cast<UINT>(sourceRect.bottom() + 1);
    sourceBox.back = 1;

    auto* stagingTexture = comPtr<ID3D11Texture2D>(m_stagingTexture);
    context->CopySubresourceRegion(stagingTexture, 0, 0, 0, 0, desktopCopy, 0, &sourceBox);

    D3D11_MAPPED_SUBRESOURCE mapped{};
    HRESULT hr = context->Map(stagingTexture, 0, D3D11_MAP_READ, 0, &mapped);
    if (FAILED(hr) || !mapped.pData) {
        return QImage();
    }

    QImage image(sourceRect.size(), QImage::Format_ARGB32_Premultiplied);
    if (!image.isNull()) {
        const auto* sourceBytes = static_cast<const uchar*>(mapped.pData);
        if (isBgraFormat(m_desktopCopyFormat)) {
            const qsizetype targetStride = image.bytesPerLine();
            const qsizetype copyBytes = std::min<qsizetype>(targetStride, sourceRect.width() * 4);
            for (int y = 0; y < sourceRect.height(); ++y) {
                auto* targetLine = reinterpret_cast<QRgb*>(image.scanLine(y));
                std::memcpy(targetLine, sourceBytes + static_cast<qsizetype>(y) * mapped.RowPitch, copyBytes);
                for (int x = 0; x < sourceRect.width(); ++x) {
                    targetLine[x] = targetLine[x] | 0xff000000u;
                }
            }
        } else if (isRgbaFormat(m_desktopCopyFormat)) {
            for (int y = 0; y < sourceRect.height(); ++y) {
                const uchar* sourceLine = sourceBytes + static_cast<qsizetype>(y) * mapped.RowPitch;
                auto* targetLine = reinterpret_cast<QRgb*>(image.scanLine(y));
                for (int x = 0; x < sourceRect.width(); ++x) {
                    const uchar* pixel = sourceLine + static_cast<qsizetype>(x) * 4;
                    targetLine[x] = qRgba(pixel[0], pixel[1], pixel[2], 255);
                }
            }
        }
    }
    context->Unmap(stagingTexture, 0);

    if (image.isNull()) {
        return QImage();
    }

    return image.scaled(gridSize, Qt::IgnoreAspectRatio, Qt::FastTransformation)
        .convertToFormat(QImage::Format_ARGB32_Premultiplied);
#endif
}

bool AcrylicScreenCapture::ensureDesktopCopyTexture(int width, int height, int format) {
#ifndef Q_OS_WIN
    Q_UNUSED(width);
    Q_UNUSED(height);
    Q_UNUSED(format);
    return false;
#else
    if (m_desktopCopy && m_desktopCopyWidth == width && m_desktopCopyHeight == height && m_desktopCopyFormat == format) {
        return true;
    }

    ID3D11Texture2D* oldTexture = comPtr<ID3D11Texture2D>(m_desktopCopy);
    releaseCom(oldTexture);
    m_desktopCopy = nullptr;

    D3D11_TEXTURE2D_DESC desc{};
    desc.Width = static_cast<UINT>(width);
    desc.Height = static_cast<UINT>(height);
    desc.MipLevels = 1;
    desc.ArraySize = 1;
    desc.Format = static_cast<DXGI_FORMAT>(format);
    desc.SampleDesc.Count = 1;
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.BindFlags = 0;
    desc.CPUAccessFlags = 0;
    desc.MiscFlags = 0;

    ID3D11Texture2D* texture = nullptr;
    HRESULT hr = comPtr<ID3D11Device>(m_device)->CreateTexture2D(&desc, nullptr, &texture);
    if (FAILED(hr) || !texture) {
        m_desktopCopyWidth = 0;
        m_desktopCopyHeight = 0;
        m_desktopCopyFormat = 0;
        return false;
    }

    m_desktopCopy = texture;
    m_desktopCopyWidth = width;
    m_desktopCopyHeight = height;
    m_desktopCopyFormat = format;
    return true;
#endif
}

bool AcrylicScreenCapture::ensureStagingTexture(int width, int height, int format) {
#ifndef Q_OS_WIN
    Q_UNUSED(width);
    Q_UNUSED(height);
    Q_UNUSED(format);
    return false;
#else
    if (m_stagingTexture && m_stagingWidth == width && m_stagingHeight == height && m_stagingFormat == format) {
        return true;
    }

    ID3D11Texture2D* oldTexture = comPtr<ID3D11Texture2D>(m_stagingTexture);
    releaseCom(oldTexture);
    m_stagingTexture = nullptr;

    D3D11_TEXTURE2D_DESC desc{};
    desc.Width = static_cast<UINT>(width);
    desc.Height = static_cast<UINT>(height);
    desc.MipLevels = 1;
    desc.ArraySize = 1;
    desc.Format = static_cast<DXGI_FORMAT>(format);
    desc.SampleDesc.Count = 1;
    desc.Usage = D3D11_USAGE_STAGING;
    desc.BindFlags = 0;
    desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    desc.MiscFlags = 0;

    ID3D11Texture2D* texture = nullptr;
    HRESULT hr = comPtr<ID3D11Device>(m_device)->CreateTexture2D(&desc, nullptr, &texture);
    if (FAILED(hr) || !texture) {
        m_stagingWidth = 0;
        m_stagingHeight = 0;
        m_stagingFormat = 0;
        return false;
    }

    m_stagingTexture = texture;
    m_stagingWidth = width;
    m_stagingHeight = height;
    m_stagingFormat = format;
    return true;
#endif
}

QImage AcrylicScreenCapture::stableGridImage(const QImage& image, const QRect& physicalRect, const QSize& gridSize) {
#ifndef Q_OS_WIN
    Q_UNUSED(image);
    Q_UNUSED(physicalRect);
    Q_UNUSED(gridSize);
    return QImage();
#else
    if (image.isNull()) {
        return QImage();
    }

    QImage normalized = image.convertToFormat(QImage::Format_ARGB32_Premultiplied);
    if (looksLikeBlueFlashFrame(normalized)) {
        if (!m_lastGrid.isNull()
            && m_lastGrid.size() == gridSize
            && m_lastGridPhysicalRect == physicalRect) {
            return m_lastGrid;
        }
        return QImage();
    }

    m_lastGrid = normalized;
    m_lastGridPhysicalRect = physicalRect;
    return normalized;
#endif
}

void AcrylicScreenCapture::resetDuplication() {
#ifdef Q_OS_WIN
    auto* duplication = comPtr<IDXGIOutputDuplication>(m_duplication);
    releaseCom(duplication);
    auto* desktopCopy = comPtr<ID3D11Texture2D>(m_desktopCopy);
    releaseCom(desktopCopy);
    auto* stagingTexture = comPtr<ID3D11Texture2D>(m_stagingTexture);
    releaseCom(stagingTexture);
#endif
    m_duplication = nullptr;
    m_desktopCopy = nullptr;
    m_stagingTexture = nullptr;
    m_outputIndex = -1;
    m_outputRect = QRect();
    m_desktopCopyWidth = 0;
    m_desktopCopyHeight = 0;
    m_desktopCopyFormat = 0;
    m_stagingWidth = 0;
    m_stagingHeight = 0;
    m_stagingFormat = 0;
    m_lastGrid = QImage();
    m_lastGridPhysicalRect = QRect();
}

void AcrylicScreenCapture::resetDevice() {
    resetDuplication();
#ifdef Q_OS_WIN
    auto* adapter = comPtr<IDXGIAdapter>(m_adapter);
    releaseCom(adapter);
    auto* context = comPtr<ID3D11DeviceContext>(m_context);
    releaseCom(context);
    auto* device = comPtr<ID3D11Device>(m_device);
    releaseCom(device);
#endif
    m_adapter = nullptr;
    m_context = nullptr;
    m_device = nullptr;
    m_available = false;
    m_initialized = false;
}
