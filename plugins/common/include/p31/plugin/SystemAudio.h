//
//  SystemAudio.h
//  p31-plugin-common
//
//  Everything this Mac plays, as one stereo stream: a Core Audio process tap
//  on all processes (macOS 14.2+), read through a private aggregate device.
//  The standalone apps analyse this instead of an input device; no loopback
//  driver is needed. macOS asks the user once for permission to record
//  system audio; without it the stream is silent.
//

#pragma once

#include <cstdint>
#include <memory>

namespace p31 {

class SystemAudioCapture {
public:
    // Called on Core Audio's realtime IO thread with planar float channels
    // (one or two) at `sampleRate`. Must not allocate, lock or log.
    using Sink = void (*)(void* context, const float* const* channels, uint32_t channelCount, uint32_t frames,
                          double sampleRate);

    // This macOS has process taps.
    static bool available();

    SystemAudioCapture(Sink sink, void* context);
    ~SystemAudioCapture();
    SystemAudioCapture(const SystemAudioCapture&) = delete;
    SystemAudioCapture& operator=(const SystemAudioCapture&) = delete;

    // Main thread. false when the tap cannot be set up. Follows the default
    // output device from then on (the stream restarts when it changes).
    bool start();
    // Main thread. No sink call is in flight once this returns.
    void stop();
    bool running() const;

private:
    struct Impl;
    std::unique_ptr<Impl> _impl;
};

} // namespace p31
