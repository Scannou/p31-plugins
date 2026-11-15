//
//  ThemeColor.h
//  p31-render (ported from fb2k-components-mac-suite shared/ThemeColor.h)
//
//  Theme colours stored as 0xAARRGGBB, read in the Display P3 colour space.
//
//  On a wide-gamut screen P3 values look more saturated than the same
//  numbers read as sRGB; on an sRGB screen the system maps them back, so
//  nothing is lost there. Use these helpers for every stored colour (drawing,
//  colour wells, gradients) so what is picked is what is drawn.
//  Objective-C++ only.
//

#pragma once

#import <Cocoa/Cocoa.h>

#include <algorithm>
#include <cmath>
#include <cstdint>

NS_ASSUME_NONNULL_BEGIN

namespace p31::ui {

inline NSColor* colorFromARGB(uint32_t argb) {
    return [NSColor colorWithDisplayP3Red:((argb >> 16) & 0xFF) / 255.0
                                    green:((argb >> 8) & 0xFF) / 255.0
                                     blue:(argb & 0xFF) / 255.0
                                    alpha:((argb >> 24) & 0xFF) / 255.0];
}

// Opaque black when the colour cannot be expressed in Display P3.
inline uint32_t argbFromColor(NSColor* color) {
    NSColor* c = [color colorUsingColorSpace:[NSColorSpace displayP3ColorSpace]];
    if (!c) return 0xFF000000u;
    const auto byte = [](CGFloat v) { return static_cast<uint32_t>(std::lround(std::clamp(v, 0.0, 1.0) * 255.0)); };
    return (byte(c.alphaComponent) << 24) | (byte(c.redComponent) << 16) | (byte(c.greenComponent) << 8) |
           byte(c.blueComponent);
}

// Colour space for gradients and bitmaps built from theme colours, so they
// keep the P3 saturation instead of being clipped to sRGB. Caller releases.
inline CGColorSpaceRef createThemeColorSpace() {
    return CGColorSpaceCreateWithName(kCGColorSpaceDisplayP3);
}

} // namespace p31::ui

NS_ASSUME_NONNULL_END
