#include "PseudoHarmonicEngine.h"
#include <algorithm>
#include <random>

PseudoHarmonicEngine::PseudoHarmonicEngine()
{
    initChannelBend();
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
    // Save old gains to rescale active voices
    auto oldGains = harmonicGains_;

    recomputeFreqRatios();
    recomputeGains();

    // Rescale active voices' oscillator amplitudes to match new gain profile
    for (auto& v : voices_)
    {
        if (!v.active) continue;
        for (int h = 0; h < kMaxHarmonics; ++h)
        {
            float oldG = std::abs(oldGains[h]);
            float newG = std::abs(harmonicGains_[h]);
            if (oldG > 1e-10f)
                v.x[h] *= newG / oldG;
            else if (newG > 1e-10f)
                v.x[h] = std::complex<float>(0.0f, 0.0f); // was silent, stays silent

            // Update sustain level from current amplitude
            v.sustainLevel[h] = params_.sustain * std::abs(v.x[h]);
        }
    }

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
    int fullStretch = static_cast<int>(params_.warp);
    float fracWeight = params_.warp - float(fullStretch);

    for (int h = 0; h < kMaxHarmonics; ++h)
    {
        float harmonic = float(h + 1);
        float pseudo = harmonic;
        auto factors = primeFactors(h + 1);
        for (int p : factors)
        {
            switch (p)
            {
                case 2: pseudo *= params_.stretch2 / 2.0f; break;
                case 3: pseudo *= params_.stretch3 / 3.0f; break;
                case 5: pseudo *= params_.stretch5 / 5.0f; break;
                case 7: pseudo *= params_.stretch7 / 7.0f; break;
                default: break;
            }
        }

        // Blend in log-freq space: below fullStretch = full pseudo,
        // at boundary = fractional blend, above = pure harmonic
        float weight; // 1 = full pseudo, 0 = pure harmonic
        if (h < fullStretch)
            weight = 1.0f;
        else if (h == fullStretch)
            weight = fracWeight;
        else
            weight = 0.0f;

        if (weight >= 1.0f)
            freqRatios_[h] = pseudo;
        else if (weight <= 0.0f)
            freqRatios_[h] = harmonic;
        else
            freqRatios_[h] = std::exp2(weight * std::log2(pseudo) + (1.0f - weight) * std::log2(harmonic));
    }
}

void PseudoHarmonicEngine::recomputeGains()
{
    float sumGains = 0.0f;
    int fullPartials = static_cast<int>(params_.curvePartials);
    float fracWeight = params_.curvePartials - float(fullPartials);

    for (int h = 0; h < kMaxHarmonics; ++h)
    {
        float n = float(h + 1);
        float gain = (1.0f / n)
                     * std::sin(float(M_PI) * n * params_.strikePos / 2.0f)
                     * ((h + 1) % 2 == 0 ? params_.oddEven : 1.0f);

        // Apply partials windowing: full weight up to fullPartials,
        // fractional weight on the next, zero beyond
        if (h < fullPartials)
            ; // full weight
        else if (h == fullPartials)
            gain *= fracWeight;
        else
            gain = 0.0f;

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
            impactVec_[h] = params_.strike * harmonicGains_[h];
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

    // Inherit current pitch bend state for this channel
    if (params_.mpeEnabled)
    {
        // MPE: master bend from ch 1, per-note bend from member channel
        float masterRaw = float(channelBendRaw_[0] - 8192) / 8192.0f;
        v.masterBendSemitones = masterRaw * params_.mpeMasterBendRange;
        if (mpeChannel > 1)
        {
            float noteRaw = float(channelBendRaw_[mpeChannel - 1] - 8192) / 8192.0f;
            v.noteBendSemitones = noteRaw * params_.mpePerNoteBendRange;
        }
        else
        {
            v.noteBendSemitones = 0.0f;
        }
    }
    else
    {
        // Standard MIDI: single bend range, applied as master bend
        float raw = float(channelBendRaw_[mpeChannel - 1] - 8192) / 8192.0f;
        v.masterBendSemitones = raw * params_.pitchBendRange;
        v.noteBendSemitones = 0.0f;
    }

    v.updateRotation(freqRatios_, decayRates_, releaseRates_, float(sampleRate_));
    v.impact(impactVec_, velocity);

    // Compute per-harmonic sustain levels from post-impact amplitude
    for (int h = 0; h < kMaxHarmonics; ++h)
        v.sustainLevel[h] = params_.sustain * std::abs(v.x[h]);

    // Recompute rotation so sustainExcitation reflects the sustain levels
    v.updateRotation(freqRatios_, decayRates_, releaseRates_, float(sampleRate_));

    nextVoice_ = (idx + 1) % kMaxVoices;
}

void PseudoHarmonicEngine::noteOff(int note, int mpeChannel)
{
    for (auto& v : voices_)
    {
        if (v.active && v.midiNote == note && v.mpeChannel == mpeChannel && !v.releasing)
        {
            if (mpeChannel >= 1 && mpeChannel <= 16 && sustainOn_[mpeChannel - 1])
            {
                v.sustained = true;
            }
            else
            {
                v.noteOff();
                v.updateRotation(freqRatios_, decayRates_, releaseRates_, float(sampleRate_));
            }
            break;
        }
    }
}

void PseudoHarmonicEngine::pitchBend(int bendValue, int channel)
{
    // Store raw bend per channel
    if (channel >= 1 && channel <= 16)
        channelBendRaw_[channel - 1] = bendValue;

    float raw = float(bendValue - 8192) / 8192.0f;

    if (params_.mpeEnabled)
    {
        if (channel == 1)
        {
            // Manager channel: update master bend on ALL voices
            float semitones = raw * params_.mpeMasterBendRange;
            for (auto& v : voices_)
            {
                if (v.active)
                {
                    v.masterBendSemitones = semitones;
                    v.updateRotation(freqRatios_, decayRates_, releaseRates_, float(sampleRate_));
                }
            }
        }
        else
        {
            // Member channel: per-note bend on matching voices
            // Skip releasing voices — controller resets member channel after note-off
            float semitones = raw * params_.mpePerNoteBendRange;
            for (auto& v : voices_)
            {
                if (v.active && !v.releasing && v.mpeChannel == channel)
                {
                    v.noteBendSemitones = semitones;
                    v.updateRotation(freqRatios_, decayRates_, releaseRates_, float(sampleRate_));
                }
            }
        }
    }
    else
    {
        // Standard MIDI: bend all voices on this channel
        float semitones = raw * params_.pitchBendRange;
        for (auto& v : voices_)
        {
            if (v.active && v.mpeChannel == channel)
            {
                v.masterBendSemitones = semitones;
                v.updateRotation(freqRatios_, decayRates_, releaseRates_, float(sampleRate_));
            }
        }
    }
}

void PseudoHarmonicEngine::sustainPedal(bool on, int channel)
{
    if (channel >= 1 && channel <= 16)
        sustainOn_[channel - 1] = on;

    if (!on)
    {
        // Release all sustained voices on this channel
        for (auto& v : voices_)
        {
            if (v.active && v.sustained && v.mpeChannel == channel)
            {
                v.sustained = false;
                v.noteOff();
                v.updateRotation(freqRatios_, decayRates_, releaseRates_, float(sampleRate_));
            }
        }
    }
}

void PseudoHarmonicEngine::allNotesOff()
{
    for (auto& v : voices_)
    {
        if (v.active) { v.sustained = false; v.noteOff(); }
    }
}

void PseudoHarmonicEngine::processBlock(float* outputL, float* outputR, int numSamples)
{
    static thread_local std::mt19937 rng(42);
    static thread_local std::uniform_real_distribution<float> noiseDist(-1.0f, 1.0f);

    // Relax detune per block and update rotation to match
    for (auto& v : voices_)
    {
        if (v.active)
        {
            v.detuneAdd *= relaxFactor_;
            v.updateRotation(freqRatios_, decayRates_, releaseRates_, float(sampleRate_));
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
        outputL[i] = sample * params_.volume;
        outputR[i] = sample * params_.volume;
    }
}

std::vector<PseudoHarmonicEngine::ActiveNote> PseudoHarmonicEngine::getActiveNotes() const
{
    std::vector<ActiveNote> notes;
    for (const auto& v : voices_)
    {
        if (v.active && !v.releasing)
        {
            float bendTotal = v.masterBendSemitones + v.noteBendSemitones;
            float bentFreq = v.baseFreq * std::pow(2.0f, bendTotal / 12.0f);
            notes.push_back({v.midiNote, bentFreq, v.mpeChannel});
        }
    }
    return notes;
}
