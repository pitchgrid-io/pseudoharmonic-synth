# PseudoHarmonic Synth

An additive synthesizer that lets you continuously stretch the harmonic series per prime factor, turning harmonic timbres into *pseudoharmonic* ones — and revealing how consonance shifts along with them.

## What Is This?

In a standard harmonic sound, partials sit at exact integer multiples of the fundamental (1×, 2×, 3×, 4×, 5×, …). PseudoHarmonic Synth decouples these ratios by prime factor:

| Parameter | Controls | Default |
|-----------|----------|---------|
| **Stretch 2** | All even partials (2, 4, 6, 8, …) | 2.0 |
| **Stretch 3** | Partials with factor 3 (3, 6, 9, 12, …) | 3.0 |
| **Stretch 5** | Partials with factor 5 (5, 10, 15, 20, …) | 5.0 |
| **Stretch 7** | Partials with factor 7 (7, 14, 21, 28, …) | 7.0 |

Each partial's frequency ratio is computed from its prime factorization. For example, partial 12 = 2² × 3, so its ratio becomes `stretch2² × stretch3`. When all stretches are at their integer defaults, you get a standard harmonic spectrum. Deviate, and the timbre becomes *pseudoharmonic* — inharmonic, but in a structured, musically meaningful way.

## Real-Time Consonance Visualization

The synth computes and displays a **Plomp-Levelt consonance curve** in real time, showing how the stretched spectrum reshapes which intervals sound consonant. As you adjust the stretch parameters, you can *see* consonance peaks migrate — the tuning system that "fits" the timbre changes continuously.

This connects directly to [PitchGrid](https://pitchgrid.io): the consonance curve reveals which rank-2 temperaments and MOS scales are natural partners for a given pseudoharmonic spectrum.

## Architecture

- **DSP Engine** — 32-partial additive synthesis using complex-number oscillator rotation (no wavetables). Per-voice impact/decay model with strike position and odd/even controls.
- **Consonance Curve** — Plomp-Levelt roughness calculation with Hull₃ envelope extraction, yielding a spiky consonance function C(x) over 0–1200 cents.
- **Web UI** — Svelte frontend connected via WebSocket bridge, providing real-time parameter control and visualization.
- **MTS-ESP** — Supports dynamic microtuning via MTS-ESP client.

## Building

Requires CMake ≥ 3.15, Node/npm (for the web UI), and the git submodules (JUCE + scalatrix):

```bash
git submodule update --init --recursive
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

Builds Standalone, VST3, and AU targets. Release builds bundle the web UI into the plugin;
Debug builds load it from the vite dev server (see Development below).

## Development

For day-to-day work use the self-contained dev launcher:

```bash
./run_dev.sh
```

It starts the Svelte UI (vite) on an **ephemeral port**, builds the Debug Standalone, and
launches it — the app reads that port from `PH_DEV_UI_PORT` and loads the dev server, so
frontend changes hot-reload without rebuilding the plugin. Ctrl+C (or closing the app) stops
the dev server and frees the port.

The synth standalone also hosts a WebSocket server on an auto-assigned port; the UI discovers
it at runtime (via the `uiMounted` native function) and connects to visualize and control
parameters, modulation, and presets in real time.

To run the UI on its own (e.g. against a separately launched build):

```bash
cd ui && npm install && npm run dev
```

## Part of the PitchGrid Ecosystem

PseudoHarmonic Synth is a research tool exploring the relationship between timbre and tuning — a key question in the [PitchGrid](https://pitchgrid.io) project's mission to generalize Western tonal structure via rank-2 regular temperaments.

## License

© 2026 PitchGrid / Bayes GmbH
