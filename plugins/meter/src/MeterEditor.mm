//
//  MeterEditor.mm
//  p31-meter (ported from foo_jl_levelmeter_mac LevelMeterController.mm / LevelMeterPreferences.mm)
//
//  Owns the meter view and, on every display frame, has the measurement take
//  in the audio received since the previous frame and hands its readings to
//  the view. Also describes the settings page, the factory presets and the
//  meter's own right-click items.
//

#include "MeterEditor.h"

#import <p31/render/EditorRootView.h>
#import <p31/render/Menus.h>
#import <p31/render/MeterView.h>

#include <p31/plugin/EditorSupport.h>

#include <algorithm>
#include <cstdio>
#include <vector>

namespace {

using namespace p31::meter;
using p31::ui::Form;

// Keep redrawing this long after the audio stops, while the bars fall.
constexpr double kSettleSeconds = 4.0;

std::string fmt(const char* format, double v) {
    char s[48];
    std::snprintf(s, sizeof(s), format, v);
    return s;
}

std::vector<std::pair<std::string, MeterSettings>> factoryPresets() {
    std::vector<std::pair<std::string, MeterSettings>> out;
    out.emplace_back("Default", MeterSettings{});

    MeterSettings levels;
    levels.showLoudness = false;
    levels.showCorrelation = false;
    levels.showBalance = false;
    out.emplace_back("Levels only", levels);

    MeterSettings analog;
    analog.style = static_cast<int>(MeterStyle::Analog);
    out.emplace_back("Analog VU", analog);

    // Loud modern masters pin a -18 dBFS reference at the top of the scale.
    MeterSettings loud = analog;
    loud.face = static_cast<int>(FaceStyle::Dark);
    loud.referenceDb = -10;
    out.emplace_back("Analog VU, loud masters", loud);

    MeterSettings k14;
    k14.scale = static_cast<int>(ScaleType::K14);
    out.emplace_back("K-14", k14);

    MeterSettings broadcast;
    broadcast.targetLufs = -23;
    out.emplace_back("Broadcast (EBU R128)", broadcast);
    return out;
}

class MeterEditor final : public p31::Editor {
public:
    MeterEditor(MeterModule& module, MeterMeasurement& measurement, uint32_t width, uint32_t height)
        : _module(module), _plugin(module.plugin()), _measurement(measurement), _settings(module.settings()) {
        _params.changed(_plugin);

        const NSRect frame = NSMakeRect(0, 0, width, height);
        _view = [[P31MeterView alloc] initWithFrame:frame];
        _root = [[P31EditorRootView alloc] initWithFrame:frame content:_view title:@"Meter"];
        [_root setVersion:@P31_VERSION
              description:@"Peak, RMS and loudness meter with an analog VU look"
                      url:@P31_PROJECT_URL];

        MeterEditor* editor = this;
        _root.makeForm = ^Form { return editor->buildForm(); };
        _root.settingsPanel.onChange = ^{ editor->edited(); };
        _root.presetMenu = ^NSMenu* { return editor->presetMenu(); };
        _root.isFrozen = ^BOOL { return editor->_plugin.frozen(); };
        _root.toggleFreeze = ^{ p31::editor::toggleFreeze(editor->_plugin); };
        _root.requestResize = ^BOOL(NSSize size) {
            return editor->_plugin.requestEditorResize(static_cast<uint32_t>(std::max(0.0, size.width)),
                                                       static_cast<uint32_t>(std::max(0.0, size.height)));
        };
        _root.extendMenu = ^(NSMenu* menu) { editor->addMenuItems(menu); };
        _view.onResetPeaks = ^{ editor->resetPeaks(); };
        _view.onResetLoudness = ^{ editor->resetLoudness(); };
        _view.onFrame = ^(double timestamp) { editor->tick(timestamp); };

        apply();
    }

    ~MeterEditor() override {
        [_view stopAnimating];
        [_root removeFromSuperview];
        _root = nil;
        _view = nil;
    }

    bool attach(void* parent) override {
        NSView* parentView = (__bridge NSView*)parent;
        if (parentView == nil) return false;
        [parentView addSubview:_root];
        return true;
    }

    void setSize(uint32_t width, uint32_t height) override {
        [_root setFrameSize:NSMakeSize(width, height)];
    }

    void* view() const override { return (__bridge void*)_root; }

    void setVisible(bool visible) override {
        _root.hidden = !visible;
        if (visible) show();
    }

    void settingsChanged() override {
        _settings = _module.settings();
        apply();
        [_root reloadSettings];
    }

private:
    // Push _settings to the view and the measurement.
    void apply() {
        // The brand takes the theme's main color, as the logo README asks.
        [_root setBrandLightColor:_settings.light[ColorLevel] darkColor:_settings.dark[ColorLevel]];
        [_root setAppearanceMode:_settings.appearance];
        [_view applySettings:_settings];
        _measurement.settingsChanged();
        show();
    }

    // The readings as they are now, whether or not anything moves.
    void show() {
        [_view showSnapshot:_measurement.snapshot()];
    }

    // A control on the settings page changed _settings.
    void edited() {
        _module.setSettings(_settings);
        _settings = _module.settings();
        apply();
    }

    void applyPreset(const MeterSettings& preset) {
        MeterSettings s = preset;
        s.appearance = _settings.appearance;
        _module.setSettings(s);
        settingsChanged();
    }

    NSMenu* presetMenu() {
        MeterEditor* editor = this;
        return p31::ui::makePresetMenu(
            p31::editor::presetSource<MeterSettings>(_module, factoryPresets(),
                                                     [editor](const MeterSettings& s) { editor->applyPreset(s); }),
            _root);
    }

    void resetPeaks() {
        _measurement.resetPeaks();
        show();
    }

    void resetLoudness() {
        _measurement.resetLoudness();
        show();
    }

    void setStyle(MeterStyle style) {
        _settings.style = static_cast<int>(style);
        edited();
        [_root reloadSettings];
    }

    // The meter's part of the right-click menu.
    void addMenuItems(NSMenu* menu) {
        MeterEditor* editor = this;
        [menu addItem:[NSMenuItem separatorItem]];
        [menu addItem:[P31MenuItem itemWithTitle:@"Reset Peaks" handler:^{ editor->resetPeaks(); }]];
        [menu addItem:[P31MenuItem itemWithTitle:@"Reset Loudness" handler:^{ editor->resetLoudness(); }]];
        [menu addItem:[NSMenuItem separatorItem]];
        const auto style = [&](NSString* title, MeterStyle value) {
            P31MenuItem* item = [P31MenuItem itemWithTitle:title handler:^{ editor->setStyle(value); }];
            const bool chosen = _settings.style == static_cast<int>(value);
            item.state = chosen ? NSControlStateValueOn : NSControlStateValueOff;
            [menu addItem:item];
        };
        style(@"Digital", MeterStyle::Digital);
        style(@"Analog VU", MeterStyle::Analog);
    }

    Form buildForm();

    void tick(double) {
        if (_params.changed(_plugin)) [_root reloadSettings];
        [_root refreshFreeze];
        [_root snapContentToPixels];
        // The measurement goes on while the picture is frozen.
        _measurement.update();
        if (_plugin.frozen()) return;
        // Once everything has fallen back there is nothing left to animate.
        const bool settled = _measurement.quietSeconds() > kSettleSeconds;
        if (settled && _settledShown) return;
        _settledShown = settled;
        show();
    }

    MeterModule& _module;
    p31::PluginBase& _plugin;   // parameters and source
    MeterMeasurement& _measurement;
    MeterSettings _settings;    // what the settings page edits
    p31::editor::ParamWatch _params;
    bool _settledShown = false;
    P31MeterView* _view = nil;
    P31EditorRootView* _root = nil;
};

Form MeterEditor::buildForm() {
    using namespace p31::ui;
    MeterSettings& s = _settings;
    MeterSettings* p = &s;
    const auto digital = [p] { return p->style == static_cast<int>(MeterStyle::Digital); };
    const auto analog = [p] { return p->style == static_cast<int>(MeterStyle::Analog); };
    const auto dbfsScale = [p] {
        return p->style == static_cast<int>(MeterStyle::Digital) && p->scale == static_cast<int>(ScaleType::Dbfs);
    };
    // Rows that do not apply to the chosen style are not shown.
    const auto onlyIf = [](Row r, std::function<bool()> visible) {
        r.visible = std::move(visible);
        return r;
    };
    Form f;

    f.push_back(section("Display"));
    f.push_back(popup("Style",
                      {{"Digital", static_cast<int>(MeterStyle::Digital)}, {"Analog VU", static_cast<int>(MeterStyle::Analog)}},
                      [p] { return p->style; }, [p](int v) { p->style = v; },
                      "Bars with loudness and stereo figures, or moving-coil needle faces."));
    f.push_back(onlyIf(popup("Orientation",
                             {{"Auto", static_cast<int>(Orientation::Auto)},
                              {"Vertical", static_cast<int>(Orientation::Vertical)},
                              {"Horizontal", static_cast<int>(Orientation::Horizontal)}},
                             [p] { return p->orientation; }, [p](int v) { p->orientation = v; },
                             "Auto runs the bars along the editor's longer side."),
                       digital));
    {
        static constexpr const char* kSizeNames[] = {"Small", "Medium", "Large", "Extra large"};
        std::vector<std::pair<std::string, int>> sizes;
        for (size_t i = 0; i < std::size(kTextSizes); ++i) {
            sizes.emplace_back(std::string(kSizeNames[i]) + " (" + std::to_string(kTextSizes[i]) + " pt)", kTextSizes[i]);
        }
        f.push_back(onlyIf(popup("Text size", sizes, [p] { return p->textSize; }, [p](int v) { p->textSize = v; },
                                 "Readouts, scales, labels and loudness figures."),
                           digital));
    }
    f.push_back(onlyIf(popup("Bars",
                             {{"Peak + RMS", static_cast<int>(BarMode::PeakAndRms)},
                              {"Peak", static_cast<int>(BarMode::Peak)},
                              {"RMS", static_cast<int>(BarMode::Rms)}},
                             [p] { return p->barMode; }, [p](int v) { p->barMode = v; },
                             "Peak + RMS: the wide bar is RMS, the slim column above it the peak."),
                       digital));
    f.push_back(onlyIf(popup("Scale",
                             {{"dBFS", static_cast<int>(ScaleType::Dbfs)},
                              {"K-20", static_cast<int>(ScaleType::K20)},
                              {"K-14", static_cast<int>(ScaleType::K14)},
                              {"K-12", static_cast<int>(ScaleType::K12)}},
                             [p] { return p->scale; }, [p](int v) { p->scale = v; },
                             "K-system: 0 at -20 / -14 / -12 dBFS; green below 0, yellow to +4, red above."),
                       digital));
    f.push_back(onlyIf(popup("Scale spacing",
                             {{"Extended top", static_cast<int>(ScaleSpacing::Extended)},
                              {"Linear", static_cast<int>(ScaleSpacing::Linear)}},
                             [p] { return p->spacing; }, [p](int v) { p->spacing = v; },
                             "Extended gives the top 20 dB half the bar."),
                       digital));
    {
        std::vector<std::pair<std::string, int>> floors;
        for (int db : kFloorChoices) floors.emplace_back(std::to_string(db) + " dB", db);
        f.push_back(onlyIf(popup("Range", floors, [p] { return p->floorDb; }, [p](int v) { p->floorDb = v; },
                                 "Level at the bottom of the bars."),
                           digital));
    }
    f.push_back(onlyIf(checkbox("LED segments", [p] { return p->segments; }, [p](bool v) { p->segments = v; },
                                "Bars split into segments like an LED meter."),
                       digital));

    f.push_back(onlyIf(section("Show"), digital));
    f.push_back(onlyIf(checkbox("Readouts", [p] { return p->showReadouts; }, [p](bool v) { p->showReadouts = v; },
                                "Highest peak above each bar (click to reset) and the loudness values."),
                       digital));
    f.push_back(onlyIf(checkbox("Loudness", [p] { return p->showLoudness; }, [p](bool v) { p->showLoudness = v; },
                                "Momentary and short-term bars; integrated, range, true peak and PLR."),
                       digital));
    f.push_back(onlyIf(checkbox("Correlation", [p] { return p->showCorrelation; },
                                [p](bool v) { p->showCorrelation = v; }, "+1 mono, 0 wide, below 0 out of phase."),
                       digital));
    f.push_back(onlyIf(checkbox("Balance", [p] { return p->showBalance; }, [p](bool v) { p->showBalance = v; },
                                "Which channel is louder; full scale at 12 dB."),
                       digital));

    f.push_back(section("Levels"));
    f.push_back(checkbox("True peak (4x oversampled)", [p] { return p->truePeak; }, [p](bool v) { p->truePeak = v; },
                         "Catches peaks between samples, which can pass 0 dBFS after conversion."));
    {
        Row r = slider("RMS window", kMinRmsMs, kMaxRmsMs, [p] { return p->rmsMs; },
                       [p](double v) { p->rmsMs = static_cast<int>(v); }, [](double v) { return fmt("%.0f ms", v); },
                       "Time the RMS level is averaged over.");
        r.step = 10;
        r.visible = digital;
        r.enabled = [p] { return p->barMode != static_cast<int>(BarMode::Peak); };
        f.push_back(r);
    }
    {
        Row r = checkbox("RMS +3 dB", [p] { return p->rmsSinePeak; }, [p](bool v) { p->rmsSinePeak = v; },
                         "A full-scale sine reads 0 dB on both peak and RMS (AES17).");
        r.visible = digital;
        r.enabled = [p] { return p->barMode != static_cast<int>(BarMode::Peak); };
        f.push_back(r);
    }
    f.push_back(onlyIf(slider("Peak fall", kMinPeakFallTenths, kMaxPeakFallTenths, [p] { return p->peakFallTenths; },
                              [p](double v) { p->peakFallTenths = static_cast<int>(v); },
                              [](double v) { return fmt("%.1f dB/s", v / 10.0); },
                              "11.8 dB/s is the usual digital meter rate (20 dB in 1.7 s)."),
                       digital));
    {
        Row r = slider("Peak hold", 0, kMaxPeakHoldMs, [p] { return p->peakHoldMs; },
                       [p](double v) { p->peakHoldMs = static_cast<int>(v); },
                       [](double v) { return v < 50 ? std::string("Off") : fmt("%.1f s", v / 1000.0); },
                       "How long the hold line stays before it falls.");
        r.step = 100;
        r.visible = digital;
        r.enabled = [p] { return !p->holdForever; };
        f.push_back(r);
    }
    f.push_back(onlyIf(checkbox("Hold peaks until clicked", [p] { return p->holdForever; },
                                [p](bool v) { p->holdForever = v; }),
                       digital));
    f.push_back(onlyIf(slider("Warning from", kMinZoneDb, 0, [p] { return p->warnDb; },
                              [p](double v) { p->warnDb = static_cast<int>(v); },
                              [](double v) { return fmt("%.0f dBFS", v); }),
                       dbfsScale));
    f.push_back(onlyIf(slider("Over from", kMinZoneDb, 0, [p] { return p->overDb; },
                              [p](double v) { p->overDb = static_cast<int>(v); },
                              [](double v) { return fmt("%.0f dBFS", v); },
                              "Where the bars turn the warning and over colors (dBFS scale)."),
                       dbfsScale));

    f.push_back(onlyIf(section("Loudness"), digital));
    f.push_back(onlyIf(popup("Target",
                             {{"-9 LUFS (club, loud masters)", -9},
                              {"-11 LUFS", -11},
                              {"-14 LUFS (streaming)", -14},
                              {"-16 LUFS (podcasts, Apple)", -16},
                              {"-18 LUFS", -18},
                              {"-23 LUFS (EBU R128)", -23},
                              {"-24 LUFS (ATSC A/85)", -24}},
                             [p] { return p->targetLufs; }, [p](int v) { p->targetLufs = v; },
                             "Marker on the loudness bars; readings above it turn the warning color."),
                       digital));

    f.push_back(onlyIf(section("Analog"), analog));
    f.push_back(onlyIf(popup("Face",
                             {{"Vintage", static_cast<int>(FaceStyle::Vintage)},
                              {"Dark", static_cast<int>(FaceStyle::Dark)},
                              {"Theme colors", static_cast<int>(FaceStyle::Theme)}},
                             [p] { return p->face; }, [p](int v) { p->face = v; },
                             "The Vintage and Dark faces keep their own colors."),
                       analog));
    f.push_back(onlyIf(popup("Meters",
                             {{"Auto", static_cast<int>(NeedleLayout::Auto)},
                              {"Two faces", static_cast<int>(NeedleLayout::Pair)},
                              {"One face, two needles", static_cast<int>(NeedleLayout::Single)}},
                             [p] { return p->needleLayout; }, [p](int v) { p->needleLayout = v; },
                             "Auto puts two faces side by side or stacked, whichever is bigger."),
                       analog));
    f.push_back(onlyIf(popup("Ballistics",
                             {{"VU (300 ms)", static_cast<int>(NeedleBallistics::VU)},
                              {"PPM, DIN style", static_cast<int>(NeedleBallistics::PpmDin)},
                              {"PPM, BBC / EBU style", static_cast<int>(NeedleBallistics::PpmBbc)},
                              {"PPM, Nordic style", static_cast<int>(NeedleBallistics::PpmNordic)}},
                             [p] { return p->needle; }, [p](int v) { p->needle = v; },
                             "VU averages over 300 ms; PPMs rise fast and fall slowly."),
                       analog));
    f.push_back(onlyIf(slider("Reference", kMinReferenceDb, kMaxReferenceDb, [p] { return p->referenceDb; },
                              [p](double v) { p->referenceDb = static_cast<int>(v); },
                              [](double v) { return fmt("0 VU = %.0f dBFS", v); },
                              "The sine level that reads 0 VU. Around -10 dBFS suits loud modern masters."),
                       analog));
    f.push_back(onlyIf(checkbox("Peak LED", [p] { return p->peakLed; }, [p](bool v) { p->peakLed = v; }), analog));
    {
        Row r = slider("Peak LED at", kMinPeakLedDb, 0, [p] { return p->peakLedDb; },
                       [p](double v) { p->peakLedDb = static_cast<int>(v); },
                       [](double v) { return fmt("%.0f dBFS", v); });
        r.visible = analog;
        r.enabled = [p] { return p->peakLed; };
        f.push_back(r);
    }

    f.push_back(section("Input"));
    f.push_back(p31::editor::inputSourceRow(_plugin));
    f.push_back(p31::editor::inputGainRow(_plugin));
    f.push_back(p31::editor::freezeRow(_plugin));

    f.push_back(section("Appearance"));
    f.push_back(p31::editor::appearanceRow(s.appearance));
    f.push_back(popup("Theme", p31::editor::themeChoices(kMeterThemes, kMeterThemeMenuOrder), [p] { return p->theme; },
                      [p](int v) {
                          if (v == 0) p->theme = 0;
                          else applyTheme(*p, v);
                      },
                      "A color preset for the levels, the loudness bars, text and background"));
    for (int i = 0; i < kColorCount; ++i) f.push_back(colorPair(kColorNames[i], s.light[i], s.dark[i]));
    return f;
}

} // namespace

std::unique_ptr<p31::Editor> makeMeterEditor(MeterModule& module, MeterMeasurement& measurement, uint32_t width,
                                             uint32_t height) {
    return std::make_unique<MeterEditor>(module, measurement, width, height);
}
