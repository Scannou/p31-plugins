//
//  Form.h
//  p31-render
//
//  A settings page as data: rows bound to values through callbacks. The
//  settings panel turns a Form into controls; each plugin's editor describes
//  its own settings this way.
//

#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <utility>
#include <vector>

namespace p31::ui {

struct Row {
    enum class Kind {
        Section,     // heading
        Popup,       // one of `choices`
        Slider,      // min .. max in `step`s, value shown by `format`
        Checkbox,
        Number,      // integer text field, min .. max
        ColorPair,   // light and dark color
    };

    Kind kind = Kind::Section;
    std::string label;
    std::string tooltip;

    // Popup: title and value of each item.
    std::vector<std::pair<std::string, int>> choices;
    // Slider / Number.
    double min = 0.0, max = 1.0, step = 1.0;
    std::function<std::string(double)> format;

    // Popup / Slider / Checkbox / Number value.
    std::function<double()> get;
    std::function<void(double)> set;
    // Optional: a slider drag or checkbox click as one edit (parameters).
    std::function<void()> begin, end;

    // ColorPair, ARGB.
    std::function<uint32_t()> getLight, getDark;
    std::function<void(uint32_t)> setLight, setDark;

    // Optional: greyed out / not shown when these return false.
    std::function<bool()> enabled;
    std::function<bool()> visible;
};

using Form = std::vector<Row>;

// Builders for the common row kinds.
inline Row section(std::string label) {
    Row r;
    r.kind = Row::Kind::Section;
    r.label = std::move(label);
    return r;
}

template <class Get, class Set>
Row popup(std::string label, std::vector<std::pair<std::string, int>> choices, Get get, Set set,
          std::string tooltip = {}) {
    Row r;
    r.kind = Row::Kind::Popup;
    r.label = std::move(label);
    r.choices = std::move(choices);
    r.get = [get] { return static_cast<double>(get()); };
    r.set = [set](double v) { set(static_cast<int>(v)); };
    r.tooltip = std::move(tooltip);
    return r;
}

template <class Get, class Set>
Row slider(std::string label, double min, double max, Get get, Set set, std::function<std::string(double)> format,
           std::string tooltip = {}) {
    Row r;
    r.kind = Row::Kind::Slider;
    r.label = std::move(label);
    r.min = min;
    r.max = max;
    r.get = [get] { return static_cast<double>(get()); };
    r.set = [set](double v) { set(v); };
    r.format = std::move(format);
    r.tooltip = std::move(tooltip);
    return r;
}

template <class Get, class Set>
Row checkbox(std::string label, Get get, Set set, std::string tooltip = {}) {
    Row r;
    r.kind = Row::Kind::Checkbox;
    r.label = std::move(label);
    r.get = [get] { return get() ? 1.0 : 0.0; };
    r.set = [set](double v) { set(v >= 0.5); };
    r.tooltip = std::move(tooltip);
    return r;
}

template <class Get, class Set>
Row number(std::string label, int min, int max, Get get, Set set, std::string tooltip = {}) {
    Row r;
    r.kind = Row::Kind::Number;
    r.label = std::move(label);
    r.min = min;
    r.max = max;
    r.get = [get] { return static_cast<double>(get()); };
    r.set = [set](double v) { set(static_cast<int>(v)); };
    r.tooltip = std::move(tooltip);
    return r;
}

// `light` and `dark` are references into the settings value the form edits.
inline Row colorPair(std::string label, uint32_t& light, uint32_t& dark, std::function<void()> onEdit = {}) {
    Row r;
    r.kind = Row::Kind::ColorPair;
    r.label = std::move(label);
    uint32_t* l = &light;
    uint32_t* d = &dark;
    r.getLight = [l] { return *l; };
    r.getDark = [d] { return *d; };
    r.setLight = [l, onEdit](uint32_t v) { *l = v; if (onEdit) onEdit(); };
    r.setDark = [d, onEdit](uint32_t v) { *d = v; if (onEdit) onEdit(); };
    return r;
}

} // namespace p31::ui
