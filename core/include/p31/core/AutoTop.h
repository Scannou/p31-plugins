//
//  AutoTop.h
//  p31-core
//
//  The top of a dB display that follows the material, so quiet and loud
//  material both fill it: P31 Scope's auto gain (StereoAnalysis.h) for a
//  display with a dB scale, where the scale moves instead of the signal.
//
//  Follows the loudest level shown (instant attack, slow release) and aims
//  the top at the level that puts it `headroomDb` below: up at once, so
//  nothing goes past the top, and down gently. Silence holds the envelope,
//  so the top does not creep down between tracks and blow out the first
//  notes of the next one.
//

#pragma once

namespace p31 {

class AutoTop {
public:
    struct Settings {
        float  headroomDb = 6.0f;      // loudest level this far below the top
        float  minTopDb = -60.0f;
        float  maxTopDb = 0.0f;
        float  silenceDb = -100.0f;    // levels at or below this hold the envelope
        double releaseSeconds = 3.0;   // envelope fall, as the scope's amplitude time constant

        // For a display `rangeDb` tall, as P31 Spectrum and P31 Spectrogram
        // have: the loudest level 90% of the way up, like the scope's trace,
        // and the top between -60 and 0 dBFS.
        static Settings forDisplay(float rangeDb) {
            Settings s;
            s.headroomDb = 0.1f * rangeDb;
            return s;
        }
    };

    void configure(const Settings& settings);
    // Start over from `topDb` (the display's own top), with no history.
    void reset(float topDb);

    // `loudestDb` is the loudest level shown since the last call (-inf or
    // anything at or below the silence level for none), `seconds` the time
    // it covers. Returns the new top.
    float process(float loudestDb, double seconds);
    float topDb() const { return _top; }

private:
    Settings _settings;
    float _env = -1e9f;   // dB; far below everything until a level arrives
    float _top = 0.0f;
};

} // namespace p31
