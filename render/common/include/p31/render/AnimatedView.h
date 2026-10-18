//
//  AnimatedView.h
//  p31-render
//
//  NSView base class that calls `onFrame` once per display refresh while it
//  is on screen: a CADisplayLink on macOS 14+, a 60 Hz timer before that.
//  Frames stop while the view has no window, is hidden, or its window is
//  fully covered, so an editor nobody looks at costs nothing.
//

#pragma once

#import <Cocoa/Cocoa.h>

#include "p31/render/ObjCNames.h"

NS_ASSUME_NONNULL_BEGIN

@interface P31AnimatedView : NSView

// Called on the main thread with the frame's timestamp in seconds
// (CACurrentMediaTime clock).
@property (nonatomic, copy, nullable) void (^onFrame)(double timestamp);

// Stop the frame callbacks for good (call before releasing the view).
- (void)stopAnimating;

// The view's effective appearance is a dark one.
- (BOOL)isDarkAppearance;

@end

NS_ASSUME_NONNULL_END
