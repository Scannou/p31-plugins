//
//  ScopeEditor.mm
//  p31-scope (ported from foo_jl_vectorscope_mac VectorscopeController.mm / VectorscopePreferences.mm)
//
//  Owns the scope view and, on every display frame, pulls the audio played
//  since the previous frame from the plugin's ring buffer, analyses it and
//  hands points and correlation readings to the view. Also describes the
//  settings page and the factory presets.
//

#include "ScopeEditor.h"

#include "ScopePlugin.h"

#import <p31/render/EditorRootView.h>
#import <p31/render/ScopeView.h>

#include <p31/core/AudioRing.h>
#include <p31/plugin/EditorSupport.h>
#include <p31/scope/StereoAnalysis.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <vector>

namespace {

using namespace p31::scope;
using p31::stereo::StereoAnalyzer;
using p31::ui::Form;

// Longest stretch analysed in one frame. After a stall (window hidden, UI
// blocked) the scope resumes at "now" instead of replaying the gap.
constexpr double kMaxWindowSeconds = 0.05;
// Covers kMaxWindowSeconds up to 384 kHz.
constexpr size_t kMaxFramesPerTick = 32768;
// Blocks quieter than this (-80 dBFS) leave the trace alone. A stopped DAW
// keeps sending digital silence, which would otherwise burn a dot into the
// centre instead of letting the afterglow fade as it does in foobar2000.
constexpr float kTraceGate = 1e-4f;

std::string fmt(const char* format, double v) {
    char s[48];
    std::snprintf(s, sizeof(s), format, v);
    return s;
}

std::vector<std::pair<std::string, ScopeSettings>> factoryPresets() {
    std::vector<std::pair<std::string, ScopeSettings>> out;
    out.emplace_back("Default", ScopeSettings{});

    ScopeSettings lissajous;
    lissajous.style = ScopeStyleLissajous;
    out.emplace_back("Lissajous", lissajous);

    // The default splits trace and meter into bands; this is the plain look.
    ScopeSettings single;
    single.traceColorMode = TraceColorSingle;
    single.correlationMode = CorrelationSingle;
    out.emplace_back("Single colour", single);

    ScopeSettings dots;
    dots.drawMode = DrawModeDots;
    dots.persistenceMs = 150;
    out.emplace_back("Dots", dots);

    ScopeSettings amber;
    applyTheme(amber, 12);   // P3 amber
    amber.persistenceMs = 800;
    amber.brightness = 45;
    out.emplace_back("Long afterglow (amber)", amber);
    return out;
}

class ScopeEditor final : public p31::Editor {
public:
    ScopeEditor(ScopePlugin& plugin, uint32_t width, uint32_t height)
        : _plugin(plugin), _reader(plugin.ring()), _settings(plugin.settings()) {
        _lr.resize(kMaxFramesPerTick * 2);
        _xy.resize(kMaxFramesPerTick * 2);
        _params.changed(plugin);

        const NSRect frame = NSMakeRect(0, 0, width, height);
        _view = [[P31ScopeView alloc] initWithFrame:frame];
        _root = [[P31EditorRootView alloc] initWithFrame:frame content:_view title:@"Scope"];
        [_root setVersion:@P31_VERSION description:@"Vectorscope with tri-band phase correlation" url:@P31_PROJECT_URL];

        ScopeEditor* editor = this;
        _root.makeForm = ^Form { return editor->buildForm(); };
        _root.settingsPanel.onChange = ^{ editor->edited(); };
        _root.presetMenu = ^NSMenu* { return editor->presetMenu(); };
        _root.isFrozen = ^BOOL { return editor->_plugin.frozen(); };
        _root.toggleFreeze = ^{ p31::editor::toggleFreeze(editor->_plugin); };
        _root.requestResize = ^BOOL(NSSize size) {
            return editor->_plugin.requestEditorResize(static_cast<uint32_t>(std::max(0.0, size.width)),
                                                       static_cast<uint32_t>(std::max(0.0, size.height)));
        };
        _view.onFrame = ^(double timestamp) { editor->tick(timestamp); };

        apply();
    }

    ~ScopeEditor() override {
        [_view stopAnimating];
        [_root removeFromSuperview];
        _root = nil;
        _view = nil;
    }

    bool attach(void* parent) override {
        NSView* parentView = (__bridge NSView*)parent;
        if (parentView == nil) return false;
        [parentView addSubview:_root];
        _reader.skipToNow();
        return true;
    }

    void setSize(uint32_t width, uint32_t height) override {
        [_root setFrameSize:NSMakeSize(width, height)];
    }

    void setVisible(bool visible) override {
        _root.hidden = !visible;
        if (visible) _reader.skipToNow();
    }

    void settingsChanged() override {
        _settings = _plugin.settings();
        apply();
        [_root reloadSettings];
    }

private:
    // Push _settings to the view and the analyzer.
    void apply() {
        const ScopeSettings s = _settings.sanitized();
        // The brand takes the theme's main color, as the logo README asks.
        [_root setBrandLightColor:_settings.light.trace darkColor:_settings.dark.trace];
        [_root setAppearanceMode:s.appearance];
        [_view applySettings:s];

        StereoAnalyzer::Settings a;
        a.integrationSeconds = s.integrationMs / 1000.0;
        // The band meters only run while their bars are on screen.
        a.multiband = s.showCorrelation && s.correlationMode == CorrelationBands;
        a.lissajous = s.style == ScopeStyleLissajous;
        a.bandTrace = s.traceColorMode == TraceColorBands;
        a.lowHz = s.lowHz;
        a.highHz = s.highHz;
        a.autoGain = s.autoGain;
        a.manualGain = static_cast<float>(std::pow(10.0, s.gainDb / 20.0));
        _analyzer.configure(a);
    }

    // A control on the settings page changed _settings.
    void edited() {
        _plugin.setSettings(_settings);
        _settings = _plugin.settings();
        apply();
    }

    void applyPreset(const ScopeSettings& preset) {
        ScopeSettings s = preset;
        s.appearance = _settings.appearance;
        _plugin.setSettings(s);
        settingsChanged();
    }

    NSMenu* presetMenu() {
        ScopeEditor* editor = this;
        return p31::ui::makePresetMenu(
            p31::editor::presetSource<ScopeSettings>(_plugin, factoryPresets(),
                                                     [editor](const ScopeSettings& s) { editor->applyPreset(s); }),
            _root);
    }

    Form buildForm();

    void tick(double now) {
        if (_params.changed(_plugin)) [_root reloadSettings];
        [_root refreshFreeze];
        // Frozen: the trace and meter hold; on release they continue from "now".
        if (_plugin.frozen()) {
            _wasFrozen = true;
            return;
        }
        if (_wasFrozen) {
            _wasFrozen = false;
            _reader.skipToNow();
            _lastTick = now;
        }

        // Real elapsed time drives the afterglow and meter decay, so a late
        // frame does not slow the fade. Capped so the first frame after the
        // view was hidden does not wipe everything at once.
        const double dt = _lastTick > 0 ? std::clamp(now - _lastTick, 0.0, 0.1) : 1.0 / 60.0;
        _lastTick = now;

        const double sampleRate = _plugin.currentSampleRate();
        size_t frames = 0;
        if (sampleRate > 0) {
            const auto maxFrames = std::min(kMaxFramesPerTick,
                                            static_cast<size_t>(std::ceil(sampleRate * kMaxWindowSeconds)));
            frames = _reader.readNew(_lr.data(), maxFrames).frames;
            const float gain = _plugin.analysisGain();
            if (gain != 1.0f) {
                for (size_t i = 0; i < frames * 2; ++i) _lr[i] *= gain;
            }
        }
        if (frames > 0) {
            _analyzer.process(_lr.data(), frames, sampleRate, _xy.data());
            if (p31::stereo::blockPeak(_lr.data(), frames) < kTraceGate) frames = 0;
        } else {
            _analyzer.idle(dt);
        }

        float bands[StereoAnalyzer::BandCount];
        BOOL bandsSilent[StereoAnalyzer::BandCount];
        const float* bandXY[StereoAnalyzer::BandCount];
        for (int b = 0; b < StereoAnalyzer::BandCount; ++b) {
            bands[b] = _analyzer.bandCorrelation(b);
            bandsSilent[b] = _analyzer.bandSilent(b);
            bandXY[b] = frames > 0 ? _analyzer.bandPoints(b) : nullptr;
        }
        [_view setCorrelation:_analyzer.correlation()
                       silent:_analyzer.silent()
                        bands:bands
                  bandsSilent:bandsSilent];
        [_view advanceTrace:(frames > 0 ? _xy.data() : nullptr)
                      bands:bandXY
                      count:static_cast<NSInteger>(frames)
                 sampleRate:sampleRate
                  deltaTime:dt];
    }

    ScopePlugin& _plugin;
    p31::AudioRingReader _reader;
    ScopeSettings _settings;   // what the settings page edits
    StereoAnalyzer _analyzer;
    p31::editor::ParamWatch _params;
    std::vector<float> _lr;   // interleaved L/R read this frame
    std::vector<float> _xy;   // its goniometer points
    double _lastTick = 0.0;
    bool _wasFrozen = false;
    P31ScopeView* _view = nil;
    P31EditorRootView* _root = nil;
};

Form ScopeEditor::buildForm() {
    using namespace p31::ui;
    ScopeSettings& s = _settings;
    ScopeSettings* p = &s;
    Form f;

    f.push_back(section("Scope"));
    f.push_back(popup("Style",
                      {{"Goniometer (Mid / Side)", ScopeStyleGoniometer}, {"Lissajous (Left / Right)", ScopeStyleLissajous}},
                      [p] { return p->style; }, [p](int v) { p->style = v; },
                      "Goniometer draws mono upright; Lissajous plots left against right, so mono is a diagonal"));
    f.push_back(popup("Trace", {{"Lines", DrawModeLines}, {"Dots", DrawModeDots}}, [p] { return p->drawMode; },
                      [p](int v) { p->drawMode = v; },
                      "Lines join the samples and draw fast movement dimmer, like an oscilloscope tube"));
    {
        Row r = slider("Dot size", 0, kMaxDotSize, [p] { return p->dotSizeTenths; },
                       [p](double v) { p->dotSizeTenths = v < kMinDotSize ? 0 : static_cast<int>(v); },
                       [](double v) { return v < kMinDotSize ? std::string("Auto") : fmt("%.1f pt", v / 10.0); },
                       "Size of each dot; Auto grows gently with the scope");
        r.step = 5;
        r.visible = [p] { return p->drawMode == DrawModeDots; };
        f.push_back(r);
    }
    f.push_back(popup("Trace color", {{"Single", TraceColorSingle}, {"Low / Mid / High", TraceColorBands}},
                      [p] { return p->traceColorMode; }, [p](int v) { p->traceColorMode = v; },
                      "Color the trace by frequency band, split at the crossovers below"));
    {
        Row r = slider("Afterglow", kMinPersistenceMs, kMaxPersistenceMs, [p] { return p->persistenceMs; },
                       [p](double v) { p->persistenceMs = static_cast<int>(v); },
                       [](double v) { return fmt("%.0f ms", v); }, "How long the trace takes to fade");
        r.step = 10;
        f.push_back(r);
    }
    f.push_back(slider("Brightness", 0, 100, [p] { return p->brightness; },
                       [p](double v) { p->brightness = static_cast<int>(v); },
                       [](double v) { return fmt("%.0f %%", v); }));
    f.push_back(checkbox("Auto gain", [p] { return p->autoGain; }, [p](bool v) { p->autoGain = v; },
                         "Scale the picture so quiet and loud material both fill the scope"));
    {
        Row r = slider("Fixed gain", 0, kMaxGainDb, [p] { return p->gainDb; },
                       [p](double v) { p->gainDb = static_cast<int>(v); },
                       [](double v) { return fmt("%.0f dB", v); },
                       "Picture gain with auto gain off (0 dB: full scale touches the outline)");
        r.enabled = [p] { return !p->autoGain; };
        f.push_back(r);
    }

    f.push_back(section("Correlation meter"));
    f.push_back(checkbox("Show correlation meter", [p] { return p->showCorrelation; },
                         [p](bool v) { p->showCorrelation = v; },
                         "+1 = mono, 0 = wide or unrelated, below 0 = out of phase"));
    const auto meter = [p] { return p->showCorrelation; };
    const auto add = [&f, &meter](Row r) {
        r.enabled = meter;
        f.push_back(std::move(r));
    };
    add(popup("Bands", {{"Single", CorrelationSingle}, {"Low / Mid / High", CorrelationBands}},
              [p] { return p->correlationMode; }, [p](int v) { p->correlationMode = v; },
              "Three bands show which part of the spectrum is out of phase"));
    add(popup("Position",
              {{"Auto", p31::scope_layout::MeterAuto},
               {"Bottom", p31::scope_layout::MeterBottom},
               {"Top", p31::scope_layout::MeterTop},
               {"Left", p31::scope_layout::MeterLeft},
               {"Right", p31::scope_layout::MeterRight}},
              [p] { return p->meterPosition; }, [p](int v) { p->meterPosition = v; },
              "Auto puts the meter below or to the right, whichever leaves the larger scope"));
    add(slider("Bar thickness", kMinBarThickness, kMaxBarThickness, [p] { return p->barThickness; },
               [p](double v) { p->barThickness = static_cast<int>(v); }, [](double v) { return fmt("%.0f pt", v); }));
    {
        Row r = slider("Response time", kMinIntegrationMs, kMaxIntegrationMs, [p] { return p->integrationMs; },
                       [p](double v) { p->integrationMs = static_cast<int>(v); },
                       [](double v) { return fmt("%.0f ms", v); }, "Time window the reading is averaged over");
        r.step = 10;
        add(r);
    }
    {
        Row r = slider("Range hold", 0, kMaxRangeHoldMs, [p] { return p->rangeHoldMs; },
                       [p](double v) { p->rangeHoldMs = static_cast<int>(v); },
                       [](double v) { return v <= 0 ? std::string("Off") : fmt("%.0f ms", v); },
                       "A dim band shows how far the reading has swung recently; each end is held this long");
        r.step = 50;
        add(r);
    }
    add(number("Low / mid crossover (Hz)", kMinLowHz, kMaxLowHz, [p] { return p->lowHz; },
               [p](int v) { p->lowHz = v; }, "Split for the band meters and the band-colored trace"));
    add(number("Mid / high crossover (Hz)", kMinHighHz, kMaxHighHz, [p] { return p->highHz; },
               [p](int v) { p->highHz = v; }, "Split for the band meters and the band-colored trace"));

    f.push_back(section("Input"));
    f.push_back(p31::editor::inputGainRow(_plugin));
    f.push_back(p31::editor::freezeRow(_plugin));

    f.push_back(section("Appearance"));
    f.push_back(p31::editor::appearanceRow(s.appearance));
    f.push_back(popup("Theme", p31::editor::themeChoices(kThemes, kThemeMenuOrder), [p] { return p->theme; },
                      [p](int v) {
                          if (v == 0) p->theme = 0;
                          else applyTheme(*p, v);
                      },
                      "A color preset for trace, background, grid and bands"));
    f.push_back(checkbox("Show grid", [p] { return p->showGrid; }, [p](bool v) { p->showGrid = v; },
                         "Full-scale outline, level guides and the mid / side and left / right axes"));
    f.push_back(checkbox("Show labels", [p] { return p->showLabels; }, [p](bool v) { p->showLabels = v; },
                         "L / R / M / S around the scope and the correlation meter's scale"));
    f.push_back(slider("Grid opacity", 0, 100, [p] { return p->gridOpacity; },
                       [p](double v) { p->gridOpacity = static_cast<int>(v); },
                       [](double v) { return fmt("%.0f %%", v); }));
    const auto custom = [p] { p->theme = 0; };
    f.push_back(colorPair("Trace", s.light.trace, s.dark.trace, custom));
    f.push_back(colorPair("Background", s.light.background, s.dark.background, custom));
    f.push_back(colorPair("Grid", s.light.grid, s.dark.grid, custom));
    f.push_back(colorPair("Low band", s.light.bands[0], s.dark.bands[0], custom));
    f.push_back(colorPair("Mid band", s.light.bands[1], s.dark.bands[1], custom));
    f.push_back(colorPair("High band", s.light.bands[2], s.dark.bands[2], custom));
    return f;
}

} // namespace

std::unique_ptr<p31::Editor> makeScopeEditor(ScopePlugin& plugin, uint32_t width, uint32_t height) {
    return std::make_unique<ScopeEditor>(plugin, width, height);
}
