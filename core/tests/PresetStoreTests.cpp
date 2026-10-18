//
//  PresetStoreTests.cpp
//  p31-core-tests
//

#include <p31/core/PresetStore.h>

#include <doctest/doctest.h>

#include <filesystem>
#include <fstream>
#include <random>
#include <string>

namespace fs = std::filesystem;
using p31::PresetStore;

namespace {

// A fresh folder under the system temp dir, removed afterwards.
struct TempDir {
    fs::path path;
    TempDir() {
        std::random_device rd;
        path = fs::temp_directory_path() / ("p31-presets-" + std::to_string(rd()));
    }
    ~TempDir() {
        std::error_code ec;
        fs::remove_all(path, ec);
    }
};

} // namespace

TEST_CASE("Presets save, list, load and delete") {
    TempDir dir;
    const PresetStore store(dir.path / "presets", dir.path / "support" / "default.p31preset");
    CHECK(store.names().empty());   // folder does not exist yet

    CHECK(store.save("zeta", "a=1\n"));
    CHECK(store.save("Alpha", "a=2\n"));
    CHECK(store.save("beta", "a=3\n"));
    CHECK(store.names() == std::vector<std::string>{"Alpha", "beta", "zeta"});
    CHECK(store.load("beta").value() == "a=3\n");
    CHECK(store.exists("zeta"));

    CHECK(store.save("beta", "a=4\n"));   // overwrite
    CHECK(store.load("beta").value() == "a=4\n");

    CHECK(store.remove("zeta"));
    CHECK_FALSE(store.exists("zeta"));
    CHECK_FALSE(store.load("zeta").has_value());
    CHECK(store.names().size() == 2);

    // Other files in the folder are not presets.
    { std::ofstream(dir.path / "presets" / "notes.txt") << "x"; }
    CHECK(store.names().size() == 2);
}

TEST_CASE("Preset names cannot escape the folder") {
    CHECK(PresetStore::cleanName("  Warm / Dark  ") == "Warm - Dark");
    CHECK(PresetStore::cleanName("../../etc") == "-.-..-etc");
    CHECK(PresetStore::cleanName(".hidden") == "-hidden");
    CHECK(PresetStore::cleanName("   ").empty());
    CHECK(PresetStore::cleanName(std::string(300, 'x')).size() == 100);

    TempDir dir;
    const PresetStore store(dir.path / "presets", dir.path / "default.p31preset");
    CHECK_FALSE(store.save("   ", "a=1\n"));
    CHECK(store.save("../escape", "a=1\n"));
    CHECK_FALSE(fs::exists(dir.path / "escape.p31preset"));
    CHECK(store.names() == std::vector<std::string>{"-.-escape"});
}

TEST_CASE("Default settings for new instances") {
    TempDir dir;
    const PresetStore store(dir.path / "presets", dir.path / "support" / "default.p31preset");
    CHECK_FALSE(store.hasDefault());
    CHECK_FALSE(store.loadDefault().has_value());
    CHECK(store.saveDefault("b=5\n"));
    CHECK(store.hasDefault());
    CHECK(store.loadDefault().value() == "b=5\n");
    CHECK(store.names().empty());   // not listed as a user preset
    CHECK(store.clearDefault());
    CHECK_FALSE(store.hasDefault());
}

TEST_CASE("Standard locations") {
    const PresetStore store = PresetStore::forPlugin("P31 Scope");
    const std::string dir = store.presetDir().string();
    CHECK(dir.find("Library/Audio/Presets/P31/P31 Scope") != std::string::npos);
}
