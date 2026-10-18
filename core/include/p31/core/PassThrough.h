//
//  PassThrough.h
//  p31-core
//
//  Copies an audio block from input to output channels unchanged. Realtime
//  safe: no allocation, no locks. Handles in-place buffers, a missing input
//  (output is silenced) and mono input feeding a stereo output (duplicated).
//

#pragma once

#include <cstdint>
#include <cstring>

namespace p31 {

// Returns the output constant mask (bit c set = channel c is constant)
// derived from `inConstantMask`.
inline uint64_t passThrough(const float* const* in, uint32_t inChannels, float* const* out,
                            uint32_t outChannels, uint32_t frames, uint64_t inConstantMask) {
    const auto bit = [](uint32_t c) { return c < 64 ? uint64_t{1} << c : uint64_t{0}; };
    uint64_t outMask = 0;
    for (uint32_t c = 0; c < outChannels; ++c) {
        float* dst = out[c];
        if (dst == nullptr) continue;
        const uint32_t s = c < inChannels ? c : inChannels - 1;
        const float* src = (in != nullptr && inChannels > 0) ? in[s] : nullptr;
        if (src == nullptr) {
            std::memset(dst, 0, frames * sizeof(float));
            outMask |= bit(c);
            continue;
        }
        if (src != dst) std::memcpy(dst, src, frames * sizeof(float));
        if (inConstantMask & bit(s)) outMask |= bit(c);
    }
    return outMask;
}

} // namespace p31
