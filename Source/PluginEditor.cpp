#include "PluginEditor.h"

#ifndef NDEBUG
static const char* localDevServerAddress = "http://localhost:5174";
#endif

namespace
{
#ifdef NDEBUG
static auto streamToVector(juce::InputStream& stream)
{
    std::vector<std::byte> result((size_t)stream.getTotalLength());
    stream.setPosition(0);
    [[maybe_unused]] const auto bytesRead = stream.read(result.data(), result.size());
    jassert(bytesRead == (ssize_t)result.size());
    return result;
}

static juce::String getExtension(const juce::String& filename)
{
    return filename.fromLastOccurrenceOf(".", false, false);
}

static const char* getMimeForExtension(const juce::String& extension)
{
    static const std::unordered_map<juce::String, const char*> mimeMap = {
        {{"htm"}, "text/html"},
        {{"html"}, "text/html"},
        {{"txt"}, "text/plain"},
        {{"jpg"}, "image/jpeg"},
        {{"jpeg"}, "image/jpeg"},
        {{"svg"}, "image/svg+xml"},
        {{"ico"}, "image/vnd.microsoft.icon"},
        {{"json"}, "application/json"},
        {{"png"}, "image/png"},
        {{"css"}, "text/css"},
        {{"map"}, "application/json"},
        {{"js"}, "text/javascript"},
        {{"woff2"}, "font/woff2"}};

    const auto it = mimeMap.find(extension.toLowerCase());
    if (it != mimeMap.end())
        return it->second;

    return "application/octet-stream";
}
#endif
} // namespace

PseudoHarmonicEditor::PseudoHarmonicEditor(PseudoHarmonicProcessor& p)
    : AudioProcessorEditor(p),
      processor(p),
      webComponent{juce::WebBrowserComponent::Options{}
                       .withNativeIntegrationEnabled()
#ifdef NDEBUG
                       .withResourceProvider([this](const auto& url) { return getResource(url); })
#endif
                       .withNativeFunction(
                           juce::Identifier{"uiMounted"},
                           [this](const juce::Array<juce::var>& args,
                                  Completion completion) {
                               uiMounted(args, std::move(completion));
                           })}
{
    addAndMakeVisible(webComponent);
    setResizable(true, true);
    setSize(1300, 720);

#ifndef NDEBUG
    webComponent.goToURL(localDevServerAddress);
#else
    webComponent.goToURL(juce::WebBrowserComponent::getResourceProviderRoot());
#endif
}

void PseudoHarmonicEditor::resized()
{
    webComponent.setBounds(getLocalBounds());
}

void PseudoHarmonicEditor::uiMounted(const juce::Array<juce::var>&,
                                      Completion completion)
{
    completion(static_cast<int>(processor.getWSPort()));
}

#ifdef NDEBUG
std::optional<PseudoHarmonicEditor::Resource> PseudoHarmonicEditor::getResource(const juce::String& url)
{
    const auto urlToRetrieve = url == "/" ? juce::String{"index.html"}
                                         : url.fromFirstOccurrenceOf("/", false, false);

    if (auto* entry = guiZipFile_.getEntry(urlToRetrieve))
    {
        std::unique_ptr<juce::InputStream> stream(guiZipFile_.createStreamForEntry(*entry));
        auto v = streamToVector(*stream);
        auto mime = getMimeForExtension(getExtension(entry->filename).toLowerCase());
        return Resource{std::move(v), std::move(mime)};
    }
    return std::nullopt;
}
#endif
