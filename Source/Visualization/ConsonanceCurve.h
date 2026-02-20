#pragma once

#include <scalatrix/consonance.hpp>
#include <scalatrix/spectrum.hpp>
#include <vector>
#include <cmath>

static constexpr int kCurveResolution = 4000;  // 0.5ct steps over 2000ct
static constexpr float kCurveMaxCents = 2000.0f;

struct ConsonanceCurveData
{
    std::vector<float> plCurve;
    std::vector<float> hullCurve;
    std::vector<float> spikyCurve;
    std::vector<float> consonance;

    // Interval lines (cents positions of currently sounding intervals)
    std::vector<float> intervalCents;
    std::vector<float> intervalConsonance;
};

class ConsonanceCurveCalculator
{
public:
    // Compute full PL + Hull₃ + spiky₃ + C(x) using scalatrix
    void compute(const scalatrix::Spectrum& spectrum);

    // Get consonance value at a specific cents position
    float consonanceAt(float cents) const;

    // Get interval data for currently active note pairs
    void computeIntervals(const std::vector<float>& noteFreqs);

    const ConsonanceCurveData& getData() const { return data_; }

private:
    ConsonanceCurveData data_;
    // Cache the hull result for interval lookups
    scalatrix::HullResult hullResult_;
};
