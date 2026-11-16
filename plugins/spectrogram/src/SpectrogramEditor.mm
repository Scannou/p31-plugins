//
//  SpectrogramEditor.mm
//  p31-spectrogram (ported from foo_jl_spectrogram_mac SpectrogramController.mm / SpectrogramPreferences.mm)
//
//  Owns the spectrogram view and, on every display frame, renders the columns
//  the display clock has passed: each one reads its own analysis window from
//  the plugin's ring buffer, so column spacing does not depend on frame
//  timing. Also describes the settings page and the factory presets.
//

#include "SpectrogramEditor.h"

#include "SpectrogramPlugin.h"

#import <p31/render/EditorRootView.h>
#import <p31/render/SpectrogramView.h>

#include <p31/core/AudioRing.h>
#include <p31/plugin/EditorSupport.h>
#include <p31/spectrogram/SpectrogramClock.h>
#include <p31/spectrogram/SpectrogramEngine.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

namespace {

using namespace p31::spectrogram;
using p31::ui::Form;

// No new audio for this long means the host stopped processing: the picture
// holds, as it did in foobar2000 on pause and stop. Longer than any host
// buffer the clock follows.
constexpr double kStallSeconds = 0.6;
// A display frame later than this says nothing about the host's buffer length.
constexpr double kLateFrameSeconds = 0.1;
// Toolbar to the plot's right edge.
constexpr CGFloat kToolbarGap = 8.0;

std::string fmt(const char* format, double v) {
    char s[48];
    std::snprintf(s, sizeof(s), format, v);
    return s;
}

// Samples beyond +60 dBFS, NaN or infinite come from a broken plugin
// upstream and read as silence, as in P31 Spectrum.
inline float cleanSample(float v) {
    return std::fabs(v) <= 1000.0f ? v : 0.0f;
}

int mapNamed(const char* name) {
    for (int m = 0; m < kMapCount; ++m) {
        if (std::string(kMaps[m].name) == name) return m;
    }
    return 0;
}

std::vector<std::pair<std::string, SpectrogramSettings>> factoryPresets() {
    std::vector<std::pair<std::string, SpectrogramSettings>> out;
    out.emplace_back("Default", SpectrogramSettings{});

    SpectrogramSettings pitch;
    pitch.analysisMode = SpectrogramEngine::ModeMultiRes;
    pitch.fftSize = 8192;
    pitch.window = SpectrogramEngine::WindowBlackmanHarris;
    pitch.rangeDb = 90;
    out.emplace_back("Fine pitch", pitch);

    SpectrogramSettings transients;
    transients.analysisMode = SpectrogramEngine::ModeStandard;
    transients.fftSize = 1024;
    transients.scrollSpeed = 240;
    out.emplace_back("Fast transients", transients);

    SpectrogramSettings side;
    side.channel = ChannelSide;
    applyColormap(side, mapNamed("Ice"));
    out.emplace_back("Side channel", side);

    // To sit under P31 Spectrum: its default theme, frequency along the same axis.
    SpectrogramSettings phosphor;
    applyColormap(phosphor, mapNamed("P7 blue"));
    phosphor.orientation = OrientationHorizontal;
    out.emplace_back("P7 blue, horizontal", phosphor);
    return out;
}

class SpectrogramEditor final : public p31::Editor {
public:
    SpectrogramEditor(SpectrogramPlugin& plugin, uint32_t width, uint32_t height)
        : _plugin(plugin), _settings(plugin.settings()) {
        _params.changed(plugin);
        _written = plugin.ring().writePosition();

        const NSRect frame = NSMakeRect(0, 0, width, height);
        _view = [[P31SpectrogramView alloc] initWithFrame:frame];
        _root = [[P31EditorRootView alloc] initWithFrame:frame content:_view title:@"Spectrogram"];
        [_root setVersion:@P31_VERSION description:@"Scrolling spectrogram" url:@P31_PROJECT_URL];

        SpectrogramEditor* editor = this;
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

    ~SpectrogramEditor() override {
        [_view stopAnimating];
        [_root removeFromSuperview];
        _root = nil;
        _view = nil;
    }

    bool attach(void* parent) override {
        NSView* parentView = (__bridge NSView*)parent;
        if (parentView == nil) return false;
        [parentView addSubview:_root];
        _clock.reset();
        return true;
    }

    void setSize(uint32_t width, uint32_t height) override {
        [_root setFrameSize:NSMakeSize(width, height)];
    }

    void setVisible(bool visible) override {
        _root.hidden = !visible;
        // Continue at "now" rather than replaying what played while hidden.
        if (visible) _clock.reset();
    }

    void settingsChanged() override {
        _settings = _plugin.settings();
        apply();
        [_root reloadSettings];
    }

private:
    // Push _settings to the view and the engine.
    void apply() {
        const SpectrogramSettings s = _settings.sanitized();
        // The brand takes the colormap's accent, the color of the probe
        // marker; darkened where it sits on a light background.
        uint32_t lut[kLutSize];
        buildLut(s, lut);
        uint32_t accent = accentColor(lut);
        if (!isDisplayP3Map(s.colormap)) accent = srgbToDisplayP3(accent);
        [_root setBrandLightColor:darkened(accent) darkColor:accent];
        [_root setAppearanceMode:s.appearance];
        [_view applySettings:s];
        _engine.configure(engineSettings(s));
        _channel = s.channel;
        _scrollSpeed = s.scrollSpeed;
    }

    static uint32_t darkened(uint32_t argb) {
        uint32_t out = argb & 0xFF000000u;
        for (int shift = 0; shift <= 16; shift += 8) {
            out |= static_cast<uint32_t>(std::lround(((argb >> shift) & 0xFF) * 0.6)) << shift;
        }
        return out;
    }

    // A control on the settings page changed _settings.
    void edited() {
        _plugin.setSettings(_settings);
        _settings = _plugin.settings();
        apply();
    }

    void applyPreset(const SpectrogramSettings& preset) {
        SpectrogramSettings s = preset;
        s.appearance = _settings.appearance;
        _plugin.setSettings(s);
        settingsChanged();
    }

    NSMenu* presetMenu() {
        SpectrogramEditor* editor = this;
        return p31::ui::makePresetMenu(
            p31::editor::presetSource<SpectrogramSettings>(
                _plugin, factoryPresets(), [editor](const SpectrogramSettings& s) { editor->applyPreset(s); }),
            _root);
    }

    Form buildForm();

    // The toolbar stays over the plot, left of the labels beside it.
    void placeToolbar() {
        const CGFloat inset = [_view plotInsetRight] + kToolbarGap;
        if (std::fabs(_root.toolbarInsetRight - inset) > 0.5) _root.toolbarInsetRight = inset;
    }

    // `count` frames ending at ring position `end` (at most the write
    // position), as the mono signal the channel setting asks for. Frames
    // from before the start of the stream, or already overwritten, are silent.
    void readWindow(uint64_t end, size_t count) {
        _lr.resize(count * 2);
        _samples.resize(count);
        const auto have = static_cast<size_t>(std::min<uint64_t>(end, count));
        const size_t pad = count - have;
        std::fill_n(_lr.begin(), pad * 2, 0.0f);
        _plugin.ring().copy(end - have, have, _lr.data() + pad * 2);

        const float gain = _plugin.analysisGain();
        const float* lr = _lr.data();
        float* out = _samples.data();
        switch (_channel) {
            case ChannelLeft:
                for (size_t i = 0; i < count; ++i) out[i] = gain * cleanSample(lr[2 * i]);
                break;
            case ChannelRight:
                for (size_t i = 0; i < count; ++i) out[i] = gain * cleanSample(lr[2 * i + 1]);
                break;
            case ChannelSide:
                for (size_t i = 0; i < count; ++i) {
                    out[i] = 0.5f * gain * (cleanSample(lr[2 * i]) - cleanSample(lr[2 * i + 1]));
                }
                break;
            default:
                for (size_t i = 0; i < count; ++i) {
                    out[i] = 0.5f * gain * (cleanSample(lr[2 * i]) + cleanSample(lr[2 * i + 1]));
                }
                break;
        }
    }

    // Columns sit on a fixed time grid (one per device pixel of travel). Each
    // tick renders the grid points SpectrogramClock says the display has
    // passed. The stream time is the audio written to the ring so far.
    void tick(double now) {
        if (_params.changed(_plugin)) [_root reloadSettings];
        [_root refreshFreeze];
        [_root snapContentToPixels];
        placeToolbar();

        const double sampleRate = _plugin.currentSampleRate();
        const uint64_t written = _plugin.ring().writePosition();
        const bool onTime = _lastTick > 0 && now - _lastTick < kLateFrameSeconds;
        _lastTick = now;
        if (written != _written) {
            // Audio arrives one host buffer at a time; the clock keeps the
            // columns far enough behind for theirs to be there.
            if (onTime && sampleRate > 0) _clock.noteStreamStep(static_cast<double>(written - _written) / sampleRate);
            _written = written;
            _lastAudio = now;
        }

        const NSInteger rows = [_view plotRows];
        // Frozen, or the host stopped processing: the picture holds, and
        // continues from "now" afterwards.
        if (_plugin.frozen() || rows <= 0 || sampleRate <= 0 || now - _lastAudio > kStallSeconds) {
            _clock.reset();
            // At rest the picture sits on whole pixels, not between two.
            if (!_holding) [_view flushWithPhase:0.0];
            _holding = true;
            return;
        }
        _holding = false;

        const double hop = 1.0 / (_scrollSpeed * [_view columnsPerPoint]);
        const int due = _clock.due(now, true, static_cast<double>(written) / sampleRate, hop);

        _engine.setRows(static_cast<int>(rows));
        _engine.setSampleRate(sampleRate);
        _levels.resize(static_cast<size_t>(rows));
        const auto window = static_cast<size_t>(_engine.windowLength());

        for (int i = 0; i < due; ++i) {
            // The window ends at the column's time: the plugin has no audio
            // ahead of "now" to centre it on, so the picture trails by half
            // a window instead.
            const auto end = static_cast<int64_t>(std::llround(_clock.next(hop) * sampleRate));
            // The audio is not there yet; pick up next tick.
            if (end > static_cast<int64_t>(written)) break;
            // A restarted clock steps back a little: columns already drawn
            // (and those before the stream began) are not drawn again.
            if (end > _renderedEnd) {
                readWindow(static_cast<uint64_t>(end), window);
                _engine.renderColumn(_samples.data(), _levels.data());
                [_view appendColumn:_levels.data() rows:rows];
                _renderedEnd = end;
            }
            _clock.advance(hop);
        }
        // Every frame, so the sub-column phase keeps the picture moving evenly.
        [_view flushWithPhase:_clock.phase(hop)];
    }

    SpectrogramPlugin& _plugin;
    SpectrogramSettings _settings;   // what the settings page edits
    SpectrogramEngine _engine;
    SpectrogramClock _clock;
    p31::editor::ParamWatch _params;
    std::vector<float> _lr;          // interleaved L/R of one analysis window
    std::vector<float> _samples;     // the window as the analysed mono signal
    std::vector<float> _levels;      // one column
    int _channel = ChannelMid;
    int _scrollSpeed = 120;          // points per second
    uint64_t _written = 0;           // ring write position at the last tick
    int64_t _renderedEnd = 0;        // ring position the newest column's window ends at
    double _lastTick = 0.0;
    double _lastAudio = 0.0;         // time of the last tick that found new audio
    bool _holding = false;           // the picture is at rest (frozen, no audio)
    P31SpectrogramView* _view = nil;
    P31EditorRootView* _root = nil;
};

Form SpectrogramEditor::buildForm() {
    using namespace p31::ui;
    SpectrogramSettings& s = _settings;
    SpectrogramSettings* p = &s;
    Form f;

    f.push_back(section("Display"));
    f.push_back(popup("Orientation",
                      {{"Horizontal: frequency along X", OrientationHorizontal},
                       {"Vertical: frequency along Y", OrientationVertical}},
                      [p] { return p->orientation; }, [p](int v) { p->orientation = v; },
                      "As in P31 Spectrum: the same setting lines up their frequency axes. Changing it clears the "
                      "picture."));
    f.push_back(checkbox("Reverse scroll direction", [p] { return p->reverseScroll; },
                         [p](bool v) { p->reverseScroll = v; },
                         "New audio enters at the bottom (horizontal) or the left (vertical)"));
    {
        std::vector<std::pair<std::string, int>> speeds;
        for (int speed : kScrollSpeeds) speeds.emplace_back(std::to_string(speed) + " pt/s", speed);
        f.push_back(popup("Scroll speed", speeds, [p] { return p->scrollSpeed; }, [p](int v) { p->scrollSpeed = v; }));
    }
    f.push_back(slider("Top level", kMinTopDb, kMaxTopDb, [p] { return p->topDb; },
                       [p](double v) { p->topDb = static_cast<int>(v); }, [](double v) { return fmt("%.0f dB", v); },
                       "Level shown in the brightest color. Lower it for quiet material."));
    f.push_back(slider("Range", kMinRangeDb, kMaxRangeDb, [p] { return p->rangeDb; },
                       [p](double v) { p->rangeDb = static_cast<int>(v); }, [](double v) { return fmt("%.0f dB", v); },
                       "dB span below the top level. Smaller: more contrast, less quiet detail."));
    f.push_back(slider("Slope", 0, kMaxSlopeTenths, [p] { return p->slopeTenths; },
                       [p](double v) { p->slopeTenths = static_cast<int>(v); },
                       [](double v) { return fmt("%.1f dB/oct", v / 10.0); },
                       "Tilt around 1 kHz that brightens treble; the dB scale is then exact only at 1 kHz"));
    f.push_back(checkbox("Show frequency axis", [p] { return p->showFreqAxis; }, [p](bool v) { p->showFreqAxis = v; },
                         "Frequency labels right of the plot (vertical) or below it (horizontal)"));
    f.push_back(checkbox("Show dB color scale", [p] { return p->showLegend; }, [p](bool v) { p->showLegend = v; },
                         "Color bar with its dB levels on the right"));
    {
        std::vector<std::pair<std::string, int>> sizes;
        for (int size = kMinFontSize; size <= kMaxFontSize; ++size) sizes.emplace_back(std::to_string(size) + " pt", size);
        Row freq = popup("Frequency text", sizes, [p] { return p->freqFontSize; }, [p](int v) { p->freqFontSize = v; });
        freq.enabled = [p] { return p->showFreqAxis; };
        f.push_back(freq);
        Row db = popup("dB text", sizes, [p] { return p->dbFontSize; }, [p](int v) { p->dbFontSize = v; });
        db.enabled = [p] { return p->showLegend; };
        f.push_back(db);
    }

    f.push_back(section("Analysis"));
    f.push_back(popup("Resolution",
                      {{"Reassigned", SpectrogramEngine::ModeReassigned},
                       {"Multi-resolution", SpectrogramEngine::ModeMultiRes},
                       {"Standard", SpectrogramEngine::ModeStandard}},
                      [p] { return p->analysisMode; }, [p](int v) { p->analysisMode = v; },
                      "Reassigned: thin tonal lines and short kicks together. Multi-resolution: a 4x window for "
                      "bass, sharper lines but smeared kicks. Standard: one window everywhere."));
    {
        std::vector<std::pair<std::string, int>> sizes;
        for (int n : kFftSizes) sizes.emplace_back(std::to_string(n), n);
        f.push_back(popup("FFT size", sizes, [p] { return p->fftSize; }, [p](int v) { p->fftSize = v; },
                          "At 44.1/48 kHz, scaled up at higher rates. Larger: finer pitch, blurrier timing, and "
                          "the picture trails the audio by half a window."));
    }
    f.push_back(popup("Window",
                      {{"Hann", SpectrogramEngine::WindowHann},
                       {"Blackman-Harris", SpectrogramEngine::WindowBlackmanHarris}},
                      [p] { return p->window; }, [p](int v) { p->window = v; },
                      "Blackman-Harris keeps loud tones from smearing over quiet detail at wide dB ranges"));
    f.push_back(popup("Channel",
                      {{"Mid (L + R)", ChannelMid},
                       {"Left", ChannelLeft},
                       {"Right", ChannelRight},
                       {"Side (L − R)", ChannelSide}},
                      [p] { return p->channel; }, [p](int v) { p->channel = v; }));
    f.push_back(popup("Frequency scale",
                      {{"Logarithmic", SpectrogramEngine::ScaleLog},
                       {"Mel", SpectrogramEngine::ScaleMel},
                       {"Linear", SpectrogramEngine::ScaleLinear}},
                      [p] { return p->freqScale; }, [p](int v) { p->freqScale = v; },
                      "Changing the scale or range clears the picture"));
    f.push_back(number("Lowest frequency (Hz)", 10, 2000, [p] { return p->minHz; }, [p](int v) { p->minHz = v; }));
    f.push_back(number("Highest frequency (Hz)", 1000, 96000, [p] { return p->maxHz; }, [p](int v) { p->maxHz = v; },
                       "Rows above half the sample rate stay empty"));

    f.push_back(section("Input"));
    f.push_back(p31::editor::inputSourceRow(_plugin));
    f.push_back(p31::editor::inputGainRow(_plugin));
    f.push_back(p31::editor::freezeRow(_plugin));

    f.push_back(section("Appearance"));
    f.push_back(p31::editor::appearanceRow(s.appearance));
    {
        // Custom, the general-purpose maps, then the P31 Spectrum themes in
        // its menu order: the phosphors first.
        std::vector<std::pair<std::string, int>> maps{{"Custom", kCustomMap}};
        for (int m = 0; m < kFirstThemeMap; ++m) maps.emplace_back(kMaps[m].name, m);
        constexpr int kPhosphors = 4;
        for (int m = kMapCount - kPhosphors; m < kMapCount; ++m) maps.emplace_back(kMaps[m].name, m);
        for (int m = kFirstThemeMap; m < kMapCount - kPhosphors; ++m) maps.emplace_back(kMaps[m].name, m);
        f.push_back(popup("Colormap", maps, [p] { return p->colormap; }, [p](int v) { applyColormap(*p, v); },
                          "A preset fills in the four colors below; editing one switches to Custom. Theme ramps "
                          "also set the background and grid colors."));
    }
    // Custom ramp: floor (silence) up to peak (top level).
    const auto custom = [p] { p->colormap = kCustomMap; };
    static const char* const kCustomNames[kCustomColorCount] = {"Floor", "Low", "Mid", "Peak"};
    for (int i = 0; i < kCustomColorCount; ++i) {
        f.push_back(color(std::string(kCustomNames[i]) + " color", s.customColors[i], custom,
                          std::string(kCustomNames[i]) + " color of the ramp"));
    }
    f.push_back(slider("Grid opacity", 0, 100, [p] { return p->gridOpacity; },
                       [p](double v) { p->gridOpacity = static_cast<int>(v); },
                       [](double v) { return fmt("%.0f %%", v); },
                       "Lines across the plot at the labelled frequencies; 0 % hides them"));
    // Margins around the plot; labels take their color from the background.
    f.push_back(colorPair("Background", s.light.background, s.dark.background));
    f.push_back(colorPair("Grid", s.light.grid, s.dark.grid));
    return f;
}

} // namespace

std::unique_ptr<p31::Editor> makeSpectrogramEditor(SpectrogramPlugin& plugin, uint32_t width, uint32_t height) {
    return std::make_unique<SpectrogramEditor>(plugin, width, height);
}
