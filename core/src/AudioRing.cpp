//
//  AudioRing.cpp
//  p31-core
//

#include "p31/core/AudioRing.h"

#include <algorithm>
#include <cstring>

namespace p31 {

namespace {
    constexpr size_t kMaxCapacity = size_t{1} << 24;

    size_t roundUpPow2(size_t v) {
        size_t p = 1;
        while (p < v && p < kMaxCapacity) p <<= 1;
        return p;
    }
} // namespace

AudioRing::AudioRing(uint32_t channels, size_t minCapacityFrames)
    : _channels(std::clamp<uint32_t>(channels, 1, kMaxChannels)),
      _capacity(roundUpPow2(std::max<size_t>(minCapacityFrames, 1))),
      _mask(_capacity - 1),
      _samples(new std::atomic<float>[_capacity * _channels]()) {}

void AudioRing::write(const float* const* data, uint32_t channelCount, uint32_t frames) {
    if (frames == 0) return;

    // Resolve each ring channel to an input channel once, not per sample.
    const float* src[kMaxChannels] = {};
    if (data != nullptr && channelCount > 0) {
        for (uint32_t c = 0; c < _channels; ++c) src[c] = data[std::min(c, channelCount - 1)];
    }

    const uint64_t start = _written.load(std::memory_order_relaxed);
    const uint64_t end = start + frames;

    // Announce the overwrite before touching any sample. A reader that sees
    // one of the new samples is then guaranteed (through the fence pair, see
    // copy()) to also see this claim, and so to discard what it read.
    _claimed.store(end, std::memory_order_relaxed);
    std::atomic_thread_fence(std::memory_order_release);

    // Only the last `capacity` frames of an oversized block survive.
    const uint32_t first = frames > _capacity ? frames - static_cast<uint32_t>(_capacity) : 0;
    for (uint32_t c = 0; c < _channels; ++c) {
        const float* in = src[c];
        for (uint32_t i = first; i < frames; ++i) {
            const size_t slot = static_cast<size_t>((start + i) & _mask) * _channels + c;
            _samples[slot].store(in ? in[i] : 0.0f, std::memory_order_relaxed);
        }
    }

    _written.store(end, std::memory_order_release);
}

uint64_t AudioRing::copy(uint64_t from, size_t frames, float* out) const {
    for (size_t i = 0; i < frames; ++i) {
        const size_t slot = static_cast<size_t>((from + i) & _mask) * _channels;
        for (uint32_t c = 0; c < _channels; ++c) {
            out[i * _channels + c] = _samples[slot + c].load(std::memory_order_relaxed);
        }
    }

    // Pairs with the release fence in write(): any claim made before a
    // sample we loaded is visible now. Frames more than one capacity behind
    // the claim may have been overwritten during the copy.
    std::atomic_thread_fence(std::memory_order_acquire);
    const uint64_t claimed = _claimed.load(std::memory_order_relaxed);
    const uint64_t oldestIntact = claimed > _capacity ? claimed - _capacity : 0;
    if (from >= oldestIntact) return from;

    const size_t lost = static_cast<size_t>(std::min<uint64_t>(frames, oldestIntact - from));
    std::fill_n(out, lost * _channels, 0.0f);
    return from + lost;
}

size_t AudioRing::latest(float* out, size_t frames) const {
    const uint64_t end = writePosition();
    const size_t n = static_cast<size_t>(std::min<uint64_t>({frames, end, _capacity}));
    const size_t pad = frames - n;
    std::fill_n(out, pad * _channels, 0.0f);

    const uint64_t from = end - n;
    const uint64_t intact = copy(from, n, out + pad * _channels);
    return n - static_cast<size_t>(intact - from);
}

AudioRingReader::AudioRingReader(const AudioRing& ring) : _ring(ring), _cursor(ring.writePosition()) {}

AudioRingReader::Result AudioRingReader::readNew(float* out, size_t maxFrames) {
    const uint64_t end = _ring.writePosition();
    const uint64_t pending = end - _cursor;
    _cursor = end;

    Result r;
    r.frames = static_cast<size_t>(std::min<uint64_t>({pending, maxFrames, _ring.capacity()}));
    r.skipped = pending - r.frames;
    if (r.frames == 0) return r;

    const uint64_t from = end - r.frames;
    const uint64_t intact = _ring.copy(from, r.frames, out);
    if (intact > from) {
        // Drop the overwritten head; keep what survived at the front.
        const size_t lost = static_cast<size_t>(intact - from);
        const size_t ch = _ring.channels();
        std::memmove(out, out + lost * ch, (r.frames - lost) * ch * sizeof(float));
        r.frames -= lost;
        r.skipped += lost;
    }
    return r;
}

} // namespace p31
