#include "ConsonanceCurve.h"
#include <algorithm>
#include <numeric>

// Plomp-Levelt roughness model (Sethares parameterization)
float ConsonanceCurveCalculator::plompLeveltRoughness(float f1, float a1, float f2, float a2)
{
    if (f1 > f2) { std::swap(f1, f2); std::swap(a1, a2); }
    float s = 0.24f / (0.021f * f1 + 19.0f);
    float diff = f2 - f1;
    float minAmp = std::min(a1, a2);
    return minAmp * (std::exp(-3.5f * s * diff) - std::exp(-5.75f * s * diff));
}

float ConsonanceCurveCalculator::plDissonance(float cents, const float* freqRatios,
                                               const float* amplitudes, int n) const
{
    float ratio = std::pow(2.0f, cents / 1200.0f);
    float f0 = 261.63f; // Middle C

    float totalRoughness = 0.0f;
    for (int i = 0; i < n; ++i)
    {
        for (int j = 0; j < n; ++j)
        {
            float fi = f0 * freqRatios[i];
            float fj = f0 * ratio * freqRatios[j];
            totalRoughness += plompLeveltRoughness(fi, amplitudes[i], fj, amplitudes[j]);
        }
    }
    return totalRoughness;
}

void ConsonanceCurveCalculator::compute(const float* freqRatios, const float* amplitudes, int numPartials)
{
    const float step = kCurveMaxCents / float(kCurveResolution);

    // 1. Compute PL dissonance curve
    for (int i = 0; i < kCurveResolution; ++i)
    {
        float cents = float(i) * step;
        data_.plCurve[i] = plDissonance(cents, freqRatios, amplitudes, numPartials);
    }

    // 2. Find local maxima of second derivative (Hull₃ anchor points)
    // First compute second derivative
    std::array<float, kCurveResolution> d2{};
    for (int i = 1; i < kCurveResolution - 1; ++i)
        d2[i] = data_.plCurve[i + 1] - 2.0f * data_.plCurve[i] + data_.plCurve[i - 1];

    // Find maxima of d2 above threshold
    std::vector<int> anchorIdx;
    anchorIdx.push_back(0);
    float d2Threshold = 0.005f;

    for (int i = 2; i < kCurveResolution - 2; ++i)
    {
        // local max of d2 and above threshold
        if (d2[i] > d2[i - 1] && d2[i] > d2[i + 1] &&
            d2[i] > d2[i - 2] && d2[i] > d2[i + 2] &&
            std::abs(d2[i]) > d2Threshold)
        {
            anchorIdx.push_back(i);
        }
    }
    anchorIdx.push_back(kCurveResolution - 1);

    // 3. Interpolate hull through anchor points (linear for speed — good enough for viz)
    // Build hull via linear interpolation through anchors
    {
        int seg = 0;
        for (int i = 0; i < kCurveResolution; ++i)
        {
            while (seg + 1 < (int)anchorIdx.size() - 1 && i > anchorIdx[seg + 1])
                ++seg;

            int i0 = anchorIdx[seg];
            int i1 = anchorIdx[std::min(seg + 1, (int)anchorIdx.size() - 1)];

            if (i0 == i1)
            {
                data_.hullCurve[i] = data_.plCurve[i0];
            }
            else
            {
                float t = float(i - i0) / float(i1 - i0);
                data_.hullCurve[i] = data_.plCurve[i0] + t * (data_.plCurve[i1] - data_.plCurve[i0]);
            }
        }
    }

    // 4. Spiky = hull - PL (consonance residual)
    float peakSpiky = 0.0f;
    for (int i = 0; i < kCurveResolution; ++i)
    {
        data_.spikyCurve[i] = std::max(0.0f, data_.hullCurve[i] - data_.plCurve[i]);
        peakSpiky = std::max(peakSpiky, data_.spikyCurve[i]);
    }

    // 5. C(x) = max(0, 1 + 0.5 * log10(spiky / peak))
    for (int i = 0; i < kCurveResolution; ++i)
    {
        if (peakSpiky > 0.0f && data_.spikyCurve[i] > 0.0f)
        {
            float normalized = data_.spikyCurve[i] / peakSpiky;
            data_.consonance[i] = std::max(0.0f, 1.0f + 0.5f * std::log10(normalized));
        }
        else
        {
            data_.consonance[i] = 0.0f;
        }
    }
}

float ConsonanceCurveCalculator::consonanceAt(float cents) const
{
    float idx = cents / (kCurveMaxCents / float(kCurveResolution));
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
            // Reduce to within equave
            while (cents > kCurveMaxCents) cents -= kCurveMaxCents;
            data_.intervalCents.push_back(cents);
            data_.intervalConsonance.push_back(consonanceAt(cents));
        }
    }
}
