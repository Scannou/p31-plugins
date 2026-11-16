//
//  SuiteView.h
//  p31-render
//
//  P31 Suite's window: one pane per shown panel, placed as a
//  p31::suite::Layout says, with a divider to drag between neighbours, a
//  handle on each panel to drag it somewhere else (the place it would take
//  lights up) and a resize grip in the bottom-right corner. The view edits
//  nothing itself: it reports the arrangement the user asks for and shows
//  the one it is given. Objective-C++ only.
//

#pragma once

#import <Cocoa/Cocoa.h>

#include "p31/render/ObjCNames.h"

#include <p31/suite/SuiteLayout.h>

NS_ASSUME_NONNULL_BEGIN

@interface P31SuiteView : NSView

// The arrangement shown. Panes are made for its panels and removed with them.
- (void)setLayout:(const p31::suite::Layout &)layout;
// Where a panel's editor goes; nil while the panel is hidden.
- (nullable NSView *)paneForPanel:(int)panel;

// A divider was dragged, a panel dropped or hidden: the arrangement asked for.
@property (nonatomic, copy, nullable) void (^onLayoutEdit)(const p31::suite::Layout &layout);
// Ask for an arrangement as the user's edits do, once the current event is
// over: safe from a control or a menu of a panel the change removes.
- (void)requestLayout:(const p31::suite::Layout &)layout;
// The grip was dragged: ask the host for this size. Returns NO if refused.
@property (nonatomic, copy, nullable) BOOL (^requestResize)(NSSize size);

// Controls for a panel's toolbar: a handle that drags the panel to a new
// place, and a button that hides it.
- (NSArray<NSView *> *)toolbarItemsForPanel:(int)panel;

@end

NS_ASSUME_NONNULL_END
