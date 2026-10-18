//
//  SpectrumEditor.mm
//  p31-spectrum (ported from foo_jl_spectrum_mac SpectrumController.mm)
//
//  Owns the spectrum view and, on every display frame, pulls the audio played
//  since the previous frame from the plugin's ring buffer, runs it through the
//  analyzer and hands the bar levels to the view.
//

#include "SpectrumEditor.h"

#include "SpectrumPlugin.h"

#import <p31/render/SpectrumView.h>

#include <p31/core/AudioRing.h>
#include <p31/spectrum/SpectrumAnalyzer.h>

#include <algorithm>
#include <cmath>
#include <vector>

namespace {

using p31::spectrum::SpectrumAnalyzer;

// Longest stretch read in one frame. After a stall (window hidden, UI
// blocked) the analyzer resumes near "now" instead of replaying the gap.
constexpr double kMaxWindowSeconds = 0.1;
// Covers kMaxWindowSeconds up to 192 kHz (85 ms at 384 kHz).
constexpr size_t kMaxFramesPerTick = 32768;

class SpectrumEditor final : public p31::Editor {
public:
    SpectrumEditor(const SpectrumPlugin& plugin, uint32_t width, uint32_t height)
        : _plugin(plugin), _reader(plugin.ring()), _settings(plugin.settings().sanitized()) {
        _lr.resize(kMaxFramesPerTick * 2);

        _view = [[P31SpectrumView alloc] initWithFrame:NSMakeRect(0, 0, width, height)];
        _view.autoresizingMask = NSViewWidthSizable | NSViewHeightSizable;
        [_view applySettings:_settings];
        _analyzer.configure(p31::spectrum::analyzerSettings(_settings, static_cast<int>([_view autoBarCount])));
        pushToView();
        SpectrumEditor* editor = this;
        _view.onFrame = ^(double timestamp) { editor->tick(timestamp); };
    }

    ~SpectrumEditor() override {
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
        if (visible) {
            // Start clean rather than from levels frozen when it was hidden.
            _reader.skipToNow();
            _analyzer.reset();
            pushToView();
        }
    }

private:
    // In Curve mode and with "Auto" bars the bar count follows the view.
    void syncBarCount() {
        if (!_settings.viewSizedBars()) return;
        const int count = static_cast<int>([_view autoBarCount]);
        if (count != _analyzer.settings().barCount) {
            _analyzer.configure(p31::spectrum::analyzerSettings(_settings, count));
        }
    }

    void tick(double now) {
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

    const SpectrumPlugin& _plugin;
    p31::AudioRingReader _reader;
    p31::spectrum::SpectrumSettings _settings;
    SpectrumAnalyzer _analyzer;
    std::vector<float> _lr;   // interleaved L/R read this frame
    double _lastTick = 0.0;
    bool _wasActive = false;
    P31SpectrumView* _view = nil;
};

} // namespace

std::unique_ptr<p31::Editor> makeSpectrumEditor(const SpectrumPlugin& plugin, uint32_t width, uint32_t height) {
    return std::make_unique<SpectrumEditor>(plugin, width, height);
}
