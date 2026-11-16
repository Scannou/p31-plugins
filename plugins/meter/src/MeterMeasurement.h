//
//  MeterMeasurement.h
//  p31-meter (in place of foo_jl_levelmeter_mac MeterSource and the controller's timer)
//
//  The meter's readings. Reads every frame the plugin receives from the ring
//  buffer, runs the meter engine over it and keeps the latest snapshot.
//
//  It belongs to the plugin, not to the editor: a timer on the main thread
//  keeps it measuring while the editor is closed or covered, so the
//  integrated loudness misses nothing. An open editor also updates it on
//  every display frame, for readings as fresh as the picture.
//
//  Every call may come from any thread (hosts differ on what the main thread
//  is); they exclude each other.
//

#pragma once

#include "MeterModule.h"

#include <p31/meter/MeterEngine.h>

class MeterMeasurement {
public:
    // Starts measuring.
    explicit MeterMeasurement(MeterModule& module);
    ~MeterMeasurement();
    MeterMeasurement(const MeterMeasurement&) = delete;
    MeterMeasurement& operator=(const MeterMeasurement&) = delete;

    // Stop and continue (P31 Suite with the meter panel hidden). What was
    // measured stays; audio that passes meanwhile is not counted.
    void setRunning(bool running);

    // The module's settings changed.
    void settingsChanged();

    // Take in the audio received since the last call and move the display
    // ballistics on by the time that passed.
    void update();

    p31::meter::Snapshot snapshot() const;
    // Time since audio last arrived; large before any has.
    double quietSeconds() const;

    // Clear held maxima and clip lights.
    void resetPeaks();
    // Start a new loudness measurement.
    void resetLoudness();

private:
    struct State;
    State* _state;   // shared with the timer, which outlives a late tick
};
