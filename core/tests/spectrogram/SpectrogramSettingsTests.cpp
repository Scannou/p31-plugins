//
//  SpectrogramSettingsTests.cpp
//  p31-core-tests
//

#include <p31/core/StateCodec.h>
#include <p31/spectrogram/SpectrogramSettings.h>

#include <doctest/doctest.h>

#include <string>

using namespace p31::spectrogram;

TEST_CASE("Spectrogram defaults are the fb2k component's") {
    const SpectrogramSettings s;
    CHECK(s.fftSize == 4096);
    CHECK(s.analysisMode == SpectrogramEngine::ModeReassigned);
    CHECK(s.rangeDb == 60);
    CHECK(s.scrollSpeed == 120);
    CHECK(s.orientation == OrientationVertical);
    CHECK(std::string(kMaps[s.colormap].name) == "Inferno");
    // The custom ramp starts as the default colormap.
    for (int i = 0; i < kCustomColorCount; ++i) CHECK(s.customColors[i] == customColorSeed(s.colormap, i));
}

TEST_CASE("Spectrogram settings survive the state text") {
    SpectrogramSettings s;
    s.fftSize = 8192;
    s.analysisMode = SpectrogramEngine::ModeMultiRes;
    s.channel = ChannelSide;
    s.topDb = -12;
    s.slopeTenths = 30;
    s.reverseScroll = true;
    s.showLegend = false;
    s.colormap = kCustomMap;
    s.customColors[2] = 0xFF123456;
    s.dark.grid = 0xFFABCDEF;

    SpectrogramSettings back;
    p31::decodeSettings(p31::encodeSettings(s), back);
    CHECK(back.fftSize == 8192);
    CHECK(back.analysisMode == SpectrogramEngine::ModeMultiRes);
    CHECK(back.channel == ChannelSide);
    CHECK(back.topDb == -12);
    CHECK(back.slopeTenths == 30);
    CHECK(back.reverseScroll);
    CHECK_FALSE(back.showLegend);
    CHECK(back.colormap == kCustomMap);
    CHECK(back.customColors[2] == 0xFF123456);
    CHECK(back.dark.grid == 0xFFABCDEF);
    // Unset fields keep their defaults.
    CHECK(back.light.background == SpectrogramSettings{}.light.background);
}

TEST_CASE("Spectrogram settings loaded from disk are brought into range") {
    SpectrogramSettings s;
    s.fftSize = 3000;
    s.analysisMode = 9;
    s.channel = -1;
    s.minHz = 5000;
    s.maxHz = 10;
    s.topDb = 20;
    s.rangeDb = 5;
    s.scrollSpeed = 77;
    s.orientation = 4;
    s.freqFontSize = 40;
    s.colormap = kMapCount;
    s.customColors[0] = 0x00102030;

    const SpectrogramSettings c = s.sanitized();
    CHECK(c.fftSize == 4096);
    CHECK(c.analysisMode == SpectrogramEngine::ModeReassigned);
    CHECK(c.channel == ChannelMid);
    CHECK(c.minHz == 2000);
    CHECK(c.maxHz == 2100);
    CHECK(c.topDb == kMaxTopDb);
    CHECK(c.rangeDb == kMinRangeDb);
    CHECK(c.scrollSpeed == 120);
    CHECK(c.orientation == OrientationVertical);
    CHECK(c.freqFontSize == kMaxFontSize);
    CHECK(c.colormap == 0);
    CHECK(c.customColors[0] == 0xFF102030);

    s.colormap = kCustomMap;
    CHECK(s.sanitized().colormap == kCustomMap);
}

TEST_CASE("Engine settings follow the user settings") {
    SpectrogramSettings s;
    s.fftSize = 2048;
    s.analysisMode = SpectrogramEngine::ModeStandard;
    s.freqScale = SpectrogramEngine::ScaleMel;
    s.minHz = 30;
    s.maxHz = 16000;
    s.topDb = -6;
    s.rangeDb = 90;
    s.slopeTenths = 45;
    const SpectrogramEngine::Settings e = engineSettings(s);
    CHECK(e.fftSize == 2048);
    CHECK(e.mode == SpectrogramEngine::ModeStandard);
    CHECK(e.scale == SpectrogramEngine::ScaleMel);
    CHECK(e.minHz == 30.0);
    CHECK(e.maxHz == 16000.0);
    CHECK(e.topDb == -6.0f);
    CHECK(e.rangeDb == 90.0f);
    CHECK(e.slopeDb == doctest::Approx(4.5f));
}

TEST_CASE("Choosing a colormap fills in the colors it brings") {
    SpectrogramSettings s;
    const PanelColors light = s.light, dark = s.dark;

    // A general-purpose map: the custom ramp follows, the panel stays.
    int viridis = -1;
    for (int m = 0; m < kMapCount; ++m) if (std::string(kMaps[m].name) == "Viridis") viridis = m;
    REQUIRE(viridis >= 0);
    applyColormap(s, viridis);
    CHECK(s.colormap == viridis);
    for (int i = 0; i < kCustomColorCount; ++i) CHECK(s.customColors[i] == customColorSeed(viridis, i));
    CHECK(s.light.background == light.background);
    CHECK(s.dark.grid == dark.grid);

    // A theme ramp also brings its panel colors.
    const int p31 = kMapCount - 4;
    REQUIRE(std::string(kMaps[p31].name) == "P31 green");
    applyColormap(s, p31);
    CHECK(s.colormap == p31);
    CHECK(s.dark.background == 0xFF0C120E);
    CHECK(s.dark.grid == 0xFFB8E6C4);
    CHECK(s.light.background == 0xFFF1F5F2);
    CHECK(s.customColors[0] == 0xFF0C120E);

    // Custom keeps every color; an invalid index changes nothing.
    const SpectrogramSettings before = s;
    applyColormap(s, kCustomMap);
    CHECK(s.colormap == kCustomMap);
    CHECK(s.customColors[3] == before.customColors[3]);
    CHECK(s.dark.background == before.dark.background);
    applyColormap(s, kMapCount);
    CHECK(s.colormap == kCustomMap);

    // The lookup table is the custom ramp's, or the chosen map's.
    uint32_t lut[kLutSize], want[kLutSize];
    s.customColors[0] = 0xFF112233;
    buildLut(s, lut);
    CHECK(lut[0] == 0xFF112233);
    applyColormap(s, viridis);
    buildLut(s, lut);
    buildLut(viridis, want);
    CHECK(lut[128] == want[128]);
}
