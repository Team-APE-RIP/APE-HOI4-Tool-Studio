//-------------------------------------------------------------------------------------
// WindowAviRecorder.cpp -- Part of APE HOI4 Tool Studio
//
// Copyright (C) 2026 Team APE:RIP. All rights reserved.
// Licensed under the Team APE:RIP Source Code License Agreement.
//
// https://github.com/Team-APE-RIP/APE-HOI4-Tool-Studio/
//-------------------------------------------------------------------------------------
#include "WindowAviRecorder.h"

#include <QByteArray>
#include <QDir>
#include <QFileInfo>
#include <QImage>

#ifdef Q_OS_WIN
#include <windows.h>
#include <vfw.h>
#endif

namespace {
int alignedRgb24Stride(int width) {
    return ((width * 3) + 3) & ~3;
}

QString aviErrorText(const QString& operation, long result) {
    return QStringLiteral("%1 failed with AVI error 0x%2")
        .arg(operation)
        .arg(static_cast<qulonglong>(static_cast<unsigned long>(result)), 8, 16, QLatin1Char('0'));
}
}

struct WindowAviRecorder::Impl {
#ifdef Q_OS_WIN
    PAVIFILE file = nullptr;
    PAVISTREAM stream = nullptr;
    bool aviInitialized = false;
#endif
    QString outputPath;
    QSize frameSize;
    int frameRate = 10;
    int frameIndex = 0;
    QByteArray frameBuffer;
};

WindowAviRecorder::WindowAviRecorder()
    : m_impl(std::make_unique<Impl>()) {
}

WindowAviRecorder::~WindowAviRecorder() {
    stop();
}

bool WindowAviRecorder::start(const QString& outputPath, const QSize& frameSize, int frameRate, QString* errorMessage) {
    stop();

    if (outputPath.trimmed().isEmpty()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Recording output path is empty.");
        }
        return false;
    }
    if (!frameSize.isValid() || frameSize.width() <= 0 || frameSize.height() <= 0) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Recording frame size is invalid.");
        }
        return false;
    }
    if (frameRate <= 0) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Recording frame rate is invalid.");
        }
        return false;
    }

#ifndef Q_OS_WIN
    Q_UNUSED(outputPath);
    Q_UNUSED(frameSize);
    Q_UNUSED(frameRate);
    if (errorMessage) {
        *errorMessage = QStringLiteral("AVI recording is only supported on Windows.");
    }
    return false;
#else
    const QFileInfo outputInfo(outputPath);
    if (!QDir().mkpath(outputInfo.absolutePath())) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Recording output directory could not be created.");
        }
        return false;
    }

    AVIFileInit();
    m_impl->aviInitialized = true;

    const QString nativePath = QDir::toNativeSeparators(outputPath);
    HRESULT result = AVIFileOpenW(
        &m_impl->file,
        reinterpret_cast<LPCWSTR>(nativePath.utf16()),
        OF_WRITE | OF_CREATE,
        nullptr);
    if (result != AVIERR_OK) {
        if (errorMessage) {
            *errorMessage = aviErrorText(QStringLiteral("AVIFileOpenW"), result);
        }
        stop();
        return false;
    }

    AVISTREAMINFOW streamInfo = {};
    streamInfo.fccType = streamtypeVIDEO;
    streamInfo.dwScale = 1;
    streamInfo.dwRate = static_cast<DWORD>(frameRate);
    streamInfo.dwSuggestedBufferSize = static_cast<DWORD>(alignedRgb24Stride(frameSize.width()) * frameSize.height());
    streamInfo.dwQuality = static_cast<DWORD>(-1);
    SetRect(&streamInfo.rcFrame, 0, 0, frameSize.width(), frameSize.height());

    result = AVIFileCreateStreamW(m_impl->file, &m_impl->stream, &streamInfo);
    if (result != AVIERR_OK) {
        if (errorMessage) {
            *errorMessage = aviErrorText(QStringLiteral("AVIFileCreateStreamW"), result);
        }
        stop();
        return false;
    }

    BITMAPINFOHEADER bitmapHeader = {};
    bitmapHeader.biSize = sizeof(BITMAPINFOHEADER);
    bitmapHeader.biWidth = frameSize.width();
    bitmapHeader.biHeight = frameSize.height();
    bitmapHeader.biPlanes = 1;
    bitmapHeader.biBitCount = 24;
    bitmapHeader.biCompression = BI_RGB;
    bitmapHeader.biSizeImage = streamInfo.dwSuggestedBufferSize;

    result = AVIStreamSetFormat(m_impl->stream, 0, &bitmapHeader, sizeof(bitmapHeader));
    if (result != AVIERR_OK) {
        if (errorMessage) {
            *errorMessage = aviErrorText(QStringLiteral("AVIStreamSetFormat"), result);
        }
        stop();
        return false;
    }

    m_impl->outputPath = outputPath;
    m_impl->frameSize = frameSize;
    m_impl->frameRate = frameRate;
    m_impl->frameIndex = 0;
    m_impl->frameBuffer.resize(alignedRgb24Stride(frameSize.width()) * frameSize.height());
    return true;
#endif
}

bool WindowAviRecorder::writeFrame(const QImage& image, QString* errorMessage) {
#ifndef Q_OS_WIN
    Q_UNUSED(image);
    if (errorMessage) {
        *errorMessage = QStringLiteral("AVI recording is only supported on Windows.");
    }
    return false;
#else
    if (!isActive()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Recording stream is not active.");
        }
        return false;
    }
    if (image.isNull()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Recording frame is empty.");
        }
        return false;
    }

    QImage frame = image;
    if (frame.size() != m_impl->frameSize) {
        frame = frame.scaled(m_impl->frameSize, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
    }
    frame = frame.convertToFormat(QImage::Format_RGB888);

    const int width = m_impl->frameSize.width();
    const int height = m_impl->frameSize.height();
    const int destinationStride = alignedRgb24Stride(width);
    m_impl->frameBuffer.fill(0);

    for (int y = 0; y < height; ++y) {
        const uchar* sourceLine = frame.constScanLine(height - 1 - y);
        uchar* destinationLine = reinterpret_cast<uchar*>(m_impl->frameBuffer.data()) + y * destinationStride;
        for (int x = 0; x < width; ++x) {
            const uchar* sourcePixel = sourceLine + x * 3;
            uchar* destinationPixel = destinationLine + x * 3;
            destinationPixel[0] = sourcePixel[2];
            destinationPixel[1] = sourcePixel[1];
            destinationPixel[2] = sourcePixel[0];
        }
    }

    LONG samplesWritten = 0;
    LONG bytesWritten = 0;
    const HRESULT result = AVIStreamWrite(
        m_impl->stream,
        m_impl->frameIndex,
        1,
        m_impl->frameBuffer.data(),
        m_impl->frameBuffer.size(),
        AVIIF_KEYFRAME,
        &samplesWritten,
        &bytesWritten);
    if (result != AVIERR_OK || samplesWritten != 1 || bytesWritten <= 0) {
        if (errorMessage) {
            *errorMessage = result == AVIERR_OK
                ? QStringLiteral("AVIStreamWrite did not accept the recording frame.")
                : aviErrorText(QStringLiteral("AVIStreamWrite"), result);
        }
        return false;
    }

    ++m_impl->frameIndex;
    return true;
#endif
}

void WindowAviRecorder::stop() {
#ifdef Q_OS_WIN
    if (m_impl->stream) {
        AVIStreamRelease(m_impl->stream);
        m_impl->stream = nullptr;
    }
    if (m_impl->file) {
        AVIFileRelease(m_impl->file);
        m_impl->file = nullptr;
    }
    if (m_impl->aviInitialized) {
        AVIFileExit();
        m_impl->aviInitialized = false;
    }
#endif
    m_impl->outputPath.clear();
    m_impl->frameSize = QSize();
    m_impl->frameIndex = 0;
    m_impl->frameBuffer.clear();
}

bool WindowAviRecorder::isActive() const {
#ifdef Q_OS_WIN
    return m_impl->file && m_impl->stream;
#else
    return false;
#endif
}

QString WindowAviRecorder::outputPath() const {
    return m_impl->outputPath;
}

int WindowAviRecorder::frameCount() const {
    return m_impl->frameIndex;
}
