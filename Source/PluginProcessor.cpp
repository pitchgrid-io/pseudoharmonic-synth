#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "BuildTimestamp.h"
#include "DSP/SpectrumModel.h"
#include "Mod/ModConfigJson.h"
#include "libMTSClient.h"
#include <scalatrix/mos.hpp>
#include <numeric>
#include <set>
#include <algorithm>
#include <cmath>

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
    constexpr float stretchDeviation = 0.03f;  // ±3% relative range
    constexpr int   stretchSteps = 126;        // 127 distinct values

    auto stretchRange = [](float prime, float dev, int steps) {
        float lo = prime * (1.0f - dev);
        float hi = prime * (1.0f + dev);
        float step = (hi - lo) / float(steps);
        return juce::NormalisableRange<float>(lo, hi, step);
    };

    layout.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{"stretch2",  v}, "Stretch 2nd",  stretchRange(2.0f,  stretchDeviation, stretchSteps), 2.0f));
    layout.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{"stretch3",  v}, "Stretch 3rd",  stretchRange(3.0f,  stretchDeviation, stretchSteps), 3.0f));
    layout.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{"stretch5",  v}, "Stretch 5th",  stretchRange(5.0f,  stretchDeviation, stretchSteps), 5.0f));
    layout.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{"stretch7",  v}, "Stretch 7th",  stretchRange(7.0f,  stretchDeviation, stretchSteps), 7.0f));
    layout.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{"stretch11", v}, "Stretch 11th", stretchRange(11.0f, stretchDeviation, stretchSteps), 11.0f));
    layout.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{"stretch13", v}, "Stretch 13th", stretchRange(13.0f, stretchDeviation, stretchSteps), 13.0f));
    layout.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{"decay",     v}, "Decay",        makeLogRange(0.01f, 20.0f, 1.0f), 2.0f));
    layout.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{"release",   v}, "Release",      makeLogRange(0.01f, 20.0f, 1.0f), 1.0f));
    layout.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{"strikePos", v}, "Strike Pos",   juce::NormalisableRange<float>(0.01f, 1.0f, 0.01f), 0.5f));
    layout.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{"oddEven",   v}, "Odd Even",     juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 1.0f));
    layout.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{"strike",    v}, "Strike",       juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.2f));
    layout.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{"volume",    v}, "Volume",       makeLogRange(0.01f, 2.0f, 0.5f), 1.0f));
    layout.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{"noiseMix",  v}, "Noise Mix",    juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.0f));
    layout.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{"sustain",   v}, "Sustain",      juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.0f));
    layout.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{"detune",    v}, "Detune",       makeLogRange(0.5f, 2.0f, 1.0f), 1.0f));
    layout.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{"relaxTime", v}, "Relax Time",   makeLogRange(0.01f, 1.0f, 0.1f), 0.1f));

    layout.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{"curvePartials", v}, "Partials", juce::NormalisableRange<float>(1.0f, 32.0f, 0.1f), 16.0f));
    layout.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{"warp", v}, "Warp", juce::NormalisableRange<float>(0.0f, 32.0f, 0.1f), 32.0f));
    layout.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{"logBaseline", v}, "Log Baseline", juce::NormalisableRange<float>(0.1f, 2.0f, 0.01f), 0.5f));

    // Spectral shaping (SineBank-style). Neutral defaults preserve the base timbre.
    layout.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{"centreFocus", v}, "Centre Focus", juce::NormalisableRange<float>(0.0f, 31.0f, 0.1f), 0.0f));
    layout.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{"ampTilt",     v}, "Amp Tilt",     juce::NormalisableRange<float>(0.0f, 6.0f, 0.01f), 0.0f));
    layout.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{"phaseSpread", v}, "Phase Spread", juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f), 0.0f));

    // Per-voice multimode filter.
    layout.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{"filterType",   v}, "Filter Type",   juce::NormalisableRange<float>(0.0f, 4.0f, 1.0f), 0.0f));
    layout.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{"filterCutoff", v}, "Filter Cutoff", makeLogRange(20.0f, 20000.0f, 2000.0f), 12000.0f));
    layout.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{"filterReso",   v}, "Filter Reso",   juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f), 0.1f));

    // Master effects (delay + reverb).
    layout.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{"delayTime",     v}, "Delay Time",     makeLogRange(0.01f, 2.0f, 0.3f), 0.3f));
    layout.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{"delayFeedback", v}, "Delay Feedback", juce::NormalisableRange<float>(0.0f, 0.95f, 0.001f), 0.3f));
    layout.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{"delayMix",      v}, "Delay Mix",      juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f), 0.0f));
    layout.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{"reverbAmount",  v}, "Reverb Amount",  juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f), 0.0f));
    layout.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{"reverbSize",    v}, "Reverb Size",    juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f), 0.5f));

    // Macros: 8 automatable global control handles (0..1), assignable as mod
    // sources and drivable by host automation, MIDI CC, or OSC.
    for (int i = 1; i <= 8; ++i)
        layout.add(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID{"macro" + juce::String(i), v},
            "Macro " + juce::String(i),
            juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f), 0.0f));

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

    // Seed engine with default MPE-enabled state (matches SynthParams default).
    // pendingParams_ starts default-constructed, paramsDirty_ defaults to false,
    // so push it explicitly so the audio thread picks it up on the first block.
    paramsDirty_ = true;

    // Register as an MTS-ESP client.  Safe even if libMTS is not installed —
    // the client handles missing library gracefully (HasMaster will return
    // false and note frequencies fall back to 12-TET).
    mtsClient_ = MTS_RegisterClient();
    engine_.setMTSClient(mtsClient_);

    // Start WS bridge on auto-assigned port
    wsBridge_.start(0);

    // Handle param changes from UI
    wsBridge_.onParamChange([this](const std::string& id, float value) {
        handleParamFromUI(id, value);
    });

    // Handle structured commands from UI (mod-route edits, preset commands…).
    wsBridge_.onCommand([this](const nlohmann::json& j) {
        handleCommandFromUI(j);
    });

    // Re-send state when a new client connects
    wsBridge_.onClientConnect([this]() {
        curveNeedsUpdate_ = true;
        paramsNeedBroadcast_ = true;
        modStateNeedsBroadcast_ = true;
        sendPresetListToUI();
    });

    // Start OSC receiver (sends heartbeat to plugin on port 34562)
    oscReceiver_.start(34562);

    // Start timer for UI updates (30 fps)
    startTimerHz(30);
}

PseudoHarmonicProcessor::~PseudoHarmonicProcessor()
{
    for (auto* id : paramIDs)
        apvts_.removeParameterListener(id, this);

    stopTimer();
    oscReceiver_.stop();
    wsBridge_.stop();

    if (mtsClient_)
    {
        engine_.setMTSClient(nullptr);
        MTS_DeregisterClient(mtsClient_);
        mtsClient_ = nullptr;
    }
}

void PseudoHarmonicProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    sampleRate_ = sampleRate;
    engine_.prepareToPlay(sampleRate, samplesPerBlock);
    modEngine_.prepare(sampleRate, samplesPerBlock);
    masterFx_.prepare(sampleRate, samplesPerBlock);
}

void PseudoHarmonicProcessor::processBlock(juce::AudioBuffer<float>& buffer,
                                            juce::MidiBuffer& midiMessages)
{
    buffer.clear();

    // Swap in pending params (audio thread is sole writer to engine_.params()).
    // Base params flow through the ModEngine, which resolves them into the
    // effective params fed to the audio engine.  In Phase 1 there are no
    // sources or routes, so resolve() is an identity and audio is unchanged;
    // once time-varying sources exist (Phase 3) resolve() runs every block.
    bool baseChanged = paramsDirty_.exchange(false);
    if (baseChanged)
    {
        // Copy pending snapshot — mutex not needed here because the audio thread
        // is the only consumer and float field reads are atomic-width on all
        // supported platforms. The mutex protects writer-writer races only.
        modEngine_.setBaseParams(pendingParams_);
    }

    // Push an edited modulation config (routes + LFO/env settings) into the
    // engine.  Rare (user edits), so a brief lock on the audio thread is fine;
    // runtime generator state is preserved by applyConfig().
    if (modConfigDirty_.exchange(false))
    {
        std::lock_guard<std::mutex> lock(modMutex_);
        modEngine_.applyConfig(modConfig_);
    }

    // Advance modulation sources and evaluate the matrix (control rate, once per
    // block).  Uses the previous block's expression snapshot — a sub-block
    // latency that is inaudible.  Note gating from this block's MIDI (below)
    // applies from the next block.
    const bool modActive = modEngine_.hasActiveModulation();
    if (modActive)
    {
        // Apply fresh OSC-driven macro values (only on version change).
        for (int i = 0; i < 8; ++i)
        {
            uint32_t v = oscReceiver_.getMacroVersion(i);
            if (v != lastOscMacroVer_[i]) { lastOscMacroVer_[i] = v; macroValues_[i].store(oscReceiver_.getMacro(i)); }
        }
        // Push current macro values (host/CC/OSC-driven) into the ModEngine.
        for (int i = 0; i < 8; ++i)
            modEngine_.setMacro(i, macroValues_[i].load());

        ModInputs mi;
        mi.timbreY    = lastTimbreY_;
        mi.pressureZ  = lastPressureZ_;
        mi.velocity   = lastVelocity_;
        mi.pitchX     = lastPitchX_;
        mi.consonance = 0.0f;  // wired to the live consonance value in a later step
        double dt = double(buffer.getNumSamples()) / sampleRate_;
        modEngine_.process(mi, dt);
    }
    if (baseChanged || modActive)
    {
        modEngine_.resolve(engine_.params());
        engine_.paramsChanged();
    }

    // Process MIDI
    for (const auto metadata : midiMessages)
    {
        auto msg = metadata.getMessage();

        // DIAG: log all MIDI to file
        // {
        //     static juce::File logFile("/tmp/ph_midi_log.txt");
        //     static bool firstWrite = true;
        //     if (firstWrite) { logFile.replaceWithText("PseudoHarmonic MIDI Log\n"); firstWrite = false; }
        //     auto desc = msg.getDescription();
        //     logFile.appendText(desc + "\n");
        // }

        if (msg.isNoteOn())
        {
            engine_.noteOn(msg.getNoteNumber(), msg.getFloatVelocity(), msg.getChannel());
            // Feed the modulation engine: gate/trigger + expression snapshot.
            lastVelocity_ = msg.getFloatVelocity();
            lastPitchX_   = juce::jlimit(-1.0f, 1.0f, (msg.getNoteNumber() - 60) / 36.0f);
            modEngine_.noteOn();
        }
        else if (msg.isNoteOff())
        {
            engine_.noteOff(msg.getNoteNumber(), msg.getChannel());
            modEngine_.noteOff();
        }
        else if (msg.isPitchWheel())
            engine_.pitchBend(msg.getPitchWheelValue(), msg.getChannel());
        else if (msg.isSustainPedalOn())
            engine_.sustainPedal(true, msg.getChannel());
        else if (msg.isSustainPedalOff())
            engine_.sustainPedal(false, msg.getChannel());
        else if (msg.isChannelPressure())
        {
            float z = msg.getChannelPressureValue() / 127.0f;
            engine_.channelPressure(z, msg.getChannel());
            lastPressureZ_ = z;
        }
        else if (msg.isController())
        {
            const int cc = msg.getControllerNumber();
            const float val = msg.getControllerValue() / 127.0f;
            if (cc == 74)
            {
                // MPE "Y" / timbre (CC74) — modulation source.
                lastTimbreY_ = val;
            }
            // MIDI CC → macro external drive (default map CC 20..27 → macro 1..8).
            for (int i = 0; i < 8; ++i)
                if (macroCC_[i] == cc)
                    macroValues_[i].store(val);
        }
        else if (msg.isAllNotesOff() || msg.isAllSoundOff())
        {
            engine_.allNotesOff();
            modEngine_.allNotesOff();
        }
    }

    // Process audio
    auto* left = buffer.getWritePointer(0);
    auto* right = buffer.getNumChannels() > 1 ? buffer.getWritePointer(1) : nullptr;
    engine_.processBlock(left, right ? right : left, buffer.getNumSamples());
    if (right == nullptr && buffer.getNumChannels() > 1)
        buffer.copyFrom(1, 0, buffer, 0, 0, buffer.getNumSamples());

    // Master effects (delay + reverb) on the summed stereo output, using the
    // effective (modulated) FX params.
    {
        const auto& fp = engine_.params();
        float* fl = buffer.getWritePointer(0);
        float* fr = buffer.getNumChannels() > 1 ? buffer.getWritePointer(1) : fl;
        masterFx_.process(fl, fr, buffer.getNumSamples(),
                          fp.delayTime, fp.delayFeedback, fp.delayMix,
                          fp.reverbAmount, fp.reverbSize);
    }

    // Measure peak level from output
    float peak = 0.0f;
    for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
    {
        auto* data = buffer.getReadPointer(ch);
        for (int i = 0; i < buffer.getNumSamples(); ++i)
            peak = std::max(peak, std::abs(data[i]));
    }
    peakLevel_.store(peak);

    // Cache active notes for UI (lock-free: write array first, then publish count)
    {
        int n = 0;
        engine_.forEachActiveNote([&](int note, float freq, int channel) {
            if (n < kMaxVoices)
                cachedActiveNotes_[n++] = {note, freq, channel};
        });
        cachedActiveNoteCount_.store(n, std::memory_order_release);
    }
}

void PseudoHarmonicProcessor::parameterChanged(const juce::String& parameterID, float newValue)
{
    // Macros: store into the effective macro array read by the audio thread.
    if (parameterID.startsWith("macro"))
    {
        int idx = parameterID.substring(5).getIntValue() - 1;  // "macro1" -> 0
        if (idx >= 0 && idx < 8)
        {
            macroValues_[idx].store(newValue);
            paramsNeedBroadcast_ = true;
        }
        return;
    }

    std::lock_guard<std::mutex> lock(pendingParamsMutex_);
    auto& p = pendingParams_;

    if      (parameterID == "stretch2")  p.stretch2 = newValue;
    else if (parameterID == "stretch3")  p.stretch3 = newValue;
    else if (parameterID == "stretch5")  p.stretch5 = newValue;
    else if (parameterID == "stretch7")  p.stretch7 = newValue;
    else if (parameterID == "stretch11") p.stretch11 = newValue;
    else if (parameterID == "stretch13") p.stretch13 = newValue;
    else if (parameterID == "decay")     p.decay = newValue;
    else if (parameterID == "release")   p.release = newValue;
    else if (parameterID == "strikePos") p.strikePos = newValue;
    else if (parameterID == "oddEven")   p.oddEven = newValue;
    else if (parameterID == "strike")    p.strike = newValue * 0.1f;
    else if (parameterID == "volume")    p.volume = newValue;
    else if (parameterID == "noiseMix")  p.noiseMix = newValue;
    else if (parameterID == "sustain")   p.sustain = newValue * 0.1f;
    else if (parameterID == "detune")    p.detune = newValue;
    else if (parameterID == "relaxTime") p.relaxTime = newValue;
    else if (parameterID == "curvePartials") { p.curvePartials = newValue; autoLogBaseline_ = true; }
    else if (parameterID == "warp") p.warp = newValue;
    else if (parameterID == "logBaseline") { p.logBaseline = newValue; if (!updatingLogBaseline_) autoLogBaseline_ = false; }
    else if (parameterID == "centreFocus") p.centreFocus = newValue;
    else if (parameterID == "ampTilt")     p.ampTilt = newValue;
    else if (parameterID == "phaseSpread") p.phaseSpread = newValue;
    else if (parameterID == "filterType")   p.filterType = int(newValue + 0.5f);
    else if (parameterID == "filterCutoff") p.filterCutoff = newValue;
    else if (parameterID == "filterReso")   p.filterReso = newValue;
    else if (parameterID == "delayTime")     p.delayTime = newValue;
    else if (parameterID == "delayFeedback") p.delayFeedback = newValue;
    else if (parameterID == "delayMix")      p.delayMix = newValue;
    else if (parameterID == "reverbAmount")  p.reverbAmount = newValue;
    else if (parameterID == "reverbSize")    p.reverbSize = newValue;
    else return;

    // Spectrum-affecting params trigger auto logBaseline recalculation
    if (parameterID == "stretch2" || parameterID == "stretch3" ||
        parameterID == "stretch5" || parameterID == "stretch7" ||
        parameterID == "strikePos" || parameterID == "oddEven" ||
        parameterID == "curvePartials" || parameterID == "warp" ||
        parameterID == "centreFocus" || parameterID == "ampTilt")
        autoLogBaseline_ = true;

    paramsDirty_ = true;
    curveNeedsUpdate_ = true;
    paramsNeedBroadcast_ = true;
}

void PseudoHarmonicProcessor::applyFollowTuning(const TuningParams& tuning)
{
    // Build MOS and get base_scale nodes
    scalatrix::MOS mos = scalatrix::MOS::fromParams(
        tuning.mosA, tuning.mosB, tuning.mode,
        tuning.stretch, tuning.skew, 1);

    const auto& nodes = mos.base_scale.getNodes();
    if (nodes.size() < 2) return;

    double equave = mos.equave; // log2 frequency ratio of the equave

    // Collect tuning_coord.x values and labels from all base_scale nodes
    struct ScaleNode { double x; std::string label; };
    std::vector<ScaleNode> scaleNodes;
    scaleNodes.reserve(nodes.size());
    for (const auto& node : nodes)
        scaleNodes.push_back({node.tuning_coord.x, mos.nodeLabelDigit(node.natural_coord)});

    // Helper: find nearest scale degree to a log2 value
    // No mod-equave reduction — base_scale nodes span [0, equave] and our
    // candidate ratios naturally fall in this range.  Direct comparison avoids
    // the fmod boundary bug where fmod(equave, equave)==0 maps the equave
    // ratio to the root node.
    struct NearestResult { double nodeX; double deviation; std::string label; };

    auto findNearest = [&](double log2val) -> NearestResult {
        double bestAbsDev = 1e9;
        double bestX = 0.0;
        double bestSignedDev = 0.0;
        std::string bestLabel;
        for (const auto& sn : scaleNodes) {
            double dev = log2val - sn.x;
            double absDev = std::abs(dev);
            if (absDev < bestAbsDev) {
                bestAbsDev = absDev;
                bestSignedDev = dev;
                bestX = sn.x;
                bestLabel = sn.label;
            }
        }
        return {bestX, bestSignedDev, bestLabel};
    };

    // Helper: find best adjusted prime value across a set of ratios,
    // filtering out ratios whose dependencies include invalid (clamped) primes.
    struct RatioCandidate {
        std::string name;                     // e.g. "5:4"
        double log2val;                       // log2 of the ratio interval
        std::function<double(double)> solve;  // given bestX, compute adjusted prime value
        std::set<int> deps;                   // primes this ratio depends on (besides the one being solved)
    };

    struct PrimeResult {
        double adjustedVal;
        std::string chosenRatio;
        std::string scaleDegreeLabel;
        double nodeX;
        double deviation;
        bool clamped;      // solved value was outside APVTS range
        bool noSolution;   // no valid candidates remained after filtering
    };

    std::set<int> invalidPrimes;  // primes that are clamped or have no solution

    auto findBestPrime = [&](const std::vector<RatioCandidate>& candidates, double prime) -> PrimeResult {
        // Filter out candidates whose deps overlap with invalidPrimes
        std::vector<const RatioCandidate*> valid;
        for (const auto& c : candidates) {
            bool ok = true;
            for (int dep : c.deps) {
                if (invalidPrimes.count(dep)) { ok = false; break; }
            }
            if (ok) valid.push_back(&c);
        }

        if (valid.empty())
            return {prime, "", "", 0.0, 0.0, false, true};

        double bestAbsDev = 1e9;
        double bestSignedDev = 0.0;
        double bestVal = prime;
        std::string bestRatio;
        std::string bestLabel;
        double bestNodeX = 0.0;

        for (const auto* c : valid) {
            auto nr = findNearest(c->log2val);
            if (std::abs(nr.deviation) < bestAbsDev) {
                bestAbsDev = std::abs(nr.deviation);
                bestSignedDev = nr.deviation;
                bestVal = c->solve(nr.nodeX);
                bestRatio = c->name;
                bestLabel = nr.label;
                bestNodeX = nr.nodeX;
            }
        }
        double lo = prime * (1.0 - 0.03);
        double hi = prime * (1.0 + 0.03);
        bool clamped = (bestVal < lo || bestVal > hi);
        return {std::clamp(bestVal, lo, hi), bestRatio, bestLabel, bestNodeX, bestSignedDev, clamped, false};
    };

    // --- Prime 2: {2:1} ---
    auto p2 = findBestPrime({
        {"2:1", std::log2(2.0), [](double bx) { return std::exp2(bx); }, {}}
    }, 2.0);
    double r2 = p2.adjustedVal;
    if (p2.clamped || p2.noSolution) invalidPrimes.insert(2);

    // --- Prime 3: {3:2, 3:1, 8:3} ---
    auto p3 = findBestPrime({
        {"3:2", std::log2(3.0 / r2),           [&](double bx) { return r2 * std::exp2(bx); },           {2}},
        {"3:1", std::log2(3.0),                 [](double bx) { return std::exp2(bx); },                 {}},
        {"8:3", 3.0 * std::log2(r2) - std::log2(3.0),
                                                [&](double bx) { return r2 * r2 * r2 / std::exp2(bx); },{2}}
    }, 3.0);
    double r3 = p3.adjustedVal;
    if (p3.clamped || p3.noSolution) invalidPrimes.insert(3);

    // --- Prime 5: {5:4, 6:5, 5:3, 8:5, 12:5} ---
    auto p5 = findBestPrime({
        {"5:4",  std::log2(5.0 / (r2 * r2)),        [&](double bx) { return r2 * r2 * std::exp2(bx); },        {2}},
        {"6:5",  std::log2(r2 * r3 / 5.0),          [&](double bx) { return r2 * r3 / std::exp2(bx); },        {2, 3}},
        {"5:3",  std::log2(5.0 / r3),               [&](double bx) { return r3 * std::exp2(bx); },             {3}},
        {"8:5",  std::log2(r2 * r2 * r2 / 5.0),     [&](double bx) { return r2 * r2 * r2 / std::exp2(bx); },  {2}},
        {"12:5", std::log2(r2 * r2 * r3 / 5.0),     [&](double bx) { return r2 * r2 * r3 / std::exp2(bx); },  {2, 3}}
    }, 5.0);
    double r5 = p5.adjustedVal;
    if (p5.clamped || p5.noSolution) invalidPrimes.insert(5);

    // --- Prime 7: {7:4, 7:5, 7:6, 8:7, 9:7, 7:3, 14:6} ---
    auto p7 = findBestPrime({
        {"7:4",  std::log2(7.0 / (r2 * r2)),        [&](double bx) { return r2 * r2 * std::exp2(bx); },        {2}},
        {"7:5",  std::log2(7.0 / r5),               [&](double bx) { return r5 * std::exp2(bx); },             {5}},
        {"7:6",  std::log2(7.0 / (r2 * r3)),        [&](double bx) { return r2 * r3 * std::exp2(bx); },        {2, 3}},
        {"8:7",  std::log2(r2 * r2 * r2 / 7.0),     [&](double bx) { return r2 * r2 * r2 / std::exp2(bx); },  {2}},
        {"9:7",  std::log2(r3 * r3 / 7.0),          [&](double bx) { return r3 * r3 / std::exp2(bx); },        {3}},
        {"7:3",  std::log2(7.0 / r3),               [&](double bx) { return r3 * std::exp2(bx); },             {3}},
        {"14:6", std::log2(7.0 / r3),               [&](double bx) { return r3 * std::exp2(bx); },             {2, 3}}
    }, 7.0);
    double r7 = p7.adjustedVal;
    if (p7.clamped || p7.noSolution) invalidPrimes.insert(7);

    // --- Prime 11: {11:6, 11:7, 11:8, 11:9, 11:5, 11:4} ---
    auto p11 = findBestPrime({
        {"11:6", std::log2(11.0 / (r2 * r3)),       [&](double bx) { return r2 * r3 * std::exp2(bx); },        {2, 3}},
        {"11:7", std::log2(11.0 / r7),              [&](double bx) { return r7 * std::exp2(bx); },             {7}},
        {"11:8", std::log2(11.0 / (r2 * r2 * r2)),  [&](double bx) { return r2 * r2 * r2 * std::exp2(bx); },  {2}},
        {"11:9", std::log2(11.0 / (r3 * r3)),       [&](double bx) { return r3 * r3 * std::exp2(bx); },        {3}},
        {"11:5", std::log2(11.0 / r5),              [&](double bx) { return r5 * std::exp2(bx); },             {5}},
        {"11:4", std::log2(11.0 / (r2 * r2)),       [&](double bx) { return r2 * r2 * std::exp2(bx); },        {2}}
    }, 11.0);
    double r11 = p11.adjustedVal;
    if (p11.clamped || p11.noSolution) invalidPrimes.insert(11);

    // --- Prime 13: {13:7, 13:8, 13:9, 13:10, 13:5} ---
    auto p13 = findBestPrime({
        {"13:7",  std::log2(13.0 / r7),              [&](double bx) { return r7 * std::exp2(bx); },             {7}},
        {"13:8",  std::log2(13.0 / (r2 * r2 * r2)),  [&](double bx) { return r2 * r2 * r2 * std::exp2(bx); },  {2}},
        {"13:9",  std::log2(13.0 / (r3 * r3)),       [&](double bx) { return r3 * r3 * std::exp2(bx); },        {3}},
        {"13:10", std::log2(13.0 / (r2 * r5)),       [&](double bx) { return r2 * r5 * std::exp2(bx); },        {2, 5}},
        {"13:5",  std::log2(13.0 / r5),              [&](double bx) { return r5 * std::exp2(bx); },             {5}}
    }, 13.0);
    double r13 = p13.adjustedVal;

    // Store debug info for UI
    auto makeEntry = [](int prime, const PrimeResult& pr) {
        return nlohmann::json{
            {"prime", prime},
            {"scaleDegree", pr.scaleDegreeLabel},
            {"notePitch", pr.nodeX * 1200.0},
            {"chosenRatio", pr.chosenRatio},
            {"deviation", pr.deviation},
            {"adjustedVal", pr.adjustedVal},
            {"clamped", pr.clamped},
            {"noSolution", pr.noSolution}
        };
    };
    followTuningInfo_ = nlohmann::json::array({
        makeEntry(2, p2), makeEntry(3, p3), makeEntry(5, p5),
        makeEntry(7, p7), makeEntry(11, p11), makeEntry(13, p13)
    });

    // Set all six APVTS params
    auto setStretch = [this](const char* id, float value) {
        auto* param = apvts_.getParameter(id);
        if (!param) return;
        float normalized = param->getNormalisableRange().convertTo0to1(value);
        juce::MessageManager::callAsync([param, normalized]() {
            param->beginChangeGesture();
            param->setValueNotifyingHost(normalized);
            param->endChangeGesture();
        });
    };

    setStretch("stretch2",  static_cast<float>(r2));
    setStretch("stretch3",  static_cast<float>(r3));
    setStretch("stretch5",  static_cast<float>(r5));
    setStretch("stretch7",  static_cast<float>(r7));
    setStretch("stretch11", static_cast<float>(r11));
    setStretch("stretch13", static_cast<float>(r13));
}

void PseudoHarmonicProcessor::timerCallback()
{
    // Poll MTS-ESP master presence.  We track two states:
    //   - raw master presence: is a master alive on the bus?  Controls
    //     whether the MTS option is offered in the UI.
    //   - effective MTS activity: are we actually routing through MTS?
    //     False when the user has manually picked MIDI/MPE.
    // When a master *newly* appears we auto-clear any override so MTS takes
    // over automatically (matching the spec).  Manual user picks set the
    // override back on via handleParamFromUI.
    bool rawMaster = (mtsClient_ && MTS_HasMaster(mtsClient_));
    if (rawMaster && !wasRawMasterActive_)
        mtsOverride_.store(false);
    bool effectiveMts = rawMaster && !mtsOverride_.load();

    if (effectiveMts != wasEffectiveMtsActive_)
    {
        wasEffectiveMtsActive_ = effectiveMts;
        engine_.setMTSMasterActive(effectiveMts);
        paramsNeedBroadcast_ = true;
    }
    if (rawMaster != wasRawMasterActive_)
    {
        wasRawMasterActive_ = rawMaster;
        paramsNeedBroadcast_ = true;
    }
    {
        std::string curScale;
        if (rawMaster && mtsClient_)
            if (const char* n = MTS_GetScaleName(mtsClient_))
                curScale = n;
        if (curScale != lastMtsScaleName_)
        {
            lastMtsScaleName_ = std::move(curScale);
            paramsNeedBroadcast_ = true;
        }
    }

    // Send curve data when spectrum params change
    bool curveJustUpdated = curveNeedsUpdate_.exchange(false);
    if (curveJustUpdated)
        sendCurveToUI();

    // Send current params
    if (paramsNeedBroadcast_.exchange(false))
        sendParamsToUI();

    if (modStateNeedsBroadcast_.exchange(false))
        sendModStateToUI();

    // Send output level
    wsBridge_.sendLevel(peakLevel_.load());

    // Always send active notes (cheap — reads lock-free cache from audio thread)
    int noteCount = cachedActiveNoteCount_.load(std::memory_order_acquire);
    nlohmann::json noteArr = nlohmann::json::array();
    std::vector<float> freqs;
    std::vector<int> midiNotes;
    for (int i = 0; i < noteCount; ++i)
    {
        const auto& n = cachedActiveNotes_[i];
        noteArr.push_back({{"note", n.note}, {"freq", n.freq}, {"channel", n.channel}});
        freqs.push_back(n.freq);
        midiNotes.push_back(n.note);
    }
    wsBridge_.sendActiveNotes(noteArr);

    // Compute interval lines
    if (freqs.size() >= 2)
    {
        bool oscConnected = oscReceiver_.isConnected();
        if (oscConnected && oscReceiver_.getTuningVersion() > 0)
            curveCalc_.computeIntervals(freqs, midiNotes, oscReceiver_.getTuningParams());
        else
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
    else
    {
        wsBridge_.sendIntervals(nlohmann::json::array());
    }

    // Send spectrum to plugin via OSC when connected and enabled
    bool oscConnected = oscReceiver_.isConnected();
    if (oscConnected && oscSendSpectrum_.load() && (curveJustUpdated || spectrumNeedsSend_.exchange(false)))
    {
        const auto& ratios = engine_.getFreqRatios();
        const auto& p = pendingParams_;  // read pending (message thread)

        // Amplitudes from the shared spectrum model (includes centre-of-focus,
        // amp tilt, even/odd, strike comb, and the fractional partial window).
        std::array<float, kMaxHarmonics> amps{};
        SpectrumModel::computeGains(p, amps);

        std::vector<std::pair<float, float>> spectrum;
        spectrum.reserve(kMaxHarmonics);
        for (int h = 0; h < kMaxHarmonics; ++h)
        {
            float a = std::abs(amps[h]);
            if (a > 1e-8f)
                spectrum.push_back({ratios[h], a});
        }
        oscReceiver_.sendSpectrum(spectrum);
    }

    // Scale degrees from OSC tuning
    if (oscConnected)
    {
        auto ver = oscReceiver_.getTuningVersion();
        if (ver != lastTuningVersion_ || curveJustUpdated)
        {
            bool tuningChanged = (ver != lastTuningVersion_);
            lastTuningVersion_ = ver;
            auto tuning = oscReceiver_.getTuningParams();

            // Follow Tuning: adjust stretch params to match MOS scale degrees
            if (tuningChanged && followTuning_.load())
                applyFollowTuning(tuning);
            auto degrees = curveCalc_.computeScaleDegrees(tuning);

            cachedScaleDegrees_ = nlohmann::json::array();
            for (const auto& d : degrees)
            {
                cachedScaleDegrees_.push_back({
                    {"cents", d.cents},
                    {"consonance", d.consonance},
                    {"label", d.label},
                    {"tx", d.tuningX},
                    {"ty", d.tuningY},
                    {"inScale", d.inScale}
                });
            }

            // Compute and send node consonances back to plugin
            if (oscSendConsonance_.load())
            {
                auto nodeConsonances = curveCalc_.computeNodeConsonances(tuning);
                oscReceiver_.sendNodeConsonances(nodeConsonances);
            }
        }
        wsBridge_.sendScaleDegrees(cachedScaleDegrees_);

        // Send follow-tuning debug info when active
        if (followTuning_.load() && !followTuningInfo_.empty())
            wsBridge_.sendFollowTuningInfo(followTuningInfo_);
        else
            wsBridge_.sendFollowTuningInfo(nlohmann::json::array());
    }
    else if (wasOscConnected_)
    {
        // Just disconnected — clear UI
        cachedScaleDegrees_ = nlohmann::json::array();
        wsBridge_.sendScaleDegrees(cachedScaleDegrees_);
        followTuningInfo_ = nlohmann::json::array();
        wsBridge_.sendFollowTuningInfo(followTuningInfo_);
    }
    if (oscConnected != wasOscConnected_)
        paramsNeedBroadcast_ = true;
    wasOscConnected_ = oscConnected;
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

    // Non-APVTS params (MPE settings, OSC settings) — handle directly
    if (id == "oscSendConsonance") { oscSendConsonance_ = (value > 0.5f); return; }
    if (id == "oscSendSpectrum") { oscSendSpectrum_ = (value > 0.5f); spectrumNeedsSend_ = true; return; }
    if (id == "showRatioLabels") { showRatioLabels_ = (value > 0.5f); paramsNeedBroadcast_ = true; return; }
    if (id == "followTuning") {
        followTuning_ = (value > 0.5f);
        // Apply immediately if toggled on with active tuning
        if (followTuning_.load() && oscReceiver_.isConnected() && oscReceiver_.getTuningVersion() > 0)
            applyFollowTuning(oscReceiver_.getTuningParams());
        paramsNeedBroadcast_ = true;
        return;
    }

    // Mode overrides: the user explicitly asked for MIDI/MPE/MTS via the
    // mode dropdown.  These aren't APVTS params; they update local state
    // and force a param broadcast so the UI re-renders immediately.
    if (id == "mtsOverride")
    {
        mtsOverride_.store(value > 0.5f);
        paramsNeedBroadcast_ = true;
        return;
    }

    {
        std::lock_guard<std::mutex> lock(pendingParamsMutex_);
        auto& p = pendingParams_;
        if (id == "pitchBendRange") p.pitchBendRange = value;
        else if (id == "mpeEnabled")
        {
            p.mpeEnabled = (value > 0.5f);
            // Explicit MIDI/MPE pick from UI implies the user wants to
            // override MTS for this session.
            mtsOverride_.store(true);
        }
        else if (id == "mpeMasterBendRange") p.mpeMasterBendRange = value;
        else if (id == "mpePerNoteBendRange") p.mpePerNoteBendRange = value;
        else if (id == "excitationMode") p.excitationMode = (value > 0.5f) ? 1 : 0;
        else return;
    }

    paramsDirty_ = true;
    curveNeedsUpdate_ = true;
    paramsNeedBroadcast_ = true;
}

void PseudoHarmonicProcessor::sendCurveToUI()
{
    const auto& ratios = engine_.getFreqRatios();
    const auto& p = pendingParams_;  // read pending (message thread)

    // Amplitudes from the shared spectrum model so the consonance curve
    // reflects the audible spectrum (centre-of-focus, amp tilt, even/odd,
    // strike comb, and the fractional partial window).
    std::array<float, kMaxHarmonics> amps{};
    SpectrumModel::computeGains(p, amps);

    // Build scalatrix::Spectrum from ratios + amplitudes.
    std::vector<scalatrix::Partial> partials;
    partials.reserve(kMaxHarmonics);
    for (int h = 0; h < kMaxHarmonics; ++h)
    {
        double a = std::abs(double(amps[h]));
        if (a > 1e-8)
            partials.push_back({double(ratios[h]), a});
    }
    scalatrix::Spectrum spectrum(std::move(partials));

    float logBaselineToUse = autoLogBaseline_ ? 0.0f : p.logBaseline;
    curveCalc_.compute(spectrum, logBaselineToUse);

    // If auto-computed, update the param to reflect the optimal value
    if (autoLogBaseline_)
    {
        float computed = curveCalc_.getEffectiveLogBaseline();
        {
            std::lock_guard<std::mutex> lock(pendingParamsMutex_);
            pendingParams_.logBaseline = computed;
            paramsDirty_ = true;
        }
        // Update APVTS on message thread so UI knob reflects new value
        auto* param = apvts_.getParameter("logBaseline");
        if (param)
        {
            float normalized = param->getNormalisableRange().convertTo0to1(computed);
            updatingLogBaseline_ = true;
            juce::MessageManager::callAsync([this, param, normalized]() {
                param->setValueNotifyingHost(normalized);
                updatingLogBaseline_ = false;
            });
        }
        paramsNeedBroadcast_ = true;
    }

    const auto& data = curveCalc_.getData();
    nlohmann::json curveJson;

    // Downsample: every 4th point → 1000 points for UI
    constexpr int kDownsampleStep = 4;
    constexpr int kUIResolution = kCurveResolution / kDownsampleStep;

    nlohmann::json plArr = nlohmann::json::array();
    nlohmann::json hullArr = nlohmann::json::array();
    nlohmann::json consArr = nlohmann::json::array();
    for (int i = 0; i < kCurveResolution; i += kDownsampleStep)
    {
        plArr.push_back(data.plCurve[i]);
        hullArr.push_back(data.hullCurve[i]);
        consArr.push_back(data.consonance[i]);
    }
    curveJson["pl"] = plArr;
    curveJson["hull"] = hullArr;
    curveJson["consonance"] = consArr;
    curveJson["maxCents"] = kCurveMaxCents;
    curveJson["resolution"] = kUIResolution;

    // Compute peak ratio labels from pairwise partial intervals
    {
        // Store original (unreduced) ratio + reduced form for each entry
        struct RatioEntry { float cents; int num; int den; int rp; int rq; };
        std::vector<RatioEntry> entries;

        for (int j = 1; j < kMaxHarmonics; ++j)
        {
            if (std::abs(amps[j]) < 1e-6f) continue;
            for (int i = 0; i < j; ++i)
            {
                if (std::abs(amps[i]) < 1e-6f) continue;
                float ratio = ratios[j] / ratios[i];
                if (ratio <= 1.0f) continue;
                float cents = 1200.0f * std::log2(ratio);
                if (cents < 0.0f || cents > kCurveMaxCents) continue;

                int num = j + 1, den = i + 1;
                int g = std::gcd(num, den);
                entries.push_back({cents, num, den, num / g, den / g});
            }
        }

        // Sort by cents
        std::sort(entries.begin(), entries.end(),
                  [](const RatioEntry& a, const RatioEntry& b) { return a.cents < b.cents; });

        // Pass 1: form groups within 2 cents
        struct Group { float avgCents; size_t begin; size_t end; };
        std::vector<Group> groups;
        size_t ei = 0;
        while (ei < entries.size())
        {
            size_t ej = ei + 1;
            float groupSum = entries[ei].cents;
            while (ej < entries.size() && entries[ej].cents - entries[ei].cents < 2.0f)
            {
                groupSum += entries[ej].cents;
                ++ej;
            }
            groups.push_back({groupSum / float(ej - ei), ei, ej});
            ei = ej;
        }

        // Count how many groups each reduced form appears in
        std::map<std::pair<int,int>, int> reducedGroupCount;
        for (const auto& g : groups)
        {
            std::set<std::pair<int,int>> seen;
            for (size_t k = g.begin; k < g.end; ++k)
                seen.insert({entries[k].rp, entries[k].rq});
            for (const auto& r : seen)
                reducedGroupCount[r]++;
        }

        // Pass 2: build labels per group
        // For each reduced form in a group:
        //   - If that reduced form is unique to this group → show reduced (e.g., "2:1")
        //   - If that reduced form appears in multiple groups (separated by warp) →
        //     show the smallest original ratio from this group (e.g., "4:2")
        nlohmann::json peakLabels = nlohmann::json::array();
        for (const auto& g : groups)
        {
            // Collect reduced forms and their smallest original ratio in this group
            std::map<std::pair<int,int>, std::pair<int,int>> reducedToSmallestOrig;
            for (size_t k = g.begin; k < g.end; ++k)
            {
                auto rkey = std::make_pair(entries[k].rp, entries[k].rq);
                auto orig = std::make_pair(entries[k].num, entries[k].den);
                auto it = reducedToSmallestOrig.find(rkey);
                if (it == reducedToSmallestOrig.end() || orig < it->second)
                    reducedToSmallestOrig[rkey] = orig;
            }

            nlohmann::json labels = nlohmann::json::array();
            for (const auto& [reduced, smallestOrig] : reducedToSmallestOrig)
            {
                if (reducedGroupCount[reduced] == 1)
                    labels.push_back(std::to_string(reduced.first) + ":" + std::to_string(reduced.second));
                else
                    labels.push_back(std::to_string(smallestOrig.first) + ":" + std::to_string(smallestOrig.second));
            }

            float cons = curveCalc_.consonanceAt(g.avgCents);
            if (cons > 1e-4f)
                peakLabels.push_back({{"cents", g.avgCents}, {"consonance", cons}, {"labels", labels}});
        }
        curveJson["peakLabels"] = peakLabels;
    }

    wsBridge_.sendCurveData(curveJson);
}

void PseudoHarmonicProcessor::sendParamsToUI()
{
    std::lock_guard<std::mutex> lock(pendingParamsMutex_);
    const auto& p = pendingParams_;

    // Effective tuning mode for the badge: MTS wins unless the user has
    // explicitly overridden, or no master is available.
    bool masterAvailable = wasRawMasterActive_;
    bool mtsActive = wasEffectiveMtsActive_;
    const char* tuningMode = mtsActive ? "MTS"
                                       : (p.mpeEnabled ? "MPE" : "MIDI");
    const std::string& mtsScaleName = lastMtsScaleName_;

    nlohmann::json j = {
        {"stretch2", p.stretch2},
        {"stretch3", p.stretch3},
        {"stretch5", p.stretch5},
        {"stretch7", p.stretch7},
        {"stretch11", p.stretch11},
        {"stretch13", p.stretch13},
        {"decay", p.decay},
        {"release", p.release},
        {"strikePos", p.strikePos},
        {"oddEven", p.oddEven},
        {"strike", p.strike / 0.1f},
        {"volume", p.volume},
        {"noiseMix", p.noiseMix},
        {"sustain", p.sustain / 0.1f},
        {"detune", p.detune},
        {"relaxTime", p.relaxTime},
        {"pitchBendRange", p.pitchBendRange},
        {"mpeEnabled", p.mpeEnabled},
        {"mpeMasterBendRange", p.mpeMasterBendRange},
        {"mpePerNoteBendRange", p.mpePerNoteBendRange},
        {"curvePartials", p.curvePartials},
        {"logBaseline", p.logBaseline},
        {"warp", p.warp},
        {"centreFocus", p.centreFocus},
        {"ampTilt", p.ampTilt},
        {"phaseSpread", p.phaseSpread},
        {"excitationMode", p.excitationMode},
        {"filterType", p.filterType}, {"filterCutoff", p.filterCutoff}, {"filterReso", p.filterReso},
        {"delayTime", p.delayTime}, {"delayFeedback", p.delayFeedback}, {"delayMix", p.delayMix},
        {"reverbAmount", p.reverbAmount}, {"reverbSize", p.reverbSize},
        {"macro1", macroValues_[0].load()}, {"macro2", macroValues_[1].load()},
        {"macro3", macroValues_[2].load()}, {"macro4", macroValues_[3].load()},
        {"macro5", macroValues_[4].load()}, {"macro6", macroValues_[5].load()},
        {"macro7", macroValues_[6].load()}, {"macro8", macroValues_[7].load()},
        {"oscSendConsonance", oscSendConsonance_.load()},
        {"oscSendSpectrum", oscSendSpectrum_.load()},
        {"showRatioLabels", showRatioLabels_.load()},
        {"followTuning", followTuning_.load()},
        {"oscConnected", oscReceiver_.isConnected()},
        {"tuningMode", tuningMode},
        {"mtsMasterAvailable", masterAvailable},
        {"mtsActive", mtsActive},
        {"mtsScaleName", mtsScaleName},
        {"buildTimestamp", PH_BUILD_TIMESTAMP}
    };
    wsBridge_.sendParams(j);
}

void PseudoHarmonicProcessor::handleCommandFromUI(const nlohmann::json& j)
{
    const std::string type = j.value("type", std::string());
    if (type == "modConfig" && j.contains("data"))
    {
        try
        {
            std::lock_guard<std::mutex> lock(modMutex_);
            modConfig_ = modjson::fromJson(j["data"]);
        }
        catch (...) { return; }
        modConfigDirty_ = true;
        modStateNeedsBroadcast_ = true;   // echo to any other clients
    }
    else if (type == "requestModState")
    {
        modStateNeedsBroadcast_ = true;
    }
    else if (type == "presetList")
    {
        sendPresetListToUI();
    }
    else if (type == "presetSave")
    {
        const std::string name = j.value("name", std::string());
        if (!name.empty()) { presetManager_.save(name, buildPresetJson().dump(2)); sendPresetListToUI(); }
    }
    else if (type == "presetLoad")
    {
        const std::string name = j.value("name", std::string());
        auto txt = presetManager_.load(name);
        if (!txt.empty())
        {
            try { applyPresetJson(nlohmann::json::parse(txt)); } catch (...) {}
        }
    }
    else if (type == "presetDelete")
    {
        presetManager_.remove(j.value("name", std::string()));
        sendPresetListToUI();
    }
}

nlohmann::json PseudoHarmonicProcessor::buildPresetJson()
{
    nlohmann::json params;
    for (auto* id : paramIDs)
        if (auto* rp = apvts_.getRawParameterValue(id))
            params[id] = rp->load();

    nlohmann::json j;
    j["params"] = params;
    {
        std::lock_guard<std::mutex> lock(pendingParamsMutex_);
        j["excitationMode"] = pendingParams_.excitationMode;
    }
    {
        std::lock_guard<std::mutex> mlock(modMutex_);
        j["mod"] = modjson::toJson(modConfig_);
    }
    return j;
}

void PseudoHarmonicProcessor::applyPresetJson(const nlohmann::json& j)
{
    const nlohmann::json params = j.contains("params") ? j["params"] : nlohmann::json::object();

    // Set every param: use the preset value if present, else the parameter's
    // default — so a preset load is a clean reset, not a layer over current state.
    for (auto* id : paramIDs)
    {
        float value = 0.0f;
        if (params.contains(id))
            value = params[id].get<float>();
        else if (auto* p = apvts_.getParameter(id))
            value = p->getNormalisableRange().convertFrom0to1(p->getDefaultValue());
        else
            continue;
        handleParamFromUI(id, value);
    }

    handleParamFromUI("excitationMode", static_cast<float>(j.value("excitationMode", 0)));

    {
        std::lock_guard<std::mutex> mlock(modMutex_);
        modConfig_ = j.contains("mod") ? modjson::fromJson(j["mod"]) : ModConfig{};
    }
    modConfigDirty_ = true;
    modStateNeedsBroadcast_ = true;
    paramsNeedBroadcast_ = true;
}

void PseudoHarmonicProcessor::sendPresetListToUI()
{
    nlohmann::json a = nlohmann::json::array();
    for (auto& n : presetManager_.list()) a.push_back(n);
    wsBridge_.sendMessage("presets", a);
}

void PseudoHarmonicProcessor::sendModStateToUI()
{
    nlohmann::json data;
    {
        std::lock_guard<std::mutex> lock(modMutex_);
        data["config"] = modjson::toJson(modConfig_);
    }
    data["sources"] = modjson::sourceList();
    data["dests"]   = modjson::destList();
    wsBridge_.sendMessage("modstate", data);
}

void PseudoHarmonicProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    auto state = apvts_.copyState();
    std::unique_ptr<juce::XmlElement> xml(state.createXml());

    // Save non-APVTS MIDI/MPE settings
    std::lock_guard<std::mutex> lock(pendingParamsMutex_);
    const auto& p = pendingParams_;
    auto* midi = xml->createNewChildElement("MidiSettings");
    midi->setAttribute("mpeEnabled", p.mpeEnabled ? 1 : 0);
    midi->setAttribute("pitchBendRange", (double)p.pitchBendRange);
    midi->setAttribute("mpeMasterBendRange", (double)p.mpeMasterBendRange);
    midi->setAttribute("mpePerNoteBendRange", (double)p.mpePerNoteBendRange);

    // Non-APVTS synth settings (mode switches that aren't automatable floats).
    auto* synth = xml->createNewChildElement("SynthSettings");
    synth->setAttribute("excitationMode", p.excitationMode);

    // Modulation configuration (routes + LFO/envelope settings) as a JSON blob.
    {
        std::lock_guard<std::mutex> mlock(modMutex_);
        auto* mod = xml->createNewChildElement("ModConfig");
        mod->addTextElement(juce::String(modjson::toJson(modConfig_).dump()));
    }

    auto* osc = xml->createNewChildElement("OSCSettings");
    osc->setAttribute("sendConsonance", oscSendConsonance_.load() ? 1 : 0);
    osc->setAttribute("sendSpectrum", oscSendSpectrum_.load() ? 1 : 0);
    osc->setAttribute("showRatioLabels", showRatioLabels_.load() ? 1 : 0);
    osc->setAttribute("followTuning", followTuning_.load() ? 1 : 0);

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
            std::lock_guard<std::mutex> lock(pendingParamsMutex_);
            auto& p = pendingParams_;
            p.mpeEnabled = midi->getIntAttribute("mpeEnabled", 1) != 0;
            p.pitchBendRange = (float)midi->getDoubleAttribute("pitchBendRange", 2.0);
            p.mpeMasterBendRange = (float)midi->getDoubleAttribute("mpeMasterBendRange", 2.0);
            p.mpePerNoteBendRange = (float)midi->getDoubleAttribute("mpePerNoteBendRange", 48.0);
            paramsDirty_ = true;

            // Remove before passing to APVTS so it doesn't see unknown elements
            xml->removeChildElement(midi, true);
        }

        if (auto* synth = xml->getChildByName("SynthSettings"))
        {
            std::lock_guard<std::mutex> lock(pendingParamsMutex_);
            pendingParams_.excitationMode = synth->getIntAttribute("excitationMode", 0);
            paramsDirty_ = true;
            xml->removeChildElement(synth, true);
        }

        if (auto* mod = xml->getChildByName("ModConfig"))
        {
            try
            {
                auto txt = mod->getAllSubText().toStdString();
                if (!txt.empty())
                {
                    std::lock_guard<std::mutex> mlock(modMutex_);
                    modConfig_ = modjson::fromJson(nlohmann::json::parse(txt));
                    modConfigDirty_ = true;
                    modStateNeedsBroadcast_ = true;
                }
            }
            catch (...) {}
            xml->removeChildElement(mod, true);
        }

        if (auto* osc = xml->getChildByName("OSCSettings"))
        {
            oscSendConsonance_ = osc->getIntAttribute("sendConsonance", 0) != 0;
            oscSendSpectrum_ = osc->getIntAttribute("sendSpectrum", 1) != 0;
            showRatioLabels_ = osc->getIntAttribute("showRatioLabels", 0) != 0;
            followTuning_ = osc->getIntAttribute("followTuning", 0) != 0;
            xml->removeChildElement(osc, true);
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
