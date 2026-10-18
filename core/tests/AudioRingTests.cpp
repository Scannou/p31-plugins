//
//  AudioRingTests.cpp
//  p31-core-tests
//

#include <doctest/doctest.h>
#include <p31/core/AudioRing.h>

#include <atomic>
#include <chrono>
#include <cstdint>
#include <random>
#include <thread>
#include <vector>

using p31::AudioRing;
using p31::AudioRingReader;

namespace {

// Ramp values stay exact in a float up to 2^24; wrap well below that.
constexpr uint64_t kRampWrap = uint64_t{1} << 20;
float rampAt(uint64_t pos) { return static_cast<float>(pos % kRampWrap); }

// Writes `frames` stereo frames continuing the ramp: left = ramp, right = -ramp.
struct RampWriter {
    uint64_t pos = 0;
    std::vector<float> left, right;

    void write(AudioRing& ring, uint32_t frames) {
        left.resize(frames);
        right.resize(frames);
        for (uint32_t i = 0; i < frames; ++i) {
            left[i] = rampAt(pos + i);
            right[i] = -rampAt(pos + i);
        }
        const float* data[] = {left.data(), right.data()};
        ring.write(data, 2, frames);
        pos += frames;
    }
};

// Checks that `frames` interleaved stereo frames are the ramp from `firstPos`.
bool isRamp(const float* out, size_t frames, uint64_t firstPos) {
    for (size_t i = 0; i < frames; ++i) {
        if (out[2 * i] != rampAt(firstPos + i) || out[2 * i + 1] != -rampAt(firstPos + i)) return false;
    }
    return true;
}

} // namespace

TEST_CASE("capacity is a power of two and channels are clamped") {
    CHECK(AudioRing(2, 1000).capacity() == 1024);
    CHECK(AudioRing(2, 1024).capacity() == 1024);
    CHECK(AudioRing(2, 0).capacity() == 1);
    CHECK(AudioRing(0, 16).channels() == 1);
    CHECK(AudioRing(5, 16).channels() == 2);
}

TEST_CASE("planar input comes out interleaved") {
    AudioRing ring(2, 16);
    AudioRingReader reader(ring);
    const float l[] = {1, 2, 3};
    const float r[] = {-1, -2, -3};
    const float* data[] = {l, r};
    ring.write(data, 2, 3);

    float out[32] = {};
    const auto res = reader.readNew(out, 16);
    REQUIRE(res.frames == 3);
    CHECK(res.skipped == 0);
    const float expected[] = {1, -1, 2, -2, 3, -3};
    for (int i = 0; i < 6; ++i) CHECK(out[i] == expected[i]);
}

TEST_CASE("channel count mismatches") {
    AudioRing ring(2, 16);
    AudioRingReader reader(ring);
    float out[32] = {};

    SUBCASE("mono input feeds both sides") {
        const float m[] = {0.5f, -0.25f};
        const float* data[] = {m};
        ring.write(data, 1, 2);
        REQUIRE(reader.readNew(out, 16).frames == 2);
        CHECK(out[0] == 0.5f);
        CHECK(out[1] == 0.5f);
        CHECK(out[2] == -0.25f);
        CHECK(out[3] == -0.25f);
    }
    SUBCASE("extra input channels are ignored") {
        const float a[] = {1}, b[] = {2}, c[] = {3};
        const float* data[] = {a, b, c};
        ring.write(data, 3, 1);
        REQUIRE(reader.readNew(out, 16).frames == 1);
        CHECK(out[0] == 1);
        CHECK(out[1] == 2);
    }
    SUBCASE("no input writes silence") {
        out[0] = out[1] = 9;
        ring.write(nullptr, 0, 4);
        REQUIRE(reader.readNew(out, 16).frames == 4);
        for (int i = 0; i < 8; ++i) CHECK(out[i] == 0.0f);
    }
    SUBCASE("mono ring keeps the first input channel") {
        AudioRing mono(1, 16);
        AudioRingReader monoReader(mono);
        const float l[] = {7}, r[] = {8};
        const float* data[] = {l, r};
        mono.write(data, 2, 1);
        REQUIRE(monoReader.readNew(out, 16).frames == 1);
        CHECK(out[0] == 7);
    }
}

TEST_CASE("variable block sizes and wraparound arrive in order") {
    AudioRing ring(2, 64);
    AudioRingReader reader(ring);
    RampWriter writer;
    std::vector<float> out(2 * 64);
    uint64_t readPos = 0;

    for (uint32_t block : {1u, 7u, 0u, 63u, 64u, 5u, 33u, 17u, 64u, 2u}) {
        writer.write(ring, block);
        const auto res = reader.readNew(out.data(), 64);
        CHECK(res.frames == block);
        CHECK(res.skipped == 0);
        CHECK(isRamp(out.data(), res.frames, readPos));
        readPos += res.frames;
    }
    CHECK(ring.writePosition() == readPos);
}

TEST_CASE("readNew keeps the most recent frames when more arrived than fit") {
    AudioRing ring(2, 64);
    AudioRingReader reader(ring);
    RampWriter writer;
    writer.write(ring, 50);

    std::vector<float> out(2 * 20);
    const auto res = reader.readNew(out.data(), 20);
    CHECK(res.frames == 20);
    CHECK(res.skipped == 30);
    CHECK(isRamp(out.data(), 20, 30));

    // Nothing new: nothing returned.
    CHECK(reader.readNew(out.data(), 20).frames == 0);
}

TEST_CASE("a reader that fell behind gets the last capacity frames") {
    AudioRing ring(2, 16);
    AudioRingReader reader(ring);
    RampWriter writer;
    for (int i = 0; i < 4; ++i) writer.write(ring, 10);   // 40 frames into 16 slots

    std::vector<float> out(2 * 64);
    const auto res = reader.readNew(out.data(), 64);
    CHECK(res.frames == 16);
    CHECK(res.skipped == 24);
    CHECK(isRamp(out.data(), 16, 24));
}

TEST_CASE("a single block larger than the capacity keeps its tail") {
    AudioRing ring(2, 16);
    AudioRingReader reader(ring);
    RampWriter writer;
    writer.write(ring, 40);
    CHECK(ring.writePosition() == 40);

    std::vector<float> out(2 * 64);
    const auto res = reader.readNew(out.data(), 64);
    CHECK(res.frames == 16);
    CHECK(isRamp(out.data(), 16, 24));
}

TEST_CASE("copy flags frames that were overwritten") {
    AudioRing ring(2, 16);
    RampWriter writer;
    writer.write(ring, 40);   // intact: 24..39

    std::vector<float> out(2 * 8, 9.0f);
    const uint64_t intact = ring.copy(20, 8, out.data());
    CHECK(intact == 24);
    for (int i = 0; i < 8; ++i) CHECK(out[i] == 0.0f);   // frames 20..23 zeroed
    CHECK(isRamp(out.data() + 8, 4, 24));
}

TEST_CASE("latest returns the newest frames without consuming them") {
    AudioRing ring(2, 64);
    AudioRingReader reader(ring);
    RampWriter writer;
    std::vector<float> out(2 * 32, 9.0f);

    SUBCASE("zero-padded at the front before enough audio arrived") {
        writer.write(ring, 10);
        CHECK(ring.latest(out.data(), 32) == 10);
        for (int i = 0; i < 2 * 22; ++i) CHECK(out[i] == 0.0f);
        CHECK(isRamp(out.data() + 2 * 22, 10, 0));
    }
    SUBCASE("exactly the newest frames once there are enough") {
        writer.write(ring, 100);
        CHECK(ring.latest(out.data(), 32) == 32);
        CHECK(isRamp(out.data(), 32, 68));
    }
    SUBCASE("the reader cursor is unaffected") {
        writer.write(ring, 10);
        ring.latest(out.data(), 32);
        CHECK(reader.readNew(out.data(), 32).frames == 10);
    }
    SUBCASE("more than the capacity is clamped") {
        AudioRing small(2, 8);
        RampWriter w;
        w.write(small, 20);
        std::vector<float> big(2 * 12, 9.0f);
        CHECK(small.latest(big.data(), 12) == 8);
        for (int i = 0; i < 8; ++i) CHECK(big[i] == 0.0f);
        CHECK(isRamp(big.data() + 8, 8, 12));
    }
}

TEST_CASE("a reader only sees frames written after it was created") {
    AudioRing ring(2, 64);
    RampWriter writer;
    writer.write(ring, 10);

    AudioRingReader reader(ring);
    std::vector<float> out(2 * 64);
    CHECK(reader.readNew(out.data(), 64).frames == 0);

    writer.write(ring, 5);
    const auto res = reader.readNew(out.data(), 64);
    CHECK(res.frames == 5);
    CHECK(isRamp(out.data(), 5, 10));

    writer.write(ring, 7);
    reader.skipToNow();
    CHECK(reader.readNew(out.data(), 64).frames == 0);
}

// The writer runs in its own thread with random block sizes; the reader
// checks that everything it gets is an intact, gap-accounted ramp. The ring
// is small so the reader regularly falls behind and overwrites happen during
// copies. Run under ThreadSanitizer via the `tsan` preset.
TEST_CASE("concurrent writer and reader never see torn or misplaced frames") {
    AudioRing ring(2, 1024);
    AudioRingReader reader(ring);
    std::atomic<bool> done{false};
    constexpr uint64_t kTotalFrames = 4'000'000;

    std::thread writerThread([&] {
        RampWriter writer;
        writer.left.reserve(512);
        writer.right.reserve(512);
        std::mt19937 rng(1);
        std::uniform_int_distribution<uint32_t> block(1, 512);
        while (writer.pos < kTotalFrames) {
            writer.write(ring, block(rng));
            if (rng() % 64 == 0) std::this_thread::yield();
        }
        done.store(true, std::memory_order_release);
    });

    std::vector<float> out(2 * 2048);
    uint64_t expectedNext = 0;
    uint64_t reads = 0, skippedTotal = 0, latestChecks = 0;
    bool ok = true;
    std::mt19937 rng(2);

    while (ok) {
        const bool finished = done.load(std::memory_order_acquire);

        const auto res = reader.readNew(out.data(), 1 + rng() % 1024);
        expectedNext += res.skipped;
        if (!isRamp(out.data(), res.frames, expectedNext)) ok = false;
        expectedNext += res.frames;
        skippedTotal += res.skipped;
        ++reads;

        const size_t want = 1 + rng() % 2048;
        const size_t got = ring.latest(out.data(), want);
        if (got > 0) {
            // The intact frames sit at the end and must be consecutive.
            const float* tail = out.data() + 2 * (want - got);
            const uint64_t first = static_cast<uint64_t>(tail[0]);
            if (!isRamp(tail, got, first)) ok = false;
            ++latestChecks;
        }

        if (finished) break;
        if (rng() % 16 == 0) std::this_thread::sleep_for(std::chrono::microseconds(rng() % 200));
    }
    writerThread.join();

    // Drain whatever arrived after the last read.
    const auto res = reader.readNew(out.data(), 1024);
    expectedNext += res.skipped;
    CHECK(isRamp(out.data(), res.frames, expectedNext));
    expectedNext += res.frames;

    CHECK(ok);
    CHECK(expectedNext == ring.writePosition());
    CHECK(reads > 100);
    CHECK(latestChecks > 100);
    MESSAGE("reads: " << reads << ", frames skipped: " << skippedTotal);
}
