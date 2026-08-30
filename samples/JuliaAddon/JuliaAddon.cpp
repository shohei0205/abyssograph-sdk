// SPDX-License-Identifier: MIT
// Copyright (c) 2026 shohei-s.com

#include "AbyssoAddon.h"
#include "AbyssoAddonKit.h"

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstring>
#include <thread>
#include <vector>

// ---------------------------------------------------------------------------
// JuliaAddon --- an add-on with parameters, written with the helper header
// (AbyssoAddonKit.h).
//
// What this sample demonstrates:
//   1. A single Double item that changes the image substantially (the value c)
//   2. Dependent items through ABYSSO_CHANGE_PROPS --- selecting a preset
//      updates c, and editing c returns the preset to "Manual". Changes to
//      other items are reported without any callback into the host
//   3. The main item types (ComboBox / ComboBoxItem / CheckBox / Double /
//      Int32 / Group / Description)
//
// Not one line implements an exported function: once the items are registered,
// AddonKit implements GetAbyssoAddonProp, SetAbyssoAddonValue and
// ResetAbyssoAddonValue. Calculation and colorization live in this file, which
// does not link against the application.
// ---------------------------------------------------------------------------

namespace {

// GUIDs. Always generate new ones for your own add-on.
// Add-on: 7276a1f7-bcbd-4496-a390-dc547f4f7407
constexpr AbyssoGuid kAddonGuid  = ABYSSO_GUID(72,76,a1,f7,bc,bd,44,96,a3,90,dc,54,7f,4f,74,07);

constexpr AbyssoGuid kGroupJulia = ABYSSO_GUID(46,45,fe,e0,3d,c2,4f,43,8a,8c,d6,a4,59,97,e0,0c);
constexpr AbyssoGuid kComboPreset= ABYSSO_GUID(44,04,67,15,fe,59,48,40,8b,d2,63,52,6a,6f,df,4f);
constexpr AbyssoGuid kPreManual  = ABYSSO_GUID(cf,90,45,2c,09,81,4c,32,96,f9,db,6f,02,c9,1e,98);
constexpr AbyssoGuid kPreRabbit  = ABYSSO_GUID(1a,ad,2e,ac,f8,c9,4a,07,b3,40,6b,94,70,bb,69,b0);
constexpr AbyssoGuid kPreDendrite= ABYSSO_GUID(93,b2,26,76,c5,9d,48,c7,a4,17,d3,63,95,69,b4,42);
constexpr AbyssoGuid kPreSanMarco= ABYSSO_GUID(b2,6b,26,9e,96,d8,4c,14,91,49,e6,e7,ee,00,d6,cd);
constexpr AbyssoGuid kPreSiegel  = ABYSSO_GUID(50,6f,c5,61,dc,2c,40,54,83,ba,98,b7,a6,ea,0e,06);
constexpr AbyssoGuid kPreSpiral  = ABYSSO_GUID(7b,76,78,74,be,ac,47,b2,a9,c3,ca,e0,2f,d0,55,4b);
constexpr AbyssoGuid kReal       = ABYSSO_GUID(1e,1b,46,ce,27,5d,43,41,91,5a,14,6f,a5,e4,a5,8a);
constexpr AbyssoGuid kImag       = ABYSSO_GUID(ab,ed,16,51,bb,ea,42,d8,9c,63,13,24,ed,b4,2c,fb);
constexpr AbyssoGuid kPresetHint = ABYSSO_GUID(ef,76,07,22,02,61,4e,27,93,d9,64,db,e9,9e,ed,11);

constexpr AbyssoGuid kGroupColor = ABYSSO_GUID(de,cd,17,96,a4,45,4b,88,ad,28,15,b0,54,82,f4,8a);
constexpr AbyssoGuid kComboPal   = ABYSSO_GUID(90,6b,16,a0,05,ed,49,72,b6,d9,70,ba,6d,3f,87,56);
constexpr AbyssoGuid kPalFire    = ABYSSO_GUID(41,eb,4d,08,ab,2c,40,af,b4,9c,ce,42,04,21,2e,53);
constexpr AbyssoGuid kPalIce     = ABYSSO_GUID(15,9f,d2,1f,8c,16,4a,66,8b,f8,09,58,49,23,75,17);
constexpr AbyssoGuid kPalRainbow = ABYSSO_GUID(14,fe,f7,4a,37,db,4f,90,b4,fd,e8,97,79,e5,5b,91);
constexpr AbyssoGuid kPalMono    = ABYSSO_GUID(20,b9,5b,3b,c7,47,43,df,80,c5,56,d0,51,ac,98,0f);
constexpr AbyssoGuid kCheckSmooth= ABYSSO_GUID(28,56,b1,38,1b,02,44,7a,a4,ef,8c,19,a7,d4,82,8d);
constexpr AbyssoGuid kCycle      = ABYSSO_GUID(0c,0d,c3,cd,ab,26,4b,d6,ae,95,29,c9,b3,f4,18,a6);

constexpr AbyssoGuid kGroupCompute = ABYSSO_GUID(69,0f,b7,2b,67,b5,4a,c3,bf,72,4b,21,1e,9f,2a,f6);
constexpr AbyssoGuid kIterations   = ABYSSO_GUID(89,78,49,e6,0e,b9,44,f4,9a,c7,8e,e6,bd,7b,a3,0b);
constexpr AbyssoGuid kIterHint     = ABYSSO_GUID(15,b9,c3,21,be,b8,47,ad,a3,cc,4b,37,e2,af,a0,93);

constexpr AbyssoGuid kInfoC        = ABYSSO_GUID(c6,fe,de,0a,51,05,4a,a3,83,92,ea,34,70,2b,c7,1d);
constexpr AbyssoGuid kInfoIter     = ABYSSO_GUID(6b,45,15,96,2d,5c,47,37,9a,66,ef,fe,37,16,40,7b);
constexpr AbyssoGuid kInfoPalette  = ABYSSO_GUID(c2,52,0c,df,cb,81,46,90,87,45,7b,35,92,20,15,bd);
constexpr AbyssoGuid kInfoInterior = ABYSSO_GUID(25,ce,14,91,1b,27,4c,4d,90,c9,48,bf,28,d0,15,6d);

// ---------------------------------------------------------------------------
// Values --- AddonKit holds no values; it binds to the variables defined here.
// ---------------------------------------------------------------------------

constexpr int32_t kPaletteFire    = 0;
constexpr int32_t kPaletteIce     = 1;
constexpr int32_t kPaletteRainbow = 2;
constexpr int32_t kPaletteMono    = 3;

// The order must match the order of the ComboBox child items.
struct Preset { double re; double im; };
constexpr Preset kPresets[] = {
    { 0.0,     0.0    }, // [0] Manual (the value is unused)
    { -0.123,  0.745  }, // Douady rabbit
    {  0.0,    1.0    }, // Dendrite
    { -0.75,   0.0    }, // San Marco
    { -0.391, -0.587  }, // Siegel disk
    { -0.7269, 0.1889 }, // Spiral
};
constexpr int kPresetCount = static_cast<int>(sizeof(kPresets) / sizeof(kPresets[0]));

struct Values
{
    int32_t preset     = 1;
    double  cr         = kPresets[1].re;
    double  ci         = kPresets[1].im;
    int32_t palette    = kPaletteFire;
    int32_t smoothing  = 1;
    double  cycle      = 16.0;
    int32_t iterations = 0;
};

// Sentinel for points inside the set (points that never escaped). A negative
// sentinel cannot be used: a smoothed iteration count can itself be negative
// for a point that escapes on the first iteration.
constexpr float kInterior = 1.0e30f;

struct AddonState
{
    Values values;
    bool   initialized = false;

    std::vector<float> source; // Iteration counts, at the reduced resolution
};

AddonState& State()
{
    static AddonState state;
    return state;
}

Values& V() { return State().values; }

// ---------------------------------------------------------------------------
// Dependent items (preset and c, in both directions)
//
// OnChange and OnReset are called after the bound variable has been updated.
// ---------------------------------------------------------------------------

int32_t ApplyPreset()
{
    const int32_t index = V().preset;
    // Selecting "Manual" alone does not change any value.
    if (index <= 0 || index >= kPresetCount) return abysso::kNoRedraw;

    V().cr = kPresets[index].re;
    V().ci = kPresets[index].im;
    return ABYSSO_CHANGE_PROPS;
}

int32_t BackToManual()
{
    if (V().preset == 0) return ABYSSO_CHANGE_NONE;
    V().preset = 0;
    return ABYSSO_CHANGE_PROPS;
}

// The preset and c are linked, so resetting any of them resets all three items.
int32_t ResetJulia()
{
    V().preset = 1;
    V().cr = kPresets[1].re;
    V().ci = kPresets[1].im;
    return ABYSSO_CHANGE_PROPS;
}

// ---------------------------------------------------------------------------
// Item tree --- the parameter UI of this add-on is defined entirely by this registration.
// ---------------------------------------------------------------------------

abysso::AddonKit& Kit()
{
    static abysso::AddonKit kit([](abysso::AddonKit& k) {
        auto julia = k.Group(kGroupJulia, { "JULIA SET", "ジュリア集合" });
        julia.Combo(kComboPreset, { "Preset", "プリセット" }, &V().preset)
             .Item(kPreManual,   { "Manual",        "手動" })
             .Item(kPreRabbit,   { "Douady rabbit", "うさぎ" })
             .Item(kPreDendrite, { "Dendrite",      "樹状" })
             .Item(kPreSanMarco, { "San Marco",     "サンマルコ" })
             .Item(kPreSiegel,   { "Siegel disk",   "ジーゲル円板" })
             .Item(kPreSpiral,   { "Spiral",        "渦巻き" })
             .Default(1)
             .OnChange(ApplyPreset)
             .OnReset(ResetJulia);
        julia.Double(kReal, { "c (real)", "c の実部" }, &V().cr)
             .Range(-2.0, 2.0).Step(0.001).Default(kPresets[1].re)
             .OnChange(BackToManual).OnReset(ResetJulia);
        julia.Double(kImag, { "c (imaginary)", "c の虚部" }, &V().ci)
             .Range(-2.0, 2.0).Step(0.001).Default(kPresets[1].im)
             .OnChange(BackToManual).OnReset(ResetJulia);
        julia.Note(kPresetHint, { "Moving c switches the preset to Manual",
                                  "c を動かすとプリセットは手動に戻る" });

        auto color = k.Group(kGroupColor, { "COLOR", "色" });
        color.Combo(kComboPal, { "Palette", "パレット" }, &V().palette)
             .Item(kPalFire,    { "Fire",       "炎" })
             .Item(kPalIce,     { "Ice",        "氷" })
             .Item(kPalRainbow, { "Rainbow",    "虹" })
             .Item(kPalMono,    { "Monochrome", "モノクロ" })
             .Default(kPaletteFire);
        color.Check(kCheckSmooth, { "Smoothing", "スムージング" }, &V().smoothing)
             .Default(1);
        color.Double(kCycle, { "Color cycle", "カラー周期" }, &V().cycle)
             .Range(1.0, 64.0).Step(1.0).Default(16.0);

        auto compute = k.Group(kGroupCompute, { "COMPUTATION", "計算" });
        compute.Int(kIterations, { "Iterations", "反復回数" }, &V().iterations)
               .Range(0, 4000).Step(50).Default(0);
        compute.Note(kIterHint, { "0 = decide automatically from the scale",
                                  "0 でスケールから自動決定" });

        // Details overlay items. They do not appear in the parameter tree.
        k.InfoRow(kInfoC,        { "c",          "c" });
        k.InfoRow(kInfoIter,     { "Iterations", "反復回数" });
        k.InfoRow(kInfoPalette,  { "Palette",    "パレット" });
        k.InfoRow(kInfoInterior, { "Interior",   "内部の画素" });
    });
    return kit;
}

// ---------------------------------------------------------------------------
// Colorization (a periodic palette built from cosines)
// ---------------------------------------------------------------------------

struct Rgb { double r, g, b; };

int32_t Pack(const Rgb& c)
{
    const auto to8 = [](double v) {
        const int i = static_cast<int>(std::lround(std::clamp(v, 0.0, 1.0) * 255.0));
        return std::clamp(i, 0, 255);
    };
    return (to8(c.r) << 16) | (to8(c.g) << 8) | to8(c.b);
}

int32_t PaletteColor(int32_t palette, double t)
{
    constexpr double kTau = 6.283185307179586;

    if (palette == kPaletteMono)
    {
        // Triangle wave: one cycle runs black to white to black.
        const double v = t < 0.5 ? t * 2.0 : (1.0 - t) * 2.0;
        return Pack({ v, v, v });
    }

    Rgb a{}, b{}, d{};
    switch (palette)
    {
    case kPaletteIce:
        a = { 0.30, 0.42, 0.55 }; b = { 0.28, 0.32, 0.40 }; d = { 0.60, 0.55, 0.45 };
        break;
    case kPaletteRainbow:
        a = { 0.50, 0.50, 0.50 }; b = { 0.50, 0.50, 0.50 }; d = { 0.00, 0.33, 0.67 };
        break;
    case kPaletteFire:
    default:
        a = { 0.45, 0.25, 0.15 }; b = { 0.45, 0.30, 0.15 }; d = { 0.00, 0.12, 0.24 };
        break;
    }

    return Pack({ a.r + b.r * std::cos(kTau * (t + d.r)),
                  a.g + b.g * std::cos(kTau * (t + d.g)),
                  a.b + b.b * std::cos(kTau * (t + d.b)) });
}

} // namespace

// ---------------------------------------------------------------------------
// Exported functions --- two required ones from macros, and the seven optional
// ones from a single line that forwards to AddonKit.
// ---------------------------------------------------------------------------

ABYSSO_ADDON_ABI_VERSION_IMPL()

ABYSSO_ADDON_INFO_IMPL(kAddonGuid,
                       Kit().Japanese() ? "ジュリア集合 (サンプル)" : "Julia set (sample)",
                       "1.0.0.0")

ABYSSO_ADDON_EXPORT_ALL(Kit())

ABYSSO_ADDON_API int32_t InitializeAbyssoAddon(const AbyssoHostInfo* host)
{
    if (host == nullptr) return ABYSSO_ERROR_INVALID_ARG;
    if (host->abiVersion != ABYSSO_ADDON_ABI_VERSION) return ABYSSO_ERROR_NOT_SUPPORTED;

    Kit().Reset(nullptr); // Set every value to its default
    Kit().ClearInfo();    // Nothing rendered yet; empty info items are not displayed
    State().initialized = true;
    return 0;
}

ABYSSO_ADDON_API void ShutdownAbyssoAddon(void)
{
    AddonState& s = State();
    s.source.clear();
    s.source.shrink_to_fit();
    s.initialized = false;
}

// ---------------------------------------------------------------------------
// Rendering
//
// The iteration is a scalar implementation. Only the rows are distributed over
// std::thread, so that the image keeps up with parameter changes; a plain
// nested loop would produce exactly the same result.
// ---------------------------------------------------------------------------

ABYSSO_ADDON_API int32_t RenderAbyssoAddonFrame(const AbyssoRenderRequest* req)
{
    AddonState& s = State();
    abysso::AddonKit& kit = Kit();

    // The render request is read through abysso::Request, which applies
    // ABYSSO_REQUEST_HAS internally.
    const abysso::Request r(req);
    if (!r.Valid() || !s.initialized) return ABYSSO_ERROR_INVALID_ARG;

    const int32_t width      = r.Width();
    const int32_t height     = r.Height();
    int32_t* const outPixels = r.Pixels();

    const Values v = s.values; // Copy the parameters at the start of the frame
    kit.BeginFrame();          // Reset the cancellation flag

    // A value of 0 derives the count from the scale; deeper zoom needs more iterations.
    const int32_t maxIterations = v.iterations > 0
        ? v.iterations
        : static_cast<int32_t>(std::clamp(
              300.0 + 120.0 * std::max(0.0, std::log10(1.5 / std::max(r.Scale(), 1e-300))),
              300.0, 4000.0));

    const int divisor = r.QualityDivisor();
    const int sw = std::max(1, (width  + divisor - 1) / divisor);
    const int sh = std::max(1, (height + divisor - 1) / divisor);

    s.source.resize(static_cast<size_t>(sw) * static_cast<size_t>(sh));

    // Mapping from screen coordinates to the complex plane; it must match the host.
    const double minX = r.MinX();
    const double maxX = r.MaxX();
    const double minY = r.MinY();
    const double maxY = r.MaxY();

    const double bailout2 = 65536.0; // A larger value makes the smoothing smoother
    const double invLog2  = 1.0 / std::log(2.0);
    const bool   live     = r.Live();

    std::atomic<int>       nextRow{ 0 };
    std::atomic<long long> interior{ 0 };
    std::atomic<bool>      aborted{ false };

    const auto worker = [&]() {
        long long localInterior = 0;
        for (;;)
        {
            const int sy = nextRow.fetch_add(1, std::memory_order_relaxed);
            if (sy >= sh) break;
            if (live && kit.Cancelled(r))
            {
                aborted.store(true, std::memory_order_relaxed);
                break;
            }

            // Use the corresponding full-resolution pixel rather than the centre
            // of the reduced cell, so that a divisor of 1 matches a plain loop.
            const int    py = std::min(height - 1, sy * divisor);
            const double y0 = minY + (maxY - minY) * py / height;
            float*       row = s.source.data() + static_cast<size_t>(sy) * static_cast<size_t>(sw);

            for (int sx = 0; sx < sw; ++sx)
            {
                const int    px = std::min(width - 1, sx * divisor);
                const double x0 = minX + (maxX - minX) * px / width;

                // Julia set. Only these two lines differ from the Mandelbrot set:
                // z starts at the pixel coordinate, and c comes from the settings
                // rather than from the screen.
                double x = x0, y = y0;
                int32_t iter = 0;
                double mag2 = x * x + y * y;
                while (mag2 <= bailout2 && iter < maxIterations)
                {
                    const double xt = x * x - y * y + v.cr;
                    y = 2.0 * x * y + v.ci;
                    x = xt;
                    mag2 = x * x + y * y;
                    ++iter;
                }

                if (iter >= maxIterations)
                {
                    row[sx] = kInterior;
                    ++localInterior;
                    continue;
                }

                if (v.smoothing)
                {
                    // Continuous iteration count; the larger |z| is, the more overshoot is subtracted.
                    row[sx] = static_cast<float>(
                        iter + 1.0 - std::log(std::log(mag2) * 0.5) * invLog2);
                }
                else
                {
                    row[sx] = static_cast<float>(iter);
                }
            }
        }
        interior.fetch_add(localInterior, std::memory_order_relaxed);
    };

    unsigned hardware = std::thread::hardware_concurrency();
    if (hardware == 0) hardware = 1;
    const int workers = static_cast<int>(std::min<unsigned>(hardware, 32u));

    std::vector<std::thread> threads;
    threads.reserve(static_cast<size_t>(workers - 1));
    for (int i = 1; i < workers; ++i) threads.emplace_back(worker);
    worker();
    for (std::thread& t : threads) t.join();

    if (aborted.load(std::memory_order_relaxed)) return 0;

    // Colorization, and nearest-neighbour scaling of the reduced grid.
    const double invCycle = 1.0 / v.cycle;
    for (int y = 0; y < height; ++y)
    {
        const int    sy  = divisor == 1 ? y : std::min(sh - 1, y / divisor);
        const float* row = s.source.data() + static_cast<size_t>(sy) * static_cast<size_t>(sw);
        int32_t*     out = outPixels + static_cast<size_t>(y) * static_cast<size_t>(width);

        for (int x = 0; x < width; ++x)
        {
            const int   sx = divisor == 1 ? x : std::min(sw - 1, x / divisor);
            const float value = row[sx];

            if (value >= kInterior) { out[x] = 0; continue; } // Black inside the set

            double t = value * invCycle;
            t -= std::floor(t); // Wrap into [0,1)
            out[x] = PaletteColor(v.palette, t);
        }
    }

    // Details overlay. Updated only when the frame has completed.
    // The lambda captures by value: AddonKit re-executes it when the language changes.
    const int32_t iterationsUsed = maxIterations;
    const bool    manual         = v.iterations > 0;
    const double  interiorRatio  = static_cast<double>(interior.load(std::memory_order_relaxed))
                                 / (static_cast<double>(sw) * static_cast<double>(sh));
    const double  cr = v.cr, ci = v.ci;
    const int32_t palette = v.palette;

    kit.Publish([iterationsUsed, manual, interiorRatio, cr, ci, palette]
                (abysso::AddonKit::Writer& w) {
        w.Setf(kInfoC, "%.4f %c %.4fi", cr, ci < 0.0 ? '-' : '+', std::fabs(ci));
        w.Setf(kInfoIter, "%d", iterationsUsed);
        if (manual) w.Append(kInfoIter, w.Ja() ? " (手動)" : " (manual)");
        w.Set(kInfoPalette, Kit().ChoiceCaption(kComboPal, palette));
        w.Setf(kInfoInterior, "%.1f%%", interiorRatio * 100.0);
    });

    return 1;
}
