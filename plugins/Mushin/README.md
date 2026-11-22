# Mushin

MPC standalone port of the **Mushin** multi-stage audio effect plugin by PhilippeBack.

## Signal Flow

```
Input -> [Pre-Dist Noise] -> S-Curve Waveshaper -> [Pre-Filter Noise]
      -> Dual Filter & LFO Matrix (Serial / Parallel) -> Info Decay (Bit/Rate)
      -> [Post-Filter Noise] -> Dry/Wet Mix -> Trance Gate -> Final Gain
      -> Stereo Delay -> Final Limiter -> Output
```

Sidechain envelope follower (Internal or Test Signal; External falls back to Internal on 2-in MPC effects) can modulate S-Curve Drive, Filter Cutoff, or Final Gain.

## UI Pages

- **MAIN**: 8 Performance Macro Orbs (`EXHAUSTION`, `CHOKE`, `INFO DECAY`, `CORRUPT MIX`, `KINETIC MOD`, `RESONATOR`, `GATE CARVE`, `SMASH`) mapped to Q-Link Banks 1–2 (`Q-Link 13..16`, `9..12`).
- **S-CURVE**: `S-Curve` (Drive, Threshold, Gain, Mix, Exhaustion, Auto Gain, Exh Link) / `Info Decay` (Active, Bit Depth, Downsample, Mix).
- **FILTER**: `Filter A` & `Filter B` (Cutoff, Reso, Drive, Grit, icon-baked Type & Mode switches) + isolated `Routing` (`Serial` / `Parallel`).
- **LFO (SubTab 1)**: `LFO 1 & 2` (Freq & Wave on one 4-Q-Link row) / `Noise Osc` (Active, Type, Freq, Level, icon-baked Route, Filter FM).
- **LFO MTX (LFO SubTab 2)**: `LFO 1 Matrix` & `LFO 2 Matrix` (12 bipolar modulation targets for Filter A/B Cutoff, Reso, Grit).
- **GATE**: `Trance Gate` (Active, Pattern menu, Rate, Depth, Mix, 8-frame 16-step pattern strip) / `Gate Env` (`Start`, `Hold`, `End` faders + `AHDS` `Envelope` visual).
- **DLY+LIM**: `Delay` (Active, Sync, Ping-Pong, Feedback, Time / Tempo, Mix) / `Limiter` (Active, Drive, Release, Ceiling, Mode `CLN`/`LMC`, Mix).

*(Sidechain parameters `35..44` remain intact and automatable, omitted from the touch UI.)*

## Notes & Licence

- Ported from `C:\Dev\github\philippeback\mushin` (CC BY-NC-ND 4.0).
- All DSP uses pure C++ and the MIT-licensed `mpc::dsp` SDK headers (no JUCE dependencies).
- Filter coefficients update at a 16-sample sub-block rate to keep ARM CPU usage low on standalone MPC hardware.
- See [MPC_PORT_NOTES.md](MPC_PORT_NOTES.md) for MPC hardware UI/TUI lessons learned and workarounds.
