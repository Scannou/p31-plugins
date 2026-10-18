//
//  ScopeSettings.h
//  p31-core (ported from foo_jl_vectorscope_mac VectorscopeConfig.h / VectorscopeThemes.h)
//
//  Every user setting of P31 Scope as one plain value, with the defaults,
//  ranges and color themes of the fb2k component. The plugin keeps one of
//  these per instance (and will save it in its state); nothing here touches
//  any platform API.
//

#pragma once

#include "p31/core/Appearance.h"
#include "p31/scope/ScopeLayout.h"

#include <algorithm>
#include <cstdint>
#include <string>

namespace p31::scope {

// What the scope plots
enum ScopeStyle {
    ScopeStyleGoniometer = 0,   // Mid up, side across: mono is vertical
    ScopeStyleLissajous = 1     // Left across, right up: mono is diagonal
};

// How the trace is drawn
enum DrawMode {
    DrawModeLines = 0,      // Samples joined by lines; fast beam movement is dimmer
    DrawModeDots = 1        // One dot per sample
};

// How the trace is coloured
enum TraceColorMode {
    TraceColorSingle = 0,   // One trace colour
    TraceColorBands = 1     // Low / mid / high bands, each in its own colour
};

// Correlation meter layout
enum CorrelationMode {
    CorrelationSingle = 0,  // One broadband bar
    CorrelationBands = 1    // Low / mid / high bars
};

constexpr int kBandCount = 3;

// --- Ranges ---
constexpr int kMinDotSize = 10;           // tenths of a point; below this is auto
constexpr int kMaxDotSize = 80;
constexpr int kMinPersistenceMs = 20;
constexpr int kMaxPersistenceMs = 2000;
constexpr int kMaxGainDb = 36;
constexpr int kMinBarThickness = 4;
constexpr int kMaxBarThickness = 24;
constexpr int kMinIntegrationMs = 100;
constexpr int kMaxIntegrationMs = 3000;
constexpr int kMaxRangeHoldMs = 5000;
constexpr int kMinLowHz = 40;
constexpr int kMaxLowHz = 1000;
constexpr int kMinHighHz = 1000;
constexpr int kMaxHighHz = 12000;

// Colors for one appearance (ARGB, 0xAARRGGBB).
struct ScopeColors {
    uint32_t trace;
    uint32_t background;
    uint32_t grid;
    uint32_t bands[kBandCount];   // low, mid, high
};

struct ScopeSettings {
    // Scope
    int  style = ScopeStyleGoniometer;
    int  drawMode = DrawModeLines;
    int  traceColorMode = TraceColorSingle;
    int  dotSizeTenths = 0;        // dot diameter in tenths of a point; 0 = auto
    int  persistenceMs = 300;      // afterglow time constant
    int  brightness = 60;          // 0-100
    bool autoGain = true;          // scale quiet material up to fill the scope
    int  gainDb = 0;               // fixed gain when auto gain is off

    // Correlation meter
    bool showCorrelation = true;
    int  correlationMode = CorrelationSingle;
    int  meterPosition = scope_layout::MeterAuto;
    int  barThickness = 6;         // points
    int  integrationMs = 500;      // correlation time window
    int  rangeHoldMs = 200;        // how long the recent range is held; 0 = off
    int  lowHz = 250;              // low / mid crossover
    int  highHz = 2000;            // mid / high crossover

    // Appearance
    bool showGrid = true;          // outline, level guides, axes
    bool showLabels = true;        // L / R / M / S and the correlation scale
    int  gridOpacity = 40;         // 0-100
    int  appearance = AppearanceAuto;
    int  theme = 1;                // index into kThemes; 0 = custom colors
    ScopeColors light{0xFF1B8A3A, 0xFFF4F4F4, 0xFF9AA0A0, {0xFFB86A00, 0xFF1B8A3A, 0xFF2E7DD1}};
    ScopeColors dark{0xFF33FF66, 0xFF0A0A0A, 0xFF4D4D4D, {0xFFFFB000, 0xFF33FF66, 0xFF4DA3F0}};

    // A copy with every value inside the range the controls offer (state
    // loaded from disk is untrusted).
    ScopeSettings sanitized() const {
        ScopeSettings s = *this;
        const auto pick = [](int v, int lo, int hi, int fallback) {
            return v < lo || v > hi ? fallback : v;
        };
        s.style = pick(style, ScopeStyleGoniometer, ScopeStyleLissajous, ScopeStyleGoniometer);
        s.drawMode = pick(drawMode, DrawModeLines, DrawModeDots, DrawModeLines);
        s.traceColorMode = pick(traceColorMode, TraceColorSingle, TraceColorBands, TraceColorSingle);
        s.dotSizeTenths = dotSizeTenths < kMinDotSize ? 0 : std::min(dotSizeTenths, kMaxDotSize);
        s.persistenceMs = std::clamp(persistenceMs, kMinPersistenceMs, kMaxPersistenceMs);
        s.brightness = std::clamp(brightness, 0, 100);
        s.gainDb = std::clamp(gainDb, 0, kMaxGainDb);
        s.correlationMode = pick(correlationMode, CorrelationSingle, CorrelationBands, CorrelationSingle);
        s.meterPosition = pick(meterPosition, scope_layout::MeterAuto, scope_layout::MeterRight,
                               scope_layout::MeterAuto);
        s.barThickness = std::clamp(barThickness, kMinBarThickness, kMaxBarThickness);
        s.integrationMs = std::clamp(integrationMs, kMinIntegrationMs, kMaxIntegrationMs);
        s.rangeHoldMs = std::clamp(rangeHoldMs, 0, kMaxRangeHoldMs);
        s.lowHz = std::clamp(lowHz, kMinLowHz, kMaxLowHz);
        s.highHz = std::clamp(highHz, kMinHighHz, kMaxHighHz);
        s.gridOpacity = std::clamp(gridOpacity, 0, 100);
        s.appearance = pick(appearance, AppearanceAuto, AppearanceDark, AppearanceAuto);
        s.theme = pick(theme, 0, 11, 0);   // kThemeCount - 1
        return s;
    }
};

// Every field with its state key, for StateWriter / StateReader.
template <class S, class Visitor>
void visitColors(S& c, const char* prefix, Visitor&& v) {
    const std::string p(prefix);
    v(p + "trace", c.trace);
    v(p + "background", c.background);
    v(p + "grid", c.grid);
    v(p + "band_low", c.bands[0]);
    v(p + "band_mid", c.bands[1]);
    v(p + "band_high", c.bands[2]);
}

template <class Visitor>
void visitFields(ScopeSettings& s, Visitor&& v) {
    v("style", s.style);
    v("draw_mode", s.drawMode);
    v("trace_color_mode", s.traceColorMode);
    v("dot_size_tenths", s.dotSizeTenths);
    v("persistence_ms", s.persistenceMs);
    v("brightness", s.brightness);
    v("auto_gain", s.autoGain);
    v("gain_db", s.gainDb);
    v("show_correlation", s.showCorrelation);
    v("correlation_mode", s.correlationMode);
    v("meter_position", s.meterPosition);
    v("bar_thickness", s.barThickness);
    v("integration_ms", s.integrationMs);
    v("range_hold_ms", s.rangeHoldMs);
    v("low_hz", s.lowHz);
    v("high_hz", s.highHz);
    v("show_grid", s.showGrid);
    v("show_labels", s.showLabels);
    v("grid_opacity", s.gridOpacity);
    v("appearance", s.appearance);
    v("theme", s.theme);
    visitColors(s.light, "light.", v);
    visitColors(s.dark, "dark.", v);
}

// --- Themes ---
//
// Curated color presets. A theme stamps the trace, background, grid and band
// colors for both appearances at once. Band colors come from each palette's
// own accents, warm to cool, and keep clear of red: the correlation meter
// uses red for out-of-phase readings. Index 0 is "Custom" (applying it
// changes nothing); real presets start at 1.

struct ScopeTheme {
    const char* name;
    ScopeColors light;
    ScopeColors dark;
};

inline constexpr ScopeTheme kThemes[] = {
    {"Custom",
     {0xFF1B8A3A, 0xFFF4F4F4, 0xFF9AA0A0, {0xFFB86A00, 0xFF1B8A3A, 0xFF2E7DD1}},
     {0xFF33FF66, 0xFF0A0A0A, 0xFF4D4D4D, {0xFFFFB000, 0xFF33FF66, 0xFF4DA3F0}}},
    {"Default (Phosphor Green)",
     {0xFF1B8A3A, 0xFFF4F4F4, 0xFF9AA0A0, {0xFFB86A00, 0xFF1B8A3A, 0xFF2E7DD1}},
     {0xFF33FF66, 0xFF0A0A0A, 0xFF4D4D4D, {0xFFFFB000, 0xFF33FF66, 0xFF4DA3F0}}},
    {"Blue",
     {0xFF2E7DD1, 0xFFF2F2F2, 0xFF808080, {0xFFB86A00, 0xFF1F8F6B, 0xFF2E7DD1}},
     {0xFF4DA3F0, 0xFF161616, 0xFF909090, {0xFFF5A623, 0xFF4DD0A8, 0xFF4DA3F0}}},
    {"Amber",
     {0xFFB86A00, 0xFFF7F3EA, 0xFFB4AA96, {0xFFB86A00, 0xFF6B8E12, 0xFF2A8FA8}},
     {0xFFFFB000, 0xFF0C0A06, 0xFF4D4433, {0xFFFFB000, 0xFFB5D94A, 0xFF7CCBE0}}},
    {"Nord",
     {0xFF5E81AC, 0xFFECEFF4, 0xFFB0B6C0, {0xFFB37A1E, 0xFF5E8D4E, 0xFF5E81AC}},
     {0xFF88C0D0, 0xFF2E3440, 0xFF4C566A, {0xFFEBCB8B, 0xFFA3BE8C, 0xFF88C0D0}}},
    {"Dracula",
     {0xFF8B5CF6, 0xFFF8F8F2, 0xFFCFCFD6, {0xFFD97706, 0xFF8B5CF6, 0xFF0E9BB5}},
     {0xFFBD93F9, 0xFF282A36, 0xFF44475A, {0xFFFFB86C, 0xFFBD93F9, 0xFF8BE9FD}}},
    {"Gruvbox",
     {0xFFD65D0E, 0xFFFBF1C7, 0xFFBDB4A0, {0xFFC46A08, 0xFF79740E, 0xFF076678}},
     {0xFFFE8019, 0xFF282828, 0xFF504945, {0xFFFE8019, 0xFFB8BB26, 0xFF83A598}}},
    {"Solarized",
     {0xFF268BD2, 0xFFFDF6E3, 0xFF93A1A1, {0xFFB58900, 0xFF2AA198, 0xFF6C71C4}},
     {0xFF268BD2, 0xFF002B36, 0xFF586E75, {0xFFB58900, 0xFF2AA198, 0xFF6C71C4}}},
    {"Tokyo Night",
     {0xFF3D59A1, 0xFFD5D6DB, 0xFFA8AECB, {0xFFB15C00, 0xFF587539, 0xFF3D59A1}},
     {0xFF7AA2F7, 0xFF1A1B26, 0xFF414868, {0xFFE0AF68, 0xFF9ECE6A, 0xFF7AA2F7}}},
    {"Catppuccin",
     {0xFF8839EF, 0xFFEFF1F5, 0xFFBCC0CC, {0xFFC7791A, 0xFF8839EF, 0xFF1E66F5}},
     {0xFFCBA6F7, 0xFF1E1E2E, 0xFF45475A, {0xFFF9E2AF, 0xFFCBA6F7, 0xFF89DCEB}}},
    {"Monokai",
     {0xFF4E9A06, 0xFFF8F8F2, 0xFFC8C8B8, {0xFFCF7A00, 0xFF4E9A06, 0xFF1F9FB5}},
     {0xFFA6E22E, 0xFF272822, 0xFF49483E, {0xFFFD971F, 0xFFA6E22E, 0xFF66D9EF}}},
    {"Sunset",
     {0xFFE8551A, 0xFFFFF3EC, 0xFFD8B8A0, {0xFFC77700, 0xFFC2399A, 0xFF7C4DDB}},
     {0xFFFF6B35, 0xFF1A0F0A, 0xFF4D3326, {0xFFFFB347, 0xFFF472B6, 0xFFA78BFA}}},
};

inline constexpr int kThemeCount = static_cast<int>(sizeof(kThemes) / sizeof(kThemes[0]));
// sanitized() checks the theme index against this count.
static_assert(kThemeCount == 12);

// Stamp a preset's colors into `s`. No-op for 0 (Custom) or an invalid index.
inline void applyTheme(ScopeSettings& s, int index) {
    if (index <= 0 || index >= kThemeCount) return;
    s.light = kThemes[index].light;
    s.dark = kThemes[index].dark;
    s.theme = index;
}

} // namespace p31::scope
