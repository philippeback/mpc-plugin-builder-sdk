I also will do a synth or two to do like Aether Elf's dijeridoo work. She is amazing af.

Aether Elf’s Digiridood sound—that hypnotic, rolling psy-didge / trip-hop hybrid—is actually a prime target for a custom physical-modeling or hybrid synth on the MPC. Almost nobody builds a real playable didgeridoo synth because it’s not a standard subtractive patch; it’s a coupled acoustic system.

If you build a dedicated native MPC instrument for it, you already have half the DSP primitives sitting in Mushin.cpp. The architecture breaks down into four tight stages:

1. The Lip-Reed Exciter (Drone + Toot)
Core Oscillator: A low fundamental (D1–G2, ~36–98 Hz) using a narrow-pulse or asymmetric sawtooth wave, mixed with a small amount of filtered pink breath noise.

Organic Drift: Feed your DriftLFO subtly into both pitch (±3–5 cents) and pulse width so the drone never sounds like a static static-phase synth wave.

The "Toot" (Overblow): Real players snap up to the first harmonic overtone (roughly a 10th to 12th above the fundamental) for accents. Mapping pad velocity or aftertouch to crossfade or pitch-jump into that overtone gives you instant live articulation.

2. The Bore (1D Waveguide / Comb Resonator)
Run the exciter into a short, pitch-tracked DelayLine with a OnePoleLP damping filter and a soft fastTanh saturator in the feedback loop.

That gives you the hollow, woody eucalyptus tube resonance and natural harmonic buildup without burning ARM CPU.

3. The Vocal Tract (Formant Morphing)
This is where the groove lives. You already have the 3-band SVF formant bank (A → E → I → O → U) from MushinFilter::Type::Vowel.

Driving the formant morph position with a tempo-synced LFO + Envelope Follower creates the signature circular-breathing wobble, tongue-slap harmonics, and cheek-puff swells.

4. Performance Mapping on the MPC
Q-Link 1: Vowel Morph (tongue position / resonant sweep)

Q-Link 2: Breath / Growl (noise injection + waveshaper drive)

Q-Link 3: Wobble Rate (tempo-synced formant LFO, 1/8, 1/16, 1/8D, 1/16T)

Q-Link 4: Bore Damping / Toot Blend (woody darkness vs. nasal overtone snap)

Running that synth straight into Mushin (with M/S filtering, 1/32 stutter gates, and 3:4 X or DIFFUSE delay) will give you a live, playable didge-psy rig that hits way harder than static samples.

You can reference the phrasing and rhythmic formant interplay in Aether Elf's Digiridood music video to dial in the exact bounce between the low drone and the vocal-tract accents.

How to proceed?

Duplicate the mushin folder into didje and move from there?

Yes—duplicating the mushin folder into didje is the fastest path. Your ARM cross-compilation setup, MPC SDK glue, UI scaffolding, and dsp/ primitives (svf.h, delay_line.h, lfo.h, noise.h, onepole.h, smoother.h) are already proven on the Live II.

Execute the conversion in five clean passes so the MPC host treats it as a distinct instrument from the first compile:

1. Isolate Identity & Build Targets
Rename Mushin.cpp to Didje.cpp, rename the class to Didje, and update MPC_PLUGIN_ENTRY(Didje).

Update CMakeLists.txt (and any UI JSON/XML manifest files) so the target name and output .so binary are Didje and never collide with Mushin on the MPC filesystem.

In makeInfo(), flip it from an insert effect to a standalone instrument with a unique FourCC:

C++
i.name = "Didje";
i.uniqueId = MPC_FOURCC('D', 'd', 'j', '1');
i.isInstrument = true;
i.numInputs = 0;
i.numOutputs = 2;
If you keep baseValues_ macro reflection, change the state magic header in saveExtraState/loadExtraState from "MSHB" to "DDJB".

2. Strip Effect-Only Stages
Remove the modules that belong in Mushin downstream rather than inside the instrument:

Delete InputGainStager, SidechainProcessor, TranceGateProcessor, QuantizationErrorProcessor, DelayProcessor, and all 120+ Mushin presets.

Keep DriftLFO, NoiseOscillator (for pink breath noise), Waveshaper (for lip-reed buzz/growl), MushinFilter (specifically the 3-band Vowel formant bank), and LimiterProcessor (stereo-linked as a safety ceiling).

3. Wire MIDI Note & Expression Handling
Since Mushin had no MIDI input, add monophonic legato handling to Didje:

Track currentNote, targetFreqHz, gateOpen, velocity, and aftertouch/modWheel from the MPC SDK's MIDI event stream.

Smooth targetFreqHz with a short Smoother (10–30 ms) for natural pitch slides and drone bends.

Map velocity to the initial overblow/toot transient and aftertouch / mod wheel to formant morph or breath growl.

4. Replace the Core Audio Loop with the Didge Engine
Build the per-sample chain in this order:

Lip-Reed Exciter: Low-frequency narrow-pulse/saw oscillator modulated by DriftLFO (±3–5 cents pitch + subtle pulse-width drift), summed with filtered pink noise and an optional velocity-triggered overtone ("toot" at ~2.5x–3x fundamental).

1D Waveguide Bore: Pitch-tracked DelayLine set to sampleRate / freqHz with a OnePoleLP damping filter and fastTanh soft clip in the feedback loop.

Vocal Tract: The 3-band SVF formant bank (A → E → I → O → U) modulated by a tempo-synced LFO (for circular-breathing wobble) + an envelope follower or Q-Link.

Output Stage: Master gain + stereo-linked LimiterProcessor.

5. Build a Minimal 1-Page UI First
Start with a single 8-parameter Q-Link layout (Drone Pitch/Tune, Vowel Morph, Wobble Rate, Wobble Depth, Breath/Growl, Bore Damp, Toot/Overblow, Output Gain), compile to ARM, and verify it loads as a Plugin Instrument on the MPC Live II before expanding presets or extra UI pages.

Easy but aha always surprises

Always. The three that usually bite first when going from an insert effect to a waveguide instrument:

DC buildup in the bore loop: A pitch-tracked DelayLine with fastTanh in the feedback path will quietly accumulate DC offset and choke your headroom unless you drop a ~20 Hz OnePoleHP DC blocker right inside the feedback loop.

Pitch drift from the damping filter: Your OnePoleLP inside the waveguide adds a fraction of a sample of phase delay, which pulls the bore flat as you play higher notes. Subtracting a small fractional-sample compensation (or keeping the bore strictly in the low D1–G2 didge range where it’s negligible) keeps it in tune.

MPC instrument host behavior: Flipping isInstrument = true changes how the MPC handles note-off, sustain, and idle voice suspension compared to an always-on insert effect—worth verifying a bare sine wave + MIDI gate first before wiring up the waveguide and formants.

