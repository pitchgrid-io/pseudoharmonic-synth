#include "PluginEditor.h"

PseudoHarmonicEditor::PseudoHarmonicEditor(PseudoHarmonicProcessor& p)
    : AudioProcessorEditor(p), processor(p)
{
    setSize(400, 200);

    openBrowserButton = std::make_unique<juce::TextButton>("Open UI in Browser");
    openBrowserButton->onClick = [this]() {
        auto port = processor.getWSPort();
        juce::URL url("http://localhost:5174?wsPort=" + juce::String(port));
        url.launchInDefaultBrowser();
    };
    addAndMakeVisible(*openBrowserButton);

    portLabel = std::make_unique<juce::Label>("port", "WS Port: " + juce::String(processor.getWSPort()));
    portLabel->setColour(juce::Label::textColourId, juce::Colours::white);
    portLabel->setJustificationType(juce::Justification::centred);
    addAndMakeVisible(*portLabel);
}

void PseudoHarmonicEditor::resized()
{
    auto bounds = getLocalBounds().reduced(20);
    portLabel->setBounds(bounds.removeFromTop(40));
    bounds.removeFromTop(20);
    openBrowserButton->setBounds(bounds.removeFromTop(40).reduced(60, 0));
}
