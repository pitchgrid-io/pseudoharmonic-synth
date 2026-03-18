#include "PluginProcessor.h"
#include "PluginEditor.h"

static juce::NormalisableRange<float> makeLogRange(float min, float max, float centre)
{
    auto r = juce::NormalisableRange<float>(min, max, 0.001f);
    r.setSkewForCentre(centre);
    return r;
}

juce::AudioProcessorValueTreeState::ParameterLayout PseudoHarmonicProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    constexpr int v = 1; // parameter version for VST3 host compatibility

    layout.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{"stretch2",  v}, "Stretch 2nd",  juce::NormalisableRange<float>(1.9f, 2.1f, 0.001f), 2.0f));
    layout.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{"stretch3",  v}, "Stretch 3rd",  juce::NormalisableRange<float>(2.9f, 3.1f, 0.001f), 3.0f));
    layout.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{"stretch5",  v}, "Stretch 5th",  juce::NormalisableRange<float>(4.9f, 5.1f, 0.001f), 5.0f));
    layout.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{"stretch7",  v}, "Stretch 7th",  juce::NormalisableRange<float>(6.9f, 7.1f, 0.001f), 7.0f));
    layout.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{"decay",     v}, "Decay",        makeLogRange(0.01f, 20.0f, 1.0f), 2.0f));
    layout.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{"release",   v}, "Release",      makeLogRange(0.01f, 20.0f, 1.0f), 1.0f));
    layout.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{"strikePos", v}, "Strike Pos",   juce::NormalisableRange<float>(0.01f, 1.0f, 0.01f), 0.5f));
    layout.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{"oddEven",   v}, "Odd Even",     juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 1.0f));
    layout.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{"volume",    v}, "Volume",       makeLogRange(0.001f, 0.1f, 0.01f), 0.02f));
    layout.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{"noiseMix",  v}, "Noise Mix",    juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.0f));
    layout.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{"sustain",   v}, "Sustain",      juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.0f));
    layout.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{"detune",    v}, "Detune",       makeLogRange(0.5f, 2.0f, 1.0f), 1.0f));
    layout.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{"relaxTime", v}, "Relax Time",   makeLogRange(0.01f, 1.0f, 0.1f), 0.1f));

    layout.add(std::make_unique<juce::AudioParameterInt>(juce::ParameterID{"curvePartials", v}, "Curve Partials", 1, 32, 16));
    layout.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{"logBaseline", v}, "Log Baseline", juce::NormalisableRange<float>(0.1f, 2.0f, 0.01f), 0.5f));

    return layout;
}

PseudoHarmonicProcessor::PseudoHarmonicProcessor()
    : AudioProcessor(BusesProperties()
                         .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts_(*this, nullptr, "Parameters", createParameterLayout())
{
    // Register APVTS listeners for all 12 params
    for (auto* id : paramIDs)
        apvts_.addParameterListener(id, this);

    // Start WS bridge on auto-assigned port
    wsBridge_.start(0);

    // Handle param changes from UI
    wsBridge_.onParamChange([this](const std::string& id, float value) {
        handleParamFromUI(id, value);
    });

    // Re-send state when a new client connects
    wsBridge_.onClientConnect([this]() {
        curveNeedsUpdate_ = true;
        paramsNeedBroadcast_ = true;
    });

    // Start timer for UI updates (30 fps)
    startTimerHz(30);
}

PseudoHarmonicProcessor::~PseudoHarmonicProcessor()
{
    for (auto* id : paramIDs)
        apvts_.removeParameterListener(id, this);

    stopTimer();
    wsBridge_.stop();
}

void PseudoHarmonicProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    engine_.prepareToPlay(sampleRate, samplesPerBlock);
}

void PseudoHarmonicProcessor::processBlock(juce::AudioBuffer<float>& buffer,
                                            juce::MidiBuffer& midiMessages)
{
    buffer.clear();

    // Process MIDI
    for (const auto metadata : midiMessages)
    {
        auto msg = metadata.getMessage();

        // DIAG: log all MIDI to file
        {
            static juce::File logFile("/tmp/ph_midi_log.txt");
            static bool firstWrite = true;
            if (firstWrite) { logFile.replaceWithText("PseudoHarmonic MIDI Log\n"); firstWrite = false; }
            auto desc = msg.getDescription();
            logFile.appendText(desc + "\n");
        }

        if (msg.isNoteOn())
            engine_.noteOn(msg.getNoteNumber(), msg.getFloatVelocity(), msg.getChannel());
        else if (msg.isNoteOff())
            engine_.noteOff(msg.getNoteNumber(), msg.getChannel());
        else if (msg.isPitchWheel())
            engine_.pitchBend(msg.getPitchWheelValue(), msg.getChannel());
        else if (msg.isSustainPedalOn())
            engine_.sustainPedal(true, msg.getChannel());
        else if (msg.isSustainPedalOff())
            engine_.sustainPedal(false, msg.getChannel());
        else if (msg.isAllNotesOff() || msg.isAllSoundOff())
            engine_.allNotesOff();
    }

    // Process audio
    auto* left = buffer.getWritePointer(0);
    auto* right = buffer.getNumChannels() > 1 ? buffer.getWritePointer(1) : nullptr;
    engine_.processBlock(left, right ? right : left, buffer.getNumSamples());
    if (right == nullptr && buffer.getNumChannels() > 1)
        buffer.copyFrom(1, 0, buffer, 0, 0, buffer.getNumSamples());
}

void PseudoHarmonicProcessor::parameterChanged(const juce::String& parameterID, float newValue)
{
    auto& p = engine_.params();

    if      (parameterID == "stretch2")  p.stretch2 = newValue;
    else if (parameterID == "stretch3")  p.stretch3 = newValue;
    else if (parameterID == "stretch5")  p.stretch5 = newValue;
    else if (parameterID == "stretch7")  p.stretch7 = newValue;
    else if (parameterID == "decay")     p.decay = newValue;
    else if (parameterID == "release")   p.release = newValue;
    else if (parameterID == "strikePos") p.strikePos = newValue;
    else if (parameterID == "oddEven")   p.oddEven = newValue;
    else if (parameterID == "volume")    p.volume = newValue;
    else if (parameterID == "noiseMix")  p.noiseMix = newValue;
    else if (parameterID == "sustain")   p.sustain = newValue;
    else if (parameterID == "detune")    p.detune = newValue;
    else if (parameterID == "relaxTime") p.relaxTime = newValue;
    else if (parameterID == "curvePartials") p.curvePartials = static_cast<int>(newValue);
    else if (parameterID == "logBaseline") p.logBaseline = newValue;
    else return;

    engine_.paramsChanged();
    curveNeedsUpdate_ = true;
    paramsNeedBroadcast_ = true;
}

void PseudoHarmonicProcessor::timerCallback()
{
    // Send curve data when spectrum params change
    if (curveNeedsUpdate_.exchange(false))
        sendCurveToUI();

    // Send current params
    if (paramsNeedBroadcast_.exchange(false))
        sendParamsToUI();

    // Always send active notes (cheap)
    auto notes = engine_.getActiveNotes();
    nlohmann::json noteArr = nlohmann::json::array();
    std::vector<float> freqs;
    for (const auto& n : notes)
    {
        noteArr.push_back({{"note", n.note}, {"freq", n.freq}, {"channel", n.channel}});
        freqs.push_back(n.freq);
    }
    wsBridge_.sendActiveNotes(noteArr);

    // Compute interval lines
    if (!freqs.empty())
    {
        curveCalc_.computeIntervals(freqs);
        const auto& data = curveCalc_.getData();
        nlohmann::json intervals = nlohmann::json::array();
        for (size_t i = 0; i < data.intervalCents.size(); ++i)
        {
            intervals.push_back({
                {"cents", data.intervalCents[i]},
                {"consonance", data.intervalConsonance[i]}
            });
        }
        wsBridge_.sendIntervals(intervals);
    }
}

void PseudoHarmonicProcessor::handleParamFromUI(const std::string& id, float value)
{
    // Route APVTS params through setValueNotifyingHost so the host sees the change.
    // Must dispatch to message thread — VST3 wrapper only calls performEdit()
    // (which notifies the host) when on the message thread.
    auto* param = apvts_.getParameter(juce::String(id));
    if (param)
    {
        float normalized = param->getNormalisableRange().convertTo0to1(value);
        juce::MessageManager::callAsync([param, normalized]() {
            param->beginChangeGesture();
            param->setValueNotifyingHost(normalized);
            param->endChangeGesture();
        });
        return;
    }

    // Non-APVTS params (MPE settings) — handle directly
    auto& p = engine_.params();
    if (id == "pitchBendRange") p.pitchBendRange = value;
    else if (id == "mpeEnabled") p.mpeEnabled = (value > 0.5f);
    else if (id == "mpeMasterBendRange") p.mpeMasterBendRange = value;
    else if (id == "mpePerNoteBendRange") p.mpePerNoteBendRange = value;
    else return;

    engine_.paramsChanged();
    curveNeedsUpdate_ = true;
    paramsNeedBroadcast_ = true;
}

void PseudoHarmonicProcessor::sendCurveToUI()
{
    const auto& ratios = engine_.getFreqRatios();
    auto& p = engine_.params();

    // Build amplitude array (same as gains normalization)
    std::array<float, kMaxHarmonics> amps{};
    for (int h = 0; h < kMaxHarmonics; ++h)
    {
        float n = float(h + 1);
        amps[h] = (1.0f / n) * std::sin(float(M_PI) * n * p.strikePos / 2.0f)
                  * ((h + 1) % 2 == 0 ? p.oddEven : 1.0f);
    }
    // Normalize
    float sum = 0;
    for (int h = 0; h < p.numHarmonics; ++h) sum += std::abs(amps[h]);
    if (sum > 0) for (int h = 0; h < p.numHarmonics; ++h) amps[h] /= sum;

    // Build scalatrix::Spectrum from ratios + amplitudes
    // Use abs() since PL model needs magnitudes, and skip near-zero partials
    int numPartials = std::min(p.numHarmonics, p.curvePartials);
    std::vector<scalatrix::Partial> partials;
    partials.reserve(numPartials);
    for (int h = 0; h < numPartials; ++h)
    {
        double a = std::abs(double(amps[h]));
        if (a > 1e-8)
            partials.push_back({double(ratios[h]), a});
    }
    scalatrix::Spectrum spectrum(std::move(partials));

    curveCalc_.compute(spectrum, p.logBaseline);

    const auto& data = curveCalc_.getData();
    nlohmann::json curveJson;

    // Downsample: every 4th point → 1000 points for UI
    constexpr int kDownsampleStep = 4;
    constexpr int kUIResolution = kCurveResolution / kDownsampleStep;

    nlohmann::json plArr = nlohmann::json::array();
    nlohmann::json pyrArr = nlohmann::json::array();
    nlohmann::json consArr = nlohmann::json::array();
    for (int i = 0; i < kCurveResolution; i += kDownsampleStep)
    {
        plArr.push_back(data.plCurve[i]);
        pyrArr.push_back(data.pyramidCurve[i]);
        consArr.push_back(data.consonance[i]);
    }
    curveJson["pl"] = plArr;
    curveJson["pyramid"] = pyrArr;
    curveJson["consonance"] = consArr;
    curveJson["maxCents"] = kCurveMaxCents;
    curveJson["resolution"] = kUIResolution;

    wsBridge_.sendCurveData(curveJson);
}

void PseudoHarmonicProcessor::sendParamsToUI()
{
    const auto& p = engine_.params();
    nlohmann::json j = {
        {"stretch2", p.stretch2},
        {"stretch3", p.stretch3},
        {"stretch5", p.stretch5},
        {"stretch7", p.stretch7},
        {"decay", p.decay},
        {"release", p.release},
        {"strikePos", p.strikePos},
        {"oddEven", p.oddEven},
        {"volume", p.volume},
        {"noiseMix", p.noiseMix},
        {"sustain", p.sustain},
        {"detune", p.detune},
        {"relaxTime", p.relaxTime},
        {"pitchBendRange", p.pitchBendRange},
        {"mpeEnabled", p.mpeEnabled},
        {"mpeMasterBendRange", p.mpeMasterBendRange},
        {"mpePerNoteBendRange", p.mpePerNoteBendRange},
        {"curvePartials", p.curvePartials},
        {"logBaseline", p.logBaseline}
    };
    wsBridge_.sendParams(j);
}

void PseudoHarmonicProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    auto state = apvts_.copyState();
    std::unique_ptr<juce::XmlElement> xml(state.createXml());

    // Save non-APVTS MIDI/MPE settings
    const auto& p = engine_.params();
    auto* midi = xml->createNewChildElement("MidiSettings");
    midi->setAttribute("mpeEnabled", p.mpeEnabled ? 1 : 0);
    midi->setAttribute("pitchBendRange", (double)p.pitchBendRange);
    midi->setAttribute("mpeMasterBendRange", (double)p.mpeMasterBendRange);
    midi->setAttribute("mpePerNoteBendRange", (double)p.mpePerNoteBendRange);

    copyXmlToBinary(*xml, destData);
}

void PseudoHarmonicProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xml(getXmlFromBinary(data, sizeInBytes));
    if (xml && xml->hasTagName(apvts_.state.getType()))
    {
        // Restore non-APVTS MIDI/MPE settings
        if (auto* midi = xml->getChildByName("MidiSettings"))
        {
            auto& p = engine_.params();
            p.mpeEnabled = midi->getIntAttribute("mpeEnabled", 0) != 0;
            p.pitchBendRange = (float)midi->getDoubleAttribute("pitchBendRange", 2.0);
            p.mpeMasterBendRange = (float)midi->getDoubleAttribute("mpeMasterBendRange", 2.0);
            p.mpePerNoteBendRange = (float)midi->getDoubleAttribute("mpePerNoteBendRange", 48.0);
            engine_.paramsChanged();

            // Remove before passing to APVTS so it doesn't see unknown elements
            xml->removeChildElement(midi, true);
        }

        apvts_.replaceState(juce::ValueTree::fromXml(*xml));
    }
}

juce::AudioProcessorEditor* PseudoHarmonicProcessor::createEditor()
{
    return new PseudoHarmonicEditor(*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new PseudoHarmonicProcessor();
}
