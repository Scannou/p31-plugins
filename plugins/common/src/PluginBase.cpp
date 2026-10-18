//
//  PluginBase.cpp
//  p31-plugin-common
//

#include "p31/plugin/PluginBase.h"

#include <p31/core/DenormalGuard.h>
#include <p31/core/PassThrough.h>

#include <array>
#include <cstdio>

namespace p31 {

namespace {
    constexpr clap_id kInputPortId = 0;
    constexpr clap_id kOutputPortId = 1;

    // State blob: magic, format version, payload size (all little endian),
    // then the payload. Phase 1 has no payload; later versions append fields
    // and must keep reading every older version.
    constexpr std::array<uint8_t, 4> kStateMagic{'P', '3', '1', 'S'};
    constexpr uint32_t kStateVersion = 1;
    constexpr uint32_t kMaxStatePayload = 1u << 20;

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
}

PluginBase* PluginBase::self(const clap_plugin* plugin) {
    return static_cast<PluginBase*>(plugin->plugin_data);
}

bool PluginBase::init() { return true; }

void PluginBase::destroy() { delete this; }

bool PluginBase::activate(double sampleRate, uint32_t minFrames, uint32_t maxFrames) {
    if (!onActivate(sampleRate, minFrames, maxFrames)) return false;
    _active = true;
    return true;
}

void PluginBase::deactivate() {
    _active = false;
    onDeactivate();
}

clap_process_status PluginBase::process(const clap_process* proc) {
    const DenormalGuard denormals;
    const uint32_t frames = proc->frames_count;
    if (proc->audio_outputs_count == 0) return CLAP_PROCESS_CONTINUE;

    const clap_audio_buffer* in = proc->audio_inputs_count > 0 ? &proc->audio_inputs[0] : nullptr;
    clap_audio_buffer& out = proc->audio_outputs[0];
    if (out.data32 == nullptr) return CLAP_PROCESS_ERROR;

    const float* const* inData = in ? in->data32 : nullptr;
    const uint32_t inCount = (in && inData) ? in->channel_count : 0;
    out.constant_mask = passThrough(inData, inCount, out.data32, out.channel_count, frames,
                                    in ? in->constant_mask : 0);

    if (inCount > 0) analyze(inData, inCount, frames);
    return CLAP_PROCESS_CONTINUE;
}

const void* PluginBase::getExtension(const char* id) {
    if (std::strcmp(id, CLAP_EXT_AUDIO_PORTS) == 0) return &s_audioPorts;
    if (std::strcmp(id, CLAP_EXT_CONFIGURABLE_AUDIO_PORTS) == 0) return &s_configurablePorts;
    if (std::strcmp(id, CLAP_EXT_LATENCY) == 0) return &s_latency;
    if (std::strcmp(id, CLAP_EXT_STATE) == 0) return &s_state;
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

#pragma mark - state

bool PluginBase::saveState(const clap_ostream* stream) {
    std::array<uint8_t, 12> header{};
    std::copy(kStateMagic.begin(), kStateMagic.end(), header.begin());
    putU32(header.data() + 4, kStateVersion);
    putU32(header.data() + 8, 0);  // payload size
    return writeAll(stream, header.data(), header.size());
}

bool PluginBase::loadState(const clap_istream* stream) {
    std::array<uint8_t, 12> header{};
    if (!readAll(stream, header.data(), header.size())) return false;
    if (!std::equal(kStateMagic.begin(), kStateMagic.end(), header.begin())) return false;
    const uint32_t version = getU32(header.data() + 4);
    const uint32_t payload = getU32(header.data() + 8);
    if (version == 0 || version > kStateVersion) return false;
    if (payload > kMaxStatePayload) return false;
    // Nothing to read yet in version 1.
    return payload == 0;
}

const clap_plugin_state PluginBase::s_state{
    [](const clap_plugin* p, const clap_ostream* s) { return self(p)->saveState(s); },
    [](const clap_plugin* p, const clap_istream* s) { return self(p)->loadState(s); },
};

} // namespace p31
