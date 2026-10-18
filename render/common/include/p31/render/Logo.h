//
//  Logo.h
//  p31-render
//
//  The P31 logo (assets/brand/p31-logo/outline) as vector paths, drawn in a
//  single colour as the brand README asks: the theme decides the colour.
//  Objective-C++ only.
//

#pragma once

#import <Cocoa/Cocoa.h>

#include "p31/render/ObjCNames.h"

#include <cstdint>

NS_ASSUME_NONNULL_BEGIN

namespace p31::ui {

enum class LogoKind {
    Lockup,   // small mark + "P31", for 24 pt tall headers
    Mark,     // ring + figure, for 48 pt and up
};

// The logo's outline as one path in its SVG's coordinates (y down), and
// that SVG's size. Parsed once; owned by the cache, do not release.
CGPathRef logoPath(LogoKind kind, CGSize* size);

// The logo at `height` points, filled with `color`.
NSImage* logoImage(LogoKind kind, CGFloat height, NSColor* color);

} // namespace p31::ui

// Draws a logo scaled to fit its bounds (aspect kept, centred vertically,
// left-aligned), in the light or dark colour for its effective appearance.
@interface P31LogoView : NSView
- (instancetype)initWithKind:(p31::ui::LogoKind)kind;
@property (nonatomic) uint32_t lightColor;   // ARGB
@property (nonatomic) uint32_t darkColor;    // ARGB
// Clicked; the cursor becomes a pointing hand while this is set.
@property (nonatomic, copy, nullable) void (^onClick)(void);
// Width that keeps the logo's aspect ratio at `height`.
- (CGFloat)widthForHeight:(CGFloat)height;
@end

NS_ASSUME_NONNULL_END
