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
    bool releasing = false;

    // Per-harmonic oscillator state (complex rotation)
    std::array<std::complex<float>, kMaxHarmonics> x{};
    std::array<std::complex<float>, kMaxHarmonics> rot{};

    void noteOn(int note, float vel, float freq, int channel = 0)
    {
        midiNote = note;
        velocity = vel;
        baseFreq = freq;
        mpeChannel = channel;
        active = true;
        releasing = false;
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
        float freq = baseFreq + detuneAdd;
        const auto& rates = releasing ? releaseRates : decayRates;
        for (int h = 0; h < kMaxHarmonics; ++h)
        {
            float factor = std::exp(-rates[h] / sampleRate);
            float phase = 2.0f * float(M_PI) * freq * freqRatios[h] / sampleRate;
            rot[h] = factor * std::complex<float>(std::cos(phase), std::sin(phase));
        }
    }

    float processSample()
    {
        float sum = 0.0f;
        for (int h = 0; h < kMaxHarmonics; ++h)
        {
            x[h] *= rot[h];
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
