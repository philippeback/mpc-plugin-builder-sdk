# Mushin — User Guide

**Product:** Mushin Multi-Stage Dynamic Distortion, Filter, Rhythm & Visualizer Effect  
**Document Type:** User Guide  
**Manual Version:** 1.0  
**Plugin Version:** 1.0.0 (`uid`: `Msh2`, VST2 Effect)  
**Manufacturer:** PhilippeBack  
**Language:** English  
**License:** CC-BY-NC-ND-4.0  

---

## Table of Contents

- [Introduction](#introduction)
  - [Overview & Features](#overview--features)
  - [System Requirements & Support](#system-requirements--support)
- [Setup & Installation](#setup--installation)
  - [Manual Drive Installation](#manual-drive-installation)
  - [Network Deployment (Developer Mode)](#network-deployment-developer-mode)
  - [Enabling External Plugins on MPC](#enabling-external-plugins-on-mpc)
- [Conventions Used in This Guide](#conventions-used-in-this-guide)
- [User Interface & Global Operation](#user-interface--global-operation)
  - [Loading & Preset Management](#loading--preset-management)
  - [MPC Screen Tabs & Hardware Mapping](#mpc-screen-tabs--hardware-mapping)
  - [Program Mode vs. Screen Mode Q-Links](#program-mode-vs-screen-mode-q-links)
- [Signal Flow & Processing Modules](#signal-flow--processing-modules)
  - [Signal Flow Overview](#signal-flow-overview)
  - [MAIN: Performance Macro Orbs (`F1`)](#main-performance-macro-orbs-f1)
  - [SATURATION: Input Stage, S-Curve & Info Decay (`F2`)](#saturation-input-stage-s-curve--info-decay-f2)
  - [FILTER: Dual Filters A & B, XY Telemetry & Routing (`F3`)](#filter-dual-filters-a--b-xy-telemetry--routing-f3)
  - [LFO: Dual Oscillators, Noise Generator & Envelope Follower (`F4` SubTab 1)](#lfo-dual-oscillators-noise-generator--envelope-follower-f4-subtab-1)
  - [LFO MTX: 2×6 Modulation Matrix & Dynamic Envelope Targets (`F4` SubTab 2)](#lfo-mtx-26-modulation-matrix--dynamic-envelope-targets-f4-subtab-2)
  - [GATE: 16-Step Trance Gate & AHDS Envelope Shaper (`F5` SubTab 1)](#gate-16-step-trance-gate--ahds-envelope-shaper-f5-subtab-1)
  - [PUMP: Sidechain Pumper & Detector Filter (`F5` SubTab 2)](#pump-sidechain-pumper--detector-filter-f5-subtab-2)
  - [DLY+LIM: Color Delay & Lookahead Limiter (`F6` SubTab 1)](#dlylim-color-delay--lookahead-limiter-f6-subtab-1)
  - [SCOPE: Real-Time Visual Synthesizer (`F6` SubTab 2)](#scope-real-time-visual-synthesizer-f6-subtab-2)
- ["How-To" Workflows](#how-to-workflows)
  - [Performing with the 8 Macro Orbs Without Losing Patch Balance](#performing-with-the-8-macro-orbs-without-losing-patch-balance)
  - [Creating an Instant 4×4 Pumping Groove Without External Routing](#creating-an-instant-44-pumping-groove-without-external-routing)
  - [Customizing Rhythmic Gates with Steps, Patterns & Phase Offset](#customizing-rhythmic-gates-with-steps-patterns--phase-offset)
  - [Sculpting Stereo Width with M/S Filtering & Delay Color](#sculpting-stereo-width-with-ms-filtering--delay-color)
- [Appendix](#appendix)
  - [Appendix A: Complete Q-Link Bank Mapping](#appendix-a-complete-q-link-bank-mapping)
  - [Appendix B: Factory Preset Directory (168 Programs)](#appendix-b-factory-preset-directory-168-programs)
  - [Appendix C: Troubleshooting](#appendix-c-troubleshooting)
- [Trademarks and Licenses](#trademarks-and-licenses)

---

## Introduction

Thank you for installing **Mushin** by **PhilippeBack**. Mushin is a multi-stage stereo audio destruction, dynamic filtering, rhythmic gating, ducking, and spatial-finishing effect designed natively for Akai Pro MPC standalone hardware and touchscreen Q-Link control.

### Overview & Features

- **136 Parameters (122 Automatable + 14 Read-Only Telemetry Controls):** Full recall via host chunk state (`MSHB` format) and automatic parameter sync across hardware Q-Links and the MPC touchscreen.
- **168 Built-In Programs:** Curated presets organized by category prefixes (*PSY*, *SAT*, *FLT*, *VOWL*, *DECY*, *LFO*, *GATE*, *PUMP*, *COMBO/MUSH*, *L/R*, *M/S*, *PAD*, *SYN*, *SWV*, *VOX*).
- **Swappable Core Saturation & Dual SVF Filter Engine:** **S-Curve** asymmetric/hard **Exhaustion** waveshaper paired with dual 4-type (**CLN**, **VOWL**, **ACID**, **DIGI**), 4-mode (**LP**, **HP**, **BP**, **NOTCH**) filters supporting **SERIAL**, **PARAL**, **STEREO** (L→A / R→B), and **M/S** (Mid→A / Side→B) routing, plus **Flt->Sat** order reversal.
- **Modulation, Noise & Audio Dynamics:** Two multi-shape LFOs (**Sine**, **Triangle**, **Saw**, **Square**, **Random**, **Drift**), a 6-type **Noise** / tone generator with **Filter FM**, an audio **Envelope Follower** targeting cutoffs, drive, resonance, and LFO rate/amount, and a **2×6** bipolar LFO modulation matrix.
- **16-Step Trance Gate & 4×4 Sidechain Pump:** **128** built-in gate patterns, 16 individually tappable step toggles, `Start`/`Hold`/`End` envelope shaping, phase **Gate Offset** (`-100%` to `+100%`), and a multi-target **Sidechain** processor driven either by live input audio (**INT**) or an internal quarter-note pulse (**4x4**).
- **Color Ping-Pong Delay, Lookahead Limiter & Real-Time Visualizer:** **Delay Color** macro sweeping from warm saturated tape echoes (`-1.00`) to crystalline chorus shimmer (`+1.00`), 2 ms lookahead **CLEAN** / **LMC** limiter, and a dedicated multi-layer **SCOPE** visualizer screen (`F6` SubTab 2).
- **ARM Standalone Optimization:** Written in dependency-free C++ using MIT-licensed `mpc::dsp` primitives, with 16-sample sub-block filter coefficient updates for single-digit ARM CPU overhead.

### System Requirements & Support

| Requirement | Specification |
|---|---|
| **Supported Hardware** | Akai Pro MPC X, MPC Live, MPC Live II, MPC One, MPC One+, MPC Key 61, MPC Key 37, MPC Live III, MPC XL, Akai Force (`32-bit` armv7 & `64-bit` aarch64 binaries included) |
| **Minimum MPC Firmware** | **MPC OS 3.10.1** or later |
| **Plugin Format** | External VST 2.4 Effect (`mushin-arm32.so` / `mushin-arm64.so`) |
| **Audio I/O** | Stereo In (2) / Stereo Out (2) |

> **Important:** Mushin is an **effect** plugin. In accordance with the MPC External VST2 host contract, effect plugins do not receive MIDI note events and cannot transmit MIDI output back to the MPC.

For source builds, simulator testing (`host_sim`), or documentation inside the SDK repository, consult `plugins/Mushin/README.md` and `docs/troubleshooting.md`.

---

## Setup & Installation

Mushin ships as a self-contained plugin folder named *`PhilippeBack - VST - Mushin/`* containing `plugin.json`, `version.xml`, both `VST/` ARM binaries (`arm32` and `arm64`), and the bespoke `Plugin Skins/` touchscreen assets.

### Manual Drive Installation

1. Build or obtain the output folder *`PhilippeBack - VST - Mushin/`* (when building from source in the SDK, run `python tools/build.py plugins/Mushin`).
2. Connect your MPC storage drive (USB drive, SD card, or internal SATA/NVMe drive) to your computer.
3. Locate or create a top-level folder named *`VST2 Plugins`* on the MPC drive.
4. Copy the entire *`PhilippeBack - VST - Mushin/`* folder inside *`VST2 Plugins/`* (do not rename the folder or move files out of `Plugin Skins/` or `VST/`).
5. Safely eject the drive and restart your MPC so it scans for external plugins at boot.

> **Important:** Removable drives formatted `exFAT` on some MPC units may mount `noexec`. If Mushin installs cleanly onto a removable drive but fails to load, copy the folder to internal storage instead.

### Network Deployment (Developer Mode)

If your MPC and computer share a local network:

1. On the MPC, open **Preferences > Info**, then tap the **MPC version** value 3 times, the **Serial number** value 2 times, and the **Firmware version** once to enable **Developer mode**.
2. Push an updated build directly to the MPC (for example, targeting drive `4`):
   `python tools/mpc_remote.py <MPC-IP> update plugins/Mushin --drive 4`
3. Save any open project on your MPC, then run the restart command so the MPC service rescans its plugins.
4. Switch **Developer mode** off in **Preferences > Info** when finished on untrusted networks.

### Enabling External Plugins on MPC

Mushin will not appear in the browser until external plugin hosting is switched on:

1. Press **MENU** and tap the gear icon to open **Preferences**.
2. Navigate to **Preferences > Enhancements (Beta)**.
3. Enable **Enable Enhancements**.
4. Enable **External Plugin Support** and confirm the prompt.

---

## Conventions Used in This Guide

- **Bold Text:** Used for physical MPC hardware controls (**Q-Link Knob 1**, **F1**–**F6**), on-screen tabs (**MAIN**, **FILTER**), switches, knobs, sliders, and menu options (**Delay Color**, **SC Source**).
- *Italic Text:* Used for cross-references to other sections, disk paths (*`VST2 Plugins/`*), newly introduced terms (*Exhaustion*), and emphasis.
- **Menu > Submenu > Option (`>`):** Indicates sequential navigation through screens or preferences (for example, **Preferences > Enhancements (Beta) > Enable Enhancements**).
- **UI Actions:** *Tap* refers to pressing a touchscreen switch or selector; *Turn* refers to adjusting a hardware Q-Link knob or rotary dial; *Double-tap* opens the fine-entry numeric/menu overlay on knobs.
- **Callouts:**
  - **Important:** Critical prerequisites, host limits, or actions that risk unsaved project data.
  - **Note:** Behavioral nuances, conditional visibility rules, and technical context.
  - **Tip:** Creative performance shortcuts and sound-design pairings.

---

## User Interface & Global Operation

### Loading & Preset Management

1. Select an audio track, drum program, pad insert, keygroup, or return/master bus in **Channel Mixer** / **Track View**.
2. Tap an empty FX insert slot, browse **External Plugins**, and select **Mushin** (listed under *PhilippeBack*).
3. Select any of the **168** factory patches from the MPC preset selector. Switching presets automatically refreshes all touchscreen knobs, buttons, 16 gate steps, and hardware Q-Link assignments.

> **Note:** On load, the MPC VST2 host toggles **Parameter 0** (**Gain**) slightly as part of plugin initialization. Parameter 0 is intentionally mapped to linear output **Gain** so this host probe produces no audible glitch or side effects.

> **Note:** If you update Mushin to a newer version on an MPC drive, remove and re-add existing instances on saved tracks so newly added parameters and skin tabs initialize properly.

### MPC Screen Tabs & Hardware Mapping

Mushin arranges its **136** controls across **9 logical touchscreen tabs** mapped to the MPC's **6 Function Keys (`F1`–`F6`)**, paired in dual-theme Cyan (top zone / Bank A) and Magenta (bottom zone / Bank B) bays:

| Function Key | SubTab | Screen Tab Name | Skin Background | Primary Focus |
|---|---|---|---|---|
| **F1** | 1 | **MAIN** | `bg_main.png` | 8 Cyber-Mecha Performance Macro Orbs |
| **F2** | 1 | **SATURATION** | `bg_scurve.png` | **S-Curve** saturation, **Info Decay**, and **Input Stage** |
| **F3** | 1 | **FILTER** | `bg_filter.png` | Dual **Filter A / B**, live Frequency×Resonance XY pads, **Routing** |
| **F4** | 1 | **LFO** | `bg_lfo.png` | **LFO 1 & 2**, live XY trace, **Noise Osc**, **Env Follower** |
| **F4** | 2 | **LFO MTX** | `bg_matrix.png` | **LFO 1 & 2 Matrix** + **Env > Flt** & **Env > LFO** modulation |
| **F5** | 1 | **GATE** | `bg_gate.png` | 16-Step **Trance Gate**, 128 patterns, **Gate Env** AHDS graph |
| **F5** | 2 | **PUMP** | `bg_pump.png` | **Sidechain Pump**, **Detector Filter**, **IN/DUCK** meters |
| **F6** | 1 | **DLY+LIM** | `bg_dly_lim.png` | Color Stereo/Ping-Pong **Delay**, Lookahead **Limiter** & **LIMITER GR** |
| **F6** | 2 | **SCOPE** | `bg_scope.png` | Real-time Milkdrop-style sprite visualizer + live macro control |

### Program Mode vs. Screen Mode Q-Links

- **Screen Mode Q-Links:** Every tab provides a tailored `RowTopDown` 16-knob map matching the 4×4 control layout on screen (`Q-Link 13–16` = Row 1 top, `Q-Link 9–12` = Row 2, `Q-Link 5–8` = Row 3, `Q-Link 1–4` = Row 4 bottom).
- **Program Mode Q-Links:** Outside of the custom plugin screen, Q-Links 1–16 default sequentially to Parameters `0–15` (**Gain**, **Drive**, **Exhaustion**, **Threshold**, **Auto Gain**, **Mix**, **Filter A Cutoff/Reso/Drive/Grit/Type/Mode**, **Filter B Cutoff/Reso/Drive/Grit**).

---

## Signal Flow & Processing Modules

### Signal Flow Overview

Audio enters Mushin stereo-in (`inputs[0..1]`), passes through automated gain leveling, optionally swaps filter and saturation order via **Flt->Sat**, traverses digital degradation and dry/wet blending, is sliced by the tempo-synced gate, ducked/modulated by the sidechain envelope follower, echoed, and brickwall-limited before stereo output:

```text
Input ──► Input Gain Stager (Auto In, In Target, In Trim)
               │
               ├──► [Env Follower] ──► Cutoffs, A Reso, Drive, LFO1 Spd, LFO Amt
               ├──► [Noise / Tone Osc] ──► (PRE-D / PRE-F / POST) + Filter FM
               │
               ▼ (Order controlled by Flt->Sat)
      ┌─► S-Curve Waveshaper (Active, Drive, Exhaustion, Threshold, Auto Gain)
      │        │
      │        ▼
      └─► Dual SVF Filter A & B (SERIAL / PARAL / STEREO / M/S, Filt Mix)
               │
               ▼
          Info Decay (Bit Depth, Downsample, Mix, Exh Link) + [Post Noise]
               │
               ▼
          Dry/Wet Mix ──► 16-Step Trance Gate ──► Final Gain (× Sidechain)
               │
               ▼
          Color Delay (Time / Sync Tempo, Ping-Pong, Delay Color)
               │
               ▼
          Lookahead Limiter (2 ms CLEAN / LMC, Drive, Ceiling, Release, Mix) ──► Output
```

---

### MAIN: Performance Macro Orbs (`F1`)

The **MAIN** page hosts **8 animated cyber-mecha Macro Orbs** arranged in two banks (`Bank 1 — Core Destruction & Choke` on Row 1, Cyan; `Bank 2 — Motion, Space & Rhythm` on Row 3, Magenta). Orbs respond non-destructively on top of your underlying patch parameters: turning a macro modifies its assigned DSP destinations relative to stored base values (`baseValues_[]`), while tweaking underlying parameters updates those stored bases inversely so neither overwrites the other.

| Parameter | Description | Range / Values |
|---|---|---|
| **EXHAUSTION** | Pushes **S-Curve** into active overdrive: turns **S-Curve Active** **On**, boosts **Drive** up to `+12.0x`, engages hard **Exhaustion** clipping above `0.65`, and compensates output **Gain** downward up to `-35%` to tame loudness jumps. | `0.00 – 1.00` (Default: `0.00`) |
| **CHOKE** | Squeezes both **Filter A** and **Filter B** cutoffs dramatically downward (up to `-7` octaves), raises resonance (`+0.50`), and when **Routing** is **SERIAL** with a wide-open **Filter B LP**, morphs Filter B into a tight band-choking highpass sweep (`20 Hz` up to `640 Hz`). | `0.00 – 1.00` (Default: `0.00`) |
| **INFO DECAY** | Activates bit-crushing and sample-rate reduction: enables **Decay Active**, reduces **Bit Depth** down toward `2.0 bit`, raises **Downsample** from `1x` up to `12x`, and pushes **Decay Mix** toward full wet. | `0.00 – 1.00` (Default: `0.00`) |
| **CORRUPT MIX** | Pushes main **Mix** toward `1.00` wet while awakening **Noise Active** (defaulting to **PRE-F** routing if previously inactive) and lifting **Noise Level** up to `+24.0 dB`. | `0.00 – 1.00` (Default: `0.00`) |
| **KINETIC MOD** | Unlocks kinetic filter motion: accelerates **LFO 1 Freq** (`+6.0 Hz`) and **LFO 2 Freq** (`+8.5 Hz`), opens **L1 A Cut**, **L1 A Res**, **L2 B Cut**, and **L2 B Res** matrix depths, and pulls both filter cutoffs down into resonant sweep range. | `0.00 – 1.00` (Default: `0.00`) |
| **RESONATOR** | Engages **Delay** as a metallic comb/Karplus-style resonator (`32 ms` down to `12 ms` when Delay was previously off), cranks **Feedback** up to `0.88`, raises delay **Mix** (`0.45`), and injects `0.50` **Grit** into both Filters A and B. | `0.00 – 1.00` (Default: `0.00`) |
| **GATE CARVE** | Engages the **Trance Gate** (selecting `05 4-On-Floor` if previously inactive), tightens **Start** (`12.2 → 5.0 ms`), **Hold** (`68% → 28%`), and **End** (`52 → 16 ms`) transients, pushes **Depth** (`76% → 100%`), and lifts gate **Mix** (`0.64 → 1.00`). | `0.00 – 1.00` (Default: `0.00`) |
| **SMASH** | Enables the final **Limiter**, forces aggressive **LMC** crushing mode above `0.30`, drives **Limit Drive** up to `+18.0 dB`, and pushes **Limit Mix** to parallel/full wet smash. | `0.00 – 1.00` (Default: `0.00`) |

> **Tip:** All 8 macro orbs remain accessible on Q-Links `9–16` when viewing the **SCOPE** (`F6` SubTab 2) visualizer page for live set performances.

---

### SATURATION: Input Stage, S-Curve & Info Decay (`F2`)

The **SATURATION** page houses three interrelated zones: the top **S-CURVE** waveshaper & master stage (Rows 1–2), bottom-left **INFO DECAY** lo-fi engine (Rows 3–4 Col 1), and bottom-right **INPUT STAGE** gain-conditioning & signal-order block (Rows 3–4).

#### S-Curve & Master Stage

| Parameter | Description | Range / Values |
|---|---|---|
| **S-Curve Active** | Master bypass switch for the **S-Curve** / **Exhaustion** waveshaper block. When **Off**, audio enters the filter/mixing stage unshaped while preserving Auto-Level input trim. | `Off, On` (Default: `Off`) |
| **Drive** | Scales pre-shaper input gain before asymmetric soft saturation or hard clipping. | `1.0 – 20.0 x` (Default: `1.0 x`; mod range up to `32.0 x`) |
| **Threshold** | Sets the saturation knee / hard-clip ceiling (`dbToGain`). Lower negative values cause earlier, heavier harmonic compression and fuzz. | `-24.0 – 0.0 dB` (Default: `0.0 dB`) |
| **Mix** *(Dry/Wet Mix)* | Blends pre-saturation/filter/decay input audio (`0.00` dry) with post-decay processed signal (`1.00` wet) before the Trance Gate and Delay. | `0.00 – 1.00` (Default: `1.00`) |
| **Exhaustion** | Toggles between warm hyperbolic-tangent S-curve shaping (**Off**) and razor-sharp symmetric hard diode clipping (**On**) at **Threshold**. | `Off, On` (Default: `Off`) |
| **Auto Gain** | Automatically compensates output level as **Drive** rises, tracking peak input envelope so perceived volume stays balanced. | `Off, On` (Default: `Off`) |
| **Gain** *(Parameter 0)* | Post-gate linear output multiplier applied right before **Delay** and **Limiter** (also modulated downward when **Sidechain** targets **SC Gain**). | `0.00 – 2.00 x` (Default: `1.00 x`) |
| **Flt->Sat** | Swaps the order of core processing stages: **Off** runs *S-Curve → Dual Filters*; **On** runs *Dual Filters → S-Curve* so resonant peaks hit saturation directly. | `Off, On` (Default: `Off`) |

#### Info Decay

| Parameter | Description | Range / Values |
|---|---|---|
| **Active** *(Decay Active)* | Enables or bypasses bit-depth quantization and sample-rate downsampling. | `Off, On` (Default: `Off`) |
| **Bit Depth** | Quantizes amplitude into fewer digital levels (`16.0` clean down to `2.0 bit` extreme step noise). | `2.0 – 16.0 bit` (Default: `16.0 bit`) |
| **Downsample** | Integer zero-order hold divider holding every `N`-th sample (`1x` bypasses downsampling; `32x` produces heavy aliasing artifacts). | `1 – 32 x` (Default: `1 x`) |
| **Mix** *(Decay Mix)* | Parallel blend between clean audio and crushed/downsampled **Info Decay** output. | `0.00 – 1.00` (Default: `1.00`) |
| **Exh Link** | When **On** alongside active **Exhaustion**, overrides **Bit Depth** directly down to `4.0 bit` for instant industrial crunch. | `Off, On` (Default: `Off`) |
| **Decay Viz** | `[Read-Only Visualizer]` Internal crunch intensity meter driving the `#FF4D8D` animated pixelated overlay across Row 3 of the **SATURATION** page. | `0.00 – 1.00` |

#### Input Stage

| Parameter | Description | Range / Values |
|---|---|---|
| **Auto In** | Activates Mushin's fast-attack (`1 ms`), smooth-release (`450 ms`) automatic input gain stager so quiet pads and loud master buses hit the shaper at consistent calibration. | `Off, On` (Default: `On`) |
| **In Target** | Reference RMS/peak calibration level targeted by **Auto In** (`0.25x` to `8.0x` automatic trim range). | `-24.0 – 0.0 dB` (Default: `-9.0 dB`) |
| **In Trim** | Manual pre-FX gain trim applied right after **Auto In** calibration. | `-18.0 – +24.0 dB` (Default: `0.0 dB`) |

---

### FILTER: Dual Filters A & B, XY Telemetry & Routing (`F3`)

The **FILTER** page provides two identical 4-topology stereo SVF/formant engines (**Filter A** in Cyan on top; **Filter B** in Magenta on bottom), two live draggable **XYPad** displays (**A FREQ X RES** and **B FREQ X RES**), parallel blend, and 4 signal routing topologies.

#### Filter A Controls (Cyan Zone)

| Parameter | Description | Range / Values |
|---|---|---|
| **Cutoff** *(A Cutoff)* | Primary cutoff / center frequency on logarithmic scale. In **VOWL** mode, logs smoothly sweep across the 5 vocal formant anchors `A → E → I → O → U`. | `20 – 20000 Hz` (Default: `20000 Hz`) |
| **Reso** *(A Reso)* | Filter resonance / Q peaking. **ACID** type boosts Q by `1.15×` for squelchy 303 peaks; **VOWL** raises parallel formant Q up to `0.98`. | `0.00 – 1.00` (Default: `0.10`) |
| **Drive** *(A Drive)* | Pre-filter saturation stage pushing harmonics into Filter A (`ACID` applies an additional `1.4×` drive factor). | `1.0 – 10.0 x` (Default: `1.0 x`) |
| **Grit** *(A Grit)* | Post-filter saturation fold adding aggressive edge before Filter A outputs. | `0.00 – 1.00` (Default: `0.00`) |
| **Type** *(A Type)* | Selects Filter A's voicing character: **CLN** (transparent 12 dB/oct SVF), **VOWL** (3-band vocal formant array), **ACID** (303-inspired hot-drive high-Q SVF), or **DIGI** (cascaded 24 dB/oct surgical digital SVF with hard clipping and quantized Grit). | `CLN, VOWL, ACID, DIGI` (Default: `CLN`) |
| **Mode** *(A Mode)* | Selects filter response shape (or spectral tilt mix in **VOWL** mode). | `LP, HP, BP, NOTCH` (Default: `LP`) |

#### Filter B & Global Filter Routing (Magenta Zone + Routing Subpanel)

| Parameter | Description | Range / Values |
|---|---|---|
| **Cutoff** *(B Cutoff)* | Filter B cutoff frequency (logarithmic). Sweeps `A → E → I → O → U` when Filter B **Type** is **VOWL**. | `20 – 20000 Hz` (Default: `20000 Hz`) |
| **Reso** *(B Reso)* | Filter B resonance / Q factor. | `0.00 – 1.00` (Default: `0.10`) |
| **Drive** *(B Drive)* | Pre-filter saturation drive for Filter B. | `1.0 – 10.0 x` (Default: `1.0 x`) |
| **Grit** *(B Grit)* | Post-filter saturation edge for Filter B. | `0.00 – 1.00` (Default: `0.00`) |
| **Type** *(B Type)* | Selects Filter B voicing (**CLN**, **VOWL**, **ACID**, **DIGI**). | `CLN, VOWL, ACID, DIGI` (Default: `CLN`) |
| **Mode** *(B Mode)* | Selects Filter B response (**LP**, **HP**, **BP**, **NOTCH**). | `LP, HP, BP, NOTCH` (Default: `LP`) |
| **Filt Mix** | Blends bypassed pre-filter audio (`0.00`) with filtered A/B output (`1.00`). | `0.00 – 1.00` (Default: `1.00`) |
| **Path** *(Routing)* | Configures how signal passes through Filters A and B: **SERIAL** (`A → B`), **PARAL** (`0.5 × (A + B)`), **STEREO** (`Left → A`, `Right → B`), or **M/S** (`Mid → A`, `Side → B`, decoded back to L/R). | `SERIAL, PARAL, STEREO, M/S` (Default: `SERIAL`) |

> **Tip:** Drag the **A FREQ X RES** and **B FREQ X RES** cyan/magenta pucks directly on the touchscreen to perform live two-handed Acid/Vowel filter sweeps.

---

### LFO: Dual Oscillators, Noise Generator & Envelope Follower (`F4` SubTab 1)

| Parameter | Description | Range / Values |
|---|---|---|
| **LFO 1 Freq** | Oscillation frequency of **LFO 1** on logarithmic scale (expandable up to `35 Hz` via **Env L1 Spd** dynamic modulation). | `0.10 – 20.00 Hz` (Default: `1.00 Hz`) |
| **LFO 1 Wave** | Waveform shape for **LFO 1** (**Drift** creates dual-timescale Catmull-Rom analog thermal wander). | `Sine, Triangle, Saw, Square, Random, Drift` (Default: `Sine`) |
| **LFO 2 Freq** | Oscillation frequency of **LFO 2** on logarithmic scale (modulated dynamically via **Env LFO Amt**). | `0.10 – 20.00 Hz` (Default: `1.00 Hz`) |
| **LFO 2 Wave** | Waveform shape for **LFO 2** (independent RNG seed from **LFO 1** in **Drift** mode). | `Sine, Triangle, Saw, Square, Random, Drift` (Default: `Sine`) |
| **Noise Active** | Enables audible injection of the **Noise** / tone generator into the signal path (**Filter FM** still runs from raw oscillator output when `Filter FM > 0` even if **Noise Active** is **Off**). | `Off, On` (Default: `Off`) |
| **Type** *(Noise Type)* | Selects noise spectrum or pitched carrier waveform for audio injection and Filter FM. | `White, Pink, Sine, Triangle, Saw, Square` (Default: `White`) |
| **Freq** *(Noise Freq)* | Pitched tone frequency when **Noise Type** is **Sine**, **Triangle**, **Saw**, or **Square**. | `20 – 20000 Hz` (Default: `440 Hz`) |
| **Level** *(Noise Level)* | Output level of injected noise / oscillator signal. | `-60.0 – 0.0 dB` (Default: `-20.0 dB`) |
| **Route** *(Noise Route)* | Insertion point: **PRE-D** (before Waveshaper), **PRE-F** (right before Dual Filters), or **POST** (after Info Decay, prior to Mix/Gate). | `PRE-D, PRE-F, POST` (Default: `PRE-D`) |
| **Filter FM** | Bipolar-less depth injecting the **Noise/Tone Osc** signal directly into both **Filter A** and **Filter B** cutoff frequency (`exp2`) for growling audio-rate FM timbres. | `0.00 – 1.00` (Default: `0.00`) |
| **Env Cut** | Bipolar **Envelope Follower** depth modulating both **Filter A** and **Filter B** cutoffs from input peak dynamics (`5 ms` attack / `90 ms` release). | `-1.00 to +1.00` (Default: `0.00`) |
| **Env Drive** | Bipolar **Envelope Follower** depth modulating **S-Curve Drive** (`× exp2`) and Filter A/B pre-drive dynamically with input loudness. | `-1.00 to +1.00` (Default: `0.00`) |

> **Note:** The wide **LFO 1 X × LFO 2 Y** XY pad in Row 2 maps X directly to **LFO 1 Freq** and Y directly to **LFO 2 Freq** for instant polyrhythmic speed sweeps.

---

### LFO MTX: 2×6 Modulation Matrix & Dynamic Envelope Targets (`F4` SubTab 2)

Rows 1–2 assign **LFO 1** across Filter A & B (Cols 1–3); Rows 3–4 assign **LFO 2** across Filter A & B (Cols 1–3). Column 4 hosts the **ENV > FLT** (Cyan) and **ENV > LFO** (Magenta) dynamic destinations.

| Parameter | Source → Destination | Range / Values |
|---|---|---|
| **L1 A Cut** / **L1 A Res** / **L1 A Grit** | Bipolar **LFO 1** depth targeting Filter A **Cutoff** (`±5` octaves), **Reso**, and **Grit**. | `-1.00 to +1.00` (Default: `0.00`) |
| **L1 B Cut** / **L1 B Res** / **L1 B Grit** | Bipolar **LFO 1** depth targeting Filter B **Cutoff**, **Reso**, and **Grit**. | `-1.00 to +1.00` (Default: `0.00`) |
| **L2 A Cut** / **L2 A Res** / **L2 A Grit** | Bipolar **LFO 2** depth targeting Filter A **Cutoff**, **Reso**, and **Grit**. | `-1.00 to +1.00` (Default: `0.00`) |
| **L2 B Cut** / **L2 B Res** / **L2 B Grit** | Bipolar **LFO 2** depth targeting Filter B **Cutoff**, **Reso**, and **Grit**. | `-1.00 to +1.00` (Default: `0.00`) |
| **Env A Res** | Envelope Follower depth modulating Filter A **Reso** (`±0.90`). | `-1.00 to +1.00` (Default: `0.00`) |
| **Env B Cut** | Additive Envelope Follower offset added to **Filter B Cutoff** relative to Filter A (great for counter-sweeping **M/S** or **STEREO** filters). | `-1.00 to +1.00` (Default: `0.00`) |
| **Env L1 Spd** | Envelope Follower acceleration/slowdown scaling **LFO 1 Freq** (`× 2^(3·amt·env)` up to `35 Hz`). | `-1.00 to +1.00` (Default: `0.00`) |
| **Env LFO Amt** | Dynamic amplitude scaling applied across **all** LFO 1 & LFO 2 Matrix slots simultaneously (`× 2^(2·amt·env)`). | `-1.00 to +1.00` (Default: `0.00`) |

---

### GATE: 16-Step Trance Gate & AHDS Envelope Shaper (`F5` SubTab 1)

| Parameter | Description | Range / Values |
|---|---|---|
| **Gate Active** | Enables the tempo-synchronized 16-step amplitude chopper (locks to host PPQ when transport is playing, free-runs at project BPM when stopped). | `Off, On` (Default: `Off`) |
| **Pattern** *(Gate Pattern)* | Selects one of **128** factory step masks organized across Core (`01–08`), House/Trance (`09–24`), Techno (`25–40`), Breakbeat/DnB (`41–56`), Trap/Hip-Hop (`57–72`), Euclidean (`73–88`), Afro-Latin/Funk (`89–104`), Glitch (`105–120`), and Sparse (`121–128`). Loading a pattern updates **Step 1–16** immediately. | `01 Straight 16` – `128 Sustain 15` (`128` patterns) |
| **Rate** *(Gate Rate)* | Clock division per step across a 16-step loop. | `1/16, 1/8, 1/4` (Default: `1/16`) |
| **Depth** *(Gate Depth)* | Attenuation depth on closed steps (`0%` keeps signal full; `100%` drops closed steps to silence). Synced inversely with **Gate Floor**. | `0 – 100%` (Default: `50%`) |
| **Step 1 – Step 16** | Individual step toggles across the 16-step sequence (`Odd` beat steps `1, 5, 9, 13` display beat pip markers). | `Off, On` (Default: pattern `01`: even steps `On`, odd steps `Off`) |
| **Mix** *(Gate Mix)* | Dry/wet blend around the gate gain multiplier. | `0.00 – 1.00` (Default: `1.00`) |
| **Start** *(Gate Start)* | Fade-in attack ramp time at the start of each active step (capped internally to `0.5 × step`). | `0.0 – 100.0 ms` (Default: `5.0 ms`) |
| **Hold** *(Gate Hold)* | Total active gate duty cycle as a percentage of one step duration. | `10 – 100%` (Default: `50%`) |
| **End** *(Gate End)* | Release ramp down from peak to step close (capped to `0.5 × step`). | `0.0 – 200.0 ms` (Default: `10.0 ms`) |
| **Gate Offset** | Phase-shifts the gate pattern timing relative to the beat by up to one full step earlier (`-100%`) or later (`+100%`). | `-100% to +100%` (Default: `0%`) |
| **Gate Floor** | `[Read-Only Telemetry]` Complement of **Gate Depth** (`1.0 - Depth/100`) driving the live touchscreen AHDS envelope height. | `0.00 – 1.00` (Default: `0.50`) |
| **Gate Step** (`135`, `tg_play_step`) | `[Read-Only Telemetry]` Active step playhead tracking the rhythm in real time with a glowing chaser/halo highlight across **Step 1–16** (switches dark when bypassed). | `0.00 – 1.00` (Default: `1.00` / Off) |

---

### PUMP: Sidechain Pumper & Detector Filter (`F5` SubTab 2)

| Parameter | Description | Range / Values |
|---|---|---|
| **SC Active** | Enables the Sidechain processor (requires at least one target toggle **SC Drive / Cut / Gain / Reso** set **On**). | `Off, On` (Default: `Off`) |
| **Mode** *(SC Mode)* | Switches detector rectification between transient **PEAK** sensing and squared-energy **RMS** sensing (when **SC Source** is **INT**). | `PEAK, RMS` (Default: `PEAK`) |
| **SC Ratio** | Scales pump curve steepness above threshold (`1:1` flat → `4:1` classic → `20:1` hard brickwall pump). | `1.0:1 – 20.0:1` (Default: `4.0:1`) |
| **SC Mix** | Scales total modulation intensity produced by the sidechain envelope. | `0.00 – 1.00` (Default: `1.00`) |
| **Thresh** *(SC Thresh)* | Detector threshold level (`dB`) where sidechain ducking / modulation begins. | `-60.0 – 0.0 dB` (Default: `-24.0 dB`) |
| **Amount** *(SC Amount)* | Bipolar depth & direction of sidechain modulation (`-60%` ducks Gain / drops Cutoff; positive values expand/swell upwards). | `-100% to +100%` (Default: `-60%`) |
| **Attack** *(SC Attack)* | Envelope attack time constant. | `0.1 – 500.0 ms` (Default: `10.0 ms`) |
| **Release** *(SC Release)* | Recovery release time governing how fast volume/cutoff pumps back between beats. | `1 – 2000 ms` (Default: `100 ms`) |
| **HPF** *(SC HPF)* | Sidechain key detector Highpass filter isolating sub/low energy when raised. | `20 – 2000 Hz` (Default: `20 Hz`) |
| **LPF** *(SC LPF)* | Sidechain key detector Lowpass filter focusing detector on kick body vs hats. | `500 – 20000 Hz` (Default: `20000 Hz`) |
| **SC Reso** | Routes sidechain modulation to **Filter A Reso** (`+scMod × 0.50`). | `Off, On` (Default: `Off`) |
| **SC Drive** | Routes sidechain modulation to **S-Curve Drive** (`× exp2(scMod × 2.5)`). | `Off, On` (Default: `Off`) |
| **SC Cut** | Routes sidechain modulation to both **Filter A** and **Filter B** cutoffs (`× exp2(scMod × 6.0)`). | `Off, On` (Default: `Off`) |
| **SC Gain** | Routes sidechain modulation to post-gate **Output Gain** (`× (1 + scMod)` — classic ducking when **SC Amount** < 0). | `Off, On` (Default: `On`) |
| **SC Source** | Selects the detector key trigger: **INT** triggers from Mushin's filtered audio input; **4x4** generates an internal sample-accurate quarter-note trigger pulse (`14 ms` pulse on every beat). | `INT, 4x4` (Default: `INT`) |
| **SC In / SC Duck** | `[Read-Only Meters]` Live vertical cyan (**IN** input peak) and magenta (**DUCK** modulation depth) LED telemetry meters in Row 3 Col 4. | `0.00 – 1.00` |

> **Note:** Legacy parameter index `42` (**Target**: `DRIVE, CUT, GAIN`) remains stored in parameter state for chunk compatibility and is superseded in the touchscreen UI by the independent multi-target toggles **SC Drive**, **SC Cut**, **SC Gain**, and **SC Reso**.

---

### DLY+LIM: Color Delay & Lookahead Limiter (`F6` SubTab 1)

#### Stereo / Ping-Pong Color Delay (Top Zone)

| Parameter | Description | Range / Values |
|---|---|---|
| **Active** *(Delay Active)* | Enables or bypasses the Stereo/Ping-Pong Delay module. | `Off, On` (Default: `Off`) |
| **Sync** *(Delay Sync)* | Locks delay time to host/project BPM note divisions (**On**) vs free milliseconds (**Off**). **Conditionally switches Row 2 Col 2** between **Delay Time** and **Delay Tempo**. | `Off, On` (Default: `Off`) |
| **Ping-Pong** | When **On**, sums stereo input into the Left tap (`0.5 × (L + R)`) and cross-bounces `L → R → L` on beat even for mono/centered signals; **Off** keeps independent Dual L/R stereo delay lines. | `Off, On` (Default: `Off`) |
| **Delay Color** | Bipolar feedback toning & stereo modulation macro: **Negative** (`-1.00`) rolls feedback lowpass down to `480 Hz`, adds rational tape saturation, and applies subtle `0.75 Hz` `±0.65 ms` flutter; **Positive** (`+1.00`) sweeps feedback highpass up to `950 Hz`, opens top air (`16 kHz`), and adds `1.25 Hz` `±1.45 ms` stereo chorus shimmer. | `-1.00 to +1.00` (Default: `0.00`) |
| **Feedback** *(Delay FB)* | Regenerative delay feedback loop amount (internally limited below self-oscillation clipping). | `0.00 – 0.95` (Default: `0.30`) |
| **Time** *(Delay Time)* | *(Visible when **Sync = Off**)* Manual delay time in milliseconds (up to `2000 ms`, usable down to `12–32 ms` for Karplus/comb resonator effects). | `1 – 2000 ms` (Default: `300 ms`) |
| **Tempo** *(Delay Tempo)* | *(Visible when **Sync = On**)* Rhythmic note division synced to project BPM. | `1/16, 1/8, 1/4, 1/2` (Default: `1/4`) |
| **Mix** *(Delay Mix)* | Wet delay blend mixed over the dry post-gate signal. | `0.00 – 1.00` (Default: `0.30`) |

#### Final 2 ms Lookahead Limiter (Bottom Zone)

| Parameter | Description | Range / Values |
|---|---|---|
| **Active** *(Limit Active)* | Enables the 2 ms lookahead final stereo peak limiter. | `Off, On` (Default: `Off`) |
| **Drive** *(Limit Drive)* | Input gain pushing transients into the lookahead limiter ceiling. | `0.0 – 24.0 dB` (Default: `0.0 dB`) |
| **Release** *(Limit Rel)* | Recovery speed of the limiter gain-reduction envelope. | `10 – 1000 ms` (Default: `100 ms`) |
| **Ceiling** *(Limit Ceil)* | Maximum brickwall output ceiling (`dBFS`). | `-12.0 – 0.0 dB` (Default: `-0.1 dB`) |
| **Mode** *(Limit Mode)* | **CLEAN** applies transparent brickwall limiting (`1:∞` above ceiling); **LMC** applies aggressive `20:1` vintage listen-mic squash character. | `CLEAN, LMC` (Default: `CLEAN`) |
| **Mix** *(Limit Mix)* | Parallel blend between delayed uncompressed input and gain-reduced limiter output (allows NY-style parallel drum smash). | `0.00 – 1.00` (Default: `1.00`) |
| **Lim GR** | `[Read-Only Meter]` Horizontal cyan-to-magenta **LIMITER GR** bar displaying real-time peak gain reduction. | `0.00 – 1.00` |

---

### SCOPE: Real-Time Visual Synthesizer (`F6` SubTab 2)

The **SCOPE** tab renders a multi-layer SNES-sprite / Milkdrop-style real-time audio visualizer driven at ~30 Hz (sample-rate independent) by 8 read-only telemetry parameters, with a live **4-Theme Switcher** pill strip (`NEON | SOLAR | CYBER | VOID`, **Parameter 134**) pinned in the upper-right corner:

1. **Warping Tunnel / Wireframe Backdrop (`Viz Phase`):** 24-frame seamless sprite layer reacting to output pulse and **Delay Feedback** — cyan/magenta Lissajous tunnel (**NEON**), swirling solar flare vortex (**SOLAR**), 3D perspective octagon vector corridor (**CYBER**), or bioluminescent abyssal caustics (**VOID**).
2. **Center Reactor Core (`Viz Pulse`):** Centered 20-frame visual centerpiece — sacred mandala kaleidoscope (**NEON**), solar eclipse shockwave corona (**SOLAR**), spinning 3D wireframe octahedron/cube reticle (**CYBER**), or orbiting lotus medusa nebula (**VOID**).
3. **Twin Lissajous Comets / Reticles (`Viz X1 / Y1`, `Viz X2 / Y2`):** Dual theme-matched sprite pucks reacting simultaneously to **LFO 1/2**, stereo side energy, and **SC Duck** envelope activity.
4. **Mirrored Center-Outward Wing Meters (`Viz Left / Right`):** Dual stereo peak bars pulsing outward across the bottom rim in each theme's accent palette.
5. **Full Live Q-Link Performance Matrix:** All 16 hardware Q-Links remain live directly inside **SCOPE** for hands-on performance (**Q-Link 13–16 / 9–12**: 8 Macro Orbs; **Q-Link 1–8**: **Drive**, **Mix**, **Delay Feedback**, **Delay Mix**, **A Cutoff**, **A Reso**, **B Cutoff**, **LFO 1 Freq**).

| Control / Telemetry Parameter | Type | Range |
|---|---|---|
| **Theme** (`134`, `viz_theme`) | `[Automatable]` Top-right corner live visual switcher (persists across factory preset switches) | `NEON`, `SOLAR`, `CYBER`, `VOID` |
| **Viz Phase** (`126`) / **Viz Pulse** (`127`) | `[Read-Only Visualizer]` Tunnel & Center Core animation drivers | `0.00 – 1.00` |
| **Viz X1 / Y1** (`128–129`), **Viz X2 / Y2** (`130–131`) | `[Read-Only Visualizer]` Dual Lissajous orb sprite XY coordinates | `0.00 – 1.00` |
| **Viz Left** (`132`) / **Viz Right** (`133`) | `[Read-Only Visualizer]` Mirrored L/R stereo output energy wings | `0.00 – 1.00` |

---

## "How-To" Workflows

### Performing with the 8 Macro Orbs Without Losing Patch Balance

1. Load any multi-stage preset (such as `M/S: Dub Tape Center` or `PSY: Magic Acid Gate`) and switch to **MAIN** (**F1**).
2. Turn **Q-Link 14** (**CHOKE**) or **Q-Link 9** (**KINETIC MOD**) clockwise during a build-up; Mushin stores your preset's original filter cutoffs and LFO rates inside `baseValues_[]` and shifts dynamically relative to them.
3. Return the Q-Link back to `0.00` at the drop—every underlying parameter returns cleanly to your exact preset calibration without manual re-dialing.

### Creating an Instant 4×4 Pumping Groove Without External Routing

1. Navigate to **PUMP** (**F5** SubTab 2).
2. Tap **Active** (`Q-Link 13`) **On** and set **SC Source** (`Q-Link 4`, bottom-right) to **4x4**.
3. Ensure **SC Gain** (`Q-Link 3`) is **On** (and optionally toggle **SC Cut** **On** so the filter darkens on each kick transient too).
4. Turn **SC Amount** (`Q-Link 10`) negative (`-55%` to `-75%`), **SC Ratio** (`Q-Link 15`) to `6:1–8:1`, and dial **Release** (`Q-Link 12`) between `130–220 ms` until the pad or synth breathes in pocket with your project BPM.

### Customizing Rhythmic Gates with Steps, Patterns & Phase Offset

1. Navigate to **GATE** (**F5** SubTab 1) and set **Gate Active** **On**.
2. Turn **Pattern** (`Q-Link 14`) to audition any of the **128** patterns (`01–128`). **Step 1–16** touchscreen buttons update automatically to reveal the active pulse mask.
3. Tap individual step buttons on Row 2 to mute downbeats or add ghost 16th syncopations.
4. Sweep **Gate Offset** (`Q-Link 8`, Row 3 Col 4) slightly positive or negative to pull the rhythmic chop forward or push it behind the beat for custom swing.

### Sculpting Stereo Width with M/S Filtering & Delay Color

1. On **FILTER** (**F3**), set **Routing** (`Q-Link 4`) to **M/S** (`Mid → Filter A`, `Side → Filter B`).
2. Keep **Filter A** (**CLN LP** or **ACID LP**) tight around `120–400 Hz` or gentle on the center mono image while modulating **Filter B** (`VOWL BP` / `CLN HP`) with **LFO 1/2** (`L1 B Cut`) so only the stereo sides swirl wide.
3. On **DLY+LIM** (**F6** SubTab 1), turn **Delay Active** and **Ping-Pong** **On**, enable **Delay Sync** (`1/8` or `1/16`), and dial **Delay Color** (`Q-Link 16`, Row 1 Col 4) negative (`-0.50`) for warm analog dub tape echoes or positive (`+0.55`) for crystalline stereo chorus tails.

---

## Appendix

### Appendix A: Complete Q-Link Bank Mapping

*(Screen Mode Q-Links use `RowTopDown`: `13–16` = Row 1 top, `9–12` = Row 2, `5–8` = Row 3, `1–4` = Row 4 bottom; `—` indicates an unmapped display/spacer cell)*

| Page (`F-Key`) | Row 1 (`Q13 / Q14 / Q15 / Q16`) | Row 2 (`Q9 / Q10 / Q11 / Q12`) | Row 3 (`Q5 / Q6 / Q7 / Q8`) | Row 4 (`Q1 / Q2 / Q3 / Q4`) |
|---|---|---|---|---|
| **MAIN** (`F1`) | `77` EXHAUSTION / `78` CHOKE / `79` INFO DECAY / `80` CORRUPT MIX | `81` KINETIC MOD / `82` RESONATOR / `83` GATE CARVE / `84` SMASH | `— / — / — / —` | `— / — / — / —` |
| **SATURATION** (`F2`) | `96` S-Curve Active / `1` Drive / `3` Threshold / `5` Mix | `2` Exhaustion / `4` Auto Gain / `0` Gain / `120` Flt->Sat | `59` Decay Active / `60` Bit Depth / `61` Downsample / `62` Decay Mix | `63` Exh Link / `115` Auto In / `116` In Target / `117` In Trim |
| **FILTER** (`F3`) | `6` A Cutoff / `7` A Reso / `8` A Drive / `9` A Grit | `10` A Type / `11` A Mode / `— / —` | `12` B Cutoff / `13` B Reso / `14` B Drive / `15` B Grit | `16` B Type / `17` B Mode / `85` Filt Mix / `18` Path (Routing) |
| **LFO** (`F4.1`) | `19` LFO 1 Freq / `20` LFO 1 Wave / `21` LFO 2 Freq / `22` LFO 2 Wave | `45` Noise Active / `46` Noise Type / `47` Noise Freq / `48` Noise Level | `49` Noise Route / `50` Filter FM / `118` Env Cut / `119` Env Drive | `— / — / — / —` |
| **LFO MTX** (`F4.2`) | `23` L1 A Cut / `24` L1 A Res / `25` L1 A Grit / `121` Env A Res | `26` L1 B Cut / `27` L1 B Res / `28` L1 B Grit / `122` Env B Cut | `29` L2 A Cut / `30` L2 A Res / `31` L2 A Grit / `123` Env L1 Spd | `32` L2 B Cut / `33` L2 B Res / `34` L2 B Grit / `124` Env LFO Amt |
| **GATE** (`F5.1`) | `51` Gate Active / `53` Pattern / `54` Rate / `58` Depth | `— / — / — /` `52` Gate Mix | `55` Start / `56` Hold / `57` End / `114` Gate Offset | `— / — / — / —` |
| **PUMP** (`F5.2`) | `35` SC Active / `40` Mode / `90` SC Ratio / `87` SC Mix | `37` Thresh / `41` Amount / `38` Attack / `39` Release | `43` HPF / `44` LPF / `94` SC Reso / `—` | `91` SC Drive / `92` SC Cut / `93` SC Gain / `36` SC Source |
| **DLY+LIM** (`F6.1`) | `64` Delay Active / `69` Sync / `68` Ping-Pong / `125` Delay Color | `66` Feedback / `65` Time / `70` Tempo / `67` Delay Mix | `71` Lim Active / `73` Lim Drive / `75` Lim Rel / `74` Lim Ceil | `72` Lim Mode / `— / — /` `76` Lim Mix |
| **SCOPE** (`F6.2`) | `77` EXHAUSTION / `78` CHOKE / `79` INFO DECAY / `80` CORRUPT MIX | `81` KINETIC MOD / `82` RESONATOR / `83` GATE CARVE / `84` SMASH | `6` A Cutoff / `7` A Reso / `12` B Cutoff / `19` LFO 1 Freq | `1` Drive / `5` Mix / `66` Delay FB / `67` Delay Mix |

---

### Appendix B: Factory Preset Directory (168 Programs)

| Bank Prefix | Count | Program Indices & Names |
|---|---|---|
| **Core** | 6 | `0` Init, `1` Warm Tape Sat, `2` Dual Resonator, `3` Crushed Velvet, `4` Rhythmic Gater, `5` Mushin Pumper |
| **PSY:** | 15 | `6` PSY: Init, `7` PSY: Forest FM Lead, `8` PSY: Rolling Bass 16, `9` PSY: Dark Squelch, `10` PSY: Gate Grid 148, `11` PSY: Alien Formant, `12` PSY: Night Riser, `13` PSY: Magic Acid Gate, `14` PSY: Acid Squelch Zap, `15` PSY: Euclid 7/16 Magic, `16` PSY: Hypno 3-Step Arp, `17` PSY: Magic Trance Gate, `18` PSY: Goa 303 Squelcher, `19` PSY: Twilight Polyrhythm, `20` PSY: Magic Neuro Chop |
| **SAT:** | 10 | `21` SAT: Init, `22` SAT: Hard Clip, `23` SAT: Warm Bus, `24` SAT: Velvet Tape, `25` SAT: Tube Glow, `26` SAT: Sub Crunch, `27` SAT: Fuzz Wall, `28` SAT: Edge Bite, `29` SAT: Master Glue, `30` SAT: Burned Coil |
| **FLT:** | 10 | `31` FLT: Init, `32` FLT: Acid Sweep, `33` FLT: Parallel Band, `34` FLT: Notch Phaser, `35` FLT: 303 Squelch, `36` FLT: Dual Peak, `37` FLT: Sub Focus, `38` FLT: Telephone, `39` FLT: Stereo Split, `40` FLT: Notch Canyon |
| **VOWL:** | 9 | `41` VOWL: Init, `42` VOWL: Talkbox, `43` VOWL: Choir Dual, `44` VOWL: Talking Gate, `45` VOWL: Oh-Ah Morph, `46` VOWL: Robot Throat, `47` VOWL: Acid Choir, `48` VOWL: Cyber Mouth, `49` VOWL: Nasal Scream |
| **DECY:** | 9 | `50` DECY: Init, `51` DECY: 8-Bit Arcade, `52` DECY: Lo-Fi Dust, `53` DECY: Exh Shred, `54` DECY: Gameboy Lead, `55` DECY: CRT Static, `56` DECY: Mod Bit-Rot, `57` DECY: Broken Tape, `58` DECY: 4-Bit Savage |
| **LFO:** | 9 | `59` LFO: Init, `60` LFO: Alien FM, `61` LFO: Wobble Bass, `62` LFO: Shimmer S&H, `63` LFO: Crossfade Mod, `64` LFO: Hyper Flutter, `65` LFO: Sci-Fi Siren, `66` LFO: Stepped Arps, `67` LFO: Dual Drift |
| **GATE:** | 10 | `68` GATE: Init, `69` GATE: Kick Safe, `70` GATE: Euclid 7, `71` GATE: DnB Chop, `72` GATE: Trance Arp, `73` GATE: Triplet Hop, `74` GATE: Galop Ride, `75` GATE: Trap HiHat, `76` GATE: Son Clave, `77` GATE: Glitch Stutter |
| **PUMP:** | 8 | `78` PUMP: Init, `79` PUMP: House Duck, `80` PUMP: Test Pulse 4x4, `81` PUMP: Kick Keyed LPF, `82` PUMP: Vacuum Drop, `83` PUMP: Filter Breathe, `84` PUMP: Drive Surge, `85` PUMP: French Touch |
| **COMBO / MUSH:** | 18 | `86` COMBO: Init, `87` MUSH: Full Chaos, `88` MUSH: Everything On, `89` COMBO: Sub Slam, `90` COMBO: Acid Rave, `91` COMBO: Vowel Bass, `92` COMBO: Industrial, `93` COMBO: Pump & Shred, `94` COMBO: Dub Techno, `95` COMBO: Cyber Gate, `96` COMBO: Drum Smash, `97` COMBO: Neon Synth, `98` COMBO: Warehouse, `99` COMBO: Glitch Talk, `100` COMBO: Acid Rain, `101` COMBO: Doom Drone, `102` COMBO: Vaporwave, `103` COMBO: Electro Pump |
| **L/R:** | 4 | `104` L/R: Wide Dual Tilt, `105` L/R: PingPong Formant, `106` L/R: Acid Counterpan, `107` L/R: Crunch Split |
| **M/S:** | 8 | `108` M/S: Sub Focus & Air, `109` M/S: Mid Acid Side Glow, `110` M/S: Width Breather, `111` M/S: Side Shatter, `112` M/S: Mono Kick Wide Tops, `113` M/S: Dub Tape Center, `114` M/S: Neuro Formant Orbit, `115` M/S: Cyberpunk Space Warp |
| **PAD:** | 18 | `116` PAD: Lush M/S Bloom, `117` PAD: Tape Shimmer, `118` PAD: Sidechain Cloud, `119` PAD: Formant Choir Wash, `120` PAD: Subterranean Drift, `121` PAD: Gate Breath, `122` PAD: CS-80 Horizon, `123` PAD: Juno Milky Chorus, `124` PAD: Boards VHS Drift, `125` PAD: Saw II Ambient Mist, `126` PAD: Solina String Veil, `127` PAD: Pryda 4x4 Swell, `128` PAD: D-50 Cathedral, `129` PAD: Burial Night Rain, `130` PAD: Prophet Silk Swell, `131` PAD: Angelic Vocal Cloud, `132` PAD: Orbital Mirage, `133` PAD: Hyperborean Ice |
| **SYN:** | 6 | `134` SYN: Pluck Dynamic, `135` SYN: Stereo Supersaw, `136` SYN: Acid Lead Express, `137` SYN: Neon Stutter, `138` SYN: Polyrhythm Arp, `139` SYN: Mid Punch Stack |
| **SWV:** | 6 | `140` SWV: Outrun Tape Lead, `141` SWV: Juno Chorus Wash, `142` SWV: VHS Dream Keys, `143` SWV: Neon Night Drive, `144` SWV: Retro Gate Arps, `145` SWV: CRT Cyber Bass |
| **VOX:** | 22 | `146` VOX: Air & Velvet, `147` VOX: AM Radio Megaphone, `148` VOX: Dub Throw Echo, `149` VOX: Throat Doubler, `150` VOX: Chopped Adlib, `151` VOX: Cyber Pop Pump, `152` VOX: Lead Studio Glow, `153` VOX: Tight Slap Tape, `154` VOX: Wide 80s Chorus, `155` VOX: Warm Lo-Fi Cass, `156` VOX: Crisp Pop Breath, `157` VOX: Formant Deep Shift, `158` VOX: Formant Angel High, `159` VOX: Walkie Grit Box, `160` VOX: Psychedelic Swirl, `161` VOX: Techno Hook Gate, `162` VOX: DnB Rolling Chop, `163` VOX: Sidechain Whisper, `164` VOX: Acid Choir Talk, `165` VOX: Bit-Crushed Rap Adlib, `166` VOX: Haunted Cathedral, `167` VOX: Hyper Future Vox |

---

### Appendix C: Troubleshooting

| Symptom | Likely Cause | Resolution |
|---|---|---|
| **Mushin does not appear in External Plugins** | External plugin hosting is disabled, firmware is below `3.10.1`, folder path is wrong, or drive is mounted `noexec`. | Confirm **MPC OS >= 3.10.1**, turn **On** **Preferences > Enhancements (Beta) > Enable Enhancements + External Plugin Support**, verify path `<Drive>/VST2 Plugins/PhilippeBack - VST - Mushin/`, or move to internal storage. |
| **Touchscreen tabs or skins missing** | Plugin folder renamed or **Plugin Skins** files mismatched against `plugin.json` `name`/`manufacturer`. | Restore exact folder name *`PhilippeBack - VST - Mushin`* and verify `TUI.json`, `Q-Links.json`, and all images inside *`Plugin Skins/`*. |
| **No audio passes or signal suddenly drops** | **Trance Gate** active with a silent step mask/100% **Depth** when transport is stopped, **Filter A/B** cutoff closed down at `20 Hz` LP, or **CHOKE** macro engaged near `1.00`. | Switch **Init** preset, verify **CHOKE** / **EXHAUSTION** / **GATE** switches on **MAIN**, or raise **A/B Cutoff**. |
| **Updated presets / SCOPE / INT\|4x4 buttons not visible in older song** | Old project cached earlier parameter/skin bindings on the track. | Remove Mushin from the track FX slot and re-insert **Mushin** cleanly from **External Plugins** after restarting the MPC. |

---

## Trademarks and Licenses

- **Mushin** DSP architecture and plugin port © PhilippeBack, licensed under **CC-BY-NC-ND-4.0**.
- Built using the MIT-licensed `mpc::dsp` and `framework/mpc_plugin.h` MPC Plugin Builder SDK headers (dependency-free pure C++; no proprietary JUCE, Steinberg VST SDK, or firmware assets redistributed).
- **Akai Professional**, **MPC**, and **Q-Link** are trademarks of **inMusic Brands, Inc.**, registered in the U.S. and other countries. **AIR Music Technology** and **Hybrid** are trademarks of **inMusic Brands, Inc.** All other product names and trademarks are the property of their respective owners.
