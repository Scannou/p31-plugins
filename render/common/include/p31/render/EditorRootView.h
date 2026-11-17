//
//  EditorRootView.h
//  p31-render
//
//  The editor's top-level view: the analyzer view filling it, a small
//  toolbar (Freeze, Settings) that shows while the mouse is over the editor
//  or while frozen, a right-click menu with the same actions plus presets,
//  and a resize grip in the bottom-right corner. The settings open in a
//  window of their own (SettingsWindow.h). Objective-C++ only.
//

#pragma once

#import <Cocoa/Cocoa.h>

#import "p31/render/SettingsPanel.h"
#import "p31/render/SettingsWindow.h"

NS_ASSUME_NONNULL_BEGIN

namespace p31::ui {
constexpr CGFloat kResizeGripSize = 16.0;
}

// Bottom-right corner handle of an editor. Hosts like Live leave resizing to
// the plugin: dragging this asks for a new size of the grip's superview, the
// way plugins with their own resize corner do.
@interface P31ResizeGrip : NSView
@property (nonatomic, copy, nullable) BOOL (^onResize)(NSSize size);
@end

@interface P31EditorRootView : NSView

// `title` is the plugin's name after the brand ("Spectrum").
- (instancetype)initWithFrame:(NSRect)frame content:(NSView *)content title:(NSString *)title;

// Version ("0.1.0"), one-line description and project page, for the
// settings header and the About dialog.
- (void)setVersion:(NSString *)version description:(NSString *)description url:(NSString *)url;
// Distance of the toolbar from the right edge, to keep it clear of labels
// the content draws there.
@property (nonatomic) CGFloat toolbarInsetRight;
// The brand color per appearance (ARGB): the theme's main color, used for
// the lockup and the About mark.
- (void)setBrandLightColor:(uint32_t)light darkColor:(uint32_t)dark;
- (void)showAbout;

@property (nonatomic, readonly) NSView *content;
@property (nonatomic, readonly) P31SettingsPanel *settingsPanel;

// The settings page, built each time the panel opens.
@property (nonatomic, copy, nullable) p31::ui::Form (^makeForm)(void);
// Presets menu (panel header and right-click menu).
@property (nonatomic, copy, nullable) NSMenu * (^presetMenu)(void);
// The grip was dragged: ask the host for this size. Returns NO if refused.
// Without it there is no grip (a panel of P31 Suite).
@property (nonatomic, copy, nullable) BOOL (^requestResize)(NSSize size);
// Extra toolbar controls, left of Freeze, each on a chip like the buttons.
@property (nonatomic, copy, nullable) NSArray<NSView *> *toolbarAccessories;
// Adds items to the end of the right-click menu.
@property (nonatomic, copy, nullable) void (^extendMenu)(NSMenu *menu);
// Open the settings in `window`, under a tab sorted by `order`, instead of
// a window of the editor's own: P31 Suite's panels share one.
- (void)useSettingsWindow:(P31SettingsWindow *)window order:(NSInteger)order;
// Where dialogs (presets, About) should appear: the settings while their
// window has the focus, else the editor.
@property (nonatomic, readonly) NSView *dialogAnchor;
// Freeze state and toggle.
@property (nonatomic, copy, nullable) BOOL (^isFrozen)(void);
@property (nonatomic, copy, nullable) void (^toggleFreeze)(void);

// Bring the settings window up on this editor's page, or close it.
- (void)showSettings;
- (void)hideSettings;
// The settings window is on screen with this editor's page.
@property (nonatomic, readonly) BOOL settingsVisible;

// Re-read the freeze state (cheap; call every frame).
- (void)refreshFreeze;
// Keep the content on whole device pixels (cheap; call every frame: a host
// can move the editor without resizing it, and only the frame notices).
- (void)snapContentToPixels;
// Re-read the open panel's values after the settings or a parameter changed
// from outside (state load, preset, automation).
- (void)reloadSettings;

// p31::Appearance: follow the system, or force light or dark.
- (void)setAppearanceMode:(int)mode;

@end

NS_ASSUME_NONNULL_END
