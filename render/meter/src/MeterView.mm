//
//  MeterView.mm
//  p31-render (ported from foo_jl_levelmeter_mac LevelMeterView.mm)
//

#import "p31/render/MeterView.h"

#import "p31/render/MeterRenderer.h"

// Transparent view over the meter for the parts that change every frame.
@interface P31MeterLiveView : NSView
@property (nonatomic, copy, nullable) void (^drawHandler)(void);
@end

@implementation P31MeterLiveView
- (BOOL)isFlipped { return YES; }
- (BOOL)isOpaque { return NO; }
- (NSView *)hitTest:(NSPoint)point { return nil; }  // clicks go to the meter view
- (void)drawRect:(NSRect)dirtyRect {
    if (self.drawHandler) self.drawHandler();
}
@end

// The view draws the background, scales and names, which only change with
// the settings or the size; the live view above it draws the readings.
// Redrawing the scale text every frame cost more main-thread time than the
// readings themselves.
@implementation P31MeterView {
    p31::meter::MeterRenderer _renderer;
    p31::meter::MeterSettings _settings;
    p31::meter::DisplaySettings _display;
    p31::meter::Snapshot _snapshot;
    P31MeterLiveView *_liveView;
}

- (instancetype)initWithFrame:(NSRect)frameRect {
    self = [super initWithFrame:frameRect];
    if (self) {
        self.wantsLayer = YES;
        self.layerContentsRedrawPolicy = NSViewLayerContentsRedrawOnSetNeedsDisplay;

        _liveView = [[P31MeterLiveView alloc] initWithFrame:self.bounds];
        _liveView.autoresizingMask = NSViewWidthSizable | NSViewHeightSizable;
        _liveView.wantsLayer = YES;
        _liveView.layerContentsRedrawPolicy = NSViewLayerContentsRedrawOnSetNeedsDisplay;
        __weak P31MeterView *weakSelf = self;
        _liveView.drawHandler = ^{ [weakSelf drawPass:p31::meter::Pass::Live]; };
        [self addSubview:_liveView];

        [self reloadDisplay];
    }
    return self;
}

- (BOOL)isFlipped { return YES; }   // the renderer works top-down
- (BOOL)isOpaque { return NO; }

- (void)applySettings:(const p31::meter::MeterSettings &)settings {
    _settings = settings.sanitized();
    [self reloadDisplay];
}

- (void)reloadDisplay {
    _display = p31::meter::displaySettings(_settings, [self isDarkAppearance]);
    [self setNeedsDisplay:YES];
    [_liveView setNeedsDisplay:YES];
}

- (void)showSnapshot:(const p31::meter::Snapshot &)snapshot {
    _snapshot = snapshot;
    [_liveView setNeedsDisplay:YES];
}

- (void)setFrameSize:(NSSize)newSize {
    [super setFrameSize:newSize];
    [self setNeedsDisplay:YES];
    [_liveView setNeedsDisplay:YES];
}

- (void)drawRect:(NSRect)dirtyRect {
    [self drawPass:p31::meter::Pass::Static];
}

- (void)drawPass:(p31::meter::Pass)pass {
    CGContextRef ctx = [[NSGraphicsContext currentContext] CGContext];
    _renderer.draw(ctx, self.bounds, _display, _snapshot, pass);
}

- (void)viewDidChangeEffectiveAppearance {
    [super viewDidChangeEffectiveAppearance];
    [self reloadDisplay];
}

#pragma mark - Mouse

- (BOOL)acceptsFirstMouse:(NSEvent *)event { return YES; }
- (BOOL)mouseDownCanMoveWindow { return NO; }

- (void)mouseDown:(NSEvent *)event {
    if (event.modifierFlags & NSEventModifierFlagControl) {
        [self rightMouseDown:event];  // ctrl-click opens the context menu
        return;
    }
    const NSPoint p = [self convertPoint:event.locationInWindow fromView:nil];
    switch (_renderer.hitTest(p)) {
        case p31::meter::HitArea::Peaks:
            if (self.onResetPeaks) self.onResetPeaks();
            break;
        case p31::meter::HitArea::Loudness:
            if (self.onResetLoudness) self.onResetLoudness();
            break;
        default:
            [super mouseDown:event];
            break;
    }
}

@end
