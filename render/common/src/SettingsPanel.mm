//
//  SettingsPanel.mm
//  p31-render
//

#import "p31/render/SettingsPanel.h"

#import "p31/render/Logo.h"

#include <algorithm>
#include <cmath>
#include <vector>

using p31::ui::Row;

namespace {
    constexpr CGFloat kHeaderHeight = 40.0;
    constexpr CGFloat kLockupHeight = 24.0;   // as the brand README specifies
    constexpr CGFloat kMargin = 16.0;
    constexpr CGFloat kGridWidth = 420.0;
    constexpr CGFloat kSliderWidth = 150.0;
    constexpr CGFloat kValueWidth = 72.0;

    NSColor *colorFromARGB(uint32_t argb) {
        return [NSColor colorWithSRGBRed:((argb >> 16) & 0xFF) / 255.0
                                   green:((argb >> 8) & 0xFF) / 255.0
                                    blue:(argb & 0xFF) / 255.0
                                   alpha:((argb >> 24) & 0xFF) / 255.0];
    }

    uint32_t argbFromColor(NSColor *color) {
        NSColor *c = [color colorUsingColorSpace:[NSColorSpace sRGBColorSpace]];
        if (!c) return 0xFF000000;
        const auto byte = [](CGFloat v) { return static_cast<uint32_t>(std::lround(std::clamp(v, 0.0, 1.0) * 255.0)); };
        return (byte(c.alphaComponent) << 24) | (byte(c.redComponent) << 16) | (byte(c.greenComponent) << 8) |
               byte(c.blueComponent);
    }

    NSString *str(const std::string &s) {
        NSString *r = [NSString stringWithUTF8String:s.c_str()];
        return r ? r : @"";
    }

    NSTextField *makeLabel(NSString *text) {
        NSTextField *l = [NSTextField labelWithString:text];
        l.font = [NSFont systemFontOfSize:NSFont.smallSystemFontSize];
        l.textColor = NSColor.secondaryLabelColor;
        return l;
    }

    // The controls of one row, kept for reload.
    struct RowControls {
        NSGridRow *gridRow = nil;
        NSControl *control = nil;      // popup, slider, checkbox, field
        NSTextField *valueLabel = nil; // slider value
        NSColorWell *lightWell = nil;
        NSColorWell *darkWell = nil;
        NSTextField *label = nil;
    };
}

@interface P31SettingsPanel ()
- (void)controlChanged:(id)sender;
- (void)colorChanged:(NSColorWell *)sender;
@end

// The grid's scrolling container: flipped so the rows start at the top.
@interface P31FlippedView : NSView
@end
@implementation P31FlippedView
- (BOOL)isFlipped { return YES; }
@end

@implementation P31SettingsPanel {
    p31::ui::Form _form;
    std::vector<RowControls> _controls;
    NSVisualEffectView *_background;
    P31LogoView *_lockup;
    NSTextField *_title;
    NSTextField *_versionLabel;
    NSButton *_presetsButton;
    NSButton *_doneButton;
    NSBox *_divider;
    NSScrollView *_scroll;
    P31FlippedView *_document;
    NSGridView *_grid;
    NSInteger _trackingRow;   // slider in a drag, or -1
}

- (instancetype)initWithFrame:(NSRect)frame title:(NSString *)title {
    self = [super initWithFrame:frame];
    if (!self) return nil;
    _trackingRow = -1;

    _background = [[NSVisualEffectView alloc] initWithFrame:self.bounds];
    _background.material = NSVisualEffectMaterialWindowBackground;
    _background.blendingMode = NSVisualEffectBlendingModeWithinWindow;
    _background.state = NSVisualEffectStateActive;
    _background.autoresizingMask = NSViewWidthSizable | NSViewHeightSizable;
    [self addSubview:_background];

    _lockup = [[P31LogoView alloc] initWithKind:p31::ui::LogoKind::Lockup];
    _lockup.toolTip = @"About";
    [self addSubview:_lockup];

    _title = [NSTextField labelWithString:title];
    _title.font = [NSFont systemFontOfSize:NSFont.systemFontSize + 2 weight:NSFontWeightSemibold];
    _title.lineBreakMode = NSLineBreakByTruncatingTail;
    [self addSubview:_title];

    _versionLabel = [NSTextField labelWithString:@""];
    _versionLabel.font = [NSFont monospacedDigitSystemFontOfSize:NSFont.smallSystemFontSize weight:NSFontWeightRegular];
    _versionLabel.textColor = NSColor.secondaryLabelColor;
    [self addSubview:_versionLabel];

    _presetsButton = [NSButton buttonWithTitle:@"Presets" target:self action:@selector(showPresets:)];
    _presetsButton.controlSize = NSControlSizeSmall;
    _presetsButton.font = [NSFont systemFontOfSize:NSFont.smallSystemFontSize];
    _presetsButton.image = [NSImage imageWithSystemSymbolName:@"chevron.down" accessibilityDescription:nil];
    _presetsButton.imagePosition = NSImageTrailing;
    [self addSubview:_presetsButton];

    _doneButton = [NSButton buttonWithTitle:@"Done" target:self action:@selector(done:)];
    _doneButton.controlSize = NSControlSizeSmall;
    _doneButton.font = [NSFont systemFontOfSize:NSFont.smallSystemFontSize];
    _doneButton.keyEquivalent = @"\r";
    [self addSubview:_doneButton];

    _divider = [[NSBox alloc] initWithFrame:NSZeroRect];
    _divider.boxType = NSBoxSeparator;
    [self addSubview:_divider];

    _scroll = [[NSScrollView alloc] initWithFrame:NSZeroRect];
    _scroll.hasVerticalScroller = YES;
    _scroll.autohidesScrollers = YES;
    _scroll.drawsBackground = NO;
    _scroll.borderType = NSNoBorder;
    _document = [[P31FlippedView alloc] initWithFrame:NSZeroRect];
    _scroll.documentView = _document;
    [self addSubview:_scroll];
    return self;
}

- (BOOL)isFlipped { return YES; }

- (void)setVersion:(NSString *)version {
    _version = [version copy];
    _versionLabel.stringValue = version ? version : @"";
    [self setNeedsLayout:YES];
}

- (void)setLogoLightColor:(uint32_t)light darkColor:(uint32_t)dark {
    _lockup.lightColor = light;
    _lockup.darkColor = dark;
}

- (void)setOnLogoClick:(void (^)(void))onLogoClick {
    _onLogoClick = [onLogoClick copy];
    _lockup.onClick = _onLogoClick;
    [self.window invalidateCursorRectsForView:_lockup];
}

// Clicks stop here instead of reaching the analyzer underneath.
- (void)mouseDown:(NSEvent *)event {}
- (void)rightMouseDown:(NSEvent *)event {}

#pragma mark - Building

- (void)setForm:(p31::ui::Form)form {
    _form = std::move(form);
    [_grid removeFromSuperview];
    _controls.assign(_form.size(), RowControls{});

    _grid = [[NSGridView alloc] initWithFrame:NSZeroRect];
    _grid.rowSpacing = 7.0;
    _grid.columnSpacing = 10.0;
    bool lastWasColor = false;

    for (size_t i = 0; i < _form.size(); ++i) {
        const Row &row = _form[i];
        RowControls &rc = _controls[i];
        const NSInteger tag = static_cast<NSInteger>(i);
        NSString *tooltip = row.tooltip.empty() ? nil : str(row.tooltip);

        if (row.kind == Row::Kind::ColorPair && !lastWasColor) {
            // Column captions above a run of color rows.
            NSTextField *light = makeLabel(@"Light"), *dark = makeLabel(@"Dark");
            NSStackView *captions = [NSStackView stackViewWithViews:@[light, dark]];
            captions.spacing = 8.0;
            [light.widthAnchor constraintEqualToConstant:44].active = YES;
            [dark.widthAnchor constraintEqualToConstant:44].active = YES;
            [_grid addRowWithViews:@[NSGridCell.emptyContentView, captions]];
        }
        lastWasColor = row.kind == Row::Kind::ColorPair;

        NSView *content = nil;
        switch (row.kind) {
            case Row::Kind::Section: {
                NSTextField *h = [NSTextField labelWithString:str(row.label)];
                h.font = [NSFont boldSystemFontOfSize:NSFont.smallSystemFontSize];
                rc.gridRow = [_grid addRowWithViews:@[h, NSGridCell.emptyContentView]];
                if (i > 0) rc.gridRow.topPadding = 10.0;
                [rc.gridRow mergeCellsInRange:NSMakeRange(0, 2)];
                [rc.gridRow cellAtIndex:0].xPlacement = NSGridCellPlacementLeading;
                rc.label = h;
                continue;
            }
            case Row::Kind::Popup: {
                NSPopUpButton *p = [[NSPopUpButton alloc] initWithFrame:NSZeroRect pullsDown:NO];
                p.controlSize = NSControlSizeSmall;
                p.font = [NSFont systemFontOfSize:NSFont.smallSystemFontSize];
                for (const auto &[title, value] : row.choices) {
                    [p addItemWithTitle:str(title)];
                    p.lastItem.tag = value;
                }
                rc.control = p;
                content = p;
                break;
            }
            case Row::Kind::Slider: {
                NSSlider *s = [NSSlider sliderWithValue:0 minValue:row.min maxValue:row.max
                                                 target:self action:@selector(controlChanged:)];
                s.controlSize = NSControlSizeSmall;
                s.continuous = YES;
                [s.widthAnchor constraintEqualToConstant:kSliderWidth].active = YES;
                rc.valueLabel = makeLabel(@"");
                rc.valueLabel.textColor = NSColor.labelColor;
                rc.valueLabel.font = [NSFont monospacedDigitSystemFontOfSize:NSFont.smallSystemFontSize
                                                                      weight:NSFontWeightRegular];
                [rc.valueLabel.widthAnchor constraintEqualToConstant:kValueWidth].active = YES;
                rc.control = s;
                content = [NSStackView stackViewWithViews:@[s, rc.valueLabel]];
                break;
            }
            case Row::Kind::Checkbox: {
                NSButton *b = [NSButton checkboxWithTitle:str(row.label) target:self action:@selector(controlChanged:)];
                b.controlSize = NSControlSizeSmall;
                b.font = [NSFont systemFontOfSize:NSFont.smallSystemFontSize];
                rc.control = b;
                content = b;
                break;
            }
            case Row::Kind::Number: {
                NSTextField *f = [NSTextField textFieldWithString:@""];
                f.controlSize = NSControlSizeSmall;
                f.font = [NSFont monospacedDigitSystemFontOfSize:NSFont.smallSystemFontSize weight:NSFontWeightRegular];
                NSNumberFormatter *fmt = [[NSNumberFormatter alloc] init];
                fmt.allowsFloats = NO;
                fmt.minimum = @(row.min);
                fmt.maximum = @(row.max);
                f.formatter = fmt;
                f.cell.sendsActionOnEndEditing = YES;
                [f.widthAnchor constraintEqualToConstant:70].active = YES;
                rc.control = f;
                content = f;
                break;
            }
            case Row::Kind::ColorPair: {
                const auto well = [&](NSInteger wellTag, NSString *tip) {
                    NSColorWell *w = [[NSColorWell alloc] initWithFrame:NSMakeRect(0, 0, 44, 22)];
                    if (@available(macOS 13.0, *)) w.colorWellStyle = NSColorWellStyleMinimal;
                    w.tag = wellTag;
                    w.target = self;
                    w.action = @selector(colorChanged:);
                    w.toolTip = tip;
                    [w.widthAnchor constraintEqualToConstant:44].active = YES;
                    [w.heightAnchor constraintEqualToConstant:22].active = YES;
                    return w;
                };
                rc.lightWell = well(tag * 2, @"Used with the light appearance");
                rc.darkWell = well(tag * 2 + 1, @"Used with the dark appearance");
                NSStackView *pair = [NSStackView stackViewWithViews:@[rc.lightWell, rc.darkWell]];
                pair.spacing = 8.0;
                content = pair;
                break;
            }
        }

        if (rc.control) {
            rc.control.tag = tag;
            rc.control.target = self;
            rc.control.action = @selector(controlChanged:);
            rc.control.toolTip = tooltip;
        }
        if (row.kind == Row::Kind::Checkbox) {
            rc.gridRow = [_grid addRowWithViews:@[NSGridCell.emptyContentView, content]];
        } else {
            rc.label = makeLabel([str(row.label) stringByAppendingString:@":"]);
            rc.label.toolTip = tooltip;
            rc.gridRow = [_grid addRowWithViews:@[rc.label, content]];
        }
        rc.gridRow.yPlacement = NSGridCellPlacementCenter;
    }
    [_grid columnAtIndex:0].xPlacement = NSGridCellPlacementTrailing;
    [_document addSubview:_grid];
    [self reload];
    [self setNeedsLayout:YES];
}

#pragma mark - Layout

- (void)layout {
    [super layout];
    const NSRect b = self.bounds;
    _background.frame = b;

    const CGFloat buttonY = (kHeaderHeight - 22.0) / 2.0;
    [_doneButton sizeToFit];
    [_presetsButton sizeToFit];
    NSRect done = _doneButton.frame, presets = _presetsButton.frame;
    done.origin = NSMakePoint(NSMaxX(b) - kMargin + 4 - done.size.width, buttonY);
    presets.origin = NSMakePoint(NSMinX(done) - 6 - presets.size.width, buttonY);
    _doneButton.frame = done;
    _presetsButton.frame = presets;
    _presetsButton.hidden = self.presetMenu == nil;
    // Lockup, then the name and version on the lockup's text baseline area.
    const CGFloat right = (_presetsButton.hidden ? NSMinX(done) : NSMinX(presets)) - 8;
    const CGFloat lockupWidth = [_lockup widthForHeight:kLockupHeight];
    _lockup.frame = NSMakeRect(kMargin, (kHeaderHeight - kLockupHeight) / 2.0, lockupWidth, kLockupHeight);
    [_title sizeToFit];
    [_versionLabel sizeToFit];
    const CGFloat titleX = NSMaxX(_lockup.frame) + 8;
    const CGFloat titleW = std::max<CGFloat>(0, std::min(_title.frame.size.width, right - titleX));
    const CGFloat titleH = _title.frame.size.height;
    _title.frame = NSMakeRect(titleX, (kHeaderHeight - titleH) / 2.0, titleW, titleH);
    const CGFloat versionX = NSMaxX(_title.frame) + 6;
    const CGFloat versionH = _versionLabel.frame.size.height;
    // Sits on the title's baseline.
    const CGFloat versionY = NSMaxY(_title.frame) - versionH - (_title.font.descender - _versionLabel.font.descender);
    _versionLabel.frame = NSMakeRect(versionX, versionY, _versionLabel.frame.size.width, versionH);
    _versionLabel.hidden = NSMaxX(_versionLabel.frame) > right || _versionLabel.stringValue.length == 0;

    _divider.frame = NSMakeRect(0, kHeaderHeight - 1.0, b.size.width, 1.0);
    _scroll.frame = NSMakeRect(0, kHeaderHeight, b.size.width, std::max<CGFloat>(0, b.size.height - kHeaderHeight));
    [self layoutDocument];
}

- (void)layoutDocument {
    if (!_grid) return;
    const NSSize fit = _grid.fittingSize;
    const CGFloat width = std::max<CGFloat>(_scroll.contentSize.width, std::min(fit.width, kGridWidth) + 2 * kMargin);
    const CGFloat x = std::max<CGFloat>(kMargin, (width - fit.width) / 2.0);
    _grid.frame = NSMakeRect(std::floor(x), 4.0, fit.width, fit.height);
    _document.frame = NSMakeRect(0, 0, width, fit.height + 4.0 + kMargin);
}

#pragma mark - Values

- (void)reload {
    for (size_t i = 0; i < _form.size(); ++i) {
        const Row &row = _form[i];
        RowControls &rc = _controls[i];
        const bool visible = !row.visible || row.visible();
        const bool enabled = !row.enabled || row.enabled();
        if (rc.gridRow) rc.gridRow.hidden = !visible;
        if (!visible) continue;

        switch (row.kind) {
            case Row::Kind::Section:
                break;
            case Row::Kind::Popup: {
                NSPopUpButton *p = (NSPopUpButton *)rc.control;
                [p selectItemWithTag:static_cast<NSInteger>(row.get())];
                break;
            }
            case Row::Kind::Slider: {
                const double v = row.get();
                if (i != static_cast<size_t>(_trackingRow)) ((NSSlider *)rc.control).doubleValue = v;
                rc.valueLabel.stringValue = row.format ? str(row.format(v)) : [NSString stringWithFormat:@"%g", v];
                break;
            }
            case Row::Kind::Checkbox:
                ((NSButton *)rc.control).state = row.get() >= 0.5 ? NSControlStateValueOn : NSControlStateValueOff;
                break;
            case Row::Kind::Number:
                if (rc.control.currentEditor == nil) rc.control.integerValue = static_cast<NSInteger>(row.get());
                break;
            case Row::Kind::ColorPair:
                rc.lightWell.color = colorFromARGB(row.getLight());
                rc.darkWell.color = colorFromARGB(row.getDark());
                break;
        }
        if (rc.control) rc.control.enabled = enabled;
        if (rc.lightWell) rc.lightWell.enabled = enabled;
        if (rc.darkWell) rc.darkWell.enabled = enabled;
        if (rc.label && row.kind != Row::Kind::Section) {
            rc.label.textColor = enabled ? NSColor.secondaryLabelColor : NSColor.tertiaryLabelColor;
        }
    }
    [self layoutDocument];
}

- (void)edited {
    if (self.onChange) self.onChange();
    [self reload];
}

- (void)controlChanged:(id)sender {
    const NSInteger tag = [sender tag];
    if (tag < 0 || static_cast<size_t>(tag) >= _form.size()) return;
    const Row &row = _form[static_cast<size_t>(tag)];

    switch (row.kind) {
        case Row::Kind::Popup:
            row.set(static_cast<double>(((NSPopUpButton *)sender).selectedTag));
            break;
        case Row::Kind::Slider: {
            NSSlider *s = sender;
            double v = s.doubleValue;
            if (row.step > 0) v = row.min + std::round((v - row.min) / row.step) * row.step;
            // A drag is one edit: begin on its first change, end on mouse up.
            const NSEventType type = NSApp.currentEvent.type;
            const bool dragging = type == NSEventTypeLeftMouseDown || type == NSEventTypeLeftMouseDragged;
            if (_trackingRow != tag) {
                if (row.begin) row.begin();
                _trackingRow = dragging ? tag : -1;
                row.set(v);
                if (!dragging && row.end) row.end();
            } else {
                row.set(v);
                if (!dragging) {
                    _trackingRow = -1;
                    if (row.end) row.end();
                }
            }
            break;
        }
        case Row::Kind::Checkbox:
            if (row.begin) row.begin();
            row.set(((NSButton *)sender).state == NSControlStateValueOn ? 1.0 : 0.0);
            if (row.end) row.end();
            break;
        case Row::Kind::Number:
            row.set(std::clamp(static_cast<double>(((NSTextField *)sender).integerValue), row.min, row.max));
            break;
        default:
            return;
    }
    [self edited];
}

- (void)colorChanged:(NSColorWell *)sender {
    const NSInteger index = sender.tag / 2;
    if (index < 0 || static_cast<size_t>(index) >= _form.size()) return;
    const Row &row = _form[static_cast<size_t>(index)];
    if (row.kind != Row::Kind::ColorPair) return;
    const uint32_t argb = argbFromColor(sender.color);
    if (sender.tag % 2 == 0) row.setLight(argb);
    else row.setDark(argb);
    [self edited];
}

#pragma mark - Header

- (void)showPresets:(NSButton *)sender {
    if (!self.presetMenu) return;
    NSMenu *menu = self.presetMenu();
    [menu popUpMenuPositioningItem:nil atLocation:NSMakePoint(0, NSMaxY(sender.bounds) + 4) inView:sender];
}

- (void)done:(id)sender {
    // Close the shared color panel if one of our wells opened it.
    for (const RowControls &rc : _controls) {
        if (rc.lightWell.active) [rc.lightWell deactivate];
        if (rc.darkWell.active) [rc.darkWell deactivate];
    }
    [self.window makeFirstResponder:nil];
    if (self.onDone) self.onDone();
}

@end
