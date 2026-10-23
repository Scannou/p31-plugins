//
//  ScopeSettingsTests.cpp
//  p31-core-tests
//

#include <p31/scope/ScopeSettings.h>
#include <p31/spectrum/SpectrumSettings.h>

#include <doctest/doctest.h>

#include <string>

namespace scope = p31::scope;
namespace spectrum = p31::spectrum;

TEST_CASE("Scope themes match P31 Spectrum's names, indices and menu order") {
    REQUIRE(scope::kThemeCount == spectrum::kThemeCount);
    for (int i = 1; i < scope::kThemeCount; ++i) {
        CAPTURE(i);
        CHECK(std::string(scope::kThemes[i].name) == spectrum::kThemes[i].name);
        CHECK(scope::kThemes[i].light.trace == spectrum::kThemes[i].light.bar);
        CHECK(scope::kThemes[i].dark.trace == spectrum::kThemes[i].dark.bar);
        CHECK(scope::kThemes[i].light.background == spectrum::kThemes[i].light.background);
        CHECK(scope::kThemes[i].dark.background == spectrum::kThemes[i].dark.background);
        CHECK(scope::kThemes[i].light.grid == spectrum::kThemes[i].light.grid);
        CHECK(scope::kThemes[i].dark.grid == spectrum::kThemes[i].dark.grid);
        CHECK(scope::kThemeMenuOrder[i] == spectrum::kThemeMenuOrder[i]);
    }
}

TEST_CASE("The default colors are the theme the default names") {
    const scope::ScopeSettings s;
    CHECK(std::string(scope::kThemes[s.theme].name) == "Classic (Green)");
    CHECK(scope::themeMatching(s) == s.theme);
}

TEST_CASE("The theme shown is the one the colors match") {
    scope::ScopeSettings s;
    scope::applyTheme(s, 12);
    CHECK(std::string(scope::kThemes[12].name) == "P3 amber");
    CHECK(scope::themeMatching(s) == 12);

    // A project saved with the older list: index 4 was Nord, now Dracula.
    scope::applyTheme(s, 3);   // Nord's colors
    s.theme = 4;
    CHECK(scope::themeMatching(s) == 3);

    s.dark.bands[1] = 0xFF123456;   // one hand-picked color
    CHECK(scope::themeMatching(s) == 0);
}
