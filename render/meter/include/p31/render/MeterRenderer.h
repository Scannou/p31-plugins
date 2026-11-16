//
//  MeterRenderer.h
//  p31-render (ported from foo_jl_levelmeter_mac MeterRenderer.h)
//
//  Draws the meter into the current NSGraphicsContext. Coordinates are
//  top-left origin: call it from a flipped view (or a flipped context).
//  Objective-C++ only.
//

#pragma once

#import <Cocoa/Cocoa.h>

#include <p31/meter/DisplaySettings.h>
#include <p31/meter/MeterEngine.h>
#include <p31/meter/MeterLayout.h>

namespace p31::meter {

// Which part of the meter a click landed on.
enum class HitArea {
    None,
    Peaks,      // readouts, clip lights, level bars: reset maxima
    Loudness,   // loudness bars and figures: reset the measurement
};

// What a draw call covers. Static is what only changes with the settings or
// the size (background, scales, names); Live is the rest, drawn over it.
enum class Pass {
    All,
    Static,
    Live,
};

class MeterRenderer {
public:
    // Draw `pass` inside `bounds`.
    void draw(CGContextRef ctx, CGRect bounds, const DisplaySettings &s, const Snapshot &snap,
              Pass pass = Pass::All);

    // What a click at `p` would reset (uses the last layout drawn).
    HitArea hitTest(CGPoint p) const;

private:
    void drawDigital(CGContextRef ctx, CGRect bounds, const DisplaySettings &s, const Snapshot &snap, Pass pass);
    void drawAnalog(CGContextRef ctx, CGRect bounds, const DisplaySettings &s, const Snapshot &snap);

    meter_layout::Result _layout;
    bool _analog = false;
    CGRect _bounds = CGRectZero;
};

} // namespace p31::meter
