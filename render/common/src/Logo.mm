//
//  Logo.mm
//  p31-render
//

#import "p31/render/Logo.h"

#include "p31/render/LogoData.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <string_view>

namespace p31::ui {

namespace {

NSColor* colorFromARGB(uint32_t argb) {
    return [NSColor colorWithSRGBRed:((argb >> 16) & 0xFF) / 255.0
                               green:((argb >> 8) & 0xFF) / 255.0
                                blue:(argb & 0xFF) / 255.0
                               alpha:((argb >> 24) & 0xFF) / 255.0];
}

// Numbers and commands of an SVG path's d attribute.
class PathReader {
public:
    explicit PathReader(std::string_view d) : _d(d) {}

    // Next command letter, or 0 at the end. A number where a command is
    // expected repeats the previous command (SVG's implicit repetition).
    char command(char previous) {
        skip();
        if (_pos >= _d.size()) return 0;
        const char c = _d[_pos];
        if (std::isalpha(static_cast<unsigned char>(c))) {
            ++_pos;
            return c;
        }
        // An implicit command after a move is a line.
        if (previous == 'M') return 'L';
        if (previous == 'm') return 'l';
        return previous;
    }

    bool number(CGFloat& out) {
        skip();
        if (_pos >= _d.size()) return false;
        const char* start = _d.data() + _pos;
        char* end = nullptr;
        const double v = std::strtod(start, &end);
        if (end == start) return false;
        _pos += static_cast<size_t>(end - start);
        out = v;
        return true;
    }

private:
    void skip() {
        while (_pos < _d.size() && (std::isspace(static_cast<unsigned char>(_d[_pos])) || _d[_pos] == ',')) ++_pos;
    }

    std::string_view _d;
    size_t _pos = 0;
};

// Appends one d attribute to `path`. Supports M L H V Q C Z, absolute and
// relative, which covers the outlined logo files.
void appendPathData(CGMutablePathRef path, std::string_view d) {
    PathReader r(d);
    CGPoint current = CGPointZero, start = CGPointZero;
    char cmd = 0;
    while ((cmd = r.command(cmd)) != 0) {
        const bool rel = std::islower(static_cast<unsigned char>(cmd));
        const auto point = [&](CGPoint& p) {
            CGFloat x, y;
            if (!r.number(x) || !r.number(y)) return false;
            p = rel ? CGPointMake(current.x + x, current.y + y) : CGPointMake(x, y);
            return true;
        };
        CGPoint a, b, c;
        CGFloat v;
        switch (std::toupper(static_cast<unsigned char>(cmd))) {
            case 'M':
                if (!point(a)) return;
                CGPathMoveToPoint(path, nullptr, a.x, a.y);
                current = start = a;
                break;
            case 'L':
                if (!point(a)) return;
                CGPathAddLineToPoint(path, nullptr, a.x, a.y);
                current = a;
                break;
            case 'H':
                if (!r.number(v)) return;
                current.x = rel ? current.x + v : v;
                CGPathAddLineToPoint(path, nullptr, current.x, current.y);
                break;
            case 'V':
                if (!r.number(v)) return;
                current.y = rel ? current.y + v : v;
                CGPathAddLineToPoint(path, nullptr, current.x, current.y);
                break;
            case 'Q':
                if (!point(a) || !point(b)) return;
                CGPathAddQuadCurveToPoint(path, nullptr, a.x, a.y, b.x, b.y);
                current = b;
                break;
            case 'C':
                if (!point(a) || !point(b) || !point(c)) return;
                CGPathAddCurveToPoint(path, nullptr, a.x, a.y, b.x, b.y, c.x, c.y);
                current = c;
                break;
            case 'Z':
                CGPathCloseSubpath(path);
                current = start;
                break;
            default:
                return;   // unsupported command: keep what was read
        }
    }
}

// Value of attribute `name` in the tag starting at `tag`, or empty.
std::string_view attribute(std::string_view svg, size_t tag, std::string_view name) {
    const size_t end = svg.find('>', tag);
    const std::string key = " " + std::string(name) + "=\"";
    const size_t at = svg.find(key, tag);
    if (at == std::string_view::npos || at > end) return {};
    const size_t from = at + key.size();
    const size_t to = svg.find('"', from);
    return to == std::string_view::npos ? std::string_view{} : svg.substr(from, to - from);
}

struct ParsedLogo {
    CGPathRef path = nullptr;
    CGSize size = CGSizeZero;
};

ParsedLogo parse(std::string_view svg) {
    ParsedLogo out;
    CGMutablePathRef path = CGPathCreateMutable();
    const size_t root = svg.find("<svg");
    if (root != std::string_view::npos) {
        // viewBox="0 0 w h"
        const std::string box(attribute(svg, root, "viewBox"));
        double x = 0, y = 0, w = 0, h = 0;
        if (std::sscanf(box.c_str(), "%lf %lf %lf %lf", &x, &y, &w, &h) == 4) out.size = CGSizeMake(w, h);
    }
    for (size_t at = svg.find("<path"); at != std::string_view::npos; at = svg.find("<path", at + 5)) {
        appendPathData(path, attribute(svg, at, "d"));
    }
    out.path = path;
    return out;
}

const ParsedLogo& parsed(LogoKind kind) {
    static const ParsedLogo lockup = parse(logo_data::kLockupSvg);
    static const ParsedLogo mark = parse(logo_data::kMarkSvg);
    return kind == LogoKind::Lockup ? lockup : mark;
}

// Fill the logo into `rect` of a context whose y axis points up.
void drawLogo(CGContextRef ctx, LogoKind kind, CGRect rect, NSColor* color) {
    const ParsedLogo& logo = parsed(kind);
    if (!logo.path || logo.size.width <= 0 || logo.size.height <= 0) return;
    const CGFloat scale = std::min(rect.size.width / logo.size.width, rect.size.height / logo.size.height);
    CGContextSaveGState(ctx);
    // SVG y runs down: flip around the drawing rect.
    CGContextTranslateCTM(ctx, rect.origin.x, rect.origin.y + (rect.size.height + logo.size.height * scale) / 2.0);
    CGContextScaleCTM(ctx, scale, -scale);
    CGContextAddPath(ctx, logo.path);
    CGContextSetFillColorWithColor(ctx, color.CGColor);
    CGContextFillPath(ctx);
    CGContextRestoreGState(ctx);
}

} // namespace

CGPathRef logoPath(LogoKind kind, CGSize* size) {
    const ParsedLogo& logo = parsed(kind);
    if (size) *size = logo.size;
    return logo.path;
}

NSImage* logoImage(LogoKind kind, CGFloat height, NSColor* color) {
    CGSize size;
    logoPath(kind, &size);
    const CGFloat width = size.height > 0 ? height * size.width / size.height : height;
    NSColor* fill = [color copy];
    return [NSImage imageWithSize:NSMakeSize(width, height)
                          flipped:NO
                   drawingHandler:^BOOL(NSRect rect) {
                       drawLogo(NSGraphicsContext.currentContext.CGContext, kind, rect, fill);
                       return YES;
                   }];
}

} // namespace p31::ui

@implementation P31LogoView {
    p31::ui::LogoKind _kind;
}

- (instancetype)initWithKind:(p31::ui::LogoKind)kind {
    self = [super initWithFrame:NSZeroRect];
    if (self) {
        _kind = kind;
        _lightColor = 0xFF1B8A3A;
        _darkColor = 0xFF4CFF7C;   // the brand's phosphor green
    }
    return self;
}

- (void)setLightColor:(uint32_t)lightColor {
    _lightColor = lightColor;
    [self setNeedsDisplay:YES];
}

- (void)setDarkColor:(uint32_t)darkColor {
    _darkColor = darkColor;
    [self setNeedsDisplay:YES];
}

- (CGFloat)widthForHeight:(CGFloat)height {
    CGSize size;
    p31::ui::logoPath(_kind, &size);
    return size.height > 0 ? std::ceil(height * size.width / size.height) : height;
}

- (BOOL)isDark {
    NSAppearanceName match = [self.effectiveAppearance
        bestMatchFromAppearancesWithNames:@[NSAppearanceNameAqua, NSAppearanceNameDarkAqua]];
    return [match isEqualToString:NSAppearanceNameDarkAqua];
}

- (void)drawRect:(NSRect)dirtyRect {
    const uint32_t argb = [self isDark] ? _darkColor : _lightColor;
    // Left-aligned at full height; the frame is sized with widthForHeight:.
    p31::ui::drawLogo(NSGraphicsContext.currentContext.CGContext, _kind, self.bounds,
                      p31::ui::colorFromARGB(argb));
}

- (void)viewDidChangeEffectiveAppearance {
    [super viewDidChangeEffectiveAppearance];
    [self setNeedsDisplay:YES];
}

- (void)resetCursorRects {
    if (self.onClick) [self addCursorRect:self.bounds cursor:NSCursor.pointingHandCursor];
}

- (void)mouseDown:(NSEvent *)event {
    if (self.onClick) self.onClick();
}

- (BOOL)acceptsFirstMouse:(NSEvent *)event { return YES; }

@end
