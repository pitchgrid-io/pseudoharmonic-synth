#include "ConsonanceCurve.h"
#include <algorithm>

void ConsonanceCurveCalculator::compute(const scalatrix::Spectrum& spectrum, float logBaseline)
{
    const double f0 = 261.63; // Middle C
    const double margin = 300.0;

    // 1. Compute PL curve with margin
    auto plCurve = scalatrix::computePLCurve(
        spectrum, f0, -margin, kCurveMaxCents + margin);

    // 2. Compute pyramid consonance curve
    pyramidResult_ = scalatrix::computePyramidCurve(
        spectrum, f0, -margin, kCurveMaxCents + margin, 0.5, double(logBaseline));

    // 3. Resample to kCurveResolution points over [0, kCurveMaxCents]
    data_.plCurve.resize(kCurveResolution);
    data_.pyramidCurve.resize(kCurveResolution);
    data_.consonance.resize(kCurveResolution);

    const double step = double(kCurveMaxCents) / double(kCurveResolution - 1);

    for (int i = 0; i < kCurveResolution; ++i)
    {
        double targetCents = double(i) * step;

        // Interpolate PL curve
        auto itPL = std::lower_bound(
            plCurve.cents.begin(), plCurve.cents.end(), targetCents);

        if (itPL == plCurve.cents.begin())
        {
            data_.plCurve[i] = float(plCurve.pl[0]);
        }
        else if (itPL == plCurve.cents.end())
        {
            data_.plCurve[i] = float(plCurve.pl.back());
        }
        else
        {
            size_t idx1 = size_t(itPL - plCurve.cents.begin());
            size_t idx0 = idx1 - 1;
            double t = (targetCents - plCurve.cents[idx0])
                       / (plCurve.cents[idx1] - plCurve.cents[idx0]);
            data_.plCurve[i] = float(plCurve.pl[idx0]
                + t * (plCurve.pl[idx1] - plCurve.pl[idx0]));
        }

        // Interpolate pyramid consonance
        auto itPyr = std::lower_bound(
            pyramidResult_.cents.begin(), pyramidResult_.cents.end(), targetCents);

        if (itPyr == pyramidResult_.cents.begin())
        {
            data_.pyramidCurve[i] = float(pyramidResult_.pyramid[0]);
            data_.consonance[i] = float(pyramidResult_.consonance[0]);
        }
        else if (itPyr == pyramidResult_.cents.end())
        {
            data_.pyramidCurve[i] = float(pyramidResult_.pyramid.back());
            data_.consonance[i] = float(pyramidResult_.consonance.back());
        }
        else
        {
            size_t idx1 = size_t(itPyr - pyramidResult_.cents.begin());
            size_t idx0 = idx1 - 1;
            double t = (targetCents - pyramidResult_.cents[idx0])
                       / (pyramidResult_.cents[idx1] - pyramidResult_.cents[idx0]);
            data_.pyramidCurve[i] = float(pyramidResult_.pyramid[idx0]
                + t * (pyramidResult_.pyramid[idx1] - pyramidResult_.pyramid[idx0]));
            data_.consonance[i] = float(pyramidResult_.consonance[idx0]
                + t * (pyramidResult_.consonance[idx1] - pyramidResult_.consonance[idx0]));
        }
    }
}

float ConsonanceCurveCalculator::consonanceAt(float cents) const
{
    if (data_.consonance.empty()) return 0.0f;
    float idx = cents / (kCurveMaxCents / float(kCurveResolution - 1));
    int i0 = std::clamp(int(idx), 0, kCurveResolution - 2);
    float t = idx - float(i0);
    return data_.consonance[i0] + t * (data_.consonance[i0 + 1] - data_.consonance[i0]);
}

void ConsonanceCurveCalculator::computeIntervals(const std::vector<float>& noteFreqs)
{
    data_.intervalCents.clear();
    data_.intervalConsonance.clear();

    for (size_t i = 0; i < noteFreqs.size(); ++i)
    {
        for (size_t j = i + 1; j < noteFreqs.size(); ++j)
        {
            float higher = std::max(noteFreqs[i], noteFreqs[j]);
            float lower = std::min(noteFreqs[i], noteFreqs[j]);
            float cents = 1200.0f * std::log2(higher / lower);
            while (cents > kCurveMaxCents) cents -= kCurveMaxCents;
            data_.intervalCents.push_back(cents);
            data_.intervalConsonance.push_back(consonanceAt(cents));
        }
    }
}
