//
//  Menus.h
//  p31-render
//
//  Menu items that run a block, and the Presets menu shared by both
//  plugins: factory presets, the user's preset files, save / delete, and
//  the default for new instances. Objective-C++ only.
//

#pragma once

#import <Cocoa/Cocoa.h>

#include "p31/render/ObjCNames.h"

#include <p31/core/PresetStore.h>

#include <functional>
#include <string>
#include <utility>
#include <vector>

NS_ASSUME_NONNULL_BEGIN

@interface P31MenuItem : NSMenuItem
+ (instancetype)itemWithTitle:(NSString *)title handler:(void (^)(void))handler;
@end

namespace p31::ui {

struct PresetSource {
    std::vector<std::pair<std::string, std::string>> factory;   // name, settings text
    PresetStore store;
    std::function<std::string()> currentText;                  // what Save writes
    std::function<void(const std::string&)> apply;             // load settings text
};

// Dialogs run as sheets on `anchor`'s window (or modally without one).
NSMenu* makePresetMenu(const PresetSource& source, NSView* anchor);

} // namespace p31::ui

NS_ASSUME_NONNULL_END
