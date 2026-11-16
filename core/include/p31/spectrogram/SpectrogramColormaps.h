//
//  SpectrogramColormaps.h
//  p31-core (ported from foo_jl_spectrogram_mac SpectrogramColormaps.h)
//
//  Color ramps that map a normalized level (0 = floor, 1 = top) to a color.
//  Plain C++, so it can be unit-tested and reused.
//

#pragma once

#include <cmath>
#include <cstdint>

namespace p31::spectrogram {

struct ColorStop {
    float    pos;   // 0..1, ascending
    uint32_t rgb;   // 0xRRGGBB
};

struct Colormap {
    const char*      name;
    const ColorStop* stops;
    int              count;
};

constexpr int kLutSize = 256;

// Perceptually uniform ramps (matplotlib), sampled at ten even stops.
inline constexpr ColorStop kInferno[] = {
    {0.000f, 0x000004}, {0.111f, 0x1B0C41}, {0.222f, 0x4A0C6B}, {0.333f, 0x781C6D},
    {0.444f, 0xA52C60}, {0.556f, 0xCF4446}, {0.667f, 0xED6925}, {0.778f, 0xFB9B06},
    {0.889f, 0xF7D13D}, {1.000f, 0xFCFFA4}
};
inline constexpr ColorStop kMagma[] = {
    {0.000f, 0x000004}, {0.111f, 0x180F3D}, {0.222f, 0x440F76}, {0.333f, 0x721F81},
    {0.444f, 0x9E2F7F}, {0.556f, 0xCD4071}, {0.667f, 0xF1605D}, {0.778f, 0xFD9668},
    {0.889f, 0xFECA8D}, {1.000f, 0xFCFDBF}
};
inline constexpr ColorStop kPlasma[] = {
    {0.000f, 0x0D0887}, {0.111f, 0x46039F}, {0.222f, 0x7201A8}, {0.333f, 0x9C179E},
    {0.444f, 0xBD3786}, {0.556f, 0xD8576B}, {0.667f, 0xED7953}, {0.778f, 0xFB9F3A},
    {0.889f, 0xFDCA26}, {1.000f, 0xF0F921}
};
inline constexpr ColorStop kViridis[] = {
    {0.000f, 0x440154}, {0.111f, 0x482878}, {0.222f, 0x3E4989}, {0.333f, 0x31688E},
    {0.444f, 0x26828E}, {0.556f, 0x1F9E89}, {0.667f, 0x35B779}, {0.778f, 0x6ECE58},
    {0.889f, 0xB5DE2B}, {1.000f, 0xFDE725}
};
inline constexpr ColorStop kTurbo[] = {
    {0.000f, 0x30123B}, {0.111f, 0x4662D7}, {0.222f, 0x36AAF9}, {0.333f, 0x1AE4B6},
    {0.444f, 0x72FE5E}, {0.556f, 0xC7EF34}, {0.667f, 0xFABA39}, {0.778f, 0xF66B19},
    {0.889f, 0xCA2A04}, {1.000f, 0x7A0403}
};
// Hardware-analyzer ramp: black, blue, purple, red, yellow, white.
inline constexpr ColorStop kClassic[] = {
    {0.00f, 0x000000}, {0.20f, 0x000040}, {0.40f, 0x1C1CE0}, {0.55f, 0x8412C4},
    {0.68f, 0xD0106A}, {0.78f, 0xE42410}, {0.89f, 0xF2C814}, {1.00f, 0xFFFFFF}
};
inline constexpr ColorStop kIce[] = {
    {0.00f, 0x000000}, {0.30f, 0x061A4A}, {0.55f, 0x1560C0}, {0.80f, 0x4FD0F0},
    {1.00f, 0xFFFFFF}
};
inline constexpr ColorStop kHeat[] = {
    {0.00f, 0x000000}, {0.35f, 0x7A0000}, {0.60f, 0xF03000}, {0.85f, 0xFFD000},
    {1.00f, 0xFFFFFF}
};
inline constexpr ColorStop kGray[] = {
    {0.00f, 0x000000}, {1.00f, 0xFFFFFF}
};

// Ramps from the P31 Spectrum color themes (dark variants): the theme's
// background, rising through its accent color to a near-white top.
inline constexpr ColorStop kThemeBlue[] = {
    {0.00f, 0x161616}, {0.30f, 0x264057}, {0.65f, 0x4287C4}, {0.85f, 0x4DA3F0}, {1.00f, 0xD2E8FB}
};
inline constexpr ColorStop kThemeGreen[] = {
    {0.00f, 0x0A0A0A}, {0.30f, 0x165426}, {0.65f, 0x2BCE54}, {0.85f, 0x33FF66}, {1.00f, 0xCCFFD9}
};
inline constexpr ColorStop kThemeNord[] = {
    {0.00f, 0x2E3440}, {0.30f, 0x495E6B}, {0.65f, 0x76A4B3}, {0.85f, 0x88C0D0}, {1.00f, 0xE1EFF3}
};
inline constexpr ColorStop kThemeDracula[] = {
    {0.00f, 0x282A36}, {0.30f, 0x554A70}, {0.65f, 0x9F7ED2}, {0.85f, 0xBD93F9}, {1.00f, 0xEEE4FE}
};
inline constexpr ColorStop kThemeGruvbox[] = {
    {0.00f, 0x282828}, {0.30f, 0x684224}, {0.65f, 0xD36E1C}, {0.85f, 0xFE8019}, {1.00f, 0xFFDFC6}
};
inline constexpr ColorStop kThemeSolarized[] = {
    {0.00f, 0x002B36}, {0.30f, 0x0B4865}, {0.65f, 0x1E78B3}, {0.85f, 0x268BD2}, {1.00f, 0xC9E2F4}
};
inline constexpr ColorStop kThemeTokyoNight[] = {
    {0.00f, 0x1A1B26}, {0.30f, 0x374465}, {0.65f, 0x6787CD}, {0.85f, 0x7AA2F7}, {1.00f, 0xDEE8FD}
};
inline constexpr ColorStop kThemeCatppuccin[] = {
    {0.00f, 0x1E1E2E}, {0.30f, 0x52476A}, {0.65f, 0xA88BCF}, {0.85f, 0xCBA6F7}, {1.00f, 0xF2E9FD}
};
inline constexpr ColorStop kThemeMonokai[] = {
    {0.00f, 0x272822}, {0.30f, 0x4D6026}, {0.65f, 0x8DBD2C}, {0.85f, 0xA6E22E}, {1.00f, 0xE9F8CB}
};
inline constexpr ColorStop kThemeSunset[] = {
    {0.00f, 0x1A0F0A}, {0.30f, 0x5F2B17}, {0.65f, 0xD1592C}, {0.85f, 0xFF6B35}, {1.00f, 0xFFDACC}
};
inline constexpr ColorStop kThemeP31[] = {
    {0.00f, 0x0C120E}, {0.30f, 0x1F592F}, {0.65f, 0x3FD066}, {0.85f, 0x4CFF7C}, {1.00f, 0xD2FFDE}
};
inline constexpr ColorStop kThemeP3[] = {
    {0.00f, 0x130F07}, {0.30f, 0x5A3F05}, {0.65f, 0xD09001}, {0.85f, 0xFFB000}, {1.00f, 0xFFEBBF}
};
inline constexpr ColorStop kThemeP4[] = {
    {0.00f, 0x0F1113}, {0.30f, 0x505356}, {0.65f, 0xBBC1C5}, {0.85f, 0xE6EDF2}, {1.00f, 0xF9FAFC}
};
inline constexpr ColorStop kThemeP7[] = {
    {0.00f, 0x0A0E15}, {0.30f, 0x28415B}, {0.65f, 0x5B95D0}, {0.85f, 0x6FB7FF}, {1.00f, 0xDBEDFF}
};

#define P31_SPECTROGRAM_MAP(name, stops) { name, stops, static_cast<int>(sizeof(stops) / sizeof(stops[0])) }
inline constexpr Colormap kMaps[] = {
    P31_SPECTROGRAM_MAP("Inferno", kInferno),
    P31_SPECTROGRAM_MAP("Magma", kMagma),
    P31_SPECTROGRAM_MAP("Classic", kClassic),
    P31_SPECTROGRAM_MAP("Ice", kIce),
    P31_SPECTROGRAM_MAP("Heat", kHeat),
    P31_SPECTROGRAM_MAP("Plasma", kPlasma),
    P31_SPECTROGRAM_MAP("Viridis", kViridis),
    P31_SPECTROGRAM_MAP("Turbo", kTurbo),
    P31_SPECTROGRAM_MAP("Grayscale", kGray),
    // Theme ramps start here (kFirstThemeMap). New maps go at the end so a
    // saved index keeps its meaning.
    P31_SPECTROGRAM_MAP("Default (Blue)", kThemeBlue),
    P31_SPECTROGRAM_MAP("Classic (Green)", kThemeGreen),
    P31_SPECTROGRAM_MAP("Nord", kThemeNord),
    P31_SPECTROGRAM_MAP("Dracula", kThemeDracula),
    P31_SPECTROGRAM_MAP("Gruvbox", kThemeGruvbox),
    P31_SPECTROGRAM_MAP("Solarized", kThemeSolarized),
    P31_SPECTROGRAM_MAP("Tokyo Night", kThemeTokyoNight),
    P31_SPECTROGRAM_MAP("Catppuccin", kThemeCatppuccin),
    P31_SPECTROGRAM_MAP("Monokai", kThemeMonokai),
    P31_SPECTROGRAM_MAP("Sunset", kThemeSunset),
    P31_SPECTROGRAM_MAP("P31 green", kThemeP31),
    P31_SPECTROGRAM_MAP("P3 amber", kThemeP3),
    P31_SPECTROGRAM_MAP("P4 white", kThemeP4),
    P31_SPECTROGRAM_MAP("P7 blue", kThemeP7),
};
#undef P31_SPECTROGRAM_MAP
constexpr int kMapCount = static_cast<int>(sizeof(kMaps) / sizeof(kMaps[0]));
constexpr int kFirstThemeMap = 9;  // index of "Default (Blue)"

// Panel colors of the P31 Spectrum theme behind each theme ramp (ARGB), in
// kMaps order from kFirstThemeMap. Choosing a theme ramp stamps these.
struct ThemePanel {
    uint32_t bgLight, gridLight, bgDark, gridDark;
};
inline constexpr ThemePanel kThemePanels[] = {
    {0xFFF2F2F2, 0xFF808080, 0xFF161616, 0xFF909090},   // Default (Blue)
    {0xFFF4F4F4, 0xFF9AA0A0, 0xFF0A0A0A, 0xFF4D4D4D},   // Classic (Green)
    {0xFFECEFF4, 0xFFB0B6C0, 0xFF2E3440, 0xFF4C566A},   // Nord
    {0xFFF8F8F2, 0xFFCFCFD6, 0xFF282A36, 0xFF44475A},   // Dracula
    {0xFFFBF1C7, 0xFFBDB4A0, 0xFF282828, 0xFF504945},   // Gruvbox
    {0xFFFDF6E3, 0xFF93A1A1, 0xFF002B36, 0xFF586E75},   // Solarized
    {0xFFD5D6DB, 0xFFA8AECB, 0xFF1A1B26, 0xFF414868},   // Tokyo Night
    {0xFFEFF1F5, 0xFFBCC0CC, 0xFF1E1E2E, 0xFF45475A},   // Catppuccin
    {0xFFF8F8F2, 0xFFC8C8B8, 0xFF272822, 0xFF49483E},   // Monokai
    {0xFFFFF3EC, 0xFFD8B8A0, 0xFF1A0F0A, 0xFF4D3326},   // Sunset
    {0xFFF1F5F2, 0xFF5F7A66, 0xFF0C120E, 0xFFB8E6C4},   // P31 green
    {0xFFF7F3EA, 0xFF7D6E55, 0xFF130F07, 0xFFE8D4AE},   // P3 amber
    {0xFFF4F5F6, 0xFF6E767E, 0xFF0F1113, 0xFFD5DCE2},   // P4 white
    {0xFFF0F4F9, 0xFF64748A, 0xFF0A0E15, 0xFFC4D6EC},   // P7 blue
};
static_assert(sizeof(kThemePanels) / sizeof(kThemePanels[0]) == kMapCount - kFirstThemeMap,
              "one panel entry per theme ramp");

// "Custom": a ramp through four user colors at these positions.
constexpr int   kCustomMap = -1;
constexpr int   kCustomColorCount = 4;   // floor, low, mid, peak
constexpr float kCustomStops[kCustomColorCount] = {0.0f, 0.35f, 0.7f, 1.0f};

// Which color space a map's numbers are in. The theme ramps and the custom
// ramp are built from theme / user colors, which are stored as Display P3
// (p31/render/ThemeColor.h). The general-purpose maps are defined in sRGB;
// reading them as P3 would distort them.
inline bool isDisplayP3Map(int index) {
    return index == kCustomMap || index >= kFirstThemeMap;
}

// Color at position t (0..1) as 0xFFRRGGBB, interpolated between stops.
inline uint32_t colorAt(const Colormap& map, float t) {
    if (t <= map.stops[0].pos) return 0xFF000000u | map.stops[0].rgb;
    for (int i = 1; i < map.count; ++i) {
        const ColorStop& a = map.stops[i - 1];
        const ColorStop& b = map.stops[i];
        if (t > b.pos) continue;
        const float u = (t - a.pos) / (b.pos - a.pos);
        uint32_t out = 0xFF000000u;
        for (int shift = 16; shift >= 0; shift -= 8) {
            const float ca = static_cast<float>((a.rgb >> shift) & 0xFF);
            const float cb = static_cast<float>((b.rgb >> shift) & 0xFF);
            out |= static_cast<uint32_t>(ca + (cb - ca) * u + 0.5f) << shift;
        }
        return out;
    }
    return 0xFF000000u | map.stops[map.count - 1].rgb;
}

// Fill `lut` (kLutSize entries, 0xFFRRGGBB) for the map at `index`.
// An out-of-range index (corrupted config) falls back to the first map.
inline void buildLut(int index, uint32_t* lut) {
    if (index < 0 || index >= kMapCount) index = 0;
    for (int i = 0; i < kLutSize; ++i) {
        lut[i] = colorAt(kMaps[index], static_cast<float>(i) / static_cast<float>(kLutSize - 1));
    }
}

// Fill `lut` for the custom ramp through `colors` (kCustomColorCount, ARGB).
inline void buildCustomLut(const uint32_t* colors, uint32_t* lut) {
    ColorStop stops[kCustomColorCount];
    for (int i = 0; i < kCustomColorCount; ++i) stops[i] = {kCustomStops[i], colors[i] & 0xFFFFFFu};
    const Colormap map = {"Custom", stops, kCustomColorCount};
    for (int i = 0; i < kLutSize; ++i) {
        lut[i] = colorAt(map, static_cast<float>(i) / static_cast<float>(kLutSize - 1));
    }
}

// The colormap's signature color for markers drawn over the picture (the
// frequency probe): the most vivid of its upper half, scored saturation x
// value, so a ramp that ends in white still yields its color (Inferno: orange,
// theme ramps: the theme accent). A colorless map yields its top.
inline uint32_t accentColor(const uint32_t* lut) {
    float best = -1.0f;
    uint32_t pick = lut[kLutSize - 1];
    for (int i = kLutSize / 2; i < kLutSize; ++i) {
        const float r = static_cast<float>((lut[i] >> 16) & 0xFF) / 255.0f;
        const float g = static_cast<float>((lut[i] >> 8) & 0xFF) / 255.0f;
        const float b = static_cast<float>(lut[i] & 0xFF) / 255.0f;
        const float hi = r > g ? (r > b ? r : b) : (g > b ? g : b);
        const float lo = r < g ? (r < b ? r : b) : (g < b ? g : b);
        const float score = hi > 0.0f ? (hi - lo) : 0.0f;   // saturation x value
        if (score > best + 1e-4f) { best = score; pick = lut[i]; }
    }
    return best < 0.08f ? lut[kLutSize - 1] : pick;
}

// The custom colors that reproduce map `index` as closely as four stops can:
// the map sampled at kCustomStops. Used to seed the color wells.
inline uint32_t customColorFromMap(int index, int i) {
    if (index < 0 || index >= kMapCount) index = 0;
    return colorAt(kMaps[index], kCustomStops[i]);
}

// The same color in Display P3 numbers (0xAARRGGBB in, alpha kept). Stored
// colors are P3; a general-purpose map seeding them is sRGB, so it is
// converted to keep its look. Both spaces share the sRGB transfer curve and
// the D65 white point: linearize, change primaries, encode again.
inline uint32_t srgbToDisplayP3(uint32_t argb) {
    const auto linear = [](uint32_t byte) {
        const double v = static_cast<double>(byte & 0xFF) / 255.0;
        return v <= 0.04045 ? v / 12.92 : std::pow((v + 0.055) / 1.055, 2.4);
    };
    const auto encoded = [](double v) {
        v = v < 0.0 ? 0.0 : (v > 1.0 ? 1.0 : v);
        const double e = v <= 0.0031308 ? v * 12.92 : 1.055 * std::pow(v, 1.0 / 2.4) - 0.055;
        return static_cast<uint32_t>(std::lround(e * 255.0)) & 0xFFu;
    };
    const double r = linear(argb >> 16), g = linear(argb >> 8), b = linear(argb);
    return (argb & 0xFF000000u) |
           (encoded(0.8224621 * r + 0.1775380 * g) << 16) |
           (encoded(0.0331941 * r + 0.9668058 * g) << 8) |
           encoded(0.0170827 * r + 0.0723974 * g + 0.9105199 * b);
}

// Map `index`'s color at custom stop `i`, as the opaque Display P3 color the
// custom ramp is stored in.
inline uint32_t customColorSeed(int index, int i) {
    const uint32_t c = 0xFF000000u | customColorFromMap(index, i);
    return isDisplayP3Map(index) ? c : srgbToDisplayP3(c);
}

} // namespace p31::spectrogram
