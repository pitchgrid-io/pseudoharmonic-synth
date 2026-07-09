# Modulation Architecture — design reference

Status: **active design** (supersedes the earlier "modern mod-matrix only" decision).
Audience: future us. This records *why* we chose a general (EaganMatrix-class) modulation
engine, how EaganMatrix concepts map onto this synth, the subset relationship that lets a
simple UI ride on top of it, and what a full implementation entails.

## Decision & rationale

We implement the **general, EaganMatrix-style modulation model as the engine**, and expose it
through **two UI layers**:

- **Basic mode** — a Bitwig/Serum-style experience: `source → destination` routes with a
  bipolar **depth** and an optional **transfer curve**; plus a handful of envelopes, LFOs and
  macros. Easy to learn.
- **Expert mode** — the full model: multi-component **formulas**, component **modes** and
  **transfer functions**, **operator precedence**, **ancillary operators**, **blend**, and
  **persistence/interpolation**, edited on a source×destination **matrix**.

The key insight that makes this "best of both worlds" rather than two separate systems:
**a basic depth+curve route is a strict subset of a general formula** — specifically a formula
with a single active component whose input is the chosen source, a range = depth, and a transfer
curve. So the basic UI *authors the same underlying objects* the expert UI does; there is one
engine, one data model, one persistence format. Nothing is thrown away when a user switches modes;
basic-authored routes open cleanly in expert mode.

Originally (see `plan`) we picked the simpler model to reduce build cost. We changed our mind:
the extra engine complexity is contained and one-time, whereas retrofitting formula power onto a
scalar-depth engine later would be a disruptive migration of the data model, persistence, and UI.
Better to build the general core now and gate the complexity behind an expert toggle.

## Mapping EaganMatrix → this synth

EaganMatrix is driven by a 3D fingerboard (Continuum). We have no fingerboard; our performance
inputs are MPE/MIDI + host/OSC. The WXYZ performance dimensions map as:

| EaganMatrix | Meaning | This synth's source |
|---|---|---|
| **W** | Window/gate (finger down / constant / SG-gated) | Note gate; or "always on" constant; or a Shape Generator |
| **X** | Left-right position → pitch | MIDI note number / key-follow; MPE pitch bend |
| **Y** | Front-back position → timbre | MPE Y (CC74 / slide) |
| **Z** | Pressure | Channel pressure / poly aftertouch; velocity as fallback |

Other EaganMatrix sources map directly: **Macros i–vi** → our **Macros** (external-drivable via
MIDI CC / OSC / automation); **Shape Generators SG1–5** → our **LFOs/shape generators**;
**FormulaDelay / other formulas** → **formula-referencing-formula** (deferred, see below).

Difference in the matrix's job: EaganMatrix's matrix routes **both audio and control** between
modules (its "banks" are patched together on the same grid). Our audio path is fixed
(`oscillator → filter → amp → master fx`), so **our matrix is control-only**: it modulates synth
*parameters*. This removes a large chunk of EaganMatrix's complexity (audio routing, per-module
source/destination columns) while keeping the expressive part (the formula language).

## The general model

### Sources
Per-voice expression (gate W, pitch X, timbre Y, pressure Z), velocity, key-follow, note-random;
**Envelopes** (multi-stage); **Shape Generators / LFOs** (gate-triggerable, many shapes,
per-use phase/shape selection as in EaganMatrix); **Macros**; **External** (MIDI CC, OSC, host
automation); the live **consonance** value; and (deferred) **other formulas / formula-delay**.

### Destinations
Every synth parameter (`ModDest` enum in `Source/Mod/ModEngine.h`): spectrum (stretches,
centre-of-focus, amp tilt, phase spread, warp, partials), envelope/timbre params, filter, fx, amp.

### Formula
A reusable, named control function (EaganMatrix "A–V"). Structure:

- **Four components W, X, Y, Z**, each:
  - *input source* (which performance/mod source drives it),
  - *mode*: Continuous / Initial (sample-and-hold at note start) / Relative / Derivative,
  - *transfer function*: Linear, S, Squared, Square-root, 2-Step, 3-Step,
  - *range* (min→max) and *domain* (input window) → output scaling,
  - *multiplier* (decade steps) and *modifier* (scale by a macro or voice number).
- **Operator precedence** combining the components: `W+X+Y+Z` (default), `W·(X+Y+Z)`,
  `(W+X+Y)·Z`, `(W+X)·(Y+Z)`, `(W·X)+(Y·Z)`.
- **Ancillary operators** (primary + secondary): apply a math op to the result —
  `·`, `|·|`, `+`, `^` (power), `log`, `mod`, `quant`, `crosses`, `min`, `max` — with the operand
  a constant / another formula / MIDI clock; plus optional unit conversion (note-number → Hz).
- **Blend**: morph a set of the formula's own settings (component ranges, persistence, …) between
  a *primary* and *secondary* value, driven by a macro / formula / input-source flag.
- **Persistence** (slews *decreases* only — adds sustain) and **Interpolation** (smooths all
  changes — removes stepping/zipper noise).

### Matrix / routes
The matrix is a set of **routes** `{ source, destination, formula }`. The formula scales how the
source affects the destination (per block, control-rate, smoothed). "Direct" routes (no audio
source; EaganMatrix Direct+/−/×) are just routes whose formula injects a value straight into a
destination. Multiple routes on one destination **sum** (with per-route add/subtract/multiply as
an expert option).

### Subset relationship (why basic works)
A **basic route** = a formula with exactly one active component:
`component.input = <chosen source>`, `mode = Continuous`, `transferFn = <chosen curve>`,
`range = <bipolar depth>`, everything else neutral, no ancillary, no blend. The basic UI writes
exactly this; the expert UI can open and extend it. A **basic envelope/LFO** is the same
underlying Envelope/ShapeGenerator object with advanced fields hidden.

## UI: basic vs expert

- A single **mode toggle** (persisted per user/preset). Basic hides: multi-component formula
  editing, modes other than Continuous, operator precedence, ancillary, blend, persistence,
  domain windows, per-use SG shape/phase. Expert reveals all of it plus the raw matrix grid.
- Basic surface: knobs with **depth rings + drag-to-assign**, a compact **macro panel**, simple
  **env/LFO editors**, and the **spectrum + consonance** view.
- Expert surface: adds a **formula editor** panel (WXYZ components, operators, ancillary, blend,
  persistence/interpolation) and a **matrix** view (sources × destinations).
- Progressive disclosure: a route authored in basic shows a small "advanced" affordance that opens
  its formula in the expert editor.

## What a full implementation entails (and current phasing)

Building the general core is more work than scalar depths, concentrated in:

1. **Formula evaluator** — the WXYZ component math, modes, transfer functions, operator precedence,
   ancillary ops, blend, persistence/interpolation. Pure, unit-testable, control-rate.
2. **Data model + persistence** — formulas, routes, sources are serialized (JSON preset + APVTS
   state). Design the schema **general from day one** so basic-authored data is forward-compatible
   with expert features. This is the main reason to build general now rather than migrate later.
3. **Sources/destinations registries** — already begun (`ModDest`); add source registry + IDs.
4. **UI** — two layers over one model (basic subset view + expert full view), plus the mode toggle.

Pragmatic staging (the engine is general; UI/feature breadth grows):
- **Phase 3 (now)**: general `Formula` + `ModMatrix` + `ModSource` (envelopes, LFOs/shape gens,
  expression), evaluated control-rate; **basic-equivalent** authoring wired first (single-component
  formulas), but the types are the full general ones. Verify audible modulation end-to-end.
- **Deferred within the general model** (add incrementally, no schema break): secondary ancillary,
  formula-references-formula / formula-delay, blend-on-transfer-function, derivative/relative modes
  polish, per-use SG phase-modulation. Track here as they land.
- **UI expert mode**: lands in the UI phase (formula editor + matrix grid) after the basic surface.

## Per-voice vs global modulation (important)

The audio engine currently applies **one `SynthParams` to all voices** (shared `freqRatios_`/
`harmonicGains_`). So modulation is first implemented **globally**: sources are evaluated once per
block and produce one modulated `SynthParams` for all voices. This already delivers the headline
value — LFO/macro/envelope-swept spectra — at low risk.

**True per-voice modulation** (each note's own envelope/expression driving its *own* spectrum, as
EaganMatrix does per finger) requires a real refactor: per-voice effective params and per-voice
`SpectrumModel` evaluation each block (≈32 voices × recompute; control-rate, cache prime
factorisations). This is the **next architectural step (Phase 3b)** and is tracked here, not yet
built. Note that some per-voice expression already exists (per-note MPE bend, pressure→noise/
sustain live in the voice). Non-spectrum per-voice targets (amp, filter cutoff — Phase 5) can be
made per-voice cheaply without a spectrum recompute.

## Open questions to revisit
- Macro count (currently planning 8) and whether macros are themselves formula-driven.
- Whether any modulation needs audio-rate (FM/ring) — currently control-rate only.
- Host-automation exposure: base params + macros only (VST3 param-count limits); formulas/matrix
  live in state, not as automatable params.
