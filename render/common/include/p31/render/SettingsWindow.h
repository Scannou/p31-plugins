//
//  SettingsWindow.h
//  p31-render
//
//  The window settings pages open in: a floating panel beside the editor,
//  with a tab per page when it holds several (P31 Suite: one per shown
//  panel). Pages are P31SettingsPanels; the window shows the selected one
//  and keeps the others hidden, so their owners can tell whether they are on
//  screen. Objective-C++ only.
//

#pragma once

#import <Cocoa/Cocoa.h>

#import "p31/render/SettingsPanel.h"

#include "p31/render/ObjCNames.h"

NS_ASSUME_NONNULL_BEGIN

@interface P31SettingsWindow : NSPanel

// `title` is the window's title ("P31 Spectrum Settings").
- (instancetype)initWithTitle:(NSString *)title;

// Add a page under the tab `tabTitle`. Tabs are sorted by `order`.
// `prepare` runs each time the page is about to be shown (builds its form).
- (void)addPage:(P31SettingsPanel *)page
       tabTitle:(NSString *)tabTitle
          order:(NSInteger)order
        prepare:(nullable void (^)(void))prepare;
// Remove a page; the window closes when its last shown page goes.
- (void)removePage:(P31SettingsPanel *)page;

// Bring the window up on `page`'s tab. The first time, it is placed beside
// `editorWindow` (the window of the editor that asked).
- (void)showPage:(P31SettingsPanel *)page besideWindow:(nullable NSWindow *)editorWindow;
// The window is on screen with `page`'s tab selected.
- (BOOL)isShowingPage:(P31SettingsPanel *)page;

@end

NS_ASSUME_NONNULL_END
