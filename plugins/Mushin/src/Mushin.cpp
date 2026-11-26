// Mushin.cpp - MPC standalone port of the Mushin multi-stage effect plugin.
//
// Signal path:
//   in -> Input Gain Stager (Auto-Level + Trim) -> [Pre-Dist Noise]
//      -> S-Curve Waveshaper <-> Dual Filter (Serial/Parallel/Stereo/M-S, Flt->Sat reversible)
//      -> [Pre-Filter / Post-Filter Noise] -> Info Decay (Bit/Downsample)
//      -> Dry/Wet Mix -> Trance Gate -> Final Gain -> Stereo Delay -> Final Limiter -> out
//
// Envelope Follower modulates Filter Cutoff & Drive from input dynamics.
// Sidechain envelope follower (External or Internal 4x4) modulates Drive, Filter Cutoff, or Final Gain.

#include "mpc_plugin.h"
#include "dsp/delay_line.h"
#include "dsp/lfo.h"
#include "dsp/math.h"
#include "dsp/noise.h"
#include "dsp/onepole.h"
#include "dsp/smoother.h"
#include "dsp/svf.h"
#include "dsp/tempo.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <vector>

namespace {

using namespace mpc::dsp;

// ---------------------------------------------------------------------------
// Input Gain Stager (Auto-Leveler + Manual Pre-FX Trim)
// ---------------------------------------------------------------------------
class InputGainStager
{
public:
    void prepare(double sampleRate)
    {
        sampleRate_ = sampleRate;
        attackCoef_ = std::exp(-1.0f / (float(sampleRate) * 0.001f));    // 1 ms instant peak catch (no overshoot)
        releaseCoef_ = std::exp(-1.0f / (float(sampleRate) * 0.450f));   // 450 ms smooth program release
        gainSmoothCoef_ = std::exp(-1.0f / (float(sampleRate) * 0.035f)); // 35 ms click-free gain glide
        settleTargetSamples_ = std::max(1, (int)(sampleRate * 0.015));   // 15 ms active settle before upward boost
        trimSmoother_.setTime(50.0f, sampleRate);
        reset();
    }

    void reset()
    {
        env_ = 0.0f;
        settleCount_ = 0;
        desiredAutoGain_ = 1.0f;
        currentAutoGain_ = 1.0f;
        trimSmoother_.snap(1.0f);
    }

    void setTargetDb(float db) { targetGain_ = dbToGain(clamp(db, -24.0f, 0.0f)); }
    void setTrimDb(float db) { trimSmoother_.setTarget(dbToGain(clamp(db, -18.0f, 24.0f))); }
    void snapTrim(float db) { trimSmoother_.snap(dbToGain(clamp(db, -18.0f, 24.0f))); }

    void processSample(float& left, float& right, bool autoEnabled) noexcept
    {
        const float trim = trimSmoother_.process();
        const float peak = std::max(std::fabs(left), std::fabs(right));

        if (!autoEnabled) {
            desiredAutoGain_ = 1.0f;
            settleCount_ = 0;
        } else if (peak > 0.008f) {
            if (peak > env_)
                env_ = attackCoef_ * (env_ - peak) + peak;
            else
                env_ = releaseCoef_ * (env_ - peak) + peak;

            if (settleCount_ < settleTargetSamples_) {
                ++settleCount_;
            }
            const float rawGain = clamp(targetGain_ / std::max(env_, 0.008f), 0.25f, 8.0f);
            // Allow immediate attenuation during transient onset, hold boost until initial 15ms settle completes
            desiredAutoGain_ = (settleCount_ >= settleTargetSamples_) ? rawGain : std::min(1.0f, rawGain);
        }

        currentAutoGain_ = gainSmoothCoef_ * (currentAutoGain_ - desiredAutoGain_) + desiredAutoGain_;
        const float totalGain = currentAutoGain_ * trim;
        left *= totalGain;
        right *= totalGain;
    }

private:
    double sampleRate_ = 44100.0;
    float attackCoef_ = 0.9775f;
    float releaseCoef_ = 0.99995f;
    float gainSmoothCoef_ = 0.9993f;
    int settleTargetSamples_ = 661;
    int settleCount_ = 0;
    float env_ = 0.0f;
    float targetGain_ = 0.3548f; // -9 dBFS
    float desiredAutoGain_ = 1.0f;
    float currentAutoGain_ = 1.0f;
    Smoother trimSmoother_;
};

// ---------------------------------------------------------------------------
// Audio Envelope Follower (Dynamic Cutoff & Drive Modulation)
// ---------------------------------------------------------------------------
class EnvelopeFollower
{
public:
    void prepare(double sampleRate)
    {
        attackCoef_ = std::exp(-1.0f / (float(sampleRate) * 0.005f));  // 5 ms snappy attack
        releaseCoef_ = std::exp(-1.0f / (float(sampleRate) * 0.090f)); // 90 ms musical release
        reset();
    }

    void reset() { env_ = 0.0f; }

    float processSample(float peak) noexcept
    {
        if (peak > env_)
            env_ = attackCoef_ * (env_ - peak) + peak;
        else
            env_ = releaseCoef_ * (env_ - peak) + peak;
        return clamp(env_ * 2.5f, 0.0f, 1.0f);
    }

private:
    float attackCoef_ = 0.995f;
    float releaseCoef_ = 0.9997f;
    float env_ = 0.0f;
};

// ---------------------------------------------------------------------------
// S-Curve Waveshaper
// ---------------------------------------------------------------------------
class Waveshaper
{
public:
    void prepare(double sampleRate)
    {
        sampleRate_ = sampleRate;
        drive_.setTime(50.0f, sampleRate);
        threshold_.setTime(50.0f, sampleRate);
        decay_ = std::exp(-1.0f / (float(sampleRate) * 0.1f));
        reset();
    }

    void reset()
    {
        drive_.snap(1.0f);
        threshold_.snap(1.0f);
        inputPeak_ = 0.0f;
    }

    void snapToCurrent()
    {
        drive_.snap(drive_.target());
        threshold_.snap(threshold_.target());
    }

    void setDrive(float newDrive) { drive_.setTarget(newDrive); }
    void setExhaustion(bool isExhausted) { exhausted_ = isExhausted; }
    void setThreshold(float newThreshold) { threshold_.setTarget(std::fabs(newThreshold)); }
    void setAutoGain(bool enabled) { autogain_ = enabled; }

    void advanceSmoothers(float& outDrive, float& outThreshold)
    {
        outDrive = drive_.process();
        outThreshold = threshold_.process();
    }

    float processSample(float sample, float currentDrive, float currentThreshold) noexcept
    {
        const float x = sample * currentDrive;
        float y;
        if (exhausted_) {
            y = clamp(x, -currentThreshold, currentThreshold);
        } else {
            const float denom = currentThreshold + 1.0e-3f;
            const float shaped = fastTanh(x / denom) * denom;
            const float satMix = clamp(std::max((currentDrive - 1.0f) * 1.8f, (1.0f - currentThreshold) * 2.5f), 0.0f, 1.0f);
            y = x + (shaped - x) * satMix;
        }

        if (autogain_) {
            inputPeak_ = std::max(std::fabs(sample), inputPeak_ * decay_);
            const float a = clamp(inputPeak_, 0.01f, 1.0f);
            const float gainComp = a + (1.0f - a) / std::max(currentDrive, 1.0e-4f);
            y *= gainComp;
        }
        return y;
    }

private:
    double sampleRate_ = 44100.0;
    Smoother drive_;
    Smoother threshold_;
    bool exhausted_ = false;
    bool autogain_ = false;
    float inputPeak_ = 0.0f;
    float decay_ = 0.9997f;
};

// ---------------------------------------------------------------------------
// Stereo Filter (Clean / Vowel / Acid / Digital + LP / HP / BP / Notch)
// ---------------------------------------------------------------------------
class MushinFilter
{
public:
    enum class Type { Clean = 0, Vowel, Acid, Digital };
    enum class Mode { Lowpass = 0, Highpass, Bandpass, Notch };

    void prepare(double sampleRate)
    {
        svfL_.setSampleRate(sampleRate);
        svfR_.setSampleRate(sampleRate);
        for (int i = 0; i < 3; ++i) {
            fL_[i].setSampleRate(sampleRate);
            fR_[i].setSampleRate(sampleRate);
        }
        reset();
    }

    void reset()
    {
        svfL_.reset();
        svfR_.reset();
        for (int i = 0; i < 3; ++i) {
            fL_[i].reset();
            fR_[i].reset();
        }
    }

    void setType(int t) { type_ = (Type)clamp(t, 0, 3); }
    void setMode(int m) { mode_ = (Mode)clamp(m, 0, 3); }
    void setDrive(float d) { drive_ = d; }
    void setGrit(float g) { grit_ = g; }

    void setCoefficients(float cutoffHz, float resonance01, bool allowBypass = true)
    {
        const float c = clamp(cutoffHz, 20.0f, 20000.0f);
        float r = clamp(resonance01, 0.0f, 1.0f);
        if (type_ == Type::Acid) {
            r = clamp(r * 1.15f, 0.0f, 0.99f);
        }
        bypass_ = (allowBypass && type_ == Type::Clean && mode_ == Mode::Lowpass && c >= 19950.0f && drive_ <= 1.001f && grit_ <= 0.001f);
        svfL_.set(c, r);
        svfR_.set(c, r);

        if (type_ == Type::Vowel) {
            // Morph across 5 vocal formants A -> E -> I -> O -> U via log cutoff
            static constexpr float kFormants[5][3] = {
                { 730.0f, 1090.0f, 2440.0f }, // A
                { 530.0f, 1840.0f, 2480.0f }, // E
                { 270.0f, 2290.0f, 3010.0f }, // I
                { 570.0f,  840.0f, 2410.0f }, // O
                { 300.0f,  870.0f, 2240.0f }, // U
            };
            const float norm = clamp(std::log2(c / 20.0f) / 9.965784f, 0.0f, 1.0f) * 4.0f;
            const int i0 = clamp((int)norm, 0, 3);
            const float frac = norm - (float)i0;
            // High-Q resonant formants (Q ~ 4..17) for clear vocal "oh / ah / ee" character
            const float fRes = clamp(0.88f + r * 0.10f, 0.88f, 0.98f);
            for (int k = 0; k < 3; ++k) {
                const float fk = kFormants[i0][k] + (kFormants[i0 + 1][k] - kFormants[i0][k]) * frac;
                fL_[k].set(fk, fRes);
                fR_[k].set(fk, fRes);
            }
            vowelGain_ = clamp(0.45f - r * 0.18f, 0.24f, 0.45f);
        }
    }

    float processSample(int ch, float sample) noexcept
    {
        if (bypass_) return sample;

        float preDrive = drive_;
        if (type_ == Type::Acid) preDrive *= 1.4f;

        const float preMix = clamp((preDrive - 1.0f) * 1.8f, 0.0f, 1.0f);
        const float driven = sample * preDrive;
        const float x = driven + (fastTanh(driven) - driven) * preMix;

        float filtered = 0.0f;
        if (type_ == Type::Vowel) {
            SVF* fb = (ch == 0) ? fL_ : fR_;
            const float f0 = fb[0].process(x, SVF::Mode::Bandpass);
            const float f1 = fb[1].process(x, SVF::Mode::Bandpass);
            const float f2 = fb[2].process(x, SVF::Mode::Bandpass);
            switch (mode_) {
                case Mode::Lowpass:
                    filtered = vowelGain_ * (1.25f * f0 + 0.70f * f1 + 0.25f * f2);
                    break;
                case Mode::Bandpass:
                    filtered = vowelGain_ * (1.00f * f0 + 0.85f * f1 + 0.50f * f2);
                    break;
                case Mode::Highpass:
                    filtered = vowelGain_ * (0.55f * f0 + 1.00f * f1 + 0.85f * f2);
                    break;
                case Mode::Notch:
                    filtered = x - vowelGain_ * (1.00f * f0 + 0.80f * f1 + 0.45f * f2);
                    break;
            }
        } else {
            SVF::Mode svfMode = SVF::Mode::Lowpass;
            switch (mode_) {
                case Mode::Lowpass:  svfMode = SVF::Mode::Lowpass; break;
                case Mode::Highpass: svfMode = SVF::Mode::Highpass; break;
                case Mode::Bandpass: svfMode = SVF::Mode::Bandpass; break;
                case Mode::Notch:    svfMode = SVF::Mode::Notch; break;
            }
            filtered = (ch == 0) ? svfL_.process(x, svfMode) : svfR_.process(x, svfMode);
        }

        float out = filtered * (1.0f + grit_);
        const float postMix = clamp(std::max(preMix, grit_ * 10.0f), 0.0f, 1.0f);
        return out + (fastTanh(out) - out) * postMix;
    }

private:
    Type type_ = Type::Clean;
    Mode mode_ = Mode::Lowpass;
    float drive_ = 1.0f;
    float grit_ = 0.0f;
    float vowelGain_ = 0.40f;
    bool bypass_ = false;
    SVF svfL_, svfR_;
    SVF fL_[3], fR_[3];
};

// ---------------------------------------------------------------------------
// Noise / Tone Oscillator
// ---------------------------------------------------------------------------
class NoiseOscillator
{
public:
    enum class Type { White = 0, Pink, Sine, Triangle, Saw, Square };

    void prepare(double sampleRate)
    {
        sampleRate_ = (float)sampleRate;
        reset();
    }

    void reset()
    {
        phase_ = 0.0f;
        pink_.reset();
    }

    void setType(int t) { type_ = (Type)clamp(t, 0, 5); }
    void setFrequency(float hz) { freq_ = clamp(hz, 20.0f, 20000.0f); }

    float nextSample() noexcept
    {
        phase_ += freq_ / sampleRate_;
        if (phase_ >= 1.0f) phase_ -= 1.0f;

        switch (type_) {
            case Type::White:    return white_.process();
            case Type::Pink:     return pink_.process();
            case Type::Sine:     return fastSin01(phase_);
            case Type::Triangle:
                if (phase_ < 0.25f) return 4.0f * phase_;
                if (phase_ < 0.75f) return 2.0f - 4.0f * phase_;
                return 4.0f * phase_ - 4.0f;
            case Type::Saw:      return 2.0f * phase_ - 1.0f;
            case Type::Square:   return (phase_ < 0.5f) ? 1.0f : -1.0f;
        }
        return 0.0f;
    }

private:
    Type type_ = Type::White;
    float sampleRate_ = 44100.0f;
    float freq_ = 440.0f;
    float phase_ = 0.0f;
    WhiteNoise white_{0x13579BDFu};
    PinkNoise pink_{0x2468ACE0u};
};

// ---------------------------------------------------------------------------
// Sidechain Envelope Follower & Filter
// ---------------------------------------------------------------------------
class SidechainProcessor
{
public:
    enum class Source { Internal = 0, TestSignal };
    enum class Target { Drive = 0, Cutoff, Gain };

    void prepare(double sampleRate)
    {
        sampleRate_ = sampleRate;
        hpFilter_.setSampleRate(sampleRate);
        lpFilter_.setSampleRate(sampleRate);
        hpFilter_.set(hpFreq_, 0.0f);
        lpFilter_.set(lpFreq_, 0.0f);
        reset();
    }

    void reset()
    {
        envelope_ = 0.0f;
        lastEnv_ = 0.0f;
        testPhaseSamples_ = 0.0;
        hpFilter_.reset();
        lpFilter_.reset();
    }

    void setParameters(float attackMs, float releaseMs, bool isRMS, float thresholdDb,
                       float amount, float hpFreq, float lpFreq, float ratio = 4.0f)
    {
        attack_ = (attackMs > 0.0f) ? std::exp(-1.0f / (float(sampleRate_) * attackMs * 0.001f)) : 0.0f;
        release_ = (releaseMs > 0.0f) ? std::exp(-1.0f / (float(sampleRate_) * releaseMs * 0.001f)) : 0.0f;
        rmsMode_ = isRMS;
        threshold_ = dbToGain(thresholdDb);
        amount_ = amount;
        ratioScale_ = clamp((ratio - 1.0f) / 3.0f, 0.0f, 8.0f);

        const float clampedHp = clamp(hpFreq, 20.0f, 20000.0f);
        const float clampedLp = clamp(lpFreq, 20.0f, 20000.0f);
        if (clampedHp != hpFreq_) {
            hpFreq_ = clampedHp;
            hpFilter_.set(hpFreq_, 0.0f);
        }
        if (clampedLp != lpFreq_) {
            lpFreq_ = clampedLp;
            lpFilter_.set(lpFreq_, 0.0f);
        }
    }

    void setActive(bool a) { active_ = a; }
    bool isActive() const { return active_; }
    void setSource(int s) { source_ = (Source)clamp(s, 0, 1); }
    Source getSource() const { return source_; }
    void setTarget(int t) { target_ = (Target)clamp(t, 0, 2); }
    Target getTarget() const { return target_; }

    float lastEnv() const { return lastEnv_; }

    float processSample(float sidechainSample, bool testPulse = false) noexcept
    {
        if (!active_) { lastEnv_ = 0.0f; return 0.0f; }

        float x = 0.0f;
        if (source_ == Source::TestSignal) {
            x = testPulse ? 1.0f : 0.0f;
        } else {
            float filtered = hpFilter_.process(sidechainSample, SVF::Mode::Highpass);
            filtered = lpFilter_.process(filtered, SVF::Mode::Lowpass);
            x = std::fabs(filtered);
            if (rmsMode_) x = x * x;
        }

        if (x > envelope_)
            envelope_ = attack_ * (envelope_ - x) + x;
        else
            envelope_ = release_ * (envelope_ - x) + x;

        const float env = clamp((rmsMode_ && source_ == Source::Internal)
                                    ? std::sqrt(std::max(0.0f, envelope_))
                                    : envelope_,
                                0.0f, 1.0f);
        lastEnv_ = env;
        float modulation = 0.0f;
        if (env > threshold_ && env > 1.0e-5f) {
            // Ratio scales steepness: 1:1 = 0, 4:1 = full 1.0 at +8 dB over, 20:1 = hard pump
            modulation = clamp(((env - threshold_) / (1.5f * threshold_ + 1.0e-4f)) * ratioScale_, 0.0f, 1.0f);
        }
        return modulation * amount_;
    }

    double& testPhaseSamples() { return testPhaseSamples_; }

private:
    double sampleRate_ = 44100.0;
    double testPhaseSamples_ = 0.0;
    float attack_ = 0.0f, release_ = 0.0f, envelope_ = 0.0f, lastEnv_ = 0.0f;
    bool rmsMode_ = false;
    bool active_ = false;
    Source source_ = Source::Internal;
    Target target_ = Target::Gain;
    float threshold_ = 0.0f;
    float amount_ = 0.0f;
    float ratioScale_ = 1.0f;
    float hpFreq_ = 20.0f;
    float lpFreq_ = 20000.0f;
    SVF hpFilter_, lpFilter_;
};

// ---------------------------------------------------------------------------
// Trance Gate
// ---------------------------------------------------------------------------
static constexpr uint16_t kGateMasks[128] = {
    // 00-07: Core (preserved)
    0xAAAA, // 01 Straight 16
    0x5555, // 02 Offbeat 16
    0xEEEE, // 03 Classic 1
    0x9999, // 04 Classic 2
    0xF0F0, // 05 4-On-Floor
    0xD7D7, // 06 Galop
    0x8888, // 07 Space Gate
    0x8912, // 08 Euclidean 5
    // 08-23: House / Trance / 4x4
    0x1111, // 09 Quarter Pulse
    0x4444, // 10 Upbeat 8th
    0xCCCC, // 11 Pumping 8th
    0xBDBD, // 12 Trance Arp A
    0xDBDB, // 13 Trance Arp B
    0x7777, // 14 Triplet Feel
    0xEEFE, // 15 Sidechain Dip
    0x3333, // 16 Stutter 4x4
    0x6666, // 17 Synco 4x4
    0x7575, // 18 Rolling Trance
    0xB5B5, // 19 Gate Push
    0x2222, // 20 2nd 16th
    0xA0A0, // 21 Half Offbeat
    0x00FF, // 22 Half Bar On
    0xFF00, // 23 Half Bar Off
    0xFFFF, // 24 All Open
    // 24-39: Techno & Driving
    0x9595, // 25 Techno Drive
    0xB7B7, // 26 Rolling 16th A
    0x7B7B, // 27 Rolling 16th B
    0x9249, // 28 Hypno 3-Step
    0x2492, // 29 Hypno 3-Rot
    0x1451, // 30 Berlin Dub
    0xA9A9, // 31 Acid Gate A
    0x5959, // 32 Acid Gate B
    0xF333, // 33 Industrial
    0x3F3F, // 34 Heavy Chug
    0x8421, // 35 Polyrhythm 5
    0xD555, // 36 Synco Drive
    0x1FFF, // 37 Stomp & Run
    0xFF11, // 38 Beat Drop
    0xA5A5, // 39 Warehouse
    0x5A5A, // 40 Dark Pulse
    // 40-55: Breakbeat / DnB / Jungle
    0x1449, // 41 Two-Step A
    0x4449, // 42 Two-Step B
    0x54D5, // 43 Amen Accent
    0x4D4D, // 44 Think Break
    0xA98D, // 45 Jungle Chop
    0x6525, // 46 DnB Roller
    0x4491, // 47 Halftime DnB
    0x2929, // 48 Broken 16th
    0x9313, // 49 Ghost Shuffle
    0x94C5, // 50 Break Synco
    0x6967, // 51 Neuro Gate
    0xD595, // 52 Liquid Flow
    0xF511, // 53 Snare Roll
    0x1513, // 54 Double Kick
    0x5A59, // 55 Skippy Beat
    0x3355, // 56 Stepper
    // 56-71: Trap / Hip-Hop / Halftime
    0x1191, // 57 Boom Bap A
    0x1419, // 58 Boom Bap B
    0x5757, // 59 Trap Hats A
    0xD5F5, // 60 Trap Hats B
    0xF111, // 61 Trap Roll End
    0x4A49, // 62 808 Bounce
    0x1501, // 63 Halftime Dip
    0x0441, // 64 Sparse 808
    0x1249, // 65 Triplet Hop
    0xA949, // 66 Drill Slide
    0x31C7, // 67 Stutter 3+3+2
    0x4489, // 68 Late Bounce
    0x0303, // 69 Sub Drop
    0x1919, // 70 Lazy Swing
    0x11F1, // 71 Mid Roll
    0xF101, // 72 End Burst
    // 72-87: Euclidean Rhythms
    0x0821, // 73 Euclid 3
    0x2084, // 74 Euclid 3 Rot
    0x2249, // 75 Euclid 5 Rot
    0x4949, // 76 Euclid 6
    0x9292, // 77 Euclid 6 Rot
    0x52A5, // 78 Euclid 7
    0xA54A, // 79 Euclid 7 Rot
    0x9529, // 80 Euclid 7 R2
    0x5B5B, // 81 Euclid 9
    0xB6B6, // 82 Euclid 9 Rot
    0xADAD, // 83 Euclid 10 R2
    0x6DB7, // 84 Euclid 11
    0xB6DB, // 85 Euclid 11 Rot
    0xEF7B, // 86 Euclid 13
    0xFF77, // 87 Euclid 14
    0xFFFE, // 88 Euclid 15
    // 88-103: Funk / Clave / Afro-Latin
    0x2449, // 89 Son Clave 3-2
    0x4914, // 90 Son Clave 2-3
    0x1489, // 91 Rumba 3-2
    0x8914, // 92 Rumba 2-3
    0x8449, // 93 Bossa Nova
    0x8949, // 94 Tresillo
    0x6D6D, // 95 Cinquillo
    0x4D2D, // 96 Funk Chop A
    0x52D3, // 97 Funk Chop B
    0x9A69, // 98 Clavinet
    0x5D5D, // 99 Samba Gate
    0xAAA5, // 100 Afrobeat A
    0x9525, // 101 Afrobeat B
    0x9959, // 102 Reggaeton
    0x1949, // 103 Dancehall
    0x5554, // 104 Disco Octave
    // 104-119: Stutter / Glitch / Build-ups
    0x7511, // 105 Accel Build
    0x112F, // 106 Decel Fade
    0xFC41, // 107 Ramp Up
    0x113F, // 108 Ramp Down
    0x0F0F, // 109 Burst 1
    0x1717, // 110 Burst 2
    0x49A3, // 111 Glitch A
    0xA646, // 112 Glitch B
    0x140F, // 113 Call & Resp A
    0x0F05, // 114 Call & Resp B
    0x54FF, // 115 Tail Chop
    0xA6DB, // 116 Gate Flutter
    0xFFFB, // 117 16th Gap 1
    0xEFFF, // 118 16th Gap 2
    0xEEE0, // 119 16th Gap 4
    0x94A5, // 120 Micro Chop
    // 120-127: Odd / Sparse / Breathers
    0x0001, // 121 Beat 1 Only
    0x0101, // 122 Beat 1 & 3
    0x1010, // 123 Beat 2 & 4
    0x0100, // 124 Beat 3 Only
    0x2211, // 125 Sparse 4/16
    0x0421, // 126 Sparse 5/16
    0x4A95, // 127 Odd 7/16
    0x7FFF  // 128 Sustain 15
};

class TranceGateProcessor
{
public:
    void reset() { currentSamplePos_ = 0.0; }

    float processSample(double sPos, double stepDurationSamples, double cycleDurationSamples,
                        uint16_t patternMask, double holdWidthSamples, double attackEnd,
                        double decayStart, float baseAtten, float tgMix) const noexcept
    {
        double sCycle = std::fmod(sPos, cycleDurationSamples);
        if (sCycle < 0.0) sCycle += cycleDurationSamples;

        int step = (int)(sCycle / stepDurationSamples);
        step = clamp(step, 0, 15);

        const double sOffset = sCycle - (step * stepDurationSamples);
        const bool stepActive = (patternMask & (1u << step)) != 0;

        float envVal = 0.0f;
        if (stepActive) {
            if (sOffset < attackEnd && attackEnd > 0.0) {
                envVal = (float)(sOffset / attackEnd);
            } else if (sOffset < decayStart) {
                envVal = 1.0f;
            } else if (sOffset < holdWidthSamples && (holdWidthSamples - decayStart) > 0.0) {
                envVal = 1.0f - (float)((sOffset - decayStart) / (holdWidthSamples - decayStart));
            }
        }

        const float sampleGain = baseAtten + ((1.0f - baseAtten) * envVal);
        return (1.0f - tgMix) + (tgMix * sampleGain);
    }

    void advanceFreeRunningClock(int numSamples, double cycleDurationSamples) noexcept
    {
        if (cycleDurationSamples > 0.0) {
            currentSamplePos_ = std::fmod(currentSamplePos_ + numSamples, cycleDurationSamples);
        }
    }

    double currentSamplePos() const { return currentSamplePos_; }
    void setCurrentSamplePos(double p) { currentSamplePos_ = p; }

private:
    double currentSamplePos_ = 0.0;
};

// ---------------------------------------------------------------------------
// Quantization Error (Info Decay)
// ---------------------------------------------------------------------------
class QuantizationErrorProcessor
{
public:
    void reset()
    {
        heldSample_[0] = heldSample_[1] = 0.0f;
        sampleCounter_[0] = sampleCounter_[1] = 0;
    }

    float processSample(int ch, float sample, float bitDepth, float steps, int downsample, float mix) noexcept
    {
        if (mix <= 0.001f) return sample;

        float wetSample = sample;
        if (downsample > 1) {
            if (sampleCounter_[ch] % (uint32_t)downsample == 0) {
                heldSample_[ch] = wetSample;
            }
            wetSample = heldSample_[ch];
            sampleCounter_[ch]++;
        } else {
            sampleCounter_[ch] = 0;
        }

        if (bitDepth < 15.5f) {
            wetSample = std::round(wetSample * steps) / steps;
        }

        return (1.0f - mix) * sample + mix * wetSample;
    }

private:
    float heldSample_[2] = {0.0f, 0.0f};
    uint32_t sampleCounter_[2] = {0, 0};
};

// ---------------------------------------------------------------------------
// Stereo Delay
// ---------------------------------------------------------------------------
class DelayProcessor
{
public:
    void prepare(double sampleRate)
    {
        sampleRate_ = sampleRate;
        const int maxSamples = (int)(sampleRate * 2.05) + 1;
        delayL_.prepare(maxSamples);
        delayR_.prepare(maxSamples);
        fbFilterL_.setCutoff(8000.0f, sampleRate);
        fbFilterR_.setCutoff(8000.0f, sampleRate);
        reset();
    }

    void reset()
    {
        delayL_.reset();
        delayR_.reset();
        fbFilterL_.reset();
        fbFilterR_.reset();
        lastFbL_ = lastFbR_ = 0.0f;
    }

    void processSample(float& left, float& right, float timeMs, float feedback, float mix,
                       bool pingpong, bool sync, float syncBpm, int syncType) noexcept
    {
        if (mix <= 0.001f) return;

        float targetTimeMs = timeMs;
        if (sync) {
            const float bpm = syncBpm > 1.0f ? syncBpm : 120.0f;
            const float quarterSec = 60.0f / bpm;
            float factor = 1.0f;
            switch (syncType) {
                case 0: factor = 0.25f; break;
                case 1: factor = 0.5f;  break;
                case 2: factor = 1.0f;  break;
                case 3: factor = 2.0f;  break;
            }
            targetTimeMs = quarterSec * factor * 1000.0f;
        }

        const float maxD = (float)delayL_.maxDelay();
        const float delaySamples = clamp(msToSamples(targetTimeMs, sampleRate_), 1.0f, maxD);

        const float delayedL = delayL_.readLinear(delaySamples);
        const float delayedR = delayR_.readLinear(delaySamples);

        if (pingpong) {
            delayL_.write(left + lastFbR_ * feedback);
            delayR_.write(right + lastFbL_ * feedback);
        } else {
            delayL_.write(left + lastFbL_ * feedback);
            delayR_.write(right + lastFbR_ * feedback);
        }

        lastFbL_ = fbFilterL_.process(delayedL);
        lastFbR_ = fbFilterR_.process(delayedR);

        left = (1.0f - mix) * left + mix * delayedL;
        right = (1.0f - mix) * right + mix * delayedR;
    }

private:
    double sampleRate_ = 44100.0;
    DelayLine delayL_, delayR_;
    OnePoleLP fbFilterL_, fbFilterR_;
    float lastFbL_ = 0.0f, lastFbR_ = 0.0f;
};

// ---------------------------------------------------------------------------
// Final Lookahead Limiter
// ---------------------------------------------------------------------------
class LimiterProcessor
{
public:
    void prepare(double sampleRate)
    {
        sampleRate_ = sampleRate;
        const int lookaheadSamples = clamp((int)(0.002 * sampleRate), 1, 256);
        bufL_.assign((size_t)(lookaheadSamples + 1), 0.0f);
        bufR_.assign((size_t)(lookaheadSamples + 1), 0.0f);
        reset();
    }

    void reset()
    {
        std::fill(bufL_.begin(), bufL_.end(), 0.0f);
        std::fill(bufR_.begin(), bufR_.end(), 0.0f);
        writeIdx_[0] = writeIdx_[1] = 0;
        env_[0] = env_[1] = 0.0f;
        smoothedGR_[0] = smoothedGR_[1] = 1.0f;
    }

    void processSample(float& left, float& right, float driveGain, float ceilingDb,
                       float ceilingGain, float releaseCoef, float mix, int mode) noexcept
    {
        if (bufL_.empty()) return;

        float* bufs[2] = { bufL_.data(), bufR_.data() };
        const int len = (int)bufL_.size();
        const float in[2] = { left * driveGain, right * driveGain };
        float out[2] = { 0.0f, 0.0f };

        for (int ch = 0; ch < 2; ++ch) {
            const float inVal = in[ch];
            int& wIdx = writeIdx_[ch];
            bufs[ch][wIdx] = inVal;
            const int rIdx = (wIdx + 1) % len;
            const float delayed = bufs[ch][rIdx];
            wIdx = rIdx;

            const float absVal = std::fabs(inVal);
            if (absVal > env_[ch])
                env_[ch] = absVal;
            else
                env_[ch] = absVal + releaseCoef * (env_[ch] - absVal);

            if (env_[ch] < 1.0e-6f) env_[ch] = 0.0f;

            float targetGain = 1.0f;
            if (env_[ch] > 1.0e-5f) {
                if (mode == 0) {
                    if (env_[ch] > ceilingGain)
                        targetGain = ceilingGain / env_[ch];
                } else {
                    const float envDb = gainToDb(env_[ch]);
                    const float diffDb = envDb - ceilingDb;
                    if (diffDb > 0.0f) {
                        const float compressedDb = ceilingDb + (diffDb / 20.0f);
                        targetGain = dbToGain(compressedDb) / env_[ch];
                    }
                }
            }

            smoothedGR_[ch] += 0.25f * (targetGain - smoothedGR_[ch]);
            if (std::isnan(smoothedGR_[ch]) || std::isinf(smoothedGR_[ch]))
                smoothedGR_[ch] = 1.0f;

            const float wet = delayed * smoothedGR_[ch];
            out[ch] = (1.0f - mix) * delayed + mix * wet;
        }

        left = out[0];
        right = out[1];
    }

    float currentGR() const noexcept
    {
        return 1.0f - std::min(smoothedGR_[0], smoothedGR_[1]);
    }

private:
    double sampleRate_ = 44100.0;
    std::vector<float> bufL_, bufR_;
    int writeIdx_[2] = {0, 0};
    float env_[2] = {0.0f, 0.0f};
    float smoothedGR_[2] = {1.0f, 1.0f};
};

} // namespace

// ---------------------------------------------------------------------------
// Mushin MPC Plugin
// ---------------------------------------------------------------------------
class Mushin : public mpc::Plugin
{
public:
    enum Params {
        // S-Curve & Master (0..5)
        kGain = 0, kDrive, kExhaustion, kThreshold, kAutoGain, kMix,
        // Filter A (6..11)
        kFACutoff, kFAReso, kFADrive, kFAGrit, kFAType, kFAMode,
        // Filter B & Routing (12..18)
        kFBCutoff, kFBReso, kFBDrive, kFBGrit, kFBType, kFBMode, kRouting,
        // LFOs (19..22)
        kLfo1Freq, kLfo1Wave, kLfo2Freq, kLfo2Wave,
        // Mod Matrix (23..34)
        kModL1T1, kModL1T2, kModL1T3, kModL1T4, kModL1T5, kModL1T6,
        kModL2T1, kModL2T2, kModL2T3, kModL2T4, kModL2T5, kModL2T6,
        // Sidechain (35..44)
        kScActive, kScSource, kScThresh, kScAttack, kScRelease,
        kScMode, kScAmount, kScTarget, kScHpf, kScLpf,
        // Noise Oscillator (45..50)
        kNoiseActive, kNoiseType, kNoiseFreq, kNoiseLevel, kNoiseRoute, kNoiseFmMod,
        // Trance Gate (51..58)
        kTgActive, kTgMix, kTgPattern, kTgRate, kTgStart, kTgHold, kTgEnd, kTgDepth,
        // Info Decay (59..63)
        kQeActive, kQeDepth, kQeDownsample, kQeMix, kQeLink,
        // Stereo Delay (64..70)
        kDelayActive, kDelayTime, kDelayFeedback, kDelayMix, kDelayPingPong, kDelaySync, kDelayTempo,
        // Limiter (71..76)
        kLimActive, kLimMode, kLimDrive, kLimCeiling, kLimRelease, kLimMix,
        // Performance Macros (77..84 - appended at end to preserve parameter IDs)
        kMacroExhaustion, kMacroChoke, kMacroDecay, kMacroCorrupt,
        kMacroKinetic, kMacroResonator, kMacroGate, kMacroSmash,
        // Appended parameters (85+)
        kFilterMix,
        kTgFloor,
        kScMix,
        kScInMeter,
        kScDuckMeter,
        kScRatio,
        kScTargetDrive,
        kScTargetCut,
        kScTargetGain,
        kScTargetReso,
        kLimGrMeter,
        kScvActive,
        kDecayViz,
        kTgStep0, kTgStep1, kTgStep2,  kTgStep3,
        kTgStep4, kTgStep5, kTgStep6,  kTgStep7,
        kTgStep8, kTgStep9, kTgStep10, kTgStep11,
        kTgStep12, kTgStep13, kTgStep14, kTgStep15,
        kTgOffset,
        kAutoIn,
        kInTarget,
        kInTrim,
        kEnvCut,
        kEnvDrive,
        kFiltPreSat,
        kEnvARes,
        kEnvBCut,
        kEnvL1Rate,
        kEnvLfoAmt,

        kNumParams
    };

    static mpc::PluginInfo makeInfo()
    {
        mpc::PluginInfo i;
        i.name = "Mushin";
        i.vendor = "PhilippeBack";
        i.uniqueId = MPC_FOURCC('M', 's', 'h', '2');
        i.version = 1000;
        i.isInstrument = false;
        i.numInputs = 2;
        i.numOutputs = 2;
        i.tailSamples = 0;
        return i;
    }

    Mushin() : mpc::Plugin(makeInfo())
    {
        using namespace mpc;

        // S-Curve & Master (Parameter 0 is Gain - harmless on host load nudge)
        addParam(floatParam ("gain",       "Gain",        0.0f, 2.0f, 1.0f, "x", 2));
        addParam(floatParam ("drive",      "Drive",       1.0f, 20.0f, 1.0f, "x", 1));
        addParam(boolParam  ("exhaustion", "Exhaustion",  false));
        addParam(floatParam ("threshold",  "Threshold",   -24.0f, 0.0f, 0.0f, "dB", 1));
        addParam(boolParam  ("autogain",   "Auto Gain",   false));
        addParam(floatParam ("mix",        "Mix",         0.0f, 1.0f, 1.0f, "", 2));

        // Filter A
        addParam(logParam   ("filter_a_cutoff",    "Cutoff",   20.0f, 20000.0f, 20000.0f, "Hz", 0));
        addParam(floatParam ("filter_a_resonance", "Reso",     0.0f, 1.0f, 0.1f, "", 2));
        addParam(floatParam ("filter_a_drive",     "Drive",    1.0f, 10.0f, 1.0f, "x", 1));
        addParam(floatParam ("filter_a_grit",      "Grit",     0.0f, 1.0f, 0.0f, "", 2));
        addParam(choiceParam("filter_a_type",      "Type",     {"CLN", "VOWL", "ACID", "DIGI"}, 0));
        addParam(choiceParam("filter_a_mode",      "Mode",     {"LP", "HP", "BP", "NOTCH"}, 0));

        // Filter B & Routing
        addParam(logParam   ("filter_b_cutoff",    "Cutoff",   20.0f, 20000.0f, 20000.0f, "Hz", 0));
        addParam(floatParam ("filter_b_resonance", "Reso",     0.0f, 1.0f, 0.1f, "", 2));
        addParam(floatParam ("filter_b_drive",     "Drive",    1.0f, 10.0f, 1.0f, "x", 1));
        addParam(floatParam ("filter_b_grit",      "Grit",     0.0f, 1.0f, 0.0f, "", 2));
        addParam(choiceParam("filter_b_type",      "Type",     {"CLN", "VOWL", "ACID", "DIGI"}, 0));
        addParam(choiceParam("filter_b_mode",      "Mode",     {"LP", "HP", "BP", "NOTCH"}, 0));
        addParam(choiceParam("routing",            "Path",     {"SERIAL", "PARAL", "STEREO", "M/S"}, 0));

        // LFOs
        addParam(logParam   ("lfo1_freq", "LFO 1 Freq", 0.1f, 20.0f, 1.0f, "Hz", 2));
        addParam(choiceParam("lfo1_wave", "LFO 1 Wave", {"Sine", "Triangle", "Saw", "Square", "Random"}, 0));
        addParam(logParam   ("lfo2_freq", "LFO 2 Freq", 0.1f, 20.0f, 1.0f, "Hz", 2));
        addParam(choiceParam("lfo2_wave", "LFO 2 Wave", {"Sine", "Triangle", "Saw", "Square", "Random"}, 0));

        // Mod Matrix
        addParam(floatParam("mod_lfo1_target1", "A Cut",  -1.0f, 1.0f, 0.0f, "", 2));
        addParam(floatParam("mod_lfo1_target2", "A Res",  -1.0f, 1.0f, 0.0f, "", 2));
        addParam(floatParam("mod_lfo1_target3", "A Grit", -1.0f, 1.0f, 0.0f, "", 2));
        addParam(floatParam("mod_lfo1_target4", "B Cut",  -1.0f, 1.0f, 0.0f, "", 2));
        addParam(floatParam("mod_lfo1_target5", "B Res",  -1.0f, 1.0f, 0.0f, "", 2));
        addParam(floatParam("mod_lfo1_target6", "B Grit", -1.0f, 1.0f, 0.0f, "", 2));

        addParam(floatParam("mod_lfo2_target1", "A Cut",  -1.0f, 1.0f, 0.0f, "", 2));
        addParam(floatParam("mod_lfo2_target2", "A Res",  -1.0f, 1.0f, 0.0f, "", 2));
        addParam(floatParam("mod_lfo2_target3", "A Grit", -1.0f, 1.0f, 0.0f, "", 2));
        addParam(floatParam("mod_lfo2_target4", "B Cut",  -1.0f, 1.0f, 0.0f, "", 2));
        addParam(floatParam("mod_lfo2_target5", "B Res",  -1.0f, 1.0f, 0.0f, "", 2));
        addParam(floatParam("mod_lfo2_target6", "B Grit", -1.0f, 1.0f, 0.0f, "", 2));

        // Sidechain
        addParam(boolParam  ("sc_active",    "Active",  false));
        addParam(choiceParam("sc_source",    "Source",  {"INT", "TEST"}, 0));
        addParam(floatParam ("sc_threshold", "Thresh",  -60.0f, 0.0f, -24.0f, "dB", 1));
        addParam(floatParam ("sc_attack",    "Attack",  0.1f, 500.0f, 10.0f, "ms", 1));
        addParam(floatParam ("sc_release",   "Release", 1.0f, 2000.0f, 100.0f, "ms", 0));
        addParam(choiceParam("sc_mode",      "Mode",    {"PEAK", "RMS"}, 0));
        addParam(floatParam ("sc_amount",    "Amount",  -100.0f, 100.0f, -60.0f, "%", 0));
        addParam(choiceParam("sc_target",    "Target",  {"DRIVE", "CUT", "GAIN"}, 2));
        addParam(logParam   ("sc_hp_freq",   "HPF",     20.0f, 2000.0f, 20.0f, "Hz", 0));
        addParam(logParam   ("sc_lp_freq",   "LPF",     500.0f, 20000.0f, 20000.0f, "Hz", 0));

        // Noise Oscillator
        addParam(boolParam  ("noise_active",  "Active",    false));
        addParam(choiceParam("noise_type",    "Type",      {"White", "Pink", "Sine", "Triangle", "Saw", "Square"}, 0));
        addParam(logParam   ("noise_freq",    "Freq",      20.0f, 20000.0f, 440.0f, "Hz", 0));
        addParam(floatParam ("noise_level",   "Level",     -60.0f, 0.0f, -20.0f, "dB", 1));
        addParam(choiceParam("noise_routing", "Route",     {"PRE-D", "PRE-F", "POST"}, 0));
        addParam(floatParam ("noise_fm_mod",  "Filter FM", 0.0f, 1.0f, 0.0f, "", 2));

        // Trance Gate
        addParam(boolParam  ("tg_active",  "Active",  false));
        addParam(floatParam ("tg_mix",     "Mix",     0.0f, 1.0f, 1.0f, "", 2));
        addParam(choiceParam("tg_pattern", "Pattern", {
            "01 Straight 16", "02 Offbeat 16", "03 Classic 1", "04 Classic 2",
            "05 4-On-Floor", "06 Galop", "07 Space Gate", "08 Euclidean 5",
            "09 Quarter Pulse", "10 Upbeat 8th", "11 Pumping 8th", "12 Trance Arp A",
            "13 Trance Arp B", "14 Triplet Feel", "15 Sidechain Dip", "16 Stutter 4x4",
            "17 Synco 4x4", "18 Rolling Trance", "19 Gate Push", "20 2nd 16th",
            "21 Half Offbeat", "22 Half Bar On", "23 Half Bar Off", "24 All Open",
            "25 Techno Drive", "26 Rolling 16th A", "27 Rolling 16th B", "28 Hypno 3-Step",
            "29 Hypno 3-Rot", "30 Berlin Dub", "31 Acid Gate A", "32 Acid Gate B",
            "33 Industrial", "34 Heavy Chug", "35 Polyrhythm 5", "36 Synco Drive",
            "37 Stomp & Run", "38 Beat Drop", "39 Warehouse", "40 Dark Pulse",
            "41 Two-Step A", "42 Two-Step B", "43 Amen Accent", "44 Think Break",
            "45 Jungle Chop", "46 DnB Roller", "47 Halftime DnB", "48 Broken 16th",
            "49 Ghost Shuffle", "50 Break Synco", "51 Neuro Gate", "52 Liquid Flow",
            "53 Snare Roll", "54 Double Kick", "55 Skippy Beat", "56 Stepper",
            "57 Boom Bap A", "58 Boom Bap B", "59 Trap Hats A", "60 Trap Hats B",
            "61 Trap Roll End", "62 808 Bounce", "63 Halftime Dip", "64 Sparse 808",
            "65 Triplet Hop", "66 Drill Slide", "67 Stutter 3+3+2", "68 Late Bounce",
            "69 Sub Drop", "70 Lazy Swing", "71 Mid Roll", "72 End Burst",
            "73 Euclid 3", "74 Euclid 3 Rot", "75 Euclid 5 Rot", "76 Euclid 6",
            "77 Euclid 6 Rot", "78 Euclid 7", "79 Euclid 7 Rot", "80 Euclid 7 R2",
            "81 Euclid 9", "82 Euclid 9 Rot", "83 Euclid 10 R2", "84 Euclid 11",
            "85 Euclid 11 Rot", "86 Euclid 13", "87 Euclid 14", "88 Euclid 15",
            "89 Son Clave 3-2", "90 Son Clave 2-3", "91 Rumba 3-2", "92 Rumba 2-3",
            "93 Bossa Nova", "94 Tresillo", "95 Cinquillo", "96 Funk Chop A",
            "97 Funk Chop B", "98 Clavinet", "99 Samba Gate", "100 Afrobeat A",
            "101 Afrobeat B", "102 Reggaeton", "103 Dancehall", "104 Disco Octave",
            "105 Accel Build", "106 Decel Fade", "107 Ramp Up", "108 Ramp Down",
            "109 Burst 1", "110 Burst 2", "111 Glitch A", "112 Glitch B",
            "113 Call & Resp A", "114 Call & Resp B", "115 Tail Chop", "116 Gate Flutter",
            "117 16th Gap 1", "118 16th Gap 2", "119 16th Gap 4", "120 Micro Chop",
            "121 Beat 1 Only", "122 Beat 1 & 3", "123 Beat 2 & 4", "124 Beat 3 Only",
            "125 Sparse 4/16", "126 Sparse 5/16", "127 Odd 7/16", "128 Sustain 15"
        }, 0));
        addParam(choiceParam("tg_rate",    "Rate",    {"1/16", "1/8", "1/4"}, 0));
        addParam(floatParam ("tg_start",   "Start",   0.0f, 100.0f, 5.0f, "ms", 1));
        addParam(floatParam ("tg_hold",    "Hold",    10.0f, 100.0f, 50.0f, "%", 0));
        addParam(floatParam ("tg_end",     "End",     0.0f, 200.0f, 10.0f, "ms", 1));
        addParam(floatParam ("tg_depth",   "Depth",   0.0f, 100.0f, 50.0f, "%", 0));

        // Info Decay
        addParam(boolParam ("qe_active",     "Active",     false));
        addParam(floatParam("qe_depth",      "Bit Depth",  2.0f, 16.0f, 16.0f, "bit", 1));
        addParam(intParam  ("qe_downsample", "Downsample", 1, 32, 1, "x"));
        addParam(floatParam("qe_mix",        "Mix",        0.0f, 1.0f, 1.0f, "", 2));
        addParam(boolParam ("qe_link",       "Exh Link",   false));

        // Stereo Delay
        addParam(boolParam  ("delay_active",   "Active",    false));
        addParam(logParam   ("delay_time",     "Time",      1.0f, 2000.0f, 300.0f, "ms", 0));
        addParam(floatParam ("delay_feedback", "Feedback",  0.0f, 0.95f, 0.3f, "", 2));
        addParam(floatParam ("delay_mix",      "Mix",       0.0f, 1.0f, 0.3f, "", 2));
        addParam(boolParam  ("delay_pingpong", "Ping-Pong", false));
        addParam(boolParam  ("delay_sync",     "Sync",      false));
        addParam(choiceParam("delay_tempo",    "Tempo",     {"1/16", "1/8", "1/4", "1/2"}, 2));

        // Limiter
        addParam(boolParam  ("limiter_active",  "Active",  false));
        addParam(choiceParam("limiter_mode",    "Mode",    {"CLEAN", "LMC"}, 0));
        addParam(floatParam ("limiter_drive",   "Drive",   0.0f, 24.0f, 0.0f, "dB", 1));
        addParam(floatParam ("limiter_ceiling", "Ceiling", -12.0f, 0.0f, -0.1f, "dB", 1));
        addParam(floatParam ("limiter_release", "Release", 10.0f, 1000.0f, 100.0f, "ms", 0));
        addParam(floatParam ("limiter_mix",     "Mix",     0.0f, 1.0f, 1.0f, "", 2));

        // Performance Macros (0..100%)
        addParam(floatParam ("macro_exhaustion", "EXHAUSTION",  0.0f, 1.0f, 0.0f, "", 2));
        addParam(floatParam ("macro_choke",      "CHOKE",       0.0f, 1.0f, 0.0f, "", 2));
        addParam(floatParam ("macro_decay",      "INFO DECAY",  0.0f, 1.0f, 0.0f, "", 2));
        addParam(floatParam ("macro_corrupt",    "CORRUPT MIX", 0.0f, 1.0f, 0.0f, "", 2));
        addParam(floatParam ("macro_kinetic",    "KINETIC MOD", 0.0f, 1.0f, 0.0f, "", 2));
        addParam(floatParam ("macro_resonator",  "RESONATOR",   0.0f, 1.0f, 0.0f, "", 2));
        addParam(floatParam ("macro_gate",       "GATE CARVE",  0.0f, 1.0f, 0.0f, "", 2));
        addParam(floatParam ("macro_smash",      "SMASH",       0.0f, 1.0f, 0.0f, "", 2));
        addParam(floatParam ("filter_mix",       "Filt Mix",    0.0f, 1.0f, 1.0f, "", 2));
        {
            auto p = floatParam("tg_floor", "Gate Floor", 0.0f, 1.0f, 0.5f, "", 2);
            p.automatable = false;
            addParam(p);
        }
        addParam(floatParam ("sc_mix",       "SC Mix",     0.0f, 1.0f, 1.0f, "", 2));
        {
            auto p = floatParam("sc_in_meter", "SC In", 0.0f, 1.0f, 0.0f, "", 2);
            p.automatable = false;
            addParam(p);
        }
        {
            auto p = floatParam("sc_duck_meter", "SC Duck", 0.0f, 1.0f, 0.0f, "", 2);
            p.automatable = false;
            addParam(p);
        }
        addParam(floatParam ("sc_ratio",     "SC Ratio",   1.0f, 20.0f, 4.0f, ":1", 1));
        addParam(boolParam  ("sc_tgt_drive", "SC Drive",   false));
        addParam(boolParam  ("sc_tgt_cut",   "SC Cut",     false));
        addParam(boolParam  ("sc_tgt_gain",  "SC Gain",    true));
        addParam(boolParam  ("sc_tgt_reso",  "SC Reso",    false));
        {
            auto p = floatParam("lim_gr_meter", "Lim GR", 0.0f, 1.0f, 0.0f, "", 2);
            p.automatable = false;
            addParam(p);
        }
        addParam(boolParam  ("scv_active",   "S-Curve Active", false));
        {
            auto p = floatParam("decay_viz", "Decay Viz", 0.0f, 1.0f, 0.0f, "", 2);
            p.automatable = false;
            addParam(p);
        }
        for (int i = 0; i < 16; ++i) {
            const std::string id = "tg_step_" + std::to_string(i);
            const std::string name = "Step " + std::to_string(i + 1);
            addParam(boolParam(id.c_str(), name.c_str(),
                               (kGateMasks[0] & (1u << i)) != 0));
        }
        addParam(floatParam("tg_offset", "Gate Offset", -100.0f, 100.0f, 0.0f, "%", 0));
        addParam(boolParam ("in_auto",   "Auto In",     true));
        addParam(floatParam("in_target", "In Target",   -24.0f, 0.0f, -9.0f, "dB", 1));
        addParam(floatParam("in_trim",     "In Trim",   -18.0f, 24.0f, 0.0f, "dB", 1));
        addParam(floatParam("env_cut",     "Env Cut",   -1.0f, 1.0f, 0.0f, "", 2));
        addParam(floatParam("env_drive",   "Env Drive", -1.0f, 1.0f, 0.0f, "", 2));
        addParam(boolParam ("filt_pre_sat","Flt->Sat",  false));
        addParam(floatParam("env_a_res",   "Env A Res", -1.0f, 1.0f, 0.0f, "", 2));
        addParam(floatParam("env_b_cut",   "Env B Cut", -1.0f, 1.0f, 0.0f, "", 2));
        addParam(floatParam("env_l1_rate", "Env L1 Spd",-1.0f, 1.0f, 0.0f, "", 2));
        addParam(floatParam("env_lfo_amt", "Env LFO Amt",-1.0f, 1.0f, 0.0f, "", 2));

        if (numParams() != kNumParams) mpc::log("Mushin: parameter count mismatch");

        for (int i = 0; i < kNumParams; ++i) {
            baseValues_[i] = get(i);
        }

        addBuiltInPrograms();
    }

    void parameterChanged(int index, float plainValue) override
    {
        if (index < 0 || index >= kNumParams) return;
        if (isBulkLoading()) {
            baseValues_[index] = plainValue;
            return;
        }
        if (updatingFromMacro_) return;

        if (index >= kMacroExhaustion && index <= kMacroSmash) {
            baseValues_[index] = plainValue;
            applyMacros();
        } else if (index == kTgFloor) {
            const float newDepth = mpc::clamp((1.0f - plainValue) * 100.0f, 0.0f, 100.0f);
            baseValues_[kTgDepth] = newDepth;
            updatingFromMacro_ = true;
            setTargetParam(kTgDepth, newDepth);
            updatingFromMacro_ = false;
        } else if (index == kTgPattern) {
            baseValues_[kTgPattern] = plainValue;
            const int patIdx = mpc::clamp((int)std::lround(plainValue), 0, 127);
            const uint16_t mask = kGateMasks[patIdx];
            updatingFromMacro_ = true;
            for (int i = 0; i < 16; ++i) {
                const float v = (mask & (1u << i)) ? 1.0f : 0.0f;
                baseValues_[kTgStep0 + i] = v;
                setTargetParam(kTgStep0 + i, v);
            }
            updatingFromMacro_ = false;
        } else {
            updateBaseFromTarget(index, plainValue);
            if (index == kTgDepth) {
                updatingFromMacro_ = true;
                setTargetParam(kTgFloor, mpc::clamp(1.0f - plainValue * 0.01f, 0.0f, 1.0f));
                updatingFromMacro_ = false;
            } else if (index == kRouting && get(kMacroChoke) > 0.02f) {
                applyMacros();
            }
        }
    }

    void saveExtraState(std::vector<uint8_t>& out) override
    {
        out.clear();
        out.push_back('M'); out.push_back('S'); out.push_back('H'); out.push_back('B');
        const uint32_t ver = 1;
        for (int b = 0; b < 4; ++b) out.push_back((uint8_t)(ver >> (8 * b)));
        for (int i = 0; i < kNumParams; ++i) {
            uint32_t bits = 0;
            std::memcpy(&bits, &baseValues_[i], 4);
            for (int b = 0; b < 4; ++b) out.push_back((uint8_t)(bits >> (8 * b)));
        }
    }

    void loadExtraState(const uint8_t* data, size_t size) override
    {
        const size_t expected = 8 + (size_t)kNumParams * 4;
        if (data == nullptr || size < expected || std::memcmp(data, "MSHB", 4) != 0) {
            return;
        }
        uint32_t ver = 0;
        for (int b = 0; b < 4; ++b) ver |= (uint32_t)data[4 + b] << (8 * b);
        if (ver != 1) return;
        size_t pos = 8;
        for (int i = 0; i < kNumParams; ++i) {
            uint32_t bits = 0;
            for (int b = 0; b < 4; ++b) bits |= (uint32_t)data[pos + (size_t)b] << (8 * b);
            pos += 4;
            std::memcpy(&baseValues_[i], &bits, 4);
        }
    }

    void prepare(double sampleRate, int /*maxBlockSize*/) override
    {
        inputStager_.prepare(sampleRate);
        envFollower_.prepare(sampleRate);
        waveshaper_.prepare(sampleRate);
        filterA_.prepare(sampleRate);
        filterB_.prepare(sampleRate);
        lfo1_.setSampleRate(sampleRate);
        lfo2_.setSampleRate(sampleRate);
        sidechain_.prepare(sampleRate);
        noiseOsc_.prepare(sampleRate);
        delay_.prepare(sampleRate);
        limiter_.prepare(sampleRate);

        smoothedGain_.setTime(50.0f, sampleRate);
        smoothedMix_.setTime(50.0f, sampleRate);
        smoothedFilterMix_.setTime(50.0f, sampleRate);
        smoothedScMix_.setTime(50.0f, sampleRate);
        smoothedQeDepth_.setTime(50.0f, sampleRate);
        smoothedQeMix_.setTime(50.0f, sampleRate);
        smoothedDelayTime_.setTime(50.0f, sampleRate);
        smoothedDelayFb_.setTime(50.0f, sampleRate);
        smoothedDelayMix_.setTime(50.0f, sampleRate);
        smoothedLimDrive_.setTime(50.0f, sampleRate);
        smoothedLimCeil_.setTime(50.0f, sampleRate);
        smoothedLimRel_.setTime(50.0f, sampleRate);
        smoothedLimMix_.setTime(50.0f, sampleRate);

        reset();
    }

    void reset() override
    {
        inputStager_.setTargetDb(get(kInTarget));
        inputStager_.setTrimDb(get(kInTrim));
        inputStager_.reset();
        inputStager_.snapTrim(get(kInTrim));
        envFollower_.reset();
        waveshaper_.setDrive(mpc::clamp(get(kDrive), 1.0f, 32.0f));
        waveshaper_.setExhaustion(getBool(kExhaustion));
        waveshaper_.setThreshold(mpc::dsp::dbToGain(get(kThreshold)));
        waveshaper_.setAutoGain(getBool(kAutoGain));
        waveshaper_.reset();
        waveshaper_.snapToCurrent();
        filterA_.reset();
        filterB_.reset();
        lfo1_.reset();
        lfo2_.reset();
        sidechain_.reset();
        noiseOsc_.reset();
        tranceGate_.reset();
        quantError_.reset();
        delay_.reset();
        limiter_.reset();

        smoothedGain_.snap(get(kGain));
        smoothedMix_.snap(get(kMix));
        smoothedFilterMix_.snap(get(kFilterMix));
        smoothedScMix_.snap(get(kScMix));
        smoothedQeDepth_.snap(get(kQeDepth));
        smoothedQeMix_.snap(get(kQeMix));
        smoothedDelayTime_.snap(get(kDelayTime));
        smoothedDelayFb_.snap(get(kDelayFeedback));
        smoothedDelayMix_.snap(get(kDelayMix));
        smoothedLimDrive_.snap(get(kLimDrive));
        smoothedLimCeil_.snap(get(kLimCeiling));
        smoothedLimRel_.snap(get(kLimRelease));
        smoothedLimMix_.snap(get(kLimMix));
        blocksSinceReset_ = 0;
        lastSentInMeter_ = -1.0f;
        lastSentDuckMeter_ = -1.0f;
        lastSentLimGrMeter_ = -1.0f;
        lastSentDecayViz_ = -1.0f;
    }

    void process(mpc::ProcessContext& ctx) override
    {
        const double sr = ctx.sampleRate;
        const int n = ctx.numFrames;

        // 1. Update per-block parameters (macros already reflected into underlying params)
        const bool autoInActive = getBool(kAutoIn);
        inputStager_.setTargetDb(get(kInTarget));
        inputStager_.setTrimDb(get(kInTrim));

        waveshaper_.setDrive(mpc::clamp(get(kDrive), 1.0f, 32.0f));
        waveshaper_.setExhaustion(getBool(kExhaustion));
        waveshaper_.setThreshold(mpc::dsp::dbToGain(get(kThreshold)));
        waveshaper_.setAutoGain(getBool(kAutoGain));

        const float baseFACutoff = mpc::clamp(get(kFACutoff), 20.0f, 20000.0f);
        const float baseFAReso = mpc::clamp(get(kFAReso), 0.0f, 0.98f);
        const float baseFADrive = mpc::clamp(get(kFADrive), 1.0f, 10.0f);
        const float baseFAGrit = mpc::clamp(get(kFAGrit), 0.0f, 1.0f);
        filterA_.setDrive(baseFADrive);
        filterA_.setGrit(baseFAGrit);
        filterA_.setType(getInt(kFAType));
        filterA_.setMode(getInt(kFAMode));

        const float baseFBCutoff = mpc::clamp(get(kFBCutoff), 20.0f, 20000.0f);
        const float baseFBReso = mpc::clamp(get(kFBReso), 0.0f, 0.98f);
        const float baseFBDrive = mpc::clamp(get(kFBDrive), 1.0f, 10.0f);
        const float baseFBGrit = mpc::clamp(get(kFBGrit), 0.0f, 1.0f);
        filterB_.setDrive(baseFBDrive);
        filterB_.setGrit(baseFBGrit);
        filterB_.setType(getInt(kFBType));
        filterB_.setMode(getInt(kFBMode));

        const int filterRouting = getInt(kRouting);
        const bool filtPreSat = getBool(kFiltPreSat);
        const float envCutAmt = get(kEnvCut);
        const float envDriveAmt = get(kEnvDrive);
        const float envAResAmt = get(kEnvARes);
        const float envBCutAmt = get(kEnvBCut);
        const float envL1RateAmt = get(kEnvL1Rate);
        const float envLfoAmt = get(kEnvLfoAmt);
        const bool envCutActive = (std::fabs(envCutAmt) > 1.0e-4f);
        const bool envDriveActive = (std::fabs(envDriveAmt) > 1.0e-4f);
        const bool envAResActive = (std::fabs(envAResAmt) > 1.0e-4f);
        const bool envBCutActive = (std::fabs(envBCutAmt) > 1.0e-4f);
        const bool envL1RateActive = (std::fabs(envL1RateAmt) > 1.0e-4f);
        const bool envLfoActive = (std::fabs(envLfoAmt) > 1.0e-4f);
        const bool envActive = envCutActive || envDriveActive || envAResActive || envBCutActive || envL1RateActive || envLfoActive;

        const float baseLfo1Freq = mpc::clamp(get(kLfo1Freq), 0.1f, 25.0f);
        lfo1_.setFrequency(baseLfo1Freq);
        lfo1_.setShape(getInt(kLfo1Wave));
        lfo2_.setFrequency(mpc::clamp(get(kLfo2Freq), 0.1f, 25.0f));
        lfo2_.setShape(getInt(kLfo2Wave));

        float mod1[6], mod2[6];
        for (int t = 0; t < 6; ++t) {
            mod1[t] = get(kModL1T1 + t);
            mod2[t] = get(kModL2T1 + t);
        }

        const bool scDriveTarget = getBool(kScTargetDrive);
        const bool scCutTarget   = getBool(kScTargetCut);
        const bool scGainTarget  = getBool(kScTargetGain);
        const bool scResoTarget  = getBool(kScTargetReso);

        sidechain_.setActive(getBool(kScActive) && (scDriveTarget || scCutTarget || scGainTarget || scResoTarget));
        sidechain_.setSource(getInt(kScSource));
        sidechain_.setTarget(getInt(kScTarget));
        sidechain_.setParameters(
            get(kScAttack), get(kScRelease),
            getInt(kScMode) == 1,
            get(kScThresh), get(kScAmount) * 0.01f,
            get(kScHpf), get(kScLpf),
            get(kScRatio)
        );

        const bool scvActive = getBool(kScvActive);
        smoothedGain_.setTarget(std::max(0.0f, get(kGain)));
        smoothedMix_.setTarget(get(kMix));
        smoothedFilterMix_.setTarget(get(kFilterMix));
        smoothedScMix_.setTarget(get(kScMix));

        const bool noiseActive = getBool(kNoiseActive);
        const int noiseRoute = getInt(kNoiseRoute);
        const float noiseGain = mpc::dsp::dbToGain(get(kNoiseLevel));
        const float fmAmount = get(kNoiseFmMod);
        const bool needNoise = noiseActive || (fmAmount > 1.0e-4f);
        if (needNoise) {
            noiseOsc_.setType(getInt(kNoiseType));
            noiseOsc_.setFrequency(get(kNoiseFreq));
        }

        const bool qeActive = getBool(kQeActive);
        float targetQeDepth = get(kQeDepth);
        if (getBool(kQeLink) && getBool(kExhaustion)) targetQeDepth = 4.0f;
        smoothedQeDepth_.setTarget(targetQeDepth);
        smoothedQeMix_.setTarget(get(kQeMix));
        const int downsampleFactor = std::max(1, getInt(kQeDownsample));

        const bool delayActive = getBool(kDelayActive);
        smoothedDelayTime_.setTarget(get(kDelayTime));
        smoothedDelayFb_.setTarget(get(kDelayFeedback));
        smoothedDelayMix_.setTarget(get(kDelayMix));
        const bool delayPingPong = getBool(kDelayPingPong);
        const bool delaySync = getBool(kDelaySync);
        const int delayTempoVal = getInt(kDelayTempo);

        const bool limActive = getBool(kLimActive);
        const int limMode = getInt(kLimMode);
        smoothedLimDrive_.setTarget(mpc::clamp(get(kLimDrive), 0.0f, 24.0f));
        smoothedLimCeil_.setTarget(get(kLimCeiling));
        smoothedLimRel_.setTarget(get(kLimRelease));
        smoothedLimMix_.setTarget(get(kLimMix));

        // Trance Gate block params
        const float bpm = mpc::dsp::safeBpm(ctx.transport.bpm);
        if (ctx.transport.playing) {
            const double secPos = ctx.transport.ppqPos * (60.0 / (double)bpm);
            tranceGate_.setCurrentSamplePos(secPos * sr);
        }
        double rateFactor = 4.0;
        const int rateIdx = getInt(kTgRate);
        if (rateIdx == 1) rateFactor = 2.0;
        else if (rateIdx == 2) rateFactor = 1.0;

        const double stepSamples = (60.0 / ((double)bpm * rateFactor)) * sr;
        const double cycleSamples = stepSamples * 16.0;
        uint16_t patternMask = 0;
        for (int i = 0; i < 16; ++i) {
            if (getBool(kTgStep0 + i)) patternMask |= (uint16_t)(1u << i);
        }

        double startSamples = (get(kTgStart) * 0.001) * sr;
        double endSamples = (get(kTgEnd) * 0.001) * sr;
        const float tgHoldPct = get(kTgHold);
        const double holdWidthSamples = stepSamples * (tgHoldPct * 0.01);
        const double maxTrans = stepSamples * 0.5;
        if (startSamples > maxTrans) startSamples = maxTrans;
        if (endSamples > maxTrans) endSamples = maxTrans;
        double attackEnd = startSamples;
        double decayStart = holdWidthSamples - endSamples;
        if (decayStart < attackEnd) {
            attackEnd = decayStart = holdWidthSamples * 0.5;
        }
        const float tgDepthPct = get(kTgDepth);
        const float baseAtten = 1.0f - tgDepthPct * 0.01f;
        const float tgMix = get(kTgMix);
        const double tgOffsetSamples = (get(kTgOffset) * 0.01) * stepSamples;
        const bool tgActive = getBool(kTgActive) && (stepSamples > 0.0);

        const double beatSamples = (60.0 / (double)bpm) * sr;
        const double scTestPulseSamples = 0.014 * sr;
        const bool scIsTest = sidechain_.isActive() && (sidechain_.getSource() == SidechainProcessor::Source::TestSignal);
        const double scBaseBeatPos = ctx.transport.playing
            ? (ctx.transport.ppqPos * beatSamples)
            : sidechain_.testPhaseSamples();

        const float* inL = ctx.inputs[0];
        const float* inR = (ctx.numInputs > 1 && ctx.inputs[1]) ? ctx.inputs[1] : ctx.inputs[0];
        float* outL = ctx.outputs[0];
        float* outR = (ctx.numOutputs > 1 && ctx.outputs[1]) ? ctx.outputs[1] : ctx.outputs[0];

        // Cache exp/pow values for 16-sample sub-blocks
        float qeSteps = 1.0f;
        float lastQeDepth = -1.0f;
        float limDriveGain = 1.0f, limCeilGain = 1.0f, limRelCoef = 0.99f;
        float blockPeakIn = 0.0f;
        float blockPeakDuck = 0.0f;
        float blockPeakLimGr = 0.0f;

        // 2. Sample loop
        for (int s = 0; s < n; ++s) {
            const float mixVal = smoothedMix_.process();
            const float filterMixVal = smoothedFilterMix_.process();
            const float scMixVal = smoothedScMix_.process();
            const float gainVal = smoothedGain_.process();
            const float curQeDepth = smoothedQeDepth_.process();
            const float curQeMix = smoothedQeMix_.process();
            const float curDelayTime = smoothedDelayTime_.process();
            const float curDelayFb = smoothedDelayFb_.process();
            const float curDelayMix = smoothedDelayMix_.process();
            const float curLimDrive = smoothedLimDrive_.process();
            const float curLimCeil = smoothedLimCeil_.process();
            const float curLimRel = smoothedLimRel_.process();
            const float curLimMix = smoothedLimMix_.process();

            float wsDrive, wsThresh;
            waveshaper_.advanceSmoothers(wsDrive, wsThresh);

            float dryL = inL[s];
            float dryR = inR[s];
            inputStager_.processSample(dryL, dryR, autoInActive);
            const float stagedPeak = std::max(std::fabs(dryL), std::fabs(dryR));
            blockPeakIn = std::max(blockPeakIn, stagedPeak);
            const float envVal = envActive ? envFollower_.processSample(stagedPeak) : 0.0f;

            float rawGen = 0.0f;
            if (needNoise) {
                rawGen = noiseOsc_.nextSample();
            }
            const float genSample = noiseActive ? (rawGen * noiseGain) : 0.0f;
            const float fmMod = rawGen * fmAmount;

            float scMod = 0.0f;
            if (sidechain_.isActive()) {
                bool scTestPulse = false;
                if (scIsTest && beatSamples > 0.0) {
                    double inBeat = std::fmod(scBaseBeatPos + s, beatSamples);
                    if (inBeat < 0.0) inBeat += beatSamples;
                    scTestPulse = (inBeat < scTestPulseSamples);
                }
                scMod = sidechain_.processSample(0.5f * (dryL + dryR), scTestPulse) * scMixVal;
                blockPeakDuck = std::max(blockPeakDuck, std::fabs(scMod));
            }

            float tgGain = 1.0f;
            if (tgActive) {
                tgGain = tranceGate_.processSample(
                    tranceGate_.currentSamplePos() + s - tgOffsetSamples,
                    stepSamples, cycleSamples, patternMask,
                    holdWidthSamples, attackEnd, decayStart,
                    baseAtten, tgMix
                );
            }

            // Update LFOs every sample; update SVF coefficients every 16 samples to keep ARM CPU low
            const float lfo1Val = lfo1_.process();
            const float lfo2Val = lfo2_.process();

            if ((s & 15) == 0) {
                if (envL1RateActive) {
                    lfo1_.setFrequency(mpc::clamp(baseLfo1Freq * std::pow(2.0f, envL1RateAmt * envVal * 3.0f), 0.1f, 35.0f));
                }
                const float lfoScale = envLfoActive ? std::pow(2.0f, envLfoAmt * envVal * 2.0f) : 1.0f;
                float off[6];
                for (int t = 0; t < 6; ++t) {
                    off[t] = (lfo1Val * mod1[t] + lfo2Val * mod2[t]) * lfoScale;
                }
                off[0] += fmMod + envCutAmt * envVal;
                off[3] += fmMod + (envCutAmt + envBCutAmt) * envVal;

                float cutA = baseFACutoff * std::pow(2.0f, off[0] * 5.0f);
                float cutB = baseFBCutoff * std::pow(2.0f, off[3] * 5.0f);
                if (sidechain_.isActive() && scCutTarget) {
                    const float scCut = std::pow(2.0f, scMod * 6.0f);
                    cutA *= scCut;
                    cutB *= scCut;
                }

                const bool modA = (std::fabs(mod1[0]) > 1e-4f || std::fabs(mod2[0]) > 1e-4f || fmAmount > 1e-4f ||
                                   envCutActive || envAResActive || (sidechain_.isActive() && scCutTarget));
                const bool modB = (std::fabs(mod1[3]) > 1e-4f || std::fabs(mod2[3]) > 1e-4f || fmAmount > 1e-4f ||
                                   envCutActive || envBCutActive || (sidechain_.isActive() && scCutTarget));

                float resoA = baseFAReso + off[1] + envAResAmt * envVal * 0.90f;
                if (sidechain_.isActive() && scResoTarget) {
                    resoA += scMod * 0.5f;
                }

                if (envDriveActive) {
                    const float envDrvMul = std::pow(2.0f, envDriveAmt * envVal * 1.5f);
                    filterA_.setDrive(mpc::clamp(baseFADrive * envDrvMul, 1.0f, 10.0f));
                    filterB_.setDrive(mpc::clamp(baseFBDrive * envDrvMul, 1.0f, 10.0f));
                }
                filterA_.setGrit(mpc::clamp(baseFAGrit + off[2], 0.0f, 1.0f));
                filterA_.setCoefficients(cutA, mpc::clamp(resoA, 0.0f, 0.98f), !modA);
                filterB_.setGrit(mpc::clamp(baseFBGrit + off[5], 0.0f, 1.0f));
                filterB_.setCoefficients(cutB, mpc::clamp(baseFBReso + off[4], 0.0f, 1.0f), !modB);

                if (qeActive && curQeDepth != lastQeDepth) {
                    lastQeDepth = curQeDepth;
                    qeSteps = std::pow(2.0f, std::max(2.0f, curQeDepth) - 1.0f);
                }
                if (limActive) {
                    limDriveGain = mpc::dsp::dbToGain(curLimDrive);
                    limCeilGain = mpc::dsp::dbToGain(curLimCeil);
                    const float relSec = std::max(curLimRel, 10.0f) * 0.001f;
                    limRelCoef = std::exp(-1.0f / ((float)sr * relSec));
                }
            }

            float effDrive = wsDrive;
            if (envDriveActive) {
                effDrive = mpc::clamp(effDrive * std::pow(2.0f, envDriveAmt * envVal * 2.2f), 1.0f, 32.0f);
            }
            if (sidechain_.isActive() && scDriveTarget) {
                effDrive = mpc::clamp(effDrive * std::pow(2.0f, scMod * 2.5f), 1.0f, 32.0f);
            }

            float curGain = gainVal;
            if (sidechain_.isActive() && scGainTarget) {
                curGain *= std::max(0.0f, 1.0f + scMod);
            }
            curGain = std::max(0.0f, curGain);

            float wet[2] = { dryL, dryR };
            const float dry[2] = { dryL, dryR };

            auto runSatStage = [&]() noexcept {
                if (noiseActive && noiseRoute == 0) {
                    wet[0] += genSample;
                    wet[1] += genSample;
                }
                if (scvActive) {
                    wet[0] = waveshaper_.processSample(wet[0], effDrive, wsThresh);
                    wet[1] = waveshaper_.processSample(wet[1], effDrive, wsThresh);
                }
            };

            auto runFilterStage = [&]() noexcept {
                if (noiseActive && noiseRoute == 1) {
                    wet[0] += genSample;
                    wet[1] += genSample;
                }
                const float preL = wet[0];
                const float preR = wet[1];
                if (filterRouting == 0) {
                    wet[0] = filterB_.processSample(0, filterA_.processSample(0, preL));
                    wet[1] = filterB_.processSample(1, filterA_.processSample(1, preR));
                } else if (filterRouting == 1) {
                    wet[0] = 0.5f * (filterA_.processSample(0, preL) + filterB_.processSample(0, preL));
                    wet[1] = 0.5f * (filterA_.processSample(1, preR) + filterB_.processSample(1, preR));
                } else if (filterRouting == 2) {
                    // STEREO routing: Left -> Filter A, Right -> Filter B
                    wet[0] = filterA_.processSample(0, preL);
                    wet[1] = filterB_.processSample(1, preR);
                } else {
                    // M/S routing: Mid -> Filter A, Side -> Filter B
                    const float mid = filterA_.processSample(0, 0.5f * (preL + preR));
                    const float side = filterB_.processSample(1, 0.5f * (preL - preR));
                    wet[0] = mid + side;
                    wet[1] = mid - side;
                }
                wet[0] = preL + (wet[0] - preL) * filterMixVal;
                wet[1] = preR + (wet[1] - preR) * filterMixVal;
            };

            if (filtPreSat) {
                runFilterStage();
                runSatStage();
            } else {
                runSatStage();
                runFilterStage();
            }

            for (int ch = 0; ch < 2; ++ch) {
                float w = wet[ch];
                if (qeActive) {
                    w = quantError_.processSample(ch, w, curQeDepth, qeSteps, downsampleFactor, curQeMix);
                }
                if (noiseActive && noiseRoute == 2) w += genSample;

                float mixed = dry[ch] * (1.0f - mixVal) + w * mixVal;
                if (tgActive) mixed *= tgGain;

                float finalS = mixed * curGain;
                if (std::isnan(finalS) || std::isinf(finalS)) finalS = 0.0f;
                wet[ch] = finalS;
            }

            if (delayActive) {
                delay_.processSample(wet[0], wet[1], curDelayTime, curDelayFb, curDelayMix,
                                     delayPingPong, delaySync, bpm, delayTempoVal);
            }

            if (limActive) {
                limiter_.processSample(wet[0], wet[1], limDriveGain, curLimCeil, limCeilGain,
                                       limRelCoef, curLimMix, limMode);
                blockPeakLimGr = std::max(blockPeakLimGr, limiter_.currentGR());
            }

            outL[s] = wet[0];
            outR[s] = wet[1];
        }

        if (!ctx.transport.playing && tgActive) {
            tranceGate_.advanceFreeRunningClock(n, cycleSamples);
        }
        if (!ctx.transport.playing && scIsTest && beatSamples > 0.0) {
            sidechain_.testPhaseSamples() = std::fmod(sidechain_.testPhaseSamples() + n, beatSamples);
        }

        // Update read-only Pump & Limiter meters after initial settle (~30 Hz host notify)
        if (++blocksSinceReset_ > 4) {
            const float inDb = 20.0f * std::log10(std::max(blockPeakIn, 1.0e-4f));
            const float inMeter01 = mpc::clamp((inDb + 60.0f) / 60.0f, 0.0f, 1.0f);
            const float duckMeter01 = mpc::clamp(blockPeakDuck, 0.0f, 1.0f);
            const float limGr01 = mpc::clamp(blockPeakLimGr, 0.0f, 1.0f);
            float decayViz01 = 0.0f;
            if (qeActive) {
                const float bitSev = mpc::clamp((16.0f - targetQeDepth) / 14.0f, 0.0f, 1.0f);
                const float dsSev  = mpc::clamp((float)(downsampleFactor - 1) / 11.0f, 0.0f, 1.0f);
                const float rawDecay = mpc::clamp(0.55f * bitSev + 0.45f * dsSev, 0.0f, 1.0f);
                decayViz01 = mpc::clamp(rawDecay * get(kQeMix), 0.0f, 1.0f);
            }
            set(kScInMeter, inMeter01);
            set(kScDuckMeter, duckMeter01);
            set(kLimGrMeter, limGr01);
            set(kDecayViz, decayViz01);
            if (currentProgram() != lastProgram_) {
                lastProgram_ = currentProgram();
                requestDisplayUpdate();
                for (int i = 0; i < kNumParams; ++i) {
                    if (param(i).automatable) {
                        notifyHostParameterChanged(i);
                    }
                }
            }
            if (++meterNotifyCounter_ >= 12) {
                meterNotifyCounter_ = 0;
                if (std::fabs(inMeter01 - lastSentInMeter_) > 0.005f) {
                    lastSentInMeter_ = inMeter01;
                    notifyHostParameterChanged(kScInMeter);
                }
                if (std::fabs(duckMeter01 - lastSentDuckMeter_) > 0.005f) {
                    lastSentDuckMeter_ = duckMeter01;
                    notifyHostParameterChanged(kScDuckMeter);
                }
                if (std::fabs(limGr01 - lastSentLimGrMeter_) > 0.005f) {
                    lastSentLimGrMeter_ = limGr01;
                    notifyHostParameterChanged(kLimGrMeter);
                }
                if (std::fabs(decayViz01 - lastSentDecayViz_) > 0.005f) {
                    lastSentDecayViz_ = decayViz01;
                    notifyHostParameterChanged(kDecayViz);
                }
            }
        }
    }

private:
    void setTargetParam(int idx, float val)
    {
        const float clamped = mpc::clamp(val, param(idx).min, param(idx).max);
        if (std::fabs(clamped - get(idx)) <= 1e-5f) return;
        set(idx, clamped);
        notifyHostParameterChanged(idx);
    }

    void applyMacros()
    {
        updatingFromMacro_ = true;

        const float mExh     = get(kMacroExhaustion);
        const float mChoke   = get(kMacroChoke);
        const float mDecay   = get(kMacroDecay);
        const float mCorrupt = get(kMacroCorrupt);
        const float mKinetic = get(kMacroKinetic);
        const float mReso    = get(kMacroResonator);
        const float mGate    = get(kMacroGate);
        const float mSmash   = get(kMacroSmash);

        // EXHAUSTION
        setTargetParam(kScvActive, (baseValues_[kScvActive] >= 0.5f || mExh > 0.02f) ? 1.0f : 0.0f);
        setTargetParam(kDrive, mpc::clamp(baseValues_[kDrive] + 12.0f * mExh, 1.0f, 20.0f));
        setTargetParam(kExhaustion, (baseValues_[kExhaustion] >= 0.5f || mExh > 0.65f) ? 1.0f : 0.0f);
        setTargetParam(kGain, std::max(0.0f, baseValues_[kGain] * (1.0f - 0.35f * mExh)));

        // CHOKE + KINETIC MOD + RESONATOR (Filters)
        setTargetParam(kFACutoff, mpc::clamp(baseValues_[kFACutoff] * std::pow(2.0f, -7.0f * mChoke - 3.5f * mKinetic), 20.0f, 20000.0f));
        setTargetParam(kFAReso, mpc::clamp(baseValues_[kFAReso] + 0.50f * mChoke + 0.35f * mKinetic, 0.0f, 0.98f));
        setTargetParam(kFAGrit, mpc::clamp(baseValues_[kFAGrit] + 0.5f * mReso, 0.0f, 1.0f));

        const bool isParallelOrStereo = ((int)std::lround(baseValues_[kRouting]) >= 1);
        const bool chokeBand = (!isParallelOrStereo && mChoke > 0.02f && (int)std::lround(baseValues_[kFBMode]) == 0 && baseValues_[kFBCutoff] > 15000.0f);
        const float fbCutoff = chokeBand
            ? mpc::clamp(20.0f * std::pow(2.0f, 5.0f * mChoke), 20.0f, 20000.0f)
            : mpc::clamp(baseValues_[kFBCutoff] * std::pow(2.0f, (isParallelOrStereo ? -7.0f : -4.0f) * mChoke - 2.5f * mKinetic), 20.0f, 20000.0f);
        setTargetParam(kFBCutoff, fbCutoff);
        setTargetParam(kFBReso, mpc::clamp(baseValues_[kFBReso] + 0.50f * mChoke + 0.35f * mKinetic, 0.0f, 0.98f));
        setTargetParam(kFBGrit, mpc::clamp(baseValues_[kFBGrit] + 0.5f * mReso, 0.0f, 1.0f));
        setTargetParam(kFBMode, chokeBand ? 1.0f : baseValues_[kFBMode]);

        // KINETIC MOD (LFOs & Mod Matrix)
        setTargetParam(kLfo1Freq, mpc::clamp(baseValues_[kLfo1Freq] + 6.0f * mKinetic, 0.1f, 20.0f));
        setTargetParam(kLfo2Freq, mpc::clamp(baseValues_[kLfo2Freq] + 8.5f * mKinetic, 0.1f, 20.0f));
        setTargetParam(kModL1T1, mpc::clamp(baseValues_[kModL1T1] + 0.70f * mKinetic, -1.0f, 1.0f));
        setTargetParam(kModL1T2, mpc::clamp(baseValues_[kModL1T2] + 0.25f * mKinetic, -1.0f, 1.0f));
        setTargetParam(kModL2T4, mpc::clamp(baseValues_[kModL2T4] + 0.70f * mKinetic, -1.0f, 1.0f));
        setTargetParam(kModL2T5, mpc::clamp(baseValues_[kModL2T5] + 0.35f * mKinetic, -1.0f, 1.0f));

        // CORRUPT MIX
        setTargetParam(kMix, baseValues_[kMix] + (1.0f - baseValues_[kMix]) * mCorrupt);
        const bool baseNoiseOn = (baseValues_[kNoiseActive] >= 0.5f);
        setTargetParam(kNoiseActive, (baseNoiseOn || mCorrupt > 0.05f) ? 1.0f : 0.0f);
        setTargetParam(kNoiseRoute, (mCorrupt > 0.05f && !baseNoiseOn) ? 1.0f : baseValues_[kNoiseRoute]);
        setTargetParam(kNoiseLevel, mpc::clamp(baseValues_[kNoiseLevel] + 24.0f * mCorrupt, -60.0f, 0.0f));

        // INFO DECAY
        setTargetParam(kQeActive, (baseValues_[kQeActive] >= 0.5f || mDecay > 0.02f) ? 1.0f : 0.0f);
        setTargetParam(kQeDepth, (mDecay > 0.02f) ? std::min(baseValues_[kQeDepth], 16.0f - 14.0f * mDecay) : baseValues_[kQeDepth]);
        setTargetParam(kQeMix, std::max(baseValues_[kQeMix], mDecay));
        setTargetParam(kQeDownsample, (float)std::max((int)std::lround(baseValues_[kQeDownsample]), 1 + (int)(11.0f * mDecay)));

        // RESONATOR (Delay)
        const bool baseDelayOn = (baseValues_[kDelayActive] >= 0.5f);
        setTargetParam(kDelayActive, (baseDelayOn || mReso > 0.05f) ? 1.0f : 0.0f);
        setTargetParam(kDelayTime, (!baseDelayOn && mReso > 0.05f) ? (32.0f - 20.0f * mReso) : baseValues_[kDelayTime]);
        setTargetParam(kDelayFeedback, std::max(baseValues_[kDelayFeedback], 0.88f * mReso));
        setTargetParam(kDelayMix, std::max(baseValues_[kDelayMix], 0.45f * mReso));

        // GATE CARVE (at mGate=0.5: 4-On-Floor, Depth=88%, Mix=0.82, Start=8.6ms, Hold=48%, End=34ms)
        const bool baseTgOn = (baseValues_[kTgActive] >= 0.5f);
        const bool gateMacroOn = (mGate > 0.05f);
        setTargetParam(kTgActive, (baseTgOn || gateMacroOn) ? 1.0f : 0.0f);
        setTargetParam(kTgPattern, (gateMacroOn && !baseTgOn) ? 4.0f : baseValues_[kTgPattern]);
        setTargetParam(kTgStart, gateMacroOn
            ? (baseTgOn ? std::min(baseValues_[kTgStart], 12.2f - 7.2f * mGate) : (12.2f - 7.2f * mGate))
            : baseValues_[kTgStart]);
        setTargetParam(kTgHold, gateMacroOn
            ? (baseTgOn ? std::min(baseValues_[kTgHold], 68.0f - 40.0f * mGate) : (68.0f - 40.0f * mGate))
            : baseValues_[kTgHold]);
        setTargetParam(kTgEnd, gateMacroOn
            ? (baseTgOn ? std::min(baseValues_[kTgEnd], 52.0f - 36.0f * mGate) : (52.0f - 36.0f * mGate))
            : baseValues_[kTgEnd]);
        setTargetParam(kTgDepth, gateMacroOn
            ? (baseTgOn ? std::max(baseValues_[kTgDepth], 76.0f + 24.0f * mGate) : (76.0f + 24.0f * mGate))
            : baseValues_[kTgDepth]);
        setTargetParam(kTgFloor, mpc::clamp(1.0f - get(kTgDepth) * 0.01f, 0.0f, 1.0f));
        setTargetParam(kTgMix, gateMacroOn
            ? (baseTgOn ? std::max(baseValues_[kTgMix], 0.64f + 0.36f * mGate) : (0.64f + 0.36f * mGate))
            : baseValues_[kTgMix]);

        // SMASH
        setTargetParam(kLimActive, (baseValues_[kLimActive] >= 0.5f || mSmash > 0.02f) ? 1.0f : 0.0f);
        setTargetParam(kLimMode, (mSmash > 0.3f) ? 1.0f : baseValues_[kLimMode]);
        setTargetParam(kLimDrive, mpc::clamp(baseValues_[kLimDrive] + 18.0f * mSmash, 0.0f, 24.0f));
        setTargetParam(kLimMix, std::max(baseValues_[kLimMix], mSmash));

        updatingFromMacro_ = false;
    }

    void updateBaseFromTarget(int idx, float val)
    {
        const float mExh     = get(kMacroExhaustion);
        const float mChoke   = get(kMacroChoke);
        const float mCorrupt = get(kMacroCorrupt);
        const float mKinetic = get(kMacroKinetic);
        const float mReso    = get(kMacroResonator);
        const float mSmash   = get(kMacroSmash);

        switch (idx) {
            case kDrive:
                baseValues_[idx] = mpc::clamp(val - 12.0f * mExh, 1.0f, 20.0f);
                break;
            case kGain: {
                const float scale = 1.0f - 0.35f * mExh;
                baseValues_[idx] = (scale > 0.01f) ? mpc::clamp(val / scale, 0.0f, 2.0f) : val;
                break;
            }
            case kFACutoff:
                baseValues_[idx] = mpc::clamp(val * std::pow(2.0f, 7.0f * mChoke + 3.5f * mKinetic), 20.0f, 20000.0f);
                break;
            case kFAReso:
                baseValues_[idx] = mpc::clamp(val - 0.50f * mChoke - 0.35f * mKinetic, 0.0f, 1.0f);
                break;
            case kFAGrit:
                baseValues_[idx] = mpc::clamp(val - 0.5f * mReso, 0.0f, 1.0f);
                break;
            case kFBCutoff: {
                const bool isParallelOrStereo = ((int)std::lround(baseValues_[kRouting]) >= 1);
                const bool chokeBand = (!isParallelOrStereo && mChoke > 0.02f && (int)std::lround(baseValues_[kFBMode]) == 0 && baseValues_[kFBCutoff] > 15000.0f);
                baseValues_[idx] = chokeBand
                    ? val
                    : mpc::clamp(val * std::pow(2.0f, (isParallelOrStereo ? 7.0f : 4.0f) * mChoke + 2.5f * mKinetic), 20.0f, 20000.0f);
                break;
            }
            case kFBReso:
                baseValues_[idx] = mpc::clamp(val - 0.50f * mChoke - 0.35f * mKinetic, 0.0f, 1.0f);
                break;
            case kFBGrit:
                baseValues_[idx] = mpc::clamp(val - 0.5f * mReso, 0.0f, 1.0f);
                break;
            case kLfo1Freq:
                baseValues_[idx] = mpc::clamp(val - 6.0f * mKinetic, 0.1f, 20.0f);
                break;
            case kLfo2Freq:
                baseValues_[idx] = mpc::clamp(val - 8.5f * mKinetic, 0.1f, 20.0f);
                break;
            case kModL1T1:
                baseValues_[idx] = mpc::clamp(val - 0.70f * mKinetic, -1.0f, 1.0f);
                break;
            case kModL1T2:
                baseValues_[idx] = mpc::clamp(val - 0.25f * mKinetic, -1.0f, 1.0f);
                break;
            case kModL2T4:
                baseValues_[idx] = mpc::clamp(val - 0.70f * mKinetic, -1.0f, 1.0f);
                break;
            case kModL2T5:
                baseValues_[idx] = mpc::clamp(val - 0.35f * mKinetic, -1.0f, 1.0f);
                break;
            case kMix: {
                const float denom = 1.0f - mCorrupt;
                baseValues_[idx] = (denom > 0.01f) ? mpc::clamp((val - mCorrupt) / denom, 0.0f, 1.0f) : val;
                break;
            }
            case kNoiseLevel:
                baseValues_[idx] = mpc::clamp(val - 24.0f * mCorrupt, -60.0f, 0.0f);
                break;
            case kLimDrive:
                baseValues_[idx] = mpc::clamp(val - 18.0f * mSmash, 0.0f, 24.0f);
                break;
            default:
                baseValues_[idx] = val;
                break;
        }
    }

    void addPreset(const char* name, std::initializer_list<std::pair<int, float>> overrides)
    {
        std::vector<float> vals((size_t)numParams(), 0.0f);
        for (int i = 0; i < numParams(); ++i) {
            vals[(size_t)i] = get(i);
        }
        for (const auto& kv : overrides) {
            if (kv.first >= 0 && kv.first < numParams()) {
                vals[(size_t)kv.first] = kv.second;
                if (kv.first == kTgPattern) {
                    const uint16_t m = kGateMasks[mpc::clamp((int)std::lround(kv.second), 0, 127)];
                    for (int i = 0; i < 16; ++i) {
                        vals[(size_t)(kTgStep0 + i)] = (m & (1u << i)) ? 1.0f : 0.0f;
                    }
                } else if (kv.first == kTgDepth) {
                    vals[(size_t)kTgFloor] = mpc::clamp(1.0f - kv.second * 0.01f, 0.0f, 1.0f);
                }
            }
        }
        addProgram(name, std::move(vals));
    }

    void addBuiltInPrograms()
    {
        // CORE (00-05)
        addPreset("Init", {});

        addPreset("Warm Tape Sat", {
            {kScvActive, 1.0f}, {kDrive, 3.4f}, {kThreshold, -4.8f}, {kAutoGain, 1.0f}, {kMix, 0.85f},
            {kFACutoff, 6500.0f}, {kFAType, 0.0f}, {kDelayActive, 1.0f}, {kDelayTime, 240.0f},
            {kDelayFeedback, 0.25f}, {kDelayMix, 0.20f},
            {kLimActive, 1.0f}, {kLimMode, 0.0f}, {kLimDrive, 0.0f}, {kLimCeiling, -0.3f}
        });

        addPreset("Dual Resonator", {
            {kScvActive, 1.0f}, {kDrive, 1.9f}, {kThreshold, -2.0f}, {kAutoGain, 1.0f},
            {kFACutoff, 900.0f}, {kFAReso, 0.65f}, {kFAType, 2.0f},
            {kFBCutoff, 2400.0f}, {kFBReso, 0.55f}, {kFAMode, 2.0f}, {kFBMode, 0.0f},
            {kRouting, 1.0f}, {kLfo1Freq, 0.40f}, {kModL1T1, 0.35f}, {kModL1T4, -0.30f},
            {kLimActive, 1.0f}, {kLimMode, 0.0f}, {kLimDrive, 0.0f}, {kLimCeiling, -0.3f}
        });

        addPreset("Crushed Velvet", {
            {kScvActive, 1.0f}, {kDrive, 4.0f}, {kThreshold, -5.5f}, {kAutoGain, 1.0f},
            {kQeActive, 1.0f}, {kQeDepth, 6.0f}, {kQeDownsample, 4.0f}, {kQeMix, 0.70f},
            {kFACutoff, 4200.0f}, {kFAReso, 0.30f}, {kFADrive, 1.4f},
            {kLimActive, 1.0f}, {kLimMode, 0.0f}, {kLimDrive, 0.0f}, {kLimCeiling, -0.3f}
        });

        addPreset("Rhythmic Gater", {
            {kScvActive, 1.0f}, {kDrive, 2.2f}, {kThreshold, -2.5f}, {kAutoGain, 1.0f},
            {kTgActive, 1.0f}, {kTgPattern, 4.0f}, {kTgRate, 0.0f},
            {kTgHold, 65.0f}, {kTgDepth, 90.0f}, {kTgFloor, 0.10f},
            {kDelayActive, 1.0f}, {kDelaySync, 1.0f}, {kDelayTempo, 1.0f}, {kDelayPingPong, 1.0f}, {kDelayMix, 0.32f},
            {kLimActive, 1.0f}, {kLimMode, 0.0f}, {kLimDrive, 0.0f}, {kLimCeiling, -0.3f}
        });

        addPreset("Mushin Pumper", {
            {kAutoIn, 1.0f}, {kInTarget, -8.5f},
            {kScvActive, 1.0f}, {kDrive, 2.0f}, {kThreshold, -2.5f}, {kAutoGain, 1.0f},
            {kFACutoff, 11000.0f}, {kFAReso, 0.35f},
            {kScActive, 1.0f}, {kScSource, 1.0f}, {kScMode, 0.0f}, {kScRatio, 8.0f},
            {kScThresh, -18.0f}, {kScAmount, -78.0f}, {kScAttack, 2.0f}, {kScRelease, 190.0f},
            {kScTargetGain, 1.0f}, {kScTargetCut, 1.0f}
        });

        // PSY: Psytrance & Darkpsy Chains (06-12)
        addPreset("PSY: Init", {
            {kAutoIn, 1.0f}, {kInTarget, -8.5f},
            {kScvActive, 1.0f}, {kDrive, 2.2f}, {kThreshold, -2.5f}, {kAutoGain, 1.0f},
            {kFAType, 2.0f}, {kFAMode, 0.0f}, {kFACutoff, 2600.0f}, {kFAReso, 0.48f}, {kFADrive, 1.5f},
            {kFBType, 0.0f}, {kFBMode, 1.0f}, {kFBCutoff, 42.0f}, {kFBReso, 0.22f},
            {kDelayActive, 1.0f}, {kDelaySync, 1.0f}, {kDelayTempo, 0.0f}, {kDelayPingPong, 1.0f},
            {kDelayFeedback, 0.34f}, {kDelayMix, 0.20f},
            {kLimActive, 1.0f}, {kLimMode, 1.0f}, {kLimDrive, 1.5f}, {kLimCeiling, -0.3f}
        });

        addPreset("PSY: Forest FM Lead", {
            {kAutoIn, 1.0f}, {kInTarget, -8.0f},
            {kRouting, 1.0f}, {kFilterMix, 0.88f},
            {kScvActive, 1.0f}, {kDrive, 3.0f}, {kThreshold, -4.0f}, {kAutoGain, 1.0f},
            {kNoiseType, 4.0f}, {kNoiseFreq, 25.0f}, {kNoiseFmMod, 0.34f},
            {kFAType, 2.0f}, {kFAMode, 2.0f}, {kFACutoff, 1150.0f}, {kFAReso, 0.72f}, {kFADrive, 2.1f}, {kFAGrit, 0.24f},
            {kFBType, 2.0f}, {kFBMode, 0.0f}, {kFBCutoff, 2900.0f}, {kFBReso, 0.52f}, {kFBDrive, 1.5f},
            {kLfo1Freq, 6.2f}, {kLfo1Wave, 4.0f}, {kModL1T1, 0.54f},
            {kLfo2Freq, 0.45f}, {kLfo2Wave, 0.0f}, {kModL2T4, -0.45f}, {kModL2T2, 0.20f},
            {kEnvCut, 0.22f}, {kEnvDrive, 0.15f},
            {kDelayActive, 1.0f}, {kDelaySync, 1.0f}, {kDelayTempo, 0.0f}, {kDelayPingPong, 1.0f},
            {kDelayFeedback, 0.45f}, {kDelayMix, 0.28f},
            {kLimActive, 1.0f}, {kLimMode, 1.0f}, {kLimDrive, 2.5f}, {kLimCeiling, -0.3f}
        });

        addPreset("PSY: Rolling Bass 16", {
            {kAutoIn, 1.0f}, {kInTarget, -7.5f},
            {kScvActive, 1.0f}, {kDrive, 2.2f}, {kThreshold, -3.5f}, {kAutoGain, 1.0f}, {kMix, 0.80f},
            {kFAType, 2.0f}, {kFAMode, 0.0f}, {kFACutoff, 1350.0f}, {kFAReso, 0.30f}, {kFADrive, 1.3f},
            {kFBType, 0.0f}, {kFBMode, 1.0f}, {kFBCutoff, 42.0f}, {kFBReso, 0.15f},
            {kLfo1Freq, 0.25f}, {kLfo1Wave, 0.0f}, {kModL1T1, 0.18f},
            {kScActive, 1.0f}, {kScSource, 1.0f}, {kScMode, 0.0f}, {kScRatio, 10.0f},
            {kScThresh, -16.0f}, {kScAmount, -78.0f}, {kScAttack, 2.0f}, {kScRelease, 95.0f},
            {kScTargetGain, 1.0f}, {kScTargetCut, 0.0f},
            {kLimActive, 1.0f}, {kLimMode, 0.0f}, {kLimDrive, 1.5f}, {kLimCeiling, -0.3f}
        });

        addPreset("PSY: Dark Squelch", {
            {kAutoIn, 1.0f}, {kInTarget, -8.0f},
            {kRouting, 1.0f}, {kFilterMix, 0.90f},
            {kScvActive, 1.0f}, {kDrive, 3.8f}, {kThreshold, -5.5f}, {kAutoGain, 1.0f},
            {kFAType, 2.0f}, {kFAMode, 0.0f}, {kFACutoff, 640.0f}, {kFAReso, 0.84f}, {kFADrive, 2.5f}, {kFAGrit, 0.28f},
            {kFBType, 1.0f}, {kFBMode, 2.0f}, {kFBCutoff, 1850.0f}, {kFBReso, 0.68f}, {kFBDrive, 1.6f},
            {kLfo1Freq, 2.2f}, {kLfo1Wave, 2.0f}, {kModL1T1, 0.70f},
            {kLfo2Freq, 0.65f}, {kLfo2Wave, 1.0f}, {kModL2T4, -0.55f},
            {kDelayActive, 1.0f}, {kDelaySync, 1.0f}, {kDelayTempo, 0.0f}, {kDelayPingPong, 1.0f},
            {kDelayFeedback, 0.36f}, {kDelayMix, 0.22f},
            {kLimActive, 1.0f}, {kLimMode, 1.0f}, {kLimDrive, 2.5f}, {kLimCeiling, -0.3f}
        });

        addPreset("PSY: Gate Grid 148", {
            {kAutoIn, 1.0f}, {kInTarget, -8.0f},
            {kRouting, 1.0f}, {kFilterMix, 0.88f},
            {kScvActive, 1.0f}, {kDrive, 2.8f}, {kThreshold, -3.5f}, {kAutoGain, 1.0f},
            {kFAType, 2.0f}, {kFAMode, 0.0f}, {kFACutoff, 2300.0f}, {kFAReso, 0.62f}, {kFADrive, 1.8f},
            {kFBType, 1.0f}, {kFBMode, 2.0f}, {kFBCutoff, 1450.0f}, {kFBReso, 0.56f},
            {kLfo1Freq, 0.80f}, {kLfo1Wave, 2.0f}, {kModL1T1, 0.55f},
            {kTgActive, 1.0f}, {kTgPattern, 25.0f}, {kTgRate, 0.0f}, {kTgOffset, 10.0f},
            {kTgDepth, 88.0f}, {kTgStart, 2.0f}, {kTgHold, 52.0f}, {kTgEnd, 12.0f},
            {kDelayActive, 1.0f}, {kDelaySync, 1.0f}, {kDelayTempo, 0.0f}, {kDelayPingPong, 1.0f},
            {kDelayFeedback, 0.40f}, {kDelayMix, 0.26f},
            {kLimActive, 1.0f}, {kLimMode, 1.0f}, {kLimDrive, 2.4f}, {kLimCeiling, -0.3f}
        });

        addPreset("PSY: Alien Formant", {
            {kAutoIn, 1.0f}, {kInTarget, -8.0f},
            {kRouting, 1.0f}, {kFilterMix, 0.90f},
            {kScvActive, 1.0f}, {kDrive, 3.2f}, {kThreshold, -4.5f}, {kAutoGain, 1.0f},
            {kNoiseType, 3.0f}, {kNoiseFreq, 95.0f}, {kNoiseFmMod, 0.30f},
            {kFAType, 1.0f}, {kFAMode, 0.0f}, {kFACutoff, 780.0f}, {kFAReso, 0.74f}, {kFADrive, 2.0f}, {kFAGrit, 0.24f},
            {kFBType, 2.0f}, {kFBMode, 2.0f}, {kFBCutoff, 2200.0f}, {kFBReso, 0.68f}, {kFBDrive, 1.5f},
            {kLfo1Freq, 4.2f}, {kLfo1Wave, 4.0f}, {kModL1T1, 0.62f},
            {kLfo2Freq, 0.85f}, {kLfo2Wave, 2.0f}, {kModL2T4, -0.55f},
            {kEnvBCut, -0.35f}, {kEnvL1Rate, 0.30f},
            {kDelayActive, 1.0f}, {kDelaySync, 1.0f}, {kDelayTempo, 0.0f}, {kDelayPingPong, 1.0f},
            {kDelayFeedback, 0.42f}, {kDelayMix, 0.26f},
            {kLimActive, 1.0f}, {kLimMode, 1.0f}, {kLimDrive, 2.4f}, {kLimCeiling, -0.3f}
        });

        addPreset("PSY: Night Riser", {
            {kAutoIn, 1.0f}, {kInTarget, -8.0f},
            {kRouting, 0.0f}, {kScvActive, 1.0f}, {kDrive, 3.2f}, {kThreshold, -4.0f}, {kAutoGain, 1.0f},
            {kNoiseActive, 1.0f}, {kNoiseType, 1.0f}, {kNoiseRoute, 1.0f}, {kNoiseLevel, -24.0f}, {kNoiseFmMod, 0.24f},
            {kFAType, 2.0f}, {kFAMode, 1.0f}, {kFACutoff, 220.0f}, {kFAReso, 0.55f}, {kFADrive, 1.6f},
            {kFBType, 2.0f}, {kFBMode, 0.0f}, {kFBCutoff, 3200.0f}, {kFBReso, 0.72f}, {kFBDrive, 1.9f}, {kFBGrit, 0.22f},
            {kLfo1Freq, 0.25f}, {kLfo1Wave, 2.0f}, {kModL1T1, 0.60f}, {kModL1T4, -0.45f},
            {kDelayActive, 1.0f}, {kDelayTime, 18.0f}, {kDelayFeedback, 0.72f}, {kDelayMix, 0.30f},
            {kLimActive, 1.0f}, {kLimMode, 1.0f}, {kLimDrive, 2.8f}, {kLimCeiling, -0.3f}
        });

        // SAT: S-Curve & Exhaustion (13-22)
        addPreset("SAT: Init", {
            {kScvActive, 1.0f}, {kDrive, 1.0f}, {kAutoGain, 1.0f},
            {kLimActive, 1.0f}, {kLimMode, 0.0f}, {kLimDrive, 0.0f}, {kLimCeiling, -0.3f}
        });
        addPreset("SAT: Hard Clip", {
            {kScvActive, 1.0f}, {kDrive, 7.0f}, {kExhaustion, 1.0f}, {kThreshold, -8.5f},
            {kAutoGain, 1.0f}, {kMix, 0.90f}, {kLimActive, 1.0f}, {kLimCeiling, -0.5f}
        });
        addPreset("SAT: Warm Bus", {
            {kScvActive, 1.0f}, {kDrive, 1.9f}, {kThreshold, -3.0f}, {kAutoGain, 1.0f},
            {kMix, 0.65f}, {kLimActive, 1.0f}, {kLimMode, 0.0f}, {kLimDrive, 2.0f}, {kLimCeiling, -0.3f}
        });
        addPreset("SAT: Velvet Tape", {
            {kScvActive, 1.0f}, {kDrive, 3.0f}, {kThreshold, -4.0f}, {kAutoGain, 1.0f},
            {kMix, 0.80f}, {kFACutoff, 11000.0f}, {kFAReso, 0.15f},
            {kLimActive, 1.0f}, {kLimMode, 0.0f}, {kLimDrive, 0.0f}, {kLimCeiling, -0.3f}
        });
        addPreset("SAT: Tube Glow", {
            {kScvActive, 1.0f}, {kDrive, 3.2f}, {kThreshold, -2.5f}, {kAutoGain, 1.0f},
            {kMix, 0.75f}, {kFADrive, 1.9f}, {kFACutoff, 14000.0f},
            {kLimActive, 1.0f}, {kLimMode, 0.0f}, {kLimDrive, 0.0f}, {kLimCeiling, -0.3f}
        });
        addPreset("SAT: Sub Crunch", {
            {kScvActive, 1.0f}, {kDrive, 5.5f}, {kExhaustion, 1.0f}, {kThreshold, -6.5f},
            {kAutoGain, 1.0f}, {kFACutoff, 280.0f}, {kFAReso, 0.45f}, {kMix, 0.85f},
            {kLimActive, 1.0f}, {kLimMode, 0.0f}, {kLimDrive, 0.0f}, {kLimCeiling, -0.3f}
        });
        addPreset("SAT: Fuzz Wall", {
            {kScvActive, 1.0f}, {kDrive, 11.5f}, {kExhaustion, 1.0f}, {kThreshold, -12.0f},
            {kAutoGain, 1.0f}, {kFACutoff, 4800.0f}, {kFAReso, 0.25f},
            {kLimActive, 1.0f}, {kLimMode, 0.0f}, {kLimDrive, 0.0f}, {kLimCeiling, -0.8f}
        });
        addPreset("SAT: Edge Bite", {
            {kScvActive, 1.0f}, {kDrive, 5.2f}, {kThreshold, -7.5f}, {kAutoGain, 1.0f},
            {kFBType, 0.0f}, {kFBMode, 1.0f}, {kFBCutoff, 180.0f}, {kFADrive, 1.8f}, {kFAGrit, 0.28f},
            {kLimActive, 1.0f}, {kLimMode, 0.0f}, {kLimDrive, 0.0f}, {kLimCeiling, -0.3f}
        });
        addPreset("SAT: Master Glue", {
            {kScvActive, 1.0f}, {kDrive, 1.5f}, {kThreshold, -2.0f}, {kAutoGain, 1.0f}, {kMix, 0.50f},
            {kLimActive, 1.0f}, {kLimMode, 0.0f}, {kLimDrive, 1.8f}, {kLimCeiling, -0.2f}, {kLimRelease, 120.0f}
        });
        addPreset("SAT: Burned Coil", {
            {kScvActive, 1.0f}, {kDrive, 9.5f}, {kExhaustion, 1.0f}, {kThreshold, -10.5f},
            {kAutoGain, 1.0f}, {kNoiseActive, 1.0f}, {kNoiseType, 0.0f}, {kNoiseLevel, -38.0f},
            {kLimActive, 1.0f}, {kLimMode, 0.0f}, {kLimDrive, 0.0f}, {kLimCeiling, -0.5f}
        });

        // FLT: Dual Filter & Routing (23-32)
        addPreset("FLT: Init", {
            {kRouting, 0.0f}, {kFAType, 0.0f}, {kFAMode, 0.0f}, {kFACutoff, 4500.0f}, {kFAReso, 0.25f},
            {kFBType, 0.0f}, {kFBMode, 1.0f}, {kFBCutoff, 35.0f}, {kFBReso, 0.20f},
            {kLfo1Freq, 0.50f}, {kLfo2Freq, 0.33f},
            {kLimActive, 1.0f}, {kLimMode, 0.0f}, {kLimDrive, 0.0f}, {kLimCeiling, -0.3f}
        });
        addPreset("FLT: Acid Sweep", {
            {kScvActive, 1.0f}, {kDrive, 1.8f}, {kAutoGain, 1.0f},
            {kFAType, 2.0f}, {kFAMode, 0.0f}, {kFACutoff, 650.0f}, {kFAReso, 0.78f}, {kFADrive, 2.0f},
            {kLfo1Freq, 0.35f}, {kLfo1Wave, 0.0f}, {kModL1T1, 0.55f},
            {kLimActive, 1.0f}, {kLimMode, 0.0f}, {kLimDrive, 0.0f}, {kLimCeiling, -0.3f}
        });
        addPreset("FLT: Parallel Band", {
            {kRouting, 1.0f}, {kFAType, 0.0f}, {kFAMode, 2.0f}, {kFACutoff, 550.0f}, {kFAReso, 0.65f},
            {kFBType, 0.0f}, {kFBMode, 2.0f}, {kFBCutoff, 2800.0f}, {kFBReso, 0.65f},
            {kLfo1Freq, 0.25f}, {kModL1T1, 0.45f}, {kLfo2Freq, 0.40f}, {kModL2T4, -0.45f},
            {kLimActive, 1.0f}, {kLimMode, 0.0f}, {kLimDrive, 0.0f}, {kLimCeiling, -0.3f}
        });
        addPreset("FLT: Notch Phaser", {
            {kRouting, 0.0f}, {kFAType, 0.0f}, {kFAMode, 3.0f}, {kFACutoff, 1100.0f}, {kFAReso, 0.75f},
            {kFBType, 0.0f}, {kFBMode, 3.0f}, {kFBCutoff, 2600.0f}, {kFBReso, 0.70f},
            {kLfo1Freq, 0.30f}, {kModL1T1, 0.60f}, {kLfo2Freq, 0.45f}, {kModL2T4, -0.60f},
            {kLimActive, 1.0f}, {kLimMode, 0.0f}, {kLimDrive, 0.0f}, {kLimCeiling, -0.3f}
        });
        addPreset("FLT: 303 Squelch", {
            {kScvActive, 1.0f}, {kDrive, 2.0f}, {kAutoGain, 1.0f},
            {kFAType, 2.0f}, {kFAMode, 0.0f}, {kFACutoff, 420.0f}, {kFAReso, 0.86f}, {kFADrive, 2.4f}, {kFAGrit, 0.22f},
            {kLfo1Freq, 0.85f}, {kLfo1Wave, 2.0f}, {kModL1T1, 0.70f},
            {kLimActive, 1.0f}, {kLimMode, 0.0f}, {kLimDrive, 0.0f}, {kLimCeiling, -0.3f}
        });
        addPreset("FLT: Dual Peak", {
            {kRouting, 1.0f}, {kFAType, 2.0f}, {kFAMode, 2.0f}, {kFACutoff, 480.0f}, {kFAReso, 0.78f},
            {kFBType, 2.0f}, {kFBMode, 2.0f}, {kFBCutoff, 3200.0f}, {kFBReso, 0.75f},
            {kLimActive, 1.0f}, {kLimMode, 0.0f}, {kLimDrive, 0.0f}, {kLimCeiling, -0.3f}
        });
        addPreset("FLT: Sub Focus", {
            {kRouting, 0.0f}, {kFAType, 0.0f}, {kFAMode, 0.0f}, {kFACutoff, 220.0f}, {kFAReso, 0.35f},
            {kFBType, 0.0f}, {kFBMode, 1.0f}, {kFBCutoff, 35.0f}, {kFBReso, 0.25f}, {kFBDrive, 1.7f},
            {kLimActive, 1.0f}, {kLimMode, 0.0f}, {kLimDrive, 0.0f}, {kLimCeiling, -0.3f}
        });
        addPreset("FLT: Telephone", {
            {kRouting, 0.0f}, {kFAType, 3.0f}, {kFAMode, 1.0f}, {kFACutoff, 450.0f}, {kFAReso, 0.30f},
            {kFBType, 3.0f}, {kFBMode, 0.0f}, {kFBCutoff, 3100.0f}, {kFBReso, 0.40f}, {kFADrive, 1.8f},
            {kLimActive, 1.0f}, {kLimMode, 0.0f}, {kLimDrive, 0.0f}, {kLimCeiling, -0.3f}
        });
        addPreset("FLT: Stereo Split", {
            {kRouting, 2.0f}, {kFilterMix, 0.90f}, {kFAType, 0.0f}, {kFAMode, 0.0f}, {kFACutoff, 750.0f}, {kFAReso, 0.60f},
            {kFBType, 2.0f}, {kFBMode, 1.0f}, {kFBCutoff, 1400.0f}, {kFBReso, 0.65f},
            {kLfo1Freq, 0.20f}, {kModL1T1, 0.50f}, {kModL1T4, -0.50f},
            {kLimActive, 1.0f}, {kLimMode, 0.0f}, {kLimDrive, 0.0f}, {kLimCeiling, -0.3f}
        });
        addPreset("FLT: Notch Canyon", {
            {kFAType, 2.0f}, {kFAMode, 3.0f}, {kFACutoff, 800.0f}, {kFAReso, 0.84f},
            {kFBType, 0.0f}, {kFBMode, 3.0f}, {kFBCutoff, 2100.0f}, {kFBReso, 0.80f},
            {kLfo1Freq, 0.16f}, {kModL1T1, 0.70f}, {kLfo2Freq, 0.23f}, {kModL2T4, -0.65f},
            {kLimActive, 1.0f}, {kLimMode, 0.0f}, {kLimDrive, 0.0f}, {kLimCeiling, -0.3f}
        });

        // VOWL: Formant Vowel Filter (33-41)
        addPreset("VOWL: Init", {
            {kFAType, 1.0f}, {kFAMode, 2.0f}, {kFACutoff, 900.0f}, {kFAReso, 0.55f},
            {kLfo1Freq, 0.50f}, {kModL1T1, 0.35f},
            {kLimActive, 1.0f}, {kLimMode, 0.0f}, {kLimDrive, 0.0f}, {kLimCeiling, -0.3f}
        });
        addPreset("VOWL: Talkbox", {
            {kFAType, 1.0f}, {kFAMode, 2.0f}, {kFACutoff, 900.0f}, {kFAReso, 0.68f},
            {kFADrive, 1.7f}, {kFAGrit, 0.22f}, {kLfo1Freq, 0.50f}, {kModL1T1, 0.55f},
            {kLimActive, 1.0f}, {kLimMode, 0.0f}, {kLimDrive, 0.0f}, {kLimCeiling, -0.3f}
        });
        addPreset("VOWL: Choir Dual", {
            {kRouting, 1.0f}, {kFAType, 1.0f}, {kFAMode, 0.0f}, {kFACutoff, 600.0f}, {kFAReso, 0.55f},
            {kFBType, 1.0f}, {kFBMode, 1.0f}, {kFBCutoff, 2200.0f}, {kFBReso, 0.55f},
            {kLfo1Freq, 0.22f}, {kModL1T1, 0.50f}, {kLfo2Freq, 0.31f}, {kModL2T4, -0.50f},
            {kDelayActive, 1.0f}, {kDelaySync, 1.0f}, {kDelayTempo, 1.0f}, {kDelayPingPong, 1.0f},
            {kDelayFeedback, 0.35f}, {kDelayMix, 0.30f},
            {kLimActive, 1.0f}, {kLimMode, 0.0f}, {kLimDrive, 0.0f}, {kLimCeiling, -0.3f}
        });
        addPreset("VOWL: Talking Gate", {
            {kFAType, 1.0f}, {kFAMode, 2.0f}, {kFACutoff, 1200.0f}, {kFAReso, 0.72f},
            {kLfo1Freq, 0.40f}, {kModL1T1, 0.60f},
            {kTgActive, 1.0f}, {kTgPattern, 11.0f}, {kTgDepth, 85.0f}, {kTgMix, 0.85f},
            {kLimActive, 1.0f}, {kLimMode, 0.0f}, {kLimDrive, 0.0f}, {kLimCeiling, -0.3f}
        });
        addPreset("VOWL: Oh-Ah Morph", {
            {kFAType, 1.0f}, {kFAMode, 0.0f}, {kFACutoff, 380.0f}, {kFAReso, 0.75f}, {kFADrive, 1.55f},
            {kLfo1Freq, 0.60f}, {kLfo1Wave, 1.0f}, {kModL1T1, 0.65f},
            {kLimActive, 1.0f}, {kLimMode, 0.0f}, {kLimDrive, 0.0f}, {kLimCeiling, -0.3f}
        });
        addPreset("VOWL: Robot Throat", {
            {kFAType, 1.0f}, {kFAMode, 3.0f}, {kFACutoff, 1100.0f}, {kFAReso, 0.82f}, {kFAGrit, 0.25f},
            {kLfo1Freq, 1.80f}, {kLfo1Wave, 3.0f}, {kModL1T1, 0.45f},
            {kLimActive, 1.0f}, {kLimMode, 0.0f}, {kLimDrive, 0.0f}, {kLimCeiling, -0.3f}
        });
        addPreset("VOWL: Acid Choir", {
            {kRouting, 0.0f}, {kFAType, 1.0f}, {kFAMode, 2.0f}, {kFACutoff, 820.0f}, {kFAReso, 0.76f},
            {kFBType, 2.0f}, {kFBMode, 0.0f}, {kFBCutoff, 3200.0f}, {kFBReso, 0.65f},
            {kLfo1Freq, 0.45f}, {kModL1T1, 0.50f}, {kLfo2Freq, 0.30f}, {kModL2T4, -0.40f},
            {kLimActive, 1.0f}, {kLimMode, 0.0f}, {kLimDrive, 0.0f}, {kLimCeiling, -0.3f}
        });
        addPreset("VOWL: Cyber Mouth", {
            {kScvActive, 1.0f}, {kDrive, 2.5f}, {kAutoGain, 1.0f},
            {kFAType, 1.0f}, {kFAMode, 1.0f}, {kFACutoff, 1500.0f}, {kFAReso, 0.78f}, {kFAGrit, 0.22f},
            {kQeActive, 1.0f}, {kQeDepth, 9.0f}, {kQeDownsample, 2.0f}, {kQeMix, 0.40f},
            {kLfo1Freq, 0.90f}, {kModL1T1, 0.55f},
            {kLimActive, 1.0f}, {kLimMode, 0.0f}, {kLimDrive, 0.0f}, {kLimCeiling, -0.3f}
        });
        addPreset("VOWL: Nasal Scream", {
            {kScvActive, 1.0f}, {kDrive, 3.8f}, {kExhaustion, 1.0f}, {kAutoGain, 1.0f}, {kFiltPreSat, 1.0f},
            {kFAType, 1.0f}, {kFAMode, 1.0f}, {kFACutoff, 1900.0f}, {kFAReso, 0.85f}, {kFADrive, 2.0f},
            {kLfo1Freq, 1.40f}, {kLfo1Wave, 2.0f}, {kModL1T1, 0.40f},
            {kLimActive, 1.0f}, {kLimMode, 0.0f}, {kLimDrive, 0.0f}, {kLimCeiling, -0.3f}
        });

        // DECY: Info Decay & Bit-Rot (42-50)
        addPreset("DECY: Init", {
            {kScvActive, 1.0f}, {kDrive, 1.0f}, {kAutoGain, 1.0f},
            {kQeActive, 1.0f}, {kQeDepth, 16.0f}, {kQeDownsample, 1.0f}, {kQeMix, 1.0f},
            {kLimActive, 1.0f}, {kLimMode, 0.0f}, {kLimDrive, 0.0f}, {kLimCeiling, -0.3f}
        });
        addPreset("DECY: 8-Bit Arcade", {
            {kScvActive, 1.0f}, {kDrive, 1.7f}, {kAutoGain, 1.0f},
            {kQeActive, 1.0f}, {kQeDepth, 8.0f}, {kQeDownsample, 3.0f}, {kQeMix, 0.85f},
            {kLimActive, 1.0f}, {kLimMode, 0.0f}, {kLimDrive, 0.0f}, {kLimCeiling, -0.3f}
        });
        addPreset("DECY: Lo-Fi Dust", {
            {kQeActive, 1.0f}, {kQeDepth, 6.5f}, {kQeDownsample, 6.0f}, {kQeMix, 0.60f},
            {kNoiseActive, 1.0f}, {kNoiseType, 1.0f}, {kNoiseRoute, 1.0f}, {kNoiseLevel, -32.0f},
            {kFACutoff, 3800.0f}, {kFAReso, 0.20f},
            {kLimActive, 1.0f}, {kLimMode, 0.0f}, {kLimDrive, 0.0f}, {kLimCeiling, -0.3f}
        });
        addPreset("DECY: Exh Shred", {
            {kScvActive, 1.0f}, {kDrive, 4.2f}, {kExhaustion, 1.0f}, {kThreshold, -5.5f},
            {kAutoGain, 1.0f}, {kQeActive, 1.0f}, {kQeLink, 1.0f}, {kQeDownsample, 8.0f},
            {kQeMix, 0.85f}, {kFACutoff, 5200.0f},
            {kLimActive, 1.0f}, {kLimMode, 0.0f}, {kLimDrive, 0.0f}, {kLimCeiling, -0.3f}
        });
        addPreset("DECY: Gameboy Lead", {
            {kQeActive, 1.0f}, {kQeDepth, 4.0f}, {kQeDownsample, 5.0f}, {kQeMix, 0.90f},
            {kFBType, 0.0f}, {kFBMode, 1.0f}, {kFBCutoff, 250.0f}, {kFACutoff, 6500.0f},
            {kLimActive, 1.0f}, {kLimMode, 0.0f}, {kLimDrive, 0.0f}, {kLimCeiling, -0.3f}
        });
        addPreset("DECY: CRT Static", {
            {kQeActive, 1.0f}, {kQeDepth, 7.5f}, {kQeDownsample, 4.0f}, {kQeMix, 0.70f},
            {kNoiseActive, 1.0f}, {kNoiseType, 0.0f}, {kNoiseRoute, 2.0f}, {kNoiseLevel, -34.0f},
            {kFACutoff, 4500.0f}, {kFAReso, 0.40f}
        });
        addPreset("DECY: Mod Bit-Rot", {
            {kQeActive, 1.0f}, {kQeDepth, 5.5f}, {kQeDownsample, 9.0f}, {kQeMix, 0.80f},
            {kFACutoff, 2600.0f}, {kFAReso, 0.55f}, {kLfo1Freq, 0.80f}, {kModL1T1, 0.45f},
            {kLimActive, 1.0f}, {kLimMode, 0.0f}, {kLimDrive, 0.0f}, {kLimCeiling, -0.3f}
        });
        addPreset("DECY: Broken Tape", {
            {kScvActive, 1.0f}, {kDrive, 2.4f}, {kAutoGain, 1.0f},
            {kQeActive, 1.0f}, {kQeDepth, 9.0f}, {kQeDownsample, 3.0f}, {kQeMix, 0.45f},
            {kNoiseActive, 1.0f}, {kNoiseType, 1.0f}, {kNoiseLevel, -36.0f},
            {kDelayActive, 1.0f}, {kDelayTime, 190.0f}, {kDelayFeedback, 0.42f}, {kDelayMix, 0.30f}
        });
        addPreset("DECY: 4-Bit Savage", {
            {kScvActive, 1.0f}, {kDrive, 4.2f}, {kAutoGain, 1.0f},
            {kQeActive, 1.0f}, {kQeDepth, 4.0f}, {kQeDownsample, 10.0f}, {kQeMix, 0.90f},
            {kLimActive, 1.0f}, {kLimMode, 0.0f}, {kLimDrive, 0.0f}, {kLimCeiling, -0.5f}
        });

        // LFO: Modulation, Noise & FM (51-59)
        addPreset("LFO: Init", {
            {kFACutoff, 2400.0f}, {kFAReso, 0.35f},
            {kLfo1Freq, 1.0f}, {kLfo1Wave, 0.0f}, {kModL1T1, 0.25f},
            {kLfo2Freq, 0.33f}, {kLfo2Wave, 1.0f}, {kModL2T4, -0.25f},
            {kLimActive, 1.0f}, {kLimMode, 0.0f}, {kLimDrive, 0.0f}, {kLimCeiling, -0.3f}
        });
        addPreset("LFO: Alien FM", {
            {kNoiseType, 4.0f}, {kNoiseFreq, 140.0f}, {kNoiseFmMod, 0.32f},
            {kFAType, 2.0f}, {kFACutoff, 1400.0f}, {kFAReso, 0.60f},
            {kLfo1Freq, 3.2f}, {kLfo1Wave, 4.0f}, {kModL1T1, 0.50f},
            {kLimActive, 1.0f}, {kLimMode, 0.0f}, {kLimDrive, 0.0f}, {kLimCeiling, -0.3f}
        });
        addPreset("LFO: Wobble Bass", {
            {kScvActive, 1.0f}, {kDrive, 2.8f}, {kAutoGain, 1.0f},
            {kFAType, 2.0f}, {kFAMode, 0.0f}, {kFACutoff, 420.0f}, {kFAReso, 0.72f}, {kFADrive, 1.7f},
            {kLfo1Freq, 1.5f}, {kLfo1Wave, 2.0f}, {kModL1T1, 0.75f},
            {kLimActive, 1.0f}, {kLimMode, 0.0f}, {kLimDrive, 0.0f}, {kLimCeiling, -0.3f}
        });
        addPreset("LFO: Shimmer S&H", {
            {kFAType, 0.0f}, {kFAMode, 2.0f}, {kFACutoff, 1600.0f}, {kFAReso, 0.70f},
            {kFBType, 0.0f}, {kFBMode, 2.0f}, {kFBCutoff, 3200.0f}, {kFBReso, 0.65f},
            {kLfo1Freq, 4.5f}, {kLfo1Wave, 4.0f}, {kModL1T1, 0.65f},
            {kLfo2Freq, 6.0f}, {kLfo2Wave, 4.0f}, {kModL2T4, -0.60f},
            {kDelayActive, 1.0f}, {kDelaySync, 1.0f}, {kDelayTempo, 1.0f}, {kDelayPingPong, 1.0f}, {kDelayMix, 0.35f},
            {kLimActive, 1.0f}, {kLimMode, 0.0f}, {kLimDrive, 0.0f}, {kLimCeiling, -0.3f}
        });
        addPreset("LFO: Crossfade Mod", {
            {kRouting, 3.0f}, {kFACutoff, 650.0f}, {kFAReso, 0.65f}, {kFBCutoff, 2400.0f}, {kFBReso, 0.65f},
            {kLfo1Freq, 0.45f}, {kModL1T1, 0.70f}, {kModL1T4, -0.70f},
            {kLimActive, 1.0f}, {kLimMode, 0.0f}, {kLimDrive, 0.0f}, {kLimCeiling, -0.3f}
        });
        addPreset("LFO: Hyper Flutter", {
            {kFAType, 2.0f}, {kFACutoff, 1800.0f}, {kFAReso, 0.55f},
            {kLfo1Freq, 9.5f}, {kLfo1Wave, 1.0f}, {kModL1T1, 0.45f}, {kModL1T2, 0.25f},
            {kLimActive, 1.0f}, {kLimMode, 0.0f}, {kLimDrive, 0.0f}, {kLimCeiling, -0.3f}
        });
        addPreset("LFO: Sci-Fi Siren", {
            {kNoiseType, 2.0f}, {kNoiseFreq, 220.0f}, {kNoiseFmMod, 0.38f},
            {kFAType, 2.0f}, {kFAMode, 2.0f}, {kFACutoff, 1100.0f}, {kFAReso, 0.78f},
            {kLfo1Freq, 0.65f}, {kLfo1Wave, 0.0f}, {kModL1T1, 0.70f},
            {kDelayActive, 1.0f}, {kDelayPingPong, 1.0f}, {kDelayFeedback, 0.50f}, {kDelayMix, 0.35f},
            {kLimActive, 1.0f}, {kLimMode, 0.0f}, {kLimDrive, 0.0f}, {kLimCeiling, -0.3f}
        });
        addPreset("LFO: Stepped Arps", {
            {kFAType, 2.0f}, {kFAMode, 0.0f}, {kFACutoff, 700.0f}, {kFAReso, 0.80f},
            {kLfo1Freq, 6.0f}, {kLfo1Wave, 4.0f}, {kModL1T1, 0.65f},
            {kDelayActive, 1.0f}, {kDelaySync, 1.0f}, {kDelayTempo, 0.0f}, {kDelayFeedback, 0.40f}, {kDelayMix, 0.30f},
            {kLimActive, 1.0f}, {kLimMode, 0.0f}, {kLimDrive, 0.0f}, {kLimCeiling, -0.3f}
        });
        addPreset("LFO: Dual Drift", {
            {kFACutoff, 1200.0f}, {kFAReso, 0.50f}, {kFBCutoff, 2800.0f}, {kFBReso, 0.50f},
            {kLfo1Freq, 0.14f}, {kModL1T1, 0.55f}, {kLfo2Freq, 0.21f}, {kModL2T4, 0.55f}
        });

        // GATE: 128-Pattern Gate & Offset (60-69)
        addPreset("GATE: Init", {
            {kTgActive, 1.0f}, {kTgPattern, 0.0f}, {kTgRate, 0.0f},
            {kTgDepth, 80.0f}, {kTgHold, 55.0f}, {kTgMix, 1.0f}
        });
        addPreset("GATE: Kick Safe", {
            {kTgActive, 1.0f}, {kTgPattern, 0.0f}, {kTgOffset, 35.0f}, {kTgDepth, 85.0f},
            {kTgStart, 8.0f}, {kTgHold, 55.0f}, {kTgEnd, 20.0f}
        });
        addPreset("GATE: Euclid 7", {
            {kTgActive, 1.0f}, {kTgPattern, 77.0f}, {kTgRate, 0.0f}, {kTgDepth, 90.0f},
            {kTgHold, 60.0f}, {kDelayActive, 1.0f}, {kDelaySync, 1.0f}, {kDelayTempo, 1.0f},
            {kDelayPingPong, 1.0f}, {kDelayMix, 0.30f}
        });
        addPreset("GATE: DnB Chop", {
            {kScvActive, 1.0f}, {kDrive, 2.2f}, {kAutoGain, 1.0f},
            {kFAType, 2.0f}, {kFAMode, 2.0f}, {kFACutoff, 1800.0f}, {kFAReso, 0.45f},
            {kTgActive, 1.0f}, {kTgPattern, 44.0f}, {kTgDepth, 95.0f}, {kTgHold, 45.0f},
            {kLimActive, 1.0f}, {kLimMode, 0.0f}, {kLimDrive, 0.0f}, {kLimCeiling, -0.3f}
        });
        addPreset("GATE: Trance Arp", {
            {kTgActive, 1.0f}, {kTgPattern, 11.0f}, {kTgRate, 0.0f}, {kTgDepth, 90.0f},
            {kTgHold, 60.0f}, {kDelayActive, 1.0f}, {kDelaySync, 1.0f}, {kDelayTempo, 0.0f},
            {kDelayPingPong, 1.0f}, {kDelayFeedback, 0.45f}, {kDelayMix, 0.35f}
        });
        addPreset("GATE: Triplet Hop", {
            {kTgActive, 1.0f}, {kTgPattern, 64.0f}, {kTgRate, 0.0f}, {kTgDepth, 85.0f},
            {kTgStart, 4.0f}, {kTgHold, 65.0f}, {kTgEnd, 25.0f}
        });
        addPreset("GATE: Galop Ride", {
            {kTgActive, 1.0f}, {kTgPattern, 5.0f}, {kTgRate, 0.0f}, {kTgDepth, 80.0f},
            {kTgHold, 55.0f}, {kFAType, 0.0f}, {kFACutoff, 4500.0f}, {kFAReso, 0.35f}
        });
        addPreset("GATE: Trap HiHat", {
            {kTgActive, 1.0f}, {kTgPattern, 58.0f}, {kTgRate, 0.0f}, {kTgDepth, 95.0f},
            {kTgStart, 2.0f}, {kTgHold, 40.0f}, {kTgEnd, 12.0f}, {kFBMode, 1.0f}, {kFBCutoff, 300.0f}
        });
        addPreset("GATE: Son Clave", {
            {kTgActive, 1.0f}, {kTgPattern, 88.0f}, {kTgRate, 0.0f}, {kTgDepth, 88.0f},
            {kTgHold, 65.0f}, {kDelayActive, 1.0f}, {kDelaySync, 1.0f}, {kDelayTempo, 1.0f},
            {kDelayPingPong, 1.0f}, {kDelayFeedback, 0.30f}, {kDelayMix, 0.25f}
        });
        addPreset("GATE: Glitch Stutter", {
            {kTgActive, 1.0f}, {kTgPattern, 110.0f}, {kTgRate, 0.0f}, {kTgDepth, 100.0f},
            {kTgStart, 1.0f}, {kTgHold, 45.0f}, {kTgEnd, 5.0f},
            {kQeActive, 1.0f}, {kQeDepth, 7.0f}, {kQeDownsample, 4.0f}, {kQeMix, 0.55f}
        });

        // PUMP: Deep Sidechain Ducking (no Limiter Drive undoing the pump!) (70-77)
        addPreset("PUMP: Init", {
            {kScActive, 1.0f}, {kScSource, 1.0f}, {kScMode, 0.0f}, {kScRatio, 10.0f},
            {kScThresh, -18.0f}, {kScAmount, -78.0f}, {kScAttack, 2.0f}, {kScRelease, 190.0f},
            {kScTargetGain, 1.0f}
        });
        addPreset("PUMP: House Duck", {
            {kScActive, 1.0f}, {kScSource, 1.0f}, {kScMode, 0.0f}, {kScRatio, 10.0f}, {kScThresh, -18.0f},
            {kScAmount, -82.0f}, {kScAttack, 1.5f}, {kScRelease, 210.0f},
            {kScTargetGain, 1.0f}, {kScTargetCut, 1.0f}, {kFACutoff, 10000.0f}, {kFAReso, 0.30f}
        });
        addPreset("PUMP: Test Pulse 4x4", {
            {kScActive, 1.0f}, {kScSource, 1.0f}, {kScMode, 0.0f}, {kScRatio, 12.0f},
            {kScThresh, -18.0f}, {kScAmount, -85.0f}, {kScAttack, 1.0f}, {kScRelease, 230.0f},
            {kScTargetGain, 1.0f}, {kScTargetCut, 1.0f}, {kFACutoff, 8500.0f}, {kFAReso, 0.40f}
        });
        addPreset("PUMP: Kick Keyed LPF", {
            {kScActive, 1.0f}, {kScSource, 0.0f}, {kScMode, 0.0f}, {kScRatio, 10.0f}, {kScThresh, -15.0f},
            {kScAmount, -75.0f}, {kScAttack, 1.0f}, {kScRelease, 175.0f},
            {kScHpf, 30.0f}, {kScLpf, 160.0f}, {kScTargetGain, 1.0f}
        });
        addPreset("PUMP: Vacuum Drop", {
            {kScActive, 1.0f}, {kScSource, 1.0f}, {kScMode, 0.0f}, {kScRatio, 16.0f}, {kScThresh, -20.0f},
            {kScAmount, -92.0f}, {kScAttack, 0.8f}, {kScRelease, 300.0f},
            {kScTargetGain, 1.0f}, {kScTargetCut, 1.0f}, {kFACutoff, 12000.0f}, {kFAReso, 0.45f}
        });
        addPreset("PUMP: Filter Breathe", {
            {kFAType, 2.0f}, {kFAMode, 0.0f}, {kFACutoff, 7500.0f}, {kFAReso, 0.55f},
            {kScActive, 1.0f}, {kScSource, 1.0f}, {kScMode, 0.0f}, {kScRatio, 10.0f}, {kScThresh, -18.0f},
            {kScAmount, -80.0f}, {kScAttack, 3.0f}, {kScRelease, 250.0f},
            {kScTargetGain, 0.0f}, {kScTargetCut, 1.0f}, {kScTargetReso, 1.0f},
            {kLimActive, 1.0f}, {kLimMode, 0.0f}, {kLimDrive, 0.0f}, {kLimCeiling, -0.3f}
        });
        addPreset("PUMP: Drive Surge", {
            {kScvActive, 1.0f}, {kDrive, 2.0f}, {kAutoGain, 1.0f},
            {kScActive, 1.0f}, {kScSource, 0.0f}, {kScMode, 0.0f}, {kScRatio, 10.0f}, {kScThresh, -15.0f},
            {kScAmount, 75.0f}, {kScAttack, 2.0f}, {kScRelease, 180.0f},
            {kScTargetGain, 0.0f}, {kScTargetDrive, 1.0f}, {kScTargetReso, 1.0f}, {kFACutoff, 3200.0f},
            {kLimActive, 1.0f}, {kLimMode, 0.0f}, {kLimDrive, 0.0f}, {kLimCeiling, -0.3f}
        });
        addPreset("PUMP: French Touch", {
            {kScvActive, 1.0f}, {kDrive, 2.4f}, {kThreshold, -4.0f}, {kAutoGain, 1.0f},
            {kFACutoff, 6500.0f}, {kFAReso, 0.45f},
            {kScActive, 1.0f}, {kScSource, 1.0f}, {kScMode, 0.0f}, {kScRatio, 10.0f}, {kScThresh, -18.0f},
            {kScAmount, -80.0f}, {kScAttack, 1.5f}, {kScRelease, 210.0f},
            {kScTargetGain, 1.0f}, {kScTargetCut, 1.0f}
        });

        // COMBO & MUSH: Multi-Engine Chains with Macro Headroom (78-95)
        addPreset("COMBO: Init", {
            {kScvActive, 1.0f}, {kDrive, 1.5f}, {kAutoGain, 1.0f},
            {kRouting, 0.0f}, {kFAType, 0.0f}, {kFAMode, 0.0f}, {kFACutoff, 12000.0f}, {kFAReso, 0.15f},
            {kFBType, 0.0f}, {kFBMode, 1.0f}, {kFBCutoff, 32.0f}, {kFBReso, 0.15f},
            {kDelayActive, 1.0f}, {kDelaySync, 1.0f}, {kDelayTempo, 0.0f}, {kDelayPingPong, 1.0f},
            {kDelayFeedback, 0.25f}, {kDelayMix, 0.15f},
            {kLimActive, 1.0f}, {kLimMode, 0.0f}, {kLimDrive, 0.0f}, {kLimCeiling, -0.3f}
        });
        addPreset("MUSH: Full Chaos", {
            {kScvActive, 1.0f}, {kDrive, 4.8f}, {kExhaustion, 0.0f}, {kThreshold, -6.5f}, {kAutoGain, 1.0f},
            {kFAType, 1.0f}, {kFAMode, 2.0f}, {kFACutoff, 1200.0f}, {kFAReso, 0.52f}, {kFADrive, 1.5f},
            {kFBType, 2.0f}, {kFBMode, 1.0f}, {kFBCutoff, 280.0f}, {kFBReso, 0.38f},
            {kLfo1Freq, 0.65f}, {kModL1T1, 0.40f}, {kLfo2Freq, 1.10f}, {kModL2T4, 0.35f},
            {kQeActive, 1.0f}, {kQeDepth, 7.0f}, {kQeDownsample, 4.0f}, {kQeMix, 0.35f},
            {kTgActive, 1.0f}, {kTgPattern, 80.0f}, {kTgDepth, 72.0f},
            {kDelayActive, 1.0f}, {kDelaySync, 1.0f}, {kDelayTempo, 0.0f}, {kDelayPingPong, 1.0f},
            {kDelayFeedback, 0.32f}, {kDelayMix, 0.22f},
            {kLimActive, 1.0f}, {kLimMode, 0.0f}, {kLimDrive, 4.5f}, {kLimCeiling, -0.5f}
        });
        addPreset("MUSH: Everything On", {
            {kScvActive, 1.0f}, {kDrive, 3.6f}, {kExhaustion, 0.0f}, {kThreshold, -5.0f}, {kAutoGain, 1.0f},
            {kRouting, 1.0f}, {kFAType, 1.0f}, {kFAMode, 2.0f}, {kFACutoff, 1050.0f}, {kFAReso, 0.50f}, {kFADrive, 1.4f}, {kFAGrit, 0.18f},
            {kFBType, 2.0f}, {kFBMode, 0.0f}, {kFBCutoff, 2600.0f}, {kFBReso, 0.45f},
            {kLfo1Freq, 0.50f}, {kModL1T1, 0.38f}, {kLfo2Freq, 0.80f}, {kModL2T4, -0.35f},
            {kNoiseActive, 1.0f}, {kNoiseType, 1.0f}, {kNoiseRoute, 1.0f}, {kNoiseLevel, -38.0f}, {kNoiseFmMod, 0.18f},
            {kQeActive, 1.0f}, {kQeDepth, 8.5f}, {kQeDownsample, 2.0f}, {kQeMix, 0.28f},
            {kTgActive, 1.0f}, {kTgPattern, 77.0f}, {kTgOffset, 15.0f}, {kTgDepth, 68.0f},
            {kScActive, 1.0f}, {kScSource, 1.0f}, {kScMode, 0.0f}, {kScRatio, 8.0f}, {kScThresh, -18.0f}, {kScAmount, -58.0f},
            {kScAttack, 2.0f}, {kScRelease, 160.0f},
            {kScTargetGain, 1.0f}, {kScTargetCut, 1.0f},
            {kDelayActive, 1.0f}, {kDelaySync, 1.0f}, {kDelayTempo, 0.0f}, {kDelayPingPong, 1.0f},
            {kDelayFeedback, 0.35f}, {kDelayMix, 0.22f},
            {kLimActive, 1.0f}, {kLimMode, 0.0f}, {kLimDrive, 2.8f}, {kLimCeiling, -0.5f}
        });
        addPreset("COMBO: Sub Slam", {
            {kScvActive, 1.0f}, {kDrive, 3.8f}, {kThreshold, -5.0f}, {kAutoGain, 1.0f},
            {kFAType, 0.0f}, {kFAMode, 0.0f}, {kFACutoff, 210.0f}, {kFAReso, 0.60f}, {kFADrive, 2.0f},
            {kFBType, 2.0f}, {kFBMode, 1.0f}, {kFBCutoff, 55.0f}, {kFBReso, 0.40f}, {kFBDrive, 1.7f},
            {kLimActive, 1.0f}, {kLimMode, 1.0f}, {kLimDrive, 5.5f}, {kLimCeiling, -0.4f}
        });
        addPreset("COMBO: Acid Rave", {
            {kScvActive, 1.0f}, {kDrive, 4.0f}, {kThreshold, -4.5f}, {kAutoGain, 1.0f}, {kFiltPreSat, 1.0f},
            {kFAType, 2.0f}, {kFAMode, 0.0f}, {kFACutoff, 550.0f}, {kFAReso, 0.80f}, {kFADrive, 2.2f},
            {kLfo1Freq, 0.50f}, {kLfo1Wave, 2.0f}, {kModL1T1, 0.65f}, {kEnvCut, 0.28f}, {kEnvARes, 0.25f},
            {kTgActive, 1.0f}, {kTgPattern, 4.0f}, {kTgDepth, 75.0f}, {kTgHold, 60.0f},
            {kDelayActive, 1.0f}, {kDelaySync, 1.0f}, {kDelayTempo, 0.0f}, {kDelayPingPong, 1.0f},
            {kDelayFeedback, 0.35f}, {kDelayMix, 0.25f},
            {kLimActive, 1.0f}, {kLimMode, 1.0f}, {kLimDrive, 4.8f}, {kLimCeiling, -0.5f}
        });
        addPreset("COMBO: Vowel Bass", {
            {kScvActive, 1.0f}, {kDrive, 3.8f}, {kThreshold, -6.0f}, {kAutoGain, 1.0f},
            {kFAType, 1.0f}, {kFAMode, 2.0f}, {kFACutoff, 750.0f}, {kFAReso, 0.72f}, {kFADrive, 2.0f}, {kFAGrit, 0.30f},
            {kFBType, 2.0f}, {kFBMode, 1.0f}, {kFBCutoff, 55.0f}, {kFBReso, 0.30f},
            {kLfo1Freq, 1.20f}, {kLfo1Wave, 0.0f}, {kModL1T1, 0.55f},
            {kLimActive, 1.0f}, {kLimMode, 1.0f}, {kLimDrive, 5.0f}, {kLimCeiling, -0.5f}
        });
        addPreset("COMBO: Industrial", {
            {kScvActive, 1.0f}, {kDrive, 7.0f}, {kExhaustion, 1.0f}, {kThreshold, -9.5f}, {kAutoGain, 1.0f},
            {kFAType, 2.0f}, {kFAMode, 2.0f}, {kFACutoff, 1400.0f}, {kFAReso, 0.65f}, {kFAGrit, 0.38f},
            {kNoiseActive, 1.0f}, {kNoiseType, 4.0f}, {kNoiseFreq, 110.0f}, {kNoiseLevel, -32.0f}, {kNoiseFmMod, 0.28f},
            {kQeActive, 1.0f}, {kQeDepth, 6.0f}, {kQeDownsample, 4.0f}, {kQeMix, 0.65f},
            {kLimActive, 1.0f}, {kLimMode, 1.0f}, {kLimDrive, 6.5f}, {kLimCeiling, -0.5f}
        });
        addPreset("COMBO: Pump & Shred", {
            {kScvActive, 1.0f}, {kDrive, 3.8f}, {kExhaustion, 1.0f}, {kThreshold, -5.0f}, {kAutoGain, 1.0f},
            {kQeActive, 1.0f}, {kQeLink, 1.0f}, {kQeDownsample, 5.0f}, {kQeMix, 0.50f},
            {kFACutoff, 6800.0f}, {kFAReso, 0.40f},
            {kScActive, 1.0f}, {kScSource, 1.0f}, {kScMode, 0.0f}, {kScRatio, 10.0f}, {kScThresh, -18.0f}, {kScAmount, -78.0f},
            {kScAttack, 1.5f}, {kScRelease, 185.0f},
            {kScTargetGain, 1.0f}, {kScTargetCut, 1.0f}, {kScTargetReso, 1.0f}
        });
        addPreset("COMBO: Dub Techno", {
            {kScvActive, 1.0f}, {kDrive, 2.2f}, {kThreshold, -3.5f}, {kAutoGain, 1.0f},
            {kRouting, 1.0f}, {kFAType, 2.0f}, {kFAMode, 0.0f}, {kFACutoff, 680.0f}, {kFAReso, 0.60f},
            {kFBType, 0.0f}, {kFBMode, 2.0f}, {kFBCutoff, 1900.0f}, {kFBReso, 0.50f},
            {kLfo1Freq, 0.18f}, {kModL1T1, 0.40f}, {kLfo2Freq, 0.27f}, {kModL2T4, -0.35f},
            {kNoiseActive, 1.0f}, {kNoiseType, 1.0f}, {kNoiseRoute, 1.0f}, {kNoiseLevel, -36.0f},
            {kDelayActive, 1.0f}, {kDelaySync, 1.0f}, {kDelayTempo, 1.0f}, {kDelayPingPong, 1.0f},
            {kDelayFeedback, 0.58f}, {kDelayMix, 0.42f},
            {kLimActive, 1.0f}, {kLimMode, 0.0f}, {kLimDrive, 1.8f}, {kLimCeiling, -0.3f}
        });
        addPreset("COMBO: Cyber Gate", {
            {kScvActive, 1.0f}, {kDrive, 3.0f}, {kAutoGain, 1.0f},
            {kFAType, 1.0f}, {kFAMode, 0.0f}, {kFACutoff, 950.0f}, {kFAReso, 0.68f},
            {kLfo1Freq, 0.75f}, {kLfo1Wave, 1.0f}, {kModL1T1, 0.55f},
            {kQeActive, 1.0f}, {kQeDepth, 7.0f}, {kQeDownsample, 3.0f}, {kQeMix, 0.45f},
            {kTgActive, 1.0f}, {kTgPattern, 44.0f}, {kTgOffset, 20.0f}, {kTgDepth, 90.0f},
            {kDelayActive, 1.0f}, {kDelaySync, 1.0f}, {kDelayTempo, 0.0f}, {kDelayPingPong, 1.0f},
            {kDelayFeedback, 0.38f}, {kDelayMix, 0.28f},
            {kLimActive, 1.0f}, {kLimMode, 0.0f}, {kLimDrive, 3.5f}, {kLimCeiling, -0.4f}
        });
        addPreset("COMBO: Drum Smash", {
            {kScvActive, 1.0f}, {kDrive, 2.8f}, {kThreshold, -3.5f}, {kAutoGain, 1.0f}, {kMix, 0.80f},
            {kFBType, 0.0f}, {kFBMode, 1.0f}, {kFBCutoff, 38.0f}, {kFBReso, 0.30f}, {kFADrive, 1.6f},
            {kLimActive, 1.0f}, {kLimMode, 1.0f}, {kLimDrive, 8.0f}, {kLimRelease, 75.0f}, {kLimCeiling, -0.3f}
        });
        addPreset("COMBO: Neon Synth", {
            {kRouting, 1.0f}, {kScvActive, 1.0f}, {kDrive, 2.0f}, {kAutoGain, 1.0f},
            {kFAType, 1.0f}, {kFAMode, 0.0f}, {kFACutoff, 1400.0f}, {kFAReso, 0.45f},
            {kFBType, 2.0f}, {kFBMode, 0.0f}, {kFBCutoff, 4200.0f}, {kFBReso, 0.40f},
            {kLfo1Freq, 0.28f}, {kModL1T1, 0.45f}, {kLfo2Freq, 0.35f}, {kModL2T4, -0.45f},
            {kDelayActive, 1.0f}, {kDelaySync, 1.0f}, {kDelayTempo, 1.0f}, {kDelayPingPong, 1.0f},
            {kDelayFeedback, 0.42f}, {kDelayMix, 0.32f},
            {kLimActive, 1.0f}, {kLimMode, 0.0f}, {kLimDrive, 0.0f}, {kLimCeiling, -0.3f}
        });
        addPreset("COMBO: Warehouse", {
            {kScvActive, 1.0f}, {kDrive, 4.8f}, {kExhaustion, 1.0f}, {kThreshold, -6.5f}, {kAutoGain, 1.0f},
            {kFAType, 2.0f}, {kFAMode, 0.0f}, {kFACutoff, 720.0f}, {kFAReso, 0.70f}, {kFADrive, 1.8f},
            {kTgActive, 1.0f}, {kTgPattern, 24.0f}, {kTgOffset, 15.0f}, {kTgDepth, 80.0f},
            {kDelayActive, 1.0f}, {kDelaySync, 1.0f}, {kDelayTempo, 0.0f}, {kDelayFeedback, 0.45f}, {kDelayMix, 0.30f},
            {kLimActive, 1.0f}, {kLimMode, 1.0f}, {kLimDrive, 5.0f}, {kLimCeiling, -0.4f}
        });
        addPreset("COMBO: Glitch Talk", {
            {kFAType, 1.0f}, {kFAMode, 2.0f}, {kFACutoff, 1000.0f}, {kFAReso, 0.75f},
            {kLfo1Freq, 4.2f}, {kLfo1Wave, 4.0f}, {kModL1T1, 0.65f},
            {kQeActive, 1.0f}, {kQeDepth, 5.0f}, {kQeDownsample, 6.0f}, {kQeMix, 0.65f},
            {kTgActive, 1.0f}, {kTgPattern, 111.0f}, {kTgDepth, 95.0f},
            {kDelayActive, 1.0f}, {kDelaySync, 1.0f}, {kDelayTempo, 0.0f}, {kDelayPingPong, 1.0f}, {kDelayMix, 0.30f},
            {kLimActive, 1.0f}, {kLimMode, 0.0f}, {kLimDrive, 0.0f}, {kLimCeiling, -0.3f}
        });
        addPreset("COMBO: Acid Rain", {
            {kRouting, 1.0f}, {kScvActive, 1.0f}, {kDrive, 2.0f}, {kAutoGain, 1.0f},
            {kFAType, 2.0f}, {kFAMode, 0.0f}, {kFACutoff, 900.0f}, {kFAReso, 0.80f}, {kFADrive, 1.5f},
            {kFBType, 2.0f}, {kFBMode, 2.0f}, {kFBCutoff, 2600.0f}, {kFBReso, 0.72f},
            {kLfo1Freq, 0.65f}, {kModL1T1, 0.60f}, {kLfo2Freq, 1.10f}, {kModL2T4, -0.55f},
            {kNoiseActive, 1.0f}, {kNoiseType, 1.0f}, {kNoiseLevel, -34.0f},
            {kDelayActive, 1.0f}, {kDelaySync, 1.0f}, {kDelayTempo, 0.0f}, {kDelayPingPong, 1.0f},
            {kDelayFeedback, 0.55f}, {kDelayMix, 0.38f}, {kLimActive, 1.0f}, {kLimCeiling, -0.3f}
        });
        addPreset("COMBO: Doom Drone", {
            {kScvActive, 1.0f}, {kDrive, 5.8f}, {kExhaustion, 1.0f}, {kThreshold, -8.0f}, {kAutoGain, 1.0f},
            {kNoiseActive, 1.0f}, {kNoiseType, 4.0f}, {kNoiseFreq, 55.0f}, {kNoiseLevel, -38.0f}, {kNoiseFmMod, 0.25f},
            {kFAType, 2.0f}, {kFAMode, 0.0f}, {kFACutoff, 380.0f}, {kFAReso, 0.72f},
            {kFBType, 0.0f}, {kFBMode, 3.0f}, {kFBCutoff, 950.0f}, {kFBReso, 0.65f},
            {kLfo1Freq, 0.12f}, {kModL1T1, 0.50f}, {kLfo2Freq, 0.19f}, {kModL2T4, -0.45f},
            {kQeActive, 1.0f}, {kQeDepth, 6.0f}, {kQeDownsample, 3.0f}, {kQeMix, 0.45f},
            {kDelayActive, 1.0f}, {kDelayTime, 460.0f}, {kDelayPingPong, 1.0f}, {kDelayFeedback, 0.60f}, {kDelayMix, 0.35f},
            {kLimActive, 1.0f}, {kLimMode, 0.0f}, {kLimDrive, 0.0f}, {kLimCeiling, -0.3f}
        });
        addPreset("COMBO: Vaporwave", {
            {kScvActive, 1.0f}, {kDrive, 1.7f}, {kAutoGain, 1.0f},
            {kFAType, 0.0f}, {kFAMode, 0.0f}, {kFACutoff, 3200.0f}, {kFAReso, 0.30f},
            {kNoiseActive, 1.0f}, {kNoiseType, 1.0f}, {kNoiseRoute, 1.0f}, {kNoiseLevel, -36.0f},
            {kQeActive, 1.0f}, {kQeDepth, 7.5f}, {kQeDownsample, 5.0f}, {kQeMix, 0.55f},
            {kDelayActive, 1.0f}, {kDelayTime, 310.0f}, {kDelayPingPong, 1.0f}, {kDelayFeedback, 0.48f}, {kDelayMix, 0.38f}
        });
        addPreset("COMBO: Electro Pump", {
            {kScvActive, 1.0f}, {kDrive, 4.5f}, {kExhaustion, 1.0f}, {kThreshold, -5.8f}, {kAutoGain, 1.0f},
            {kFAType, 2.0f}, {kFAMode, 0.0f}, {kFACutoff, 5400.0f}, {kFAReso, 0.55f}, {kFAGrit, 0.28f},
            {kScActive, 1.0f}, {kScSource, 1.0f}, {kScMode, 0.0f}, {kScRatio, 10.0f}, {kScThresh, -18.0f}, {kScAmount, -80.0f},
            {kScAttack, 1.5f}, {kScRelease, 165.0f},
            {kScTargetGain, 1.0f}, {kScTargetCut, 1.0f}, {kScTargetReso, 1.0f}
        });
    }

    InputGainStager inputStager_;
    EnvelopeFollower envFollower_;
    Waveshaper waveshaper_;
    MushinFilter filterA_, filterB_;
    mpc::dsp::LFO lfo1_, lfo2_;
    SidechainProcessor sidechain_;
    NoiseOscillator noiseOsc_;
    TranceGateProcessor tranceGate_;
    QuantizationErrorProcessor quantError_;
    DelayProcessor delay_;
    LimiterProcessor limiter_;

    mpc::dsp::Smoother smoothedGain_, smoothedMix_, smoothedFilterMix_, smoothedScMix_;
    mpc::dsp::Smoother smoothedQeDepth_, smoothedQeMix_;
    mpc::dsp::Smoother smoothedDelayTime_, smoothedDelayFb_, smoothedDelayMix_;
    mpc::dsp::Smoother smoothedLimDrive_, smoothedLimCeil_, smoothedLimRel_, smoothedLimMix_;

    float baseValues_[kNumParams] = {};
    bool updatingFromMacro_ = false;
    int blocksSinceReset_ = 0;
    int meterNotifyCounter_ = 0;
    float lastSentInMeter_ = -1.0f;
    float lastSentDuckMeter_ = -1.0f;
    float lastSentLimGrMeter_ = -1.0f;
    float lastSentDecayViz_ = -1.0f;
    int lastProgram_ = 0;
};

MPC_PLUGIN_ENTRY(Mushin)
