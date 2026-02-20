#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_gui_extra/juce_gui_extra.h>
#include "PluginProcessor.h"
#ifdef NDEBUG
#include <BinaryData.h>
#endif

class PseudoHarmonicEditor : public juce::AudioProcessorEditor
{
public:
    explicit PseudoHarmonicEditor(PseudoHarmonicProcessor&);
    ~PseudoHarmonicEditor() override = default;

    void paint(juce::Graphics& g) override { g.fillAll(juce::Colour(0xff0a0a0f)); }
    void resized() override;

private:
    using Completion = juce::WebBrowserComponent::NativeFunctionCompletion;

    void uiMounted(const juce::Array<juce::var>& args, Completion completion);

#ifdef NDEBUG
    using Resource = juce::WebBrowserComponent::Resource;
    std::optional<Resource> getResource(const juce::String& url);

    juce::MemoryInputStream guiStream_{
        BinaryData::webviewgui_zip,
        BinaryData::webviewgui_zipSize,
        false
    };
    juce::ZipFile guiZipFile_{guiStream_};
#endif

    PseudoHarmonicProcessor& processor;
    juce::WebBrowserComponent webComponent;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PseudoHarmonicEditor)
};
