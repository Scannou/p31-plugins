//
//  EditorRootView.mm
//  p31-render
//

#import "p31/render/EditorRootView.h"

#import "p31/render/Menus.h"
#import "p31/render/PixelSnap.h"

#include <p31/core/Appearance.h>

#include <cmath>

namespace {
    constexpr CGFloat kToolbarInset = 8.0;
    constexpr CGFloat kButtonSize = 30.0;   // each button a rounded chip
    constexpr CGFloat kButtonGap = 6.0;
    constexpr NSTimeInterval kFade = 0.15;
    constexpr CGFloat kGripSize = p31::ui::kResizeGripSize;
}

@implementation P31ResizeGrip {
    NSPoint _start;        // screen location of the mouse down
    NSSize _startSize;     // editor size at the mouse down
}

- (BOOL)acceptsFirstMouse:(NSEvent *)event { return YES; }
- (BOOL)mouseDownCanMoveWindow { return NO; }

- (void)drawRect:(NSRect)dirtyRect {
    // Three diagonal ridges, as on a window's size box.
    [[NSColor.secondaryLabelColor colorWithAlphaComponent:0.7] setStroke];
    const CGFloat s = kGripSize;
    NSBezierPath *path = [NSBezierPath bezierPath];
    path.lineWidth = 1.0;
    for (CGFloat d : {4.0, 8.0, 12.0}) {
        [path moveToPoint:NSMakePoint(s - d, 2.0)];
        [path lineToPoint:NSMakePoint(s - 2.0, d)];
    }
    [path stroke];
}

- (void)resetCursorRects {
#if defined(__MAC_15_0) && __MAC_OS_X_VERSION_MAX_ALLOWED >= __MAC_15_0
    if (@available(macOS 15.0, *)) {
        [self addCursorRect:self.bounds
                     cursor:[NSCursor frameResizeCursorFromPosition:NSCursorFrameResizePositionBottomRight
                                                       inDirections:NSCursorFrameResizeDirectionsAll]];
    }
#endif
}

- (NSPoint)screenLocation:(NSEvent *)event {
    return [self.window convertPointToScreen:event.locationInWindow];
}

- (void)mouseDown:(NSEvent *)event {
    _start = [self screenLocation:event];
    _startSize = self.superview.bounds.size;
}

- (void)mouseDragged:(NSEvent *)event {
    if (!self.onResize) return;
    const NSPoint p = [self screenLocation:event];
    // Screen y grows upwards: dragging down makes the editor taller.
    const NSSize size = NSMakeSize(std::round(_startSize.width + p.x - _start.x),
                                   std::round(_startSize.height - (p.y - _start.y)));
    self.onResize(size);
}

- (void)rightMouseDown:(NSEvent *)event {}

@end

@implementation P31EditorRootView {
    NSView *_content;
    P31SettingsPanel *_panel;
    NSView *_toolbar;
    NSVisualEffectView *_freezeChip;
    NSVisualEffectView *_settingsChip;
    NSArray<NSView *> *_accessoryChips;
    NSButton *_freezeButton;
    NSButton *_settingsButton;
    P31ResizeGrip *_grip;
    NSTrackingArea *_tracking;
    BOOL _hovering;
    BOOL _frozenShown;
    NSString *_title;
    NSString *_version;
    NSString *_description;
    NSString *_url;
    uint32_t _brandLight, _brandDark;
}

- (instancetype)initWithFrame:(NSRect)frame content:(NSView *)content title:(NSString *)title {
    self = [super initWithFrame:frame];
    if (!self) return nil;
    self.autoresizingMask = NSViewWidthSizable | NSViewHeightSizable;
    _title = [title copy];
    _version = @"";
    _description = @"";
    _url = @"";
    _brandLight = 0xFF1B8A3A;
    _brandDark = 0xFF4CFF7C;

    _content = content;
    _content.frame = self.bounds;
    _content.autoresizingMask = NSViewWidthSizable | NSViewHeightSizable;
    [self addSubview:_content];

    _toolbarInsetRight = kToolbarInset;
    _toolbar = [[NSView alloc] initWithFrame:NSZeroRect];
    _toolbar.alphaValue = 0.0;
    _freezeButton = [self toolbarButton:@"snowflake" tip:@"Freeze the display" action:@selector(freezePressed:)];
    _settingsButton = [self toolbarButton:@"slider.horizontal.3" tip:@"Settings" action:@selector(settingsPressed:)];
    _freezeChip = [self chipWithControl:_freezeButton];
    _settingsChip = [self chipWithControl:_settingsButton];
    _accessoryChips = @[];
    [_toolbar addSubview:_freezeChip];
    [_toolbar addSubview:_settingsChip];
    [self addSubview:_toolbar];

    _panel = [[P31SettingsPanel alloc] initWithFrame:self.bounds title:title];
    _panel.autoresizingMask = NSViewWidthSizable | NSViewHeightSizable;
    _panel.hidden = YES;
    __weak P31EditorRootView *weakSelf = self;
    _panel.onDone = ^{ [weakSelf hideSettings]; };
    _panel.onLogoClick = ^{ [weakSelf showAbout]; };
    [self addSubview:_panel];

    _grip = [[P31ResizeGrip alloc] initWithFrame:NSZeroRect];
    _grip.toolTip = @"Drag to resize";
    _grip.hidden = YES;
    _grip.onResize = ^BOOL(NSSize size) {
        P31EditorRootView *root = weakSelf;
        return root.requestResize ? root.requestResize(size) : NO;
    };
    [self addSubview:_grip];
    return self;
}

- (NSButton *)toolbarButton:(NSString *)symbol tip:(NSString *)tip action:(SEL)action {
    NSImage *image = [NSImage imageWithSystemSymbolName:symbol accessibilityDescription:tip];
    NSButton *b = [NSButton buttonWithImage:image target:self action:action];
    b.bordered = NO;
    b.toolTip = tip;
    b.contentTintColor = NSColor.secondaryLabelColor;
    b.frame = NSMakeRect(0, 0, kButtonSize, kButtonSize);
    return b;
}

// A rounded, thinly outlined square behind a toolbar control.
- (NSVisualEffectView *)chipWithControl:(NSView *)control {
    NSVisualEffectView *chip = [[NSVisualEffectView alloc] initWithFrame:NSMakeRect(0, 0, kButtonSize, kButtonSize)];
    chip.material = NSVisualEffectMaterialHUDWindow;
    chip.blendingMode = NSVisualEffectBlendingModeWithinWindow;
    chip.state = NSVisualEffectStateActive;
    chip.wantsLayer = YES;
    chip.layer.cornerRadius = 7.0;
    chip.layer.masksToBounds = YES;
    chip.layer.borderWidth = 1.0;
    chip.layer.borderColor = [NSColor colorWithWhite:1.0 alpha:0.14].CGColor;
    control.frame = chip.bounds;
    control.autoresizingMask = NSViewWidthSizable | NSViewHeightSizable;
    [chip addSubview:control];
    return chip;
}

- (void)setToolbarInsetRight:(CGFloat)inset {
    _toolbarInsetRight = inset;
    [self setNeedsLayout:YES];
}

- (void)setRequestResize:(BOOL (^)(NSSize))requestResize {
    _requestResize = [requestResize copy];
    _grip.hidden = _requestResize == nil;
}

- (void)setToolbarAccessories:(NSArray<NSView *> *)accessories {
    _toolbarAccessories = [accessories copy];
    for (NSView *chip in _accessoryChips) [chip removeFromSuperview];
    NSMutableArray<NSView *> *chips = [NSMutableArray array];
    for (NSView *accessory in _toolbarAccessories) {
        NSView *chip = [self chipWithControl:accessory];
        [_toolbar addSubview:chip];
        [chips addObject:chip];
    }
    _accessoryChips = chips;
    [self setNeedsLayout:YES];
}

- (NSView *)content { return _content; }
- (P31SettingsPanel *)settingsPanel { return _panel; }
- (BOOL)settingsVisible { return !_panel.hidden; }

- (void)setPresetMenu:(NSMenu * (^)(void))presetMenu {
    _presetMenu = [presetMenu copy];
    _panel.presetMenu = _presetMenu;
}

#pragma mark - Layout

- (void)layout {
    [super layout];
    [self snapContentToPixels];
    const NSRect b = self.bounds;
    NSArray<NSView *> *chips = [_accessoryChips arrayByAddingObjectsFromArray:@[_freezeChip, _settingsChip]];
    const CGFloat step = kButtonSize + kButtonGap;
    const CGFloat w = chips.count * step - kButtonGap, h = kButtonSize;
    // Top right; this view is not flipped.
    _toolbar.frame = NSMakeRect(NSMaxX(b) - _toolbarInsetRight - w, NSMaxY(b) - kToolbarInset - h, w, h);
    for (NSUInteger i = 0; i < chips.count; ++i) chips[i].frame = NSMakeRect(i * step, 0.0, kButtonSize, kButtonSize);
    _grip.frame = NSMakeRect(NSMaxX(b) - kGripSize, 0.0, kGripSize, kGripSize);
    [self.window invalidateCursorRectsForView:_grip];
}

// The host may place the editor between pixels; keep the drawing on whole ones.
- (void)snapContentToPixels {
    p31::ui::pixelSnapContent(self, _content);
}

#pragma mark - Toolbar visibility

- (void)updateTrackingAreas {
    [super updateTrackingAreas];
    if (_tracking) [self removeTrackingArea:_tracking];
    _tracking = [[NSTrackingArea alloc]
        initWithRect:NSZeroRect
             options:NSTrackingMouseEnteredAndExited | NSTrackingActiveAlways | NSTrackingInVisibleRect
               owner:self
            userInfo:nil];
    [self addTrackingArea:_tracking];
}

- (void)mouseEntered:(NSEvent *)event {
    _hovering = YES;
    [self updateToolbar:YES];
}

- (void)mouseExited:(NSEvent *)event {
    _hovering = NO;
    [self updateToolbar:YES];
}

- (void)updateToolbar:(BOOL)animated {
    const BOOL frozen = self.isFrozen && self.isFrozen();
    _frozenShown = frozen;
    _freezeButton.contentTintColor = frozen ? NSColor.controlAccentColor : NSColor.secondaryLabelColor;
    _freezeButton.toolTip = frozen ? @"Frozen: click to resume" : @"Freeze the display";
    const CGFloat alpha = (_hovering || frozen) && _panel.hidden ? 1.0 : 0.0;
    if (_toolbar.alphaValue == alpha) return;
    if (!animated) {
        _toolbar.alphaValue = alpha;
        return;
    }
    [NSAnimationContext runAnimationGroup:^(NSAnimationContext *ctx) {
        ctx.duration = kFade;
        self->_toolbar.animator.alphaValue = alpha;
    }];
}

#pragma mark - Actions

- (void)freezePressed:(id)sender {
    if (self.toggleFreeze) self.toggleFreeze();
    [self updateToolbar:NO];
}

- (void)settingsPressed:(id)sender {
    [self showSettings];
}

- (void)showSettings {
    if (self.makeForm) [_panel setForm:self.makeForm()];
    NSView *settingsHost = self.settingsHost;
    NSView *host = settingsHost ? settingsHost : self;
    if (_panel.superview != host) [host addSubview:_panel];
    _panel.frame = host.bounds;
    // On another view the panel no longer inherits this editor's appearance.
    _panel.appearance = host == self ? nil : self.appearance;
    _panel.hidden = NO;
    [self updateToolbar:NO];
}

// Removed for good: a panel shown on another view goes too.
- (void)viewWillMoveToSuperview:(NSView *)newSuperview {
    [super viewWillMoveToSuperview:newSuperview];
    if (newSuperview == nil && _panel.superview != self) [_panel removeFromSuperview];
}

- (void)hideSettings {
    _panel.hidden = YES;
    [self updateToolbar:NO];
}

- (void)refreshFreeze {
    if ((self.isFrozen && self.isFrozen()) != _frozenShown) [self updateToolbar:NO];
}

- (void)reloadSettings {
    if (!_panel.hidden) [_panel reload];
}

- (void)setAppearanceMode:(int)mode {
    switch (mode) {
        case p31::AppearanceLight: self.appearance = [NSAppearance appearanceNamed:NSAppearanceNameAqua]; break;
        case p31::AppearanceDark: self.appearance = [NSAppearance appearanceNamed:NSAppearanceNameDarkAqua]; break;
        default: self.appearance = nil; break;
    }
    if (_panel.superview != self) _panel.appearance = self.appearance;
}

#pragma mark - Brand

- (void)setVersion:(NSString *)version description:(NSString *)description url:(NSString *)url {
    _version = [version copy];
    _description = [description copy];
    _url = [url copy];
    _panel.version = [@"v" stringByAppendingString:version];
}

- (void)setBrandLightColor:(uint32_t)light darkColor:(uint32_t)dark {
    _brandLight = light;
    _brandDark = dark;
    [_panel setLogoLightColor:light darkColor:dark];
}

- (void)showAbout {
    p31::ui::AboutInfo info;
    info.name = std::string("P31 ") + _title.UTF8String;
    info.version = _version.UTF8String;
    info.description = _description.UTF8String;
    info.url = _url.UTF8String;
    info.lightColor = _brandLight;
    info.darkColor = _brandDark;
    p31::ui::showAbout(info, self);
}

#pragma mark - Context menu

- (void)rightMouseDown:(NSEvent *)event {
    NSMenu *menu = [[NSMenu alloc] initWithTitle:@"P31"];
    menu.autoenablesItems = NO;
    __weak P31EditorRootView *weakSelf = self;

    NSString *about = [NSString stringWithFormat:@"About P31 %@\u2026", _title];
    [menu addItem:[P31MenuItem itemWithTitle:about handler:^{ [weakSelf showAbout]; }]];
    [menu addItem:[NSMenuItem separatorItem]];

    P31MenuItem *freeze = [P31MenuItem itemWithTitle:@"Freeze" handler:^{ [weakSelf freezePressed:nil]; }];
    freeze.state = (self.isFrozen && self.isFrozen()) ? NSControlStateValueOn : NSControlStateValueOff;
    freeze.enabled = self.toggleFreeze != nil;
    [menu addItem:freeze];

    if (self.presetMenu) {
        NSMenuItem *presets = [[NSMenuItem alloc] initWithTitle:@"Presets" action:nil keyEquivalent:@""];
        presets.submenu = self.presetMenu();
        [menu addItem:presets];
    }
    [menu addItem:[NSMenuItem separatorItem]];
    [menu addItem:[P31MenuItem itemWithTitle:@"Settings…" handler:^{ [weakSelf showSettings]; }]];
    if (self.extendMenu) self.extendMenu(menu);

    [NSMenu popUpContextMenu:menu withEvent:event forView:self];
}

@end
