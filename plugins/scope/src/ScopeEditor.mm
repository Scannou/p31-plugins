//
//  ScopeEditor.mm
//  p31-scope (ported from foo_jl_vectorscope_mac VectorscopeController.mm)
//
//  Owns the scope view and, on every display frame, pulls the audio played
//  since the previous frame from the plugin's ring buffer, analyses it and
//  hands points and correlation readings to the view.
//

#include "ScopeEditor.h"

#include "ScopePlugin.h"

#import <p31/render/ScopeView.h>

#include <p31/core/AudioRing.h>
#include <p31/scope/StereoAnalysis.h>

#include <algorithm>
#include <cmath>
#include <vector>

namespace {

using p31::stereo::StereoAnalyzer;

// Longest stretch analysed in one frame. After a stall (window hidden, UI
// blocked) the scope resumes at "now" instead of replaying the gap.
constexpr double kMaxWindowSeconds = 0.05;
// Covers kMaxWindowSeconds up to 384 kHz.
constexpr size_t kMaxFramesPerTick = 32768;
// Blocks quieter than this (-80 dBFS) leave the trace alone. A stopped DAW
// keeps sending digital silence, which would otherwise burn a dot into the
// centre instead of letting the afterglow fade as it does in foobar2000.
constexpr float kTraceGate = 1e-4f;

class ScopeEditor final : public p31::Editor {
public:
    ScopeEditor(const ScopePlugin& plugin, uint32_t width, uint32_t height)
        : _plugin(plugin), _reader(plugin.ring()) {
        _lr.resize(kMaxFramesPerTick * 2);
        _xy.resize(kMaxFramesPerTick * 2);

        const p31::scope::ScopeSettings& s = plugin.settings();
        applyAnalyzerSettings(s);

        _view = [[P31ScopeView alloc] initWithFrame:NSMakeRect(0, 0, width, height)];
        _view.autoresizingMask = NSViewWidthSizable | NSViewHeightSizable;
        [_view applySettings:s];
        ScopeEditor* editor = this;
        _view.onFrame = ^(double timestamp) { editor->tick(timestamp); };
    }

    ~ScopeEditor() override {
        [_view stopAnimating];
        [_view removeFromSuperview];
        _view = nil;
    }

    bool attach(void* parent) override {
        NSView* parentView = (__bridge NSView*)parent;
        if (parentView == nil) return false;
        [parentView addSubview:_view];
        _reader.skipToNow();
        return true;
    }

    void setSize(uint32_t width, uint32_t height) override {
        [_view setFrameSize:NSMakeSize(width, height)];
    }

    void setVisible(bool visible) override {
        _view.hidden = !visible;
        if (visible) _reader.skipToNow();
    }

private:
    void applyAnalyzerSettings(const p31::scope::ScopeSettings& settings) {
        using namespace p31::scope;
        const ScopeSettings s = settings.sanitized();
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

    void tick(double now) {
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

    const ScopePlugin& _plugin;
    p31::AudioRingReader _reader;
    StereoAnalyzer _analyzer;
    std::vector<float> _lr;   // interleaved L/R read this frame
    std::vector<float> _xy;   // its goniometer points
    double _lastTick = 0.0;
    P31ScopeView* _view = nil;
};

} // namespace

std::unique_ptr<p31::Editor> makeScopeEditor(const ScopePlugin& plugin, uint32_t width, uint32_t height) {
    return std::make_unique<ScopeEditor>(plugin, width, height);
}
