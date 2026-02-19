#include "PseudoHarmonicEngine.h"
#include <algorithm>
#include <random>

PseudoHarmonicEngine::PseudoHarmonicEngine()
{
    paramsChanged();
}

void PseudoHarmonicEngine::prepareToPlay(double sampleRate, int blockSize)
{
    sampleRate_ = sampleRate;
    blockSize_ = blockSize;
    paramsChanged();
}

void PseudoHarmonicEngine::paramsChanged()
{
    recomputeFreqRatios();
    recomputeGains();

    // Decay/release rates: higher harmonics decay faster (rate proportional to harmonic number)
    for (int h = 0; h < kMaxHarmonics; ++h)
    {
        decayRates_[h] = float(h + 1) / params_.decay;
        releaseRates_[h] = float(h + 1) / params_.release;
    }

    // Relax factor per block
    relaxFactor_ = std::exp(-float(blockSize_) / float(sampleRate_ * params_.relaxTime));

    updateAllRotations();
}

void PseudoHarmonicEngine::recomputeFreqRatios()
{
    for (int h = 0; h < kMaxHarmonics; ++h)
    {
        float ratio = float(h + 1);
        auto factors = primeFactors(h + 1);
        for (int p : factors)
        {
            switch (p)
            {
                case 2: ratio *= params_.stretch2 / 2.0f; break;
                case 3: ratio *= params_.stretch3 / 3.0f; break;
                case 5: ratio *= params_.stretch5 / 5.0f; break;
                case 7: ratio *= params_.stretch7 / 7.0f; break;
                default: break; // primes > 7 stay exact
            }
        }
        freqRatios_[h] = ratio;
    }
}

void PseudoHarmonicEngine::recomputeGains()
{
    float sumGains = 0.0f;
    for (int h = 0; h < kMaxHarmonics; ++h)
    {
        float n = float(h + 1);
        float gain = (1.0f / n)
                     * std::sin(float(M_PI) * n * params_.strikePos / 2.0f)
                     * ((h + 1) % 2 == 0 ? params_.oddEven : 1.0f);
        harmonicGains_[h] = gain;
        sumGains += std::abs(gain);
    }

    // Normalize
    if (sumGains > 0.0f)
    {
        float norm = float(kMaxVoices) / sumGains;
        for (int h = 0; h < kMaxHarmonics; ++h)
        {
            harmonicGains_[h] *= norm;
            impactVec_[h] = params_.volume * harmonicGains_[h];
        }
    }
}

void PseudoHarmonicEngine::updateAllRotations()
{
    for (auto& v : voices_)
    {
        if (v.active)
            v.updateRotation(freqRatios_, decayRates_, releaseRates_, float(sampleRate_));
    }
}

float PseudoHarmonicEngine::noteToFreq(int note) const
{
    // TODO: MTS-ESP query if client available
    // For now, standard 12-TET
    return 440.0f * std::pow(2.0f, (note - 69) / 12.0f);
}

void PseudoHarmonicEngine::noteOn(int note, float velocity, int mpeChannel)
{
    float freq = noteToFreq(note);

    // Find a free voice or steal oldest
    int idx = -1;
    for (int i = 0; i < kMaxVoices; ++i)
    {
        if (!voices_[i].active)
        {
            idx = i;
            break;
        }
    }
    if (idx < 0)
    {
        // Steal: round-robin
        idx = nextVoice_;
        nextVoice_ = (nextVoice_ + 1) % kMaxVoices;
    }

    auto& v = voices_[idx];
    v.noteOn(note, velocity, freq, mpeChannel);
    v.detuneAdd = freq * (params_.detune - 1.0f);
    v.updateRotation(freqRatios_, decayRates_, releaseRates_, float(sampleRate_));
    v.impact(impactVec_, velocity);
    nextVoice_ = (idx + 1) % kMaxVoices;
}

void PseudoHarmonicEngine::noteOff(int note, int mpeChannel)
{
    for (auto& v : voices_)
    {
        if (v.active && v.midiNote == note && !v.releasing)
        {
            v.noteOff();
            v.updateRotation(freqRatios_, decayRates_, releaseRates_, float(sampleRate_));
            break;
        }
    }
}

void PseudoHarmonicEngine::allNotesOff()
{
    for (auto& v : voices_)
    {
        if (v.active) v.noteOff();
    }
}

void PseudoHarmonicEngine::processBlock(float* outputL, float* outputR, int numSamples)
{
    static thread_local std::mt19937 rng(42);
    static thread_local std::uniform_real_distribution<float> noiseDist(-1.0f, 1.0f);

    // Relax detune per block
    for (auto& v : voices_)
    {
        if (v.active)
        {
            v.detuneAdd *= relaxFactor_;
            // Only update rotation if detune is significant
        }
    }

    for (int i = 0; i < numSamples; ++i)
    {
        float sample = 0.0f;
        for (auto& v : voices_)
        {
            if (!v.active) continue;

            // Add noise excitation for active (non-releasing) voices
            if (!v.releasing && params_.noiseMix > 0.0f)
            {
                for (int h = 0; h < kMaxHarmonics; ++h)
                    v.x[h] += 0.002f * params_.noiseMix * noiseDist(rng) * harmonicGains_[h] * v.velocity;
            }

            sample += v.processSample();

            // Check if voice has decayed enough to deactivate
            if (v.releasing && v.energy() < 1e-10f)
            {
                v.active = false;
            }
        }
        outputL[i] = sample;
        outputR[i] = sample;
    }
}

std::vector<PseudoHarmonicEngine::ActiveNote> PseudoHarmonicEngine::getActiveNotes() const
{
    std::vector<ActiveNote> notes;
    for (const auto& v : voices_)
    {
        if (v.active && !v.releasing)
            notes.push_back({v.midiNote, v.baseFreq, v.mpeChannel});
    }
    return notes;
}
