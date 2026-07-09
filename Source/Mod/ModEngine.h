#pragma once

#include "../DSP/PseudoHarmonicEngine.h"  // for SynthParams
#include "ModSource.h"
#include "ShapeGenerator.h"
#include "Envelope.h"
#include "Formula.h"
#include "ModMatrix.h"
#include <array>
#include <vector>

// =============================================================================
// ModEngine — the modulation fabric between the plugin's base parameters and
// the DSP engine.  It owns the modulation SOURCES (LFOs/shape generators,
// envelopes, macros, and the live expression/consonance snapshot) and the
// MATRIX of ROUTES (source→destination formulas).  Each block it advances the
// sources, evaluates the routes into a per-destination modulation offset, and
// resolves base (+) offset into the effective SynthParams for the audio engine.
//
// Modulation is currently GLOBAL (one modulated SynthParams for all voices),
// matching the shared-params audio engine.  Per-voice modulation is the next
// architectural step — see docs/modulation-architecture.md.
//
// The general (EaganMatrix-class) Formula model backs every route; a basic
// depth+curve route is just a single-component formula (Formula::basic()).
// =============================================================================

// Modulatable destinations (every synth parameter that can be modulated).
enum class ModDest
{
    Stretch2, Stretch3, Stretch5, Stretch7, Stretch11, Stretch13,
    Decay, Release,
    StrikePos, OddEven, Strike, Volume, NoiseMix, Sustain,
    Detune, RelaxTime,
    CurvePartials, Warp, LogBaseline,
    CentreFocus, AmpTilt, PhaseSpread,
    FilterCutoff, FilterReso,
    DelayMix, ReverbAmount,
    Count
};

// Serializable modulation configuration (the editable part: LFO/envelope
// settings + routes).  Owned by the processor on the message thread and pushed
// into the ModEngine (which keeps the runtime STATE — phase, envelope stage) via
// applyConfig().  This separation lets edits happen without resetting running
// generators.
struct ModConfig
{
    struct LfoCfg  { float rateHz = 1.0f; int shape = 0; bool retrigger = true; bool oneShot = false; float pulseWidth = 0.5f; };
    struct EnvCfg  { float delay = 0.0f, attack = 0.005f, hold = 0.0f, decay = 0.2f, sustain = 1.0f, release = 0.3f, curve = 0.0f; };

    std::array<LfoCfg, kNumLfos> lfos{};
    std::array<EnvCfg, kNumEnvs> envs{};
    std::vector<Route>           routes;
};

// Per-block expression / analysis snapshot fed in from the processor.
struct ModInputs
{
    bool  gate       = false;  // any voice active
    float timbreY    = 0.0f;   // MPE Y / CC74, 0..1
    float pressureZ  = 0.0f;   // channel pressure / aftertouch, 0..1
    float velocity   = 0.0f;   // last note velocity, 0..1
    float pitchX     = 0.0f;   // key-follow, bipolar -1..1 (note vs centre)
    float consonance = 0.0f;   // live consonance of sounding interval, 0..1
};

class ModEngine
{
public:
    void prepare(double sampleRate, int blockSize)
    {
        sampleRate_ = sampleRate;
        blockSize_  = blockSize;
    }

    // ---- base params ----
    void setBaseParams(const SynthParams& p) { base_ = p; }
    const SynthParams& baseParams() const { return base_; }

    // ---- source access (for UI/preset config and macro/external drive) ----
    std::array<ShapeGenerator, kNumLfos>& lfos() { return lfos_; }
    std::array<Envelope, kNumEnvs>&        envs() { return envs_; }
    void  setMacro(int i, float v) { if (i >= 0 && i < kNumMacros) macros_[i] = v; }
    float macro(int i) const { return (i >= 0 && i < kNumMacros) ? macros_[i] : 0.0f; }

    // ---- routes ----
    std::vector<Route>& routes() { return routes_; }
    const std::vector<Route>& routes() const { return routes_; }
    void clearRoutes() { routes_.clear(); }
    void addRoute(const Route& r) { routes_.push_back(r); }

    // Apply an edited configuration (routes + LFO/envelope settings) without
    // disturbing runtime state (LFO phase, envelope stage).  Called on the audio
    // thread with the config mutex held.
    void applyConfig(const ModConfig& cfg)
    {
        routes_ = cfg.routes;
        for (int i = 0; i < kNumLfos; ++i)
        {
            lfos_[i].rateHz     = cfg.lfos[i].rateHz;
            lfos_[i].shape      = static_cast<SGShape>(cfg.lfos[i].shape);
            lfos_[i].retrigger  = cfg.lfos[i].retrigger;
            lfos_[i].oneShot    = cfg.lfos[i].oneShot;
            lfos_[i].pulseWidth = cfg.lfos[i].pulseWidth;
        }
        for (int i = 0; i < kNumEnvs; ++i)
        {
            envs_[i].delay   = cfg.envs[i].delay;
            envs_[i].attack  = cfg.envs[i].attack;
            envs_[i].hold    = cfg.envs[i].hold;
            envs_[i].decay   = cfg.envs[i].decay;
            envs_[i].sustain = cfg.envs[i].sustain;
            envs_[i].release = cfg.envs[i].release;
            envs_[i].curve   = cfg.envs[i].curve;
        }
    }

    // ---- gate / trigger (monophonic-global for now) ----
    void noteOn()
    {
        bool rising = (activeNotes_ == 0);
        ++activeNotes_;
        if (rising)
            for (auto& e : envs_) e.gate(true);
        for (auto& l : lfos_) l.gate(true);   // retrigger per note-on if enabled
    }
    void noteOff()
    {
        if (activeNotes_ > 0) --activeNotes_;
        if (activeNotes_ == 0)
            for (auto& e : envs_) e.gate(false);
    }
    void allNotesOff()
    {
        activeNotes_ = 0;
        for (auto& e : envs_) e.gate(false);
    }

    // Advance sources and evaluate the matrix into the per-destination offset.
    void process(const ModInputs& in, double dt)
    {
        for (auto& l : lfos_) l.advance(dt);
        for (auto& e : envs_) e.advance(dt);

        SourceBank bank;
        bank.set(ModSourceId::GateW,     (activeNotes_ > 0) ? 1.0f : 0.0f);
        bank.set(ModSourceId::TimbreY,   in.timbreY);
        bank.set(ModSourceId::PressureZ, in.pressureZ);
        bank.set(ModSourceId::Velocity,  in.velocity);
        bank.set(ModSourceId::PitchX,    in.pitchX);
        bank.set(ModSourceId::KeyFollow, in.pitchX * 0.5f + 0.5f);
        bank.set(ModSourceId::Consonance, in.consonance);
        for (int i = 0; i < kNumLfos; ++i)
            bank.set(static_cast<ModSourceId>(static_cast<int>(ModSourceId::Lfo1) + i), lfos_[i].current());
        for (int i = 0; i < kNumEnvs; ++i)
            bank.set(static_cast<ModSourceId>(static_cast<int>(ModSourceId::Env1) + i), envs_[i].current());
        for (int i = 0; i < kNumMacros; ++i)
            bank.set(static_cast<ModSourceId>(static_cast<int>(ModSourceId::Macro1) + i), macros_[i]);

        mod_.fill(0.0f);
        for (auto& r : routes_)
        {
            if (!r.enabled) continue;
            float val = r.formula.eval(bank, dt);
            int d = static_cast<int>(r.dest);
            if (d < 0 || d >= static_cast<int>(ModDest::Count)) continue;
            switch (r.combine)
            {
                case RouteCombine::Add:      mod_[d] += val; break;
                case RouteCombine::Subtract: mod_[d] -= val; break;
                case RouteCombine::Multiply: mod_[d] += val; break; // TODO: true multiply
            }
        }
    }

    // Effective params for the audio engine: base (+) accumulated modulation.
    void resolve(SynthParams& out) const
    {
        out = base_;
        applyModOffsets(out);
    }

    // True when there is anything to modulate this block (so the processor
    // re-resolves every block rather than only on base changes).
    bool hasActiveModulation() const
    {
        for (const auto& r : routes_) if (r.enabled) return true;
        return false;
    }

private:
    void applyModOffsets(SynthParams& p) const
    {
        auto m = [this](ModDest d) { return mod_[static_cast<int>(d)]; };
        p.stretch2      += m(ModDest::Stretch2);
        p.stretch3      += m(ModDest::Stretch3);
        p.stretch5      += m(ModDest::Stretch5);
        p.stretch7      += m(ModDest::Stretch7);
        p.stretch11     += m(ModDest::Stretch11);
        p.stretch13     += m(ModDest::Stretch13);
        p.decay         += m(ModDest::Decay);
        p.release       += m(ModDest::Release);
        p.strikePos     += m(ModDest::StrikePos);
        p.oddEven       += m(ModDest::OddEven);
        p.strike        += m(ModDest::Strike);
        p.volume        += m(ModDest::Volume);
        p.noiseMix      += m(ModDest::NoiseMix);
        p.sustain       += m(ModDest::Sustain);
        p.detune        += m(ModDest::Detune);
        p.relaxTime     += m(ModDest::RelaxTime);
        p.curvePartials += m(ModDest::CurvePartials);
        p.warp          += m(ModDest::Warp);
        p.logBaseline   += m(ModDest::LogBaseline);
        p.centreFocus   += m(ModDest::CentreFocus);
        p.ampTilt       += m(ModDest::AmpTilt);
        p.phaseSpread   += m(ModDest::PhaseSpread);
        p.filterCutoff  += m(ModDest::FilterCutoff);
        p.filterReso    += m(ModDest::FilterReso);
        p.delayMix      += m(ModDest::DelayMix);
        p.reverbAmount  += m(ModDest::ReverbAmount);
    }

    double sampleRate_ = 44100.0;
    int    blockSize_  = 512;

    SynthParams base_{};
    std::array<float, static_cast<int>(ModDest::Count)> mod_{};

    std::array<ShapeGenerator, kNumLfos> lfos_{};
    std::array<Envelope, kNumEnvs>       envs_{};
    std::array<float, kNumMacros>        macros_{};
    std::vector<Route>                   routes_;

    int activeNotes_ = 0;
};
