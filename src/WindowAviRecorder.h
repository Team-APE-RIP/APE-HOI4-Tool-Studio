//-------------------------------------------------------------------------------------
// WindowAviRecorder.h -- Part of APE HOI4 Tool Studio
//
// Copyright (C) 2026 Team APE:RIP. All rights reserved.
// Licensed under the Team APE:RIP Source Code License Agreement.
//
// https://github.com/Team-APE-RIP/APE-HOI4-Tool-Studio/
//-------------------------------------------------------------------------------------
#ifndef WINDOWAVIRECORDER_H
#define WINDOWAVIRECORDER_H

#include <QSize>
#include <QString>

#include <memory>

class QImage;

class WindowAviRecorder {
public:
    WindowAviRecorder();
    ~WindowAviRecorder();

    bool start(const QString& outputPath, const QSize& frameSize, int frameRate, QString* errorMessage);
    bool writeFrame(const QImage& image, QString* errorMessage);
    void stop();

    bool isActive() const;
    QString outputPath() const;
    int frameCount() const;

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

#endif // WINDOWAVIRECORDER_H
