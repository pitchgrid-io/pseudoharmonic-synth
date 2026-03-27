#include "ConsonanceCurve.h"
#include "../Network/OSCReceiver.h"
#include <scalatrix/mos.hpp>
#include <scalatrix/scale.hpp>
#include <scalatrix/label_calculator.hpp>
#include <algorithm>
#include <map>

void ConsonanceCurveCalculator::compute(const scalatrix::Spectrum& spectrum, float logBaseline)
{
    const double f0 = 261.63; // Middle C
    const double margin = 300.0;

    // Compute consonance curve (PL + exact pyramids + hull + consonance)
    curveResult_ = scalatrix::computeConsonanceCurve(
        spectrum, f0, -margin, kCurveMaxCents + margin, 0.5, double(logBaseline));

    effectiveLogBaseline_ = float(curveResult_.logBaseline);

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
            if (cents > kCurveMaxCents) continue;
            data_.intervalCents.push_back(cents);
            data_.intervalConsonance.push_back(consonanceAt(cents));
        }
    }
}

void ConsonanceCurveCalculator::computeIntervals(const std::vector<float>& noteFreqs,
                                                  const std::vector<int>& /* midiNotes */,
                                                  const TuningParams& tuning)
{
    data_.intervalCents.clear();
    data_.intervalConsonance.clear();

    float equaveCents = float(tuning.stretch * 1200.0);

    for (size_t i = 0; i < noteFreqs.size(); ++i)
    {
        for (size_t j = i + 1; j < noteFreqs.size(); ++j)
        {
            float higher = std::max(noteFreqs[i], noteFreqs[j]);
            float lower = std::min(noteFreqs[i], noteFreqs[j]);
            float cents = 1200.0f * std::log2(higher / lower);

            // Reduce by equave until within [0, equaveCents)
            if (equaveCents > 0.0f)
                while (cents > kCurveMaxCents) cents -= equaveCents;

            if (cents > kCurveMaxCents || cents < 0.0f) continue;
            data_.intervalCents.push_back(cents);
            data_.intervalConsonance.push_back(consonanceAt(cents));
        }
    }
}

std::vector<NodeConsonance> ConsonanceCurveCalculator::computeNodeConsonances(const TuningParams& tuning) const
{
    std::vector<NodeConsonance> result;

    try
    {
        // Build MOS with mode=0 and mode=n-1, same other params
        auto mos0 = scalatrix::MOS::fromParams(
            tuning.mosA, tuning.mosB, 0,
            tuning.stretch, tuning.skew, 1);

        int lastMode = mos0.n - 1;
        auto mosN = scalatrix::MOS::fromParams(
            tuning.mosA, tuning.mosB, lastMode,
            tuning.stretch, tuning.skew, 1);

        // Collect unique nodes by natural_coord from both base_scales
        // base_scale has n+1 nodes: index 0 = root (0,0), index n = equave (a,b)
        // Skip root (always consonance 1) and equave (same as root)
        std::map<std::pair<int,int>, float> nodeMap; // natural_coord -> cents

        auto addNodes = [&](const scalatrix::MOS& mos) {
            auto& nodes = mos.base_scale.getNodes();
            if (nodes.size() < 2) return;
            double rootPitch = nodes[0].pitch;
            if (rootPitch <= 0.0) return;

            // Skip first (root) and last (equave) nodes
            for (size_t i = 1; i < nodes.size() - 1; ++i)
            {
                if (nodes[i].pitch <= 0.0) continue;
                float cents = 1200.0f * std::log2(static_cast<float>(nodes[i].pitch / rootPitch));
                if (cents <= 0.0f || cents > kCurveMaxCents) continue;

                auto key = std::make_pair(nodes[i].natural_coord.x, nodes[i].natural_coord.y);
                nodeMap[key] = cents;
            }
        };

        addNodes(mos0);
        addNodes(mosN);

        // Compute consonance at each node position
        for (const auto& [coord, cents] : nodeMap)
        {
            float cons = consonanceAt(cents);
            result.push_back({coord.first, coord.second, cons});
        }
    }
    catch (...)
    {
    }

    return result;
}

std::vector<ScaleDegreeInfo> ConsonanceCurveCalculator::computeScaleDegrees(const TuningParams& tuning) const
{
    std::vector<ScaleDegreeInfo> result;

    try
    {
        auto mos = scalatrix::MOS::fromParams(
            tuning.mosA, tuning.mosB, tuning.mode,
            tuning.stretch, tuning.skew, 1);

        auto scale = mos.generateMappedScale(
            tuning.steps, tuning.modeOffset, tuning.rootFreq, 128, 60);

        auto& nodes = scale.getNodes();
        if (nodes.empty() || nodes.size() <= 60) return result;

        double rootPitch = nodes[60].pitch;
        if (rootPitch <= 0.0) return result;

        // Collect all nodes from root onwards that fall within the plot range
        for (size_t i = 60; i < nodes.size(); ++i)
        {
            if (nodes[i].pitch <= 0.0) continue;

            float cents = 1200.0f * std::log2(static_cast<float>(nodes[i].pitch / rootPitch));
            if (cents > kCurveMaxCents) break;
            if (cents < 0.0f) continue;

            std::string label = mos.nodeLabelDigit(nodes[i].natural_coord);
            float cons = consonanceAt(cents);
            float ty = static_cast<float>(nodes[i].tuning_coord.y);
            bool inScale = mos.nodeInScale(nodes[i].natural_coord);

            result.push_back({cents, cons, std::move(label), 0.0f, ty, inScale});
        }
    }
    catch (...)
    {
        // MOS construction can throw for invalid parameters
    }

    return result;
}
