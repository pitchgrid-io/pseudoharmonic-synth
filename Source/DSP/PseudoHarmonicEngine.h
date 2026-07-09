#pragma once

#include "PseudoHarmonicVoice.h"
#include <array>
#include <atomic>
#include <cmath>
#include <vector>
#include <mutex>
#include <functional>

struct MTSClient;

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
    // User-preferred fallback mode when no MTS-ESP master is connected:
    // false = standard MIDI, true = MPE.  Actual note routing uses MPE only when
    // this is true AND no MTS-ESP master is active.
    bool mpeEnabled = true;               // default: MPE
    float pitchBendRange = 2.0f;          // standard MIDI bend range (semitones)
    float mpeMasterBendRange = 2.0f;      // MPE manager channel range (semitones)
    float mpePerNoteBendRange = 48.0f;    // MPE member channel range (semitones)

    int numHarmonics = 32;

    // Partials control (affects both sound and consonance curve)
    float curvePartials = 16.0f;          // fractional: 9.4 = 9 full + 10th at 0.4 weight
    float warp = 32.0f;        // how many partials get pseudoharmonic pitch adjustment
    float logBaseline = 0.5f;             // log formula: C = max(0, 1 + logBaseline * log10(pyr/peak))

    // --- Spectral shaping (SineBank-style; all neutral by default so the
    //     default timbre is unchanged) ---
    float centreFocus = 0.0f;   // centre-of-focus partial index (0 = 1st partial)
    float ampTilt     = 0.0f;   // dB attenuation per partial away from centre (0 = flat)
    float phaseSpread = 0.0f;   // phase offset accumulated per partial, in turns (0..1)

    // Excitation model: 0 = impact (struck/decaying), 1 = continuous (driven/sustained)
    int excitationMode = 0;

    // --- Per-voice multimode filter (TPT state-variable) ---
    int   filterType   = 0;        // 0=Off, 1=LP, 2=HP, 3=BP, 4=Notch
    float filterCutoff = 12000.0f; // Hz
    float filterReso   = 0.1f;     // 0..1 (resonance)

    // --- Master effects (delay + reverb), applied to the summed output ---
    float delayTime     = 0.3f;    // seconds
    float delayFeedback = 0.3f;    // 0..1
    float delayMix      = 0.0f;    // 0..1 (0 = off)
    float reverbAmount  = 0.0f;    // 0..1 wet (0 = off)
    float reverbSize    = 0.5f;    // 0..1 room size
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

    // MTS-ESP support.  The client is owned by the processor; the engine only
    // reads from it.  setMTSMasterActive is called whenever the master
    // (dis)connects so note routing can adapt — MPE routing is suppressed
    // while an MTS master is driving tuning.
    void setMTSClient(MTSClient* client) { mtsClient_ = client; }
    void setMTSMasterActive(bool active) { mtsMasterActive_.store(active); }
    bool isMTSMasterActive() const { return mtsMasterActive_.load(); }
    bool effectiveMpeEnabled() const { return params_.mpeEnabled && !mtsMasterActive_.load(); }
    float noteToFreq(int note, int midiChannel = -1) const;

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
    // Complex so each partial can be struck at its own phase (phaseSpread).
    std::array<std::complex<float>, kMaxHarmonics> impactVec_{};
    std::array<float, kMaxHarmonics> decayRates_{};
    std::array<float, kMaxHarmonics> releaseRates_{};
    float relaxFactor_ = 0.0f;

    // Global filter coefficients (computed per block from params_).
    FilterCoeffs filterCoeffs_{};

    // Per-channel pitch bend state (raw 14-bit, center = 8192)
    std::array<int, 16> channelBendRaw_{};  // indexed 0-15 for channels 1-16
    void initChannelBend() { channelBendRaw_.fill(8192); }

    // Per-channel sustain pedal state
    std::array<bool, 16> sustainOn_{};  // indexed 0-15 for channels 1-16

    // MTS-ESP — client pointer is owned by the processor; engine only reads.
    MTSClient* mtsClient_ = nullptr;
    std::atomic<bool> mtsMasterActive_{false};
};
