//
//  MeterSettings.h
//  p31-core (ported from foo_jl_levelmeter_mac LevelMeterConfig.h)
//
//  Every user setting of P31 Meter as one plain value, with the defaults and
//  ranges of the fb2k component, and the mapping to the engine's and the
//  renderer's settings. Nothing here touches any platform API.
//
//  Not ported: the glass background and the resets on track change, which
//  belong to the player.
//

#pragma once

#include "p31/core/Appearance.h"
#include "p31/meter/DisplaySettings.h"
#include "p31/meter/MeterEngine.h"
#include "p31/meter/MeterThemes.h"

#include <algorithm>
#include <cstdint>
#include <iterator>
#include <string>

namespace p31::meter {

// --- Ranges ---
constexpr int kFloorChoices[] = {-24, -40, -48, -60, -72, -80, -90, -120};   // bottom of the bars, dB
constexpr int kTextSizes[] = {9, 10, 12, 14};   // readout size, pt; the rest of the text scales with it
constexpr int kTargetChoices[] = {-9, -11, -14, -16, -18, -23, -24};         // LUFS
constexpr int kMinZoneDb = -40;
constexpr int kMinRmsMs = 50;
constexpr int kMaxRmsMs = 3000;
constexpr int kMinPeakFallTenths = 30;
constexpr int kMaxPeakFallTenths = 600;
constexpr int kMaxPeakHoldMs = 5000;
constexpr int kMinReferenceDb = -24;
constexpr int kMaxReferenceDb = -6;
constexpr int kMinPeakLedDb = -20;

// The six colours of one appearance, in the order the themes list them.
enum ColorIndex {
    ColorBackground = 0,
    ColorText = 1,       // scale labels, readouts
    ColorLevel = 2,      // level below the warning zone
    ColorWarning = 3,
    ColorOver = 4,
    ColorLoudness = 5,   // momentary / short-term bars
    kColorCount = 6,
};
constexpr const char* kColorNames[kColorCount] = {"Background", "Text", "Level", "Warning", "Over", "Loudness"};

constexpr int kDefaultTheme = kDefaultMeterTheme;   // Classic (Green)

struct MeterSettings {
    // Display. Enum-valued settings store the DisplaySettings enum values.
    int  style = static_cast<int>(MeterStyle::Digital);
    int  orientation = static_cast<int>(Orientation::Auto);
    int  textSize = 10;            // one of kTextSizes
    int  barMode = static_cast<int>(BarMode::PeakAndRms);
    int  scale = static_cast<int>(ScaleType::Dbfs);
    int  spacing = static_cast<int>(ScaleSpacing::Extended);
    int  floorDb = -60;            // one of kFloorChoices
    bool segments = false;         // LED-style segmented bars

    // Show
    bool showReadouts = true;
    bool showLoudness = true;
    bool showCorrelation = true;
    bool showBalance = true;

    // Levels
    bool truePeak = true;          // 4x oversampled peaks
    int  rmsMs = 300;
    bool rmsSinePeak = false;      // +3 dB: a sine's RMS reads its peak (AES17)
    int  peakFallTenths = 118;     // tenths of a dB per second; 20 dB in 1.7 s
    int  peakHoldMs = 1500;        // 0 = no hold line
    bool holdForever = false;      // hold until clicked
    int  warnDb = -12;             // dBFS where the warning colour starts (dBFS scale)
    int  overDb = -3;              // dBFS where the over colour starts

    // Loudness
    int  targetLufs = -14;         // one of kTargetChoices

    // Analog
    int  face = static_cast<int>(FaceStyle::Vintage);
    int  needleLayout = static_cast<int>(NeedleLayout::Auto);
    int  needle = static_cast<int>(NeedleBallistics::VU);
    int  referenceDb = -18;        // sine level that reads 0 VU, dBFS
    bool peakLed = true;
    int  peakLedDb = -1;           // peak LED threshold, dBFS

    // Appearance
    int  appearance = AppearanceAuto;
    int  theme = kDefaultTheme;    // index into kMeterThemes; 0 = custom colors
    uint32_t light[kColorCount] = {0xFFF4F4F4, 0xFF30353B, 0xFF1B8A3A, 0xFFB86A00, 0xFFD70015, 0xFF2E7DD1};
    uint32_t dark[kColorCount] = {0xFF0A0A0A, 0xFFC9CED4, 0xFF33FF66, 0xFFFFB000, 0xFFFF453A, 0xFF4DA3F0};

    // A copy with every value inside the range the controls offer (state
    // loaded from disk is untrusted).
    MeterSettings sanitized() const {
        MeterSettings s = *this;
        const auto pick = [](int v, int lo, int hi, int fallback) {
            return v < lo || v > hi ? fallback : v;
        };
        const auto oneOf = [](int v, const auto& choices, int fallback) {
            return std::find(std::begin(choices), std::end(choices), v) != std::end(choices) ? v : fallback;
        };
        s.style = pick(style, 0, static_cast<int>(MeterStyle::Analog), 0);
        s.orientation = pick(orientation, 0, static_cast<int>(Orientation::Horizontal), 0);
        s.textSize = oneOf(textSize, kTextSizes, 10);
        s.barMode = pick(barMode, 0, static_cast<int>(BarMode::Rms), 0);
        s.scale = pick(scale, 0, static_cast<int>(ScaleType::K12), 0);
        s.spacing = pick(spacing, 0, static_cast<int>(ScaleSpacing::Extended), static_cast<int>(ScaleSpacing::Extended));
        s.floorDb = oneOf(floorDb, kFloorChoices, -60);
        s.rmsMs = std::clamp(rmsMs, kMinRmsMs, kMaxRmsMs);
        s.peakFallTenths = std::clamp(peakFallTenths, kMinPeakFallTenths, kMaxPeakFallTenths);
        s.peakHoldMs = std::clamp(peakHoldMs, 0, kMaxPeakHoldMs);
        s.warnDb = std::clamp(warnDb, kMinZoneDb, 0);
        s.overDb = std::max(s.warnDb, std::clamp(overDb, kMinZoneDb, 0));
        s.targetLufs = oneOf(targetLufs, kTargetChoices, -14);
        s.face = pick(face, 0, static_cast<int>(FaceStyle::Theme), 0);
        s.needleLayout = pick(needleLayout, 0, static_cast<int>(NeedleLayout::Single), 0);
        s.needle = pick(needle, 0, kNeedleBallisticsCount - 1, 0);
        s.referenceDb = std::clamp(referenceDb, kMinReferenceDb, kMaxReferenceDb);
        s.peakLedDb = std::clamp(peakLedDb, kMinPeakLedDb, 0);
        s.appearance = pick(appearance, AppearanceAuto, AppearanceDark, AppearanceAuto);
        s.theme = pick(theme, 0, kMeterThemeCount - 1, 0);
        return s;
    }
};

// Every field with its state key, for StateWriter / StateReader.
template <class Visitor>
void visitFields(MeterSettings& s, Visitor&& v) {
    v("style", s.style);
    v("orientation", s.orientation);
    v("text_size", s.textSize);
    v("bar_mode", s.barMode);
    v("scale", s.scale);
    v("scale_spacing", s.spacing);
    v("floor_db", s.floorDb);
    v("segments", s.segments);
    v("show_readouts", s.showReadouts);
    v("show_loudness", s.showLoudness);
    v("show_correlation", s.showCorrelation);
    v("show_balance", s.showBalance);
    v("true_peak", s.truePeak);
    v("rms_ms", s.rmsMs);
    v("rms_sine_reads_peak", s.rmsSinePeak);
    v("peak_fall_tenths", s.peakFallTenths);
    v("peak_hold_ms", s.peakHoldMs);
    v("peak_hold_forever", s.holdForever);
    v("warn_db", s.warnDb);
    v("over_db", s.overDb);
    v("target_lufs", s.targetLufs);
    v("face", s.face);
    v("needle_layout", s.needleLayout);
    v("needle_ballistics", s.needle);
    v("reference_db", s.referenceDb);
    v("peak_led", s.peakLed);
    v("peak_led_db", s.peakLedDb);
    v("appearance", s.appearance);
    v("theme", s.theme);
    static constexpr const char* kColorKeys[kColorCount] = {"background", "text", "level", "warning", "over", "loudness"};
    for (int i = 0; i < kColorCount; ++i) {
        v(std::string("light.") + kColorKeys[i], s.light[i]);
        v(std::string("dark.") + kColorKeys[i], s.dark[i]);
    }
}

// --- Themes ---

// Stamp a preset's colors into `s`. No-op for 0 (Custom) or an invalid index.
inline void applyTheme(MeterSettings& s, int index) {
    if (index <= 0 || index >= kMeterThemeCount) return;
    std::copy(std::begin(kMeterThemes[index].light), std::end(kMeterThemes[index].light), s.light);
    std::copy(std::begin(kMeterThemes[index].dark), std::end(kMeterThemes[index].dark), s.dark);
    s.theme = index;
}

// The preset whose colors all match `s`, or 0 (Custom) if none does. The
// stored index wins when it matches: Custom and Classic (Green) share colors.
inline int themeMatching(const MeterSettings& s) {
    const auto matches = [&s](int i) {
        return std::equal(std::begin(s.light), std::end(s.light), kMeterThemes[i].light) &&
               std::equal(std::begin(s.dark), std::end(s.dark), kMeterThemes[i].dark);
    };
    if (s.theme > 0 && s.theme < kMeterThemeCount && matches(s.theme)) return s.theme;
    for (int i = 1; i < kMeterThemeCount; ++i) {
        if (matches(i)) return i;
    }
    return 0;
}

// --- Mapping ---

inline EngineSettings engineSettings(const MeterSettings& settings) {
    const MeterSettings m = settings.sanitized();
    EngineSettings s;
    s.rmsWindowSeconds = m.rmsMs / 1000.0;
    s.rmsSineReadsPeak = m.rmsSinePeak;
    s.peakFallDbPerSecond = m.peakFallTenths / 10.0;
    s.peakHoldSeconds = m.peakHoldMs / 1000.0;
    s.peakHoldForever = m.holdForever;
    s.truePeak = m.truePeak;
    s.needle = static_cast<NeedleBallistics>(m.needle);
    s.referenceDbfs = m.referenceDb;
    return s;
}

// The renderer's settings, with the colours of the light or the dark set.
inline DisplaySettings displaySettings(const MeterSettings& settings, bool dark) {
    const MeterSettings m = settings.sanitized();
    DisplaySettings d;
    d.style = static_cast<MeterStyle>(m.style);
    d.orientation = static_cast<Orientation>(m.orientation);
    d.barMode = static_cast<BarMode>(m.barMode);
    d.scale = static_cast<ScaleType>(m.scale);
    d.spacing = static_cast<ScaleSpacing>(m.spacing);
    d.floorDb = m.floorDb;
    d.segments = m.segments;
    d.textScale = m.textSize / 10.0;
    d.showReadouts = m.showReadouts;
    d.showLoudness = m.showLoudness;
    d.showCorrelation = m.showCorrelation;
    d.showBalance = m.showBalance;
    d.truePeak = m.truePeak;
    d.loudnessTarget = m.targetLufs;
    d.warnDb = m.warnDb;
    d.overDb = m.overDb;
    d.face = static_cast<FaceStyle>(m.face);
    d.needleLayout = static_cast<NeedleLayout>(m.needleLayout);
    d.needle = static_cast<NeedleBallistics>(m.needle);
    d.referenceDbfs = m.referenceDb;
    d.peakLed = m.peakLed;
    d.peakLedDbfs = m.peakLedDb;

    const uint32_t* c = dark ? m.dark : m.light;
    Palette& p = d.palette;
    p.dark = dark;
    p.background = c[ColorBackground];
    p.text = c[ColorText];
    p.safe = c[ColorLevel];
    p.warn = c[ColorWarning];
    p.over = c[ColorOver];
    p.loudness = c[ColorLoudness];
    return d;
}

} // namespace p31::meter
