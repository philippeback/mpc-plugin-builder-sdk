# Mushin — Feature Inventory & DSP Architecture Wrap-Up

Master technical summary of **Mushin** (`PhilippeBack - VST - Mushin`) at the current milestone (`136` parameters, `168` factory presets, `9` touchscreen tabs across `6` MPC Function Keys).

---

## 1. Build & Plugin Identity Snapshot

| Property | Value |
|---|---|
| **Plugin Name / Vendor** | `Mushin` by `PhilippeBack` |
| **VST2 UID / Kind / I/O** | `Msh2` · `Effect` · Stereo `2 in / 2 out` (`in-place` safe) |
| **Parameters** | **`136` total** (`122` automatable + `14` read-only UI/telemetry parameters) |
| **Factory Presets** | **`168` built-in programs** across `16` category banks (see §5) |
| **Touchscreen Layout** | **`9` logical tabs** mapped to **MPC Function Keys `F1`–`F6`** + `64` Q-Link bank slots |
| **Targets (`tools/build.py`)** | `native` (`mushin.dll`, `660 KB`), `arm32` (`mushin-arm32.so`, `360 KB`), `arm64` (`mushin-arm64.so`, `379 KB`) |
| **Host Simulator (`host_sim`)** | `20 passed, 0 warnings, 0 failures`, `3236`-byte state chunk round-trip across all `136` params, ~`0.16%` desktop CPU / block |
| **MPC OS Requirements** | **MPC OS 3.10.1+** with **Preferences → Enhancements (Beta) → Enable Enhancements** & **External Plugin Support** |

---

## 2. Full 11-Stage DSP Chain (`src/Mushin.cpp`)

```text
Input L/R ──▶ [Auto Input Stager (RMS Target + Auto Trim)]
          ──▶ [Pre-Dist Noise]
          ──▶ (Flt->Sat = Off) ──▶ [S-Curve Waveshaper] ──▶ [Dual Filter A/B + LFO Matrix]
          ──▶ (Flt->Sat = On)  ──▶ [Dual Filter A/B + LFO Matrix] ──▶ [S-Curve Waveshaper]
          ──▶ [Info Decay (2–16 Bit / 1–32x Downsample / Exh Link)]
          ──▶ [Post-Filter Noise]
          ──▶ [Dry/Wet Mix]
          ──▶ [16-Step Trance Gate (128 Masks + Touch Steps + Gate Offset + AHDS + Playhead)]
          ──▶ [Sidechain Pumper (INT / 4x4 Test -> Drive / Cut / Gain / A Reso)]
          ──▶ [Master Output Gain]
          ──▶ [Stereo / Comb / Ping-Pong / Color Delay]
          ──▶ [2 ms Lookahead Limiter (CLEAN / LMC)]
          ──▶ Output L/R + [~30 Hz Telemetry: IN/DUCK/GR/Decay/SCOPE (8 params)/Gate Playhead]
```

### Stage Highlights
1. **Input Gain Stager (`Auto In`, `In Target`, `In Trim`):** RMS level detector (`450 ms` smooth release) trimming hot/quiet stems toward `-20 dBFS`–`0 dBFS` target so saturation and limiter thresholds hit consistent sweet spots across tracks.
2. **S-Curve Saturation & Pre/Post Order (`S-Curve Active`, `Flt->Sat`):** Bypassable symmetric/asymmetric saturation with **Exhaustion** (`hard clipping + bias`), automatic loudness compensation (`Auto Gain`), and **`Flt->Sat`** order toggle (`Off` = Drive into Filter; `On` = Filter resonances into Drive).
3. **Dual Stereo Filter A (Cyan) & B (Magenta):**
   - **4 Filter Models:** `CLN` (SVF), `VOWL` (3-band morphing Vocal Formant), `ACID` (nonlinear 303-style feedback), `DIGI` (resonant crisp digital).
   - **4 Shapes:** `LP`, `HP`, `BP`, `NOTCH`, plus per-filter `Drive`, `Grit`, `Filt Mix`, and live `Freq × Res` **XY Pads**.
   - **4 Topologies (`Routing`):** `SERIAL` (A → B), `PARAL` (A + B), `STEREO` (L → A, R → B), and `M/S` (Mid → A, Side → B).
4. **LFO 1 & 2, Noise/Tone Oscillator & Modulation Matrix:**
   - Dual LFOs (`0.1–20 Hz`, `Sine/Tri/Saw/Square/Random`) paired on a full-width `LFO 1 × LFO 2` **XY Pad**.
   - 6-shape Noise/Tone Oscillator (`White`, `Pink`, `Sine`, `Tri`, `Saw`, `Sq`) insertable `PRE-D`, `PRE-F`, or `POST` with `Filter FM`.
   - **2×6 LFO Matrix** (`LFO 1` & `LFO 2` → Filter A/B `Cut`, `Res`, `Grit`) + **6 Envelope Follower destinations** (`Env Cut` & `Env Drive` on `LFO`; `Env A Res`, `Env B Cut`, `Env L1 Spd`, `Env LFO Amt` on `LFO MTX`).
5. **Info Decay:** Continuous `2.0–16.0` bit quantization + `1–32x` zero-order-hold sample-rate decimation + `Exh Link` dynamic bit/rate modulation, driving the `#FF4D8D` `Decay Viz` (`Parameter 97`) pixel bay.
6. **16-Step Trance Gate (`GATE`):**
   - **128 Factory Rhythm Masks** (`kGateMasks[128]`), updating all **16 tappable `Step 1..16` buttons** live on preset or pattern change.
   - `1/16`, `1/8`, `1/4` clock rates locked to host PPQ or free-running BPM; bipolar **`Gate Offset`** (`-100%..+100%` per step) to shift around kick transients.
   - **AHDS Shaper** (`Start`, `Hold`, `End`, `Depth` ↔ `Gate Floor`) + **Live Step Playhead (`Gate Step`, `Parameter 135`)** highlighting the active step (`0..15`, dark frame `16` on bypass).
7. **Sidechain Pumper (`PUMP`):**
   - `PEAK`/`RMS` detector, `Ratio` (`1:1–20:1`), `HPF`/`LPF` key filters, internal audio (`INT`) or self-clocked **`4x4` Test Pulse**.
   - Four independent simultaneous targets: `SC Drive`, `SC Cut` (`±6 octaves`), `SC Gain` (ducking), and `SC Reso` (Filter A resonance), plus live vertical `IN` (cyan) & `DUCK` (magenta) meters.
8. **Color Ping-Pong Delay & Lookahead Limiter (`DLY+LIM`):**
   - `1–2000 ms` free or BPM-synced (`1/16..1/2`) Stereo / `Ping-Pong` Delay (down to tight metallic comb-resonators) with **`Delay Color`** (`-1.00` saturated Dub Tape ↔ `+1.00` shimmering Chorus modulation).
   - `2 ms` lookahead Limiter (`CLEAN` brickwall / `LMC` 20:1 aggressive squash, `Drive`, `Ceiling`, `Release`, parallel `Mix`, horizontal `LIMITER GR` meter).
9. **`SCOPE` Visual Synthesizer (`F6` SubTab 2):**
   - Real-time ~30 Hz audio telemetry (`Viz Phase`, `Viz Pulse`, `Viz X1/Y1`, `Viz X2/Y2`, `Viz Left/Right`) driving 4 complete switchable themes (**NEON**, **SOLAR**, **CYBER**, **VOID** via **`Theme` `Parameter 134`**), while keeping all 16 hardware Q-Links live for macro/filter/delay stage control.

---

## 3. Macro Engine, Chunk Recall & Theme Persistence

- **`baseValues_[kNumParams]` Separation:** Non-macro parameters store their underlying preset value in `baseValues_[]`. Turning any of the 8 macros (`EXHAUSTION`..`SMASH`) writes computed deltas into the target stage parameters via `applyMacros()` without erasing `baseValues_[]`, so returning macros to `0.00` restores the patch cleanly.
- **Bidirectional Parameter Sync (`parameterChanged`):**
  - Moving `Gate Depth` updates `Gate Floor` (`1.0 - Depth/100`), and dragging `Gate Floor` in the AHDS graph updates `Gate Depth`.
  - Selecting any of the 128 `Pattern` entries syncs all 16 `Step 1..16` parameters immediately.
- **Theme Persistence Across Presets (`kVizTheme`, Parameter `134`):**
  - `addPreset()` builds `plainValues` of length `kVizTheme` (`134`, indices `0..133`), and the program-change notification loop in `process()` stops at `i < kVizTheme`.
  - Switching any of the `168` factory presets updates audio controls without overriding your chosen **SCOPE** theme (`NEON`/`SOLAR`/`CYBER`/`VOID`), while full `effGetChunk`/`effSetChunk` state chunks still save and recall all `136` parameters (`MSHB` extra state) so projects reopen with their active theme.

---

## 4. Complete Parameter Index (`0..135`)

| Indices | Module / Section | Parameters (`id` / Type) |
|---|---|---|
| `0..5` | **S-Curve Master** | `0` Gain, `1` Drive, `2` Exhaustion (bool), `3` Threshold, `4` Auto Gain (bool), `5` Mix |
| `6..18` | **Dual Filters** | `6..11` Filter A (Cutoff, Reso, Drive, Grit, Type `CLN/VOWL/ACID/DIGI`, Mode `LP/HP/BP/NOTCH`), `12..17` Filter B, `18` Routing `SERIAL/PARAL/STEREO/M/S` |
| `19..34` | **LFOs & Matrix** | `19..22` LFO 1 & 2 Freq/Wave (`Sine/Tri/Saw/Sq/Rnd`), `23..28` LFO 1 → A/B (Cut/Res/Grit), `29..34` LFO 2 → A/B (Cut/Res/Grit) |
| `35..44` | **Sidechain Core** | `35` SC Active, `36` SC Source (`INT/4x4`), `37` Thresh, `38` Attack, `39` Release, `40` Mode (`PEAK/RMS`), `41` Amount, `42` Legacy Target, `43` HPF, `44` LPF |
| `45..50` | **Noise / Tone Osc** | `45` Active, `46` Type (`White/Pink/Sine/Tri/Saw/Sq`), `47` Freq, `48` Level, `49` Route (`PRE-D/PRE-F/POST`), `50` Filter FM |
| `51..58` | **Trance Gate Core** | `51` Active, `52` Mix, `53` Pattern (`0..127`), `54` Rate (`1/16,1/8,1/4`), `55` Start, `56` Hold, `57` End, `58` Depth |
| `59..63` | **Info Decay** | `59` Active, `60` Bit Depth, `61` Downsample, `62` Mix, `63` Exh Link |
| `64..76` | **Delay & Limiter** | `64..70` Delay (Active, Time, Feedback, Mix, Ping-Pong, Sync, Tempo), `71..76` Limiter (Active, Mode `CLEAN/LMC`, Drive, Ceiling, Release, Mix) |
| `77..84` | **MAIN Macros (`F1`)** | `77` EXHAUSTION, `78` CHOKE, `79` INFO DECAY, `80` CORRUPT MIX, `81` KINETIC MOD, `82` RESONATOR, `83` GATE CARVE, `84` SMASH |
| `85..97` | **Mix, Meter & Pump Extras** | `85` Filt Mix, `86` Gate Floor *(RO)*, `87` SC Mix, `88` SC In *(RO)*, `89` SC Duck *(RO)*, `90` SC Ratio, `91..94` SC Drive/Cut/Gain/Reso, `95` Lim GR *(RO)*, `96` S-Curve Active, `97` Decay Viz *(RO)* |
| `98..114` | **Gate Steps & Offset** | `98..113` Step 1..16 (bool), `114` Gate Offset (`-100..+100%`) |
| `115..125` | **Stage II Expansions** | `115` Auto In, `116` In Target, `117` In Trim, `118` Env Cut, `119` Env Drive, `120` Flt->Sat, `121..124` Env A Res / B Cut / L1 Spd / LFO Amt, `125` Delay Color |
| `126..135` | **SCOPE & Gate Playhead** | `126..133` Viz Phase/Pulse/X1/Y1/X2/Y2/Left/Right *(RO)*, `134` Theme (`NEON/SOLAR/CYBER/VOID`), `135` Gate Step *(RO playhead)* |

*(All `14` read-only telemetry parameters — `86, 88, 89, 95, 97, 126..133, 135` — have `automatable = false`.)*

---

## 5. Factory Preset Directory (`168` Programs)

| Bank Prefix | Count | Indices | Focus |
|---|---:|---|---|
| **Core** | 6 | `0–5` | Transparent `Init`, baseline saturation, resonator, decay, gate, pump |
| **PSY:** | 15 | `6–20` | Psychedelic trance leads, 303 squelches, Euclidean/magic gates, neuro chops |
| **SAT:** | 10 | `21–30` | S-Curve & Exhaustion saturation curves from tape glue to fuzz walls |
| **FLT:** | 10 | `31–40` | Acid/resonant/parallel/stereo filter sweeps & notches |
| **VOWL:** | 9 | `41–49` | 3-band morphing Vocal Formant filters (`VOWL`) |
| **DECY:** | 9 | `50–58` | Bitcrushing, sample-rate decimation & CRT noise textures |
| **LFO:** | 9 | `59–67` | Filter FM sirens, stepped random arps & multi-LFO drifts |
| **GATE:** | 10 | `68–77` | Rhythmic masks, Kick Safe (`+35%` Offset), Euclid 7, DnB/Trap/Latin patterns |
| **PUMP:** | 8 | `78–85` | `INT` & `4x4` sidechain ducking, filter breathing & drive surge |
| **COMBO & MUSH:** | 18 | `86–103` | Full-chain flagships (`MUSH: Full Chaos`, `Everything On`) & genre rigs |
| **L/R:** | 4 | `104–107` | Dual-channel independent Left/Right (`STEREO`) FX presets |
| **M/S:** | 8 | `108–115` | Mid/Side (`M/S`) mastering/widening & dub-center textures |
| **PAD:** | 18 | `116–133` | Cinematic pads, CS-80/Juno/VHS/D-50/Burial atmospheric beds |
| **SYN:** | 6 | `134–139` | Plucks, supersaws, stutters & polyrhythmic synth enhancers |
| **SWV:** | 6 | `140–145` | Synthwave/outrun chorused leads, dream keys & CRT bass |
| **VOX:** | 22 | `146–167` | Vocal polish, megaphone, dub throws, formants & chaotic adlib chains |
