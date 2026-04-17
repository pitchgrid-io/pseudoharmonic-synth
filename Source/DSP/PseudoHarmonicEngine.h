#pragma once

#include "PseudoHarmonicVoice.h"
#include <array>
#include <cmath>
#include <vector>
#include <mutex>
#include <functional>

static constexpr int kMaxVoices = 32;

struct SynthParams
{
    // Spectrum stretch per prime (ratio for that prime partial)
    float stretch2 = 2.0f;   // default: exact harmonic
    float stretch3 = 3.0f;
    float stretch5 = 5.0f;
    float stretch7 = 7.0f;
    float stretch11 = 11.0f;
    float stretch13 = 13.0f;

    // Envelope
    float decay = 2.0f;       // seconds (higher harmonics decay faster)
    float release = 1.0f;

    // Timbre
    float strikePos = 0.5f;   // 0..1 — simulates striking position
    float oddEven = 1.0f;     // 0..1 — 0 = no even, 1 = equal
    float strike = 0.02f;     // strike strength (scales impact)
    float volume = 1.0f;      // global output volume
    float noiseMix = 0.0f;    // 0..1
    float sustain = 0.0f;     // 0..1 — sustain level (fraction of strike amplitude)

    // Detune
    float detune = 1.0f;      // ratio (1.0 = no detune)
    float relaxTime = 0.1f;   // seconds — how fast detune relaxes to 0

    // Pitch bend
    bool mpeEnabled = false;              // false = standard MIDI, true = MPE
    float pitchBendRange = 2.0f;          // standard MIDI bend range (semitones)
    float mpeMasterBendRange = 2.0f;      // MPE manager channel range (semitones)
    float mpePerNoteBendRange = 48.0f;    // MPE member channel range (semitones)

    int numHarmonics = 32;

    // Partials control (affects both sound and consonance curve)
    float curvePartials = 16.0f;          // fractional: 9.4 = 9 full + 10th at 0.4 weight
    float warp = 32.0f;        // how many partials get pseudoharmonic pitch adjustment
    float logBaseline = 0.5f;             // log formula: C = max(0, 1 + logBaseline * log10(pyr/peak))
};

class PseudoHarmonicEngine
{
public:
    PseudoHarmonicEngine();

    void prepareToPlay(double sampleRate, int blockSize);
    void processBlock(float* outputL, float* outputR, int numSamples);

    void noteOn(int note, float velocity, int mpeChannel = 0);
    void noteOff(int note, int mpeChannel = 0);
    void allNotesOff();
    void pitchBend(int bendValue, int channel);
    void sustainPedal(bool on, int channel);
    void channelPressure(float pressure, int channel);

    // Parameter access (thread-safe via atomic-like access — single writer)
    SynthParams& params() { return params_; }
    const SynthParams& params() const { return params_; }
    void paramsChanged(); // Recomputes derived arrays

    // Get currently active notes (for visualization)
    struct ActiveNote { int note; float freq; int channel; };
    std::vector<ActiveNote> getActiveNotes() const;

    // Allocation-free iteration over active notes (audio-thread safe)
    template<typename Fn>
    void forEachActiveNote(Fn&& fn) const
    {
        for (const auto& v : voices_)
        {
            if (v.active && !v.releasing)
            {
                float bendTotal = v.masterBendSemitones + v.noteBendSemitones;
                float bentFreq = v.baseFreq * std::pow(2.0f, bendTotal / 12.0f);
                fn(v.midiNote, bentFreq, v.mpeChannel);
            }
        }
    }

    // Get frequency ratios for visualization
    const std::array<float, kMaxHarmonics>& getFreqRatios() const { return freqRatios_; }

    // MTS-ESP support
    void setMTSClient(void* client) { mtsClient_ = client; }
    float noteToFreq(int note) const;

private:
    void recomputeFreqRatios();
    void recomputeGains();
    void updateAllRotations();

    SynthParams params_;
    double sampleRate_ = 44100.0;
    int blockSize_ = 512;

    std::array<PseudoHarmonicVoice, kMaxVoices> voices_;

    // Derived arrays
    std::array<float, kMaxHarmonics> freqRatios_{};
    std::array<float, kMaxHarmonics> harmonicGains_{};
    std::array<float, kMaxHarmonics> impactVec_{};
    std::array<float, kMaxHarmonics> decayRates_{};
    std::array<float, kMaxHarmonics> releaseRates_{};
    float relaxFactor_ = 0.0f;

    // Per-channel pitch bend state (raw 14-bit, center = 8192)
    std::array<int, 16> channelBendRaw_{};  // indexed 0-15 for channels 1-16
    void initChannelBend() { channelBendRaw_.fill(8192); }

    // Per-channel sustain pedal state
    std::array<bool, 16> sustainOn_{};  // indexed 0-15 for channels 1-16

    // MTS-ESP
    void* mtsClient_ = nullptr;
};
