//
//  PassThroughTests.cpp
//  p31-core-tests
//

#include <doctest/doctest.h>
#include <p31/core/DenormalGuard.h>
#include <p31/core/PassThrough.h>

using p31::passThrough;

TEST_CASE("pass-through copies every channel unchanged") {
    float inL[] = {1, 2, 3}, inR[] = {4, 5, 6};
    float outL[3] = {}, outR[3] = {};
    const float* in[] = {inL, inR};
    float* out[] = {outL, outR};

    CHECK(passThrough(in, 2, out, 2, 3, 0) == 0);
    for (int i = 0; i < 3; ++i) {
        CHECK(outL[i] == inL[i]);
        CHECK(outR[i] == inR[i]);
    }
}

TEST_CASE("pass-through in place leaves the buffers alone") {
    float l[] = {1, 2}, r[] = {3, 4};
    const float* in[] = {l, r};
    float* out[] = {l, r};
    passThrough(in, 2, out, 2, 2, 0);
    CHECK(l[1] == 2);
    CHECK(r[1] == 4);
}

TEST_CASE("pass-through duplicates mono input to a stereo output") {
    float m[] = {0.5f, -0.5f};
    float outL[2] = {}, outR[2] = {};
    const float* in[] = {m};
    float* out[] = {outL, outR};
    const uint64_t mask = passThrough(in, 1, out, 2, 2, 0b1);
    CHECK(outL[1] == -0.5f);
    CHECK(outR[1] == -0.5f);
    CHECK(mask == 0b11);   // both outputs inherit the constant flag
}

TEST_CASE("pass-through silences outputs with no input") {
    float outL[2] = {9, 9}, outR[2] = {9, 9};
    float* out[] = {outL, outR};
    CHECK(passThrough(nullptr, 0, out, 2, 2, 0) == 0b11);
    CHECK(outL[0] == 0.0f);
    CHECK(outR[1] == 0.0f);
}

TEST_CASE("pass-through maps the constant mask per channel") {
    float l[] = {1}, r[] = {2};
    float outL[1], outR[1];
    const float* in[] = {l, r};
    float* out[] = {outL, outR};
    CHECK(passThrough(in, 2, out, 2, 1, 0b10) == 0b10);
}

TEST_CASE("denormal guard flushes denormals to zero and restores the mode") {
    volatile float tiny = 1e-40f;   // denormal
    volatile float one = 1.0f;
    {
        const p31::DenormalGuard guard;
        CHECK(tiny * one == 0.0f);
    }
    CHECK(tiny * one != 0.0f);
}
