#pragma once

#include <algorithm>
#include <cmath>

// Multi-stage (DAHDSR) envelope, output in [0, 1].  Times in seconds; sustain
// is a level.  Advanced at control rate by dt.  The basic UI exposes A/D/S/R;
// expert adds Delay/Hold and the curve.
struct Envelope
{
    float delay   = 0.0f;
    float attack  = 0.005f;
    float hold    = 0.0f;
    float decay   = 0.2f;
    float sustain = 1.0f;
    float release = 0.3f;
    float curve   = 0.0f;   // 0 = linear, >0 = more exponential

    enum class Stage { Idle, Delay, Attack, Hold, Decay, Sustain, Release };
    Stage stage = Stage::Idle;
    float value  = 0.0f;
    float t      = 0.0f;      // time in current stage
    float relFrom = 0.0f;     // level at release start

    void gate(bool on)
    {
        if (on)  { stage = (delay > 0.0f) ? Stage::Delay : Stage::Attack; t = 0.0f; }
        else if (stage != Stage::Idle) { stage = Stage::Release; t = 0.0f; relFrom = value; }
    }

    bool isIdle() const { return stage == Stage::Idle; }

    // Shape a 0..1 linear ramp by the curve amount.
    float shaped(float x) const
    {
        if (curve <= 0.0f) return x;
        float k = 1.0f + curve * 4.0f;
        return (std::pow(k, x) - 1.0f) / (k - 1.0f);
    }

    float advance(double dt)
    {
        const float d = static_cast<float>(dt);
        switch (stage)
        {
            case Stage::Idle:    value = 0.0f; break;
            case Stage::Delay:
                t += d; if (t >= delay) { stage = Stage::Attack; t = 0.0f; }
                value = 0.0f; break;
            case Stage::Attack:
                t += d;
                if (attack <= 0.0f || t >= attack) { value = 1.0f; stage = Stage::Hold; t = 0.0f; }
                else value = shaped(t / attack);
                break;
            case Stage::Hold:
                t += d; value = 1.0f;
                if (t >= hold) { stage = Stage::Decay; t = 0.0f; }
                break;
            case Stage::Decay:
                t += d;
                if (decay <= 0.0f || t >= decay) { value = sustain; stage = Stage::Sustain; t = 0.0f; }
                else value = 1.0f - shaped(t / decay) * (1.0f - sustain);
                break;
            case Stage::Sustain:
                value = sustain; break;
            case Stage::Release:
                t += d;
                if (release <= 0.0f || t >= release) { value = 0.0f; stage = Stage::Idle; }
                else value = relFrom * (1.0f - shaped(t / release));
                break;
        }
        return value;
    }

    float current() const { return value; }
};
