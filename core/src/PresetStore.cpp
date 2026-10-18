//
//  PresetStore.cpp
//  p31-core
//

#include "p31/core/PresetStore.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <system_error>

namespace fs = std::filesystem;

namespace p31 {

namespace {
    // Larger files are not presets.
    constexpr std::uintmax_t kMaxFileSize = 64 * 1024;

    std::optional<std::string> readFile(const fs::path& path) {
        std::error_code ec;
        const auto size = fs::file_size(path, ec);
        if (ec || size > kMaxFileSize) return std::nullopt;
        std::ifstream in(path, std::ios::binary);
        if (!in) return std::nullopt;
        std::ostringstream s;
        s << in.rdbuf();
        return s.str();
    }

    // Written to a temporary file first, so a failed write never leaves a
    // half-written preset behind.
    bool writeFile(const fs::path& path, std::string_view text) {
        std::error_code ec;
        fs::create_directories(path.parent_path(), ec);
        if (ec) return false;
        fs::path tmp = path;
        tmp += ".tmp";
        {
            std::ofstream out(tmp, std::ios::binary | std::ios::trunc);
            if (!out) return false;
            out.write(text.data(), static_cast<std::streamsize>(text.size()));
            if (!out) return false;
        }
        fs::rename(tmp, path, ec);
        if (ec) fs::remove(tmp, ec);
        return !ec;
    }

    std::string lower(std::string s) {
        std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        return s;
    }
}

PresetStore::PresetStore(fs::path presetDir, fs::path defaultFile)
    : _presetDir(std::move(presetDir)), _defaultFile(std::move(defaultFile)) {}

PresetStore PresetStore::forPlugin(std::string_view pluginName) {
    const char* home = std::getenv("HOME");
    const fs::path base = home ? fs::path(home) / "Library" : fs::path();
    const std::string plugin = cleanName(pluginName);
    return PresetStore(base / "Audio" / "Presets" / "P31" / plugin,
                       base / "Application Support" / "P31" / plugin / (std::string("default") + kExtension));
}

std::string PresetStore::cleanName(std::string_view name) {
    std::string s;
    for (char c : name) {
        const auto u = static_cast<unsigned char>(c);
        if (c == '/' || c == ':' || c == '\\' || u < 0x20) s.push_back('-');
        else s.push_back(c);
    }
    const auto notSpace = [](unsigned char c) { return !std::isspace(c); };
    s.erase(s.begin(), std::find_if(s.begin(), s.end(), notSpace));
    s.erase(std::find_if(s.rbegin(), s.rend(), notSpace).base(), s.end());
    while (!s.empty() && s.front() == '.') s.front() = '-';   // no hidden files
    if (s.size() > 100) s.resize(100);
    return s;
}

fs::path PresetStore::fileFor(std::string_view name) const {
    return _presetDir / (cleanName(name) + kExtension);
}

std::vector<std::string> PresetStore::names() const {
    std::vector<std::string> out;
    std::error_code ec;
    if (_presetDir.empty()) return out;
    for (fs::directory_iterator it(_presetDir, ec), end; !ec && it != end; it.increment(ec)) {
        const fs::path& p = it->path();
        if (p.extension() != kExtension || !it->is_regular_file(ec)) continue;
        out.push_back(p.stem().string());
    }
    std::sort(out.begin(), out.end(), [](const std::string& a, const std::string& b) { return lower(a) < lower(b); });
    return out;
}

std::optional<std::string> PresetStore::load(std::string_view name) const {
    if (cleanName(name).empty()) return std::nullopt;
    return readFile(fileFor(name));
}

bool PresetStore::save(std::string_view name, std::string_view text) const {
    if (cleanName(name).empty() || _presetDir.empty()) return false;
    return writeFile(fileFor(name), text);
}

bool PresetStore::remove(std::string_view name) const {
    if (cleanName(name).empty()) return false;
    std::error_code ec;
    return fs::remove(fileFor(name), ec);
}

bool PresetStore::exists(std::string_view name) const {
    if (cleanName(name).empty()) return false;
    std::error_code ec;
    return fs::is_regular_file(fileFor(name), ec);
}

std::optional<std::string> PresetStore::loadDefault() const { return readFile(_defaultFile); }

bool PresetStore::saveDefault(std::string_view text) const {
    return !_defaultFile.empty() && writeFile(_defaultFile, text);
}

bool PresetStore::clearDefault() const {
    std::error_code ec;
    fs::remove(_defaultFile, ec);
    return !ec;
}

bool PresetStore::hasDefault() const {
    std::error_code ec;
    return fs::is_regular_file(_defaultFile, ec);
}

} // namespace p31
