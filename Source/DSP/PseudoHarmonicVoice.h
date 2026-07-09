#pragma once

#include <array>
#include <cmath>
#include <complex>

static constexpr int kMaxHarmonics = 32;

// Stack-allocated prime factorization (no heap allocation — safe for audio thread).
// For n <= 32, at most 5 factors (2^5 = 32). Capacity 8 covers any reasonable harmonic.
struct PrimeFactors
{
    std::array<int, 8> data{};
    int count = 0;

    const int* begin() const { return data.data(); }
    const int* end()   const { return data.data() + count; }
};

inline PrimeFactors primeFactors(int n)
{
    PrimeFactors f;
    for (int i = 2; i * i <= n; ++i)
        while (n % i == 0) { f.data[f.count++] = i; n /= i; }
    if (n > 1) f.data[f.count++] = n;
    return f;
}

// TPT state-variable filter coefficients (shared across voices, computed once
// per block from the global filter params).
struct FilterCoeffs
{
    float g = 0.0f, k = 2.0f, a1 = 0.0f, a2 = 0.0f, a3 = 0.0f;

    void set(float cutoffHz, float reso, float sampleRate)
    {
        float fc = cutoffHz;
        float nyq = 0.49f * sampleRate;
        if (fc > nyq) fc = nyq;
        if (fc < 5.0f) fc = 5.0f;
        g = std::tan(float(M_PI) * fc / sampleRate);
        k = 2.0f - 1.9f * (reso < 0.0f ? 0.0f : (reso > 1.0f ? 1.0f : reso)); // 2..0.1
        a1 = 1.0f / (1.0f + g * (g + k));
        a2 = g * a1;
        a3 = g * a2;
    }
};

struct PseudoHarmonicVoice
{
    bool active = false;
    int midiNote = -1;
    int mpeChannel = -1;
    float velocity = 0.0f;
    float baseFreq = 0.0f;
    float detuneAdd = 0.0f;
    float masterBendSemitones = 0.0f;  // from manager channel (ch 1)
    float noteBendSemitones = 0.0f;    // from member channel (per-note)
    float pressure = 1.0f;             // MPE channel pressure (0..1), 1.0 = no scaling
    float noiseEnvelope = 0.0f;        // ramps 0→1 at decay rate
    bool releasing = false;
    bool sustained = false;            // held by sustain pedal

    // Per-harmonic oscillator state (complex rotation)
    std::array<std::complex<float>, kMaxHarmonics> x{};
    std::array<std::complex<float>, kMaxHarmonics> rot{};

    // Per-harmonic sustain: target amplitude and per-sample excitation
    std::array<float, kMaxHarmonics> sustainLevel{};
    std::array<float, kMaxHarmonics> sustainExcitation{};

    // Per-voice state-variable filter state.
    float filt_ic1eq = 0.0f;
    float filt_ic2eq = 0.0f;

    // Apply the multimode filter to one sample. type: 0=Off,1=LP,2=HP,3=BP,4=Notch.
    float applyFilter(float in, const FilterCoeffs& c, int type)
    {
        if (type <= 0) return in;   // Off
        float v3 = in - filt_ic2eq;
        float v1 = c.a1 * filt_ic1eq + c.a2 * v3;
        float v2 = filt_ic2eq + c.a2 * filt_ic1eq + c.a3 * v3;
        filt_ic1eq = 2.0f * v1 - filt_ic1eq;
        filt_ic2eq = 2.0f * v2 - filt_ic2eq;
        switch (type)
        {
            case 1: return v2;                       // Low-pass
            case 2: return in - c.k * v1 - v2;       // High-pass
            case 3: return v1;                       // Band-pass
            case 4: return in - c.k * v1;            // Notch
            default: return in;
        }
    }

    void noteOn(int note, float vel, float freq, int channel = 0)
    {
        midiNote = note;
        velocity = vel;
        baseFreq = freq;
        mpeChannel = channel;
        active = true;
        releasing = false;
        sustained = false;
        pressure = 1.0f;
        noiseEnvelope = 0.0f;
        // Don't reset x — allows retriggering with continuity
    }

    void noteOff()
    {
        releasing = true;
    }

    // Impact vector is complex so each partial can be struck at its own phase
    // (phaseSpread).  With phaseSpread == 0 the vector is purely real, matching
    // the previous behaviour.
    void impact(const std::array<std::complex<float>, kMaxHarmonics>& impactVec, float strength)
    {
        for (int h = 0; h < kMaxHarmonics; ++h)
            x[h] += strength * impactVec[h];
    }

    void updateRotation(const std::array<float, kMaxHarmonics>& freqRatios,
                        const std::array<float, kMaxHarmonics>& decayRates,
                        const std::array<float, kMaxHarmonics>& releaseRates,
                        float sampleRate)
    {
        float bendTotal = masterBendSemitones + noteBendSemitones;
        float freq = baseFreq * std::pow(2.0f, bendTotal / 12.0f) + detuneAdd;
        const auto& rates = releasing ? releaseRates : decayRates;
        for (int h = 0; h < kMaxHarmonics; ++h)
        {
            float factor = std::exp(-rates[h] / sampleRate);
            float phase = 2.0f * float(M_PI) * freq * freqRatios[h] / sampleRate;
            rot[h] = factor * std::complex<float>(std::cos(phase), std::sin(phase));

            // Sustain excitation: counteract decay at the target level
            sustainExcitation[h] = releasing ? 0.0f : sustainLevel[h] * (1.0f - factor);
        }
    }

    float processSample(float pressureScale = 1.0f)
    {
        float sum = 0.0f;
        for (int h = 0; h < kMaxHarmonics; ++h)
        {
            x[h] *= rot[h];

            // Add sustain excitation in the direction of the oscillator
            float exc = sustainExcitation[h] * pressureScale;
            if (exc > 0.0f)
            {
                float mag = std::abs(x[h]);
                if (mag > 1e-15f)
                    x[h] += exc * (x[h] / mag);
                else
                    x[h] += std::complex<float>(0.0f, exc * 0.001f);
            }

            sum += x[h].imag();
        }
        return sum;
    }

    float energy() const
    {
        float e = 0.0f;
        for (int h = 0; h < kMaxHarmonics; ++h)
            e += std::norm(x[h]);
        return e;
    }
};
