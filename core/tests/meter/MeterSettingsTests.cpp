//
//  MeterSettingsTests.cpp
//  p31-core-tests
//
//  P31 Meter's settings: defaults as the fb2k component's, sanitising, the
//  state text and the themes.
//

#include <p31/core/StateCodec.h>
#include <p31/meter/MeterSettings.h>

#include <doctest/doctest.h>

#include <set>
#include <string>

using namespace p31;
using namespace p31::meter;

namespace {

// Collects the keys visitFields reports.
struct KeyCollector {
    std::set<std::string> keys;
    int count = 0;
    template <class T>
    void operator()(std::string_view key, T&) {
        keys.insert(std::string(key));
        ++count;
    }
};

} // namespace

TEST_CASE("Meter settings: defaults map to the engine's and the renderer's") {
    const MeterSettings s;
    const EngineSettings e = engineSettings(s);
    const EngineSettings engineDefaults;
    CHECK(e.rmsWindowSeconds == doctest::Approx(engineDefaults.rmsWindowSeconds));
    CHECK(e.peakFallDbPerSecond == doctest::Approx(engineDefaults.peakFallDbPerSecond));
    CHECK(e.peakHoldSeconds == doctest::Approx(engineDefaults.peakHoldSeconds));
    CHECK(e.truePeak == engineDefaults.truePeak);
    CHECK(e.needle == engineDefaults.needle);
    CHECK(e.referenceDbfs == doctest::Approx(engineDefaults.referenceDbfs));

    const DisplaySettings d = displaySettings(s, true);
    const DisplaySettings displayDefaults;
    CHECK(d.style == displayDefaults.style);
    CHECK(d.spacing == displayDefaults.spacing);
    CHECK(d.floorDb == displayDefaults.floorDb);
    CHECK(d.textScale == doctest::Approx(1.0));
    CHECK(d.loudnessTarget == doctest::Approx(displayDefaults.loudnessTarget));
    CHECK(d.warnDb == doctest::Approx(displayDefaults.warnDb));
    CHECK(d.overDb == doctest::Approx(displayDefaults.overDb));
    CHECK(d.peakLedDbfs == doctest::Approx(displayDefaults.peakLedDbfs));

    // The default colours are the default theme's.
    CHECK(themeMatching(s) == kDefaultTheme);
    CHECK(d.palette.dark);
    CHECK(d.palette.safe == kMeterThemes[kDefaultTheme].dark[ColorLevel]);
    CHECK(displaySettings(s, false).palette.background == kMeterThemes[kDefaultTheme].light[ColorBackground]);
}

TEST_CASE("Meter settings: values out of range are pulled back") {
    MeterSettings s;
    s.style = 7;
    s.textSize = 11;
    s.floorDb = -50;
    s.targetLufs = -15;
    s.rmsMs = 1;
    s.peakFallTenths = 100000;
    s.warnDb = -6;
    s.overDb = -20;      // below the warning level
    s.needle = -1;
    s.referenceDb = 0;
    s.theme = 99;
    s.appearance = 5;
    const MeterSettings c = s.sanitized();
    CHECK(c.style == static_cast<int>(MeterStyle::Digital));
    CHECK(c.textSize == 10);
    CHECK(c.floorDb == -60);
    CHECK(c.targetLufs == -14);
    CHECK(c.rmsMs == kMinRmsMs);
    CHECK(c.peakFallTenths == kMaxPeakFallTenths);
    CHECK(c.warnDb == -6);
    CHECK(c.overDb == -6);
    CHECK(c.needle == 0);
    CHECK(c.referenceDb == kMaxReferenceDb);
    CHECK(c.theme == 0);
    CHECK(c.appearance == AppearanceAuto);
}

TEST_CASE("Meter settings: text round trip, stable keys") {
    MeterSettings changed;
    changed.style = static_cast<int>(MeterStyle::Analog);
    changed.segments = true;
    changed.rmsMs = 600;
    changed.light[ColorLevel] = 0xFF123456;
    changed.dark[ColorLoudness] = 0xFF654321;
    const std::string text = encodeSettings(changed);
    MeterSettings loaded;
    decodeSettings(text, loaded);
    CHECK(encodeSettings(loaded) == text);
    CHECK(loaded.dark[ColorLoudness] == 0xFF654321);

    // Renaming a key silently resets that setting in saved projects.
    KeyCollector keys;
    visitFields(loaded, keys);
    CHECK(keys.count == 41);
    CHECK(keys.count == static_cast<int>(keys.keys.size()));
    CHECK(keys.keys.count("peak_fall_tenths") == 1);
    CHECK(keys.keys.count("needle_ballistics") == 1);
    CHECK(keys.keys.count("dark.loudness") == 1);
}

TEST_CASE("Meter settings: themes") {
    MeterSettings s;
    applyTheme(s, 14);   // P7 blue
    CHECK(s.theme == 14);
    CHECK(s.dark[ColorLevel] == 0xFF6FB7FF);
    CHECK(themeMatching(s) == 14);

    // An edited colour reads as Custom, whatever index is stored.
    s.dark[ColorLevel] = 0xFF010203;
    CHECK(themeMatching(s) == 0);

    // Custom and invalid indices change nothing.
    const MeterSettings before = s;
    applyTheme(s, 0);
    applyTheme(s, 99);
    CHECK(encodeSettings(s) == encodeSettings(before));

    // Every preset is found by its colours.
    for (int i = 1; i < kMeterThemeCount; ++i) {
        MeterSettings t;
        applyTheme(t, i);
        CHECK(themeMatching(t) == i);
    }
}
