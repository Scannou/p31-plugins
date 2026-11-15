//
//  PixelSnap.h
//  p31-render (ported from fb2k-components-mac-suite shared/PixelSnap.h)
//
//  Keeps an editor's drawing view on whole device pixels.
//
//  A host can place the editor between pixels. A view that starts between
//  pixels is resampled by the window server on every frame and looks soft.
//  The editor's container keeps whatever frame the host gives it; its drawing
//  view is placed inside on whole pixels, each edge rounded inward, so at
//  most one pixel of the container shows at an edge.
//
//  Call pixelSnapContent from the container's layout and from the frame
//  callback: a host can move the editor without resizing it, and only the
//  frame callback notices that. It is cheap and does nothing when already
//  aligned. Objective-C++ only.
//

#pragma once

#import <Cocoa/Cocoa.h>

#include <cmath>

NS_ASSUME_NONNULL_BEGIN

namespace p31::ui {

// The largest rect inside `container`'s bounds whose edges lie on whole
// device pixels of its window, in `container`'s coordinates.
inline NSRect pixelSnappedRect(NSView* container) {
    NSWindow* window = container.window;
    const NSRect bounds = container.bounds;
    if (!window || NSIsEmptyRect(bounds)) return bounds;

    const NSRect px = [window convertRectToBacking:[container convertRect:bounds toView:nil]];
    // Tolerance, so an edge a hair off a pixel boundary is not pulled in a
    // whole pixel by floating-point noise.
    const CGFloat eps = 0.01;
    const CGFloat x0 = std::ceil(NSMinX(px) - eps), y0 = std::ceil(NSMinY(px) - eps);
    const CGFloat x1 = std::floor(NSMaxX(px) + eps), y1 = std::floor(NSMaxY(px) + eps);
    if (x1 <= x0 || y1 <= y0) return bounds;

    const NSRect snapped = NSMakeRect(x0, y0, x1 - x0, y1 - y0);
    return [container convertRect:[window convertRectFromBacking:snapped] fromView:nil];
}

// Place `content` (a subview of `container`) on whole device pixels.
inline void pixelSnapContent(NSView* _Nullable container, NSView* _Nullable content) {
    if (!container || !content) return;
    const NSRect r = pixelSnappedRect(container);
    const NSRect f = content.frame;
    if (std::fabs(NSMinX(f) - NSMinX(r)) < 0.001 && std::fabs(NSMinY(f) - NSMinY(r)) < 0.001 &&
        std::fabs(NSWidth(f) - NSWidth(r)) < 0.001 && std::fabs(NSHeight(f) - NSHeight(r)) < 0.001) {
        return;
    }
    content.frame = r;
}

} // namespace p31::ui

NS_ASSUME_NONNULL_END
