#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "DSP/PseudoHarmonicEngine.h"
#include "DSP/Effects/MasterEffects.h"
#include "Mod/ModEngine.h"
#include "Preset/PresetManager.h"
#include "Network/WSBridge.h"
#include "Network/OSCReceiver.h"
#include "Visualization/ConsonanceCurve.h"
#include <mutex>
#include <string>

struct MTSClient;

class PseudoHarmonicProcessor : public juce::AudioProcessor,
                                 private juce::Timer,
                                 private juce::AudioProcessorValueTreeState::Listener
{
public:
    PseudoHarmonicProcessor();
    ~PseudoHarmonicProcessor() override;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "PseudoHarmonic"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 2.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    PseudoHarmonicEngine& getEngine() { return engine_; }
    WSBridge& getWSBridge() { return wsBridge_; }
    ConsonanceCurveCalculator& getCurveCalc() { return curveCalc_; }

    uint16_t getWSPort() const { return wsBridge_.getPort(); }

    juce::AudioProcessorValueTreeState apvts_;

private:
    void timerCallback() override;
    void parameterChanged(const juce::String& parameterID, float newValue) override;
    void handleParamFromUI(const std::string& id, float value);
    void sendCurveToUI();
    void sendParamsToUI();
    void sendModStateToUI();
    void handleCommandFromUI(const nlohmann::json& j);
    void sendPresetListToUI();
    nlohmann::json buildPresetJson();
    void applyPresetJson(const nlohmann::json& j);
    void applyFollowTuning(const TuningParams& tuning);

    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    PseudoHarmonicEngine engine_;
    ModEngine modEngine_;
    MasterEffects masterFx_;
    PresetManager presetManager_;
    WSBridge wsBridge_;
    OSCReceiver oscReceiver_;
    ConsonanceCurveCalculator curveCalc_;

    // MTS-ESP client — registered in ctor, deregistered in dtor.  Lives for
    // the lifetime of the plugin instance.  Master presence is polled from
    // the timer and mirrored to the engine.
    MTSClient* mtsClient_{nullptr};
    // Raw master presence (is a master alive right now?) vs effective usage
    // (are we actually routing through MTS?).  They differ when the user
    // manually picks MIDI or MPE while a master is connected.
    bool wasRawMasterActive_{false};
    bool wasEffectiveMtsActive_{false};
    std::atomic<bool> mtsOverride_{false};
    std::string lastMtsScaleName_;

    uint64_t lastTuningVersion_{0};
    nlohmann::json cachedScaleDegrees_;
    bool wasOscConnected_{false};

    std::atomic<bool> curveNeedsUpdate_{true};
    std::atomic<bool> paramsNeedBroadcast_{true};
    std::atomic<bool> autoLogBaseline_{true};
    std::atomic<bool> updatingLogBaseline_{false};
    std::atomic<float> peakLevel_{0.0f};

    // OSC sending settings
    std::atomic<bool> oscSendConsonance_{false};  // default off
    std::atomic<bool> oscSendSpectrum_{true};      // default on
    std::atomic<bool> spectrumNeedsSend_{true};    // trigger initial send

    // UI settings
    std::atomic<bool> showRatioLabels_{true};
    std::atomic<bool> followTuning_{false};

    // Follow-tuning debug info (updated by applyFollowTuning, read by timerCallback)
    nlohmann::json followTuningInfo_;

    // Sample rate cached for control-rate modulation (block dt).
    double sampleRate_{44100.0};

    // Expression snapshot for the ModEngine (audio thread only): updated from
    // MIDI, read once per block to build ModInputs.
    float lastVelocity_{0.0f};
    float lastTimbreY_{0.0f};    // CC74 / MPE Y, 0..1
    float lastPressureZ_{0.0f};  // channel pressure, 0..1
    float lastPitchX_{0.0f};     // key-follow, bipolar (-1..1) around middle C

    // Double-buffered params: writers update pendingParams_ behind mutex,
    // audio thread swaps into engine at start of processBlock
    SynthParams pendingParams_;
    std::mutex pendingParamsMutex_;   // only held by non-audio threads
    std::atomic<bool> paramsDirty_{false};

    // Cached active notes snapshot (written by audio thread, read by timer)
    struct ActiveNoteSnapshot {
        int note; float freq; int channel;
    };
    std::array<ActiveNoteSnapshot, kMaxVoices> cachedActiveNotes_{};
    std::atomic<int> cachedActiveNoteCount_{0};

    static constexpr const char* paramIDs[] = {
        "stretch2", "stretch3", "stretch5", "stretch7", "stretch11", "stretch13",
        "decay", "release", "strikePos", "oddEven",
        "strike", "volume", "noiseMix", "sustain", "detune", "relaxTime",
        "curvePartials", "logBaseline", "warp",
        "centreFocus", "ampTilt", "phaseSpread",
        "filterType", "filterCutoff", "filterReso",
        "delayTime", "delayFeedback", "delayMix", "reverbAmount", "reverbSize",
        "macro1", "macro2", "macro3", "macro4", "macro5", "macro6", "macro7", "macro8"
    };

    // Effective macro values (0..1) fed to the ModEngine each block. Written by
    // host automation (APVTS), MIDI CC, and OSC; read on the audio thread.
    std::array<std::atomic<float>, 8> macroValues_{};

    // Default MIDI CC → macro map (CC 20..27 → macro 1..8). -1 disables.
    std::array<int, 8> macroCC_{ {20, 21, 22, 23, 24, 25, 26, 27} };

    // Last-seen OSC macro versions, so OSC drives macros only on fresh values
    // (without clobbering host/CC-driven ones every block).
    std::array<uint32_t, 8> lastOscMacroVer_{};

    // Modulation configuration (routes + LFO/envelope settings). Authoritative
    // copy owned by the message/UI thread; pushed into the ModEngine on the
    // audio thread when dirty (under modMutex_).
    ModConfig modConfig_;
    std::mutex modMutex_;
    std::atomic<bool> modConfigDirty_{false};
    std::atomic<bool> modStateNeedsBroadcast_{true};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PseudoHarmonicProcessor)
};
