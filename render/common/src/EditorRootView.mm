//
//  EditorRootView.mm
//  p31-render
//

#import "p31/render/EditorRootView.h"

#import "p31/render/Menus.h"

#include <p31/core/Appearance.h>

namespace {
    constexpr CGFloat kToolbarInset = 6.0;
    constexpr CGFloat kButtonSize = 24.0;
    constexpr NSTimeInterval kFade = 0.15;
}

@implementation P31EditorRootView {
    NSView *_content;
    P31SettingsPanel *_panel;
    NSVisualEffectView *_toolbar;
    NSButton *_freezeButton;
    NSButton *_settingsButton;
    NSTrackingArea *_tracking;
    BOOL _hovering;
    BOOL _frozenShown;
}

- (instancetype)initWithFrame:(NSRect)frame content:(NSView *)content title:(NSString *)title {
    self = [super initWithFrame:frame];
    if (!self) return nil;
    self.autoresizingMask = NSViewWidthSizable | NSViewHeightSizable;

    _content = content;
    _content.frame = self.bounds;
    _content.autoresizingMask = NSViewWidthSizable | NSViewHeightSizable;
    [self addSubview:_content];

    _toolbar = [[NSVisualEffectView alloc] initWithFrame:NSZeroRect];
    _toolbar.material = NSVisualEffectMaterialHUDWindow;
    _toolbar.blendingMode = NSVisualEffectBlendingModeWithinWindow;
    _toolbar.state = NSVisualEffectStateActive;
    _toolbar.wantsLayer = YES;
    _toolbar.layer.cornerRadius = 6.0;
    _toolbar.layer.masksToBounds = YES;
    _toolbar.alphaValue = 0.0;
    _freezeButton = [self toolbarButton:@"snowflake" tip:@"Freeze the display" action:@selector(freezePressed:)];
    _settingsButton = [self toolbarButton:@"slider.horizontal.3" tip:@"Settings" action:@selector(settingsPressed:)];
    [_toolbar addSubview:_freezeButton];
    [_toolbar addSubview:_settingsButton];
    [self addSubview:_toolbar];

    _panel = [[P31SettingsPanel alloc] initWithFrame:self.bounds title:title];
    _panel.autoresizingMask = NSViewWidthSizable | NSViewHeightSizable;
    _panel.hidden = YES;
    __weak P31EditorRootView *weakSelf = self;
    _panel.onDone = ^{ [weakSelf hideSettings]; };
    [self addSubview:_panel];
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
    const NSRect b = self.bounds;
    const CGFloat w = 2 * kButtonSize + 6.0, h = kButtonSize + 4.0;
    // Top right; this view is not flipped.
    _toolbar.frame = NSMakeRect(NSMaxX(b) - kToolbarInset - w, NSMaxY(b) - kToolbarInset - h, w, h);
    _freezeButton.frame = NSMakeRect(2.0, 2.0, kButtonSize, kButtonSize);
    _settingsButton.frame = NSMakeRect(4.0 + kButtonSize, 2.0, kButtonSize, kButtonSize);
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
    _panel.hidden = NO;
    [self updateToolbar:NO];
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
}

#pragma mark - Context menu

- (void)rightMouseDown:(NSEvent *)event {
    NSMenu *menu = [[NSMenu alloc] initWithTitle:@"P31"];
    menu.autoenablesItems = NO;
    __weak P31EditorRootView *weakSelf = self;

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

    [NSMenu popUpContextMenu:menu withEvent:event forView:self];
}

@end
