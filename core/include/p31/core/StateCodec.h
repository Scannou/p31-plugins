//
//  StateCodec.h
//  p31-core
//
//  Settings as text, one "key=value" line per field (integers only). Used
//  for the plugin state and for preset files. Readers skip keys they do not
//  know and keep defaults for keys that are missing, so state written by an
//  older or newer version always loads.
//

#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <unordered_map>

namespace p31 {

class StateWriter {
public:
    void put(std::string_view key, int64_t value);
    const std::string& text() const { return _text; }

    // Field visitor (see visitFields in the settings headers).
    void operator()(std::string_view key, int& value) { put(key, value); }
    void operator()(std::string_view key, bool& value) { put(key, value ? 1 : 0); }
    void operator()(std::string_view key, uint32_t& value) { put(key, value); }

private:
    std::string _text;
};

class StateReader {
public:
    // Lines that are not "key=integer" are ignored.
    explicit StateReader(std::string_view text);

    bool has(std::string_view key) const { return _values.count(std::string(key)) != 0; }
    // Leaves `out` alone when the key is missing.
    bool get(std::string_view key, int64_t& out) const;

    // Field visitor: values out of the type's range count as missing.
    void operator()(std::string_view key, int& value) const;
    void operator()(std::string_view key, bool& value) const;
    void operator()(std::string_view key, uint32_t& value) const;

private:
    std::unordered_map<std::string, int64_t> _values;
};

// Settings struct `S` with a visitFields(S&, visitor) overload, to and from text.
template <class S>
std::string encodeSettings(const S& settings) {
    S copy = settings;
    StateWriter w;
    visitFields(copy, w);
    return w.text();
}

// Fields missing from `text` keep their value in `settings`.
template <class S>
void decodeSettings(std::string_view text, S& settings) {
    const StateReader r(text);
    visitFields(settings, r);
}

} // namespace p31
