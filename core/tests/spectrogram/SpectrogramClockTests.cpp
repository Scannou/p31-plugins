//
//  SpectrogramClockTests.cpp
//  p31-core-tests (ported from foo_jl_spectrogram_mac)
//
//  Unit tests for the column clock: an even column count per frame from an
//  unevenly stepping stream time, coasting through gaps, and restarts; and,
//  new in the plugin, columns that wait for audio arriving in host buffers.
//

#include "../TestHarness.h"
#include <p31/spectrogram/SpectrogramClock.h>

#include <cmath>
#include <cstdlib>
#include <string>
#include <vector>

using p31::spectrogram::SpectrogramClock;

namespace {

const double kFrame = 1.0 / 60.0;
const double kHop = 1.0 / 120.0;   // 2 columns per frame

// Stream time as the core reports it: updated in uneven steps, and absent
// for three frames every two seconds (the stall seen in recordings).
struct SteppingStream {
    double reported = 0.0;
    bool at(int frame, double host, double& out) {
        if (frame % 120 >= 117) return false;
        if (frame % 3 == 0) reported = host + (frame % 7) * 0.002;  // 0..12 ms of jitter
        out = reported;
        return true;
    }
};

// Run `frames` frames from `startFrame`, rendering every due column.
// Returns the column count of each frame.
std::vector<int> run(SpectrogramClock& clock, SteppingStream& stream, int startFrame, int frames) {
    std::vector<int> counts;
    for (int f = startFrame; f < startFrame + frames; ++f) {
        const double host = f * kFrame;
        double t = 0.0;
        const bool have = stream.at(f, host, t);
        const int n = clock.due(host, have, t, kHop);
        for (int i = 0; i < n; ++i) clock.advance(kHop);
        counts.push_back(n);
    }
    return counts;
}

// Picture position each frame: rendered columns plus the sub-column phase.
// Its step per frame is what the eye sees as the scroll speed.
void testEvenMotion() {
    g_context = "even motion";
    for (double fps : {60.0, 72.0, 75.0, 120.0}) {
        SpectrogramClock clock;
        SteppingStream stream;
        const double frame = 1.0 / fps;
        long columns = 0;
        double previous = -1.0;
        int even = 0, frames = 0;
        for (int f = 0; f < (int)(fps * 12); ++f) {
            const double host = f * frame;
            double t = 0.0;
            const bool have = stream.at(f, host, t);
            const int n = clock.due(host, have, t, kHop);
            for (int i = 0; i < n; ++i) clock.advance(kHop);
            columns += n;
            const double position = columns + clock.phase(kHop);
            if (f > (int)fps * 2 && previous >= 0.0) {
                const double expected = 120.0 / fps;
                even += std::fabs((position - previous) - expected) < 0.25 ? 1 : 0;
                frames++;
            }
            previous = position;
        }
        g_context = "even motion " + std::to_string((int)fps) + " fps";
        EXPECT(even >= frames * 95 / 100, "at least 95% of frames step within a quarter column of the mean");
    }
}

void testEvenPace() {
    g_context = "even pace";
    SpectrogramClock clock;
    SteppingStream stream;
    run(clock, stream, 0, 120);  // settle
    const std::vector<int> counts = run(clock, stream, 120, 600);

    int even = 0, total = 0, stalls = 0;
    for (int n : counts) {
        even += n == 2 ? 1 : 0;
        stalls += n == 0 ? 1 : 0;
        total += n;
    }
    EXPECT(even >= (int)counts.size() * 95 / 100, "at least 95% of frames move exactly two columns");
    EXPECT_EQ(stalls, 0, "no frame stands still through the stream's gaps");
    // Ten seconds at 120 columns per second, give or take one frame.
    EXPECT(total >= 1198 && total <= 1202, "keeps up with the stream over time");
}

void testFollowsDrift() {
    g_context = "drift";
    // The stream runs 0.5% fast: the clock must follow it, not host time.
    SpectrogramClock clock;
    int total = 0;
    for (int f = 0; f < 60 * 60; ++f) {
        const double host = f * kFrame;
        const int n = clock.due(host, true, host * 1.005, kHop);
        for (int i = 0; i < n; ++i) clock.advance(kHop);
        if (f > 0) total += n;
    }
    // One minute of stream time (60.3 s) at 120 columns per second.
    EXPECT(std::abs(total - (int)(60.3 * 120)) <= 20, "column count follows the stream's rate");
}

void testRestart() {
    g_context = "restart";
    SpectrogramClock clock;
    EXPECT_EQ(clock.due(10.0, false, 0.0, kHop), 0, "no stream time yet: nothing due");
    EXPECT_EQ(clock.due(10.0, true, 5.0, kHop), 0, "first stream time anchors");
    EXPECT_FLOAT_EQ(clock.next(kHop), 5.0 - SpectrogramClock::kRenderDelay + kHop, "grid starts behind the stream time");

    int n = clock.due(10.0 + 0.1, true, 5.1, kHop);
    EXPECT(n >= 11 && n <= 13, "about 100 ms of columns after 100 ms");
    for (int i = 0; i < n; ++i) clock.advance(kHop);

    // Seek: the stream time starts over near zero.
    EXPECT_EQ(clock.due(10.2, true, 0.0, kHop), 0, "a jump back resets");
    EXPECT_EQ(clock.due(10.2 + kFrame, true, kFrame, kHop), 0, "and re-anchors on the next frame");
    EXPECT(clock.next(kHop) < 0.1, "grid continues from the new position");

    // Pause: the controller resets; resuming re-anchors without a burst.
    clock.reset();
    EXPECT_EQ(clock.due(50.0, true, 3.0, kHop), 0, "re-anchors after a reset");
    n = clock.due(50.0 + kFrame, true, 3.0 + kFrame, kHop);
    EXPECT(n >= 1 && n <= 3, "no catch-up burst for the paused time");
}

void testUnreadColumnsSkipped() {
    g_context = "unread columns";
    // The controller renders nothing for two seconds (reads fail), then
    // reads succeed again: the grid has kept within kMaxGap of the display
    // clock instead of staying at the start of the stretch.
    SpectrogramClock clock;
    clock.due(0.0, true, 0.0, kHop);
    for (int f = 1; f <= 120; ++f) clock.due(f * kFrame, true, f * kFrame, kHop);
    const int n = clock.due(121 * kFrame, true, 121 * kFrame, kHop);
    EXPECT(n > 0 && n <= (int)(SpectrogramClock::kMaxGap / kHop) + 1, "at most kMaxGap of columns due");
    EXPECT(clock.next(kHop) > 121 * kFrame - SpectrogramClock::kRenderDelay - SpectrogramClock::kMaxGap,
          "grid moved past the unread stretch");
}

// The plugin's stream time: the audio written so far, one host buffer at a
// time. Returns how many due columns found their audio missing over
// `seconds`, after two seconds to settle. `followSteps` reports each buffer
// to the clock, as the editor does.
int missingColumns(double bufferSeconds, bool followSteps, double seconds = 10.0) {
    SpectrogramClock clock;
    double written = 0.0;
    int missing = 0;
    const int frames = (int)((seconds + 2.0) / kFrame);
    for (int f = 0; f < frames; ++f) {
        const double host = f * kFrame;
        const double now = std::floor(host / bufferSeconds) * bufferSeconds;
        if (now > written) {
            if (followSteps) clock.noteStreamStep(now - written);
            written = now;
        }
        const int n = clock.due(host, true, written, kHop);
        for (int i = 0; i < n; ++i) {
            if (clock.next(kHop) > written) {
                if (f * kFrame > 2.0) missing += n - i;
                break;
            }
            clock.advance(kHop);
        }
    }
    return missing;
}

void testHostBuffers() {
    // Up to 4096 frames at 44.1 kHz the fixed delay is enough.
    for (double buffer : {64 / 48000.0, 512 / 48000.0, 2048 / 44100.0, 4096 / 44100.0}) {
        g_context = "host buffer " + std::to_string((int)(buffer * 1000)) + " ms";
        EXPECT_EQ(missingColumns(buffer, true), 0, "every column finds its audio");
        SpectrogramClock clock;
        clock.noteStreamStep(buffer);
        EXPECT(clock.renderDelay() < 0.07, "small buffers keep the delay short");
    }
    // Longer buffers need the delay to follow them.
    for (double buffer : {8192 / 44100.0, 16384 / 48000.0}) {
        g_context = "host buffer " + std::to_string((int)(buffer * 1000)) + " ms";
        EXPECT_EQ(missingColumns(buffer, true), 0, "every column finds its audio");
        EXPECT(missingColumns(buffer, false) > 0, "the fixed delay alone would stall");
    }

    g_context = "render delay";
    SpectrogramClock clock;
    EXPECT_FLOAT_EQ(clock.renderDelay(), SpectrogramClock::kRenderDelay, "starts at the fixed delay");
    clock.noteStreamStep(0.2);
    const double longDelay = clock.renderDelay();
    EXPECT(longDelay > 0.1, "a long buffer lengthens the delay");
    for (int i = 0; i < 400; ++i) clock.noteStreamStep(0.01);
    EXPECT_FLOAT_EQ(clock.renderDelay(), SpectrogramClock::kRenderDelay, "and short ones bring it back");
}

} // namespace

TEST_CASE("Spectrogram clock: even pace") { testEvenPace(); }
TEST_CASE("Spectrogram clock: even motion") { testEvenMotion(); }
TEST_CASE("Spectrogram clock: follows drift") { testFollowsDrift(); }
TEST_CASE("Spectrogram clock: restart") { testRestart(); }
TEST_CASE("Spectrogram clock: unread columns are skipped") { testUnreadColumnsSkipped(); }
TEST_CASE("Spectrogram clock: audio arriving in host buffers") { testHostBuffers(); }
