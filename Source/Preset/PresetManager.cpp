#include "PresetManager.h"

namespace {
struct Factory { const char* name; const char* json; };

// Built-in starter presets.  JSON = { params:{id:value}, excitationMode, mod:{...} }.
// Only non-default params need be listed; the processor fills the rest from
// parameter defaults on load.  Routes need only {dest, w:{en,src,xfer,min,max}}.
const Factory kFactory[] = {
{ "Bell Stretch",
  R"JSON({
    "params": { "stretch3": 3.03, "stretch5": 5.06, "decay": 6.0, "release": 3.0,
                "strikePos": 0.26, "oddEven": 0.65, "reverbAmount": 0.35, "reverbSize": 0.72 },
    "excitationMode": 0,
    "mod": { "lfos": [ {"rate": 0.25, "shape": 0} ],
             "routes": [ {"dest": "centreFocus", "w": {"en": true, "src": "lfo1", "xfer": "linear", "min": -5, "max": 5}} ] }
  })JSON" },

{ "Bowed Pad",
  R"JSON({
    "params": { "decay": 8.0, "release": 2.5, "sustain": 6.0, "oddEven": 0.9,
                "filterType": 1, "filterCutoff": 2500.0, "filterReso": 0.35,
                "reverbAmount": 0.45, "reverbSize": 0.8, "volume": 0.9 },
    "excitationMode": 1,
    "mod": { "lfos": [ {"rate": 0.18, "shape": 0} ],
             "envs": [ {"attack": 0.8, "decay": 1.0, "sustain": 0.7, "release": 1.5} ],
             "routes": [ {"dest": "filterCutoff", "w": {"en": true, "src": "env1", "xfer": "linear", "min": 0, "max": 4000}},
                         {"dest": "centreFocus",  "w": {"en": true, "src": "lfo1", "xfer": "linear", "min": -3, "max": 3}} ] }
  })JSON" },

{ "Macro Morph",
  R"JSON({
    "params": { "decay": 4.0, "release": 2.0, "strikePos": 0.5, "delayMix": 0.25,
                "delayTime": 0.28, "delayFeedback": 0.4 },
    "excitationMode": 0,
    "mod": { "routes": [
        {"dest": "stretch3", "w": {"en": true, "src": "macro1", "xfer": "linear", "min": 0, "max": 0.06}},
        {"dest": "stretch5", "w": {"en": true, "src": "macro1", "xfer": "linear", "min": 0, "max": -0.1}},
        {"dest": "ampTilt",  "w": {"en": true, "src": "macro2", "xfer": "linear", "min": 0, "max": 3.0}} ] }
  })JSON" },

{ "Tremolo Keys",
  R"JSON({
    "params": { "decay": 3.0, "release": 1.0, "strikePos": 0.4, "oddEven": 1.0 },
    "excitationMode": 0,
    "mod": { "lfos": [ {"rate": 5.5, "shape": 0} ],
             "routes": [ {"dest": "volume", "w": {"en": true, "src": "lfo1", "xfer": "linear", "min": -0.4, "max": 0.4}} ] }
  })JSON" },
};
}

void PresetManager::seedFactory()
{
    for (auto& f : kFactory)
    {
        auto file = fileFor(f.name);
        if (!file.existsAsFile())
            file.replaceWithText(juce::String::fromUTF8(f.json));
    }
}
