# Didje

Physical-modeling and vocal-tract formant-morphing didgeridoo synthesizer for Akai MPC standalone hardware (`MPC OS 3.10.1+`).

## Signal Path

1. **Lip-Reed Exciter:** Asymmetrical narrow pulse + saw oscillator modulated by `DriftLFO` (`±4` cents organic pitch drift and pulse-width drift), mixed with bandpass-filtered pink breath noise and a velocity-triggered overblow harmonic ("toot" at `~2.828x` fundamental), shaped through an asymmetric `fastTanh` lip-reed waveshaper.
2. **1D Waveguide Bore:** Pitch-tracked `DelayLine` (Dattorro allpass interpolation) with phase-delay compensation for accurate tuning across registers, `OnePoleLP` woody damping, `20 Hz` `OnePoleHP` DC blocker inside the feedback loop, and soft saturation.
3. **Vocal Tract Formant Bank:** 3-band parallel SVF formant resonator (`A → E → I → O → U`) swept smoothly by **Vowel Morph**, tempo-synced **Wobble LFO**, envelope follower, and MIDI Aftertouch / Mod Wheel (CC 1).
4. **Output Stage:** Stereo acoustic width dispersion, **Out Gain**, and a stereo-linked 2 ms lookahead `-0.5 dBFS` safety ceiling limiter.

## Controls (`DIDJE` Page — 12 Parameters)

- **Row 1 (Expression):** `Vowel Morph` (0–100%), `Breath Growl` (0–100%), `Wobble Rate` (`1/2..1/32` tempo-synced menu), `Wobble Depth` (0–100%)
- **Row 2 (Bore & Gain):** `Drone Tune` (`-24..+24 st`), `Bore Damp` (0–100%), `Toot / Overb` (0–100%), `Out Gain` (`-24..+6 dB`, Param 0)
- **Row 3 (Articulation):** `Glide` (`0..200 ms`), `Body Width` (0–100%), `Attack` (`2..250 ms`), `Release` (`20..450 ms`)

## MIDI Performance Tips

- **Monophonic Legato:** Overlap notes to glide smoothly across drone intervals via **Glide**.
- **Velocity:** Strikes the overblow **Toot** transient on note onset.
- **Aftertouch & Mod Wheel (CC 1):** Sweeps upward into open vowel formants and adds breath growl texture.
- Pair `Didje` downstream into **`Mushin`** for M/S filtering, `1/32` Trance Gate chops, and `3:4 X` / `DIFFUSE` delay.

## Built-in Programs

1. `Earth Drone`
2. `Aether Digiridood`
3. `Hollow Eucalyptus`
4. `Overblow Toot`
5. `Psy Acid Didge`

## Licence Notes

Built with the MIT-licensed MPC Plugin SDK framework and DSP helpers (`mpc_plugin.h`, `framework/dsp/*`).
