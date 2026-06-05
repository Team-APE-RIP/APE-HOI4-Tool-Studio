//-------------------------------------------------------------------------------------
// AcrylicCudaProcessor.cpp -- Part of APE HOI4 Tool Studio
//
// Copyright (C) 2026 Team APE:RIP. All rights reserved.
// Licensed under the Team APE:RIP Source Code License Agreement.
//
// https://github.com/Team-APE-RIP/APE-HOI4-Tool-Studio/
//-------------------------------------------------------------------------------------
#include "AcrylicCudaProcessor.h"

#include <QtGlobal>

#ifdef Q_OS_WIN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace {
constexpr int kCudaBlockWidth = 16;
constexpr int kCudaBlockHeight = 16;

#ifdef Q_OS_WIN
using CUdevice = int;
using CUdeviceptr = unsigned long long;
using CUfunction = void*;
using CUmodule = void*;
using CUcontext = void*;
using CUstream = void*;
using CUresult = int;

constexpr CUresult kCudaSuccess = 0;

using CuInitFn = CUresult (WINAPI *)(unsigned int);
using CuDeviceGetCountFn = CUresult (WINAPI *)(int*);
using CuDeviceGetFn = CUresult (WINAPI *)(CUdevice*, int);
using CuCtxCreateFn = CUresult (WINAPI *)(CUcontext*, unsigned int, CUdevice);
using CuCtxDestroyFn = CUresult (WINAPI *)(CUcontext);
using CuCtxSetCurrentFn = CUresult (WINAPI *)(CUcontext);
using CuModuleLoadDataFn = CUresult (WINAPI *)(CUmodule*, const void*);
using CuModuleUnloadFn = CUresult (WINAPI *)(CUmodule);
using CuModuleGetFunctionFn = CUresult (WINAPI *)(CUfunction*, CUmodule, const char*);
using CuMemAllocFn = CUresult (WINAPI *)(CUdeviceptr*, size_t);
using CuMemFreeFn = CUresult (WINAPI *)(CUdeviceptr);
using CuMemcpyHtoDFn = CUresult (WINAPI *)(CUdeviceptr, const void*, size_t);
using CuMemcpyDtoHFn = CUresult (WINAPI *)(void*, CUdeviceptr, size_t);
using CuLaunchKernelFn = CUresult (WINAPI *)(CUfunction,
                                             unsigned int,
                                             unsigned int,
                                             unsigned int,
                                             unsigned int,
                                             unsigned int,
                                             unsigned int,
                                             unsigned int,
                                             CUstream,
                                             void**,
                                             void**);
using CuCtxSynchronizeFn = CUresult (WINAPI *)();

struct CudaDriverApi {
    HMODULE module = nullptr;
    CuInitFn init = nullptr;
    CuDeviceGetCountFn deviceGetCount = nullptr;
    CuDeviceGetFn deviceGet = nullptr;
    CuCtxCreateFn ctxCreate = nullptr;
    CuCtxDestroyFn ctxDestroy = nullptr;
    CuCtxSetCurrentFn ctxSetCurrent = nullptr;
    CuModuleLoadDataFn moduleLoadData = nullptr;
    CuModuleUnloadFn moduleUnload = nullptr;
    CuModuleGetFunctionFn moduleGetFunction = nullptr;
    CuMemAllocFn memAlloc = nullptr;
    CuMemFreeFn memFree = nullptr;
    CuMemcpyHtoDFn memcpyHtoD = nullptr;
    CuMemcpyDtoHFn memcpyDtoH = nullptr;
    CuLaunchKernelFn launchKernel = nullptr;
    CuCtxSynchronizeFn ctxSynchronize = nullptr;

    bool load() {
        if (module) {
            return true;
        }

        module = LoadLibraryW(L"nvcuda.dll");
        if (!module) {
            return false;
        }

        auto resolve = [this](const char* name) -> FARPROC {
            return GetProcAddress(module, name);
        };

        init = reinterpret_cast<CuInitFn>(resolve("cuInit"));
        deviceGetCount = reinterpret_cast<CuDeviceGetCountFn>(resolve("cuDeviceGetCount"));
        deviceGet = reinterpret_cast<CuDeviceGetFn>(resolve("cuDeviceGet"));
        ctxCreate = reinterpret_cast<CuCtxCreateFn>(resolve("cuCtxCreate_v2"));
        ctxDestroy = reinterpret_cast<CuCtxDestroyFn>(resolve("cuCtxDestroy_v2"));
        ctxSetCurrent = reinterpret_cast<CuCtxSetCurrentFn>(resolve("cuCtxSetCurrent"));
        moduleLoadData = reinterpret_cast<CuModuleLoadDataFn>(resolve("cuModuleLoadData"));
        moduleUnload = reinterpret_cast<CuModuleUnloadFn>(resolve("cuModuleUnload"));
        moduleGetFunction = reinterpret_cast<CuModuleGetFunctionFn>(resolve("cuModuleGetFunction"));
        memAlloc = reinterpret_cast<CuMemAllocFn>(resolve("cuMemAlloc_v2"));
        memFree = reinterpret_cast<CuMemFreeFn>(resolve("cuMemFree_v2"));
        memcpyHtoD = reinterpret_cast<CuMemcpyHtoDFn>(resolve("cuMemcpyHtoD_v2"));
        memcpyDtoH = reinterpret_cast<CuMemcpyDtoHFn>(resolve("cuMemcpyDtoH_v2"));
        launchKernel = reinterpret_cast<CuLaunchKernelFn>(resolve("cuLaunchKernel"));
        ctxSynchronize = reinterpret_cast<CuCtxSynchronizeFn>(resolve("cuCtxSynchronize"));

        return init && deviceGetCount && deviceGet && ctxCreate && ctxDestroy && ctxSetCurrent
            && moduleLoadData && moduleUnload && moduleGetFunction && memAlloc && memFree
            && memcpyHtoD && memcpyDtoH && launchKernel && ctxSynchronize;
    }
};

CudaDriverApi& cudaApi() {
    static CudaDriverApi api;
    return api;
}

constexpr const char* kAcrylicCudaPtx = R"ptx(
.version 6.0
.target sm_30
.address_size 64

.visible .entry acrylic_blur_tint(
    .param .u64 srcPtr,
    .param .u64 dstPtr,
    .param .u32 width,
    .param .u32 height,
    .param .u32 tintR,
    .param .u32 tintG,
    .param .u32 tintB,
    .param .u32 sourceWeight,
    .param .u32 saturation
)
{
    .reg .pred %p<8>;
    .reg .b32 %r<96>;
    .reg .b64 %rd<8>;

    ld.param.u64 %rd1, [srcPtr];
    ld.param.u64 %rd2, [dstPtr];
    ld.param.u32 %r1, [width];
    ld.param.u32 %r2, [height];
    ld.param.u32 %r3, [tintR];
    ld.param.u32 %r4, [tintG];
    ld.param.u32 %r5, [tintB];
    ld.param.u32 %r6, [sourceWeight];
    ld.param.u32 %r7, [saturation];

    mov.u32 %r8, %ctaid.x;
    mov.u32 %r9, %ntid.x;
    mov.u32 %r10, %tid.x;
    mad.lo.s32 %r11, %r8, %r9, %r10;
    mov.u32 %r12, %ctaid.y;
    mov.u32 %r13, %ntid.y;
    mov.u32 %r14, %tid.y;
    mad.lo.s32 %r15, %r12, %r13, %r14;

    setp.ge.s32 %p1, %r11, %r1;
    @%p1 bra DONE;
    setp.ge.s32 %p2, %r15, %r2;
    @%p2 bra DONE;

    sub.s32 %r16, %r1, 1;
    sub.s32 %r17, %r2, 1;
    mov.s32 %r40, 0;
    mov.s32 %r41, 0;
    mov.s32 %r42, 0;
    mov.s32 %r20, -4;

DY_LOOP:
    setp.gt.s32 %p3, %r20, 4;
    @%p3 bra DY_DONE;
    add.s32 %r21, %r15, %r20;
    max.s32 %r21, %r21, 0;
    min.s32 %r21, %r21, %r17;
    abs.s32 %r22, %r20;
    sub.s32 %r23, 5, %r22;
    mov.s32 %r24, -4;

DX_LOOP:
    setp.gt.s32 %p4, %r24, 4;
    @%p4 bra DX_DONE;
    add.s32 %r25, %r11, %r24;
    max.s32 %r25, %r25, 0;
    min.s32 %r25, %r25, %r16;
    abs.s32 %r26, %r24;
    sub.s32 %r27, 5, %r26;
    mul.lo.s32 %r28, %r23, %r27;
    mad.lo.s32 %r29, %r21, %r1, %r25;
    mul.wide.s32 %rd3, %r29, 4;
    add.s64 %rd4, %rd1, %rd3;
    ld.global.u32 %r30, [%rd4];
    and.b32 %r31, %r30, 255;
    shr.u32 %r32, %r30, 8;
    and.b32 %r32, %r32, 255;
    shr.u32 %r33, %r30, 16;
    and.b32 %r33, %r33, 255;
    mad.lo.s32 %r40, %r33, %r28, %r40;
    mad.lo.s32 %r41, %r32, %r28, %r41;
    mad.lo.s32 %r42, %r31, %r28, %r42;
    add.s32 %r24, %r24, 1;
    bra DX_LOOP;

DX_DONE:
    add.s32 %r20, %r20, 1;
    bra DY_LOOP;

DY_DONE:
    add.s32 %r40, %r40, 312;
    add.s32 %r41, %r41, 312;
    add.s32 %r42, %r42, 312;
    div.s32 %r50, %r40, 625;
    div.s32 %r51, %r41, 625;
    div.s32 %r52, %r42, 625;

    mul.lo.s32 %r53, %r50, 30;
    mad.lo.s32 %r53, %r51, 59, %r53;
    mad.lo.s32 %r53, %r52, 11, %r53;
    div.s32 %r53, %r53, 100;

    sub.s32 %r54, %r50, %r53;
    mul.lo.s32 %r54, %r54, %r7;
    div.s32 %r54, %r54, 100;
    add.s32 %r50, %r53, %r54;
    sub.s32 %r55, %r51, %r53;
    mul.lo.s32 %r55, %r55, %r7;
    div.s32 %r55, %r55, 100;
    add.s32 %r51, %r53, %r55;
    sub.s32 %r56, %r52, %r53;
    mul.lo.s32 %r56, %r56, %r7;
    div.s32 %r56, %r56, 100;
    add.s32 %r52, %r53, %r56;

    sub.s32 %r57, 100, %r6;
    mul.lo.s32 %r58, %r50, %r6;
    mad.lo.s32 %r58, %r3, %r57, %r58;
    div.s32 %r50, %r58, 100;
    mul.lo.s32 %r59, %r51, %r6;
    mad.lo.s32 %r59, %r4, %r57, %r59;
    div.s32 %r51, %r59, 100;
    mul.lo.s32 %r60, %r52, %r6;
    mad.lo.s32 %r60, %r5, %r57, %r60;
    div.s32 %r52, %r60, 100;

    max.s32 %r50, %r50, 0;
    min.s32 %r50, %r50, 255;
    max.s32 %r51, %r51, 0;
    min.s32 %r51, %r51, 255;
    max.s32 %r52, %r52, 0;
    min.s32 %r52, %r52, 255;

    shl.b32 %r61, %r50, 16;
    shl.b32 %r62, %r51, 8;
    or.b32 %r61, %r61, %r62;
    or.b32 %r61, %r61, %r52;
    or.b32 %r61, %r61, 0xff000000;
    mad.lo.s32 %r63, %r15, %r1, %r11;
    mul.wide.s32 %rd5, %r63, 4;
    add.s64 %rd6, %rd2, %rd5;
    st.global.u32 [%rd6], %r61;

DONE:
    ret;
}
)ptx";
#endif
}

AcrylicCudaProcessor& AcrylicCudaProcessor::instance() {
    static AcrylicCudaProcessor processor;
    return processor;
}

AcrylicCudaProcessor::AcrylicCudaProcessor() = default;

AcrylicCudaProcessor::~AcrylicCudaProcessor() {
    releaseCuda();
}

bool AcrylicCudaProcessor::isAvailable() const {
    return m_available;
}

bool AcrylicCudaProcessor::process(const QImage& source,
                                   QImage* output,
                                   const QColor& tint,
                                   int sourceWeight,
                                   int saturationPercent) {
#ifndef Q_OS_WIN
    Q_UNUSED(source);
    Q_UNUSED(output);
    Q_UNUSED(tint);
    Q_UNUSED(sourceWeight);
    Q_UNUSED(saturationPercent);
    return false;
#else
    if (!output || source.isNull() || source.width() <= 0 || source.height() <= 0) {
        return false;
    }
    if (!initialize()) {
        return false;
    }

    QImage input = source.convertToFormat(QImage::Format_ARGB32_Premultiplied);
    if (input.bytesPerLine() != input.width() * 4) {
        input = input.copy();
    }

    QImage cudaOutput(input.size(), QImage::Format_ARGB32_Premultiplied);
    if (cudaOutput.isNull()) {
        return false;
    }

    const qsizetype byteCount = static_cast<qsizetype>(input.width()) * input.height() * 4;
    if (!ensureDeviceBuffers(byteCount)) {
        return false;
    }

    CudaDriverApi& api = cudaApi();
    if (api.ctxSetCurrent(m_cudaContext) != kCudaSuccess) {
        return false;
    }

    if (api.memcpyHtoD(m_sourceDevice, input.constBits(), static_cast<size_t>(byteCount)) != kCudaSuccess) {
        return false;
    }

    CUdeviceptr sourceDevice = m_sourceDevice;
    CUdeviceptr outputDevice = m_outputDevice;
    unsigned int width = static_cast<unsigned int>(input.width());
    unsigned int height = static_cast<unsigned int>(input.height());
    unsigned int tintRed = static_cast<unsigned int>(tint.red());
    unsigned int tintGreen = static_cast<unsigned int>(tint.green());
    unsigned int tintBlue = static_cast<unsigned int>(tint.blue());
    unsigned int sourceMix = static_cast<unsigned int>(qBound(0, sourceWeight, 100));
    unsigned int saturation = static_cast<unsigned int>(qBound(0, saturationPercent, 200));
    void* parameters[] = {
        &sourceDevice,
        &outputDevice,
        &width,
        &height,
        &tintRed,
        &tintGreen,
        &tintBlue,
        &sourceMix,
        &saturation
    };

    const unsigned int gridX = (width + kCudaBlockWidth - 1) / kCudaBlockWidth;
    const unsigned int gridY = (height + kCudaBlockHeight - 1) / kCudaBlockHeight;
    const CUresult launchResult = api.launchKernel(
        m_cudaKernel,
        gridX,
        gridY,
        1,
        kCudaBlockWidth,
        kCudaBlockHeight,
        1,
        0,
        nullptr,
        parameters,
        nullptr
    );
    if (launchResult != kCudaSuccess || api.ctxSynchronize() != kCudaSuccess) {
        return false;
    }

    if (api.memcpyDtoH(cudaOutput.bits(), m_outputDevice, static_cast<size_t>(byteCount)) != kCudaSuccess) {
        return false;
    }

    *output = cudaOutput;
    return true;
#endif
}

bool AcrylicCudaProcessor::initialize() {
#ifndef Q_OS_WIN
    return false;
#else
    if (m_initialized) {
        return m_available;
    }

    m_initialized = true;
    CudaDriverApi& api = cudaApi();
    if (!api.load() || api.init(0) != kCudaSuccess) {
        return false;
    }

    int deviceCount = 0;
    if (api.deviceGetCount(&deviceCount) != kCudaSuccess || deviceCount <= 0) {
        return false;
    }

    CUdevice device = 0;
    if (api.deviceGet(&device, 0) != kCudaSuccess) {
        return false;
    }

    if (api.ctxCreate(reinterpret_cast<CUcontext*>(&m_cudaContext), 0, device) != kCudaSuccess) {
        return false;
    }

    if (api.moduleLoadData(reinterpret_cast<CUmodule*>(&m_cudaModule), kAcrylicCudaPtx) != kCudaSuccess) {
        releaseCuda();
        return false;
    }

    if (api.moduleGetFunction(reinterpret_cast<CUfunction*>(&m_cudaKernel),
                              static_cast<CUmodule>(m_cudaModule),
                              "acrylic_blur_tint") != kCudaSuccess) {
        releaseCuda();
        return false;
    }

    m_available = true;
    return true;
#endif
}

bool AcrylicCudaProcessor::ensureDeviceBuffers(qsizetype byteCount) {
#ifndef Q_OS_WIN
    Q_UNUSED(byteCount);
    return false;
#else
    if (byteCount <= 0) {
        return false;
    }
    if (m_sourceDevice && m_outputDevice && m_deviceBufferBytes >= byteCount) {
        return true;
    }

    releaseDeviceBuffers();
    CudaDriverApi& api = cudaApi();
    if (api.memAlloc(&m_sourceDevice, static_cast<size_t>(byteCount)) != kCudaSuccess) {
        m_sourceDevice = 0;
        return false;
    }
    if (api.memAlloc(&m_outputDevice, static_cast<size_t>(byteCount)) != kCudaSuccess) {
        api.memFree(m_sourceDevice);
        m_sourceDevice = 0;
        m_outputDevice = 0;
        return false;
    }

    m_deviceBufferBytes = byteCount;
    return true;
#endif
}

void AcrylicCudaProcessor::releaseDeviceBuffers() {
#ifdef Q_OS_WIN
    CudaDriverApi& api = cudaApi();
    if (m_sourceDevice && api.memFree) {
        api.memFree(m_sourceDevice);
    }
    if (m_outputDevice && api.memFree) {
        api.memFree(m_outputDevice);
    }
#endif
    m_sourceDevice = 0;
    m_outputDevice = 0;
    m_deviceBufferBytes = 0;
}

void AcrylicCudaProcessor::releaseCuda() {
#ifdef Q_OS_WIN
    CudaDriverApi& api = cudaApi();
    releaseDeviceBuffers();
    if (m_cudaModule && api.moduleUnload) {
        api.moduleUnload(static_cast<CUmodule>(m_cudaModule));
    }
    if (m_cudaContext && api.ctxDestroy) {
        api.ctxDestroy(static_cast<CUcontext>(m_cudaContext));
    }
#endif
    m_cudaModule = nullptr;
    m_cudaKernel = nullptr;
    m_cudaContext = nullptr;
    m_available = false;
}
