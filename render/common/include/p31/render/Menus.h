//
//  Menus.h
//  p31-render
//
//  Menu items that run a block, the Presets menu shared by both plugins
//  (factory presets, the user's preset files, save / delete, the default for
//  new instances) and the About dialog. Objective-C++ only.
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

struct AboutInfo {
    std::string name;          // "P31 Spectrum"
    std::string version;       // "0.1.0"
    std::string description;   // one line
    std::string url;           // project page, shown as a link; empty for none
    uint32_t lightColor = 0xFF1B8A3A, darkColor = 0xFF4CFF7C;   // the mark, ARGB
};

// The About dialog: the P31 mark, name, version, description, license and a
// link to the project page, with a button for the third-party notices when
// the bundle carries them.
void showAbout(const AboutInfo& info, NSView* anchor);

} // namespace p31::ui

NS_ASSUME_NONNULL_END
