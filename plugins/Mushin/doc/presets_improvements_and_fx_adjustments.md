Presets need some honing but not much

Looking through addBuiltInPrograms() in Mushin.cpp, your 96 built-in programs are already structurally cohesive—the category prefixes (SAT:, FLT:, VOWL:, DECY:, LFO:, GATE:, PUMP:, COMBO:, MUSH:) make browsing on the MPC Live II screen effortless, and you already caught the biggest trap in the PUMP: bank (keeping Limiter Drive off so it doesn't squash the sidechain ducking).

When you do that final pass to hone them, three surgical tweaks across the preset table will make every factory patch immediately playable without surprise volume jumps or dead macros:

Gain-Staging & Auto-Gain Consistency: Several FLT:, VOWL:, and GATE: presets leave kScvActive off (0.0f), which is great for clean CPU usage, while SAT: and COMBO: presets engage kScvActive with kDrive between 4.0 and 15.0 and kAutoGain on. Because Waveshaper::processSample computes gainComp from inputPeak_ (which decays at 0.1 s), transients right after silence can still spike before the peak follower settles—adding a clean kLimActive = 1.0f (CLEAN mode, 0.0 dB drive, -0.3 dB ceiling) on the high-resonance FLT: and VOWL: patches (303 Squelch, Nasal Scream, Acid Choir) will protect your monitors when sweeping resonance above 0.80 on standalone hardware.

Macro Headroom on Heavy COMBO: / MUSH: Patches: In patches like MUSH: Full Chaos or MUSH: Everything On, parameters like kDrive (6.5), kQeMix (0.55), and kLimDrive (8.0) are already pushed fairly high in baseValues_. Because applyMacros() adds positive offsets (+12.0f * mExh to Drive, +18.0f * mSmash to Limiter Drive), dialing those presets back by ~15–20% in their resting state leaves the 8 Q-Link macros plenty of upward travel before hitting the parameter ceilings (20.0x Drive / 24 dB Limiter Drive).

Dedicated DARK / PSY / ACID Performance Bank: Your COMBO: bank already leans nicely toward psytrance, industrial, and acid (Acid Rave, Doom Drone, Warehouse, Electro Pump). Grouping or tagging 6–8 of your favorite track-specific starters (e.g., bassline squelch, 148 BPM gate carve, FM lead scream, master glue) near the top right after Init saves scrolling past 80 entries during a live jam on the MPC encoder.

Yes mostly dialing things back and also adding more psy chains.

Dialing back the resting levels on the heavy COMBO:/MUSH: patches gives the 8 Q-Link macros a much wider sweet spot before hitting the ceiling, while dedicated PSY: chains give you instant 148 BPM darkpsy/psytrance starting points right out of the browser.

Here are six ready-to-drop PSY: preset definitions calibrated with ~25–35% macro headroom (so CHOKE, KINETIC MOD, EXHAUSTION, and RESONATOR all have room to sweep upward without pegging), plus the dialed-back adjustments for MUSH: Full Chaos and MUSH: Everything On:

1. New PSY: Preset Bank (addBuiltInPrograms())
C++
        // PSY: Psytrance & Darkpsy Chains (with Q-Link Macro Headroom)
        addPreset("PSY: Forest FM Lead", {
            {kScvActive, 1.0f}, {kDrive, 2.8f}, {kThreshold, -4.0f}, {kAutoGain, 1.0f},
            {kNoiseActive, 1.0f}, {kNoiseType, 2.0f}, {kNoiseFreq, 185.0f}, {kNoiseLevel, -26.0f}, {kNoiseFmMod, 0.38f},
            {kFAType, 2.0f}, {kFAMode, 2.0f}, {kFACutoff, 1350.0f}, {kFAReso, 0.62f}, {kFADrive, 1.8f},
            {kLfo1Freq, 3.8f}, {kLfo1Wave, 4.0f}, {kModL1T1, 0.42f},
            {kLfo2Freq, 0.35f}, {kLfo2Wave, 0.0f}, {kModL2T2, 0.22f},
            {kDelayActive, 1.0f}, {kDelaySync, 1.0f}, {kDelayTempo, 0.0f}, {kDelayPingPong, 1.0f},
            {kDelayFeedback, 0.42f}, {kDelayMix, 0.26f},
            {kLimActive, 1.0f}, {kLimMode, 0.0f}, {kLimDrive, 2.0f}, {kLimCeiling, -0.4f}
        });
        addPreset("PSY: Rolling Bass 16", {
            {kScvActive, 1.0f}, {kDrive, 3.2f}, {kThreshold, -3.5f}, {kAutoGain, 1.0f},
            {kFAType, 2.0f}, {kFAMode, 0.0f}, {kFACutoff, 480.0f}, {kFAReso, 0.48f}, {kFADrive, 1.6f},
            {kFBType, 0.0f}, {kFBMode, 1.0f}, {kFBCutoff, 42.0f}, {kFBReso, 0.25f},
            {kScActive, 1.0f}, {kScSource, 1.0f}, {kScMode, 0.0f}, {kScRatio, 12.0f},
            {kScThresh, -26.0f}, {kScAmount, -85.0f}, {kScAttack, 1.0f}, {kScRelease, 135.0f},
            {kScTargetGain, 1.0f}, {kScTargetCut, 1.0f},
            {kLimActive, 1.0f}, {kLimMode, 0.0f}, {kLimDrive, 2.5f}, {kLimCeiling, -0.3f}
        });
        addPreset("PSY: Dark Squelch", {
            {kRouting, 1.0f}, {kScvActive, 1.0f}, {kDrive, 3.6f}, {kThreshold, -5.0f}, {kAutoGain, 1.0f},
            {kFAType, 2.0f}, {kFAMode, 0.0f}, {kFACutoff, 720.0f}, {kFAReso, 0.72f}, {kFADrive, 2.2f}, {kFAGrit, 0.18f},
            {kFBType, 1.0f}, {kFBMode, 2.0f}, {kFBCutoff, 1450.0f}, {kFBReso, 0.60f},
            {kLfo1Freq, 1.25f}, {kLfo1Wave, 2.0f}, {kModL1T1, 0.48f},
            {kLfo2Freq, 0.45f}, {kLfo2Wave, 0.0f}, {kModL2T4, -0.45f},
            {kLimActive, 1.0f}, {kLimMode, 0.0f}, {kLimDrive, 1.5f}, {kLimCeiling, -0.4f}
        });
        addPreset("PSY: Gate Grid 148", {
            {kScvActive, 1.0f}, {kDrive, 2.4f}, {kAutoGain, 1.0f},
            {kFAType, 2.0f}, {kFAMode, 2.0f}, {kFACutoff, 1900.0f}, {kFAReso, 0.52f},
            {kLfo1Freq, 0.60f}, {kModL1T1, 0.35f},
            {kTgActive, 1.0f}, {kTgPattern, 26.0f}, {kTgRate, 0.0f}, {kTgOffset, 12.0f},
            {kTgDepth, 82.0f}, {kTgStart, 3.0f}, {kTgHold, 55.0f}, {kTgEnd, 14.0f},
            {kDelayActive, 1.0f}, {kDelaySync, 1.0f}, {kDelayTempo, 0.0f}, {kDelayPingPong, 1.0f},
            {kDelayFeedback, 0.38f}, {kDelayMix, 0.24f},
            {kLimActive, 1.0f}, {kLimMode, 0.0f}, {kLimDrive, 1.5f}, {kLimCeiling, -0.3f}
        });
        addPreset("PSY: Alien Formant", {
            {kRouting, 0.0f}, {kScvActive, 1.0f}, {kDrive, 3.0f}, {kThreshold, -4.5f}, {kAutoGain, 1.0f},
            {kFAType, 1.0f}, {kFAMode, 2.0f}, {kFACutoff, 860.0f}, {kFAReso, 0.64f}, {kFADrive, 1.7f}, {kFAGrit, 0.15f},
            {kFBType, 2.0f}, {kFBMode, 3.0f}, {kFBCutoff, 2100.0f}, {kFBReso, 0.58f},
            {kLfo1Freq, 2.4f}, {kLfo1Wave, 4.0f}, {kModL1T1, 0.44f},
            {kLfo2Freq, 0.42f}, {kLfo2Wave, 1.0f}, {kModL2T4, -0.50f},
            {kDelayActive, 1.0f}, {kDelaySync, 1.0f}, {kDelayTempo, 0.0f}, {kDelayPingPong, 1.0f},
            {kDelayFeedback, 0.40f}, {kDelayMix, 0.25f},
            {kLimActive, 1.0f}, {kLimMode, 0.0f}, {kLimDrive, 1.5f}, {kLimCeiling, -0.4f}
        });
        addPreset("PSY: Night Riser", {
            {kRouting, 0.0f}, {kScvActive, 1.0f}, {kDrive, 2.6f}, {kAutoGain, 1.0f},
            {kNoiseActive, 1.0f}, {kNoiseType, 0.0f}, {kNoiseRoute, 1.0f}, {kNoiseLevel, -34.0f}, {kNoiseFmMod, 0.20f},
            {kFAType, 2.0f}, {kFAMode, 0.0f}, {kFACutoff, 4200.0f}, {kFAReso, 0.45f},
            {kFBType, 0.0f}, {kFBMode, 1.0f}, {kFBCutoff, 140.0f}, {kFBReso, 0.35f},
            {kLfo1Freq, 0.50f}, {kModL1T1, 0.30f},
            {kDelayActive, 1.0f}, {kDelayTime, 28.0f}, {kDelayFeedback, 0.55f}, {kDelayMix, 0.22f},
            {kLimActive, 1.0f}, {kLimMode, 0.0f}, {kLimDrive, 2.0f}, {kLimCeiling, -0.3f}
        });
2. Dialed-Back MUSH: Heavy Hitters (Leaving Macro Travel)
In MUSH: Full Chaos and MUSH: Everything On, pulling the resting drive, bit-crush mix, and limiter drive back keeps the signature character intact while giving EXHAUSTION, INFO DECAY, RESONATOR, and SMASH a full sweep range:

C++
        addPreset("MUSH: Full Chaos", {
            {kScvActive, 1.0f}, {kDrive, 4.2f}, {kExhaustion, 0.0f}, {kThreshold, -6.0f}, {kAutoGain, 1.0f},
            {kFAType, 1.0f}, {kFAMode, 2.0f}, {kFACutoff, 1200.0f}, {kFAReso, 0.52f},
            {kFBType, 2.0f}, {kFBMode, 1.0f}, {kFBCutoff, 280.0f}, {kFBReso, 0.38f},
            {kLfo1Freq, 0.65f}, {kModL1T1, 0.40f}, {kLfo2Freq, 1.10f}, {kModL2T4, 0.35f},
            {kQeActive, 1.0f}, {kQeDepth, 7.0f}, {kQeDownsample, 4.0f}, {kQeMix, 0.35f},
            {kTgActive, 1.0f}, {kTgPattern, 80.0f}, {kTgDepth, 72.0f},
            {kDelayActive, 1.0f}, {kDelaySync, 1.0f}, {kDelayTempo, 0.0f}, {kDelayPingPong, 1.0f},
            {kDelayFeedback, 0.32f}, {kDelayMix, 0.22f},
            {kLimActive, 1.0f}, {kLimMode, 0.0f}, {kLimDrive, 3.5f}, {kLimCeiling, -0.5f}
        });
C++
        addPreset("MUSH: Everything On", {
            {kScvActive, 1.0f}, {kDrive, 3.0f}, {kExhaustion, 0.0f}, {kThreshold, -4.5f}, {kAutoGain, 1.0f},
            {kRouting, 1.0f}, {kFAType, 1.0f}, {kFAMode, 2.0f}, {kFACutoff, 1050.0f}, {kFAReso, 0.50f}, {kFAGrit, 0.15f},
            {kFBType, 2.0f}, {kFBMode, 0.0f}, {kFBCutoff, 2600.0f}, {kFBReso, 0.45f},
            {kLfo1Freq, 0.50f}, {kModL1T1, 0.38f}, {kLfo2Freq, 0.80f}, {kModL2T4, -0.35f},
            {kNoiseActive, 1.0f}, {kNoiseType, 1.0f}, {kNoiseRoute, 1.0f}, {kNoiseLevel, -36.0f}, {kNoiseFmMod, 0.18f},
            {kQeActive, 1.0f}, {kQeDepth, 8.5f}, {kQeDownsample, 2.0f}, {kQeMix, 0.28f},
            {kTgActive, 1.0f}, {kTgPattern, 77.0f}, {kTgOffset, 15.0f}, {kTgDepth, 68.0f},
            {kScActive, 1.0f}, {kScMode, 1.0f}, {kScRatio, 8.0f}, {kScThresh, -28.0f}, {kScAmount, -75.0f},
            {kScTargetGain, 1.0f}, {kScTargetCut, 1.0f},
            {kDelayActive, 1.0f}, {kDelaySync, 1.0f}, {kDelayTempo, 0.0f}, {kDelayPingPong, 1.0f},
            {kDelayFeedback, 0.35f}, {kDelayMix, 0.22f},
            {kLimActive, 1.0f}, {kLimMode, 0.0f}, {kLimDrive, 2.0f}, {kLimCeiling, -0.5f}
        });
Placing the PSY: block right after the top 6 signature presets puts all six psytrance/darkpsy chains within a single flick of the MPC data dial from Init.

I should put an init patch before each section like PSY Init

Putting an Init patch at the head of each section is a smart ergonomic move for the MPC Live II browser. Beyond acting as visual section dividers when spinning the data wheel, domain-prepped Init templates pre-configure the routing and safety blocks for that workflow so you start designing immediately without five pages of setup taps.

Here is how each section Init template can be pre-wired in addBuiltInPrograms():

SAT: Init — S-Curve active (Drive 1.0, Auto Gain On), Limiter active (CLEAN, -0.3 dB ceiling), everything else bypassed so you can immediately sweep Drive and Threshold.

FLT: Init — Filter A and Filter B active in SERIAL, LFO 1 & 2 pre-assigned lightly (0.0 depth or ready on A Cut / B Cut), Limiter active (CLEAN, -0.3 dB) to catch high-resonance sweeps.

VOWL: Init — Filter A set to VOWL (Bandpass, 900 Hz, Reso 0.55), LFO 1 at 0.5 Hz (Mod A Cut 0.35), Limiter active (CLEAN, -0.3 dB) so a loop talks the second you load it.

DECY: Init — Info Decay active (16 bit, 1x downsample, Mix 1.0) with S-Curve active (Auto Gain On), ready to turn Bit Depth and Downsample immediately.

LFO: Init — LFO 1 (1.0 Hz Sine) and LFO 2 (0.33 Hz Triangle) pre-patched to Filter A & B Cutoffs (0.25 / -0.25) with Filter A at 2400 Hz (Reso 0.35) so modulation is audible right away.

GATE: Init — Trance Gate active (01 Straight 16, 1/16, Depth 80%, Hold 55%, Mix 1.0), ready to scroll patterns and tg_offset.

PUMP: Init — Sidechain active (INT, RMS, Ratio 10:1, Thresh -28 dB, Amount -80%, LPF 350 Hz, SC Gain On) with Limiter off so ducking is clean out of the box.

PSY: Init — S-Curve active (Drive 2.0, Auto Gain On), Filter A ACID LP (2200 Hz, Reso 0.45), Filter B CLN HP (42 Hz sub-rumble guard), Delay active (1/16 Sync Ping-Pong, Mix 0.18), and Limiter active (CLEAN, -0.3 dB).

COMBO: Init / MUSH: Init — Clean multi-stage scaffold (S-Curve + Serial Filters + Sync Delay + Limiter at neutral settings) with maximum headroom for the 8 Q-Link performance macros.

C++
        addPreset("PSY: Init", {
            {kScvActive, 1.0f}, {kDrive, 2.0f}, {kAutoGain, 1.0f},
            {kFAType, 2.0f}, {kFAMode, 0.0f}, {kFACutoff, 2200.0f}, {kFAReso, 0.45f},
            {kFBType, 0.0f}, {kFBMode, 1.0f}, {kFBCutoff, 42.0f}, {kFBReso, 0.20f},
            {kDelayActive, 1.0f}, {kDelaySync, 1.0f}, {kDelayTempo, 0.0f}, {kDelayPingPong, 1.0f},
            {kDelayFeedback, 0.32f}, {kDelayMix, 0.18f},
            {kLimActive, 1.0f}, {kLimMode, 0.0f}, {kLimDrive, 0.0f}, {kLimCeiling, -0.3f}
        });
One detail to watch in addPreset(): it copies get(i) from the live plugin state (which starts at Init), so every section Init only needs to specify its active overrides and remains completely isolated from the presets before or after it.

Some presets are cool but drive is too much across the board no?

You are spot on—the drive is stacked way too hot across the preset library, and looking directly at the DSP math in Mushin.cpp explains why it compounds so aggressively on real loops:

Why the Drive Compounds Faster Than the Knob Numbers Suggest
The S-Curve Saturation Ramp Hits 100% by Drive = 1.25x:
Look at Waveshaper::processSample():

C++
const float satMix = clamp(std::max((currentDrive - 1.0f) * 4.0f, (1.0f - currentThreshold) * 4.0f), 0.0f, 1.0f);
Because of that * 4.0f multiplier, satMix is already 100% wet at Drive = 1.25x (or Threshold = -2.5 dB). Anything above 1.25x—and most of your presets sit between 3.0x and 9.0x—isn't blending warmth anymore; it is slamming full-scale fastTanh(x / denom) into the filters.

Four Separate Gain/Drive Stages Stack in Series:

Stage 1 (Waveshaper): kDrive (2.5x–9.0x) + kAutoGain (which boosts low-level tails back up toward unity).

Stage 2 (Filter A): kFADrive (1.8x–3.5x), plus an automatic 1.4x multiplier whenever kFAType == Acid (2.0f)—meaning kFADrive = 3.0 on an Acid filter is actually 4.2x pre-filter drive (preMix = 1.0 full fastTanh clipping) before the resonant SVF even sees the signal.

Stage 3 (Filter B): kFBDrive (1.8x–2.5x) + post-filter fastTanh(out * (1.0f + grit_)).

Stage 4 (Limiter): kLimDrive (6.0–11.0 dB) in LMC mode on top of an already clipped signal.

Zero Headroom Left for EXHAUSTION and SMASH Macros:
Because applyMacros() adds +12.0f * mExh to kDrive and +18.0f * mSmash to kLimDrive, starting a preset at Drive 5.5 + LimDrive 8.0 means turning the Q-Link macro past 30% just pins the waveform into a square wave instead of giving you a progressive musical build.

Practical Calibration Targets for the Preset Pass
Subtle / Glue / Motion Presets (FLT:, VOWL:, LFO:, GATE:, PUMP:): Keep kDrive at 1.05x–1.4x (right inside the satMix transition zone where the S-curve gently rounds transients), kFADrive / kFBDrive at 1.0x–1.3x, and kLimDrive at 0.0–1.5 dB (CLEAN mode).

Psytrance / Acid / Character Presets (PSY:, COMBO:): Set kDrive to 1.5x–2.5x, kFADrive to 1.2x–1.6x (remembering ACID multiplies that by 1.4x under the hood), and kLimDrive to 1.5–3.5 dB.

Dedicated Distortion / Destruction Presets (SAT: Hard Clip, SAT: Fuzz Wall, SAT: Burned Coil, COMBO: Industrial): Reserve 4.0x–8.0x kDrive and Exhaustion = On strictly for these presets so they actually stand apart from the rest of the bank.




