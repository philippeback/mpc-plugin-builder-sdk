# Why Mushin Matters — Live Performance Guide

**Mushin** is a 7-stage signal corruption, filtering, rhythmic gating, and dynamics engine built natively for Akai MPC standalone hardware (ARM32 and ARM64, MPC OS 3.10.1+).

In Japanese martial arts and Zen philosophy, *mushin* ("no-mind") describes a state of fluid action without hesitation or overthinking. Mushin brings that same philosophy to live MPC performance: **full-chain sound design and dramatic transitions from a single insert slot, playable in real time with zero menu-diving.**

---

## 1. Why Mushin Matters on Standalone MPC

### One Insert Slot Instead of Seven
On standalone MPC hardware, tracks have a limited number of insert slots and a finite ARM CPU budget. Building a comparable performance chain out of separate stock plugins (saturation + dual filter + LFOs + bitcrusher + noise generator + trance gate + delay + limiter) consumes multiple insert slots, scatters controls across separate plugin windows, and makes coordinated multi-parameter gestures nearly impossible live.

Mushin unifies the entire chain in one real-time-safe C++ plugin:
```
in ─▶ [Pre-Dist Noise] ─▶ S-Curve Waveshaper ─▶ [Pre-Filter Noise]
   ─▶ Dual Filter & LFO Matrix (Serial / Parallel) ─▶ Info Decay (Bit / Sample Rate)
   ─▶ [Post-Filter Noise] ─▶ Dry/Wet Mix ─▶ 16-Step Trance Gate ─▶ Gain
   ─▶ Stereo / Comb Delay ─▶ Lookahead Limiter (Clean / LMC) ─▶ out
```
Every stage is denormal-flushed, allocation-free on the audio thread, and optimized (including 16-sample sub-block SVF coefficient updates) for low ARM CPU usage.

### Macro Power Without Hidden State
Most multi-effect macro systems apply invisible DSP offsets behind the scenes—so when you switch to a detail page, the knobs don't match what you hear.

Mushin's 8 performance macros **directly drive and reflect the underlying parameters** across every detail tab (`S-CURVE`, `FILTER`, `LFO`, `LFO MTX`, `GATE`, `DLY+LIM`) while tracking your pre-macro base values:
- Perform a sweeping transition on `MAIN` with the `180×180` interactive orb-knobs or hardware Q-Links.
- Switch to `FILTER`, `LFO`, `GATE`, or `DLY+LIM` and see every affected control at its exact live value.
- Fine-tune any individual control on the detail page naturally—your edit takes effect immediately and is preserved when you dial the macro back down.

---

## 2. The 8 Performance Macros (2 Q-Link Banks)

At `0%`, every macro leaves your underlying settings untouched. Turning a macro up orchestrates multiple DSP stages simultaneously:

### Bank 1 — Core Destruction & Choke (Cyan · `Q-Link 13..16`)
| Macro | What It Does Live |
|---|---|
| **EXHAUSTION** | Pushes S-Curve `Drive` (+12x), engages hard-clipping `Exhaustion` above 65%, and trims output `Gain` (-35%) to keep levels controlled while the signal saturates. |
| **CHOKE** | Sweeps Filter A downward (-7 octaves) and boosts resonance (+0.50). In **Serial** routing, Filter B switches to High-Pass and sweeps upward (forming a tight bandpass choke); in **Parallel** routing, Filter B stays Low-Pass and sweeps downward alongside Filter A so both branches choke together. |
| **INFO DECAY** | Activates `Info Decay`, drops `Bit Depth` toward 2-bit, increases `Downsample` up to 12x, and brings up `Decay Mix` for crisp digital crunch. |
| **CORRUPT MIX** | Drives global `Dry/Wet Mix` toward 100%, activates the `Noise Osc` pre-filter, and raises `Noise Level` (+24 dB) to inject grit directly into the resonant filters. |

### Bank 2 — Motion, Space & Rhythm (Magenta · `Q-Link 9..12`)
| Macro | What It Does Live |
|---|---|
| **KINETIC MOD** | Accelerates `LFO 1` and `LFO 2` frequencies, opens their modulation matrix depths into Filter A/B Cutoff & Resonance, and pulls filter cutoffs into the midrange with boosted resonance for instant animated wobble. |
| **RESONATOR** | Boosts Filter A/B `Grit` (+0.50), activates `Delay` at short metallic comb-filter times (32 ms down to 12 ms), and pushes `Feedback` toward 0.88 for tuned, ringing resonance. |
| **GATE CARVE** | Activates the tempo-synced 16-step `Trance Gate`, tightens `Hold` toward sharp staccato slices, and raises `Depth` and `Mix` to 100% for rhythmic chopping. |
| **SMASH** | Activates the final `Limiter`, switches to aggressive 20:1 `LMC` squash mode above 30%, adds up to +18 dB of `Drive`, and raises `Limit Mix` for instant drum-bus pump and glue. |

---

## 3. What Mushin Can Do in a Live Set

### Build-Ups, Tension & Drops
- **Filter-choke riser**: Sweep **CHOKE** up over 4–8 bars to narrow a full drum bus or synth group into a resonant midrange band, then add **RESONATOR** for metallic comb tension before snapping both back to `0%` on the drop.
- **Bus slam on impact**: Bring **SMASH** in on a chorus or drop to push drums and bass into 20:1 LMC limiting without leaving the `MAIN` screen.

### Rhythmic Breakdowns Without Track Mutes
- **Instant tempo-synced chops**: Turn up **GATE CARVE** on pads, vocals, or full stems to carve 16-step rhythmic patterns locked to MPC transport BPM.
- **Tactile pattern & shape tweaks**: Jump to `GATE` (F5) to tap between 8 rhythmic patterns, switch `1/16` / `1/8` / `1/4` rate with icon buttons, or drag the graphical `Start` / `Hold` / `End` AHDS envelope.

### Lo-Fi Collapse & Industrial Texture
- **Bit & rate degradation**: Sweep **INFO DECAY** for instant 12-bit sampler grit down to 2-bit aliasing, or link bit-crushing to S-Curve saturation via `Exh Link` on `S-CURVE` (F2).
- **Pre-filter noise injection**: Use **CORRUPT MIX**—or visit `LFO` (F4) to pick **White**, **Pink**, **Sine**, **Triangle**, **Saw**, or **Square** noise/tone shapes and route them `PRE-D`, `PRE-F`, or `POST`—to turn clean hits into textured percussion.

### Evolving Acid & Dub Motion
- **One-knob LFO animation**: Turn **KINETIC MOD** to wake up dual-LFO filter sweeps, then switch to `LFO` (F4) to tap waveform icons (**Sine**, **Triangle**, **Saw**, **Square**, **Random** S&H) or `LFO MTX` (F4 sub-tab) for 2×6 bipolar modulation routing.
- **Clean/Vintage/Acid/Digital character**: On `FILTER` (F3), switch Filter A & B characters and `SERIAL` / `PARAL` routing with baked icon buttons for everything from warm tape low-pass to screaming 303-style acid resonance.

### 6 Instant Starting Points (Built-In Programs)
- **Init**: Transparent pass-through starting point; all macros at `0%`.
- **Warm Tape Sat**: Subtle S-Curve saturation, vintage low-pass warmth, and slap delay.
- **Dual Resonator**: Parallel Acid/Clean filters swept in opposite directions by LFO 1.
- **Crushed Velvet**: Exhausted saturation into 6-bit / 4x downsampled Info Decay.
- **Rhythmic Gater**: 4-on-the-floor Trance Gate feeding tempo-synced ping-pong delay.
- **Mushin Pumper**: Driven saturation into LMC squash limiting for heavy drum-bus glue.
