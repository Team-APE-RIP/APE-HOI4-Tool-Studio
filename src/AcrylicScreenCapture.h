//-------------------------------------------------------------------------------------
// AcrylicScreenCapture.h -- Part of APE HOI4 Tool Studio
//
// Copyright (C) 2026 Team APE:RIP. All rights reserved.
// Licensed under the Team APE:RIP Source Code License Agreement.
//
// https://github.com/Team-APE-RIP/APE-HOI4-Tool-Studio/
//-------------------------------------------------------------------------------------
#ifndef ACRYLICSCREENCAPTURE_H
#define ACRYLICSCREENCAPTURE_H

#include <QImage>
#include <QMutex>
#include <QPoint>
#include <QRect>
#include <QSize>

class AcrylicScreenCapture {
public:
    static AcrylicScreenCapture& instance();

    QImage captureGrid(const QPoint& logicalTopLeft,
                       const QSize& logicalSize,
                       qreal devicePixelRatio,
                       const QSize& gridSize);

    bool isAvailable() const;

private:
    AcrylicScreenCapture();
    ~AcrylicScreenCapture();

    AcrylicScreenCapture(const AcrylicScreenCapture&) = delete;
    AcrylicScreenCapture& operator=(const AcrylicScreenCapture&) = delete;

    bool ensureDevice();
    bool ensureDuplicationForRect(const QRect& physicalRect);
    bool acquireLatestFrame();
    QImage copyCurrentFrameRegion(const QRect& physicalRect, const QSize& gridSize);
    bool ensureDesktopCopyTexture(int width, int height, int format);
    bool ensureStagingTexture(int width, int height, int format);
    QImage stableGridImage(const QImage& image, const QRect& physicalRect, const QSize& gridSize);
    void resetDuplication();
    void resetDevice();

    mutable QMutex m_mutex;
    bool m_initialized = false;
    bool m_available = false;
    int m_outputIndex = -1;
    QRect m_outputRect;
    int m_desktopCopyWidth = 0;
    int m_desktopCopyHeight = 0;
    int m_desktopCopyFormat = 0;
    int m_stagingWidth = 0;
    int m_stagingHeight = 0;
    int m_stagingFormat = 0;
    QRect m_lastGridPhysicalRect;
    QImage m_lastGrid;

    void* m_device = nullptr;
    void* m_context = nullptr;
    void* m_adapter = nullptr;
    void* m_duplication = nullptr;
    void* m_desktopCopy = nullptr;
    void* m_stagingTexture = nullptr;
};

#endif // ACRYLICSCREENCAPTURE_H
