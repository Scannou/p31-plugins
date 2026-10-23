//
//  SpectrumSettings.h
//  p31-core (ported from foo_jl_spectrum_mac SpectrumConfig.h / SpectrumThemes.h)
//
//  Every user setting of P31 Spectrum as one plain value, with the defaults,
//  ranges and color themes of the fb2k component, and the mapping from these
//  friendly values to the analyzer's concrete rates. Nothing here touches any
//  platform API.
//

#pragma once

#include "p31/core/Appearance.h"
#include "p31/spectrum/SpectrumAnalyzer.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <string>

namespace p31::spectrum {

// Bar fill styles
enum BarStyle {
    BarStyleSolid = 0,      // Single color
    BarStyleGradient = 1,   // Vertical gradient (bottom -> top)
    BarStyleSpectrum = 2,   // Hue mapped across frequency (low -> high)
};

// How the spectrum is drawn
enum DrawMode {
    DrawModeBars = 0,       // Discrete frequency bars
    DrawModeCurve = 1,      // Continuous curve across all frequencies
};

// What the curve draw mode shows
enum CurveStyle {
    CurveStyleFilled = 0,        // Line over a gradient-filled area
    CurveStyleLine = 1,          // Line only
    CurveStyleColorBars = 2,     // Full-height stripes, brighter where the band is louder
    CurveStyleLineColorBars = 3, // Line, with the stripes filling the area under it
};

enum Orientation {
    OrientationHorizontal = 0,   // Frequency along X, magnitude grows up
    OrientationVertical = 1,     // Frequency along Y, magnitude grows sideways
};

// Which channels the spectrum shows
enum StereoMode {
    StereoMono = 0,     // Mid (L+R)/2 only
    StereoMidSide = 1,  // Mid, plus the side (L-R)/2 as a second line
};

// --- Ranges ---
constexpr int    kBarCountAuto = 0;          // bar count follows the view size
constexpr double kAutoBarPitch = 2.0;        // Auto bars: points per bar along the frequency axis
constexpr double kCurvePointPitch = 1.0;     // Curve mode: points per curve knot
constexpr int    kMaxBarCount = SpectrumAnalyzer::kMaxBarCount;
constexpr int    kMaxRmsMs = 500;
constexpr int    kMaxSlopeTenths = 60;
constexpr int    kMinPeakFallDbPerSec = 1;
constexpr int    kMaxPeakFallDbPerSec = 60;
constexpr int    kMaxPeakHoldMs = 2000;    // the peak history grows with the hold time
constexpr int    kMinTopDb = -60;
constexpr int    kMaxTopDb = 0;
constexpr int    kRangeDbChoices[] = {60, 80, 90, 120};
constexpr int    kCurveOctaveFractions[] = {0, 24, 12, 6, 3};
constexpr int    kFftSizes[] = {1024, 2048, 4096, 8192, 16384, 32768};

// Colors for one appearance (ARGB, 0xAARRGGBB).
struct SpectrumColors {
    uint32_t bar;
    uint32_t background;
    uint32_t grid;
};

struct SpectrumSettings {
    // Analysis
    int  fftSize = 2048;
    int  window = WindowHann;
    int  minHz = 20;
    int  maxHz = 20000;
    int  freqScale = FreqScaleSoftLog;
    int  stereoMode = StereoMono;
    int  slopeTenths = 20;         // spectral tilt around 1 kHz, 0.1 dB/octave

    // Motion
    int  smoothingMode = SmoothingRms;
    int  smoothing = 82;           // attack/decay 0-100 (higher = smoother)
    int  rmsMs = 270;              // RMS power averaging time constant
    bool shadowFill = true;        // slow-decaying dim fill behind the bars
    int  shadowFallSpeed = 76;     // 0-100 (higher = faster)
    bool peakHold = true;          // falling peak caps / line
    int  peakFallDbPerSec = 12;
    int  peakHoldMs = 920;

    // Display
    int  drawMode = DrawModeCurve;
    int  curveStyle = CurveStyleLineColorBars;
    int  curveOctaveFraction = 24; // curve detail: 0 = full, N = 1/N octave smoothing
    int  colorBarBrightness = 85;  // strength of the loudest color bar, % (10-100)
    int  barCount = kBarCountAuto;
    int  barStyle = BarStyleGradient;
    int  gapPercent = 12;          // gap between bars, % of slot width
    int  orientation = OrientationHorizontal;
    int  topDb = 0;                // level at the top of the display
    int  rangeDb = 80;             // dB from the top to the bottom

    // Appearance
    bool showDbGuides = true;
    bool showFreqAxis = true;
    int  gridOpacity = 60;         // 0-100
    int  appearance = AppearanceAuto;
    int  theme = 14;               // index into kThemes (P7 blue); 0 = custom colors
    SpectrumColors light{0xFF2D6CC0, 0xFFF0F4F9, 0xFF64748A};
    SpectrumColors dark{0xFF6FB7FF, 0xFF0A0E15, 0xFFC4D6EC};

    // A copy with every value inside the range the controls offer (state
    // loaded from disk is untrusted).
    SpectrumSettings sanitized() const {
        SpectrumSettings s = *this;
        const auto pick = [](int v, int lo, int hi, int fallback) { return v < lo || v > hi ? fallback : v; };
        const auto oneOf = [](int v, const auto& choices, int fallback) {
            return std::find(std::begin(choices), std::end(choices), v) != std::end(choices) ? v : fallback;
        };
        s.fftSize = oneOf(fftSize, kFftSizes, 2048);
        s.window = pick(window, WindowHann, WindowBlackmanHarris, WindowHann);
        s.minHz = std::clamp(minHz, 10, 1000);
        s.maxHz = std::clamp(maxHz, s.minHz + 100, 96000);
        s.freqScale = pick(freqScale, FreqScaleLog, FreqScaleSoftLog, FreqScaleSoftLog);
        s.stereoMode = pick(stereoMode, StereoMono, StereoMidSide, StereoMono);
        s.slopeTenths = std::clamp(slopeTenths, 0, kMaxSlopeTenths);
        s.smoothingMode = pick(smoothingMode, SmoothingAttackDecay, SmoothingRms, SmoothingRms);
        s.smoothing = std::clamp(smoothing, 0, 100);
        s.rmsMs = std::clamp(rmsMs, 0, kMaxRmsMs);
        s.shadowFallSpeed = std::clamp(shadowFallSpeed, 0, 100);
        s.peakFallDbPerSec = std::clamp(peakFallDbPerSec, kMinPeakFallDbPerSec, kMaxPeakFallDbPerSec);
        s.peakHoldMs = std::clamp(peakHoldMs, 0, kMaxPeakHoldMs);
        s.drawMode = pick(drawMode, DrawModeBars, DrawModeCurve, DrawModeCurve);
        s.curveStyle = pick(curveStyle, CurveStyleFilled, CurveStyleLineColorBars, CurveStyleLineColorBars);
        s.curveOctaveFraction = oneOf(curveOctaveFraction, kCurveOctaveFractions, 24);
        s.colorBarBrightness = std::clamp(colorBarBrightness, 10, 100);
        s.barCount = barCount <= kBarCountAuto ? kBarCountAuto
                                               : std::clamp(barCount, SpectrumAnalyzer::kMinBarCount, kMaxBarCount);
        s.barStyle = pick(barStyle, BarStyleSolid, BarStyleSpectrum, BarStyleGradient);
        s.gapPercent = std::clamp(gapPercent, 0, 80);
        s.orientation = pick(orientation, OrientationHorizontal, OrientationVertical, OrientationHorizontal);
        s.topDb = std::clamp(topDb, kMinTopDb, kMaxTopDb);
        s.rangeDb = oneOf(rangeDb, kRangeDbChoices, 80);
        s.gridOpacity = std::clamp(gridOpacity, 0, 100);
        s.appearance = pick(appearance, AppearanceAuto, AppearanceDark, AppearanceAuto);
        s.theme = pick(theme, 0, 14, 0);   // kThemeCount - 1
        return s;
    }

    // The bar count follows the view size ("Auto" bars, or Curve mode).
    bool viewSizedBars() const { return barCount == kBarCountAuto || drawMode == DrawModeCurve; }

    // Curve mode with color bars and no line: no peak line and no legend.
    bool colorBarsOnly() const { return drawMode == DrawModeCurve && curveStyle == CurveStyleColorBars; }
};

// Every field with its state key, for StateWriter / StateReader.
template <class S, class Visitor>
void visitColors(S& c, const char* prefix, Visitor&& v) {
    const std::string p(prefix);
    v(p + "bar", c.bar);
    v(p + "background", c.background);
    v(p + "grid", c.grid);
}

template <class Visitor>
void visitFields(SpectrumSettings& s, Visitor&& v) {
    v("fft_size", s.fftSize);
    v("window", s.window);
    v("min_hz", s.minHz);
    v("max_hz", s.maxHz);
    v("freq_scale", s.freqScale);
    v("stereo_mode", s.stereoMode);
    v("slope_tenths", s.slopeTenths);
    v("smoothing_mode", s.smoothingMode);
    v("smoothing", s.smoothing);
    v("rms_ms", s.rmsMs);
    v("shadow_fill", s.shadowFill);
    v("shadow_fall_speed", s.shadowFallSpeed);
    v("peak_hold", s.peakHold);
    v("peak_fall_db_s", s.peakFallDbPerSec);
    v("peak_hold_ms", s.peakHoldMs);
    v("draw_mode", s.drawMode);
    v("curve_style", s.curveStyle);
    v("curve_octave_fraction", s.curveOctaveFraction);
    v("color_bar_brightness", s.colorBarBrightness);
    v("bar_count", s.barCount);
    v("bar_style", s.barStyle);
    v("gap_percent", s.gapPercent);
    v("orientation", s.orientation);
    v("top_db", s.topDb);
    v("range_db", s.rangeDb);
    v("show_db_guides", s.showDbGuides);
    v("show_freq_axis", s.showFreqAxis);
    v("grid_opacity", s.gridOpacity);
    v("appearance", s.appearance);
    v("theme", s.theme);
    visitColors(s.light, "light.", v);
    visitColors(s.dark, "dark.", v);
}

// Analyzer settings for these user settings. `viewBarCount` is the count
// that fits the view, used when viewSizedBars().
inline SpectrumAnalyzer::Settings analyzerSettings(const SpectrumSettings& settings, int viewBarCount) {
    const SpectrumSettings u = settings.sanitized();
    SpectrumAnalyzer::Settings s;
    const bool curve = u.drawMode == DrawModeCurve;
    s.barCount = u.viewSizedBars() ? viewBarCount : u.barCount;
    s.octaveFraction = curve ? u.curveOctaveFraction : 0;
    s.fftSize = u.fftSize;
    s.window = u.window;
    s.minHz = u.minHz;
    s.maxHz = u.maxHz;
    s.smoothing = u.smoothing;
    s.smoothingMode = u.smoothingMode;
    s.rmsMs = u.rmsMs;
    s.side = u.stereoMode == StereoMidSide;
    s.slopeDbPerOct = static_cast<float>(u.slopeTenths) / 10.0f;
    s.freqScale = u.freqScale;
    s.topDb = static_cast<float>(u.topDb);
    s.rangeDb = static_cast<float>(u.rangeDb);

    // Friendly 0-100 / dB-per-second values to rates per 1/60 s frame.
    const double shadowSpeed = u.shadowFallSpeed / 100.0;
    s.shadowFall = static_cast<float>(0.001 + shadowSpeed * 0.0275);
    if (u.smoothingMode == SmoothingRms && u.rmsMs > 0) {
        // An RMS curve falls at most 10*log10(e)/tau dB/s (power decaying with
        // nothing coming in), slow enough that most fixed shadow rates would
        // fall faster and stay hidden under it. Scale the slider to 15-90 % of
        // that rate so the shadow always trails the curve.
        const double curveFallDbPerSec = 10.0 * std::log10(std::exp(1.0)) / (u.rmsMs / 1000.0);
        const double curveFall = curveFallDbPerSec / 60.0 / u.rangeDb;
        s.shadowFall = static_cast<float>(curveFall * (0.15 + 0.75 * shadowSpeed));
    }
    s.peakFall = static_cast<float>(u.peakFallDbPerSec / 60.0 / u.rangeDb);
    s.peakHoldSec = static_cast<float>(u.peakHoldMs / 1000.0);
    s.peakHold = u.peakHold && !u.colorBarsOnly();   // nothing drawn: keep no history
    return s;
}

// --- Themes ---
//
// Curated color presets (MiniMeters-style). A theme stamps the bar,
// background and grid colors for both appearances at once. Index 0 is
// "Custom" (applying it changes nothing); real presets start at 1.

struct SpectrumTheme {
    const char* name;
    SpectrumColors light;
    SpectrumColors dark;
};

inline constexpr SpectrumTheme kThemes[] = {
    {"Custom", {0xFF2E7DD1, 0xFFF2F2F2, 0xFF808080}, {0xFF4DA3F0, 0xFF161616, 0xFF909090}},
    {"Default (Blue)", {0xFF2E7DD1, 0xFFF2F2F2, 0xFF808080}, {0xFF4DA3F0, 0xFF161616, 0xFF909090}},
    {"Classic (Green)", {0xFF1B8A3A, 0xFFF4F4F4, 0xFF9AA0A0}, {0xFF33FF66, 0xFF0A0A0A, 0xFF4D4D4D}},
    {"Nord", {0xFF5E81AC, 0xFFECEFF4, 0xFFB0B6C0}, {0xFF88C0D0, 0xFF2E3440, 0xFF4C566A}},
    {"Dracula", {0xFF8B5CF6, 0xFFF8F8F2, 0xFFCFCFD6}, {0xFFBD93F9, 0xFF282A36, 0xFF44475A}},
    {"Gruvbox", {0xFFD65D0E, 0xFFFBF1C7, 0xFFBDB4A0}, {0xFFFE8019, 0xFF282828, 0xFF504945}},
    {"Solarized", {0xFF268BD2, 0xFFFDF6E3, 0xFF93A1A1}, {0xFF268BD2, 0xFF002B36, 0xFF586E75}},
    {"Tokyo Night", {0xFF3D59A1, 0xFFD5D6DB, 0xFFA8AECB}, {0xFF7AA2F7, 0xFF1A1B26, 0xFF414868}},
    {"Catppuccin", {0xFF8839EF, 0xFFEFF1F5, 0xFFBCC0CC}, {0xFFCBA6F7, 0xFF1E1E2E, 0xFF45475A}},
    {"Monokai", {0xFF4E9A06, 0xFFF8F8F2, 0xFFC8C8B8}, {0xFFA6E22E, 0xFF272822, 0xFF49483E}},
    {"Sunset", {0xFFE8551A, 0xFFFFF3EC, 0xFFD8B8A0}, {0xFFFF6B35, 0xFF1A0F0A, 0xFF4D3326}},
    // Named after CRT phosphors. Added after the older themes so the theme
    // index saved in existing projects keeps its meaning; the menu lists
    // them first (kThemeMenuOrder). One colour per theme: the level colour,
    // a near-black background tinted towards it, and a pale grid.
    {"P31 green", {0xFF1B8A3A, 0xFFF1F5F2, 0xFF5F7A66}, {0xFF4CFF7C, 0xFF0C120E, 0xFFB8E6C4}},
    {"P3 amber", {0xFFB86A00, 0xFFF7F3EA, 0xFF7D6E55}, {0xFFFFB000, 0xFF130F07, 0xFFE8D4AE}},
    {"P4 white", {0xFF3C434B, 0xFFF4F5F6, 0xFF6E767E}, {0xFFE6EDF2, 0xFF0F1113, 0xFFD5DCE2}},
    {"P7 blue", {0xFF2D6CC0, 0xFFF0F4F9, 0xFF64748A}, {0xFF6FB7FF, 0xFF0A0E15, 0xFFC4D6EC}},
};

inline constexpr int kThemeCount = static_cast<int>(sizeof(kThemes) / sizeof(kThemes[0]));
// sanitized() checks the theme index against this count.
static_assert(kThemeCount == 15);

// Theme menu order: Custom, the phosphors, then the rest.
inline constexpr int kThemeMenuOrder[] = {0, 11, 12, 13, 14, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
static_assert(sizeof(kThemeMenuOrder) / sizeof(kThemeMenuOrder[0]) == kThemeCount);

// Stamp a preset's colors into `s`. No-op for 0 (Custom) or an invalid index.
inline void applyTheme(SpectrumSettings& s, int index) {
    if (index <= 0 || index >= kThemeCount) return;
    s.light = kThemes[index].light;
    s.dark = kThemes[index].dark;
    s.theme = index;
}

} // namespace p31::spectrum
