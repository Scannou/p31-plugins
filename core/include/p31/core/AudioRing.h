//
//  AudioRing.h
//  p31-core
//
//  Lock-free single-producer / single-consumer audio history, carrying
//  samples from the audio thread to the UI. The writer never waits: when the
//  reader falls behind, the oldest frames are overwritten. Positions are
//  absolute frame counts since construction (64-bit; they do not wrap in
//  practice).
//
//  Every copy is validated against the writer's claim counter (seqlock
//  style), so frames overwritten while the reader copied them are reported
//  as lost instead of returned torn.
//
//  Frames are stored interleaved. The reader side hands out interleaved
//  frames too, the layout the scope and the spectrum both consume.
//

#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <memory>

namespace p31 {

class AudioRing {
public:
    static constexpr uint32_t kMaxChannels = 2;

    // Allocates; call off the audio thread. `channels` is clamped to 1..2,
    // the capacity rounded up to a power of two.
    AudioRing(uint32_t channels, size_t minCapacityFrames);
    AudioRing(const AudioRing&) = delete;
    AudioRing& operator=(const AudioRing&) = delete;

    uint32_t channels() const { return _channels; }
    size_t capacity() const { return _capacity; }

    // --- Writer (one thread, realtime safe) ---

    // Append `frames` frames from planar `data` with `channelCount`
    // channels. With fewer input channels than the ring has, the last one
    // is repeated (mono feeds both sides); extra ones are ignored. No input
    // (zero channels or null data) appends silence.
    void write(const float* const* data, uint32_t channelCount, uint32_t frames);

    // --- Reader (one thread) ---

    // Frames written so far; everything before this position is readable
    // until it is overwritten.
    uint64_t writePosition() const { return _written.load(std::memory_order_acquire); }

    // Copy frames [from, from + frames) into `out` (frames * channels()
    // floats). Every frame must be below writePosition(). Returns the first
    // position whose frame is intact: frames before it had already been
    // overwritten and are zeroed in `out`.
    uint64_t copy(uint64_t from, size_t frames, float* out) const;

    // The most recent `frames` frames, oldest first, into `out`. When fewer
    // are available the front is zero-filled. Returns how many frames came
    // from the ring.
    size_t latest(float* out, size_t frames) const;

private:
    uint32_t _channels;
    size_t _capacity;
    uint64_t _mask;
    std::unique_ptr<std::atomic<float>[]> _samples;

    // Written by the writer only. _claimed moves first (before any sample of
    // a block is stored), _written last (once the block is complete).
    alignas(64) std::atomic<uint64_t> _claimed{0};
    std::atomic<uint64_t> _written{0};

    static_assert(std::atomic<uint64_t>::is_always_lock_free);
    static_assert(std::atomic<float>::is_always_lock_free);
};

// Reader cursor for consuming every new frame once (the scope's pattern).
class AudioRingReader {
public:
    // Starts at the ring's current write position: only frames written from
    // now on are read.
    explicit AudioRingReader(const AudioRing& ring);

    struct Result {
        size_t frames = 0;     // frames written to `out`
        uint64_t skipped = 0;  // new frames not returned (over maxFrames, or overwritten)
    };

    // The frames written since the previous call, oldest first, at most
    // `maxFrames` of them: when more arrived, the most recent are kept and
    // the rest skipped. `out` holds maxFrames * channels floats.
    Result readNew(float* out, size_t maxFrames);

    // Forget everything written so far (e.g. after the view was hidden).
    void skipToNow() { _cursor = _ring.writePosition(); }

private:
    const AudioRing& _ring;
    uint64_t _cursor;
};

} // namespace p31
