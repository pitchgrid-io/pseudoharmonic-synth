#pragma once

#include <vector>
#include <array>
#include <cmath>

static constexpr int kCurveResolution = 600; // 0-1200¢ at 2¢ resolution
static constexpr float kCurveMaxCents = 1200.0f;

struct ConsonanceCurveData
{
    std::array<float, kCurveResolution> plCurve{};     // Raw Plomp-Levelt dissonance
    std::array<float, kCurveResolution> hullCurve{};   // Hull₃ envelope
    std::array<float, kCurveResolution> spikyCurve{};  // Spiky₃ consonance (hull - PL)
    std::array<float, kCurveResolution> consonance{};  // C(x) = max(0, 1 + 0.5*log10(norm_spiky))

    // Interval lines (cents positions of currently sounding intervals)
    std::vector<float> intervalCents;
    std::vector<float> intervalConsonance;
};

class ConsonanceCurveCalculator
{
public:
    // Compute full PL + Hull₃ + spiky₃ + C(x) for current spectrum
    // freqRatios: partial frequency ratios (e.g. [1, 2, 3.0167, 4, 5.0267, ...])
    // amplitudes: partial amplitudes
    // numPartials: how many partials to use
    void compute(const float* freqRatios, const float* amplitudes, int numPartials);

    // Get consonance value at a specific cents position
    float consonanceAt(float cents) const;

    // Get interval data for currently active note pairs
    void computeIntervals(const std::vector<float>& noteFreqs);

    const ConsonanceCurveData& getData() const { return data_; }

private:
    // Plomp-Levelt roughness between two pure tones
    static float plompLeveltRoughness(float f1, float a1, float f2, float a2);

    // Compute PL dissonance for complex tone at given interval (cents)
    float plDissonance(float cents, const float* freqRatios, const float* amplitudes, int n) const;

    ConsonanceCurveData data_;
};
