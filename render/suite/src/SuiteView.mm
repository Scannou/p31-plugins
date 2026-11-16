//
//  SuiteView.mm
//  p31-render
//

#import "p31/render/SuiteView.h"

#import "p31/render/EditorRootView.h"

#include <algorithm>
#include <cmath>
#include <optional>

using p31::suite::Edge;
using p31::suite::Layout;

namespace {
    constexpr CGFloat kDividerGrab = 7.0;     // width of the strip that takes the mouse
    constexpr CGFloat kEdgeZone = 24.0;       // dropping this close to the window's edge docks along it
    constexpr CGFloat kSwapZone = 0.3;        // dropping this far inside a panel (share of its size) swaps
    constexpr CGFloat kHighlightInset = 4.0;
    constexpr CGFloat kDraggedAlpha = 0.5;

    NSRect rectOf(const p31::suite::Rect &r) { return NSMakeRect(r.x, r.y, r.w, r.h); }
}

#pragma mark - Pane

// Holds one panel's editor and keeps it at its own size.
@interface P31SuitePane : NSView
@end

@implementation P31SuitePane

- (void)didAddSubview:(NSView *)subview {
    [super didAddSubview:subview];
    subview.frame = self.bounds;
}

- (void)resizeSubviewsWithOldSize:(NSSize)oldSize {
    for (NSView *subview in self.subviews) subview.frame = self.bounds;
}

@end

#pragma mark - Divider

// The strip over the gap between two panes. Reports where a drag wants the
// gap's middle, along the split's axis, in the superview's coordinates.
@interface P31SuiteDivider : NSView
@property (nonatomic) BOOL sideBySide;
@property (nonatomic, copy, nullable) void (^onDrag)(CGFloat position);
@end

@implementation P31SuiteDivider {
    CGFloat _grab;   // from the strip's middle to the mouse, at the mouse down
}

- (BOOL)acceptsFirstMouse:(NSEvent *)event { return YES; }
- (BOOL)mouseDownCanMoveWindow { return NO; }

- (void)resetCursorRects {
    [self addCursorRect:self.bounds
                 cursor:_sideBySide ? NSCursor.resizeLeftRightCursor : NSCursor.resizeUpDownCursor];
}

- (CGFloat)position:(NSEvent *)event {
    const NSPoint p = [self.superview convertPoint:event.locationInWindow fromView:nil];
    return _sideBySide ? p.x : p.y;
}

- (void)mouseDown:(NSEvent *)event {
    _grab = [self position:event] - (_sideBySide ? NSMidX(self.frame) : NSMidY(self.frame));
}

- (void)mouseDragged:(NSEvent *)event {
    if (self.onDrag) self.onDrag([self position:event] - _grab);
}

- (void)rightMouseDown:(NSEvent *)event {}

@end

#pragma mark - Handle

// The toolbar control a panel is dragged by.
@interface P31SuiteHandle : NSView
// Every mouse move of a drag, then once more with `ended` at the mouse up.
@property (nonatomic, copy, nullable) void (^onDrag)(NSEvent *event, BOOL ended);
@end

@implementation P31SuiteHandle

- (instancetype)initWithFrame:(NSRect)frame {
    self = [super initWithFrame:frame];
    if (!self) return nil;
    NSImage *image = [NSImage imageWithSystemSymbolName:@"arrow.up.and.down.and.arrow.left.and.right"
                               accessibilityDescription:@"Move this panel"];
    NSImageView *icon = [NSImageView imageViewWithImage:image];
    icon.contentTintColor = NSColor.secondaryLabelColor;
    icon.frame = self.bounds;
    icon.autoresizingMask = NSViewWidthSizable | NSViewHeightSizable;
    [self addSubview:icon];
    return self;
}

// The icon is only a picture: the handle takes the mouse.
- (NSView *)hitTest:(NSPoint)point {
    return [super hitTest:point] ? self : nil;
}

- (BOOL)acceptsFirstMouse:(NSEvent *)event { return YES; }
- (BOOL)mouseDownCanMoveWindow { return NO; }

- (void)resetCursorRects {
    [self addCursorRect:self.bounds cursor:NSCursor.openHandCursor];
}

- (void)mouseDown:(NSEvent *)event {
    [NSCursor.closedHandCursor push];
}

- (void)mouseDragged:(NSEvent *)event {
    if (self.onDrag) self.onDrag(event, NO);
}

- (void)mouseUp:(NSEvent *)event {
    [NSCursor pop];
    if (self.onDrag) self.onDrag(event, YES);
}

- (void)rightMouseDown:(NSEvent *)event {}

@end

#pragma mark - Suite view

@implementation P31SuiteView {
    Layout _layout;
    Layout::Arrangement _arrangement;
    NSMutableDictionary<NSNumber *, NSView *> *_panes;          // by panel
    NSMutableDictionary<NSNumber *, NSButton *> *_hideButtons;  // by panel
    NSMutableArray<P31SuiteDivider *> *_dividers;               // as in _arrangement
    NSView *_highlight;   // where a dragged panel would land
    P31ResizeGrip *_grip;
    // The arrangement a panel drag would leave, while it is over a place to drop.
    std::optional<Layout> _drop;
}

- (instancetype)initWithFrame:(NSRect)frame {
    self = [super initWithFrame:frame];
    if (!self) return nil;
    self.autoresizingMask = NSViewWidthSizable | NSViewHeightSizable;
    _panes = [NSMutableDictionary dictionary];
    _hideButtons = [NSMutableDictionary dictionary];
    _dividers = [NSMutableArray array];

    // Above the panes and dividers, below the grip.
    _highlight = [[NSView alloc] initWithFrame:NSZeroRect];
    _highlight.wantsLayer = YES;
    _highlight.layer.cornerRadius = 6.0;
    _highlight.layer.borderWidth = 2.0;
    _highlight.hidden = YES;
    [self addSubview:_highlight];

    _grip = [[P31ResizeGrip alloc] initWithFrame:NSZeroRect];
    _grip.toolTip = @"Drag to resize";
    __weak P31SuiteView *weakSelf = self;
    _grip.onResize = ^BOOL(NSSize size) {
        P31SuiteView *suite = weakSelf;
        return suite.requestResize ? suite.requestResize(size) : NO;
    };
    [self addSubview:_grip];

    [self arrange];
    return self;
}

// As the layout's rectangles: y grows downwards.
- (BOOL)isFlipped { return YES; }

// Seen in the gaps between the panes.
- (void)drawRect:(NSRect)dirtyRect {
    [NSColor.windowBackgroundColor setFill];
    NSRectFill(dirtyRect);
}

- (void)rightMouseDown:(NSEvent *)event {}

#pragma mark - Layout

- (void)setLayout:(const Layout &)layout {
    _layout = layout;
    // A drag in flight was aimed at the old arrangement.
    _drop.reset();
    _highlight.hidden = YES;

    for (NSNumber *panel in _panes.allKeys) {
        if (_layout.contains(panel.intValue)) continue;
        [_panes[panel] removeFromSuperview];
        [_panes removeObjectForKey:panel];
        [_hideButtons removeObjectForKey:panel];
    }
    for (int panel : _layout.panels()) {
        if (_panes[@(panel)]) continue;
        NSView *pane = [[P31SuitePane alloc] initWithFrame:NSZeroRect];
        [self addSubview:pane positioned:NSWindowBelow relativeTo:nil];
        _panes[@(panel)] = pane;
    }
    [self arrange];
}

- (NSView *)paneForPanel:(int)panel {
    return _panes[@(panel)];
}

- (void)setFrameSize:(NSSize)size {
    [super setFrameSize:size];
    [self arrange];
}

// Place the panes, the dividers and the grip for the current size.
- (void)arrange {
    const NSRect b = self.bounds;
    _arrangement = _layout.arrange(b.size.width, b.size.height);
    for (const Layout::Pane &p : _arrangement.panes) _panes[@(p.panel)].frame = rectOf(p.rect);

    const NSUInteger count = _arrangement.dividers.size();
    while (_dividers.count > count) {
        [_dividers.lastObject removeFromSuperview];
        [_dividers removeLastObject];
    }
    __weak P31SuiteView *weakSelf = self;
    while (_dividers.count < count) {
        P31SuiteDivider *divider = [[P31SuiteDivider alloc] initWithFrame:NSZeroRect];
        const NSUInteger index = _dividers.count;
        divider.onDrag = ^(CGFloat position) { [weakSelf dividerAt:index draggedTo:position]; };
        [self addSubview:divider positioned:NSWindowBelow relativeTo:_highlight];
        [_dividers addObject:divider];
    }
    for (NSUInteger i = 0; i < count; ++i) {
        const Layout::Divider &d = _arrangement.dividers[i];
        P31SuiteDivider *divider = _dividers[i];
        divider.sideBySide = d.sideBySide;
        const CGFloat grow = (kDividerGrab - p31::suite::kGap) / 2.0;
        divider.frame = d.sideBySide ? NSInsetRect(rectOf(d.rect), -grow, 0.0) : NSInsetRect(rectOf(d.rect), 0.0, -grow);
        [self.window invalidateCursorRectsForView:divider];
    }

    const CGFloat grip = p31::ui::kResizeGripSize;
    _grip.frame = NSMakeRect(NSMaxX(b) - grip, NSMaxY(b) - grip, grip, grip);
    [self.window invalidateCursorRectsForView:_grip];

    // The last panel cannot be hidden.
    for (NSButton *button in _hideButtons.allValues) button.enabled = _layout.count() > 1;
}

- (void)edit:(const Layout &)layout {
    if (self.onLayoutEdit) self.onLayoutEdit(layout);
}

- (void)requestLayout:(const Layout &)layout {
    const Layout next = layout;
    __weak P31SuiteView *weakSelf = self;
    dispatch_async(dispatch_get_main_queue(), ^{ [weakSelf edit:next]; });
}

- (void)dividerAt:(NSUInteger)index draggedTo:(CGFloat)position {
    if (index >= _arrangement.dividers.size()) return;
    const Layout::Divider &d = _arrangement.dividers[index];
    Layout next = _layout;
    next.setRatio(d.split, d.ratioAt(position));
    [self edit:next];
}

#pragma mark - Panel toolbar

- (NSArray<NSView *> *)toolbarItemsForPanel:(int)panel {
    __weak P31SuiteView *weakSelf = self;
    P31SuiteHandle *handle = [[P31SuiteHandle alloc] initWithFrame:NSZeroRect];
    handle.toolTip = @"Drag to move this panel";
    handle.onDrag = ^(NSEvent *event, BOOL ended) { [weakSelf panel:panel draggedWith:event ended:ended]; };

    NSImage *image = [NSImage imageWithSystemSymbolName:@"xmark" accessibilityDescription:@"Hide this panel"];
    NSButton *hide = [NSButton buttonWithImage:image target:self action:@selector(hidePressed:)];
    hide.bordered = NO;
    hide.toolTip = @"Hide this panel (right-click to show it again)";
    hide.contentTintColor = NSColor.secondaryLabelColor;
    hide.tag = panel;
    hide.enabled = _layout.count() > 1;
    _hideButtons[@(panel)] = hide;
    return @[handle, hide];
}

- (void)hidePressed:(NSButton *)sender {
    Layout next = _layout;
    if (next.remove(static_cast<int>(sender.tag))) [self requestLayout:next];
}

#pragma mark - Moving a panel

// The arrangement that dropping `panel` at `point` gives, if any.
- (std::optional<Layout>)dropOf:(int)panel at:(NSPoint)point {
    const NSRect b = self.bounds;
    if (!NSPointInRect(point, b)) return std::nullopt;
    Layout next = _layout;

    // Along an edge of the window, unless the panel fills that edge already.
    NSRect own = NSZeroRect;
    for (const Layout::Pane &p : _arrangement.panes) {
        if (p.panel == panel) own = rectOf(p.rect);
    }
    const bool fullHeight = NSHeight(own) >= NSHeight(b), fullWidth = NSWidth(own) >= NSWidth(b);
    std::optional<Edge> edge;
    if (point.x < kEdgeZone) {
        if (!(fullHeight && NSMinX(own) <= 0.0)) edge = Edge::Left;
    } else if (point.x > NSMaxX(b) - kEdgeZone) {
        if (!(fullHeight && NSMaxX(own) >= NSMaxX(b))) edge = Edge::Right;
    } else if (point.y < kEdgeZone) {
        if (!(fullWidth && NSMinY(own) <= 0.0)) edge = Edge::Top;
    } else if (point.y > NSMaxY(b) - kEdgeZone) {
        if (!(fullWidth && NSMaxY(own) >= NSMaxY(b))) edge = Edge::Bottom;
    }
    if (edge) return next.moveToEdge(panel, *edge) ? std::optional<Layout>(next) : std::nullopt;

    // On another panel: in its middle to trade places, else on the side
    // of it the mouse is nearest to.
    for (const Layout::Pane &p : _arrangement.panes) {
        const NSRect r = rectOf(p.rect);
        if (p.panel == panel || !NSPointInRect(point, r) || r.size.width <= 0.0 || r.size.height <= 0.0) continue;
        const CGFloat fx = (point.x - r.origin.x) / r.size.width, fy = (point.y - r.origin.y) / r.size.height;
        bool ok = false;
        if (fx > kSwapZone && fx < 1.0 - kSwapZone && fy > kSwapZone && fy < 1.0 - kSwapZone) {
            ok = next.swap(panel, p.panel);
        } else {
            const CGFloat nearest = std::min({fx, 1.0 - fx, fy, 1.0 - fy});
            const Edge side = nearest == fx ? Edge::Left
                            : nearest == 1.0 - fx ? Edge::Right
                            : nearest == fy ? Edge::Top : Edge::Bottom;
            ok = next.move(panel, p.panel, side);
        }
        return ok ? std::optional<Layout>(next) : std::nullopt;
    }
    return std::nullopt;
}

- (void)panel:(int)panel draggedWith:(NSEvent *)event ended:(BOOL)ended {
    NSView *pane = _panes[@(panel)];
    if (ended) {
        pane.alphaValue = 1.0;
        _highlight.hidden = YES;
        // Taken first: showing the new arrangement forgets the drop.
        const std::optional<Layout> drop = _drop;
        _drop.reset();
        if (drop) [self edit:*drop];
        return;
    }
    pane.alphaValue = kDraggedAlpha;
    _drop = [self dropOf:panel at:[self convertPoint:event.locationInWindow fromView:nil]];
    if (!_drop) {
        _highlight.hidden = YES;
        return;
    }
    // Light up exactly where the panel would be.
    const NSRect b = self.bounds;
    for (const Layout::Pane &p : _drop->arrange(b.size.width, b.size.height).panes) {
        if (p.panel != panel) continue;
        NSColor *accent = NSColor.controlAccentColor;
        _highlight.layer.backgroundColor = [accent colorWithAlphaComponent:0.25].CGColor;
        _highlight.layer.borderColor = accent.CGColor;
        _highlight.frame = NSInsetRect(rectOf(p.rect), kHighlightInset, kHighlightInset);
        _highlight.hidden = NO;
    }
}

@end
