//
//  MeterView.h
//  p31-render (ported from foo_jl_levelmeter_mac LevelMeterView.h)
//
//  Flipped view that draws the latest snapshot through MeterRenderer and
//  reports clicks on the bars and figures (reset). Objective-C++ only.
//

#pragma once

#import "p31/render/AnimatedView.h"

#include <p31/meter/MeterEngine.h>
#include <p31/meter/MeterSettings.h>

NS_ASSUME_NONNULL_BEGIN

@interface P31MeterView : P31AnimatedView

// Apply display settings (the colors follow the appearance).
- (void)applySettings:(const p31::meter::MeterSettings &)settings;

// Show a new set of readings.
- (void)showSnapshot:(const p31::meter::Snapshot &)snapshot;

// A click on the level bars, readouts, clip lights or an analog face.
@property (nonatomic, copy, nullable) void (^onResetPeaks)(void);
// A click on the loudness bars or figures.
@property (nonatomic, copy, nullable) void (^onResetLoudness)(void);

@end

NS_ASSUME_NONNULL_END
