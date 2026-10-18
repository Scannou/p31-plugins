//
//  SettingsPanel.h
//  p31-render
//
//  Settings page laid over the editor: a header with the title, a Presets
//  button and Done, and below it the rows of a p31::ui::Form as controls in a
//  scrolling grid. Every edit goes through the row's callbacks, then
//  `onChange`, then the controls re-read their values (so they show what the
//  settings sanitised them to). Objective-C++ only.
//

#pragma once

#import <Cocoa/Cocoa.h>

#include "p31/render/Form.h"
#include "p31/render/ObjCNames.h"

NS_ASSUME_NONNULL_BEGIN

@interface P31SettingsPanel : NSView

- (instancetype)initWithFrame:(NSRect)frame title:(NSString *)title;

// Replace the rows (builds the controls).
- (void)setForm:(p31::ui::Form)form;
// Re-read every control's value, enabled and visible state.
- (void)reload;

// After any row's set.
@property (nonatomic, copy, nullable) void (^onChange)(void);
// Done was pressed.
@property (nonatomic, copy, nullable) void (^onDone)(void);
// The menu the Presets button shows, built each time it is pressed.
@property (nonatomic, copy, nullable) NSMenu * (^presetMenu)(void);

@end

NS_ASSUME_NONNULL_END
