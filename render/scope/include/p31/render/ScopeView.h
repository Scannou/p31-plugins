//
//  ScopeView.h
//  p31-render (ported from foo_jl_vectorscope_mac VectorscopeView.h)
//
//  Goniometer view: grid and correlation bars drawn with Core Graphics, the
//  afterglow trace composited on top as a layer image. Objective-C++ only.
//

#pragma once

#import "p31/render/AnimatedView.h"

#include <p31/scope/ScopeSettings.h>

NS_ASSUME_NONNULL_BEGIN

@interface P31ScopeView : P31AnimatedView

// Apply display settings (colors, trace style, layout).
- (void)applySettings:(const p31::scope::ScopeSettings &)settings;

// Advance the afterglow by `dt` seconds and add `count` goniometer points
// (interleaved x/y, -1..1, y up; may be 0 with a NULL pointer). `bands`
// holds the same points split into low / mid / high and is drawn instead of
// `xy` when the trace is coloured by band.
// Returns NO once the trace has fully faded and nothing is left to animate.
- (BOOL)advanceTrace:(nullable const float *)xy
               bands:(const float * _Nullable const * _Nullable)bands
               count:(NSInteger)count
          sampleRate:(double)sampleRate
           deltaTime:(double)dt;

// Latest correlation readings (-1..+1). `bands` holds low / mid / high and
// is only shown in the three-band layout. The bars ease towards the new
// values on the next advanceTrace:.
- (void)setCorrelation:(float)wide
                silent:(BOOL)wideSilent
                 bands:(const float *)bands
           bandsSilent:(const BOOL *)bandsSilent;

// Drop the trace and reset the bars.
- (void)clearTrace;

@end

NS_ASSUME_NONNULL_END
