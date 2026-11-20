//
//  SystemAudio.mm
//  p31-plugin-common
//

#include "p31/plugin/SystemAudio.h"

#import <CoreAudio/AudioHardwareTapping.h>
#import <CoreAudio/CATapDescription.h>
#import <CoreAudio/CoreAudio.h>
#import <Foundation/Foundation.h>

#include <os/log.h>

#include <algorithm>
#include <atomic>
#include <cmath>
#include <vector>

namespace p31 {

namespace {
    // Frames deinterleaved per sink call; longer IO buffers go out in pieces.
    constexpr uint32_t kChunkFrames = 4096;
    // After these many seconds the log says how much audio arrived: a silent
    // stream usually means the permission to record system audio is missing.
    constexpr int64_t kReportSeconds[] = {3, 15};
    // IO buffer asked of the aggregate device. Audio reaches the views once
    // per buffer, so a short one keeps them close to what is heard.
    constexpr UInt32 kIOBufferFrames = 128;

    os_log_t logger() {
        static os_log_t log = os_log_create("com.scannou.p31", "system-audio");
        return log;
    }

    constexpr AudioObjectPropertyAddress kDefaultOutputAddress{
        kAudioHardwarePropertyDefaultOutputDevice, kAudioObjectPropertyScopeGlobal, kAudioObjectPropertyElementMain};

    template <class T>
    bool getProperty(AudioObjectID object, AudioObjectPropertySelector selector, T& value) {
        const AudioObjectPropertyAddress address{selector, kAudioObjectPropertyScopeGlobal,
                                                 kAudioObjectPropertyElementMain};
        UInt32 size = sizeof(T);
        return AudioObjectGetPropertyData(object, &address, 0, nullptr, &size, &value) == noErr && size == sizeof(T);
    }

} // namespace

struct SystemAudioCapture::Impl {
    Sink sink = nullptr;
    void* context = nullptr;

    AudioObjectID tap = kAudioObjectUnknown;
    AudioObjectID device = kAudioObjectUnknown;
    AudioDeviceIOProcID ioProc = nullptr;
    AudioObjectPropertyListenerBlock outputListener = nil;
    bool running = false;
    // Set before the IO proc starts.
    double sampleRate = 0.0;
    bool planarTap = false;          // the tap delivers one buffer per channel
    uint32_t tapChannels = 2;
    float scratch[2][kChunkFrames];  // deinterleaved channels, IO thread only

    // What arrived since the stream started, for the log.
    std::atomic<uint64_t> frames{0};
    std::atomic<float> peak{0.0f};
    std::atomic<uint32_t> inputBuffers{0};
    // How old the input was when the IO proc ran (now - input time), in
    // microseconds: the last value and the largest.
    std::atomic<uint32_t> inputAge{0};
    std::atomic<uint32_t> inputAgeMax{0};
    // Bumped by every start and stop, so a late report or device-change
    // notification can tell that its stream is gone.
    uint64_t generation = 0;
    // Outlives this object in blocks already queued on the main queue.
    std::shared_ptr<std::atomic<bool>> alive = std::make_shared<std::atomic<bool>>(true);

    bool open();
    void close();
    void deliver(const AudioBufferList* input);

    static OSStatus ioCallback(AudioObjectID, const AudioTimeStamp* now, const AudioBufferList* input,
                               const AudioTimeStamp* inputTime, AudioBufferList*, const AudioTimeStamp*,
                               void* client) {
        Impl* impl = static_cast<Impl*>(client);
        if (now != nullptr && inputTime != nullptr && (now->mFlags & kAudioTimeStampHostTimeValid) &&
            (inputTime->mFlags & kAudioTimeStampHostTimeValid) && now->mHostTime >= inputTime->mHostTime) {
            const auto age = static_cast<uint32_t>(
                std::min<UInt64>(AudioConvertHostTimeToNanos(now->mHostTime - inputTime->mHostTime) / 1000,
                                 UINT32_MAX));
            impl->inputAge.store(age, std::memory_order_relaxed);
            if (age > impl->inputAgeMax.load(std::memory_order_relaxed))
                impl->inputAgeMax.store(age, std::memory_order_relaxed);
        }
        if (input != nullptr) impl->deliver(input);
        return noErr;
    }
};

// IO thread. The tap delivers float32, as one interleaved buffer or one
// buffer per channel; the sink takes planar channels. An aggregate device
// lists its sub-devices' input streams first and its taps' last, so the
// tap is read from the end of the list.
void SystemAudioCapture::Impl::deliver(const AudioBufferList* input) {
    const uint32_t count = input->mNumberBuffers;
    inputBuffers.store(count, std::memory_order_relaxed);
    const uint32_t tapBuffers = planarTap ? tapChannels : 1;
    if (count < tapBuffers) return;
    const AudioBuffer* tapBuffer = &input->mBuffers[count - tapBuffers];
    if (tapBuffer->mData == nullptr || tapBuffer->mNumberChannels == 0) return;

    const float* planar[2] = {nullptr, nullptr};
    uint32_t channels = 0;
    uint32_t total = 0;
    const float* interleaved = nullptr;
    uint32_t stride = 0;

    if (tapBuffer->mNumberChannels > 1) {
        interleaved = static_cast<const float*>(tapBuffer->mData);
        stride = tapBuffer->mNumberChannels;
        channels = 2;
        total = tapBuffer->mDataByteSize / (sizeof(float) * stride);
    } else {
        channels = std::min<uint32_t>(tapBuffers, 2);
        total = tapBuffer->mDataByteSize / sizeof(float);
        for (uint32_t c = 0; c < channels; ++c) {
            const AudioBuffer& b = tapBuffer[c];
            if (b.mData == nullptr) return;
            planar[c] = static_cast<const float*>(b.mData);
            total = std::min<uint32_t>(total, b.mDataByteSize / sizeof(float));
        }
    }

    float loudest = 0.0f;
    for (uint32_t done = 0; done < total;) {
        const uint32_t n = std::min(kChunkFrames, total - done);
        const float* out[2] = {nullptr, nullptr};
        if (interleaved != nullptr) {
            const float* src = interleaved + static_cast<size_t>(done) * stride;
            for (uint32_t i = 0; i < n; ++i) {
                scratch[0][i] = src[static_cast<size_t>(i) * stride];
                scratch[1][i] = src[static_cast<size_t>(i) * stride + 1];
            }
            out[0] = scratch[0];
            out[1] = scratch[1];
        } else {
            for (uint32_t c = 0; c < channels; ++c) out[c] = planar[c] + done;
        }
        for (uint32_t i = 0; i < n; ++i) loudest = std::max(loudest, std::fabs(out[0][i]));
        sink(context, out, channels, n, sampleRate);
        done += n;
    }

    frames.fetch_add(total, std::memory_order_relaxed);
    if (loudest > peak.load(std::memory_order_relaxed)) peak.store(loudest, std::memory_order_relaxed);
}

bool SystemAudioCapture::Impl::open() {
    if (@available(macOS 14.2, *)) {
        // Every process, mixed down to stereo. This app plays nothing itself,
        // so it needs no exclusion.
        CATapDescription* description = [[CATapDescription alloc] initStereoGlobalTapButExcludeProcesses:@[]];
        description.name = @"P31 system audio";
        description.privateTap = YES;
        description.muteBehavior = CATapUnmuted;
        OSStatus status = AudioHardwareCreateProcessTap(description, &tap);
        if (status != noErr || tap == kAudioObjectUnknown) {
            os_log_error(logger(), "cannot create the process tap (%d)", static_cast<int>(status));
            tap = kAudioObjectUnknown;
            return false;
        }

        AudioStreamBasicDescription format{};
        if (!getProperty(tap, kAudioTapPropertyFormat, format) || format.mFormatID != kAudioFormatLinearPCM ||
            !(format.mFormatFlags & kAudioFormatFlagIsFloat) || format.mBitsPerChannel != 32 ||
            format.mSampleRate <= 0) {
            os_log_error(logger(), "the tap's format is not 32-bit float PCM");
            return false;
        }

        planarTap = (format.mFormatFlags & kAudioFormatFlagIsNonInterleaved) != 0;
        tapChannels = std::clamp<uint32_t>(format.mChannelsPerFrame, 1, 2);

        // A private aggregate device carries the tap's stream. It holds the
        // tap alone: with the output device in it, an audio interface's own
        // inputs would be part of the stream too.
        NSDictionary* aggregate = @{
            @kAudioAggregateDeviceNameKey: @"P31 System Audio",
            @kAudioAggregateDeviceUIDKey: NSUUID.UUID.UUIDString,
            @kAudioAggregateDeviceIsPrivateKey: @YES,
            @kAudioAggregateDeviceIsStackedKey: @NO,
            @kAudioAggregateDeviceTapAutoStartKey: @NO,
            @kAudioAggregateDeviceTapListKey: @[@{
                @kAudioSubTapUIDKey: description.UUID.UUIDString,
                @kAudioSubTapDriftCompensationKey: @YES,
            }],
        };
        status = AudioHardwareCreateAggregateDevice((__bridge CFDictionaryRef)aggregate, &device);
        if (status != noErr || device == kAudioObjectUnknown) {
            os_log_error(logger(), "cannot create the aggregate device (%d)", static_cast<int>(status));
            device = kAudioObjectUnknown;
            return false;
        }

        Float64 rate = 0.0;
        sampleRate = getProperty(device, kAudioDevicePropertyNominalSampleRate, rate) && rate > 0 ? rate
                                                                                                    : format.mSampleRate;
        frames.store(0);
        peak.store(0.0f);
        inputBuffers.store(0);
        inputAge.store(0);
        inputAgeMax.store(0);

        const AudioObjectPropertyAddress bufferAddress{kAudioDevicePropertyBufferFrameSize,
                                                       kAudioObjectPropertyScopeGlobal, kAudioObjectPropertyElementMain};
        UInt32 bufferFrames = kIOBufferFrames;
        if (AudioObjectSetPropertyData(device, &bufferAddress, 0, nullptr, sizeof(bufferFrames), &bufferFrames) !=
            noErr)
            os_log_error(logger(), "cannot set the IO buffer to %u frames", static_cast<unsigned>(kIOBufferFrames));
        bufferFrames = 0;
        getProperty(device, kAudioDevicePropertyBufferFrameSize, bufferFrames);
        UInt32 latency = 0, safety = 0;
        const AudioObjectPropertyAddress latencyAddress{kAudioDevicePropertyLatency, kAudioObjectPropertyScopeInput,
                                                        kAudioObjectPropertyElementMain};
        const AudioObjectPropertyAddress safetyAddress{kAudioDevicePropertySafetyOffset,
                                                       kAudioObjectPropertyScopeInput, kAudioObjectPropertyElementMain};
        UInt32 size = sizeof(latency);
        AudioObjectGetPropertyData(device, &latencyAddress, 0, nullptr, &size, &latency);
        size = sizeof(safety);
        AudioObjectGetPropertyData(device, &safetyAddress, 0, nullptr, &size, &safety);
        os_log(logger(), "IO buffer %u frames, input latency %u frames, safety offset %u frames",
               static_cast<unsigned>(bufferFrames), static_cast<unsigned>(latency), static_cast<unsigned>(safety));

        status = AudioDeviceCreateIOProcID(device, &Impl::ioCallback, this, &ioProc);
        if (status == noErr) status = AudioDeviceStart(device, ioProc);
        if (status != noErr) {
            os_log_error(logger(), "cannot start the stream (%d)", static_cast<int>(status));
            return false;
        }
        os_log(logger(), "started: %.0f Hz, tap %.0f Hz, %u channels, %{public}s", sampleRate, format.mSampleRate,
               static_cast<unsigned>(format.mChannelsPerFrame), planarTap ? "planar" : "interleaved");
        return true;
    }
    return false;
}

void SystemAudioCapture::Impl::close() {
    if (device != kAudioObjectUnknown) {
        if (ioProc != nullptr) {
            AudioDeviceStop(device, ioProc);
            AudioDeviceDestroyIOProcID(device, ioProc);
            ioProc = nullptr;
        }
        AudioHardwareDestroyAggregateDevice(device);
        device = kAudioObjectUnknown;
    }
    if (tap != kAudioObjectUnknown) {
        if (@available(macOS 14.2, *)) AudioHardwareDestroyProcessTap(tap);
        tap = kAudioObjectUnknown;
    }
}

bool SystemAudioCapture::available() {
    if (@available(macOS 14.2, *)) return true;
    return false;
}

SystemAudioCapture::SystemAudioCapture(Sink sink, void* context) : _impl(std::make_unique<Impl>()) {
    _impl->sink = sink;
    _impl->context = context;
}

SystemAudioCapture::~SystemAudioCapture() {
    stop();
    _impl->alive->store(false);
}

bool SystemAudioCapture::running() const { return _impl->running; }

bool SystemAudioCapture::start() {
    Impl* impl = _impl.get();
    if (impl->running) return true;
    if (!impl->open()) {
        impl->close();
        return false;
    }
    impl->running = true;
    const uint64_t generation = ++impl->generation;
    const std::shared_ptr<std::atomic<bool>> alive = impl->alive;

    // Another output device has another clock and rate: start over on it.
    impl->outputListener = ^(UInt32, const AudioObjectPropertyAddress*) {
        if (!alive->load() || impl->generation != generation || !impl->running) return;
        os_log(logger(), "the output device changed, restarting");
        this->stop();
        this->start();
    };
    AudioObjectAddPropertyListenerBlock(kAudioObjectSystemObject, &kDefaultOutputAddress, dispatch_get_main_queue(),
                                        impl->outputListener);

    for (const int64_t seconds : kReportSeconds) {
        dispatch_after(dispatch_time(DISPATCH_TIME_NOW, seconds * static_cast<int64_t>(NSEC_PER_SEC)),
                       dispatch_get_main_queue(), ^{
            if (!alive->load() || impl->generation != generation) return;
            os_log(logger(), "%llu frames in the first %lld s, peak %.5f, %u input buffers, input age %.1f ms (max %.1f)",
                   impl->frames.load(), static_cast<long long>(seconds), static_cast<double>(impl->peak.load()),
                   static_cast<unsigned>(impl->inputBuffers.load()), impl->inputAge.load() / 1000.0,
                   impl->inputAgeMax.load() / 1000.0);
        });
    }
    return true;
}

void SystemAudioCapture::stop() {
    Impl* impl = _impl.get();
    if (!impl->running) return;
    impl->running = false;
    ++impl->generation;
    if (impl->outputListener != nil) {
        AudioObjectRemovePropertyListenerBlock(kAudioObjectSystemObject, &kDefaultOutputAddress,
                                               dispatch_get_main_queue(), impl->outputListener);
        impl->outputListener = nil;
    }
    impl->close();
}

} // namespace p31
