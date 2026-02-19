#include "PluginProcessor.h"
#include "PluginEditor.h"

PseudoHarmonicProcessor::PseudoHarmonicProcessor()
    : AudioProcessor(BusesProperties()
                         .withOutput("Output", juce::AudioChannelSet::stereo(), true))
{
    // Start WS bridge on auto-assigned port
    wsBridge_.start(0);

    // Handle param changes from UI
    wsBridge_.onParamChange([this](const std::string& id, float value) {
        handleParamFromUI(id, value);
    });

    // Start timer for UI updates (30 fps)
    startTimerHz(30);
}

PseudoHarmonicProcessor::~PseudoHarmonicProcessor()
{
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
        if (msg.isNoteOn())
            engine_.noteOn(msg.getNoteNumber(), msg.getFloatVelocity(), msg.getChannel());
        else if (msg.isNoteOff())
            engine_.noteOff(msg.getNoteNumber(), msg.getChannel());
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
        nlohmann::json msg = {{"type", "intervals"}, {"data", intervals}};
        // Use broadcast directly since sendCurveData wraps in "curve" type
        // We need a dedicated method, but for now piggyback
        wsBridge_.sendActiveNotes(intervals); // FIXME: use proper method
    }
}

void PseudoHarmonicProcessor::handleParamFromUI(const std::string& id, float value)
{
    auto& p = engine_.params();

    if (id == "stretch2") p.stretch2 = value;
    else if (id == "stretch3") p.stretch3 = value;
    else if (id == "stretch5") p.stretch5 = value;
    else if (id == "stretch7") p.stretch7 = value;
    else if (id == "decay") p.decay = value;
    else if (id == "release") p.release = value;
    else if (id == "strikePos") p.strikePos = value;
    else if (id == "oddEven") p.oddEven = value;
    else if (id == "volume") p.volume = value;
    else if (id == "noiseMix") p.noiseMix = value;
    else if (id == "detune") p.detune = value;
    else if (id == "relaxTime") p.relaxTime = value;
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

    curveCalc_.compute(ratios.data(), amps.data(), std::min(p.numHarmonics, 16)); // limit for speed

    const auto& data = curveCalc_.getData();
    nlohmann::json curveJson;

    // Downsample for WS transfer: send every 2nd point (300 points)
    nlohmann::json plArr = nlohmann::json::array();
    nlohmann::json spikyArr = nlohmann::json::array();
    nlohmann::json consArr = nlohmann::json::array();
    nlohmann::json hullArr = nlohmann::json::array();
    for (int i = 0; i < kCurveResolution; i += 2)
    {
        plArr.push_back(data.plCurve[i]);
        spikyArr.push_back(data.spikyCurve[i]);
        consArr.push_back(data.consonance[i]);
        hullArr.push_back(data.hullCurve[i]);
    }
    curveJson["pl"] = plArr;
    curveJson["spiky"] = spikyArr;
    curveJson["consonance"] = consArr;
    curveJson["hull"] = hullArr;
    curveJson["maxCents"] = kCurveMaxCents;
    curveJson["resolution"] = kCurveResolution / 2;

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
        {"detune", p.detune},
        {"relaxTime", p.relaxTime}
    };
    wsBridge_.sendParams(j);
}

juce::AudioProcessorEditor* PseudoHarmonicProcessor::createEditor()
{
    return new PseudoHarmonicEditor(*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new PseudoHarmonicProcessor();
}
