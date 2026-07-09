# PseudoHarmonic Synth — Usage Guide

A polyphonic synth whose core oscillator is a **microtuned pseudoharmonic spectrum** (partials
retuned per prime factor), wrapped in an EaganMatrix-style modulation engine, with a real-time
consonance visualization. Runs as Standalone, VST3, and AU; the UI is a web view.

## Running

**Development (hot reload):**
```bash
./run_dev.sh
```
Starts the UI dev server on an ephemeral port, builds + launches the Debug Standalone, and loads
the UI from the dev server so frontend edits hot-reload. Ctrl+C (or closing the app) stops
everything. See the README for a Release build.

## The window at a glance

- **Header** — logo, tuning-mode selector (MPE / MIDI / MTS), active notes, Follow Tuning, settings.
- **Consonance plot** — the dissonance/consonance curve for the current spectrum over 0–1200+ cents,
  with just-intonation ratio labels at consonance peaks and overlays for the played interval and the
  external scale. As you change the spectrum, the consonance peaks migrate in real time.
- **Tabs** (below the plot): **Play · Modulation · Filter / FX · Presets**.

## Play tab

The core oscillator + envelope controls.

- **Spectrum** — the heart of the instrument. `2nd / 3rd / 5th / 7th / 11th / 13th` stretch each
  prime's partials (±3% around the just value); at their defaults you get an exact harmonic series,
  deviate for a *pseudoharmonic* timbre. `Warp` sets how many low partials are pseudoharmonic vs pure.
- **Timbre** — `Strike` (attack strength), `Strike Pos` (comb-filter position), `Odd/Even`
  (even-partial level), `Noise`.
- **Envelope** — `Decay`, `Sustain`, `Release`, `Onset Pitch` + `Settle` (attack pitch glide),
  `Volume`.
- **Consonance** — `Partials` (how many partials feed the curve) and `Log Base` (curve baseline;
  auto-optimizes as you edit the spectrum).

There are two **excitation modes** (state-persisted): *impact* (struck/decaying, the default) and
*continuous* (driven/sustained, for bowed/blown sounds). A preset sets this.

## Modulation tab

The general (EaganMatrix-class) modulation engine, presented in a basic form.

- **Macros (M1–M8)** — global control handles. Assign them as mod sources, and drive them from host
  automation, **MIDI CC 20–27**, or **OSC** (`/pitchgrid/plugin/macro <index 0-7> <value 0-1>`).
- **LFOs (×3)** — rate (Hz), shape (Sine, Triangle, Ramp Up/Down, Square, Pulse, S&H, Hann), retrigger.
- **Envelopes (×3)** — A / D / S / R (multi-stage under the hood).
- **Mod Matrix** — add routes: **source → destination**, with a **depth** and a **transfer curve**
  (Linear / S / Exp / Log / 2-Step / 3-Step). Sources include the LFOs, envelopes, macros, the
  performance inputs (gate, pitch/key-follow, MPE timbre = CC74, pressure/aftertouch, velocity),
  a per-note random, and the live consonance value. Destinations are every synth parameter
  (spectrum stretches, centre-of-focus, amp tilt, phase spread, filter cutoff/reso, delay/reverb
  mix, volume, etc.). Bipolar sources (LFOs, pitch) swing ±depth; unipolar sources go 0→depth.

Example: add `lfo1 → centreFocus` (depth ~5) and watch the timbre — and the consonance plot —
move. Routes are saved with the preset/plugin state.

> Modulation is currently **global** (one modulated spectrum shared by all voices). Per-voice
> modulation is a planned next step (see `docs/modulation-architecture.md`). The engine is the full
> general model; an **expert** UI (multi-component formulas, operator precedence, ancillary ops,
> blend) is planned to sit alongside the basic view.

## Filter / FX tab

- **Filter** (per voice) — Off / Low Pass / High Pass / Band Pass / Notch, with cutoff + resonance.
- **Delay** — time, feedback, mix.
- **Reverb** — amount, size.

All of these are also modulation destinations (cutoff, reso, delay mix, reverb amount).

## Presets tab

- Ships with factory starters: **Bell Stretch, Bowed Pad, Macro Morph, Tremolo Keys**.
- **Save** the current sound (name it), **load**, or **delete**. Presets capture params + macros +
  modulation routes + LFO/envelope settings.
- Preset files live in the user data dir (`~/Library/PitchGrid/PseudoHarmonic/Presets` on macOS) as
  `*.phpreset` JSON. The full sound also round-trips through normal host state (DAW project save).

## Tuning (scalatrix / PitchGrid)

Note pitches and the partial spectrum are both defined by scalatrix temperaments:

- **MTS-ESP** — if an MTS master is present the synth tunes to it automatically (mode badge shows MTS).
- **MPE / MIDI** — per-note (MPE) or standard bend otherwise; configurable bend ranges.
- **PitchGrid OSC** — connects to the companion PitchGrid app (heartbeat on UDP 34562). It sends the
  synth its scale; the synth sends back its spectrum and per-node consonance for the grid.
- **Follow Tuning** — back-solves the six prime stretches so the pseudoharmonic partials land on the
  external scale's degrees.

## Tips

- Watch the **consonance plot** while editing the spectrum: it shows which intervals/temperaments the
  current timbre "wants."
- For evolving pads, use *continuous* excitation + an LFO on `centreFocus` or `ampTilt` + reverb.
- Macros are the quickest way to make one knob morph several parameters at once (see the *Macro Morph*
  factory preset).
