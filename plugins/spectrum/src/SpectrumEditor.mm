//
//  SpectrumEditor.mm
//  p31-spectrum (ported from foo_jl_spectrum_mac SpectrumController.mm / SpectrumPreferences.mm)
//
//  Owns the spectrum view and, on every display frame, pulls the audio played
//  since the previous frame from the plugin's ring buffer, runs it through the
//  analyzer and hands the bar levels to the view. Also describes the settings
//  page and the factory presets.
//

#include "SpectrumEditor.h"

#include "SpectrumPlugin.h"

#import <p31/render/EditorRootView.h>
#import <p31/render/SpectrumView.h>

#include <p31/core/AudioRing.h>
#include <p31/plugin/EditorSupport.h>
#include <p31/spectrum/SpectrumAnalyzer.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <vector>

namespace {

using namespace p31::spectrum;
using p31::ui::Form;

// Longest stretch read in one frame. After a stall (window hidden, UI
// blocked) the analyzer resumes near "now" instead of replaying the gap.
constexpr double kMaxWindowSeconds = 0.1;
// Covers kMaxWindowSeconds up to 192 kHz (85 ms at 384 kHz).
constexpr size_t kMaxFramesPerTick = 32768;

std::string fmt(const char* format, double v) {
    char s[48];
    std::snprintf(s, sizeof(s), format, v);
    return s;
}

std::vector<std::pair<std::string, SpectrumSettings>> factoryPresets() {
    std::vector<std::pair<std::string, SpectrumSettings>> out;
    out.emplace_back("Default", SpectrumSettings{});

    SpectrumSettings bars;
    bars.drawMode = DrawModeBars;
    bars.barCount = 48;
    bars.smoothingMode = SmoothingAttackDecay;
    bars.smoothing = 60;
    bars.freqScale = FreqScaleLog;
    out.emplace_back("Classic bars", bars);

    SpectrumSettings rta;
    rta.curveStyle = CurveStyleFilled;
    rta.curveOctaveFraction = 3;
    rta.freqScale = FreqScaleLog;
    rta.fftSize = 8192;
    rta.rmsMs = 500;
    out.emplace_back("RTA, 1/3 octave", rta);

    SpectrumSettings detail;
    detail.curveStyle = CurveStyleLine;
    detail.curveOctaveFraction = 0;
    detail.fftSize = 16384;
    detail.window = WindowBlackmanHarris;
    detail.freqScale = FreqScaleLog;
    detail.rangeDb = 120;
    out.emplace_back("Fine detail", detail);

    SpectrumSettings ms;
    ms.stereoMode = StereoMidSide;
    ms.curveStyle = CurveStyleLine;
    ms.curveOctaveFraction = 12;
    out.emplace_back("Mid + side", ms);
    return out;
}

class SpectrumEditor final : public p31::Editor {
public:
    SpectrumEditor(SpectrumPlugin& plugin, uint32_t width, uint32_t height)
        : _plugin(plugin), _reader(plugin.ring()), _settings(plugin.settings()) {
        _lr.resize(kMaxFramesPerTick * 2);
        _params.changed(plugin);

        const NSRect frame = NSMakeRect(0, 0, width, height);
        _view = [[P31SpectrumView alloc] initWithFrame:frame];
        _root = [[P31EditorRootView alloc] initWithFrame:frame content:_view title:@"Spectrum"];
        [_root setVersion:@P31_VERSION description:@"Real-time spectrum analyzer" url:@P31_PROJECT_URL];

        SpectrumEditor* editor = this;
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
        pushToView();
    }

    ~SpectrumEditor() override {
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
        if (visible) {
            // Start clean rather than from levels frozen when it was hidden.
            _reader.skipToNow();
            _analyzer.reset();
            pushToView();
        }
    }

    void settingsChanged() override {
        _settings = _plugin.settings();
        apply();
        [_root reloadSettings];
    }

private:
    // Push _settings to the view and the analyzer.
    void apply() {
        // The brand takes the theme's main color, as the logo README asks.
        [_root setBrandLightColor:_settings.light.bar darkColor:_settings.dark.bar];
        [_root setAppearanceMode:_settings.appearance];
        [_view applySettings:_settings];
        _analyzer.configure(analyzerSettings(_settings, static_cast<int>([_view autoBarCount])));
        pushToView();
    }

    // A control on the settings page changed _settings.
    void edited() {
        _plugin.setSettings(_settings);
        _settings = _plugin.settings();
        apply();
    }

    void applyPreset(const SpectrumSettings& preset) {
        SpectrumSettings s = preset;
        s.appearance = _settings.appearance;
        _plugin.setSettings(s);
        settingsChanged();
    }

    NSMenu* presetMenu() {
        SpectrumEditor* editor = this;
        return p31::ui::makePresetMenu(
            p31::editor::presetSource<SpectrumSettings>(_plugin, factoryPresets(),
                                                         [editor](const SpectrumSettings& s) { editor->applyPreset(s); }),
            _root);
    }

    Form buildForm();

    // In Curve mode and with "Auto" bars the bar count follows the view.
    void syncBarCount() {
        if (!_settings.viewSizedBars()) return;
        const int count = static_cast<int>([_view autoBarCount]);
        if (count != _analyzer.settings().barCount) _analyzer.configure(analyzerSettings(_settings, count));
    }

    void tick(double now) {
        if (_params.changed(_plugin)) [_root reloadSettings];
        [_root refreshFreeze];
        // Frozen: the picture holds; on release it continues from "now".
        if (_plugin.frozen()) {
            _wasFrozen = true;
            return;
        }
        if (_wasFrozen) {
            _wasFrozen = false;
            _reader.skipToNow();
            _lastTick = now;
        }

        // Real elapsed time drives smoothing and falls, so a late frame does
        // not slow them down.
        const double dt = _lastTick > 0 ? now - _lastTick : 1.0 / 60.0;
        _lastTick = now;

        const double sampleRate = _plugin.currentSampleRate();
        if (sampleRate > 0) {
            _analyzer.setSampleRate(sampleRate);
            const auto maxFrames = std::min(kMaxFramesPerTick,
                                            static_cast<size_t>(std::ceil(sampleRate * kMaxWindowSeconds)));
            const size_t frames = _reader.readNew(_lr.data(), maxFrames).frames;
            const float gain = _plugin.analysisGain();
            if (gain != 1.0f) {
                for (size_t i = 0; i < frames * 2; ++i) _lr[i] *= gain;
            }
            _analyzer.process(_lr.data(), frames);
        }
        syncBarCount();

        const bool live = _analyzer.tick(dt);
        // Redraw while there is audio or levels are still settling.
        if (live || _analyzer.isActive() || _wasActive) pushToView();
        _wasActive = _analyzer.isActive();
    }

    void pushToView() {
        const auto& bars = _analyzer.bars();
        const auto& side = _analyzer.side();
        [_view setBars:bars.data()
                shadow:_analyzer.shadow().data()
                 peaks:_analyzer.peaks().data()
                  side:(side.size() == bars.size() ? side.data() : nullptr)
                 count:static_cast<NSInteger>(bars.size())];
    }

    SpectrumPlugin& _plugin;
    p31::AudioRingReader _reader;
    SpectrumSettings _settings;   // what the settings page edits
    SpectrumAnalyzer _analyzer;
    p31::editor::ParamWatch _params;
    std::vector<float> _lr;       // interleaved L/R read this frame
    double _lastTick = 0.0;
    bool _wasActive = false;
    bool _wasFrozen = false;
    P31SpectrumView* _view = nil;
    P31EditorRootView* _root = nil;
};

Form SpectrumEditor::buildForm() {
    using namespace p31::ui;
    SpectrumSettings& s = _settings;
    SpectrumSettings* p = &s;
    const auto curve = [p] { return p->drawMode == DrawModeCurve; };
    const auto bars = [p] { return p->drawMode == DrawModeBars; };
    Form f;

    f.push_back(section("Display"));
    f.push_back(popup("Draw mode", {{"Curve", DrawModeCurve}, {"Bars", DrawModeBars}},
                      [p] { return p->drawMode; }, [p](int v) { p->drawMode = v; }));
    {
        Row r = popup("Curve style",
                      {{"Filled", CurveStyleFilled}, {"Line", CurveStyleLine}, {"Color bars", CurveStyleColorBars},
                       {"Line + color bars", CurveStyleLineColorBars}},
                      [p] { return p->curveStyle; }, [p](int v) { p->curveStyle = v; },
                      "Color bars are stripes that get brighter where a frequency is louder");
        r.visible = curve;
        f.push_back(r);
    }
    {
        Row r = popup("Curve detail",
                      {{"Full", 0}, {"1/24 octave", 24}, {"1/12 octave", 12}, {"1/6 octave", 6}, {"1/3 octave", 3}},
                      [p] { return p->curveOctaveFraction; }, [p](int v) { p->curveOctaveFraction = v; },
                      "Full draws every FFT bin; an octave fraction averages the power around each point for a "
                      "smoother curve");
        r.visible = curve;
        f.push_back(r);
    }
    {
        Row r = slider("Color bar brightness", 10, 100, [p] { return p->colorBarBrightness; },
                       [p](double v) { p->colorBarBrightness = static_cast<int>(v); },
                       [](double v) { return fmt("%.0f %%", v); }, "How strong the loudest color bar gets");
        r.visible = curve;
        r.enabled = [p] { return p->curveStyle == CurveStyleColorBars || p->curveStyle == CurveStyleLineColorBars; };
        f.push_back(r);
    }
    {
        std::vector<std::pair<std::string, int>> counts{{"Auto", kBarCountAuto}};
        for (int n : {16, 24, 32, 48, 64, 96, 128, 192, 256, 384, 512}) counts.emplace_back(std::to_string(n), n);
        Row r = popup("Bars", counts, [p] { return p->barCount; }, [p](int v) { p->barCount = v; },
                      "Auto draws one bar per 2 points of width");
        r.visible = bars;
        f.push_back(r);
    }
    {
        Row r = popup("Bar style", {{"Solid", BarStyleSolid}, {"Gradient", BarStyleGradient}, {"Spectrum", BarStyleSpectrum}},
                      [p] { return p->barStyle; }, [p](int v) { p->barStyle = v; });
        r.visible = bars;
        f.push_back(r);
    }
    {
        Row r = slider("Bar gap", 0, 80, [p] { return p->gapPercent; },
                       [p](double v) { p->gapPercent = static_cast<int>(v); },
                       [](double v) { return fmt("%.0f %%", v); });
        r.visible = bars;
        f.push_back(r);
    }
    f.push_back(popup("Orientation", {{"Horizontal", OrientationHorizontal}, {"Vertical", OrientationVertical}},
                      [p] { return p->orientation; }, [p](int v) { p->orientation = v; }));
    f.push_back(slider("Top", kMinTopDb, kMaxTopDb, [p] { return p->topDb; },
                       [p](double v) { p->topDb = static_cast<int>(v); }, [](double v) { return fmt("%.0f dB", v); },
                       "Level at the top of the display; lower it to fill the height with quiet material"));
    {
        std::vector<std::pair<std::string, int>> ranges;
        for (int r : kRangeDbChoices) ranges.emplace_back(std::to_string(r) + " dB", r);
        f.push_back(popup("Range", ranges, [p] { return p->rangeDb; }, [p](int v) { p->rangeDb = v; },
                          "dB from the top of the display to the bottom"));
    }
    f.push_back(checkbox("Show peaks", [p] { return p->peakHold; }, [p](bool v) { p->peakHold = v; },
                         "Peak caps in Bars mode, a peak line in Curve mode"));
    f.push_back(checkbox("Shadow fill", [p] { return p->shadowFill; }, [p](bool v) { p->shadowFill = v; },
                         "Dim, slow-falling fill behind the levels"));
    f.push_back(checkbox("Show dB scale", [p] { return p->showDbGuides; }, [p](bool v) { p->showDbGuides = v; }));
    f.push_back(checkbox("Show frequency axis", [p] { return p->showFreqAxis; }, [p](bool v) { p->showFreqAxis = v; },
                         "Frequency gridlines and labels (in Bars mode, each bar's centre frequency)"));

    f.push_back(section("Analysis"));
    {
        std::vector<std::pair<std::string, int>> sizes;
        for (int n : kFftSizes) sizes.emplace_back(std::to_string(n), n);
        f.push_back(popup("FFT size", sizes, [p] { return p->fftSize; }, [p](int v) { p->fftSize = v; },
                          "Larger sizes resolve low frequencies better but react more slowly"));
    }
    f.push_back(popup("Window", {{"Hann", WindowHann}, {"Blackman-Harris", WindowBlackmanHarris}},
                      [p] { return p->window; }, [p](int v) { p->window = v; },
                      "Blackman-Harris keeps loud tones from spilling into quiet neighbours, at the cost of wider "
                      "peaks"));
    f.push_back(popup("Stereo", {{"Mono", StereoMono}, {"Mid + side", StereoMidSide}},
                      [p] { return p->stereoMode; }, [p](int v) { p->stereoMode = v; },
                      "Mid + side adds the stereo difference (L-R) as a second line"));
    f.push_back(popup("Frequency scale",
                      {{"Logarithmic", FreqScaleLog},
                       {"Linear", FreqScaleLinear},
                       {"Logarithmic (compact bass)", FreqScaleSoftLog}},
                      [p] { return p->freqScale; }, [p](int v) { p->freqScale = v; }));
    f.push_back(number("Lowest frequency (Hz)", 10, 1000, [p] { return p->minHz; }, [p](int v) { p->minHz = v; }));
    f.push_back(number("Highest frequency (Hz)", 1000, 96000, [p] { return p->maxHz; }, [p](int v) { p->maxHz = v; },
                       "Limited to half the sample rate"));
    f.push_back(slider("Slope", 0, kMaxSlopeTenths, [p] { return p->slopeTenths; },
                       [p](double v) { p->slopeTenths = static_cast<int>(v); },
                       [](double v) { return fmt("%.1f dB/oct", v / 10.0); },
                       "Tilts the spectrum around 1 kHz so high frequencies read higher"));

    f.push_back(section("Motion"));
    f.push_back(popup("Smoothing mode", {{"Attack / decay", SmoothingAttackDecay}, {"RMS average", SmoothingRms}},
                      [p] { return p->smoothingMode; }, [p](int v) { p->smoothingMode = v; },
                      "RMS average averages signal power over time, so levels rise and fall evenly like a "
                      "real-time analyzer"));
    {
        Row r = slider("Smoothing", 0, 100, [p] { return p->smoothing; },
                       [p](double v) { p->smoothing = static_cast<int>(v); }, [](double v) { return fmt("%.0f", v); });
        r.visible = [p] { return p->smoothingMode == SmoothingAttackDecay; };
        f.push_back(r);
    }
    {
        Row r = slider("Averaging time", 0, kMaxRmsMs, [p] { return p->rmsMs; },
                       [p](double v) { p->rmsMs = static_cast<int>(v); }, [](double v) { return fmt("%.0f ms", v); });
        r.step = 10;
        r.visible = [p] { return p->smoothingMode == SmoothingRms; };
        f.push_back(r);
    }
    {
        Row r = slider("Shadow fall", 0, 100, [p] { return p->shadowFallSpeed; },
                       [p](double v) { p->shadowFallSpeed = static_cast<int>(v); },
                       [](double v) { return fmt("%.0f", v); }, "How fast the shadow falls (higher = faster)");
        r.enabled = [p] { return p->shadowFill; };
        f.push_back(r);
    }
    {
        Row r = slider("Peak fall", kMinPeakFallDbPerSec, kMaxPeakFallDbPerSec, [p] { return p->peakFallDbPerSec; },
                       [p](double v) { p->peakFallDbPerSec = static_cast<int>(v); },
                       [](double v) { return fmt("%.0f dB/s", v); }, "How fast peaks fall after their hold");
        r.enabled = [p] { return p->peakHold; };
        f.push_back(r);
    }
    {
        Row r = slider("Peak hold", 0, 2000, [p] { return p->peakHoldMs; },
                       [p](double v) { p->peakHoldMs = static_cast<int>(v); },
                       [](double v) { return fmt("%.0f ms", v); }, "How long a peak stays before it falls");
        r.step = 10;
        r.enabled = [p] { return p->peakHold; };
        f.push_back(r);
    }

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
                      "A color preset for levels, background and grid"));
    f.push_back(slider("Grid opacity", 0, 100, [p] { return p->gridOpacity; },
                       [p](double v) { p->gridOpacity = static_cast<int>(v); },
                       [](double v) { return fmt("%.0f %%", v); }));
    const auto custom = [p] { p->theme = 0; };
    f.push_back(colorPair("Levels", s.light.bar, s.dark.bar, custom));
    f.push_back(colorPair("Background", s.light.background, s.dark.background, custom));
    f.push_back(colorPair("Grid", s.light.grid, s.dark.grid, custom));
    return f;
}

} // namespace

std::unique_ptr<p31::Editor> makeSpectrumEditor(SpectrumPlugin& plugin, uint32_t width, uint32_t height) {
    return std::make_unique<SpectrumEditor>(plugin, width, height);
}
