#pragma once

#include <array>
#include <cmath>
#include <complex>
#include <vector>

static constexpr int kMaxHarmonics = 32;

// Prime factorization helper
inline std::vector<int> primeFactors(int n)
{
    std::vector<int> factors;
    for (int i = 2; i * i <= n; ++i)
        while (n % i == 0) { factors.push_back(i); n /= i; }
    if (n > 1) factors.push_back(n);
    return factors;
}

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

    void impact(const std::array<float, kMaxHarmonics>& impactVec, float strength)
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
