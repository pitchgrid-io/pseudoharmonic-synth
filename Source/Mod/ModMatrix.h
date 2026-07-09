#pragma once

#include "Formula.h"

// Forward reference to the destination enum (defined in ModEngine.h).
enum class ModDest;

// How a route's contribution combines onto its destination's accumulator.
enum class RouteCombine { Add = 0, Subtract, Multiply };

// A single matrix route: a formula placed at (source-implied) → destination.
// In basic mode the formula is a single-component depth+curve; in expert mode
// it can be a full multi-component formula.
struct Route
{
    bool         enabled = true;
    ModDest      dest{};
    Formula      formula;
    RouteCombine combine = RouteCombine::Add;
};
