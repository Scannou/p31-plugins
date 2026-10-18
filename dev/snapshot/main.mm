//
//  main.mm
//  p31-snapshot
//
//  Minimal CLAP host for checking an editor by eye: loads a .clap, opens its
//  editor in a window, streams a synthetic signal through process() in real
//  time, and after a while saves the editor's pixels to a PNG.
//
//  p31-snapshot <plugin.clap> <out.png> [signal] [seconds] [width] [height]
//  signals: mix (default), mono, left, wide, antiphase, silence,
//           sine (1 kHz at -6 dBFS), tones (100 Hz / 1 kHz / 10 kHz at -12 dBFS),
//           white (white noise at -20 dBFS RMS)
//

#import <Cocoa/Cocoa.h>
#import <QuartzCore/QuartzCore.h>

#include <clap/clap.h>

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

const clap_host kHost{
    CLAP_VERSION_INIT, nullptr, "p31-snapshot", "P31", "", "0",
    [](const clap_host*, const char*) -> const void* { return nullptr; },
    [](const clap_host*) {},
    [](const clap_host*) {},
    [](const clap_host*) {},
};

// One stereo frame of the named test signal at sample index n.
void signalAt(const std::string& name, uint64_t n, std::mt19937& rng, float& l, float& r) {
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
        plugin->activate(plugin, kSampleRate, 32, kBlock);

        auto* gui = static_cast<const clap_plugin_gui*>(plugin->get_extension(plugin, CLAP_EXT_GUI));
        if (!gui || !gui->create(plugin, CLAP_WINDOW_API_COCOA, false)) { std::fprintf(stderr, "no gui\n"); return 1; }
        gui->set_size(plugin, width, height);
        NSWindow* window = [[NSWindow alloc] initWithContentRect:NSMakeRect(80, 80, width, height)
                                                       styleMask:NSWindowStyleMaskTitled
                                                         backing:NSBackingStoreBuffered
                                                           defer:NO];
        window.title = @"p31-snapshot";
        window.releasedWhenClosed = NO;
        clap_window cw{CLAP_WINDOW_API_COCOA, {}};
        cw.cocoa = (__bridge void*)window.contentView;
        gui->set_parent(plugin, &cw);
        gui->show(plugin);
        [window orderFrontRegardless];

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

        [[NSRunLoop mainRunLoop] runUntilDate:[NSDate dateWithTimeIntervalSinceNow:seconds]];
        const bool ok = writePng(window.contentView, outPath);

        running.store(false);
        audio.join();
        gui->destroy(plugin);
        [window close];
        plugin->deactivate(plugin);
        plugin->destroy(plugin);
        entry->deinit();
        std::printf("%s %s\n", ok ? "wrote" : "FAILED", outPath.UTF8String);
        return ok ? 0 : 1;
    }
}
