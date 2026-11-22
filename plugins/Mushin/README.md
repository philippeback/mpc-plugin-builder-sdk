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

## UI Pages (Half-Screen Zones)

- **S-CURVE**: `S-Curve` (Drive, Threshold, Mix, Gain, Exhaustion, Auto Gain) / `Info Decay` (Active, Bit Depth, Downsample, Mix, Exhaustion Link)
- **FILTER**: `Filter A` (Cutoff, Resonance, Drive, Grit, Type, Mode) / `Filter B` (+ Serial/Parallel Routing)
- **LFO MOD**: `LFO 1 & 2` (Freq, Wave) / `LFO 1 Matrix` (6 modulation targets)
- **MOD 2**: `LFO 2 Matrix` (6 modulation targets) / `Noise Osc` (Active, Type, Freq, Level, Routing, Filter FM)
- **SIDECHN**: `Sidechain` (Active, Source, Target, Mode, Threshold, Amount) / `SC Env & Filt` (Attack, Release, HPF, LPF)
- **GATE**: `Trance Gate` (Active, Pattern, Rate, Mix, Depth) / `Gate Env` (Start, Hold, End)
- **DLY+LIM**: `Delay` (Active, Sync, Ping-Pong, Time/Tempo, Feedback, Mix) / `Limiter` (Active, Mode, Drive, Ceiling, Release, Mix)

## Notes & Licence

- Ported from `C:\Dev\github\philippeback\mushin` (CC BY-NC-ND 4.0).
- All DSP uses pure C++ and the MIT-licensed `mpc::dsp` SDK headers (no JUCE dependencies).
- Filter coefficients update at a 16-sample sub-block rate to keep ARM CPU usage low on standalone MPC hardware.
