#pragma once

#include "PseudoHarmonicVoice.h"
#include <array>
#include <cmath>
#include <vector>
#include <mutex>
#include <functional>

static constexpr int kMaxVoices = 16;

struct SynthParams
{
    // Spectrum stretch per prime (ratio for that prime partial)
    float stretch2 = 2.0f;   // default: exact harmonic
    float stretch3 = 3.0f;
    float stretch5 = 5.0f;
    float stretch7 = 7.0f;

    // Envelope
    float decay = 2.0f;       // seconds (higher harmonics decay faster)
    float release = 1.0f;

    // Timbre
    float strikePos = 0.5f;   // 0..1 — simulates striking position
    float oddEven = 1.0f;     // 0..1 — 0 = no even, 1 = equal
    float volume = 0.02f;
    float noiseMix = 0.0f;    // 0..1

    // Detune
    float detune = 1.0f;      // ratio (1.0 = no detune)
    float relaxTime = 0.1f;   // seconds — how fast detune relaxes to 0

    int numHarmonics = 32;
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

    // Parameter access (thread-safe via atomic-like access — single writer)
    SynthParams& params() { return params_; }
    const SynthParams& params() const { return params_; }
    void paramsChanged(); // Recomputes derived arrays

    // Get currently active notes (for visualization)
    struct ActiveNote { int note; float freq; int channel; };
    std::vector<ActiveNote> getActiveNotes() const;

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
    int nextVoice_ = 0;

    // Derived arrays
    std::array<float, kMaxHarmonics> freqRatios_{};
    std::array<float, kMaxHarmonics> harmonicGains_{};
    std::array<float, kMaxHarmonics> impactVec_{};
    std::array<float, kMaxHarmonics> decayRates_{};
    std::array<float, kMaxHarmonics> releaseRates_{};
    float relaxFactor_ = 0.0f;

    // MTS-ESP
    void* mtsClient_ = nullptr;
};
