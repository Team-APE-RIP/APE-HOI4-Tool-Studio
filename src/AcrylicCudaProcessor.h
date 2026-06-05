//-------------------------------------------------------------------------------------
// AcrylicCudaProcessor.h -- Part of APE HOI4 Tool Studio
//
// Copyright (C) 2026 Team APE:RIP. All rights reserved.
// Licensed under the Team APE:RIP Source Code License Agreement.
//
// https://github.com/Team-APE-RIP/APE-HOI4-Tool-Studio/
//-------------------------------------------------------------------------------------
#ifndef ACRYLICCUDAPROCESSOR_H
#define ACRYLICCUDAPROCESSOR_H

#include <QColor>
#include <QImage>

class AcrylicCudaProcessor {
public:
    static AcrylicCudaProcessor& instance();

    bool process(const QImage& source,
                 QImage* output,
                 const QColor& tint,
                 int sourceWeight,
                 int saturationPercent);

    bool isAvailable() const;

private:
    AcrylicCudaProcessor();
    ~AcrylicCudaProcessor();

    AcrylicCudaProcessor(const AcrylicCudaProcessor&) = delete;
    AcrylicCudaProcessor& operator=(const AcrylicCudaProcessor&) = delete;

    bool initialize();
    bool ensureDeviceBuffers(qsizetype byteCount);
    void releaseDeviceBuffers();
    void releaseCuda();

    bool m_initialized = false;
    bool m_available = false;
    qsizetype m_deviceBufferBytes = 0;

    void* m_cudaModule = nullptr;
    void* m_cudaKernel = nullptr;
    void* m_cudaContext = nullptr;
    unsigned long long m_sourceDevice = 0;
    unsigned long long m_outputDevice = 0;
};

#endif // ACRYLICCUDAPROCESSOR_H
