#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <vector>
#include <algorithm>

// Master effects applied to the summed stereo output: a feedback delay followed
// by a Freeverb reverb (EaganMatrix "Recirculator" analog).  Parameters are
// passed per block from the (modulated) SynthParams, so they respond to
// modulation like everything else.
class MasterEffects
{
public:
    void prepare(double sampleRate, int /*maxBlock*/)
    {
        sampleRate_ = sampleRate;
        int maxDelay = static_cast<int>(sampleRate * 2.0) + 8;   // up to 2 s
        dl_.assign(maxDelay, 0.0f);
        dr_.assign(maxDelay, 0.0f);
        writePos_ = 0;
        reverb_.setSampleRate(sampleRate);
        reverb_.reset();
    }

    void reset()
    {
        std::fill(dl_.begin(), dl_.end(), 0.0f);
        std::fill(dr_.begin(), dr_.end(), 0.0f);
        reverb_.reset();
    }

    void process(float* L, float* R, int n,
                 float delayTimeS, float delayFb, float delayMix,
                 float reverbAmount, float reverbSize)
    {
        const int size = static_cast<int>(dl_.size());
        if (delayMix > 1.0e-4f && size > 2)
        {
            int   delaySamp = juce::jlimit(1, size - 1, static_cast<int>(delayTimeS * sampleRate_));
            float fb  = juce::jlimit(0.0f, 0.95f, delayFb);
            float mix = juce::jlimit(0.0f, 1.0f, delayMix);
            for (int i = 0; i < n; ++i)
            {
                int rp = writePos_ - delaySamp; if (rp < 0) rp += size;
                float dlv = dl_[rp], drv = dr_[rp];
                dl_[writePos_] = L[i] + dlv * fb;
                dr_[writePos_] = R[i] + drv * fb;
                L[i] = L[i] * (1.0f - mix) + dlv * mix;
                R[i] = R[i] * (1.0f - mix) + drv * mix;
                if (++writePos_ >= size) writePos_ = 0;
            }
        }

        if (reverbAmount > 1.0e-4f)
        {
            juce::Reverb::Parameters p;
            p.roomSize = juce::jlimit(0.0f, 1.0f, reverbSize);
            p.damping  = 0.5f;
            p.wetLevel = juce::jlimit(0.0f, 1.0f, reverbAmount);
            p.dryLevel = 1.0f - juce::jlimit(0.0f, 1.0f, reverbAmount) * 0.5f;
            p.width    = 1.0f;
            reverb_.setParameters(p);
            reverb_.processStereo(L, R, n);
        }
    }

private:
    double sampleRate_ = 44100.0;
    std::vector<float> dl_, dr_;
    int writePos_ = 0;
    juce::Reverb reverb_;
};
