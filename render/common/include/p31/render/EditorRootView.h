//
//  EditorRootView.h
//  p31-render
//
//  The editor's top-level view: the analyzer view filling it, a small
//  toolbar (Freeze, Settings) that shows while the mouse is over the editor
//  or while frozen, the settings panel laid over everything, and a
//  right-click menu with the same actions plus presets. Objective-C++ only.
//

#pragma once

#import <Cocoa/Cocoa.h>

#import "p31/render/SettingsPanel.h"

NS_ASSUME_NONNULL_BEGIN

@interface P31EditorRootView : NSView

- (instancetype)initWithFrame:(NSRect)frame content:(NSView *)content title:(NSString *)title;

@property (nonatomic, readonly) NSView *content;
@property (nonatomic, readonly) P31SettingsPanel *settingsPanel;

// The settings page, built each time the panel opens.
@property (nonatomic, copy, nullable) p31::ui::Form (^makeForm)(void);
// Presets menu (panel header and right-click menu).
@property (nonatomic, copy, nullable) NSMenu * (^presetMenu)(void);
// Freeze state and toggle.
@property (nonatomic, copy, nullable) BOOL (^isFrozen)(void);
@property (nonatomic, copy, nullable) void (^toggleFreeze)(void);

- (void)showSettings;
- (void)hideSettings;
@property (nonatomic, readonly) BOOL settingsVisible;

// Re-read the freeze state (cheap; call every frame).
- (void)refreshFreeze;
// Re-read the open panel's values after the settings or a parameter changed
// from outside (state load, preset, automation).
- (void)reloadSettings;

// p31::Appearance: follow the system, or force light or dark.
- (void)setAppearanceMode:(int)mode;

@end

NS_ASSUME_NONNULL_END
