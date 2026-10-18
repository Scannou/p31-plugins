//
//  StateCodecTests.cpp
//  p31-core-tests
//

#include <p31/core/StateCodec.h>
#include <p31/scope/ScopeSettings.h>
#include <p31/spectrum/SpectrumSettings.h>

#include <doctest/doctest.h>

#include <set>
#include <string>

using namespace p31;

namespace {

// Collects the keys a visitFields overload reports.
struct KeyCollector {
    std::set<std::string> keys;
    int count = 0;
    template <class T>
    void operator()(std::string_view key, T&) {
        keys.insert(std::string(key));
        ++count;
    }
};

// A settings value with every field changed from its default, by visiting
// each field and bumping it.
struct Bumper {
    int n = 0;
    void operator()(std::string_view, int& v) { v += 1 + (n++ % 3); }
    void operator()(std::string_view, bool& v) { v = !v; }
    void operator()(std::string_view, uint32_t& v) { v ^= 0x00102030u + static_cast<uint32_t>(n++); }
};

template <class S>
void checkRoundTrip() {
    S changed;
    Bumper bump;
    visitFields(changed, bump);
    const std::string text = encodeSettings(changed);
    S loaded;
    decodeSettings(text, loaded);
    CHECK(encodeSettings(loaded) == text);

    KeyCollector keys;
    visitFields(loaded, keys);
    CHECK(keys.count == static_cast<int>(keys.keys.size()));   // no duplicate keys
}

} // namespace

TEST_CASE("StateWriter writes key=value lines") {
    StateWriter w;
    w.put("a", 1);
    w.put("b.c_2", -40);
    w.put("Bad Key", 3);   // invalid keys are dropped
    CHECK(w.text() == "a=1\nb.c_2=-40\n");
}

TEST_CASE("StateReader parses what it can and skips the rest") {
    const StateReader r("a=1\r\nbroken\n=5\nb=x\nc=12abc\nd=-7\nlarge=4294967295\n\ne=9");
    int64_t v = 0;
    CHECK(r.get("a", v));
    CHECK(v == 1);
    CHECK(r.get("d", v));
    CHECK(v == -7);
    CHECK(r.get("e", v));
    CHECK(v == 9);
    CHECK_FALSE(r.has("b"));
    CHECK_FALSE(r.has("c"));
    CHECK_FALSE(r.has(""));

    int i = 3;
    r("large", i);   // out of int range: left alone
    CHECK(i == 3);
    uint32_t u = 0;
    r("large", u);
    CHECK(u == 4294967295u);
    bool b = false;
    r("d", b);       // not 0 / 1: left alone
    CHECK_FALSE(b);
    r("a", b);
    CHECK(b);
}

TEST_CASE("Scope settings survive a round trip with every field changed") {
    checkRoundTrip<scope::ScopeSettings>();
}

TEST_CASE("Spectrum settings survive a round trip with every field changed") {
    checkRoundTrip<spectrum::SpectrumSettings>();
}

TEST_CASE("Missing keys keep defaults, unknown keys are ignored") {
    spectrum::SpectrumSettings s;
    decodeSettings("fft_size=8192\nfrom_the_future=1\n", s);
    CHECK(s.fftSize == 8192);
    const spectrum::SpectrumSettings defaults;
    CHECK(s.rangeDb == defaults.rangeDb);
    CHECK(s.dark.bar == defaults.dark.bar);
}

TEST_CASE("Garbage state loads as sanitized defaults") {
    scope::ScopeSettings s;
    decodeSettings("persistence_ms=-99999\nstyle=42\nlight.trace=hello\n\x01\x02", s);
    const scope::ScopeSettings clean = s.sanitized();
    CHECK(clean.persistenceMs == scope::kMinPersistenceMs);
    CHECK(clean.style == scope::ScopeStyleGoniometer);
    CHECK(clean.light.trace == scope::ScopeSettings{}.light.trace);
}

TEST_CASE("Keys are stable") {
    // Renaming a key silently resets that setting in saved projects.
    KeyCollector scopeKeys, spectrumKeys;
    scope::ScopeSettings a;
    spectrum::SpectrumSettings b;
    visitFields(a, scopeKeys);
    visitFields(b, spectrumKeys);
    CHECK(scopeKeys.count == 33);
    CHECK(spectrumKeys.count == 36);
    CHECK(scopeKeys.keys.count("persistence_ms") == 1);
    CHECK(scopeKeys.keys.count("dark.band_high") == 1);
    CHECK(spectrumKeys.keys.count("peak_fall_db_s") == 1);
    CHECK(spectrumKeys.keys.count("light.grid") == 1);
}
