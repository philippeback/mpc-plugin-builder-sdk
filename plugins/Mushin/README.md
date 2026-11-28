# Mushin

MPC standalone port of the **Mushin** multi-stage audio effect plugin by PhilippeBack.

## Signal Flow

```text
Input ─▶ [Pre-Dist Noise] ─▶ S-Curve Waveshaper (bypassable) ─▶ [Pre-Filter Noise]
      ─▶ Dual Filter & LFO Matrix (Serial / Parallel, CLN/VOWL/ACID/DIGI)
      ─▶ Info Decay (Bit Depth / Downsample / Exh Link) ─▶ [Post-Filter Noise]
      ─▶ Dry/Wet Mix ─▶ 16-Step Trance Gate (128 Patterns + Custom Taps + Offset)
      ─▶ Sidechain Envelope Follower (Drive / Cutoff / Gain / A Reso) ─▶ Final Gain
      ─▶ Stereo / Comb Delay ─▶ Lookahead Limiter (CLN / LMC) ─▶ Output
```

- Sidechain envelope follower (`PEAK` / `RMS`, `Ratio` `1:1–20:1`, `HPF`/`LPF`, `INT` / `TEST` 4x4 pulse source) can simultaneously modulate any combination of S-Curve `Drive`, Filter A/B `Cutoff`, `Gain` (ducking), and Filter A `Reso`.

## UI Pages (9 Logical Screens on 6 MPC Function Keys)

- **MAIN (`F1`)**: 8 Performance Macro Orbs (`EXHAUSTION`, `CHOKE`, `INFO DECAY`, `CORRUPT MIX`, `KINETIC MOD`, `RESONATOR`, `GATE CARVE`, `SMASH`) mapped to Q-Link Banks 1–2 (`Q-Link 13..16`, `9..12`).
- **S-CURVE (`F2`)**: `S-Curve` (`Active`, `Drive`, `Threshold`, `Dry/Wet Mix`, `Exhaustion`, `Auto Gain`, `Gain`, `Filt->Sat` order toggle), `Info Decay` (`Active`, `Bit Depth`, `Downsample`, `Mix`, `Exh Link`) with a live `#FF4D8D` pixelated background strip, and `Auto In` (`Auto In`, `In Target`, `In Trim`) automatic input gain staging.
- **FILTER (`F3`)**: `Filter A` (Cyan) & `Filter B` (Magenta) (`Cutoff`, `Reso`, `Drive`, `Grit`, icon-baked `CLN`/`VOWL`/`ACID`/`DIGI` Type & `LP`/`HP`/`BP`/`NOTCH` Mode switches), `Filt Mix`, `Routing` (`SERIAL`/`PARAL`/`STEREO` L→A / R→B / `M/S` Mid→A / Side→B), and dual live `A FREQ X RES` / `B FREQ X RES` XY pads.
- **LFO (`F4` SubTab 1)**: `LFO 1 & 2` (`Freq` + 6-way icon `Wave` buttons including vintage **Drift**), live `LFO 1 X × LFO 2 Y` XY pad, `Noise Osc` (`Active`, 6-way icon `Type`, `Freq`, `Level`, icon-baked `Route`, `Filter FM`), and `Env Follower` (`Env Cut`, `Env Drive`).
- **LFO MTX (`F4` SubTab 2)**: 2×6 bipolar LFO modulation matrix targeting Filter A/B `Cutoff`, `Reso`, and `Grit`, plus Column 4 `Env Follower` matrix destinations (`Env A Res`, `Env B Cut`, `Env L1 Spd`, `Env LFO Amt`).
- **GATE (`F5` SubTab 1)**: `Trance Gate` (`Active`, 128-pattern `Pattern` menu, `Rate` `1/16`/`1/8`/`1/4`, `Depth`, `Mix`), 16 tappable `Step 1..16` toggle buttons, and `Gate Env` (`Start`, `Hold`, `End`, `Offset` `-100%..+100%` + live `AHDS` `Envelope` graph synced with `Depth`/`Floor`).
- **PUMP (`F5` SubTab 2)**: Sidechain Pumper (`Active`, `Mode` `PEAK`/`RMS`, `Ratio`, `Mix`, `Thresh`, `Amount`, `Attack`, `Release`, `HPF`, `LPF`, independent `SC Drive` / `SC Cut` / `SC Gain` / `SC Reso` target toggles, `SC Source` `INT`/`4x4`, and compact vertical `IN` cyan / `DUCK` magenta LED meters).
- **DLY+LIM (`F6` SubTab 1)**: `Delay` (`Active`, `Sync`, `Ping-Pong`, `Delay Color` Dub Tape ↔ Chorus Shimmer, `Feedback`, `Time`/`Tempo`, `Mix`) and `Limiter` (`Active`, `Drive`, `Release`, `Ceiling`, `Mode` `CLN`/`LMC`, `Mix`, and horizontal cyan-to-magenta `LIMITER GR` gain-reduction meter).
- **SCOPE (`F6` SubTab 2)**: SNES-sprite / Milkdrop-style real-time audio visualizer (~30 Hz telemetry) with a live **4-Theme Switcher** (**NEON**, **SOLAR**, **CYBER**, **VOID** via `Parameter 134`, persisting across factory preset changes), 24-frame tunnel/wireframe/caustics backdrop, 20-frame center core, twin Lissajous `XYPad` orbs, mirrored L/R energy wing meters, and full Q-Link live macro control (`EXHAUSTION`..`SMASH`, Filter A/B, LFO, Drive/Mix/Delay).

## Factory Presets (168 Programs)

Organised across 16 category groups for rapid MPC preset browsing:
- **Core (`6`)**, **PSY: (`15`)**, **SAT: (`10`)**, **FLT: (`10`)**, **VOWL: (`9`)**, **DECY: (`9`)**, **LFO: (`9`)**, **GATE: (`10`)**, **PUMP: (`8`)**, **COMBO & MUSH (`18`)**, **L/R: (`4`)**, **M/S: (`8`)**, **PAD: (`18`)**, **SYN: (`6`)**, **SWV: (`6`)**, **VOX: (`22`)**.
- Switching built-in presets calls `requestDisplayUpdate()` and notifies the host of parameters `0..133`, updating every knob, switch, step button, and Q-Link immediately while preserving your active `SCOPE` visual theme (`Parameter 134`).

## Notes & Licence

- Ported from `C:\Dev\github\philippeback\mushin` (CC BY-NC-ND 4.0).
- All DSP uses pure C++ and the MIT-licensed `mpc::dsp` SDK headers (no JUCE dependencies).
- Filter coefficients update at a 16-sample sub-block rate to keep ARM CPU usage low on standalone MPC hardware (~0.1% desktop / single-digit ARM CPU).
- On standalone MPC OS 3.10.1+, enable **Preferences → Enhancements (Beta) → Enable Enhancements** and **External Plugin Support**.
- See [MPC_PORT_NOTES.md](MPC_PORT_NOTES.md) and [doc/UI_AND_MACROS_GUIDE.md](doc/UI_AND_MACROS_GUIDE.md) for UI architecture and hardware notes.
