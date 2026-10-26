# Changelog

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
