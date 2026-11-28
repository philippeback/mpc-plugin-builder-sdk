// Didje.cpp - Physical-modeling + formant-morphing didgeridoo synthesizer for Akai MPC standalone.
//
// SPDX-License-Identifier: MIT
//
// Signal flow:
//   MIDI (Mono Legato Note Stack + Velocity + Pitch Bend + Mod Wheel + Aftertouch)
//     -> Lip-Reed Exciter (Asymmetric Pulse/Saw + DriftLFO ±4 cents & PW drift
//                          + Filtered Pink Breath Noise + Velocity Overblow Toot ~2.83x
//                          + Soft Asymmetric Lip-Reed Waveshaper)
//     -> 1D Waveguide Bore (Dattorro-interpolated DelayLine with phase-delay compensation,
//                           OnePoleLP wood damping, 20 Hz OnePoleHP DC blocker, fastTanh limiter)
//     -> Vocal Tract Formant Morphing (3-Band SVF A -> E -> I -> O -> U bank
//                                      modulated by tempo-synced Wobble LFO + Envelope Follower + Aftertouch/CC1)
//     -> Stereo Body Width + Out Gain -> Stereo-Linked 2 ms Lookahead Safety Limiter -> Out

#include "mpc_plugin.h"
#include "dsp/adsr.h"
#include "dsp/delay_line.h"
#include "dsp/lfo.h"
#include "dsp/math.h"
#include "dsp/midi_cc.h"
#include "dsp/noise.h"
#include "dsp/onepole.h"
#include "dsp/oscillator.h"
#include "dsp/smoother.h"
#include "dsp/svf.h"
#include "dsp/tempo.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

namespace {

using namespace mpc::dsp;

// Dual-spline vintage analog drift generator (extracted cleanly for pitch & pulse-width organic motion)
class DriftLFO
{
public:
    void setSeed(uint32_t seed)
    {
        rng_ = Xorshift32(seed ? seed : 0xD1D3Eu);
        reseedKnots();
    }

    void setSampleRate(double sr)
    {
        sr_ = (float)std::max(sr, 1.0);
        inc_ = mpc::clamp(freq_ / sr_, 0.0f, 0.25f);
    }

    void setFrequency(float hz)
    {
        freq_ = hz;
        inc_ = mpc::clamp(freq_ / sr_, 0.0f, 0.25f);
    }

    void reset()
    {
        reseedKnots();
        fastPhase_ = 0.0f;
        slowPhase_ = 0.0f;
    }

    float process() noexcept
    {
        const float fastV = hermite(k_[0], k_[1], k_[2], k_[3], fastPhase_);
        const float slowV = hermite(sk_[0], sk_[1], sk_[2], sk_[3], slowPhase_);
        fastPhase_ += inc_;
        if (fastPhase_ >= 1.0f) {
            fastPhase_ -= 1.0f;
            advanceKnots(k_);
        }
        slowPhase_ += inc_ * 0.318309886f; // irrational 1/pi ratio prevents cyclic repeat
        if (slowPhase_ >= 1.0f) {
            slowPhase_ -= 1.0f;
            advanceKnots(sk_);
        }
        return mpc::clamp(0.72f * fastV + 0.28f * slowV, -1.0f, 1.0f);
    }

private:
    static inline float hermite(float y0, float y1, float y2, float y3, float t) noexcept
    {
        const float t2 = t * t;
        const float t3 = t2 * t;
        return 0.5f * ((2.0f * y1)
            + (-y0 + y2) * t
            + (2.0f * y0 - 5.0f * y1 + 4.0f * y2 - y3) * t2
            + (-y0 + 3.0f * y1 - 3.0f * y2 + y3) * t3);
    }

    void advanceKnots(float arr[4]) noexcept
    {
        arr[0] = arr[1];
        arr[1] = arr[2];
        arr[2] = arr[3];
        arr[3] = rng_.nextBipolar() * 0.85f;
    }

    void reseedKnots() noexcept
    {
        for (int i = 0; i < 4; ++i) {
            k_[i] = rng_.nextBipolar() * 0.85f;
            sk_[i] = rng_.nextBipolar() * 0.85f;
        }
    }

    Xorshift32 rng_{0xD1D3Eu};
    float sr_ = 44100.0f, freq_ = 0.85f, inc_ = 0.85f / 44100.0f;
    float fastPhase_ = 0.0f, slowPhase_ = 0.0f;
    float k_[4] = {}, sk_[4] = {};
};

// 3-band parallel SVF Vocal Tract formant bank (A -> E -> I -> O -> U)
class VocalTractFormants
{
public:
    void prepare(double sampleRate)
    {
        for (int i = 0; i < 3; ++i) {
            bands_[i].setSampleRate(sampleRate);
        }
        reset();
    }

    void reset()
    {
        for (int i = 0; i < 3; ++i) {
            bands_[i].reset();
        }
        lastMorph_ = -1.0f;
        lastFocus_ = -1.0f;
    }

    void update(float morph01, float focus01) noexcept
    {
        const float m = mpc::clamp(morph01, 0.0f, 1.0f);
        const float f = mpc::clamp(focus01, 0.0f, 1.0f);
        if (std::fabs(m - lastMorph_) < 1.0e-3f && std::fabs(f - lastFocus_) < 1.0e-3f) {
            return;
        }
        lastMorph_ = m;
        lastFocus_ = f;

        // Formant frequencies A -> E -> I -> O -> U
        static constexpr float kFormants[5][3] = {
            { 730.0f, 1090.0f, 2440.0f }, // A
            { 530.0f, 1840.0f, 2480.0f }, // E
            { 270.0f, 2290.0f, 3010.0f }, // I
            { 570.0f,  840.0f, 2410.0f }, // O
            { 300.0f,  870.0f, 2240.0f }, // U
        };

        const float pos = m * 4.0f;
        const int idx = mpc::clamp((int)pos, 0, 3);
        const float frac = pos - (float)idx;
        const float qRes = mpc::clamp(0.84f + f * 0.12f, 0.80f, 0.97f);

        for (int k = 0; k < 3; ++k) {
            const float fk = kFormants[idx][k] + (kFormants[idx + 1][k] - kFormants[idx][k]) * frac;
            bands_[k].set(fk, qRes);
        }
    }

    float process(float x, float formantBlend) noexcept
    {
        const float f0 = bands_[0].process(x, SVF::Mode::Bandpass);
        const float f1 = bands_[1].process(x, SVF::Mode::Bandpass);
        const float f2 = bands_[2].process(x, SVF::Mode::Bandpass);
        const float vocal = 0.50f * (1.20f * f0 + 0.85f * f1 + 0.45f * f2);
        return x * (1.0f - 0.55f * formantBlend) + vocal * (0.45f + 0.85f * formantBlend);
    }

private:
    SVF bands_[3];
    float lastMorph_ = -1.0f;
    float lastFocus_ = -1.0f;
};

// Stereo-linked 2 ms lookahead brickwall ceiling limiter
class SafetyLimiter
{
public:
    void prepare(double sampleRate)
    {
        const int lookahead = mpc::clamp((int)(0.002 * sampleRate), 1, 256);
        bufL_.assign((size_t)(lookahead + 1), 0.0f);
        bufR_.assign((size_t)(lookahead + 1), 0.0f);
        ceilGain_ = dbToGain(-0.5f);
        relCoef_ = std::exp(-1.0f / (float(std::max(sampleRate, 1.0)) * 0.080f));
        reset();
    }

    void reset()
    {
        std::fill(bufL_.begin(), bufL_.end(), 0.0f);
        std::fill(bufR_.begin(), bufR_.end(), 0.0f);
        writeIdx_ = 0;
        env_ = 0.0f;
        smoothedGR_ = 1.0f;
    }

    void processFrame(float& left, float& right) noexcept
    {
        if (bufL_.empty()) return;
        const int len = (int)bufL_.size();
        bufL_[writeIdx_] = left;
        bufR_[writeIdx_] = right;
        const int rIdx = (writeIdx_ + 1) % len;
        const float dL = bufL_[rIdx];
        const float dR = bufR_[rIdx];
        writeIdx_ = rIdx;

        const float absPeak = std::max(std::fabs(left), std::fabs(right));
        env_ = (absPeak > env_) ? absPeak : (absPeak + relCoef_ * (env_ - absPeak));
        if (env_ < 1.0e-6f) env_ = 0.0f;

        const float targetGR = (env_ > ceilGain_ && env_ > 1.0e-5f) ? (ceilGain_ / env_) : 1.0f;
        smoothedGR_ += 0.25f * (targetGR - smoothedGR_);
        if (std::isnan(smoothedGR_) || std::isinf(smoothedGR_)) smoothedGR_ = 1.0f;

        left = dL * smoothedGR_;
        right = dR * smoothedGR_;
    }

private:
    std::vector<float> bufL_, bufR_;
    int writeIdx_ = 0;
    float env_ = 0.0f;
    float smoothedGR_ = 1.0f;
    float ceilGain_ = 0.944f;
    float relCoef_ = 0.9997f;
};

} // namespace

// ---------------------------------------------------------------------------
// Didje MPC Plugin Instrument
// ---------------------------------------------------------------------------
class Didje : public mpc::Plugin
{
public:
    enum Params {
        // Parameter 0 is nudged by the MPC host on load — keep harmless master output gain
        kOutGain = 0,
        kVowelMorph,
        kBreathGrowl,
        kWobbleRate,
        kWobbleDepth,
        kDroneTune,
        kBoreDamp,
        kTootBlend,
        kGlide,
        kBodyWidth,
        kAttack,
        kRelease,
        kNumParams
    };

    static mpc::PluginInfo makeInfo()
    {
        mpc::PluginInfo i;
        i.name = "Didje";
        i.vendor = "PhilippeBack";
        i.uniqueId = MPC_FOURCC('D', 'd', 'j', '1');
        i.version = 1000;
        i.isInstrument = true;
        i.numInputs = 0;
        i.numOutputs = 2;
        i.wantsMidi = true;
        return i;
    }

    Didje() : mpc::Plugin(makeInfo())
    {
        using namespace mpc;

        // Output Gain (Parameter 0)
        addParam(floatParam("out_gain",     "Out Gain",     -24.0f, 6.0f, -3.0f, "dB", 1));
        // Primary Vocal Tract & Exciter Controls (DIDJE Row 1)
        addParam(floatParam("vowel_morph",  "Vowel Morph",    0.0f, 100.0f, 30.0f, "%", 0));
        addParam(floatParam("breath_growl", "Breath Growl",   0.0f, 100.0f, 35.0f, "%", 0));
        addParam(choiceParam("wobble_rate", "Wobble Rate",
                             {"1/2", "1/4", "1/4D", "1/8", "1/8D", "1/8T", "1/16", "1/16T", "1/32"}, 3));
        addParam(floatParam("wobble_depth", "Wobble Depth",   0.0f, 100.0f, 45.0f, "%", 0));
        // Bore Physics & Tuning (DIDJE Row 2)
        addParam(floatParam("drone_tune",   "Drone Tune",   -24.0f, 24.0f, 0.0f, "st", 1));
        addParam(floatParam("bore_damp",    "Bore Damp",      0.0f, 100.0f, 40.0f, "%", 0));
        addParam(floatParam("toot_blend",   "Toot / Overb",   0.0f, 100.0f, 35.0f, "%", 0));
        // Playability & Envelope (DIDJE Row 3)
        addParam(floatParam("glide",        "Glide",          0.0f, 200.0f, 20.0f, "ms", 0));
        addParam(floatParam("body_width",   "Body Width",     0.0f, 100.0f, 45.0f, "%", 0));
        addParam(floatParam("amp_attack",   "Attack",         2.0f, 250.0f, 18.0f, "ms", 0));
        addParam(floatParam("amp_release",  "Release",       20.0f, 450.0f, 140.0f, "ms", 0));

        if (numParams() != kNumParams) mpc::log("Didje: parameter count mismatch");

        addBuiltInPrograms();
    }

    void prepare(double sampleRate, int /*maxBlockSize*/) override
    {
        sr_ = std::max(sampleRate, 1.0);

        pulseOsc_.setSampleRate(sr_);
        pulseOsc_.setWave(Wave::Square);
        sawOsc_.setSampleRate(sr_);
        sawOsc_.setWave(Wave::Saw);
        tootOsc_.setSampleRate(sr_);
        driftLfo_.setSampleRate(sr_);
        driftLfo_.setSeed(0xD1D3Eu);
        driftLfo_.setFrequency(0.85f);

        breathFilter_.setSampleRate(sr_);
        breathFilter_.set(1100.0f, 0.35f);

        // Max bore period: ~20 Hz lowest drone -> sr / 20 + margin
        boreDelay_.prepare((int)(sr_ / 18.0) + 16);
        // Stereo body decorrelation short acoustic delay (~1.8 ms max)
        widthDelay_.prepare((int)(sr_ * 0.004) + 8);
        boreDampLp_.setCutoff(2200.0f, sr_);
        boreDcBlocker_.setCutoff(20.0f, sr_); // prevents DC accumulation inside nonlinear bore loop
        excDcBlocker_.prepare(sr_, 20.0f);    // removes asymmetric pulse & u^2 lip-reed DC before bore mix

        formants_.prepare(sr_);
        limiter_.prepare(sr_);
        ampEnv_.setSampleRate(sr_);
        wobbleLfo_.setSampleRate(sr_);

        // Env follower (~8 ms attack, 110 ms release) + toot decay (~115 ms)
        envAtkCoef_ = std::exp(-1.0f / (float(sr_) * 0.008f));
        envRelCoef_ = std::exp(-1.0f / (float(sr_) * 0.110f));
        tootDecayCoef_ = std::exp(-1.0f / (float(sr_) * 0.115f));

        noteSmoother_.setTime(20.0f, sr_);
        outGainSmoother_.setTime(15.0f, sr_);
        vowelSmoother_.setTime(20.0f, sr_);
        breathSmoother_.setTime(20.0f, sr_);
        tootSmoother_.setTime(20.0f, sr_);
        controllers_.prepare(sr_, 15.0f);

        reset();
    }

    void reset() override
    {
        heldCount_ = 0;
        currentNote_ = -1;
        velNorm_ = 0.8f;
        pitchBendSt_ = 0.0f;
        tootTransient_ = 0.0f;
        bodyEnv_ = 0.0f;

        pulseOsc_.resetPhase();
        sawOsc_.resetPhase(0.17f);
        tootOsc_.resetPhase();
        driftLfo_.reset();
        breathFilter_.reset();
        boreDelay_.reset();
        widthDelay_.reset();
        boreDampLp_.reset();
        boreDcBlocker_.reset();
        excDcBlocker_.reset();
        formants_.reset();
        limiter_.reset();
        ampEnv_.reset();
        wobbleLfo_.reset();
        controllers_.reset();

        noteSmoother_.snap(38.0f); // D2 fallback until first Note On
        outGainSmoother_.snap(dbToGain(get(kOutGain)));
        vowelSmoother_.snap(get(kVowelMorph) * 0.01f);
        breathSmoother_.snap(get(kBreathGrowl) * 0.01f);
        tootSmoother_.snap(get(kTootBlend) * 0.01f);
    }

    void process(mpc::ProcessContext& ctx) override
    {
        const int n = ctx.numFrames;
        if (n <= 0 || ctx.numOutputs < 1) return;

        float* outL = ctx.outputs[0];
        float* outR = (ctx.numOutputs > 1 && ctx.outputs[1]) ? ctx.outputs[1] : nullptr;

        // Per-block parameter update
        outGainSmoother_.setTarget(dbToGain(get(kOutGain)));
        vowelSmoother_.setTarget(get(kVowelMorph) * 0.01f);
        breathSmoother_.setTarget(get(kBreathGrowl) * 0.01f);
        tootSmoother_.setTarget(get(kTootBlend) * 0.01f);

        const float tuneSt = get(kDroneTune);
        const float wobbleDepth01 = get(kWobbleDepth) * 0.01f;
        const float boreDamp01 = get(kBoreDamp) * 0.01f;
        const float width01 = get(kBodyWidth) * 0.01f;
        const float glideMs = get(kGlide);

        noteSmoother_.setTime(std::max(1.0f, glideMs), sr_);
        ampEnv_.set(get(kAttack), 80.0f, 1.0f, get(kRelease));

        // Exponential bore low-pass cutoff (6500 Hz bright wood -> 550 Hz dark woody tube)
        const float dampHz = 6500.0f * std::pow(0.0846f, boreDamp01);
        boreDampLp_.setCutoff(dampHz, sr_);
        const float lpAlpha = clamp(1.0f - std::exp(-2.0f * 3.14159265f * dampHz / (float)sr_), 0.02f, 1.0f);
        const float lpPhaseDelaySamples = clamp((1.0f - lpAlpha) / lpAlpha, 0.0f, 12.0f);
        const float boreFbGain = 0.82f - 0.14f * boreDamp01;

        // Tempo-synced formant wobble LFO (1/2 .. 1/32, aligned with host PPQ when playing)
        static constexpr float kWobbleBeats[9] = {
            2.0f, 1.0f, 1.5f, 0.5f, 0.75f, 0.3333333f, 0.25f, 0.1666667f, 0.125f
        };
        const int wobbleIdx = mpc::clamp(getInt(kWobbleRate), 0, 8);
        const float wobbleBeats = kWobbleBeats[wobbleIdx];
        wobbleLfo_.setShape(LFO::Shape::Sine);
        wobbleLfo_.setTempoSync(ctx.transport.bpm, wobbleBeats);
        if (ctx.transport.playing && ctx.transport.ppqValid) {
            wobbleLfo_.syncToPpq(ctx.transport.ppqPos);
        }

        // Sample-accurate MIDI dispatch interleaved with audio rendering
        int midiIdx = 0;
        const float widthDelaySamples = clamp(0.00055f * (float)sr_ * (0.3f + 0.7f * width01), 1.0f, (float)widthDelay_.maxDelay());

        for (int s = 0; s < n; ++s) {
            while (midiIdx < ctx.numMidiEvents && ctx.midi[midiIdx].sampleOffset <= s) {
                handleMidi(ctx.midi[midiIdx++], glideMs);
            }

            controllers_.tick();
            const float outGain = outGainSmoother_.process();
            const float baseVowel = vowelSmoother_.process();
            const float baseBreath = breathSmoother_.process();
            const float baseToot = tootSmoother_.process();

            const float amp = ampEnv_.process();
            if (!ampEnv_.isActive()) {
                // Silent idle state -> keep output pure zero (needed for host idle / silence check)
                tootTransient_ = 0.0f;
                bodyEnv_ *= 0.99f;
                boreDelay_.write(0.0f);
                widthDelay_.write(0.0f);
                float L = 0.0f, R = 0.0f;
                limiter_.processFrame(L, R);
                outL[s] = L;
                if (outR) outR[s] = R;
                continue;
            }

            // Control-rate pitch & filter updates every 16 samples
            const float glideNote = noteSmoother_.process();
            const float drift = driftLfo_.process();
            const float wobbleVal = wobbleLfo_.process(); // -1..1

            if ((s & 15) == 0) {
                const float rawHz = mpc::noteToHz(glideNote + tuneSt + pitchBendSt_);
                const float droneHz = mpc::clamp(rawHz * (1.0f + drift * 0.0023f), 24.0f, 540.0f);
                borePeriodSamples_ = std::max(2.0f, ((float)sr_ / droneHz) - lpPhaseDelaySamples);

                pulseOsc_.setFrequency(droneHz);
                sawOsc_.setFrequency(droneHz);
                curPulsePw_ = mpc::clamp(0.18f + 0.055f * drift, 0.08f, 0.30f);
                pulseOsc_.setPulseWidth(curPulsePw_);

                // Classic didgeridoo blown overtone ("toot") around a 10th-11th above fundamental
                tootOsc_.setFrequency(mpc::clamp(droneHz * 2.828f, 40.0f, 2200.0f));

                // Circular-breathing formant sweep: manual base + LFO + envelope follower + aftertouch/CC1
                const float exprBoost = 0.35f * controllers_.modWheel() + 0.40f * controllers_.aftertouch();
                const float lfoSweep = wobbleDepth01 * (0.45f * wobbleVal + 0.25f * bodyEnv_);
                const float morphPos = mpc::clamp(baseVowel + lfoSweep + exprBoost, 0.0f, 1.0f);
                const float focus = mpc::clamp(0.42f + 0.45f * baseBreath + 0.25f * wobbleDepth01, 0.0f, 1.0f);
                formants_.update(morphPos, focus);
            }

            // Stage 1: Lip-Reed Exciter (DC-compensated narrow pulse + saw + filtered pink breath + toot)
            const float breathAmt = mpc::clamp(baseBreath + 0.35f * controllers_.aftertouch(), 0.0f, 1.0f);
            const float dcFreePulse = pulseOsc_.process() - (2.0f * curPulsePw_ - 1.0f);
            const float rawExc = 0.56f * dcFreePulse + 0.44f * sawOsc_.process();
            const float breathNz = breathFilter_.process(pink_.process(), SVF::Mode::Bandpass) * (0.10f + 0.55f * breathAmt);

            tootTransient_ *= tootDecayCoef_;
            const float tootAmt = mpc::clamp(baseToot * 0.45f + tootTransient_ * velNorm_ * (0.25f + 0.85f * baseToot), 0.0f, 1.0f);
            const float tootWave = tootOsc_.process() * tootAmt * 0.65f;

            // Asymmetric lip-reed growl saturation + 20 Hz DC block
            const float lipDrive = 1.15f + 2.25f * breathAmt;
            const float u = (rawExc + breathNz + tootWave) * lipDrive;
            const float exciter = excDcBlocker_.process(fastTanh(u + 0.16f * u * u));

            // Stage 2: 1D Waveguide Bore (DelayLine + LP damp + 20 Hz DC blocker + fastTanh)
            const float boreTap = boreDelay_.readAllpass(borePeriodSamples_);
            const float damped = boreDampLp_.process(boreTap);
            const float dcClean = boreDcBlocker_.process(damped);
            const float loopSample = fastTanh((exciter * 0.44f + dcClean * boreFbGain));
            boreDelay_.write(loopSample);

            const float boreOut = 0.38f * exciter + 0.72f * dcClean;

            // Update dynamic formant envelope follower from waveguide body energy
            const float boreAbs = std::fabs(boreOut);
            bodyEnv_ = (boreAbs > bodyEnv_)
                ? (envAtkCoef_ * (bodyEnv_ - boreAbs) + boreAbs)
                : (envRelCoef_ * (bodyEnv_ - boreAbs) + boreAbs);

            // Stage 3: Vocal Tract Formant Morphing + slight cheek-puff AM
            const float puffGain = 1.0f + wobbleDepth01 * 0.16f * wobbleVal;
            const float vocalMono = formants_.process(boreOut, 0.55f + 0.45f * wobbleDepth01) * puffGain;

            // Stage 4: Stereo acoustic width, amplitude gate, master gain, and lookahead safety ceiling
            const float shaped = vocalMono * 0.52f * amp * (0.65f + 0.35f * velNorm_) * outGain;
            const float delayedSide = widthDelay_.process(shaped, widthDelaySamples);
            const float widthSpread = 0.35f * width01;
            float L = shaped + widthSpread * (delayedSide - shaped * 0.6f);
            float R = shaped - widthSpread * (delayedSide - shaped * 0.6f);

            limiter_.processFrame(L, R);
            if (std::isnan(L) || std::isinf(L)) L = 0.0f;
            if (std::isnan(R) || std::isinf(R)) R = 0.0f;
            outL[s] = L;
            if (outR) outR[s] = R;
        }

        // Consume any trailing events stamped at/after block end
        while (midiIdx < ctx.numMidiEvents) {
            handleMidi(ctx.midi[midiIdx++], glideMs);
        }
    }

private:
    static constexpr int kMaxHeld = 16;

    void handleMidi(const mpc::MidiEvent& ev, float glideMs) noexcept
    {
        if (ev.isNoteOn()) {
            const int note = ev.note();
            const float vel = mpc::clamp((float)ev.velocity() / 127.0f, 0.05f, 1.0f);
            const bool freshAttack = (heldCount_ == 0 && !ampEnv_.isActive());

            // Push onto held mono legato stack (deduplicate if re-triggered)
            removeHeld(note);
            if (heldCount_ < kMaxHeld) {
                heldNotes_[heldCount_++] = note;
            } else {
                for (int i = 1; i < kMaxHeld; ++i) heldNotes_[i - 1] = heldNotes_[i];
                heldNotes_[kMaxHeld - 1] = note;
            }

            currentNote_ = note;
            velNorm_ = vel;
            if (freshAttack || glideMs <= 1.0f) {
                noteSmoother_.snap((float)note);
                pulseOsc_.resetPhase();
                sawOsc_.resetPhase(0.17f);
                tootOsc_.resetPhase();
                boreDcBlocker_.reset();
                excDcBlocker_.reset();
            } else {
                noteSmoother_.setTarget((float)note);
            }

            tootTransient_ = std::max(tootTransient_, vel);
            ampEnv_.noteOn();
        } else if (ev.isNoteOff()) {
            removeHeld(ev.note());
            if (heldCount_ > 0) {
                currentNote_ = heldNotes_[heldCount_ - 1];
                noteSmoother_.setTarget((float)currentNote_);
            } else {
                ampEnv_.noteOff();
            }
        } else if (ev.isAllNotesOff()) {
            heldCount_ = 0;
            ampEnv_.noteOff();
        } else if (ev.isPitchBend()) {
            pitchBendSt_ = ((float)ev.pitchBend() / 8192.0f) * 2.0f; // +/-2 semitones
        } else {
            controllers_.handle(ev);
        }
    }

    void removeHeld(int note) noexcept
    {
        for (int i = 0; i < heldCount_; ++i) {
            if (heldNotes_[i] == note) {
                for (int j = i + 1; j < heldCount_; ++j) heldNotes_[j - 1] = heldNotes_[j];
                --heldCount_;
                return;
            }
        }
    }

    void addPreset(const char* name, std::initializer_list<std::pair<int, float>> overrides)
    {
        std::vector<float> v((size_t)numParams());
        for (int i = 0; i < numParams(); ++i) v[(size_t)i] = param(i).def;
        for (const auto& o : overrides) {
            if (o.first >= 0 && o.first < numParams()) v[(size_t)o.first] = o.second;
        }
        addProgram(name, std::move(v));
    }

    void addBuiltInPrograms()
    {
        addPreset("Earth Drone", {});

        addPreset("Aether Digiridood", {
            {kVowelMorph, 38.0f}, {kBreathGrowl, 45.0f}, {kWobbleRate, 4.0f}, {kWobbleDepth, 62.0f},
            {kDroneTune, 0.0f}, {kBoreDamp, 35.0f}, {kTootBlend, 42.0f}, {kBodyWidth, 60.0f}
        });

        addPreset("Hollow Eucalyptus", {
            {kVowelMorph, 72.0f}, {kBreathGrowl, 22.0f}, {kWobbleRate, 1.0f}, {kWobbleDepth, 32.0f},
            {kBoreDamp, 72.0f}, {kTootBlend, 18.0f}, {kBodyWidth, 40.0f}, {kAttack, 45.0f}, {kRelease, 220.0f}
        });

        addPreset("Overblow Toot", {
            {kVowelMorph, 18.0f}, {kBreathGrowl, 42.0f}, {kWobbleRate, 6.0f}, {kWobbleDepth, 38.0f},
            {kBoreDamp, 16.0f}, {kTootBlend, 78.0f}, {kGlide, 12.0f}, {kAttack, 6.0f}
        });

        addPreset("Psy Acid Didge", {
            {kVowelMorph, 46.0f}, {kBreathGrowl, 68.0f}, {kWobbleRate, 7.0f}, {kWobbleDepth, 75.0f},
            {kDroneTune, -12.0f}, {kBoreDamp, 28.0f}, {kTootBlend, 48.0f}, {kBodyWidth, 65.0f}
        });
    }

    double sr_ = 44100.0;

    // Synthesis modules
    PolyBlepOsc pulseOsc_, sawOsc_;
    WavetableSine tootOsc_;
    DriftLFO driftLfo_;
    PinkNoise pink_{0x2468ACE0u};
    SVF breathFilter_;
    DelayLine boreDelay_, widthDelay_;
    OnePoleLP boreDampLp_;
    OnePoleHP boreDcBlocker_;
    DCBlocker excDcBlocker_;
    VocalTractFormants formants_;
    SafetyLimiter limiter_;
    ADSR ampEnv_;
    LFO wobbleLfo_;
    MidiControllers controllers_;

    // Smoothers
    Smoother noteSmoother_, outGainSmoother_, vowelSmoother_, breathSmoother_, tootSmoother_;

    // Voice & modulation runtime state
    int heldNotes_[kMaxHeld] = {};
    int heldCount_ = 0;
    int currentNote_ = -1;
    float velNorm_ = 0.8f;
    float pitchBendSt_ = 0.0f;
    float curPulsePw_ = 0.18f;
    float borePeriodSamples_ = 600.0f;
    float tootTransient_ = 0.0f;
    float tootDecayCoef_ = 0.9998f;
    float bodyEnv_ = 0.0f;
    float envAtkCoef_ = 0.997f;
    float envRelCoef_ = 0.9998f;
};

MPC_PLUGIN_ENTRY(Didje)
