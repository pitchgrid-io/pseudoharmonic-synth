#include "ConsonanceCurve.h"
#include <algorithm>

void ConsonanceCurveCalculator::compute(const scalatrix::Spectrum& spectrum, float logBaseline)
{
    const double f0 = 261.63; // Middle C
    const double margin = 300.0;

    // Compute consonance curve (PL + exact pyramids + hull + consonance)
    curveResult_ = scalatrix::computeConsonanceCurve(
        spectrum, f0, -margin, kCurveMaxCents + margin, 0.5, double(logBaseline));

    // Resample to kCurveResolution points over [0, kCurveMaxCents]
    data_.plCurve.resize(kCurveResolution);
    data_.hullCurve.resize(kCurveResolution);
    data_.spikyCurve.resize(kCurveResolution);
    data_.consonance.resize(kCurveResolution);

    const double step = double(kCurveMaxCents) / double(kCurveResolution - 1);

    for (int i = 0; i < kCurveResolution; ++i)
    {
        double targetCents = double(i) * step;

        auto it = std::lower_bound(
            curveResult_.cents.begin(), curveResult_.cents.end(), targetCents);

        if (it == curveResult_.cents.begin())
        {
            data_.plCurve[i] = float(curveResult_.pl[0]);
            data_.hullCurve[i] = float(curveResult_.hull[0]);
            data_.spikyCurve[i] = float(curveResult_.spiky[0]);
            data_.consonance[i] = float(curveResult_.consonance[0]);
        }
        else if (it == curveResult_.cents.end())
        {
            data_.plCurve[i] = float(curveResult_.pl.back());
            data_.hullCurve[i] = float(curveResult_.hull.back());
            data_.spikyCurve[i] = float(curveResult_.spiky.back());
            data_.consonance[i] = float(curveResult_.consonance.back());
        }
        else
        {
            size_t idx1 = size_t(it - curveResult_.cents.begin());
            size_t idx0 = idx1 - 1;
            double t = (targetCents - curveResult_.cents[idx0])
                       / (curveResult_.cents[idx1] - curveResult_.cents[idx0]);
            data_.plCurve[i] = float(curveResult_.pl[idx0]
                + t * (curveResult_.pl[idx1] - curveResult_.pl[idx0]));
            data_.hullCurve[i] = float(curveResult_.hull[idx0]
                + t * (curveResult_.hull[idx1] - curveResult_.hull[idx0]));
            data_.spikyCurve[i] = float(curveResult_.spiky[idx0]
                + t * (curveResult_.spiky[idx1] - curveResult_.spiky[idx0]));
            data_.consonance[i] = float(curveResult_.consonance[idx0]
                + t * (curveResult_.consonance[idx1] - curveResult_.consonance[idx0]));
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
