# PseudoHarmonic v0.2.1

Pseudoharmonic additive synthesizer with real-time consonance visualization.

## Highlights

- **Interactive consonance curve** with ratio labels at peaks (e.g. 3:2, 5:4)
- **6 prime partial stretch controls** (2nd, 3rd, 5th, 7th, 11th, 13th) with ±3% deviation
- **OSC tuning sync** — receives tuning from PitchGrid Mapper for MOS scale integration
- **Embedded web UI** with real-time spectrum, consonance curve, and interval display
- **DAW integration** — 12 automatable parameters (VST3/AU), MPE support

## What's New

- Added 11th and 13th prime partial stretch controls
- Interactive ratio labels on consonance curve peaks (hover or always-on)
- OSC-based spectrum and consonance data messaging
- Equave-reduced interval display when OSC tuning is connected
- Improved sustain behavior independent of strike amplitude
- Fixed voice allocation regression with sustained notes
- Fixed interval line persistence after note release

## Formats

- **macOS**: Standalone, VST3, AU (arm64 + x86_64)
- **Windows**: Standalone, VST3 (installer)
