//
//  SettingsWindow.mm
//  p31-render
//

#import "p31/render/SettingsWindow.h"

#include <algorithm>
#include <vector>

namespace {
    constexpr CGFloat kTabBarHeight = 36.0;
    constexpr NSSize kInitialSize = {540.0, 640.0};
    constexpr NSSize kMinSize = {440.0, 280.0};
    constexpr CGFloat kGapToEditor = 12.0;
}

// Content view: the tab bar along the top (with more than one page), the
// selected page below it.
@interface P31SettingsWindowContent : NSView
@property (nonatomic) BOOL showsTabs;
@property (nonatomic, strong, nullable) NSSegmentedControl *tabs;
@property (nonatomic, weak, nullable) NSView *page;
@end

@implementation P31SettingsWindowContent

- (BOOL)isFlipped { return YES; }

// Behind the tab bar; each page draws its own background.
- (void)drawRect:(NSRect)dirtyRect {
    [NSColor.windowBackgroundColor setFill];
    NSRectFill(dirtyRect);
}

- (void)layout {
    [super layout];
    const NSRect b = self.bounds;
    CGFloat top = 0.0;
    self.tabs.hidden = !self.showsTabs;
    if (self.showsTabs) {
        [self.tabs sizeToFit];
        NSRect f = self.tabs.frame;
        f.origin = NSMakePoint(std::round((b.size.width - f.size.width) / 2.0),
                               std::round((kTabBarHeight - f.size.height) / 2.0));
        self.tabs.frame = f;
        top = kTabBarHeight;
    }
    self.page.frame = NSMakeRect(0.0, top, b.size.width, std::max<CGFloat>(0.0, b.size.height - top));
}

@end

@interface P31SettingsWindow () <NSWindowDelegate>
@end

@implementation P31SettingsWindow {
    struct Page {
        P31SettingsPanel *panel;
        NSString *title;
        NSInteger order;
        void (^prepare)(void);
    };
    std::vector<Page> _pages;           // in tab order
    P31SettingsPanel *_selected;
    P31SettingsWindowContent *_content;
    BOOL _placed;                       // positioned beside an editor once
}

- (instancetype)initWithTitle:(NSString *)title {
    self = [super initWithContentRect:NSMakeRect(0, 0, kInitialSize.width, kInitialSize.height)
                            styleMask:NSWindowStyleMaskTitled | NSWindowStyleMaskClosable |
                                      NSWindowStyleMaskResizable | NSWindowStyleMaskUtilityWindow
                              backing:NSBackingStoreBuffered
                                defer:YES];
    if (!self) return nil;
    self.title = title;
    self.releasedWhenClosed = NO;
    self.floatingPanel = YES;
    // Over the host's windows, but not over other apps'.
    self.hidesOnDeactivate = YES;
    self.contentMinSize = kMinSize;
    self.delegate = self;

    _content = [[P31SettingsWindowContent alloc] initWithFrame:NSMakeRect(0, 0, kInitialSize.width, kInitialSize.height)];
    _content.wantsLayer = YES;
    NSSegmentedControl *tabs = [NSSegmentedControl segmentedControlWithLabels:@[]
                                                                 trackingMode:NSSegmentSwitchTrackingSelectOne
                                                                       target:self
                                                                       action:@selector(tabChosen:)];
    tabs.controlSize = NSControlSizeRegular;
    _content.tabs = tabs;
    [_content addSubview:tabs];
    self.contentView = _content;
    return self;
}

#pragma mark - Pages

- (NSInteger)indexOfPage:(P31SettingsPanel *)panel {
    for (size_t i = 0; i < _pages.size(); ++i) {
        if (_pages[i].panel == panel) return static_cast<NSInteger>(i);
    }
    return -1;
}

- (void)addPage:(P31SettingsPanel *)page
       tabTitle:(NSString *)tabTitle
          order:(NSInteger)order
        prepare:(void (^)(void))prepare {
    if ([self indexOfPage:page] >= 0) return;
    Page p{page, [tabTitle copy], order, [prepare copy]};
    const auto at = std::find_if(_pages.begin(), _pages.end(), [order](const Page &q) { return q.order > order; });
    _pages.insert(at, p);
    page.hidden = YES;
    page.autoresizingMask = NSViewNotSizable;
    [_content addSubview:page];
    [self updateTabs];
}

- (void)removePage:(P31SettingsPanel *)page {
    const NSInteger i = [self indexOfPage:page];
    if (i < 0) return;
    _pages.erase(_pages.begin() + i);
    [page removeFromSuperview];
    if (page == _selected) {
        _selected = nil;
        // Another tab takes its place; with none left the window goes.
        if (_pages.empty()) [self close];
        else if (self.visible) [self select:_pages.front().panel];
    }
    [self updateTabs];
}

- (void)updateTabs {
    NSSegmentedControl *tabs = _content.tabs;
    tabs.segmentCount = static_cast<NSInteger>(_pages.size());
    for (size_t i = 0; i < _pages.size(); ++i) {
        [tabs setLabel:_pages[i].title forSegment:static_cast<NSInteger>(i)];
        [tabs setWidth:0 forSegment:static_cast<NSInteger>(i)];
    }
    const NSInteger selected = _selected ? [self indexOfPage:_selected] : -1;
    if (selected >= 0) tabs.selectedSegment = selected;
    _content.showsTabs = _pages.size() > 1;
    _content.needsLayout = YES;
}

- (void)select:(P31SettingsPanel *)page {
    const NSInteger i = [self indexOfPage:page];
    if (i < 0) return;
    if (_pages[static_cast<size_t>(i)].prepare) _pages[static_cast<size_t>(i)].prepare();
    for (const Page &p : _pages) p.panel.hidden = p.panel != page;
    _selected = page;
    _content.page = page;
    // The page's own appearance (each plugin instance has its own) for the
    // window around it too.
    self.appearance = page.appearance;
    _content.tabs.selectedSegment = i;
    _content.needsLayout = YES;
    [_content layoutSubtreeIfNeeded];
}

- (void)tabChosen:(NSSegmentedControl *)sender {
    const NSInteger i = sender.selectedSegment;
    if (i >= 0 && i < static_cast<NSInteger>(_pages.size())) [self select:_pages[static_cast<size_t>(i)].panel];
}

#pragma mark - Showing

- (void)showPage:(P31SettingsPanel *)page besideWindow:(NSWindow *)editorWindow {
    if ([self indexOfPage:page] < 0) return;
    [self select:page];
    if (!_placed && editorWindow) {
        [self placeBeside:editorWindow];
        _placed = YES;
    }
    [self makeKeyAndOrderFront:nil];
}

- (BOOL)isShowingPage:(P31SettingsPanel *)page {
    return self.visible && _selected == page;
}

// Right of the editor's window if that fits on its screen, else left of it,
// else over it; the tops aligned.
- (void)placeBeside:(NSWindow *)editor {
    const NSRect e = editor.frame;
    NSScreen *editorScreen = editor.screen;
    const NSRect screen = (editorScreen ? editorScreen : NSScreen.mainScreen).visibleFrame;
    NSRect f = self.frame;
    f.origin.y = std::max(NSMinY(screen), NSMaxY(e) - f.size.height);
    if (NSMaxX(e) + kGapToEditor + f.size.width <= NSMaxX(screen)) {
        f.origin.x = NSMaxX(e) + kGapToEditor;
    } else if (NSMinX(e) - kGapToEditor - f.size.width >= NSMinX(screen)) {
        f.origin.x = NSMinX(e) - kGapToEditor - f.size.width;
    } else {
        f.origin.x = NSMidX(e) - f.size.width / 2.0;
    }
    [self setFrame:f display:NO];
}

// Hidden pages tell their owners the settings are off screen.
- (void)windowWillClose:(NSNotification *)notification {
    for (const Page &p : _pages) p.panel.hidden = YES;
}

// Escape closes, as in other settings windows.
- (void)cancelOperation:(id)sender {
    [self close];
}

@end
