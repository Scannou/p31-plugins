//
//  main.mm
//  p31-snapshot
//
//  Minimal CLAP host for checking an editor by eye: loads a .clap, opens its
//  editor in a window, streams a synthetic signal through process() in real
//  time, and after a while saves the editor's pixels to a PNG.
//
//  p31-snapshot <plugin.clap> <out.png> [signal] [seconds] [width] [height] [settings [scroll]]
//  "settings" opens the editor's settings panel, scrolled down by `scroll` points.
//  With P31_STATE=<file>, the file's settings text is loaded as the plugin's
//  state before the editor opens. With "settings", P31_SELECT=<item title>
//  picks that item in the first settings pop-up that has it, as a click
//  would. P31_DUMP_STATE=1 prints the saved state's text at the end.
//  P31_DRAG=dx,dy drags the editor's resize grip by that many points (right,
//  down); the host side resizes the window like a DAW does. P31_ABOUT=1 opens
//  the About dialog and saves that instead of the editor. P31_HOVER=x,y puts
//  the mouse over the editor at that fraction of its width and height (0,0 is
//  bottom left), for hover readouts (held down on the spectrogram).
//  P31_HOLD=1 stops the audio shortly before the capture, as a stopped DAW
//  does (a spectrogram then rests on whole pixels).
//  P31_CLICK=x,y clicks the editor at that fraction of its width and height
//  (0,0 is top left) just before the capture, after P31_HOLD's stop: on P31
//  Meter's bars or figures it resets them.
//  For P31 Suite, where a panel is named Spectrum, Scope, Spectrogram or Meter:
//  P31_TOOLBAR=1 shows every panel's toolbar, as with the mouse over it.
//  P31_DIVIDER=n,dx,dy drags the n-th divider by that many points.
//  P31_MOVE=panel,x,y drags the panel by its handle to that fraction of the
//  editor (0,0 is top left) and drops it; with P31_MOVE_HOLD=1 the capture is
//  taken before the drop. P31_HIDE=panel clicks the panel's hide button.
//  P31_PANELS=title picks that item in the first panel's Panels menu.
//  signals: mix (default), mono, left, wide, antiphase, silence,
//           sine (1 kHz at -6 dBFS), tones (100 Hz / 1 kHz / 10 kHz at -12 dBFS),
//           white (white noise at -20 dBFS RMS),
//           sweep (40 Hz to 16 kHz every 2 s over a 220 Hz tone and a click per second),
//           file:<path> (raw interleaved stereo float32 at 48 kHz, looped; e.g.
//           ffmpeg -i in.wav -f f32le -ac 2 -ar 48000 out.f32)
//

#import <Cocoa/Cocoa.h>
#import <QuartzCore/QuartzCore.h>

#include <clap/clap.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <random>
#include <string>
#include <thread>
#include <vector>

namespace {

constexpr double kSampleRate = 48000.0;
constexpr uint32_t kBlock = 512;
constexpr double kPi = 3.14159265358979323846;

NSWindow* g_window = nil;

// Host side of plugin-initiated resizing: grow the window's content area
// keeping its top-left corner, as DAWs do.
const clap_host_gui kHostGui{
    [](const clap_host*) {},
    [](const clap_host*, uint32_t width, uint32_t height) {
        if (!g_window) return false;
        const NSRect content = [g_window contentRectForFrameRect:g_window.frame];
        const NSRect next = NSMakeRect(content.origin.x, NSMaxY(content) - height, width, height);
        [g_window setFrame:[g_window frameRectForContentRect:next] display:YES];
        return true;
    },
    [](const clap_host*) { return false; },
    [](const clap_host*) { return false; },
    [](const clap_host*, bool) {},
};

const clap_host kHost{
    CLAP_VERSION_INIT, nullptr, "p31-snapshot", "P31", "", "0",
    [](const clap_host*, const char* id) -> const void* {
        return std::strcmp(id, CLAP_EXT_GUI) == 0 ? &kHostGui : nullptr;
    },
    [](const clap_host*) {},
    [](const clap_host*) {},
    [](const clap_host*) {},
};

// Audio of a file: signal, interleaved L/R.
std::vector<float> g_file;

// One stereo frame of the named test signal at sample index n.
void signalAt(const std::string& name, uint64_t n, std::mt19937& rng, float& l, float& r) {
    if (!g_file.empty()) {
        const size_t i = static_cast<size_t>(n % (g_file.size() / 2));
        l = g_file[2 * i];
        r = g_file[2 * i + 1];
        return;
    }
    const double t = static_cast<double>(n) / kSampleRate;
    std::normal_distribution<float> noise(0.0f, 1.0f);
    if (name == "silence") { l = r = 0.0f; return; }
    if (name == "mono") { l = r = static_cast<float>(0.5 * std::sin(2 * kPi * 220 * t)); return; }
    if (name == "left") { l = static_cast<float>(0.6 * std::sin(2 * kPi * 330 * t)); r = 0.0f; return; }
    if (name == "antiphase") {
        l = static_cast<float>(0.5 * std::sin(2 * kPi * 220 * t));
        r = -l;
        return;
    }
    if (name == "sine") { l = r = static_cast<float>(0.5 * std::sin(2 * kPi * 1000 * t)); return; }
    if (name == "tones") {
        l = r = static_cast<float>(0.25 * (std::sin(2 * kPi * 100 * t) + std::sin(2 * kPi * 1000 * t) +
                                           std::sin(2 * kPi * 10000 * t)));
        return;
    }
    if (name == "white") {
        l = r = 0.1f * noise(rng);
        return;
    }
    if (name == "sweep") {
        // Phase of a logarithmic sweep from f0 to f1 over `period` seconds.
        constexpr double f0 = 40.0, f1 = 16000.0, period = 2.0;
        const double u = std::fmod(t, period), k = std::log(f1 / f0) / period;
        const double sweep = 0.25 * std::sin(2 * kPi * f0 * (std::exp(k * u) - 1.0) / k);
        const double tone = 0.1 * std::sin(2 * kPi * 220 * t);
        const double click = std::fmod(t, 1.0) < 0.002 ? 0.5 * noise(rng) : 0.0;
        l = r = static_cast<float>(sweep + tone + click + 0.0005 * noise(rng));
        return;
    }
    if (name == "wide") {
        l = 0.25f * noise(rng);
        r = 0.25f * noise(rng);
        return;
    }
    // mix: in-phase bass, mids 120 degrees apart, decorrelated air.
    const double bass = 0.35 * std::sin(2 * kPi * 70 * t);
    const double midL = 0.2 * std::sin(2 * kPi * 900 * t);
    const double midR = 0.2 * std::sin(2 * kPi * 900 * t + 2 * kPi / 3);
    l = static_cast<float>(bass + midL + 0.04 * noise(rng));
    r = static_cast<float>(bass + midR + 0.04 * noise(rng));
}

// The first view in the tree that answers `selector` (the editor's root).
NSView* findResponder(NSView* view, SEL selector) {
    if ([view respondsToSelector:selector]) return view;
    for (NSView* sub in view.subviews) {
        if (NSView* found = findResponder(sub, selector)) return found;
    }
    return nil;
}

// Loads `text` (StateCodec lines) through the plugin's state extension.
bool loadStateText(const clap_plugin* plugin, const std::string& text) {
    auto* state = static_cast<const clap_plugin_state*>(plugin->get_extension(plugin, CLAP_EXT_STATE));
    if (!state) return false;
    struct Reader {
        std::vector<uint8_t> bytes;
        size_t pos = 0;
    } reader;
    const uint8_t header[12] = {'P', '3', '1', 'S', 2, 0, 0, 0,
                                static_cast<uint8_t>(text.size()), static_cast<uint8_t>(text.size() >> 8),
                                static_cast<uint8_t>(text.size() >> 16), static_cast<uint8_t>(text.size() >> 24)};
    reader.bytes.assign(header, header + 12);
    reader.bytes.insert(reader.bytes.end(), text.begin(), text.end());
    clap_istream stream{&reader, [](const clap_istream* s, void* buffer, uint64_t size) -> int64_t {
                            auto* r = static_cast<Reader*>(s->ctx);
                            const size_t n = std::min<size_t>(size, r->bytes.size() - r->pos);
                            std::memcpy(buffer, r->bytes.data() + r->pos, n);
                            r->pos += n;
                            return static_cast<int64_t>(n);
                        }};
    return state->load(plugin, &stream);
}

NSPopUpButton* findPopupWithItem(NSView* view, NSString* title) {
    if (auto* p = [view isKindOfClass:[NSPopUpButton class]] ? (NSPopUpButton*)view : nil) {
        if ([p itemWithTitle:title]) return p;
    }
    for (NSView* sub in view.subviews) {
        if (NSPopUpButton* found = findPopupWithItem(sub, title)) return found;
    }
    return nil;
}

// Hidden views do not count: an editor inside P31 Suite keeps its own
// resize grip hidden.
NSView* findViewOfClass(NSView* view, NSString* prefix) {
    if (view.hidden) return nil;
    if ([NSStringFromClass([view class]) hasPrefix:prefix]) return view;
    for (NSView* sub in view.subviews) {
        if (NSView* found = findViewOfClass(sub, prefix)) return found;
    }
    return nil;
}

void collectViewsOfClass(NSView* view, NSString* prefix, NSMutableArray<NSView*>* out) {
    if ([NSStringFromClass([view class]) hasPrefix:prefix]) [out addObject:view];
    for (NSView* sub in view.subviews) collectViewsOfClass(sub, prefix, out);
}

NSEvent* mouseEvent(NSWindow* window, NSEventType type, NSPoint inWindow) {
    return [NSEvent mouseEventWithType:type
                              location:inWindow
                         modifierFlags:0
                             timestamp:NSProcessInfo.processInfo.systemUptime
                          windowNumber:window.windowNumber
                               context:nil
                           eventNumber:0
                            clickCount:1
                              pressure:1.0f];
}

// The editor root of P31 Suite's panel named `panel` ("Scope").
NSView* suitePanelRoot(NSWindow* window, const char* panel) {
    NSView* content = findViewOfClass(window.contentView, [NSString stringWithFormat:@"P31%sView", panel]);
    for (NSView* v = content; v; v = v.superview) {
        if ([NSStringFromClass([v class]) hasPrefix:@"P31EditorRootView"]) return v;
    }
    return nil;
}

// Press P31 Suite's n-th divider and drag it by (dx, dy) points.
bool dragDivider(NSWindow* window, int index, double dx, double dy) {
    NSMutableArray<NSView*>* dividers = [NSMutableArray array];
    collectViewsOfClass(window.contentView, @"P31SuiteDivider", dividers);
    if (index < 0 || index >= static_cast<int>(dividers.count)) return false;
    NSView* divider = dividers[index];
    const NSPoint start = [divider convertPoint:NSMakePoint(NSMidX(divider.bounds), NSMidY(divider.bounds)) toView:nil];
    [divider mouseDown:mouseEvent(window, NSEventTypeLeftMouseDown, start)];
    constexpr int kSteps = 10;
    for (int i = 1; i <= kSteps; ++i) {
        // Window y grows upwards: dragging down is a smaller y.
        const NSPoint p = NSMakePoint(start.x + dx * i / kSteps, start.y - dy * i / kSteps);
        [divider mouseDragged:mouseEvent(window, NSEventTypeLeftMouseDragged, p)];
        [[NSRunLoop mainRunLoop] runUntilDate:[NSDate dateWithTimeIntervalSinceNow:0.02]];
    }
    [divider mouseUp:mouseEvent(window, NSEventTypeLeftMouseUp, NSMakePoint(start.x + dx, start.y - dy))];
    return true;
}

// Press a panel's handle and drag it to (fx, fy) of the editor, measured
// from the top left. Returns the handle and the last event, for the drop.
NSView* dragPanel(NSWindow* window, const char* panel, double fx, double fy, NSPoint* end) {
    NSView* root = suitePanelRoot(window, panel);
    NSView* handle = root ? findViewOfClass(root, @"P31SuiteHandle") : nil;
    if (!handle) return nil;
    const NSRect content = window.contentView.bounds;
    const NSPoint start = [handle convertPoint:NSMakePoint(NSMidX(handle.bounds), NSMidY(handle.bounds)) toView:nil];
    *end = NSMakePoint(content.size.width * fx, content.size.height * (1.0 - fy));
    [handle mouseDown:mouseEvent(window, NSEventTypeLeftMouseDown, start)];
    constexpr int kSteps = 10;
    for (int i = 1; i <= kSteps; ++i) {
        const NSPoint p = NSMakePoint(start.x + (end->x - start.x) * i / kSteps, start.y + (end->y - start.y) * i / kSteps);
        [handle mouseDragged:mouseEvent(window, NSEventTypeLeftMouseDragged, p)];
        [[NSRunLoop mainRunLoop] runUntilDate:[NSDate dateWithTimeIntervalSinceNow:0.02]];
    }
    return handle;
}

// What a right-click on the first panel shows: its menu's Panels submenu.
NSMenu* panelsMenu(NSWindow* window) {
    NSView* root = findViewOfClass(window.contentView, @"P31EditorRootView");
    if (![root respondsToSelector:@selector(extendMenu)]) return nil;
    void (^extend)(NSMenu*) = [root valueForKey:@"extendMenu"];
    if (!extend) return nil;
    NSMenu* menu = [[NSMenu alloc] initWithTitle:@""];
    extend(menu);
    return [menu itemWithTitle:@"Panels"].submenu;
}

// Press on the editor's resize grip and drag it by (dx, dy) points in steps.
bool dragGrip(NSWindow* window, double dx, double dy) {
    NSView* grip = findViewOfClass(window.contentView, @"P31ResizeGrip");
    if (!grip) return false;
    const NSPoint centre = NSMakePoint(NSMidX(grip.bounds), NSMidY(grip.bounds));
    const NSPoint start = [window convertPointToScreen:[grip convertPoint:centre toView:nil]];
    const auto event = [&](NSEventType type, NSPoint screen) {
        return [NSEvent mouseEventWithType:type
                                  location:[window convertPointFromScreen:screen]
                             modifierFlags:0
                                 timestamp:NSProcessInfo.processInfo.systemUptime
                              windowNumber:window.windowNumber
                                   context:nil
                               eventNumber:0
                                clickCount:1
                                  pressure:1.0f];
    };
    [grip mouseDown:event(NSEventTypeLeftMouseDown, start)];
    constexpr int kSteps = 10;
    for (int i = 1; i <= kSteps; ++i) {
        const NSPoint p = NSMakePoint(start.x + dx * i / kSteps, start.y - dy * i / kSteps);
        [grip mouseDragged:event(NSEventTypeLeftMouseDragged, p)];
        [[NSRunLoop mainRunLoop] runUntilDate:[NSDate dateWithTimeIntervalSinceNow:0.02]];
    }
    [grip mouseUp:event(NSEventTypeLeftMouseUp, NSMakePoint(start.x + dx, start.y - dy))];
    return true;
}

std::string saveStateText(const clap_plugin* plugin) {
    auto* state = static_cast<const clap_plugin_state*>(plugin->get_extension(plugin, CLAP_EXT_STATE));
    std::string bytes;
    clap_ostream stream{&bytes, [](const clap_ostream* s, const void* buffer, uint64_t size) -> int64_t {
                            static_cast<std::string*>(s->ctx)->append(static_cast<const char*>(buffer), size);
                            return static_cast<int64_t>(size);
                        }};
    if (!state || !state->save(plugin, &stream) || bytes.size() < 12) return {};
    return bytes.substr(12);
}

bool writePng(NSView* view, NSString* path) {
    const CGFloat scale = view.window.backingScaleFactor;
    const NSSize size = view.bounds.size;
    const size_t w = static_cast<size_t>(size.width * scale), h = static_cast<size_t>(size.height * scale);
    CGColorSpaceRef cs = CGColorSpaceCreateWithName(kCGColorSpaceSRGB);
    CGContextRef ctx = CGBitmapContextCreate(nullptr, w, h, 8, w * 4, cs, kCGImageAlphaPremultipliedLast);
    CGColorSpaceRelease(cs);
    if (!ctx) return false;
    CGContextScaleCTM(ctx, scale, scale);
    [view.layer renderInContext:ctx];
    CGImageRef image = CGBitmapContextCreateImage(ctx);
    CGContextRelease(ctx);
    NSBitmapImageRep* rep = [[NSBitmapImageRep alloc] initWithCGImage:image];
    CGImageRelease(image);
    NSData* png = [rep representationUsingType:NSBitmapImageFileTypePNG properties:@{}];
    return [png writeToFile:path atomically:YES];
}

} // namespace

int main(int argc, const char* argv[]) {
    @autoreleasepool {
        if (argc < 3) {
            std::fprintf(stderr, "usage: p31-snapshot <plugin.clap> <out.png> [signal] [seconds] [w] [h]\n");
            return 2;
        }
        NSString* pluginPath = [NSString stringWithUTF8String:argv[1]];
        NSString* outPath = [NSString stringWithUTF8String:argv[2]];
        const std::string signal = argc > 3 ? argv[3] : "mix";
        const double seconds = argc > 4 ? std::atof(argv[4]) : 2.0;
        const uint32_t width = argc > 5 ? static_cast<uint32_t>(std::atoi(argv[5])) : 420;
        const uint32_t height = argc > 6 ? static_cast<uint32_t>(std::atoi(argv[6])) : 460;
        if (signal.rfind("file:", 0) == 0) {
            NSData* data = [NSData dataWithContentsOfFile:@(signal.c_str() + 5)];
            if (data.length < 8) { std::fprintf(stderr, "cannot read %s\n", signal.c_str() + 5); return 1; }
            g_file.resize(data.length / sizeof(float) / 2 * 2);
            std::memcpy(g_file.data(), data.bytes, g_file.size() * sizeof(float));
        }
        const bool openSettings = argc > 7 && std::strcmp(argv[7], "settings") == 0;
        const double scroll = argc > 8 ? std::atof(argv[8]) : 0.0;

        [NSApplication sharedApplication];
        [NSApp setActivationPolicy:NSApplicationActivationPolicyAccessory];

        CFURLRef url = (__bridge CFURLRef)[NSURL fileURLWithPath:pluginPath];
        CFBundleRef bundle = CFBundleCreate(kCFAllocatorDefault, url);
        if (!bundle || !CFBundleLoadExecutable(bundle)) { std::fprintf(stderr, "cannot load bundle\n"); return 1; }
        auto* entry = static_cast<const clap_plugin_entry*>(CFBundleGetDataPointerForName(bundle, CFSTR("clap_entry")));
        if (!entry || !entry->init(argv[1])) { std::fprintf(stderr, "no clap_entry\n"); return 1; }
        auto* factory = static_cast<const clap_plugin_factory*>(entry->get_factory(CLAP_PLUGIN_FACTORY_ID));
        const clap_plugin_descriptor* desc = factory->get_plugin_descriptor(factory, 0);
        const clap_plugin* plugin = factory->create_plugin(factory, &kHost, desc->id);
        if (!plugin || !plugin->init(plugin)) { std::fprintf(stderr, "cannot create plugin\n"); return 1; }
        if (const char* statePath = std::getenv("P31_STATE")) {
            NSString* text = [NSString stringWithContentsOfFile:@(statePath) encoding:NSUTF8StringEncoding error:nil];
            if (!text || !loadStateText(plugin, text.UTF8String)) { std::fprintf(stderr, "state not loaded\n"); return 1; }
        }
        plugin->activate(plugin, kSampleRate, 32, kBlock);

        auto* gui = static_cast<const clap_plugin_gui*>(plugin->get_extension(plugin, CLAP_EXT_GUI));
        if (!gui || !gui->create(plugin, CLAP_WINDOW_API_COCOA, false)) { std::fprintf(stderr, "no gui\n"); return 1; }
        gui->set_size(plugin, width, height);
        NSWindow* window = [[NSWindow alloc] initWithContentRect:NSMakeRect(80, 80, width, height)
                                                       styleMask:NSWindowStyleMaskTitled
                                                         backing:NSBackingStoreBuffered
                                                           defer:NO];
        window.title = @"p31-snapshot";
        g_window = window;
        window.releasedWhenClosed = NO;
        clap_window cw{CLAP_WINDOW_API_COCOA, {}};
        cw.cocoa = (__bridge void*)window.contentView;
        gui->set_parent(plugin, &cw);
        gui->show(plugin);
        [window orderFrontRegardless];
        if (openSettings) {
            NSView* root = findResponder(window.contentView, @selector(showSettings));
            if (!root) { std::fprintf(stderr, "no settings panel\n"); return 1; }
            [root performSelector:@selector(showSettings)];
            [root layoutSubtreeIfNeeded];
            if (const char* select = std::getenv("P31_SELECT")) {
                NSPopUpButton* popup = findPopupWithItem(root, @(select));
                if (!popup) { std::fprintf(stderr, "no pop-up with \"%s\"\n", select); return 1; }
                [popup selectItemWithTitle:@(select)];
                [popup sendAction:popup.action to:popup.target];
            }
            if (scroll > 0) {
                NSView* clip = findResponder(root, @selector(documentVisibleRect));
                if (auto* sv = (NSScrollView*)clip) [sv.contentView scrollToPoint:NSMakePoint(0, scroll)];
            }
        }

        // Audio thread: real-time pacing so the editor sees a steady stream.
        std::atomic<bool> running{true};
        std::thread audio([&] {
            plugin->start_processing(plugin);
            std::vector<float> inL(kBlock), inR(kBlock), outL(kBlock), outR(kBlock);
            float* inPtr[] = {inL.data(), inR.data()};
            float* outPtr[] = {outL.data(), outR.data()};
            clap_audio_buffer in{inPtr, nullptr, 2, 0, 0};
            clap_audio_buffer out{outPtr, nullptr, 2, 0, 0};
            const clap_input_events inEvents{nullptr, [](const clap_input_events*) -> uint32_t { return 0; },
                                             [](const clap_input_events*, uint32_t) -> const clap_event_header* { return nullptr; }};
            const clap_output_events outEvents{nullptr, [](const clap_output_events*, const clap_event_header*) { return true; }};
            std::mt19937 rng(7);
            uint64_t n = 0;
            auto next = std::chrono::steady_clock::now();
            while (running.load()) {
                for (uint32_t i = 0; i < kBlock; ++i) signalAt(signal, n + i, rng, inL[i], inR[i]);
                clap_process proc{};
                proc.steady_time = static_cast<int64_t>(n);
                proc.frames_count = kBlock;
                proc.audio_inputs = &in;
                proc.audio_outputs = &out;
                proc.audio_inputs_count = 1;
                proc.audio_outputs_count = 1;
                proc.in_events = &inEvents;
                proc.out_events = &outEvents;
                plugin->process(plugin, &proc);
                n += kBlock;
                next += std::chrono::microseconds(static_cast<int64_t>(kBlock * 1e6 / kSampleRate));
                std::this_thread::sleep_until(next);
            }
            plugin->stop_processing(plugin);
        });

        if (const char* drag = std::getenv("P31_DRAG")) {
            double dx = 0, dy = 0;
            if (std::sscanf(drag, "%lf,%lf", &dx, &dy) != 2 || !dragGrip(window, dx, dy)) {
                std::fprintf(stderr, "drag failed\n");
                return 1;
            }
        }
        if (std::getenv("P31_TOOLBAR")) {
            NSMutableArray<NSView*>* roots = [NSMutableArray array];
            collectViewsOfClass(window.contentView, @"P31EditorRootView", roots);
            for (NSView* root in roots) {
                [root mouseEntered:mouseEvent(window, NSEventTypeMouseMoved, NSZeroPoint)];
            }
        }
        if (const char* divider = std::getenv("P31_DIVIDER")) {
            int index = 0;
            double dx = 0, dy = 0;
            if (std::sscanf(divider, "%d,%lf,%lf", &index, &dx, &dy) != 3 || !dragDivider(window, index, dx, dy)) {
                std::fprintf(stderr, "divider drag failed\n");
                return 1;
            }
        }
        if (const char* hide = std::getenv("P31_HIDE")) {
            NSView* root = suitePanelRoot(window, hide);
            NSMutableArray<NSView*>* buttons = [NSMutableArray array];
            if (root) collectViewsOfClass(root, @"NSButton", buttons);
            NSButton* button = nil;
            for (NSView* b in buttons) {
                if ([b.toolTip hasPrefix:@"Hide this panel"]) button = (NSButton*)b;
            }
            if (!button) { std::fprintf(stderr, "no hide button\n"); return 1; }
            [button performClick:nil];
        }
        if (const char* title = std::getenv("P31_PANELS")) {
            NSMenuItem* item = [panelsMenu(window) itemWithTitle:@(title)];
            if (!item || !item.enabled) { std::fprintf(stderr, "no Panels item \"%s\"\n", title); return 1; }
            [NSApp sendAction:item.action to:item.target from:item];
        }
        NSView* moveHandle = nil;
        NSPoint moveEnd = NSZeroPoint;
        if (const char* move = std::getenv("P31_MOVE")) {
            char panel[32] = {};
            double fx = 0, fy = 0;
            if (std::sscanf(move, "%31[^,],%lf,%lf", panel, &fx, &fy) == 3) {
                moveHandle = dragPanel(window, panel, fx, fy, &moveEnd);
            }
            if (!moveHandle) { std::fprintf(stderr, "panel move failed\n"); return 1; }
            if (!std::getenv("P31_MOVE_HOLD")) {
                [moveHandle mouseUp:mouseEvent(window, NSEventTypeLeftMouseUp, moveEnd)];
                moveHandle = nil;
            }
        }
        const bool about = std::getenv("P31_ABOUT") != nullptr;
        if (about) {
            NSView* root = findResponder(window.contentView, @selector(showAbout));
            if (!root) { std::fprintf(stderr, "no About\n"); return 1; }
            [root performSelector:@selector(showAbout)];
        }
        // The view drops a hover the real mouse is not over (after layout, a
        // window change), so the simulated one is sent again before capture.
        NSView* hoverTarget = nil;
        NSEvent* hoverMove = nil;
        // The spectrogram reads out while the mouse button is held instead.
        bool hoverPress = false;
        if (const char* hover = std::getenv("P31_HOVER")) {
            double fx = 0, fy = 0;
            NSView* target = findViewOfClass(window.contentView, @"P31SpectrumView");
            if (!target) target = findViewOfClass(window.contentView, @"P31ScopeView");
            if (!target) {
                target = findViewOfClass(window.contentView, @"P31SpectrogramView");
                hoverPress = target != nil;
            }
            if (std::sscanf(hover, "%lf,%lf", &fx, &fy) != 2 || !target) {
                std::fprintf(stderr, "hover failed\n");
                return 1;
            }
            const NSPoint local = NSMakePoint(target.bounds.size.width * fx, target.bounds.size.height * fy);
            NSEvent* move = [NSEvent mouseEventWithType:hoverPress ? NSEventTypeLeftMouseDown : NSEventTypeMouseMoved
                                               location:[target convertPoint:local toView:nil]
                                          modifierFlags:0
                                              timestamp:NSProcessInfo.processInfo.systemUptime
                                           windowNumber:window.windowNumber
                                                context:nil
                                            eventNumber:0
                                             clickCount:hoverPress ? 1 : 0
                                               pressure:hoverPress ? 1.0f : 0.0f];
            // Keep the readout where it is while the audio plays.
            if (hoverPress) [target mouseDown:move];
            else [target mouseMoved:move];
            hoverTarget = target;
            hoverMove = move;
        }
        constexpr double kSettle = 0.1;   // a frame or two to draw the re-sent hover
        [[NSRunLoop mainRunLoop] runUntilDate:[NSDate dateWithTimeIntervalSinceNow:std::max(0.0, seconds - kSettle)]];
        if (std::getenv("P31_HOLD")) {
            // Long enough for the editor to notice that no audio arrives.
            running.store(false);
            audio.join();
            [[NSRunLoop mainRunLoop] runUntilDate:[NSDate dateWithTimeIntervalSinceNow:1.0]];
        }
        if (const char* click = std::getenv("P31_CLICK")) {
            double fx = 0, fy = 0;
            if (std::sscanf(click, "%lf,%lf", &fx, &fy) != 2) { std::fprintf(stderr, "click failed\n"); return 1; }
            const NSRect content = window.contentView.bounds;
            const NSPoint at = NSMakePoint(content.size.width * fx, content.size.height * (1.0 - fy));
            // The view a real click would reach.
            NSView* target = [window.contentView hitTest:at];
            if (!target) { std::fprintf(stderr, "nothing to click\n"); return 1; }
            [target mouseDown:mouseEvent(window, NSEventTypeLeftMouseDown, at)];
            [target mouseUp:mouseEvent(window, NSEventTypeLeftMouseUp, at)];
            [[NSRunLoop mainRunLoop] runUntilDate:[NSDate dateWithTimeIntervalSinceNow:kSettle]];
        }
        if (hoverTarget) {
            if (hoverPress) [hoverTarget mouseDown:hoverMove];
            else [hoverTarget mouseMoved:hoverMove];
            [[NSRunLoop mainRunLoop] runUntilDate:[NSDate dateWithTimeIntervalSinceNow:kSettle]];
        }
        bool ok = false;
        if (about) {
            // The dialog is a sheet: its own window, not layer-backed.
            NSView* sheet = window.attachedSheet.contentView;
            NSBitmapImageRep* rep = sheet ? [sheet bitmapImageRepForCachingDisplayInRect:sheet.bounds] : nil;
            if (rep) {
                [sheet cacheDisplayInRect:sheet.bounds toBitmapImageRep:rep];
                ok = [[rep representationUsingType:NSBitmapImageFileTypePNG properties:@{}] writeToFile:outPath
                                                                                             atomically:YES];
            }
        } else {
            ok = writePng(window.contentView, outPath);
        }

        if (moveHandle) [moveHandle mouseUp:mouseEvent(window, NSEventTypeLeftMouseUp, moveEnd)];
        running.store(false);
        if (audio.joinable()) audio.join();
        if (std::getenv("P31_DUMP_STATE")) std::printf("%s", saveStateText(plugin).c_str());
        gui->destroy(plugin);
        [window close];
        plugin->deactivate(plugin);
        plugin->destroy(plugin);
        entry->deinit();
        std::printf("%s %s\n", ok ? "wrote" : "FAILED", outPath.UTF8String);
        return ok ? 0 : 1;
    }
}
