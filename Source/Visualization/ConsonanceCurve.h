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
    void compute(const scalatrix::Spectrum& spectrum, float logBaseline = 0.5f);
    float consonanceAt(float cents) const;
    void computeIntervals(const std::vector<float>& noteFreqs);
    const ConsonanceCurveData& getData() const { return data_; }

private:
    ConsonanceCurveData data_;
    scalatrix::ConsonanceCurve curveResult_;
};
