//
//  Menus.mm
//  p31-render
//

#import "p31/render/Menus.h"

#import "p31/render/Logo.h"

#include <filesystem>
#include <memory>

@implementation P31MenuItem {
    void (^_handler)(void);
}

+ (instancetype)itemWithTitle:(NSString *)title handler:(void (^)(void))handler {
    P31MenuItem *item = [[self alloc] initWithTitle:title action:@selector(fire:) keyEquivalent:@""];
    item->_handler = [handler copy];
    item.target = item;
    return item;
}

- (void)fire:(id)sender {
    if (_handler) _handler();
}

@end

namespace p31::ui {

namespace {

NSString* str(const std::string& s) {
    NSString* r = [NSString stringWithUTF8String:s.c_str()];
    return r ? r : @"";
}

// Runs `alert` as a sheet when `anchor` is in a window, else modally, and
// passes the pressed button to `done`.
void runAlert(NSAlert* alert, NSView* anchor, void (^done)(NSModalResponse)) {
    NSWindow* window = anchor.window;
    if (window) {
        [alert beginSheetModalForWindow:window completionHandler:done];
    } else {
        done([alert runModal]);
    }
}

void confirm(NSString* message, NSString* info, NSString* action, NSView* anchor, void (^yes)(void)) {
    NSAlert* alert = [[NSAlert alloc] init];
    alert.messageText = message;
    alert.informativeText = info;
    [alert addButtonWithTitle:action];
    [alert addButtonWithTitle:@"Cancel"];
    runAlert(alert, anchor, ^(NSModalResponse r) {
        if (r == NSAlertFirstButtonReturn) yes();
    });
}

void reportFailure(NSString* message, NSView* anchor) {
    NSAlert* alert = [[NSAlert alloc] init];
    alert.messageText = message;
    alert.alertStyle = NSAlertStyleWarning;
    runAlert(alert, anchor, ^(NSModalResponse) {});
}

void savePreset(std::shared_ptr<const PresetSource> source, NSView* anchor) {
    NSAlert* alert = [[NSAlert alloc] init];
    alert.messageText = @"Save Preset";
    alert.informativeText = @"Saves the current settings under this name.";
    NSTextField* field = [[NSTextField alloc] initWithFrame:NSMakeRect(0, 0, 240, 24)];
    field.placeholderString = @"Preset name";
    alert.accessoryView = field;
    [alert addButtonWithTitle:@"Save"];
    [alert addButtonWithTitle:@"Cancel"];
    alert.window.initialFirstResponder = field;

    runAlert(alert, anchor, ^(NSModalResponse r) {
        if (r != NSAlertFirstButtonReturn) return;
        const std::string name = PresetStore::cleanName(field.stringValue.UTF8String ? field.stringValue.UTF8String : "");
        if (name.empty()) return;
        const std::string text = source->currentText();
        const auto write = [source, name, text, anchor] {
            if (!source->store.save(name, text)) reportFailure(@"The preset could not be saved.", anchor);
        };
        if (source->store.exists(name)) {
            // A sheet cannot open while the previous one is closing.
            dispatch_async(dispatch_get_main_queue(), ^{
                confirm([NSString stringWithFormat:@"Replace “%@”?", str(name)],
                        @"A preset with this name already exists.", @"Replace", anchor, ^{ write(); });
            });
        } else {
            write();
        }
    });
}

NSMenuItem* header(NSString* title) {
    NSMenuItem* item = [[NSMenuItem alloc] initWithTitle:title action:nil keyEquivalent:@""];
    item.enabled = NO;
    return item;
}

} // namespace

void showAbout(const AboutInfo& info, NSView* anchor) {
    NSAppearanceName match = [(anchor ? anchor.effectiveAppearance : NSApp.effectiveAppearance)
        bestMatchFromAppearancesWithNames:@[NSAppearanceNameAqua, NSAppearanceNameDarkAqua]];
    const uint32_t argb = [match isEqualToString:NSAppearanceNameDarkAqua] ? info.darkColor : info.lightColor;
    NSColor* color = [NSColor colorWithSRGBRed:((argb >> 16) & 0xFF) / 255.0
                                         green:((argb >> 8) & 0xFF) / 255.0
                                          blue:(argb & 0xFF) / 255.0
                                         alpha:1.0];

    NSAlert* alert = [[NSAlert alloc] init];
    alert.icon = logoImage(LogoKind::Mark, 64.0, color);
    alert.messageText = str(info.name);
    alert.informativeText = [NSString stringWithFormat:@"Version %@\n%@\n\nCopyright \u00A9 2026 Scannou. MIT License.",
                                                       str(info.version), str(info.description)];
    if (!info.url.empty()) {
        // Shown without the scheme; clicking opens the page.
        NSString* url = str(info.url);
        NSString* shown = [[url stringByReplacingOccurrencesOfString:@"https://" withString:@""]
                              stringByReplacingOccurrencesOfString:@"http://" withString:@""];
        NSMutableParagraphStyle* centred = [[NSMutableParagraphStyle alloc] init];
        centred.alignment = NSTextAlignmentCenter;
        NSTextField* link = [NSTextField labelWithAttributedString:
            [[NSAttributedString alloc] initWithString:shown
                                            attributes:@{
                                                NSLinkAttributeName: [NSURL URLWithString:url],
                                                NSFontAttributeName: [NSFont systemFontOfSize:NSFont.smallSystemFontSize],
                                                NSParagraphStyleAttributeName: centred,
                                            }]];
        // Links in a label only react when it is selectable with attributes.
        link.selectable = YES;
        link.allowsEditingTextAttributes = YES;
        link.frame = NSMakeRect(0, 0, 220, 18);
        alert.accessoryView = link;
    }
    [alert addButtonWithTitle:@"OK"];
    // Packaged builds carry the notices in the bundle (scripts/package.sh).
    NSBundle* bundle = [NSBundle bundleForClass:[P31MenuItem class]];
    NSURL* notices = [bundle URLForResource:@"THIRD_PARTY_NOTICES" withExtension:@"txt"];
    if (notices) [alert addButtonWithTitle:@"Third-Party Notices"];
    runAlert(alert, anchor, ^(NSModalResponse r) {
        if (r == NSAlertSecondButtonReturn && notices) [[NSWorkspace sharedWorkspace] openURL:notices];
    });
}

NSMenu* makePresetMenu(const PresetSource& sourceRef, NSView* anchor) {
    auto source = std::make_shared<const PresetSource>(sourceRef);
    __weak NSView* weakAnchor = anchor;
    NSMenu* menu = [[NSMenu alloc] initWithTitle:@"Presets"];
    menu.autoenablesItems = NO;

    [menu addItem:header(@"Factory")];
    for (const auto& [name, text] : source->factory) {
        const std::string t = text;
        [menu addItem:[P31MenuItem itemWithTitle:str(name) handler:^{ source->apply(t); }]];
    }

    [menu addItem:[NSMenuItem separatorItem]];
    [menu addItem:header(@"User")];
    const std::vector<std::string> names = source->store.names();
    if (names.empty()) [menu addItem:header(@"No saved presets")];
    NSMenu* deleteMenu = [[NSMenu alloc] initWithTitle:@"Delete Preset"];
    for (const std::string& entry : names) {
        // Blocks capture C++ references as references: take a copy.
        const std::string name = entry;
        [menu addItem:[P31MenuItem itemWithTitle:str(name) handler:^{
            if (const auto text = source->store.load(name)) source->apply(*text);
            else reportFailure(@"The preset could not be read.", weakAnchor);
        }]];
        [deleteMenu addItem:[P31MenuItem itemWithTitle:str(name) handler:^{
            confirm([NSString stringWithFormat:@"Delete “%@”?", str(name)], @"This cannot be undone.",
                    @"Delete", weakAnchor, ^{ source->store.remove(name); });
        }]];
    }

    [menu addItem:[NSMenuItem separatorItem]];
    [menu addItem:[P31MenuItem itemWithTitle:@"Save Preset…" handler:^{ savePreset(source, weakAnchor); }]];
    NSMenuItem* deleteItem = [[NSMenuItem alloc] initWithTitle:@"Delete Preset" action:nil keyEquivalent:@""];
    deleteItem.submenu = deleteMenu;
    deleteItem.enabled = !names.empty();
    [menu addItem:deleteItem];
    [menu addItem:[P31MenuItem itemWithTitle:@"Show Presets in Finder" handler:^{
        std::error_code ec;
        std::filesystem::create_directories(source->store.presetDir(), ec);
        NSURL* url = [NSURL fileURLWithPath:str(source->store.presetDir().string()) isDirectory:YES];
        [[NSWorkspace sharedWorkspace] openURL:url];
    }]];

    [menu addItem:[NSMenuItem separatorItem]];
    [menu addItem:[P31MenuItem itemWithTitle:@"Use Current Settings for New Instances" handler:^{
        if (!source->store.saveDefault(source->currentText())) {
            reportFailure(@"The default could not be saved.", weakAnchor);
        }
    }]];
    P31MenuItem* clear = [P31MenuItem itemWithTitle:@"Reset New Instances to Factory Settings" handler:^{
        source->store.clearDefault();
    }];
    clear.enabled = source->store.hasDefault();
    [menu addItem:clear];
    return menu;
}

} // namespace p31::ui
