# Why Mushin Matters — Live Performance Guide

**Mushin** is an 8-stage signal corruption, dual-filter, rhythmic gating, sidechain pumping, and dynamics engine built natively for Akai MPC standalone hardware (ARM32 and ARM64, MPC OS 3.10.1+).

In Japanese martial arts and Zen philosophy, *mushin* ("no-mind") describes a state of fluid action without hesitation or overthinking. Mushin brings that same philosophy to live MPC performance: **full-chain sound design and dramatic transitions from a single insert slot, playable in real time with zero menu-diving.**

---

## 1. Why Mushin Matters on Standalone MPC

### One Insert Slot Instead of Eight
On standalone MPC hardware, tracks have a limited number of insert slots and a finite ARM CPU budget. Mushin unifies the entire performance chain inside one real-time-safe C++ plugin:
```text
in ─▶ [Pre-Dist Noise] ─▶ S-Curve Waveshaper ─▶ [Pre-Filter Noise]
   ─▶ Dual Filter (Clean/Vowel/Acid/Digital, Serial/Parallel) & LFO Matrix
   ─▶ Info Decay (Bit/Rate Crush) ─▶ [Post-Filter Noise] ─▶ Dry/Wet Mix
   ─▶ 16-Step Trance Gate (+Offset) ─▶ Sidechain Pumper ─▶ Gain
   ─▶ Stereo / Comb Delay ─▶ Lookahead Limiter (CLN/LMC) ─▶ out
```
Every stage is denormal-flushed, allocation-free on the audio thread, and optimized (including 16-sample sub-block SVF coefficient updates) for low ARM CPU usage.

### Macro Power Without Hidden State
Mushin's 8 performance macros on **MAIN** directly drive and reflect every underlying parameter across all detail pages (`S-CURVE`, `FILTER`, `LFO`, `LFO MTX`, `GATE`, `PUMP`, `DLY+LIM`) while preserving your pre-macro base values. Perform sweeping transitions from one screen, then switch to any detail page and fine-tune live controls without fighting hidden offsets.

---

## 2. The 8 Performance Macros (2 Q-Link Banks)

### Bank 1 — Core Destruction & Choke (Cyan · `Q-Link 13..16`)
| Macro | What It Does Live |
|---|---|
| **EXHAUSTION** | Engages S-Curve, pushes `Drive` (+12x), enables hard-clipping `Exhaustion` above 65%, and trims output `Gain` (-35%) to keep levels controlled while saturating. |
| **CHOKE** | Sweeps Filter A downward (-7 octaves) and boosts resonance (+0.50). In **Serial** routing, Filter B switches to High-Pass and sweeps upward (forming a bandpass choke); in **Parallel** routing, both sweep downward together. |
| **INFO DECAY** | Activates `Info Decay`, drops `Bit Depth` toward 2-bit, increases `Downsample` up to 12x, raises `Decay Mix`, and lights up the pixelated `#FF4D8D` decay row backdrop in proportion to crunch depth. |
| **CORRUPT MIX** | Drives global `Dry/Wet Mix` toward 100%, activates the `Noise Osc` pre-filter, and raises `Noise Level` (+24 dB) into the resonant filters. |

### Bank 2 — Motion, Space & Rhythm (Magenta · `Q-Link 9..12`)
| Macro | What It Does Live |
|---|---|
| **KINETIC MOD** | Accelerates `LFO 1` and `LFO 2` frequencies, opens their modulation depths into Filter A/B Cutoff & Resonance, and pulls filter cutoffs into the midrange with boosted resonance. |
| **RESONATOR** | Boosts Filter A/B `Grit` (+0.50), activates `Delay` at short metallic comb-filter times (32 ms down to 12 ms), and pushes `Feedback` toward 0.88. |
| **GATE CARVE** | Activates the tempo-synced 16-step `Trance Gate`, tightens `Hold` toward sharp staccato slices, and raises `Depth` and `Mix` toward 100%. |
| **SMASH** | Activates the final `Limiter`, switches to aggressive 20:1 `LMC` squash mode above 30%, adds up to +18 dB of `Drive`, and raises `Limit Mix` while driving the horizontal `LIMITER GR` meter. |

---

## 3. Live Performance Highlights

- **Vocal Formant Morphing (`VOWL`)**: Switch Filter A or B to the `VOWL` formant icon on **FILTER** (`F3`) to morph across *Ah / Eh / Ee / Oh / Oo* vocal formants as you sweep Cutoff with LFOs, Noise FM, or Sidechain modulation—with live cyan & magenta `XY` cutoff/reso pucks.
- **128 Rhythmic Gate Patterns + Touchscreen Step Taps + Kick-Safe Offset**: Browse 128 curated rhythms (House, Techno, DnB, Trap, Euclidean, Afro-Latin Clave, Glitch) or **tap any of the 16 step buttons directly on the MPC screen** on **GATE** (`F5`). Dial `Offset` (`-100%..+100%`) to shift the envelope phase so kick transients punch right through.
- **Dedicated Sidechain Pumper (`PUMP`, `F5` SubTab 2)**: Shape French-touch ducking (`Ratio` up to 20:1, `PEAK`/`RMS`, `HPF`/`LPF` key filters, `Test Pulse` source) targeting any combination of `SC Drive`, `SC Cut`, `SC Gain`, and `SC Reso` with real-time `IN` and `DUCK` LED meters.
- **96 Categorized Factory Programs**: Jump straight into inspiration across Core, `SAT:`, `FLT:`, `VOWL:`, `DECY:`, `LFO:`, `GATE:`, `PUMP:`, `COMBO:`, and `MUSH:` heavy-hitters—all controls, switches, step LEDs, and macros snap immediately when a preset loads.
