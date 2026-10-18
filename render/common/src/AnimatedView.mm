//
//  AnimatedView.mm
//  p31-render
//

#import "p31/render/AnimatedView.h"

#import <QuartzCore/QuartzCore.h>

// CADisplayLink retains its target; this weak trampoline keeps the link from
// holding the view alive.
@interface P31DisplayLinkTarget : NSObject
@property (nonatomic, weak) P31AnimatedView *view;
- (void)fire:(id)link;
@end

@interface P31AnimatedView ()
- (void)frameAt:(double)timestamp;
@end

@implementation P31DisplayLinkTarget
- (void)fire:(id)link {
    double t = CACurrentMediaTime();
    if (@available(macOS 14.0, *)) t = ((CADisplayLink *)link).timestamp;
    [self.view frameAt:t];
}
@end

namespace {
    // Shortest time between frames. On fast displays this takes every second
    // (or third) refresh: an even 60-90 fps instead of drawing at up to 240 Hz.
    constexpr double kMinFrameInterval = 0.0105;
}

@implementation P31AnimatedView {
    id _displayLink;   // CADisplayLink on macOS 14+
    NSTimer *_timer;
    double _lastFrame;
    BOOL _stopped;
}

- (void)dealloc {
    [self stopTimer];
}

- (void)stopAnimating {
    _stopped = YES;
    self.onFrame = nil;
    [self stopTimer];
}

- (BOOL)isDarkAppearance {
    NSAppearanceName match = [self.effectiveAppearance
        bestMatchFromAppearancesWithNames:@[NSAppearanceNameAqua, NSAppearanceNameDarkAqua]];
    return [match isEqualToString:NSAppearanceNameDarkAqua];
}

- (void)viewDidMoveToWindow {
    [super viewDidMoveToWindow];
    if (self.window) [self startTimer];
    else [self stopTimer];
}

- (void)startTimer {
    [self stopTimer];
    if (_stopped || !self.window) return;
    _lastFrame = 0;

    // A display link fires in step with the screen's refresh, so each shown
    // frame carries exactly one update; a free-running timer drifts against
    // the refresh and shows repeated and skipped frames.
    P31DisplayLinkTarget *target = [[P31DisplayLinkTarget alloc] init];
    target.view = self;
    if (@available(macOS 14.0, *)) {
        CADisplayLink *link = [self displayLinkWithTarget:target selector:@selector(fire:)];
        link.preferredFrameRateRange = CAFrameRateRangeMake(60, 120, 60);
        [link addToRunLoop:[NSRunLoop mainRunLoop] forMode:NSRunLoopCommonModes];
        _displayLink = link;
        return;
    }
    _timer = [NSTimer timerWithTimeInterval:1.0 / 60.0
                                     target:target
                                   selector:@selector(fire:)
                                   userInfo:nil
                                    repeats:YES];
    [[NSRunLoop mainRunLoop] addTimer:_timer forMode:NSRunLoopCommonModes];
}

- (void)stopTimer {
    if (@available(macOS 14.0, *)) [(CADisplayLink *)_displayLink invalidate];
    _displayLink = nil;
    [_timer invalidate];
    _timer = nil;
}

- (void)frameAt:(double)timestamp {
    if (_lastFrame > 0 && timestamp - _lastFrame < kMinFrameInterval) return;
    _lastFrame = timestamp;

    NSWindow *window = self.window;
    if (!window || self.isHiddenOrHasHiddenAncestor) return;
    if (!(window.occlusionState & NSWindowOcclusionStateVisible)) return;

    @autoreleasepool {
        if (self.onFrame) self.onFrame(timestamp);
    }
}

@end
