//
//  PresetStore.h
//  p31-core
//
//  User presets of one plugin as text files (StateCodec format), plus the
//  settings new instances start with:
//
//    ~/Library/Audio/Presets/P31/<plugin>/<name>.p31preset
//    ~/Library/Application Support/P31/<plugin>/default.p31preset
//
//  Main thread only. Every call fails softly: a missing or unreadable folder
//  reads as no presets.
//

#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace p31 {

class PresetStore {
public:
    static constexpr const char* kExtension = ".p31preset";

    PresetStore(std::filesystem::path presetDir, std::filesystem::path defaultFile);
    // The standard locations for `pluginName` under the user's home folder.
    static PresetStore forPlugin(std::string_view pluginName);

    const std::filesystem::path& presetDir() const { return _presetDir; }

    // User preset names, sorted case-insensitively.
    std::vector<std::string> names() const;
    std::optional<std::string> load(std::string_view name) const;
    bool save(std::string_view name, std::string_view text) const;
    bool remove(std::string_view name) const;
    bool exists(std::string_view name) const;

    // A name usable as a file name: path separators and leading dots
    // replaced, trimmed, at most 100 characters. Empty when nothing is left.
    static std::string cleanName(std::string_view name);

    // Settings for new instances.
    std::optional<std::string> loadDefault() const;
    bool saveDefault(std::string_view text) const;
    bool clearDefault() const;
    bool hasDefault() const;

private:
    std::filesystem::path fileFor(std::string_view name) const;

    std::filesystem::path _presetDir;
    std::filesystem::path _defaultFile;
};

} // namespace p31
