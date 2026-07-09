#include "PseudoHarmonicEngine.h"
#include "SpectrumModel.h"
#include "libMTSClient.h"
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

            // Update sustain/drive level relative to harmonic gains.  In
            // continuous excitation mode the partials are driven to their full
            // amplitude (a bowed/blown sustain); in impact mode the target is
            // the sustain parameter (a struck sound that decays unless held).
            float sustainScale = effectiveMpeEnabled() ? 1.0f : v.velocity;
            float driveTarget  = (params_.excitationMode == 1) ? 1.0f : params_.sustain;
            v.sustainLevel[h] = driveTarget * sustainScale * harmonicGains_[h];
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

    // Global filter coefficients (shared by all voices; per-voice state).
    filterCoeffs_.set(params_.filterCutoff, params_.filterReso, float(sampleRate_));

    updateAllRotations();
}

void PseudoHarmonicEngine::recomputeFreqRatios()
{
    SpectrumModel::computeFreqRatios(params_, freqRatios_);
}

void PseudoHarmonicEngine::recomputeGains()
{
    // Magnitudes (pseudoharmonic amplitude shape) and per-partial phase offsets.
    SpectrumModel::computeGains(params_, harmonicGains_);

    std::array<float, kMaxHarmonics> phases{};
    SpectrumModel::computePhases(params_, phases);

    // Build the complex impact vector: strike * magnitude * e^{i 2*pi*phase}.
    // With phaseSpread == 0 the exponential is 1, so the vector is purely real.
    for (int h = 0; h < kMaxHarmonics; ++h)
    {
        float ang = 2.0f * float(M_PI) * phases[h];
        impactVec_[h] = params_.strike * harmonicGains_[h]
                        * std::complex<float>(std::cos(ang), std::sin(ang));
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

float PseudoHarmonicEngine::noteToFreq(int note, int midiChannel) const
{
    if (mtsClient_ && mtsMasterActive_.load())
    {
        // MTS spec: pass MIDI channel (0-15), or -1 if unknown/MPE.
        // When MPE is the user preference, we still suppress it while MTS is
        // driving tuning — so we pass the incoming channel as-is.
        signed char ch = (midiChannel >= 1 && midiChannel <= 16)
                             ? static_cast<signed char>(midiChannel - 1)
                             : static_cast<signed char>(-1);
        double freq = MTS_NoteToFrequency(mtsClient_,
                                          static_cast<char>(note),
                                          ch);
        return static_cast<float>(freq);
    }
    return 440.0f * std::pow(2.0f, (note - 69) / 12.0f);
}

void PseudoHarmonicEngine::noteOn(int note, float velocity, int mpeChannel)
{
    // MTS-ESP may request that a note be filtered (unmapped in the scale).
    if (mtsClient_ && mtsMasterActive_.load())
    {
        signed char ch = (mpeChannel >= 1 && mpeChannel <= 16)
                             ? static_cast<signed char>(mpeChannel - 1)
                             : static_cast<signed char>(-1);
        if (MTS_ShouldFilterNote(mtsClient_, static_cast<char>(note), ch))
            return;
    }

    float freq = noteToFreq(note, mpeChannel);

    // Find a free voice or steal lowest-energy voice
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
        // Steal the voice with the lowest energy
        float minEnergy = std::numeric_limits<float>::max();
        idx = 0;
        for (int i = 0; i < kMaxVoices; ++i)
        {
            float e = voices_[i].energy();
            if (e < minEnergy)
            {
                minEnergy = e;
                idx = i;
            }
        }
    }

    auto& v = voices_[idx];
    v.noteOn(note, velocity, freq, mpeChannel);
    v.detuneAdd = freq * (params_.detune - 1.0f);

    // Inherit current pitch bend state for this channel
    if (effectiveMpeEnabled())
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

    // Sustain/drive levels relative to harmonic gains (independent of strike).
    // In MPE mode, pressure controls sustain/noise instead of velocity.
    // Continuous excitation mode drives partials to full amplitude for a
    // sustained (bowed/blown) tone; impact mode uses the sustain parameter.
    float sustainScale = effectiveMpeEnabled() ? 1.0f : velocity;
    float driveTarget  = (params_.excitationMode == 1) ? 1.0f : params_.sustain;
    for (int h = 0; h < kMaxHarmonics; ++h)
        v.sustainLevel[h] = driveTarget * sustainScale * harmonicGains_[h];

    // Recompute rotation so sustainExcitation reflects the sustain levels
    v.updateRotation(freqRatios_, decayRates_, releaseRates_, float(sampleRate_));

}

void PseudoHarmonicEngine::noteOff(int note, int mpeChannel)
{
    for (auto& v : voices_)
    {
        if (v.active && v.midiNote == note && v.mpeChannel == mpeChannel && !v.releasing)
        {
            // Sustain may arrive on a different channel than notes (e.g. MPE master ch1,
            // or controller always sends CC on ch1 while DAW routes notes elsewhere).
            // Check the note's own channel first, then fall back to channel 1.
            bool held = false;
            if (mpeChannel >= 1 && mpeChannel <= 16)
            {
                held = sustainOn_[mpeChannel - 1];
                if (!held && mpeChannel != 1)
                    held = sustainOn_[0];
            }
            if (held)
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

    if (effectiveMpeEnabled())
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
        // Release all sustained voices on this channel.
        // Channel 1 sustain release affects voices on all channels (MPE master or
        // controller that always sends CC on ch1 while notes are on other channels).
        for (auto& v : voices_)
        {
            if (!v.active || !v.sustained) continue;
            bool match = (v.mpeChannel == channel);
            if (!match && channel == 1)
                match = true;
            if (match)
            {
                v.sustained = false;
                v.noteOff();
                v.updateRotation(freqRatios_, decayRates_, releaseRates_, float(sampleRate_));
            }
        }
    }
}

void PseudoHarmonicEngine::channelPressure(float pressure, int channel)
{
    if (!effectiveMpeEnabled()) return;

    for (auto& v : voices_)
    {
        if (v.active && !v.releasing && v.mpeChannel == channel)
            v.pressure = pressure;
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

    // Precompute per-sample ramp factor for noise envelope (same rate as fundamental decay)
    float noiseRampStep = 1.0f - std::exp(-decayRates_[0] / float(sampleRate_));

    for (int i = 0; i < numSamples; ++i)
    {
        float sample = 0.0f;
        for (auto& v : voices_)
        {
            if (!v.active) continue;

            // Add noise excitation for active (non-releasing) voices
            if (!v.releasing && params_.noiseMix > 0.0f)
            {
                v.noiseEnvelope += (1.0f - v.noiseEnvelope) * noiseRampStep;
                float noiseScale = effectiveMpeEnabled() ? v.pressure : v.velocity;
                float noiseMix = params_.noiseMix * noiseScale * v.noiseEnvelope;
                for (int h = 0; h < kMaxHarmonics; ++h)
                    v.x[h] += 0.002f * noiseMix * noiseDist(rng) * harmonicGains_[h];
            }

            float vs = v.processSample(v.pressure);
            vs = v.applyFilter(vs, filterCoeffs_, params_.filterType);
            sample += vs;

            // Check if voice has decayed enough to deactivate
            if (v.energy() < 1e-10f)
            {
                // Releasing voices with no energy are done
                // Non-releasing voices with no sustain excitation are also done
                bool hasSustain = false;
                if (!v.releasing)
                    for (int h = 0; h < kMaxHarmonics; ++h)
                        if (v.sustainExcitation[h] > 0.0f) { hasSustain = true; break; }

                if (v.releasing || !hasSustain)
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
