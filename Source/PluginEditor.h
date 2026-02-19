#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "PluginProcessor.h"

class PseudoHarmonicEditor : public juce::AudioProcessorEditor
{
public:
    explicit PseudoHarmonicEditor(PseudoHarmonicProcessor&);
    ~PseudoHarmonicEditor() override = default;

    void paint(juce::Graphics& g) override { g.fillAll(juce::Colour(0xff0a0a0f)); }
    void resized() override;

private:
    PseudoHarmonicProcessor& processor;
    std::unique_ptr<juce::TextButton> openBrowserButton;
    std::unique_ptr<juce::Label> portLabel;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PseudoHarmonicEditor)
};
