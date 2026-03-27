#pragma once

#include <scalatrix/consonance.hpp>
#include <scalatrix/spectrum.hpp>
#include <vector>
#include <string>
#include <cmath>

struct TuningParams;
struct NodeConsonance;

static constexpr int kCurveResolution = 4000;  // 0.5ct steps over 2000ct
static constexpr float kCurveMaxCents = 2000.0f;

struct ScaleDegreeInfo
{
    float cents;
    float consonance;
    std::string label;
    float tuningX;  // tuning_coord.x (0 = root, 1 = 1200ct)
    float tuningY;  // tuning_coord.y (0 = bottom, 1 = top)
    bool inScale;   // true if node is in the base MOS scale
};

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
    float getEffectiveLogBaseline() const { return effectiveLogBaseline_; }
    void computeIntervals(const std::vector<float>& noteFreqs);
    void computeIntervals(const std::vector<float>& noteFreqs,
                          const std::vector<int>& midiNotes,
                          const TuningParams& tuning);
    std::vector<ScaleDegreeInfo> computeScaleDegrees(const TuningParams& tuning) const;
    std::vector<NodeConsonance> computeNodeConsonances(const TuningParams& tuning) const;
    const ConsonanceCurveData& getData() const { return data_; }

private:
    ConsonanceCurveData data_;
    scalatrix::ConsonanceCurve curveResult_;
    float effectiveLogBaseline_ = 0.5f;
};
