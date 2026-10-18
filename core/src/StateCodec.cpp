//
//  StateCodec.cpp
//  p31-core
//

#include "p31/core/StateCodec.h"

#include <charconv>
#include <limits>

namespace p31 {

namespace {
    bool validKey(std::string_view key) {
        if (key.empty()) return false;
        for (char c : key) {
            const bool ok = (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_' || c == '.';
            if (!ok) return false;
        }
        return true;
    }

    template <class T>
    void readInto(const StateReader& r, std::string_view key, T& value) {
        int64_t v = 0;
        if (!r.get(key, v)) return;
        if (v < static_cast<int64_t>(std::numeric_limits<T>::min()) ||
            v > static_cast<int64_t>(std::numeric_limits<T>::max())) {
            return;
        }
        value = static_cast<T>(v);
    }
}

void StateWriter::put(std::string_view key, int64_t value) {
    if (!validKey(key)) return;
    _text.append(key);
    _text.push_back('=');
    _text.append(std::to_string(value));
    _text.push_back('\n');
}

StateReader::StateReader(std::string_view text) {
    while (!text.empty()) {
        const size_t end = text.find('\n');
        std::string_view line = text.substr(0, end);
        text = end == std::string_view::npos ? std::string_view{} : text.substr(end + 1);
        if (!line.empty() && line.back() == '\r') line.remove_suffix(1);

        const size_t eq = line.find('=');
        if (eq == std::string_view::npos) continue;
        const std::string_view key = line.substr(0, eq), value = line.substr(eq + 1);
        if (!validKey(key) || value.empty()) continue;
        int64_t v = 0;
        const auto [ptr, ec] = std::from_chars(value.data(), value.data() + value.size(), v);
        if (ec != std::errc{} || ptr != value.data() + value.size()) continue;
        _values[std::string(key)] = v;
    }
}

bool StateReader::get(std::string_view key, int64_t& out) const {
    const auto it = _values.find(std::string(key));
    if (it == _values.end()) return false;
    out = it->second;
    return true;
}

void StateReader::operator()(std::string_view key, int& value) const { readInto(*this, key, value); }
void StateReader::operator()(std::string_view key, uint32_t& value) const { readInto(*this, key, value); }
void StateReader::operator()(std::string_view key, bool& value) const {
    int64_t v = 0;
    if (get(key, v) && (v == 0 || v == 1)) value = v == 1;
}

} // namespace p31
