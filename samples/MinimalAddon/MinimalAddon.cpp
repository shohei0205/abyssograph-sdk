// SPDX-License-Identifier: MIT
// Copyright (c) 2026 shohei-s.com

#include "AbyssoAddon.h"

// ---------------------------------------------------------------------------
// MinimalAddon --- the smallest DLL that works as an add-on.
//
// It exports only the three required functions. It performs no initialization,
// supports no language switching, publishes neither parameter items nor info
// items, and does not support cancellation. It is still loaded by the host,
// can be selected, and produces an image.
//
// AbyssoAddon.h is the only header it includes; the helper header is not used.
// ---------------------------------------------------------------------------

namespace {

// GUID of the add-on. Always generate a new GUID for your own add-on.
// It is stored in project files, so changing it after distribution prevents
// saved values from being restored.
constexpr uint8_t kGuid[16] = {
    0xad, 0x7e, 0x3d, 0x53, 0x81, 0xb9, 0x47, 0xf2,
    0xf2, 0x22, 0xc5, 0xfb, 0xc2, 0x91, 0x02, 0xef
};

// SetAbyssoAddonLang is not implemented, so the name is single-language.
const char* const kName    = "Escape time (minimal sample)";
const char* const kVersion = "1.0.0.0";

constexpr int32_t kMaxIterations = 500;

} // namespace

ABYSSO_ADDON_API int32_t GetAbyssoAddonAbiVersion(void)
{
    return ABYSSO_ADDON_ABI_VERSION;
}

ABYSSO_ADDON_API int32_t GetAbyssoAddonInfo(AbyssoAddonInfo* out)
{
    if (out == nullptr) return ABYSSO_ERROR_INVALID_ARG;
    if (out->structSize < static_cast<int32_t>(sizeof(AbyssoAddonInfo)))
        return ABYSSO_ERROR_BUFFER_TOO_SMALL;

    for (int i = 0; i < 16; ++i) out->guid.bytes[i] = kGuid[i];
    out->name    = kName;
    out->version = kVersion;
    return 0;
}

// A plain escape-time algorithm.
// qualityDivisor is ignored, since honouring it is optional.
// Cancellation is not supported either, so the return value is always 1.
ABYSSO_ADDON_API int32_t RenderAbyssoAddonFrame(const AbyssoRenderRequest* req)
{
    if (req == nullptr) return ABYSSO_ERROR_INVALID_ARG;

    // Only the required fields are read. The render request also carries the
    // timeline position and the cancellation flag, which this add-on does not use.
    const int32_t width      = req->width;
    const int32_t height     = req->height;
    const double  centerX    = req->centerX;
    const double  centerY    = req->centerY;
    const double  scale      = req->scale;
    int32_t* const outPixels = req->outPixels;

    if (width <= 0 || height <= 0 || outPixels == nullptr) return ABYSSO_ERROR_INVALID_ARG;

    // Mapping from screen coordinates to the complex plane. It must match the
    // host; a different mapping shows a different region for the same viewpoint.
    const double aspect = static_cast<double>(width) / static_cast<double>(height);
    const double minX = centerX - scale * aspect;
    const double maxX = centerX + scale * aspect;
    const double minY = centerY - scale;
    const double maxY = centerY + scale;

    for (int32_t py = 0; py < height; ++py)
    {
        const double y0 = minY + (maxY - minY) * py / height;
        for (int32_t px = 0; px < width; ++px)
        {
            const double x0 = minX + (maxX - minX) * px / width;

            double x = 0.0, y = 0.0;
            int32_t iter = 0;
            while (x * x + y * y <= 4.0 && iter < kMaxIterations)
            {
                const double xt = x * x - y * y + x0;
                y = 2.0 * x * y + y0;
                x = xt;
                ++iter;
            }

            // Black inside the set, greyscale outside (0x00RRGGBB).
            const int32_t v = (iter >= kMaxIterations) ? 0 : (iter * 255 / kMaxIterations);
            outPixels[py * width + px] = (v << 16) | (v << 8) | v;
        }
    }

    return 1;
}
