//
//  DenormalGuard.h
//  p31-core
//
//  Scoped flush-to-zero for the audio thread: denormal floats are treated as
//  zero while the guard lives, and the previous FP mode is restored on exit.
//

#pragma once

#include <cstdint>

#if defined(__SSE__) || defined(__x86_64__)
#include <xmmintrin.h>
#endif

namespace p31 {

class DenormalGuard {
public:
    DenormalGuard() {
#if defined(__aarch64__)
        asm volatile("mrs %0, fpcr" : "=r"(_saved));
        const uint64_t ftz = _saved | (uint64_t{1} << 24);  // FPCR.FZ
        asm volatile("msr fpcr, %0" : : "r"(ftz));
#elif defined(__SSE__) || defined(__x86_64__)
        _saved = _mm_getcsr();
        _mm_setcsr(_saved | 0x8040u);  // FTZ | DAZ
#endif
    }
    ~DenormalGuard() {
#if defined(__aarch64__)
        asm volatile("msr fpcr, %0" : : "r"(_saved));
#elif defined(__SSE__) || defined(__x86_64__)
        _mm_setcsr(_saved);
#endif
    }
    DenormalGuard(const DenormalGuard&) = delete;
    DenormalGuard& operator=(const DenormalGuard&) = delete;

private:
#if defined(__aarch64__)
    uint64_t _saved = 0;
#else
    unsigned int _saved = 0;
#endif
};

} // namespace p31
