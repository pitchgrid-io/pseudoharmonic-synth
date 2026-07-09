#pragma once

#include "ModEngine.h"
#include <nlohmann/json.hpp>
#include <string>
#include <vector>

// =============================================================================
// Serialization for the modulation configuration (ModConfig) <-> JSON, shared
// by state persistence and the WebSocket protocol.  Enum values are stored as
// stable string names so the format survives enum reordering.  The full Formula
// is serialized generically (all four components), so basic-authored routes are
// forward-compatible with the expert editor.
// =============================================================================
namespace modjson {

// ---- enum <-> string ----
inline const char* sourceName(ModSourceId s)
{
    switch (s)
    {
        case ModSourceId::None: return "none";
        case ModSourceId::GateW: return "gate"; case ModSourceId::PitchX: return "pitch";
        case ModSourceId::TimbreY: return "timbre"; case ModSourceId::PressureZ: return "pressure";
        case ModSourceId::Velocity: return "velocity"; case ModSourceId::KeyFollow: return "keyfollow";
        case ModSourceId::NoteRandom: return "random"; case ModSourceId::Consonance: return "consonance";
        case ModSourceId::Lfo1: return "lfo1"; case ModSourceId::Lfo2: return "lfo2"; case ModSourceId::Lfo3: return "lfo3";
        case ModSourceId::Env1: return "env1"; case ModSourceId::Env2: return "env2"; case ModSourceId::Env3: return "env3";
        case ModSourceId::Macro1: return "macro1"; case ModSourceId::Macro2: return "macro2";
        case ModSourceId::Macro3: return "macro3"; case ModSourceId::Macro4: return "macro4";
        case ModSourceId::Macro5: return "macro5"; case ModSourceId::Macro6: return "macro6";
        case ModSourceId::Macro7: return "macro7"; case ModSourceId::Macro8: return "macro8";
        default: return "none";
    }
}
inline ModSourceId sourceFromName(const std::string& n)
{
    for (int i = 0; i < static_cast<int>(ModSourceId::Count); ++i)
        if (n == sourceName(static_cast<ModSourceId>(i))) return static_cast<ModSourceId>(i);
    return ModSourceId::None;
}

inline const char* destName(ModDest d)
{
    switch (d)
    {
        case ModDest::Stretch2: return "stretch2"; case ModDest::Stretch3: return "stretch3";
        case ModDest::Stretch5: return "stretch5"; case ModDest::Stretch7: return "stretch7";
        case ModDest::Stretch11: return "stretch11"; case ModDest::Stretch13: return "stretch13";
        case ModDest::Decay: return "decay"; case ModDest::Release: return "release";
        case ModDest::StrikePos: return "strikePos"; case ModDest::OddEven: return "oddEven";
        case ModDest::Strike: return "strike"; case ModDest::Volume: return "volume";
        case ModDest::NoiseMix: return "noiseMix"; case ModDest::Sustain: return "sustain";
        case ModDest::Detune: return "detune"; case ModDest::RelaxTime: return "relaxTime";
        case ModDest::CurvePartials: return "curvePartials"; case ModDest::Warp: return "warp";
        case ModDest::LogBaseline: return "logBaseline";
        case ModDest::CentreFocus: return "centreFocus"; case ModDest::AmpTilt: return "ampTilt";
        case ModDest::PhaseSpread: return "phaseSpread";
        case ModDest::FilterCutoff: return "filterCutoff"; case ModDest::FilterReso: return "filterReso";
        case ModDest::DelayMix: return "delayMix"; case ModDest::ReverbAmount: return "reverbAmount";
        default: return "volume";
    }
}
inline ModDest destFromName(const std::string& n)
{
    for (int i = 0; i < static_cast<int>(ModDest::Count); ++i)
        if (n == destName(static_cast<ModDest>(i))) return static_cast<ModDest>(i);
    return ModDest::Volume;
}

inline const char* xferName(TransferFn t)
{
    switch (t) { case TransferFn::Linear: return "linear"; case TransferFn::S: return "s";
        case TransferFn::Squared: return "squared"; case TransferFn::SquareRoot: return "sqrt";
        case TransferFn::TwoStep: return "2step"; case TransferFn::ThreeStep: return "3step"; }
    return "linear";
}
inline TransferFn xferFromName(const std::string& n)
{
    if (n == "s") return TransferFn::S; if (n == "squared") return TransferFn::Squared;
    if (n == "sqrt") return TransferFn::SquareRoot; if (n == "2step") return TransferFn::TwoStep;
    if (n == "3step") return TransferFn::ThreeStep; return TransferFn::Linear;
}

// ---- component / formula / route ----
inline nlohmann::json componentToJson(const FormulaComponent& c)
{
    return { {"en", c.enabled}, {"src", sourceName(c.source)}, {"mode", static_cast<int>(c.mode)},
             {"xfer", xferName(c.transfer)}, {"min", c.rangeMin}, {"max", c.rangeMax}, {"mult", c.multiplier} };
}
inline FormulaComponent componentFromJson(const nlohmann::json& j)
{
    FormulaComponent c;
    c.enabled = j.value("en", false);
    c.source  = sourceFromName(j.value("src", std::string("none")));
    c.mode    = static_cast<ComponentMode>(j.value("mode", 0));
    c.transfer = xferFromName(j.value("xfer", std::string("linear")));
    c.rangeMin = j.value("min", 0.0f); c.rangeMax = j.value("max", 1.0f); c.multiplier = j.value("mult", 1.0f);
    return c;
}
inline nlohmann::json routeToJson(const Route& r)
{
    return { {"en", r.enabled}, {"dest", destName(r.dest)}, {"combine", static_cast<int>(r.combine)},
             {"op", static_cast<int>(r.formula.op)}, {"anc", static_cast<int>(r.formula.ancillary)},
             {"ancAmt", r.formula.ancillaryAmount}, {"pers", r.formula.persistence}, {"interp", r.formula.interpolation},
             {"w", componentToJson(r.formula.w)}, {"x", componentToJson(r.formula.x)},
             {"y", componentToJson(r.formula.y)}, {"z", componentToJson(r.formula.z)} };
}
inline Route routeFromJson(const nlohmann::json& j)
{
    Route r;
    r.enabled = j.value("en", true);
    r.dest    = destFromName(j.value("dest", std::string("volume")));
    r.combine = static_cast<RouteCombine>(j.value("combine", 0));
    r.formula.op        = static_cast<FormulaOp>(j.value("op", 0));
    r.formula.ancillary = static_cast<AncillaryOp>(j.value("anc", 0));
    r.formula.ancillaryAmount = j.value("ancAmt", 1.0f);
    r.formula.persistence = j.value("pers", 0.0f);
    r.formula.interpolation = j.value("interp", 0.0f);
    if (j.contains("w")) r.formula.w = componentFromJson(j["w"]);
    if (j.contains("x")) r.formula.x = componentFromJson(j["x"]);
    if (j.contains("y")) r.formula.y = componentFromJson(j["y"]);
    if (j.contains("z")) r.formula.z = componentFromJson(j["z"]);
    return r;
}

// ---- full config ----
inline nlohmann::json toJson(const ModConfig& cfg)
{
    nlohmann::json lfos = nlohmann::json::array();
    for (auto& l : cfg.lfos)
        lfos.push_back({ {"rate", l.rateHz}, {"shape", l.shape}, {"retrig", l.retrigger}, {"oneShot", l.oneShot}, {"pw", l.pulseWidth} });
    nlohmann::json envs = nlohmann::json::array();
    for (auto& e : cfg.envs)
        envs.push_back({ {"delay", e.delay}, {"attack", e.attack}, {"hold", e.hold}, {"decay", e.decay},
                         {"sustain", e.sustain}, {"release", e.release}, {"curve", e.curve} });
    nlohmann::json routes = nlohmann::json::array();
    for (auto& r : cfg.routes) routes.push_back(routeToJson(r));
    return { {"lfos", lfos}, {"envs", envs}, {"routes", routes} };
}
inline ModConfig fromJson(const nlohmann::json& j)
{
    ModConfig cfg;
    if (j.contains("lfos"))
        for (int i = 0; i < kNumLfos && i < (int)j["lfos"].size(); ++i) {
            auto& s = j["lfos"][i];
            cfg.lfos[i].rateHz = s.value("rate", 1.0f); cfg.lfos[i].shape = s.value("shape", 0);
            cfg.lfos[i].retrigger = s.value("retrig", true); cfg.lfos[i].oneShot = s.value("oneShot", false);
            cfg.lfos[i].pulseWidth = s.value("pw", 0.5f);
        }
    if (j.contains("envs"))
        for (int i = 0; i < kNumEnvs && i < (int)j["envs"].size(); ++i) {
            auto& s = j["envs"][i];
            cfg.envs[i].delay = s.value("delay", 0.0f); cfg.envs[i].attack = s.value("attack", 0.005f);
            cfg.envs[i].hold = s.value("hold", 0.0f); cfg.envs[i].decay = s.value("decay", 0.2f);
            cfg.envs[i].sustain = s.value("sustain", 1.0f); cfg.envs[i].release = s.value("release", 0.3f);
            cfg.envs[i].curve = s.value("curve", 0.0f);
        }
    if (j.contains("routes"))
        for (auto& r : j["routes"]) cfg.routes.push_back(routeFromJson(r));
    return cfg;
}

// Name lists for UI dropdowns.
inline nlohmann::json sourceList()
{
    nlohmann::json a = nlohmann::json::array();
    for (int i = 1; i < static_cast<int>(ModSourceId::Count); ++i)  // skip None
        a.push_back(sourceName(static_cast<ModSourceId>(i)));
    return a;
}
inline nlohmann::json destList()
{
    nlohmann::json a = nlohmann::json::array();
    for (int i = 0; i < static_cast<int>(ModDest::Count); ++i)
        a.push_back(destName(static_cast<ModDest>(i)));
    return a;
}

} // namespace modjson
