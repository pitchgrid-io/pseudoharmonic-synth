#pragma once

#include <cmath>
#include <cstdint>

// Sub-audio Shape Generator / LFO.  Outputs a bipolar value in [-1, 1].
// Shapes mirror the EaganMatrix SG set; the basic UI exposes the common ones.
// Advanced per block at control rate.
enum class SGShape
{
    Sine = 0, Triangle, RampUp, RampDown, Square, Pulse, SampleHold, Hann
};

struct ShapeGenerator
{
    float   rateHz    = 1.0f;
    SGShape shape     = SGShape::Sine;
    bool    retrigger = true;   // reset phase on gate
    bool    oneShot   = false;  // single cycle then hold at end (envelope-ish)
    float   pulseWidth = 0.5f;  // for Pulse

    // runtime
    float    phase = 0.0f;      // [0,1)
    float    value = 0.0f;      // last output, [-1,1]
    uint32_t rngState = 0x9E3779B9u;
    float    shValue = 0.0f;    // held sample for SampleHold
    bool     finished = false;  // oneShot completed

    void reset()
    {
        phase = 0.0f; value = 0.0f; finished = false;
        shValue = nextRand();
    }

    void gate(bool on)
    {
        if (on && retrigger) reset();
    }

    // Advance by dt seconds, return current value.
    float advance(double dt)
    {
        if (!(oneShot && finished))
        {
            float prevPhase = phase;
            phase += static_cast<float>(rateHz * dt);
            if (phase >= 1.0f)
            {
                if (oneShot) { phase = 1.0f; finished = true; }
                else         { phase -= std::floor(phase); }
                // new random sample each cycle for SampleHold
                if (prevPhase <= 1.0f) shValue = nextRand();
            }
        }
        value = shapeValue();
        return value;
    }

    float current() const { return value; }

private:
    float nextRand()
    {
        // xorshift32 → [-1,1]
        rngState ^= rngState << 13; rngState ^= rngState >> 17; rngState ^= rngState << 5;
        return (static_cast<float>(rngState) / 2147483648.0f) - 1.0f;
    }

    float shapeValue() const
    {
        const float twoPi = 6.28318530718f;
        switch (shape)
        {
            case SGShape::Sine:       return std::sin(twoPi * phase);
            case SGShape::Triangle:   return 1.0f - 4.0f * std::fabs(phase - 0.5f);
            case SGShape::RampUp:     return 2.0f * phase - 1.0f;
            case SGShape::RampDown:   return 1.0f - 2.0f * phase;
            case SGShape::Square:     return (phase < 0.5f) ? 1.0f : -1.0f;
            case SGShape::Pulse:      return (phase < pulseWidth) ? 1.0f : -1.0f;
            case SGShape::SampleHold:  return shValue;
            case SGShape::Hann:       return -std::cos(twoPi * phase); // 0..1..0 → -1..1
        }
        return 0.0f;
    }
};
