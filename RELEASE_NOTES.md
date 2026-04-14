# PseudoHarmonic v0.3.0

Pseudoharmonic additive synthesizer with real-time consonance visualization.

## Highlights

- **Follow Tuning** — auto-adjust prime partials to align with MOS scale degrees from PitchGrid
- **Interactive consonance curve** with ratio labels at peaks (e.g. 3:2, 5:4)
- **6 prime partial stretch controls** (2nd, 3rd, 5th, 7th, 11th, 13th) with ±3% deviation
- **OSC tuning sync** — receives tuning from PitchGrid Mapper for MOS scale integration
- **Embedded web UI** with real-time spectrum, consonance curve, and interval display
- **DAW integration** — 12 automatable parameters (VST3/AU), MPE support

## What's New in v0.3.0

- **Follow Tuning mode** — toggle in the header (when OSC is connected) that automatically adjusts all six prime stretch parameters so key just-intonation ratios snap to the nearest MOS scale degrees. Processes primes hierarchically with dependency tracking: if a prime can't be solved within range, downstream ratios depending on it are excluded.
- **Follow Tuning debug display** — info table below the consonance plot shows chosen ratio, matched scale degree, node position, deviation, and adjusted value for each prime
- **Ratio highlighting on consonance curve** — chosen Follow Tuning ratios are highlighted in blue (valid) or red (clamped/out of range)
- **MPE pressure control** — channel pressure now modulates sustain level and noise mix
- **Gradual noise ramp** — noise amplitude ramps smoothly instead of stepping
- **Sustain from zero** — sustain level starts at zero and builds with pressure
- **Generation 3 consonance** — updated scalatrix with Gen 3 consonance model alongside Gen 2
- **Cross-channel sustain pedal fix** — sustain CC now handled correctly across MIDI channels
- **Build timestamp** shown in settings panel

## Formats

- **macOS**: Standalone, VST3, AU (arm64 + x86_64)
- **Windows**: Standalone, VST3 (installer)
