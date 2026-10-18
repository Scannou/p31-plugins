//
//  PluginBase.cpp
//  p31-plugin-common
//

#include "p31/plugin/PluginBase.h"

#include <p31/core/DenormalGuard.h>
#include <p31/core/PassThrough.h>
#include <p31/core/PresetStore.h>
#include <p31/core/StateCodec.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <strings.h>
#include <vector>

namespace p31 {

namespace {
    constexpr clap_id kInputPortId = 0;
    constexpr clap_id kOutputPortId = 1;

    // host->name of clap-wrapper's standalone app.
    constexpr const char* kStandaloneHostName = "CLAP-Wrapper-As-Standalone";

    // State blob: magic, format version, payload size (all little endian),
    // then the payload. Version 1 had no payload. Version 2's payload is
    // StateCodec text: editor size, parameter values and every setting;
    // fields are added as new keys without a version change.
    constexpr std::array<uint8_t, 4> kStateMagic{'P', '3', '1', 'S'};
    constexpr uint32_t kStateVersion = 2;
    constexpr uint32_t kMaxStatePayload = 1u << 20;

    constexpr const char* kKeyEditorWidth = "editor_width";
    constexpr const char* kKeyEditorHeight = "editor_height";
    constexpr const char* kKeyFreeze = "param.freeze";
    constexpr const char* kKeyGainCentiDb = "param.gain_cdb";

    // Editor gestures waiting for process() / flush().
    constexpr uint32_t kEditBegin = 1;
    constexpr uint32_t kEditValue = 2;
    constexpr uint32_t kEditEnd = 4;

    struct ParamDef {
        const char* name;
        double min, max, defaultValue;
        clap_param_info_flags flags;
    };
    constexpr ParamDef kParams[] = {
        {"Freeze", 0.0, 1.0, 0.0, CLAP_PARAM_IS_AUTOMATABLE | CLAP_PARAM_IS_STEPPED},
        {"Gain", PluginBase::kGainMinDb, PluginBase::kGainMaxDb, 0.0, CLAP_PARAM_IS_AUTOMATABLE},
    };
    static_assert(std::size(kParams) == PluginBase::kParamCount);

    void putU32(uint8_t* p, uint32_t v) {
        for (int i = 0; i < 4; ++i) p[i] = static_cast<uint8_t>(v >> (8 * i));
    }
    uint32_t getU32(const uint8_t* p) {
        uint32_t v = 0;
        for (int i = 0; i < 4; ++i) v |= uint32_t{p[i]} << (8 * i);
        return v;
    }

    // Streams may move fewer bytes than asked; loop until done.
    bool writeAll(const clap_ostream* s, const uint8_t* data, size_t size) {
        while (size > 0) {
            const int64_t n = s->write(s, data, size);
            if (n <= 0) return false;
            data += n;
            size -= static_cast<size_t>(n);
        }
        return true;
    }
    bool readAll(const clap_istream* s, uint8_t* data, size_t size) {
        while (size > 0) {
            const int64_t n = s->read(s, data, size);
            if (n <= 0) return false;
            data += n;
            size -= static_cast<size_t>(n);
        }
        return true;
    }
} // namespace

PluginBase::PluginBase(const clap_plugin_descriptor* desc, const clap_host* host) : _host(host) {
    _plugin.desc = desc;
    _plugin.plugin_data = this;
    _plugin.init = [](const clap_plugin* p) { return self(p)->init(); };
    _plugin.destroy = [](const clap_plugin* p) { self(p)->destroy(); };
    _plugin.activate = [](const clap_plugin* p, double sr, uint32_t minF, uint32_t maxF) {
        return self(p)->activate(sr, minF, maxF);
    };
    _plugin.deactivate = [](const clap_plugin* p) { self(p)->deactivate(); };
    _plugin.start_processing = [](const clap_plugin*) { return true; };
    _plugin.stop_processing = [](const clap_plugin*) {};
    _plugin.reset = [](const clap_plugin*) {};
    _plugin.process = [](const clap_plugin* p, const clap_process* proc) { return self(p)->process(proc); };
    _plugin.get_extension = [](const clap_plugin* p, const char* id) { return self(p)->getExtension(id); };
    _plugin.on_main_thread = [](const clap_plugin*) {};
    for (uint32_t i = 0; i < kParamCount; ++i) {
        _params[i].store(kParams[i].defaultValue);
        _pendingEdits[i].store(0);
    }
}

PluginBase::~PluginBase() = default;

PluginBase* PluginBase::self(const clap_plugin* plugin) {
    return static_cast<PluginBase*>(plugin->plugin_data);
}

bool PluginBase::init() {
    _muteOutput = _host->name != nullptr && std::strcmp(_host->name, kStandaloneHostName) == 0;
    _hostParams = static_cast<const clap_host_params*>(_host->get_extension(_host, CLAP_EXT_PARAMS));
    _hostState = static_cast<const clap_host_state*>(_host->get_extension(_host, CLAP_EXT_STATE));
    _hostGui = static_cast<const clap_host_gui*>(_host->get_extension(_host, CLAP_EXT_GUI));
    // A new instance starts from the user's saved default, if any. A
    // project's own state replaces it right after.
    decodeSettings(PresetStore::forPlugin(name()).loadDefault().value_or(std::string()));
    return true;
}

void PluginBase::destroy() { delete this; }

bool PluginBase::activate(double sampleRate, uint32_t minFrames, uint32_t maxFrames) {
    if (!onActivate(sampleRate, minFrames, maxFrames)) return false;
    _sampleRate.store(sampleRate, std::memory_order_relaxed);
    _active = true;
    return true;
}

void PluginBase::deactivate() {
    _active = false;
    onDeactivate();
}

clap_process_status PluginBase::process(const clap_process* proc) {
    const DenormalGuard denormals;
    if (proc->in_events) handleInputEvents(proc->in_events);
    if (proc->out_events) emitEditorEvents(proc->out_events);
    const uint32_t frames = proc->frames_count;
    if (proc->audio_outputs_count == 0) return CLAP_PROCESS_CONTINUE;

    const clap_audio_buffer* in = proc->audio_inputs_count > 0 ? &proc->audio_inputs[0] : nullptr;
    clap_audio_buffer& out = proc->audio_outputs[0];
    if (out.data32 == nullptr) return CLAP_PROCESS_ERROR;

    const float* const* inData = in ? in->data32 : nullptr;
    const uint32_t inCount = (in && inData) ? in->channel_count : 0;
    // Before the pass-through: with in-place buffers a muted output would
    // otherwise silence the input too.
    if (inCount > 0) analyze(inData, inCount, frames);

    if (_muteOutput) {
        out.constant_mask = passThrough(nullptr, 0, out.data32, out.channel_count, frames, 0);
    } else {
        out.constant_mask = passThrough(inData, inCount, out.data32, out.channel_count, frames,
                                        in ? in->constant_mask : 0);
    }
    return CLAP_PROCESS_CONTINUE;
}

const void* PluginBase::getExtension(const char* id) {
    if (std::strcmp(id, CLAP_EXT_AUDIO_PORTS) == 0) return &s_audioPorts;
    if (std::strcmp(id, CLAP_EXT_CONFIGURABLE_AUDIO_PORTS) == 0) return &s_configurablePorts;
    if (std::strcmp(id, CLAP_EXT_LATENCY) == 0) return &s_latency;
    if (std::strcmp(id, CLAP_EXT_PARAMS) == 0) return &s_params;
    if (std::strcmp(id, CLAP_EXT_STATE) == 0) return &s_state;
    if (std::strcmp(id, CLAP_EXT_GUI) == 0 && hasEditor()) return &s_gui;
    return extension(id);
}

#pragma mark - audio ports

uint32_t PluginBase::portCount(bool) const { return 1; }

bool PluginBase::portInfo(uint32_t index, bool isInput, clap_audio_port_info* info) const {
    if (index != 0) return false;
    const uint32_t channels = isInput ? _inChannels : _outChannels;
    info->id = isInput ? kInputPortId : kOutputPortId;
    std::snprintf(info->name, sizeof(info->name), "%s", isInput ? "Main In" : "Main Out");
    info->flags = CLAP_AUDIO_PORT_IS_MAIN;
    info->channel_count = channels;
    info->port_type = channels == 1 ? CLAP_PORT_MONO : CLAP_PORT_STEREO;
    info->in_place_pair = _inChannels == _outChannels ? (isInput ? kOutputPortId : kInputPortId) : CLAP_INVALID_ID;
    return true;
}

// Accepted layouts: mono -> mono, stereo -> stereo, mono -> stereo (the input
// is duplicated). Stereo -> mono is refused: it would change the audio.
bool PluginBase::canApplyConfiguration(const clap_audio_port_configuration_request* requests, uint32_t count,
                                       uint32_t& inChannels, uint32_t& outChannels) const {
    if (_active) return false;
    inChannels = _inChannels;
    outChannels = _outChannels;
    for (uint32_t i = 0; i < count; ++i) {
        const auto& r = requests[i];
        if (r.port_index != 0) return false;
        if (r.channel_count != 1 && r.channel_count != 2) return false;
        (r.is_input ? inChannels : outChannels) = r.channel_count;
    }
    return inChannels <= outChannels;
}

const clap_plugin_audio_ports PluginBase::s_audioPorts{
    [](const clap_plugin* p, bool isInput) { return self(p)->portCount(isInput); },
    [](const clap_plugin* p, uint32_t index, bool isInput, clap_audio_port_info* info) {
        return self(p)->portInfo(index, isInput, info);
    },
};

const clap_plugin_configurable_audio_ports PluginBase::s_configurablePorts{
    [](const clap_plugin* p, const clap_audio_port_configuration_request* req, uint32_t count) {
        uint32_t in = 0, out = 0;
        return self(p)->canApplyConfiguration(req, count, in, out);
    },
    [](const clap_plugin* p, const clap_audio_port_configuration_request* req, uint32_t count) {
        PluginBase* s = self(p);
        uint32_t in = 0, out = 0;
        if (!s->canApplyConfiguration(req, count, in, out)) return false;
        s->_inChannels = in;
        s->_outChannels = out;
        return true;
    },
};

const clap_plugin_latency PluginBase::s_latency{
    [](const clap_plugin*) -> uint32_t { return 0; },
};

#pragma mark - params

double PluginBase::paramValue(clap_id id) const {
    return id < kParamCount ? _params[id].load(std::memory_order_relaxed) : 0.0;
}

float PluginBase::analysisGain() const {
    return static_cast<float>(std::pow(10.0, paramValue(kParamGain) / 20.0));
}

bool PluginBase::paramInfo(uint32_t index, clap_param_info* info) const {
    if (index >= kParamCount) return false;
    const ParamDef& d = kParams[index];
    *info = clap_param_info{};
    info->id = index;
    info->flags = d.flags;
    info->cookie = nullptr;
    std::snprintf(info->name, sizeof(info->name), "%s", d.name);
    info->module[0] = '\0';
    info->min_value = d.min;
    info->max_value = d.max;
    info->default_value = d.defaultValue;
    return true;
}

bool PluginBase::paramValueToText(clap_id id, double value, char* out, uint32_t size) const {
    if (id >= kParamCount || size == 0) return false;
    if (id == kParamFreeze) std::snprintf(out, size, "%s", value >= 0.5 ? "On" : "Off");
    else std::snprintf(out, size, "%+.1f dB", value);
    return true;
}

bool PluginBase::paramTextToValue(clap_id id, const char* text, double* value) const {
    if (id >= kParamCount || text == nullptr) return false;
    if (id == kParamFreeze) {
        if (strcasecmp(text, "on") == 0) { *value = 1.0; return true; }
        if (strcasecmp(text, "off") == 0) { *value = 0.0; return true; }
    }
    char* end = nullptr;
    const double v = std::strtod(text, &end);
    if (end == text) return false;
    *value = std::clamp(v, kParams[id].min, kParams[id].max);
    if (id == kParamFreeze) *value = *value >= 0.5 ? 1.0 : 0.0;
    return true;
}

// Audio thread or [main-thread] flush: no allocation.
void PluginBase::handleInputEvents(const clap_input_events* in) {
    const uint32_t count = in->size(in);
    for (uint32_t i = 0; i < count; ++i) {
        const clap_event_header* h = in->get(in, i);
        if (h->space_id != CLAP_CORE_EVENT_SPACE_ID || h->type != CLAP_EVENT_PARAM_VALUE) continue;
        const auto* ev = reinterpret_cast<const clap_event_param_value*>(h);
        if (ev->param_id >= kParamCount) continue;
        const ParamDef& d = kParams[ev->param_id];
        _params[ev->param_id].store(std::clamp(ev->value, d.min, d.max), std::memory_order_relaxed);
    }
}

void PluginBase::emitEditorEvents(const clap_output_events* out) {
    for (clap_id id = 0; id < kParamCount; ++id) {
        const uint32_t edits = _pendingEdits[id].exchange(0, std::memory_order_acq_rel);
        if (edits == 0) continue;
        const auto gesture = [&](uint16_t type) {
            clap_event_param_gesture ev{};
            ev.header = {sizeof(ev), 0, CLAP_CORE_EVENT_SPACE_ID, type, 0};
            ev.param_id = id;
            out->try_push(out, &ev.header);
        };
        if (edits & kEditBegin) gesture(CLAP_EVENT_PARAM_GESTURE_BEGIN);
        if (edits & kEditValue) {
            clap_event_param_value ev{};
            ev.header = {sizeof(ev), 0, CLAP_CORE_EVENT_SPACE_ID, CLAP_EVENT_PARAM_VALUE, 0};
            ev.param_id = id;
            ev.cookie = nullptr;
            ev.note_id = -1;
            ev.port_index = -1;
            ev.channel = -1;
            ev.key = -1;
            ev.value = paramValue(id);
            out->try_push(out, &ev.header);
        }
        if (edits & kEditEnd) gesture(CLAP_EVENT_PARAM_GESTURE_END);
    }
}

void PluginBase::beginParamEdit(clap_id id) {
    if (id >= kParamCount) return;
    _pendingEdits[id].fetch_or(kEditBegin, std::memory_order_acq_rel);
    if (_hostParams) _hostParams->request_flush(_host);
}

void PluginBase::setParamFromEditor(clap_id id, double value) {
    if (id >= kParamCount) return;
    const ParamDef& d = kParams[id];
    _params[id].store(std::clamp(value, d.min, d.max), std::memory_order_relaxed);
    _pendingEdits[id].fetch_or(kEditValue, std::memory_order_acq_rel);
    if (_hostParams) _hostParams->request_flush(_host);
}

void PluginBase::endParamEdit(clap_id id) {
    if (id >= kParamCount) return;
    _pendingEdits[id].fetch_or(kEditEnd, std::memory_order_acq_rel);
    if (_hostParams) _hostParams->request_flush(_host);
}

const clap_plugin_params PluginBase::s_params{
    [](const clap_plugin*) -> uint32_t { return kParamCount; },
    [](const clap_plugin* p, uint32_t index, clap_param_info* info) { return self(p)->paramInfo(index, info); },
    [](const clap_plugin* p, clap_id id, double* value) {
        if (id >= kParamCount) return false;
        *value = self(p)->paramValue(id);
        return true;
    },
    [](const clap_plugin* p, clap_id id, double value, char* out, uint32_t size) {
        return self(p)->paramValueToText(id, value, out, size);
    },
    [](const clap_plugin* p, clap_id id, const char* text, double* value) {
        return self(p)->paramTextToValue(id, text, value);
    },
    [](const clap_plugin* p, const clap_input_events* in, const clap_output_events* out) {
        PluginBase* s = self(p);
        if (in) s->handleInputEvents(in);
        if (out) s->emitEditorEvents(out);
    },
};

#pragma mark - settings

void PluginBase::settingsEdited() {
    if (_hostState) _hostState->mark_dirty(_host);
}

#pragma mark - state

bool PluginBase::saveState(const clap_ostream* stream) {
    StateWriter w;
    if (_editorWidth > 0 && _editorHeight > 0) {
        w.put(kKeyEditorWidth, _editorWidth);
        w.put(kKeyEditorHeight, _editorHeight);
    }
    w.put(kKeyFreeze, frozen() ? 1 : 0);
    w.put(kKeyGainCentiDb, std::lround(paramValue(kParamGain) * 100.0));
    const std::string payload = w.text() + encodeSettings();
    if (payload.size() > kMaxStatePayload) return false;

    std::array<uint8_t, 12> header{};
    std::copy(kStateMagic.begin(), kStateMagic.end(), header.begin());
    putU32(header.data() + 4, kStateVersion);
    putU32(header.data() + 8, static_cast<uint32_t>(payload.size()));
    return writeAll(stream, header.data(), header.size()) &&
           writeAll(stream, reinterpret_cast<const uint8_t*>(payload.data()), payload.size());
}

bool PluginBase::loadState(const clap_istream* stream) {
    std::array<uint8_t, 12> header{};
    if (!readAll(stream, header.data(), header.size())) return false;
    if (!std::equal(kStateMagic.begin(), kStateMagic.end(), header.begin())) return false;
    const uint32_t version = getU32(header.data() + 4);
    const uint32_t payload = getU32(header.data() + 8);
    if (version == 0 || version > kStateVersion) return false;
    if (payload > kMaxStatePayload) return false;
    if (version == 1 && payload != 0) return false;

    std::vector<uint8_t> bytes(payload);
    if (payload > 0 && !readAll(stream, bytes.data(), bytes.size())) return false;
    const std::string_view text(reinterpret_cast<const char*>(bytes.data()), bytes.size());
    const StateReader r(text);

    // Parameters: missing ones return to their defaults.
    int64_t v = 0;
    _params[kParamFreeze].store(r.get(kKeyFreeze, v) && v == 1 ? 1.0 : 0.0);
    const double gain = r.get(kKeyGainCentiDb, v) ? static_cast<double>(v) / 100.0 : 0.0;
    _params[kParamGain].store(std::clamp(gain, kGainMinDb, kGainMaxDb));
    if (_hostParams) _hostParams->rescan(_host, CLAP_PARAM_RESCAN_VALUES);

    int64_t w = 0, h = 0;
    if (r.get(kKeyEditorWidth, w) && r.get(kKeyEditorHeight, h) && w > 0 && h > 0 && w < 100000 && h < 100000) {
        uint32_t width = static_cast<uint32_t>(w), height = static_cast<uint32_t>(h);
        guiClampSize(width, height);
        const bool resize = _editor && (width != _editorWidth || height != _editorHeight);
        _editorWidth = width;
        _editorHeight = height;
        if (resize && _hostGui && _hostGui->request_resize(_host, width, height)) _editor->setSize(width, height);
    }

    decodeSettings(text);
    if (_editor) _editor->settingsChanged();
    return true;
}

const clap_plugin_state PluginBase::s_state{
    [](const clap_plugin* p, const clap_ostream* s) { return self(p)->saveState(s); },
    [](const clap_plugin* p, const clap_istream* s) { return self(p)->loadState(s); },
};

#pragma mark - gui

bool PluginBase::guiCreate(const char* api, bool isFloating) {
    if (isFloating || std::strcmp(api, CLAP_WINDOW_API_COCOA) != 0 || _editor) return false;
    if (_editorWidth == 0 || _editorHeight == 0) {
        const EditorSizeLimits limits = editorSizeLimits();
        _editorWidth = limits.defaultWidth;
        _editorHeight = limits.defaultHeight;
    }
    _editor = createEditor(_editorWidth, _editorHeight);
    return _editor != nullptr;
}

void PluginBase::guiDestroy() { _editor.reset(); }

void PluginBase::guiClampSize(uint32_t& width, uint32_t& height) const {
    const EditorSizeLimits limits = editorSizeLimits();
    width = std::clamp(width, limits.minWidth, limits.maxWidth);
    height = std::clamp(height, limits.minHeight, limits.maxHeight);
}

bool PluginBase::guiSetSize(uint32_t width, uint32_t height) {
    guiClampSize(width, height);
    _editorWidth = width;
    _editorHeight = height;
    if (_editor) _editor->setSize(width, height);
    return true;
}

// macOS hosts size editors in points and the views follow the backing scale
// themselves, so set_scale is declined (as the CLAP spec asks on macOS).
const clap_plugin_gui PluginBase::s_gui{
    [](const clap_plugin*, const char* api, bool isFloating) {
        return !isFloating && std::strcmp(api, CLAP_WINDOW_API_COCOA) == 0;
    },
    [](const clap_plugin*, const char** api, bool* isFloating) {
        *api = CLAP_WINDOW_API_COCOA;
        *isFloating = false;
        return true;
    },
    [](const clap_plugin* p, const char* api, bool isFloating) { return self(p)->guiCreate(api, isFloating); },
    [](const clap_plugin* p) { self(p)->guiDestroy(); },
    [](const clap_plugin*, double) { return false; },
    [](const clap_plugin* p, uint32_t* width, uint32_t* height) {
        const PluginBase* s = self(p);
        if (s->_editorWidth == 0) {
            const EditorSizeLimits limits = s->editorSizeLimits();
            *width = limits.defaultWidth;
            *height = limits.defaultHeight;
        } else {
            *width = s->_editorWidth;
            *height = s->_editorHeight;
        }
        return true;
    },
    [](const clap_plugin*) { return true; },
    [](const clap_plugin*, clap_gui_resize_hints* hints) {
        hints->can_resize_horizontally = true;
        hints->can_resize_vertically = true;
        hints->preserve_aspect_ratio = false;
        hints->aspect_ratio_width = 1;
        hints->aspect_ratio_height = 1;
        return true;
    },
    [](const clap_plugin* p, uint32_t* width, uint32_t* height) {
        self(p)->guiClampSize(*width, *height);
        return true;
    },
    [](const clap_plugin* p, uint32_t width, uint32_t height) { return self(p)->guiSetSize(width, height); },
    [](const clap_plugin* p, const clap_window* window) {
        PluginBase* s = self(p);
        if (!s->_editor || std::strcmp(window->api, CLAP_WINDOW_API_COCOA) != 0) return false;
        return s->_editor->attach(window->cocoa);
    },
    [](const clap_plugin*, const clap_window*) { return false; },
    [](const clap_plugin*, const char*) {},
    [](const clap_plugin* p) {
        PluginBase* s = self(p);
        if (s->_editor) s->_editor->setVisible(true);
        return s->_editor != nullptr;
    },
    [](const clap_plugin* p) {
        PluginBase* s = self(p);
        if (s->_editor) s->_editor->setVisible(false);
        return s->_editor != nullptr;
    },
};

} // namespace p31
