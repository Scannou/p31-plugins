//
//  SpectrogramSettings.h
//  p31-core (ported from foo_jl_spectrogram_mac SpectrogramConfig.h / SpectrogramThemes.h)
//
//  Every user setting of P31 Spectrogram as one plain value, with the
//  defaults and ranges of the fb2k component, the mapping to the engine's
//  settings, and applying a colormap preset to the stored colors. Nothing
//  here touches any platform API.
//

#pragma once

#include "p31/core/Appearance.h"
#include "p31/spectrogram/SpectrogramColormaps.h"
#include "p31/spectrogram/SpectrogramEngine.h"

#include <algorithm>
#include <cstdint>
#include <iterator>

namespace p31::spectrogram {

// Which signal is analysed
enum Channel {
    ChannelMid = 0,         // (L + R) / 2
    ChannelLeft = 1,
    ChannelRight = 2,
    ChannelSide = 3         // (L - R) / 2
};

// Which way the frequency axis runs, as in P31 Spectrum: the same setting in
// both plugins lines up their frequency axes.
enum Orientation {
    OrientationHorizontal = 0,  // Frequency along X, time flows vertically
    OrientationVertical = 1     // Frequency along Y, time flows sideways
};

// --- Ranges ---
constexpr int kFftSizes[] = {1024, 2048, 4096, 8192, 16384};   // base window at 44.1/48 kHz
constexpr int kMinTopDb = -40;
constexpr int kMaxTopDb = 0;
constexpr int kMinRangeDb = 40;
constexpr int kMaxRangeDb = 140;
constexpr int kMaxSlopeTenths = 60;
constexpr int kScrollSpeeds[] = {30, 60, 120, 240};            // points per second
constexpr int kMinFontSize = 8;
constexpr int kMaxFontSize = 16;

// Panel colors for one appearance (ARGB): the margins around the plot, and
// gridlines. Labels are derived from the background so they stay readable on
// any of them.
struct PanelColors {
    uint32_t background;
    uint32_t grid;
};

struct SpectrogramSettings {
    // Analysis. Enum-valued settings (analysis mode, frequency scale, window)
    // store the SpectrogramEngine enum values directly.
    int  fftSize = 4096;
    int  analysisMode = SpectrogramEngine::ModeReassigned;   // thin bass lines without time smear
    int  window = SpectrogramEngine::WindowHann;
    int  channel = ChannelMid;
    int  freqScale = SpectrogramEngine::ScaleLog;
    int  minHz = 20;               // lowest displayed frequency
    int  maxHz = 20000;            // highest displayed frequency

    // Display
    int  topDb = 0;                // level mapped to the brightest color (dBFS)
    int  rangeDb = 60;             // dB span below the top level
    int  slopeTenths = 0;          // tilt around 1 kHz, tenths of dB per octave
    int  scrollSpeed = 120;        // points per second

    // Layout
    int  orientation = OrientationVertical;
    bool reverseScroll = false;    // new audio at the left (vertical) or bottom (horizontal)

    // Labels
    bool showFreqAxis = true;      // frequency labels beside the plot
    bool showLegend = true;        // color / dB scale on the right edge
    int  gridOpacity = 15;         // frequency gridline opacity 0-100 (0 hides)
    int  freqFontSize = 11;        // frequency label size, points
    int  dbFontSize = 11;          // dB scale label size, points

    // Colors
    int  appearance = AppearanceAuto;
    int  colormap = 0;             // index into kMaps, or kCustomMap
    // Custom ramp: floor, low, mid, peak (see kCustomStops). Follows the
    // chosen colormap until the user edits a color.
    uint32_t customColors[kCustomColorCount];
    PanelColors light{0xFFF2F2F2, 0xFF808080};
    PanelColors dark{0xFF161616, 0xFF909090};

    SpectrogramSettings() {
        for (int i = 0; i < kCustomColorCount; ++i) customColors[i] = customColorSeed(colormap, i);
    }

    // A copy with every value inside the range the controls offer (state
    // loaded from disk is untrusted).
    SpectrogramSettings sanitized() const {
        SpectrogramSettings s = *this;
        const auto pick = [](int v, int lo, int hi, int fallback) { return v < lo || v > hi ? fallback : v; };
        const auto oneOf = [](int v, const auto& choices, int fallback) {
            return std::find(std::begin(choices), std::end(choices), v) != std::end(choices) ? v : fallback;
        };
        s.fftSize = oneOf(fftSize, kFftSizes, 4096);
        s.analysisMode = pick(analysisMode, SpectrogramEngine::ModeStandard, SpectrogramEngine::ModeReassigned,
                              SpectrogramEngine::ModeReassigned);
        s.window = pick(window, SpectrogramEngine::WindowHann, SpectrogramEngine::WindowBlackmanHarris,
                        SpectrogramEngine::WindowHann);
        s.channel = pick(channel, ChannelMid, ChannelSide, ChannelMid);
        s.freqScale = pick(freqScale, SpectrogramEngine::ScaleLog, SpectrogramEngine::ScaleLinear,
                           SpectrogramEngine::ScaleLog);
        s.minHz = std::clamp(minHz, 10, 2000);
        s.maxHz = std::clamp(maxHz, std::max(1000, s.minHz + 100), 96000);
        s.topDb = std::clamp(topDb, kMinTopDb, kMaxTopDb);
        s.rangeDb = std::clamp(rangeDb, kMinRangeDb, kMaxRangeDb);
        s.slopeTenths = std::clamp(slopeTenths, 0, kMaxSlopeTenths);
        s.scrollSpeed = oneOf(scrollSpeed, kScrollSpeeds, 120);
        s.orientation = pick(orientation, OrientationHorizontal, OrientationVertical, OrientationVertical);
        s.gridOpacity = std::clamp(gridOpacity, 0, 100);
        s.freqFontSize = std::clamp(freqFontSize, kMinFontSize, kMaxFontSize);
        s.dbFontSize = std::clamp(dbFontSize, kMinFontSize, kMaxFontSize);
        s.appearance = pick(appearance, AppearanceAuto, AppearanceDark, AppearanceAuto);
        s.colormap = pick(colormap, kCustomMap, kMapCount - 1, 0);
        // The ramp is drawn opaque.
        for (uint32_t& c : s.customColors) c |= 0xFF000000u;
        return s;
    }
};

// Every field with its state key, for StateWriter / StateReader.
template <class Visitor>
void visitFields(SpectrogramSettings& s, Visitor&& v) {
    v("fft_size", s.fftSize);
    v("analysis_mode", s.analysisMode);
    v("window", s.window);
    v("channel", s.channel);
    v("freq_scale", s.freqScale);
    v("min_hz", s.minHz);
    v("max_hz", s.maxHz);
    v("top_db", s.topDb);
    v("range_db", s.rangeDb);
    v("slope_tenths", s.slopeTenths);
    v("scroll_speed", s.scrollSpeed);
    v("orientation", s.orientation);
    v("reverse_scroll", s.reverseScroll);
    v("show_freq_axis", s.showFreqAxis);
    v("show_legend", s.showLegend);
    v("grid_opacity", s.gridOpacity);
    v("freq_font_size", s.freqFontSize);
    v("db_font_size", s.dbFontSize);
    v("appearance", s.appearance);
    v("colormap", s.colormap);
    v("custom.floor", s.customColors[0]);
    v("custom.low", s.customColors[1]);
    v("custom.mid", s.customColors[2]);
    v("custom.peak", s.customColors[3]);
    v("light.background", s.light.background);
    v("light.grid", s.light.grid);
    v("dark.background", s.dark.background);
    v("dark.grid", s.dark.grid);
}

// Engine settings for these user settings.
inline SpectrogramEngine::Settings engineSettings(const SpectrogramSettings& settings) {
    const SpectrogramSettings u = settings.sanitized();
    SpectrogramEngine::Settings s;
    s.fftSize = u.fftSize;
    s.mode = u.analysisMode;
    s.window = u.window;
    s.scale = u.freqScale;
    s.minHz = u.minHz;
    s.maxHz = u.maxHz;
    s.topDb = static_cast<float>(u.topDb);
    s.rangeDb = static_cast<float>(u.rangeDb);
    s.slopeDb = static_cast<float>(u.slopeTenths) / 10.0f;
    return s;
}

// Choose a colormap. A preset also fills in the custom ramp's four colors,
// and a theme ramp brings its P31 Spectrum panel colors (theme ramp and panel
// numbers are already Display P3). kCustomMap keeps the colors as they are;
// an invalid index changes nothing.
inline void applyColormap(SpectrogramSettings& s, int index) {
    if (index == kCustomMap) {
        s.colormap = kCustomMap;
        return;
    }
    if (index < 0 || index >= kMapCount) return;
    s.colormap = index;
    for (int i = 0; i < kCustomColorCount; ++i) s.customColors[i] = customColorSeed(index, i);
    if (index >= kFirstThemeMap) {
        const ThemePanel& t = kThemePanels[index - kFirstThemeMap];
        s.light = {t.bgLight, t.gridLight};
        s.dark = {t.bgDark, t.gridDark};
    }
}

// The lookup table (kLutSize entries, 0xFFRRGGBB) these settings draw with.
inline void buildLut(const SpectrogramSettings& s, uint32_t* lut) {
    if (s.colormap == kCustomMap) buildCustomLut(s.customColors, lut);
    else buildLut(s.colormap, lut);
}

} // namespace p31::spectrogram
