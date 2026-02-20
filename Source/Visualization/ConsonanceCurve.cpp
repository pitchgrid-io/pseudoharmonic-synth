#include "ConsonanceCurve.h"
#include <algorithm>

void ConsonanceCurveCalculator::compute(const scalatrix::Spectrum& spectrum)
{
    const double f0 = 261.63; // Middle C
    const double margin = 300.0;

    // 1. Compute PL curve with margin for accurate hull computation
    auto plCurve = scalatrix::computePLCurve(
        spectrum, f0, -margin, kCurveMaxCents + margin);

    // 2. Compute Hull₃ (2nd derivative maxima + cubic spline)
    hullResult_ = scalatrix::computeHull3(plCurve);

    // 3. Find peak spiky value near 0 cents for normalization
    double peakSpiky = 0.0;
    for (size_t i = 0; i < hullResult_.cents.size(); ++i)
    {
        if (std::abs(hullResult_.cents[i]) < 50.0)
            peakSpiky = std::max(peakSpiky, hullResult_.spiky[i]);
    }
    if (peakSpiky <= 0.0)
    {
        for (size_t i = 0; i < hullResult_.spiky.size(); ++i)
            peakSpiky = std::max(peakSpiky, hullResult_.spiky[i]);
    }

    // 4. Resample to kCurveResolution points over [0, kCurveMaxCents]
    data_.plCurve.resize(kCurveResolution);
    data_.hullCurve.resize(kCurveResolution);
    data_.spikyCurve.resize(kCurveResolution);
    data_.consonance.resize(kCurveResolution);

    const double step = double(kCurveMaxCents) / double(kCurveResolution - 1);

    for (int i = 0; i < kCurveResolution; ++i)
    {
        double targetCents = double(i) * step;

        // Binary search for surrounding points in hullResult
        auto it = std::lower_bound(
            hullResult_.cents.begin(), hullResult_.cents.end(), targetCents);

        if (it == hullResult_.cents.begin())
        {
            size_t idx = 0;
            data_.plCurve[i] = float(hullResult_.pl[idx]);
            data_.hullCurve[i] = float(hullResult_.hull[idx]);
            data_.spikyCurve[i] = float(hullResult_.spiky[idx]);
        }
        else if (it == hullResult_.cents.end())
        {
            size_t idx = hullResult_.cents.size() - 1;
            data_.plCurve[i] = float(hullResult_.pl[idx]);
            data_.hullCurve[i] = float(hullResult_.hull[idx]);
            data_.spikyCurve[i] = float(hullResult_.spiky[idx]);
        }
        else
        {
            size_t idx1 = size_t(it - hullResult_.cents.begin());
            size_t idx0 = idx1 - 1;
            double t = (targetCents - hullResult_.cents[idx0])
                       / (hullResult_.cents[idx1] - hullResult_.cents[idx0]);
            data_.plCurve[i] = float(hullResult_.pl[idx0]
                + t * (hullResult_.pl[idx1] - hullResult_.pl[idx0]));
            data_.hullCurve[i] = float(hullResult_.hull[idx0]
                + t * (hullResult_.hull[idx1] - hullResult_.hull[idx0]));
            data_.spikyCurve[i] = float(hullResult_.spiky[idx0]
                + t * (hullResult_.spiky[idx1] - hullResult_.spiky[idx0]));
        }

        // Compute consonance
        if (peakSpiky > 0.0)
        {
            double normalized = double(data_.spikyCurve[i]) / peakSpiky;
            data_.consonance[i] = float(scalatrix::consonanceValue(normalized));
        }
        else
        {
            data_.consonance[i] = 0.0f;
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
