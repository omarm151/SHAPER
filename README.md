# Illuminize Shaper

Editable-curve waveshaper plugin (VST3 + AU) — the plugin version of the Illuminize Shaper
Max for Live device, in the spirit of FL Studio's Fruity WaveShaper and Melda MWaveShaper.

## Features
- Draw the transfer curve: drag points, bend segments, exact vertical steps, SYM on/off.
- Starter curves: Linear, Soft, Tape, Tube, Clip, Fold, Hard Fold, Steps, Rectify, Half Rectify, plus Random.
- Pre, Bias, Mix, Post; DC filter (off by default, like FL's Center); Auto gain.
- Oversampling 1x / 2x / 4x with linear-phase filters; click-free switching.
- Output peak limiter (ceiling, release, gain-reduction meter).
- Constant latency of 104 samples in every mode, reported to the host.
- Themes: Live Light, Live Dark, Neon Blue / Green / Red. Window scales up to 2.5x.
- AUDIO FORM audio-reactive display; large curve editor window.

The audio engine is a line-by-line port of the Max for Live device's gen~ code and is tested
against reference audio rendered from it (`tests/`).

## Download
Every push builds the plugin with GitHub Actions. Open the **Actions** tab, pick the latest
green "Build" run, and download **Illuminize-Shaper-macOS** at the bottom. Unzip it, then
see `INSTALL.txt` inside.

## Build locally (macOS)
Requires Xcode command-line tools and CMake 3.22+.

```
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_ARCHITECTURES=arm64
cmake --build build --config Release -j 8
ctest --test-dir build -C Release
```
Plugins end up in `build/IlluminizeShaper_artefacts/Release/`.

## Layout
- `source/dsp` — engine (plain C++, no JUCE): curve model, oversampling, limiter, auto-gain.
- `source/ui` — Ableton-style UI: curve editor, AUDIO FORM, knobs, themes.
- `tests` — engine tests + reference audio from the M4L device's code (`tools/makeref.py`).
- `tools` — UI screenshot tool and a VST3/AU host check used in CI.

## Licences
- JUCE 9 is used under its AGPLv3 / JUCE licence terms (see juce.com/legal). For a private,
  personal project the free JUCE licence tier applies; if you publish the source, AGPLv3 applies.
- Space Grotesk font: SIL Open Font License 1.1 (`assets/SpaceGrotesk-OFL.txt`).
