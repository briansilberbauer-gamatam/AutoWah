# Auto-Wah — DeepSeek Harness (`dsh`) Workspace Instructions

## 1. Mandatory Skill Activation on Turn 1
Before planning, scaffolding, writing C++ DSP/UI code, or running builds in this repository, **immediately load the project skill** using the `skill` tool:
```json
{ "name": "vst3-80s-plugin" }
```
Then read its companion reference files using the `read` tool as needed:
- `.dsh/skills/vst3-80s-plugin/references/look-and-feel-recipes.md` — JUCE 8 `Graphics` code recipes for the Sleek Futuristic Studio Hardware (Brushed Gunmetal, Flush Glass & Muted Crimson) look & feel.
- `.dsh/skills/vst3-80s-plugin/references/github-actions-ci-release.md` — Cross-platform GitHub Actions workflow (`.github/workflows/build.yml`) for Windows x64, Linux x64, macOS Universal (`arm64` + `x86_64`), and automated GitHub Release publishing.

---

## 2. Core Project & Look-and-Feel Mandates
- **Product**: **Auto-Wah**, a dual-source dynamic wah filter whose signature feature is a click-free switch between an **envelope detector** and an **LFO** as the filter's control source.
- **Plugin Format & Framework**: JUCE 8.0.8 (`VST3` + `Standalone`), C++17, CMake (`CMAKE_SKIP_RPATH TRUE`).
- **Offline Dependencies**: Reuse `/home/brians/bls/sound/plugin_poc/external/JUCE` and `/home/brians/bls/sound/plugin_poc/deps/sysroot` (copy or symlink into this workspace so everything builds offline inside `workspace-write` sandbox permissions).
- **Sleek Futuristic Studio Hardware Look & Feel (Gunmetal, Flush Glass & Muted Crimson)**:
  - **Style & Form Factor**: Sleek, futuristic desktop studio hardware unit sitting on a dark studio surface (`#12151C` -> `#07080B`) with dimensional three-quarter/chamfered presence and upper-left soft-box studio lighting.
  - **Base & Glass Overlay**: Dark gunmetal gray brushed metal (`#1D2026`) with fine horizontal grain and smoothly chamfered matte edges, topped by a thick, flush-mounted transparent glass panel with **zero visible screws or fasteners** and a subtle, low-profile **crimson red LED edge-glow (`#C82230`)** that casts faint crimson refraction inward.
  - **Displays & Visualization**:
    - **Top Header Display**: Slender, pill-shaped obsidian digital readout (`#090A0D`) at the top center displaying preset info framed by minimalist `<` `>` chevron buttons.
    - **Main Visualizer**: Large central obsidian screen embedded under the glass with frequency/dB scale markers, a glowing crimson real-time audio waveform, the computed resonant filter-response curve, an animated cutoff cursor and a scrolling modulation-source history.
  - **Controls**:
    - **Low-Profile Knobs**: Large, heavy-machined metal primary knobs with fine vertical knurled ridges on the outer rim, recessed low-profile height, subtle crimson LED under-glow, and a tiny minimalist tick mark on the flat brushed cap.
    - **Low-Profile Sliders**: Vertical touch-style / ultra-thin mechanical sliders integrated smoothly into the glass surface with subtly recessed linear tracks and minimalist tick marks alongside them.
    - **Tactile Buttons**: Rectangular, low-profile tactile buttons (`"ENV"`, `"LFO"`, `"BP"`, `"LP"`, `"BYPASS"`, LFO shape selectors) with crisp sans-serif typography, outlined by a thin, understated red glowing border when active. The inactive modulation section is dimmed but still editable.

---

## 3. DSP & Architecture Invariants
- **Two control sources**: a stereo-linked envelope detector (sensitivity, attack, release) and a free-running LFO (rate, depth, sine/triangle/square/saw). Both modulate the same analogue-modelled TPT state-variable filter in log-frequency space; the mode switch must never click and the plugin must stay zero-latency.
- **Filter core**: bandpass or lowpass 12 dB/octave SVF with start frequency, 0–6 octave sweep and Q up to 14, log-domain cutoff smoothing, resonance makeup and gentle wet-path peak control.
- **Real-time safety**: no allocations, locks, file access or message-thread calls in `processBlock()`; telemetry travels through atomics and a lock-free waveform FIFO. The host's native bypass must drive the `bypass` parameter via `getBypassParameter()`.
- **State**: APVTS parameter state plus factory-preset identity/dirty state must round-trip through VST3. Ten deterministic factory presets must cover both control sources.

---

## 4. DeepSeek V4 Pro & Flash Workflow in `dsh`
- **DeepSeek V4 Pro (`deepseek-pro` / `deepseek-reasoner`)**: Use for DSP math (envelope ballistics, LFO shapes, TPT SVF coefficients, log-domain smoothing, resonance makeup, zero-latency bypass, lock-free telemetry) and custom procedural `LookAndFeel` / `Graphics` rendering.
- **DeepSeek V4 Flash (`deepseek-flash`)**: Use for CMake scaffolding, APVTS parameter definitions, presets, unit/host test harnesses, and build execution.
- **Verification**: Every build must run:
  1. `./scripts/build.sh`
  2. `./build/AutoWahTests_artefacts/Release/AutoWahTests` (offline DSP tests)
  3. `./build/AutoWahHostSmokeTest_artefacts/Release/AutoWahHostSmokeTest dist/Auto-Wah.vst3` (VST3 host scan, control-source switch & state round-trip)
  4. `./build/AutoWahScreenshot_artefacts/Release/Auto-Wah\ Screenshot --out=dist/autowah-ui.png` (use `xvfb-run -a` if headless; an active `DISPLAY` also works), then **`read_image`** to verify the gunmetal/glass/crimson aesthetic, live waveform, response curve and clipping, and **`present`** to attach the UI preview card in the DeepSeek Harness Web UI. Render a second LFO-mode preview with `--preset=5` for verification of the control-source switch.
