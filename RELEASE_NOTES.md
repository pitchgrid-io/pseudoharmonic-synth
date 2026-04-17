# PseudoHarmonic v0.3.1

## What's New in v0.3.1

- **MTS-ESP client** — registers as an MTS-ESP client so note pitches can be driven by any connected MTS-ESP master. Scale-name changes propagate to the UI within ~33 ms.
- **Mode dropdown** — new MIDI / MPE / MTS selector next to the logo replaces the MPE toggle in the settings gear. MTS auto-selects when a master appears; MPE is the default fallback. Bend-range controls (Pitch Bend Range / Master Bend / Per-Note Bend) moved into the dropdown since they are mode-specific.
- **Audio thread safety** — parameter updates now land in a pending snapshot and swap into the engine at the top of `processBlock`, eliminating intermittent audio cracks from concurrent writes. Removed `std::vector` allocations on the audio thread.
- **Max voices 16 → 32** — reduces voice stealing under dense playing.
- **Smarter voice stealing** — picks the lowest-energy voice instead of round-robin.
- **Follow Tuning table** — shows signed deviation (not absolute); "Node X" renamed to "Note Pitch" in cents.
- **MIT LICENSE** added.
- **Build timestamp** regenerates on every build.
