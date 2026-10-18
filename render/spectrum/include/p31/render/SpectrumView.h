//
//  SpectrumView.h
//  p31-render (ported from foo_jl_spectrum_mac SpectrumView.h)
//
//  Core Graphics view that draws the spectrum as bars or a curve, with the
//  shadow layer, peak caps or line, dB guides and frequency labels. Hold the
//  left mouse button to read the frequency under the cursor. Objective-C++
//  only.
//

#pragma once

#import "p31/render/AnimatedView.h"

#include <p31/spectrum/SpectrumSettings.h>

NS_ASSUME_NONNULL_BEGIN

@interface P31SpectrumView : P31AnimatedView

// Apply display settings (colors, style, layout, axis range).
- (void)applySettings:(const p31::spectrum::SpectrumSettings &)settings;

// Bar count that follows the view size: in Curve mode one curve point per
// kCurvePointPitch points along the frequency axis, otherwise ("Auto" bars)
// one bar per kAutoBarPitch points.
- (NSInteger)autoBarCount;

// Latest bar levels, shadow fill, peak positions and, in Mid + side mode,
// the side channel (NULL otherwise); each 0..1 with `count` entries.
// Redraws.
- (void)setBars:(const float *)bars
         shadow:(const float *)shadow
          peaks:(const float *)peaks
           side:(const float * _Nullable)side
          count:(NSInteger)count;

@end

NS_ASSUME_NONNULL_END
