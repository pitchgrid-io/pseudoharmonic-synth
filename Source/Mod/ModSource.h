#pragma once

#include <array>

// Modulation source identity.  Contiguous so a SourceBank can index by value.
// Counts follow the v1 decision: 3 LFOs, 3 envelopes, 8 macros.
enum class ModSourceId
{
    None = 0,
    // Performance / expression (EaganMatrix W/X/Y/Z mapped to MPE/MIDI)
    GateW,        // note gate (0/1), or held
    PitchX,       // key-follow / note number, bipolar around centre
    TimbreY,      // MPE Y / CC74
    PressureZ,    // channel pressure / aftertouch
    Velocity,
    KeyFollow,    // alias kept for clarity; same idea as PitchX but 0..1
    NoteRandom,   // random per note-on
    Consonance,   // live consonance of the sounding interval, 0..1
    // Generators
    Lfo1, Lfo2, Lfo3,
    Env1, Env2, Env3,
    // Macros
    Macro1, Macro2, Macro3, Macro4, Macro5, Macro6, Macro7, Macro8,
    Count
};

inline constexpr int kNumLfos   = 3;
inline constexpr int kNumEnvs   = 3;
inline constexpr int kNumMacros = 8;

// Bipolar sources swing [-1,1] and are re-centred to [0,1] before a transfer
// function is applied; unipolar sources already sit in [0,1].
inline bool modSourceIsBipolar(ModSourceId id)
{
    switch (id)
    {
        case ModSourceId::Lfo1: case ModSourceId::Lfo2: case ModSourceId::Lfo3:
        case ModSourceId::PitchX:
            return true;
        default:
            return false;
    }
}

// Current value of every source for one evaluation (control-rate snapshot).
struct SourceBank
{
    std::array<float, static_cast<int>(ModSourceId::Count)> v{};

    float value(ModSourceId id) const { return v[static_cast<int>(id)]; }
    void  set(ModSourceId id, float x) { v[static_cast<int>(id)] = x; }
};
