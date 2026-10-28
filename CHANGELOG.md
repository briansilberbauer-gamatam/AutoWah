# Changelog

## 1.1.0 — 2026-10-28

### Added

- Input trim parameter (`input`, ‑24 – +24 dB) ahead of the detector and filter,
  wired into factory presets, host automation, state and the DSP test suite.
- Right-side full-height mixer bay housing the Input, Wet/Dry Mix and Output
  faders, with a horizontal stereo `OUT METER` (L/R bars, dB scale, peak hold and
  tabular peak readout) vertically below the faders.
- Mixer-bay separator hairline with a subtle crimson edge refraction.

### Changed

- Rebalanced the 1180x720 layout around the new full-height mixer bay, moving
  the filter-type selector up beside the filter-core header and tightening the
  LFO and footer sections.

## 1.0.0 — 2026-10-26

### Added

- Dual modulation sources with a click-free switch:
  - stereo-linked envelope detector with sensitivity, attack and release;
  - free-running LFO with sine, triangle, square and saw shapes, rate and depth.
- Analogue-modelled 12 dB/octave state-variable wah filter (bandpass/lowpass)
  with start frequency, 0–6 octave sweep and Q up to 14.
- Log-domain cutoff smoothing, resonance makeup and valve-like peak control.
- Mix, output, zero-latency bypass and 10 factory presets.
- Live central display: audio waveform, computed filter response, animated
  cutoff cursor and scrolling modulation history.
- Lock-free waveform/modulation telemetry and full VST3 state round-trip.
- Offline DSP test suite, VST3 host smoke test and headless 2x UI renderer.
- Cross-platform GitHub Actions build, test, packaging and release workflow.
