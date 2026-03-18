#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "DSP/PseudoHarmonicEngine.h"
#include "Network/WSBridge.h"
#include "Visualization/ConsonanceCurve.h"

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

    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    PseudoHarmonicEngine engine_;
    WSBridge wsBridge_;
    ConsonanceCurveCalculator curveCalc_;

    std::atomic<bool> curveNeedsUpdate_{true};
    std::atomic<bool> paramsNeedBroadcast_{true};

    static constexpr const char* paramIDs[] = {
        "stretch2", "stretch3", "stretch5", "stretch7",
        "decay", "release", "strikePos", "oddEven",
        "volume", "noiseMix", "sustain", "detune", "relaxTime",
        "curvePartials", "logBaseline"
    };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PseudoHarmonicProcessor)
};
