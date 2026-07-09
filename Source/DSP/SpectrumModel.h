#pragma once

#include "PseudoHarmonicEngine.h"  // SynthParams, kMaxVoices
#include "PseudoHarmonicVoice.h"   // kMaxHarmonics, primeFactors
#include <array>
#include <cmath>

// =============================================================================
// SpectrumModel — turns the SynthParams into the per-partial spectrum:
// frequency ratios, amplitudes (magnitudes) and phase offsets.  This is the
// microtuned-SineBank core: the pseudoharmonic prime-stretch defines the
// partial frequencies, and SineBank-style controls (centre-of-focus, amplitude
// tilt, even/odd, strike comb, partial window, phase spread) shape amplitude
// and phase.
//
// Stateless; fills caller-provided arrays.  Replaces the engine's former
// recomputeFreqRatios()/recomputeGains().  At neutral spectral params
// (centreFocus=0, ampTilt=0, phaseSpread=0) it reproduces the previous timbre
// exactly.
// =============================================================================
struct SpectrumModel
{
    // Frequency ratio of each partial: pseudoharmonic prime-stretch blended
    // toward the pure harmonic series above the `warp` boundary.
    static void computeFreqRatios(const SynthParams& p,
                                  std::array<float, kMaxHarmonics>& ratios)
    {
        int   fullStretch = static_cast<int>(p.warp);
        float fracWeight  = p.warp - static_cast<float>(fullStretch);

        for (int h = 0; h < kMaxHarmonics; ++h)
        {
            float harmonic = static_cast<float>(h + 1);
            float pseudo   = harmonic;
            auto  factors  = primeFactors(h + 1);
            for (int f : factors)
            {
                switch (f)
                {
                    case 2:  pseudo *= p.stretch2  / 2.0f;  break;
                    case 3:  pseudo *= p.stretch3  / 3.0f;  break;
                    case 5:  pseudo *= p.stretch5  / 5.0f;  break;
                    case 7:  pseudo *= p.stretch7  / 7.0f;  break;
                    case 11: pseudo *= p.stretch11 / 11.0f; break;
                    case 13: pseudo *= p.stretch13 / 13.0f; break;
                    default: break;
                }
            }

            // Blend in log-freq space: below the warp boundary = full pseudo,
            // at the boundary = fractional blend, above = pure harmonic.
            float weight;
            if (h < fullStretch)       weight = 1.0f;
            else if (h == fullStretch) weight = fracWeight;
            else                       weight = 0.0f;

            if (weight >= 1.0f)
                ratios[h] = pseudo;
            else if (weight <= 0.0f)
                ratios[h] = harmonic;
            else
                ratios[h] = std::exp2(weight * std::log2(pseudo)
                                      + (1.0f - weight) * std::log2(harmonic));
        }
    }

    // Amplitude (magnitude) of each partial, normalised so Σ|gain| == kMaxVoices.
    //   base   = 1/n natural rolloff
    //   comb   = sin(pi*n*strikePos/2)          — strike position
    //   evenOdd= even partials scaled by oddEven
    //   focus  = centre-of-focus + amplitude tilt (dB/partial away from centre)
    //   window = fractional partial count (curvePartials)
    static void computeGains(const SynthParams& p,
                             std::array<float, kMaxHarmonics>& gains)
    {
        int   fullPartials = static_cast<int>(p.curvePartials);
        float fracWeight   = p.curvePartials - static_cast<float>(fullPartials);
        float sumGains     = 0.0f;

        for (int h = 0; h < kMaxHarmonics; ++h)
        {
            float n    = static_cast<float>(h + 1);
            float gain = (1.0f / n)
                         * std::sin(static_cast<float>(M_PI) * n * p.strikePos / 2.0f)
                         * ((h + 1) % 2 == 0 ? p.oddEven : 1.0f);

            // Centre-of-focus amplitude tilt: partials fall off with distance
            // (in partial index) from the centre by ampTilt dB each.  Neutral
            // when ampTilt == 0.
            if (p.ampTilt != 0.0f)
            {
                float dist = std::fabs(static_cast<float>(h) - p.centreFocus);
                gain *= std::pow(10.0f, -p.ampTilt * dist / 20.0f);
            }

            // Fractional partial-count window.
            if (h < fullPartials)      ; // full weight
            else if (h == fullPartials) gain *= fracWeight;
            else                        gain = 0.0f;

            gains[h] = gain;
            sumGains += std::fabs(gain);
        }

        if (sumGains > 0.0f)
        {
            float norm = static_cast<float>(kMaxVoices) / sumGains;
            for (int h = 0; h < kMaxHarmonics; ++h)
                gains[h] *= norm;
        }
    }

    // Phase offset per partial, in turns (0..1): partial h is offset by
    // h * phaseSpread.  Neutral (all zero) when phaseSpread == 0.
    static void computePhases(const SynthParams& p,
                              std::array<float, kMaxHarmonics>& phases)
    {
        for (int h = 0; h < kMaxHarmonics; ++h)
        {
            float ph = static_cast<float>(h) * p.phaseSpread;
            phases[h] = ph - std::floor(ph);
        }
    }
};
