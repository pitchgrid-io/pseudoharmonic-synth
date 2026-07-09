#pragma once

#include <cmath>

// Transfer functions applied to a normalised [0,1] input, returning [0,1].
// These match the EaganMatrix component transfer options; the basic UI exposes
// Linear/Exp/S and the expert UI exposes all of them.
enum class TransferFn
{
    Linear = 0,
    S,          // smoothstep
    Squared,    // exp-like (slow start)
    SquareRoot, // log-like (fast start)
    TwoStep,    // {0, 0.5, 1}
    ThreeStep   // {0, 1/3, 2/3, 1}
};

inline float applyTransfer(TransferFn fn, float u)
{
    if (u < 0.0f) u = 0.0f; else if (u > 1.0f) u = 1.0f;
    switch (fn)
    {
        case TransferFn::Linear:     return u;
        case TransferFn::S:          return u * u * (3.0f - 2.0f * u);
        case TransferFn::Squared:    return u * u;
        case TransferFn::SquareRoot: return std::sqrt(u);
        case TransferFn::TwoStep:    return (u < 0.5f) ? 0.0f : (u < 1.0f ? 0.5f : 1.0f);
        case TransferFn::ThreeStep:  return (u < 1.0f/3.0f) ? 0.0f
                                          : (u < 2.0f/3.0f) ? 1.0f/3.0f
                                          : (u < 1.0f)      ? 2.0f/3.0f : 1.0f;
    }
    return u;
}
