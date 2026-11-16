//
//  SpectrogramView.h
//  p31-render (ported from foo_jl_spectrogram_mac SpectrogramView.h)
//
//  Scrolling spectrogram: columns of color-mapped levels enter at one edge
//  and move across the plot as Core Animation tile layers. Frequency axis,
//  gridlines, and the dB color scale are drawn on top. Click and hold to
//  read the frequency and note under the cursor. Objective-C++ only.
//

#pragma once

#import "p31/render/AnimatedView.h"

#include <p31/spectrogram/SpectrogramSettings.h>

NS_ASSUME_NONNULL_BEGIN

@interface P31SpectrogramView : P31AnimatedView

// Apply display settings (colormap, axis, scale, layout). Another frequency
// mapping or layout clears the picture.
- (void)applySettings:(const p31::spectrogram::SpectrogramSettings &)settings;

// Pixel rows each column must have: the plot's length along the frequency
// axis in device pixels. Held at the previous value while a resize is in
// progress (the existing image stretches meanwhile), so call this every frame.
- (NSInteger)plotRows;

// Columns per point of travel (the backing scale factor).
- (CGFloat)columnsPerPoint;

// Add one column at the edge where new audio enters. `levels` holds `rows`
// values in 0..1, highest frequency first. Call flushWithPhase: once per
// frame afterwards.
- (void)appendColumn:(const float *)levels rows:(NSInteger)rows;

// Put the columns appended since the last flush on screen, shifted by
// `phase` columns (0..1): how far time has moved past the newest column.
// Call every frame, also when no column was added, for even motion.
- (void)flushWithPhase:(CGFloat)phase;

// Drop everything drawn so far.
- (void)clear;

// Width of the labels right of the plot (frequency axis, dB scale).
- (CGFloat)plotInsetRight;

@end

NS_ASSUME_NONNULL_END
