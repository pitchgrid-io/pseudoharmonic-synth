#pragma once

#include "ModSource.h"
#include "TransferCurve.h"
#include <array>
#include <algorithm>
#include <cmath>
#include <initializer_list>

// =============================================================================
// Formula — the general (EaganMatrix-class) control function.  A formula
// combines up to four components (W, X, Y, Z), each driven by a mod source and
// shaped by a transfer function + range, via an operator precedence, then
// post-processed by an ancillary op and persistence/interpolation smoothing.
//
// A BASIC route is just a formula with a single enabled component (its source =
// the route's source, range = the route's depth, transfer = the route's curve).
// The expert UI exposes the rest.  See docs/modulation-architecture.md.
//
// v1 status: components (Continuous mode), transfer functions, operator
// precedence, a primary ancillary op, and persistence/interpolation are
// implemented.  Component modes other than Continuous, secondary ancillary,
// blend, and formula-references-formula are DEFERRED (fields reserved) and
// tracked in the design doc.  Evaluated at control rate.
// =============================================================================

enum class ComponentMode { Continuous = 0, Initial, Relative, Derivative };

enum class FormulaOp
{
    Sum = 0,          // W + X + Y + Z   (default)
    WTimesSum,        // W * (X + Y + Z)
    SumTimesZ,        // (W + X + Y) * Z
    PairProducts,     // (W + X) * (Y + Z)
    CrossProducts     // (W * X) + (Y * Z)
};

enum class AncillaryOp
{
    None = 0, Multiply, Add, Power, Min, Max, Quantize, AbsMultiply
};

struct FormulaComponent
{
    bool         enabled   = false;
    ModSourceId  source    = ModSourceId::None;
    ComponentMode mode     = ComponentMode::Continuous;
    TransferFn   transfer  = TransferFn::Linear;
    float        rangeMin  = 0.0f;   // output at input 0
    float        rangeMax  = 1.0f;   // output at input 1
    float        multiplier = 1.0f;

    // Raw component value in destination units (identity element handled by the
    // combiner, not here).
    float eval(const SourceBank& bank) const
    {
        float s = bank.value(source);
        float u = modSourceIsBipolar(source) ? (s * 0.5f + 0.5f) : s;
        float t = applyTransfer(transfer, u);
        return (rangeMin + t * (rangeMax - rangeMin)) * multiplier;
    }
};

struct Formula
{
    FormulaComponent w, x, y, z;
    FormulaOp   op        = FormulaOp::Sum;
    AncillaryOp ancillary = AncillaryOp::None;
    float       ancillaryAmount = 1.0f;

    float persistence   = 0.0f;   // seconds; slows decreases (adds sustain)
    float interpolation = 0.0f;   // seconds; smooths all changes

    // runtime smoothing state
    float state   = 0.0f;
    bool  primed  = false;

    void reset() { state = 0.0f; primed = false; }

    // Build the common single-component ("basic") formula.
    static Formula basic(ModSourceId src, float depth,
                         TransferFn curve = TransferFn::Linear)
    {
        Formula f;
        f.w.enabled = true;
        f.w.source  = src;
        f.w.transfer = curve;
        // Bipolar depth: centre at 0 so a bipolar source swings +/-depth.
        f.w.rangeMin = modSourceIsBipolar(src) ? -depth : 0.0f;
        f.w.rangeMax = depth;
        f.op = FormulaOp::Sum;
        return f;
    }

    // Evaluate raw (pre-smoothing) value from the current source snapshot.
    float evalRaw(const SourceBank& bank) const
    {
        auto sumGroup = [&](std::initializer_list<const FormulaComponent*> cs) {
            float s = 0.0f;
            for (auto* c : cs) if (c->enabled) s += c->eval(bank);
            return s;
        };
        auto factor = [&](const FormulaComponent* c) { // multiplicative identity 1
            return c->enabled ? c->eval(bank) : 1.0f;
        };
        auto product = [&](const FormulaComponent* a, const FormulaComponent* b) {
            if (!a->enabled && !b->enabled) return 0.0f;
            return factor(a) * factor(b);
        };

        float r = 0.0f;
        switch (op)
        {
            case FormulaOp::Sum:          r = sumGroup({&w, &x, &y, &z}); break;
            case FormulaOp::WTimesSum:    r = factor(&w) * sumGroup({&x, &y, &z}); break;
            case FormulaOp::SumTimesZ:    r = sumGroup({&w, &x, &y}) * factor(&z); break;
            case FormulaOp::PairProducts: r = sumGroup({&w, &x}) * sumGroup({&y, &z}); break;
            case FormulaOp::CrossProducts:r = product(&w, &x) + product(&y, &z); break;
        }

        // Primary ancillary operator.
        const float b = ancillaryAmount;
        switch (ancillary)
        {
            case AncillaryOp::None:                                   break;
            case AncillaryOp::Multiply:    r = r * b;                 break;
            case AncillaryOp::Add:         r = r + b;                 break;
            case AncillaryOp::Power:       r = std::pow(std::fabs(r), b); break;
            case AncillaryOp::Min:         r = std::min(r, b);        break;
            case AncillaryOp::Max:         r = std::max(r, b);        break;
            case AncillaryOp::Quantize:    r = (b != 0.0f) ? std::round(r / b) * b : r; break;
            case AncillaryOp::AbsMultiply: r = std::fabs(r) * b;      break;
        }
        return r;
    }

    // Evaluate with per-block smoothing (persistence + interpolation).  dt in s.
    float eval(const SourceBank& bank, double dt)
    {
        float raw = evalRaw(bank);
        if (!primed) { state = raw; primed = true; return state; }

        float target = raw;
        // Interpolation: one-pole smoothing of all changes.
        if (interpolation > 0.0f)
        {
            float a = 1.0f - std::exp(-static_cast<float>(dt) / interpolation);
            target = state + (raw - state) * a;
        }
        // Persistence: slow decreases only (increases pass through).
        if (persistence > 0.0f && target < state)
        {
            float a = 1.0f - std::exp(-static_cast<float>(dt) / persistence);
            target = state + (target - state) * a;
        }
        state = target;
        return state;
    }
};
