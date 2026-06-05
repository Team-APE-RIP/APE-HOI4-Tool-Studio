//-------------------------------------------------------------------------------------
// ImageProcessor.cpp -- Part of APE HOI4 Tool Studio
//
// Copyright (C) 2026 Team APE:RIP. All rights reserved.
// Licensed under the Team APE:RIP Source Code License Agreement.
//
// https://github.com/Team-APE-RIP/APE-HOI4-Tool-Studio/
//-------------------------------------------------------------------------------------
#include "ImageProcessor.h"

#include <algorithm>
#include <cmath>

namespace Lumorpha {

namespace {
constexpr double kPi = 3.14159265358979323846264338327950288;

Rgba8 transparentPixel() {
    return {};
}

Rgba8 pixelAtClamped(const ImageBuffer& image, int x, int y) {
    if (!isValidImage(image)) {
        return transparentPixel();
    }
    x = std::clamp(x, 0, image.width - 1);
    y = std::clamp(y, 0, image.height - 1);
    return image.pixels[static_cast<std::size_t>(y * image.width + x)];
}

std::uint8_t clampToByte(double value) {
    return static_cast<std::uint8_t>(std::clamp(std::lround(value), 0L, 255L));
}

Rgba8 lerpPixel(const Rgba8& a, const Rgba8& b, double t) {
    return {
        clampToByte(static_cast<double>(a.r) + (static_cast<double>(b.r) - a.r) * t),
        clampToByte(static_cast<double>(a.g) + (static_cast<double>(b.g) - a.g) * t),
        clampToByte(static_cast<double>(a.b) + (static_cast<double>(b.b) - a.b) * t),
        clampToByte(static_cast<double>(a.a) + (static_cast<double>(b.a) - a.a) * t)
    };
}

Rgba8 sampleNearest(const ImageBuffer& image, double x, double y) {
    return pixelAtClamped(image, static_cast<int>(std::floor(x + 0.5)), static_cast<int>(std::floor(y + 0.5)));
}

Rgba8 sampleLinear(const ImageBuffer& image, double x, double y) {
    const int x0 = static_cast<int>(std::floor(x));
    const int y0 = static_cast<int>(std::floor(y));
    const double fx = x - static_cast<double>(x0);
    const double fy = y - static_cast<double>(y0);

    const Rgba8 top = lerpPixel(pixelAtClamped(image, x0, y0), pixelAtClamped(image, x0 + 1, y0), fx);
    const Rgba8 bottom = lerpPixel(pixelAtClamped(image, x0, y0 + 1), pixelAtClamped(image, x0 + 1, y0 + 1), fx);
    return lerpPixel(top, bottom, fy);
}

double cubicWeight(double x) {
    const double ax = std::abs(x);
    if (ax <= 1.0) {
        return (1.5 * ax * ax * ax) - (2.5 * ax * ax) + 1.0;
    }
    if (ax < 2.0) {
        return (-0.5 * ax * ax * ax) + (2.5 * ax * ax) - (4.0 * ax) + 2.0;
    }
    return 0.0;
}

double sinc(double x) {
    if (std::abs(x) < 1.0e-7) {
        return 1.0;
    }
    const double pix = kPi * x;
    return std::sin(pix) / pix;
}

double lanczosWeight(double x) {
    constexpr double a = 3.0;
    const double ax = std::abs(x);
    if (ax >= a) {
        return 0.0;
    }
    return sinc(x) * sinc(x / a);
}

template <typename WeightFn>
Rgba8 sampleKernel(const ImageBuffer& image, double x, double y, int radius, WeightFn weightFn) {
    const int centerX = static_cast<int>(std::floor(x));
    const int centerY = static_cast<int>(std::floor(y));

    double totalWeight = 0.0;
    double red = 0.0;
    double green = 0.0;
    double blue = 0.0;
    double alpha = 0.0;

    for (int yy = centerY - radius + 1; yy <= centerY + radius; ++yy) {
        const double wy = weightFn(y - static_cast<double>(yy));
        if (wy == 0.0) {
            continue;
        }

        for (int xx = centerX - radius + 1; xx <= centerX + radius; ++xx) {
            const double wx = weightFn(x - static_cast<double>(xx));
            const double weight = wx * wy;
            if (weight == 0.0) {
                continue;
            }

            const Rgba8 pixel = pixelAtClamped(image, xx, yy);
            red += static_cast<double>(pixel.r) * weight;
            green += static_cast<double>(pixel.g) * weight;
            blue += static_cast<double>(pixel.b) * weight;
            alpha += static_cast<double>(pixel.a) * weight;
            totalWeight += weight;
        }
    }

    if (std::abs(totalWeight) < 1.0e-7) {
        return sampleLinear(image, x, y);
    }

    return {
        clampToByte(red / totalWeight),
        clampToByte(green / totalWeight),
        clampToByte(blue / totalWeight),
        clampToByte(alpha / totalWeight)
    };
}

Rgba8 sampleBox(const ImageBuffer& image, double left, double top, double right, double bottom) {
    if (right <= left || bottom <= top) {
        return transparentPixel();
    }

    const int xStart = static_cast<int>(std::floor(left));
    const int xEnd = static_cast<int>(std::ceil(right));
    const int yStart = static_cast<int>(std::floor(top));
    const int yEnd = static_cast<int>(std::ceil(bottom));

    double totalWeight = 0.0;
    double red = 0.0;
    double green = 0.0;
    double blue = 0.0;
    double alpha = 0.0;

    for (int y = yStart; y < yEnd; ++y) {
        const double yOverlap = std::max(0.0, std::min(bottom, static_cast<double>(y + 1)) - std::max(top, static_cast<double>(y)));
        if (yOverlap <= 0.0) {
            continue;
        }

        for (int x = xStart; x < xEnd; ++x) {
            const double xOverlap = std::max(0.0, std::min(right, static_cast<double>(x + 1)) - std::max(left, static_cast<double>(x)));
            const double weight = xOverlap * yOverlap;
            if (weight <= 0.0) {
                continue;
            }

            const Rgba8 pixel = pixelAtClamped(image, x, y);
            red += static_cast<double>(pixel.r) * weight;
            green += static_cast<double>(pixel.g) * weight;
            blue += static_cast<double>(pixel.b) * weight;
            alpha += static_cast<double>(pixel.a) * weight;
            totalWeight += weight;
        }
    }

    if (totalWeight <= 0.0) {
        return transparentPixel();
    }

    return {
        clampToByte(red / totalWeight),
        clampToByte(green / totalWeight),
        clampToByte(blue / totalWeight),
        clampToByte(alpha / totalWeight)
    };
}

ResizeFilter normalizeFilter(ResizeFilter filter, int sourceWidth, int sourceHeight, int targetWidth, int targetHeight) {
    if (filter != ResizeFilter::Auto) {
        return filter;
    }
    if (targetWidth < sourceWidth || targetHeight < sourceHeight) {
        return ResizeFilter::Lanczos3;
    }
    return ResizeFilter::Cubic;
}

} // namespace

bool isValidCrop(const ImageBuffer& image, const CropRect& rect) {
    if (!isValidImage(image)) {
        return false;
    }
    if (rect.x < 0 || rect.y < 0 || rect.width <= 0 || rect.height <= 0) {
        return false;
    }
    if (rect.x > image.width - rect.width || rect.y > image.height - rect.height) {
        return false;
    }
    return true;
}

ImageBuffer cropImage(const ImageBuffer& image, const CropRect& rect) {
    if (!isValidCrop(image, rect)) {
        return {};
    }

    ImageBuffer result;
    result.width = rect.width;
    result.height = rect.height;
    result.pixels.resize(checkedPixelCount(result.width, result.height));

    for (int y = 0; y < result.height; ++y) {
        const auto sourceOffset = static_cast<std::size_t>((rect.y + y) * image.width + rect.x);
        const auto destOffset = static_cast<std::size_t>(y * result.width);
        std::copy_n(
            image.pixels.begin() + static_cast<std::ptrdiff_t>(sourceOffset),
            result.width,
            result.pixels.begin() + static_cast<std::ptrdiff_t>(destOffset)
        );
    }

    return result;
}

ImageBuffer resizeImage(const ImageBuffer& image, int targetWidth, int targetHeight, ResizeFilter filter) {
    if (!isValidImage(image) || !imageDimensionsAreSafe(targetWidth, targetHeight)) {
        return {};
    }

    ImageBuffer result;
    result.width = targetWidth;
    result.height = targetHeight;
    result.pixels.resize(checkedPixelCount(targetWidth, targetHeight));

    const ResizeFilter activeFilter = normalizeFilter(filter, image.width, image.height, targetWidth, targetHeight);
    const double scaleX = static_cast<double>(image.width) / static_cast<double>(targetWidth);
    const double scaleY = static_cast<double>(image.height) / static_cast<double>(targetHeight);

    for (int y = 0; y < targetHeight; ++y) {
        const double sourceY = (static_cast<double>(y) + 0.5) * scaleY - 0.5;
        for (int x = 0; x < targetWidth; ++x) {
            const double sourceX = (static_cast<double>(x) + 0.5) * scaleX - 0.5;
            Rgba8 sampled;

            switch (activeFilter) {
            case ResizeFilter::Nearest:
                sampled = sampleNearest(image, sourceX, sourceY);
                break;
            case ResizeFilter::Linear:
            case ResizeFilter::Triangle:
                sampled = sampleLinear(image, sourceX, sourceY);
                break;
            case ResizeFilter::Cubic:
                sampled = sampleKernel(image, sourceX, sourceY, 2, cubicWeight);
                break;
            case ResizeFilter::Box:
                sampled = sampleBox(image, x * scaleX, y * scaleY, (x + 1) * scaleX, (y + 1) * scaleY);
                break;
            case ResizeFilter::Lanczos3:
            case ResizeFilter::Auto:
            default:
                sampled = sampleKernel(image, sourceX, sourceY, 3, lanczosWeight);
                break;
            }

            result.pixels[static_cast<std::size_t>(y * targetWidth + x)] = sampled;
        }
    }

    return result;
}

ImageBuffer cropResizeImage(const ImageBuffer& image,
                            const CropRect& rect,
                            int targetWidth,
                            int targetHeight,
                            ResizeFilter filter) {
    const ImageBuffer cropped = cropImage(image, rect);
    if (!isValidImage(cropped)) {
        return {};
    }
    return resizeImage(cropped, targetWidth, targetHeight, filter);
}

} // namespace Lumorpha
