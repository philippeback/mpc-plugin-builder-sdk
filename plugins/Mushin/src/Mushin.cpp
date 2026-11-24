// Mushin.cpp - MPC standalone port of the Mushin multi-stage effect plugin.
//
// Signal path:
//   in -> [Pre-Dist Noise] -> S-Curve Waveshaper -> [Pre-Filter Noise]
//      -> Dual Filter & LFO Matrix (Serial/Parallel) -> Info Decay (Bit/Downsample)
//      -> [Post-Filter Noise] -> Dry/Wet Mix -> Trance Gate -> Final Gain
//      -> Stereo Delay -> Final Limiter -> out
//
// Sidechain envelope follower modulates Drive, Filter Cutoff, or Final Gain.

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
        drive_.snap(drive_.target());
        threshold_.snap(threshold_.target());
        inputPeak_ = 0.0f;
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
            const float satMix = clamp(std::max((currentDrive - 1.0f) * 4.0f, (1.0f - currentThreshold) * 4.0f), 0.0f, 1.0f);
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
// Stereo Filter (Clean / Vintage / Acid / Digital + LP / HP / BP / Notch)
// ---------------------------------------------------------------------------
class MushinFilter
{
public:
    enum class Type { Clean = 0, Vintage, Acid, Digital };
    enum class Mode { Lowpass = 0, Highpass, Bandpass, Notch };

    void prepare(double sampleRate)
    {
        svfL_.setSampleRate(sampleRate);
        svfR_.setSampleRate(sampleRate);
        reset();
    }

    void reset()
    {
        svfL_.reset();
        svfR_.reset();
    }

    void setType(int t) { type_ = (Type)clamp(t, 0, 3); }
    void setMode(int m) { mode_ = (Mode)clamp(m, 0, 3); }
    void setDrive(float d) { drive_ = d; }
    void setGrit(float g) { grit_ = g; }

    void setCoefficients(float cutoffHz, float resonance01)
    {
        const float c = clamp(cutoffHz, 20.0f, 20000.0f);
        float r = clamp(resonance01, 0.0f, 1.0f);
        if (type_ == Type::Acid) {
            r = clamp(r * 1.15f, 0.0f, 0.99f);
        }
        bypass_ = (type_ == Type::Clean && mode_ == Mode::Lowpass && c >= 19950.0f && drive_ <= 1.001f && grit_ <= 0.001f);
        svfL_.set(c, r);
        svfR_.set(c, r);
    }

    float processSample(int ch, float sample) noexcept
    {
        if (bypass_) return sample;

        float preDrive = drive_;
        if (type_ == Type::Vintage) preDrive *= 1.25f;
        else if (type_ == Type::Acid) preDrive *= 1.4f;

        const float preMix = clamp((preDrive - 1.0f) * 4.0f, 0.0f, 1.0f);
        const float driven = sample * preDrive;
        const float x = driven + (fastTanh(driven) - driven) * preMix;

        SVF::Mode svfMode = SVF::Mode::Lowpass;
        switch (mode_) {
            case Mode::Lowpass:  svfMode = SVF::Mode::Lowpass; break;
            case Mode::Highpass: svfMode = SVF::Mode::Highpass; break;
            case Mode::Bandpass: svfMode = SVF::Mode::Bandpass; break;
            case Mode::Notch:    svfMode = SVF::Mode::Notch; break;
        }

        float filtered = (ch == 0) ? svfL_.process(x, svfMode) : svfR_.process(x, svfMode);
        float out = filtered * (1.0f + grit_);
        const float postMix = clamp(std::max(preMix, grit_ * 10.0f), 0.0f, 1.0f);
        return out + (fastTanh(out) - out) * postMix;
    }

private:
    Type type_ = Type::Clean;
    Mode mode_ = Mode::Lowpass;
    float drive_ = 1.0f;
    float grit_ = 0.0f;
    bool bypass_ = false;
    SVF svfL_, svfR_;
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
    enum class Source { Internal = 0, External, TestSignal };
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
        hpFilter_.reset();
        lpFilter_.reset();
    }

    void setParameters(float attackMs, float releaseMs, bool isRMS, float thresholdDb,
                       float amount, float hpFreq, float lpFreq)
    {
        attack_ = (attackMs > 0.0f) ? std::exp(-1.0f / (float(sampleRate_) * attackMs * 0.001f)) : 0.0f;
        release_ = (releaseMs > 0.0f) ? std::exp(-1.0f / (float(sampleRate_) * releaseMs * 0.001f)) : 0.0f;
        rmsMode_ = isRMS;
        threshold_ = dbToGain(thresholdDb);
        amount_ = amount;

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
    void setSource(int s) { source_ = (Source)clamp(s, 0, 2); }
    Source getSource() const { return source_; }
    void setTarget(int t) { target_ = (Target)clamp(t, 0, 2); }
    Target getTarget() const { return target_; }

    float processSample(float sidechainSample) noexcept
    {
        if (!active_) return 0.0f;

        float filtered = hpFilter_.process(sidechainSample, SVF::Mode::Highpass);
        filtered = lpFilter_.process(filtered, SVF::Mode::Lowpass);

        float x = std::fabs(filtered);
        if (rmsMode_) x = x * x;

        if (x > envelope_)
            envelope_ = attack_ * (envelope_ - x) + x;
        else
            envelope_ = release_ * (envelope_ - x) + x;

        const float env = clamp(rmsMode_ ? std::sqrt(std::max(0.0f, envelope_)) : envelope_, 0.0f, 1.0f);
        float modulation = 0.0f;
        if (env > threshold_) {
            modulation = (env - threshold_) / (1.0f - threshold_ + 1.0e-4f);
        }
        return modulation * amount_;
    }

private:
    double sampleRate_ = 44100.0;
    float attack_ = 0.0f, release_ = 0.0f, envelope_ = 0.0f;
    bool rmsMode_ = false;
    bool active_ = false;
    Source source_ = Source::Internal;
    Target target_ = Target::Gain;
    float threshold_ = 0.0f;
    float amount_ = 0.0f;
    float hpFreq_ = 20.0f;
    float lpFreq_ = 20000.0f;
    SVF hpFilter_, lpFilter_;
};

// ---------------------------------------------------------------------------
// Trance Gate
// ---------------------------------------------------------------------------
static constexpr uint16_t kGateMasks[8] = {
    0xAAAA, // Straight 16th
    0x5555, // Offbeat 16th
    0xEEEE, // Classic 1
    0x9999, // Classic 2
    0xF0F0, // Four-On-Floor
    0xD7D7, // Galop
    0x8888, // Space Gate
    0x8912  // Euclidean 5
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
        addParam(choiceParam("filter_a_type",      "Type",     {"CLN", "VINT", "ACID", "DIGI"}, 0));
        addParam(choiceParam("filter_a_mode",      "Mode",     {"LP", "HP", "BP", "NOTCH"}, 0));

        // Filter B & Routing
        addParam(logParam   ("filter_b_cutoff",    "Cutoff",   20.0f, 20000.0f, 20000.0f, "Hz", 0));
        addParam(floatParam ("filter_b_resonance", "Reso",     0.0f, 1.0f, 0.1f, "", 2));
        addParam(floatParam ("filter_b_drive",     "Drive",    1.0f, 10.0f, 1.0f, "x", 1));
        addParam(floatParam ("filter_b_grit",      "Grit",     0.0f, 1.0f, 0.0f, "", 2));
        addParam(choiceParam("filter_b_type",      "Type",     {"CLN", "VINT", "ACID", "DIGI"}, 0));
        addParam(choiceParam("filter_b_mode",      "Mode",     {"LP", "HP", "BP", "NOTCH"}, 0));
        addParam(choiceParam("routing",            "Path",     {"SERIAL", "PARAL"}, 0));

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
        addParam(choiceParam("sc_source",    "Source",  {"INT", "EXT", "TEST"}, 0));
        addParam(floatParam ("sc_threshold", "Thresh",  -60.0f, 0.0f, -24.0f, "dB", 1));
        addParam(floatParam ("sc_attack",    "Attack",  0.1f, 500.0f, 10.0f, "ms", 1));
        addParam(floatParam ("sc_release",   "Release", 1.0f, 2000.0f, 100.0f, "ms", 0));
        addParam(choiceParam("sc_mode",      "Mode",    {"PEAK", "RMS"}, 0));
        addParam(floatParam ("sc_amount",    "Amount",  -100.0f, 100.0f, 0.0f, "%", 0));
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
        addParam(choiceParam("tg_pattern", "Pattern",
            {"Straight 16", "Offbeat 16", "Classic 1", "Classic 2", "4-On-Floor", "Galop", "Space Gate", "Euclidean 5"}, 0));
        addParam(choiceParam("tg_rate",    "Rate",    {"1/16", "1/8", "1/4"}, 0));
        addParam(floatParam ("tg_start",   "Start",   0.0f, 100.0f, 5.0f, "ms", 1));
        addParam(floatParam ("tg_hold",    "Hold",    10.0f, 100.0f, 50.0f, "%", 0));
        addParam(floatParam ("tg_end",     "End",     0.0f, 200.0f, 10.0f, "ms", 1));
        addParam(floatParam ("tg_depth",   "Depth",   0.0f, 100.0f, 100.0f, "%", 0));

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
        } else {
            updateBaseFromTarget(index, plainValue);
            if (index == kRouting && get(kMacroChoke) > 0.02f) {
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
        waveshaper_.reset();
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
        smoothedQeDepth_.snap(get(kQeDepth));
        smoothedQeMix_.snap(get(kQeMix));
        smoothedDelayTime_.snap(get(kDelayTime));
        smoothedDelayFb_.snap(get(kDelayFeedback));
        smoothedDelayMix_.snap(get(kDelayMix));
        smoothedLimDrive_.snap(get(kLimDrive));
        smoothedLimCeil_.snap(get(kLimCeiling));
        smoothedLimRel_.snap(get(kLimRelease));
        smoothedLimMix_.snap(get(kLimMix));
    }

    void process(mpc::ProcessContext& ctx) override
    {
        const double sr = ctx.sampleRate;
        const int n = ctx.numFrames;

        // 1. Update per-block parameters (macros already reflected into underlying params)
        waveshaper_.setDrive(mpc::clamp(get(kDrive), 1.0f, 32.0f));
        waveshaper_.setExhaustion(getBool(kExhaustion));
        waveshaper_.setThreshold(mpc::dsp::dbToGain(get(kThreshold)));
        waveshaper_.setAutoGain(getBool(kAutoGain));

        const float baseFACutoff = mpc::clamp(get(kFACutoff), 20.0f, 20000.0f);
        const float baseFAReso = mpc::clamp(get(kFAReso), 0.0f, 0.98f);
        const float baseFAGrit = mpc::clamp(get(kFAGrit), 0.0f, 1.0f);
        filterA_.setDrive(get(kFADrive));
        filterA_.setGrit(baseFAGrit);
        filterA_.setType(getInt(kFAType));
        filterA_.setMode(getInt(kFAMode));

        const float baseFBCutoff = mpc::clamp(get(kFBCutoff), 20.0f, 20000.0f);
        const float baseFBReso = mpc::clamp(get(kFBReso), 0.0f, 0.98f);
        const float baseFBGrit = mpc::clamp(get(kFBGrit), 0.0f, 1.0f);
        filterB_.setDrive(get(kFBDrive));
        filterB_.setGrit(baseFBGrit);
        filterB_.setType(getInt(kFBType));
        filterB_.setMode(getInt(kFBMode));

        const bool parallelRouting = (getInt(kRouting) == 1);

        lfo1_.setFrequency(mpc::clamp(get(kLfo1Freq), 0.1f, 25.0f));
        lfo1_.setShape(getInt(kLfo1Wave));
        lfo2_.setFrequency(mpc::clamp(get(kLfo2Freq), 0.1f, 25.0f));
        lfo2_.setShape(getInt(kLfo2Wave));

        float mod1[6], mod2[6];
        for (int t = 0; t < 6; ++t) {
            mod1[t] = get(kModL1T1 + t);
            mod2[t] = get(kModL2T1 + t);
        }

        sidechain_.setActive(getBool(kScActive));
        sidechain_.setSource(getInt(kScSource));
        sidechain_.setTarget(getInt(kScTarget));
        sidechain_.setParameters(
            get(kScAttack), get(kScRelease),
            getInt(kScMode) == 1,
            get(kScThresh), get(kScAmount) * 0.01f,
            get(kScHpf), get(kScLpf)
        );

        smoothedGain_.setTarget(std::max(0.0f, get(kGain)));
        smoothedMix_.setTarget(get(kMix));

        const bool noiseActive = getBool(kNoiseActive);
        const int noiseRoute = getInt(kNoiseRoute);
        const float noiseGain = mpc::dsp::dbToGain(get(kNoiseLevel));
        const float fmAmount = get(kNoiseFmMod);
        const bool needNoise = noiseActive || (sidechain_.isActive() && sidechain_.getSource() == SidechainProcessor::Source::TestSignal);
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
        const uint16_t patternMask = kGateMasks[mpc::clamp(getInt(kTgPattern), 0, 7)];

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
        const bool tgActive = getBool(kTgActive) && (stepSamples > 0.0);

        const float* inL = ctx.inputs[0];
        const float* inR = (ctx.numInputs > 1 && ctx.inputs[1]) ? ctx.inputs[1] : ctx.inputs[0];
        float* outL = ctx.outputs[0];
        float* outR = (ctx.numOutputs > 1 && ctx.outputs[1]) ? ctx.outputs[1] : ctx.outputs[0];

        // Cache exp/pow values for 16-sample sub-blocks
        float qeSteps = 1.0f;
        float lastQeDepth = -1.0f;
        float limDriveGain = 1.0f, limCeilGain = 1.0f, limRelCoef = 0.99f;

        // 2. Sample loop
        for (int s = 0; s < n; ++s) {
            const float mixVal = smoothedMix_.process();
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

            const float dryL = inL[s];
            const float dryR = inR[s];

            float genSample = 0.0f;
            if (needNoise) {
                genSample = noiseOsc_.nextSample() * noiseGain;
            }
            const float fmMod = genSample * fmAmount;

            float scMod = 0.0f;
            if (sidechain_.isActive()) {
                const float scIn = (sidechain_.getSource() == SidechainProcessor::Source::TestSignal)
                                   ? genSample
                                   : 0.5f * (dryL + dryR);
                scMod = sidechain_.processSample(scIn);
            }

            float tgGain = 1.0f;
            if (tgActive) {
                tgGain = tranceGate_.processSample(
                    tranceGate_.currentSamplePos() + s,
                    stepSamples, cycleSamples, patternMask,
                    holdWidthSamples, attackEnd, decayStart,
                    baseAtten, tgMix
                );
            }

            // Update LFOs every sample; update SVF coefficients every 16 samples to keep ARM CPU low
            const float lfo1Val = lfo1_.process();
            const float lfo2Val = lfo2_.process();

            if ((s & 15) == 0) {
                float off[6];
                for (int t = 0; t < 6; ++t) {
                    off[t] = lfo1Val * mod1[t] + lfo2Val * mod2[t];
                }
                off[0] += fmMod;
                off[3] += fmMod;

                float cutA = baseFACutoff * std::pow(2.0f, off[0] * 5.0f);
                float cutB = baseFBCutoff * std::pow(2.0f, off[3] * 5.0f);
                if (sidechain_.isActive() && sidechain_.getTarget() == SidechainProcessor::Target::Cutoff) {
                    const float scCut = std::pow(2.0f, scMod * 2.0f);
                    cutA *= scCut;
                    cutB *= scCut;
                }

                filterA_.setGrit(mpc::clamp(baseFAGrit + off[2], 0.0f, 1.0f));
                filterA_.setCoefficients(cutA, mpc::clamp(baseFAReso + off[1], 0.0f, 1.0f));
                filterB_.setGrit(mpc::clamp(baseFBGrit + off[5], 0.0f, 1.0f));
                filterB_.setCoefficients(cutB, mpc::clamp(baseFBReso + off[4], 0.0f, 1.0f));

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
            if (sidechain_.isActive() && sidechain_.getTarget() == SidechainProcessor::Target::Drive) {
                effDrive = mpc::clamp(effDrive + scMod * 20.0f, 1.0f, 50.0f);
            }

            float curGain = gainVal;
            if (sidechain_.isActive() && sidechain_.getTarget() == SidechainProcessor::Target::Gain) {
                curGain += scMod;
            }
            curGain = std::max(0.0f, curGain);

            float wet[2] = { dryL, dryR };
            const float dry[2] = { dryL, dryR };

            for (int ch = 0; ch < 2; ++ch) {
                float w = wet[ch];

                if (noiseActive && noiseRoute == 0) w += genSample;
                w = waveshaper_.processSample(w, effDrive, wsThresh);

                if (noiseActive && noiseRoute == 1) w += genSample;
                if (!parallelRouting) {
                    w = filterB_.processSample(ch, filterA_.processSample(ch, w));
                } else {
                    w = 0.5f * (filterA_.processSample(ch, w) + filterB_.processSample(ch, w));
                }

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
            }

            outL[s] = wet[0];
            outR[s] = wet[1];
        }

        if (!ctx.transport.playing && tgActive) {
            tranceGate_.advanceFreeRunningClock(n, cycleSamples);
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
        setTargetParam(kDrive, mpc::clamp(baseValues_[kDrive] + 12.0f * mExh, 1.0f, 20.0f));
        setTargetParam(kExhaustion, (baseValues_[kExhaustion] >= 0.5f || mExh > 0.65f) ? 1.0f : 0.0f);
        setTargetParam(kGain, std::max(0.0f, baseValues_[kGain] * (1.0f - 0.35f * mExh)));

        // CHOKE + KINETIC MOD + RESONATOR (Filters)
        setTargetParam(kFACutoff, mpc::clamp(baseValues_[kFACutoff] * std::pow(2.0f, -7.0f * mChoke - 3.5f * mKinetic), 20.0f, 20000.0f));
        setTargetParam(kFAReso, mpc::clamp(baseValues_[kFAReso] + 0.50f * mChoke + 0.35f * mKinetic, 0.0f, 0.98f));
        setTargetParam(kFAGrit, mpc::clamp(baseValues_[kFAGrit] + 0.5f * mReso, 0.0f, 1.0f));

        const bool isParallel = ((int)std::lround(baseValues_[kRouting]) == 1);
        const bool chokeBand = (!isParallel && mChoke > 0.02f && (int)std::lround(baseValues_[kFBMode]) == 0 && baseValues_[kFBCutoff] > 15000.0f);
        const float fbCutoff = chokeBand
            ? mpc::clamp(20.0f * std::pow(2.0f, 5.0f * mChoke), 20.0f, 20000.0f)
            : mpc::clamp(baseValues_[kFBCutoff] * std::pow(2.0f, (isParallel ? -7.0f : -4.0f) * mChoke - 2.5f * mKinetic), 20.0f, 20000.0f);
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

        // GATE CARVE
        setTargetParam(kTgActive, (baseValues_[kTgActive] >= 0.5f || mGate > 0.05f) ? 1.0f : 0.0f);
        setTargetParam(kTgHold, (mGate > 0.05f) ? std::min(baseValues_[kTgHold], 75.0f - 47.0f * mGate) : baseValues_[kTgHold]);
        setTargetParam(kTgDepth, std::max(baseValues_[kTgDepth], 100.0f * mGate));
        setTargetParam(kTgMix, std::max(baseValues_[kTgMix], mGate));

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
                const bool isParallel = ((int)std::lround(baseValues_[kRouting]) == 1);
                const bool chokeBand = (!isParallel && mChoke > 0.02f && (int)std::lround(baseValues_[kFBMode]) == 0 && baseValues_[kFBCutoff] > 15000.0f);
                baseValues_[idx] = chokeBand
                    ? val
                    : mpc::clamp(val * std::pow(2.0f, (isParallel ? 7.0f : 4.0f) * mChoke + 2.5f * mKinetic), 20.0f, 20000.0f);
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
            }
        }
        addProgram(name, std::move(vals));
    }

    void addBuiltInPrograms()
    {
        addPreset("Init", {});

        addPreset("Warm Tape Sat", {
            {kDrive, 4.5f}, {kThreshold, -6.0f}, {kAutoGain, 1.0f}, {kMix, 0.85f},
            {kFACutoff, 6500.0f}, {kFAType, 1.0f}, {kDelayActive, 1.0f}, {kDelayTime, 240.0f},
            {kDelayFeedback, 0.25f}, {kDelayMix, 0.2f}
        });

        addPreset("Dual Resonator", {
            {kDrive, 2.5f}, {kFACutoff, 900.0f}, {kFAReso, 0.65f}, {kFAType, 2.0f},
            {kFBCutoff, 2400.0f}, {kFBReso, 0.55f}, {kFAMode, 2.0f}, {kFBMode, 0.0f},
            {kRouting, 1.0f}, {kLfo1Freq, 0.4f}, {kModL1T1, 0.35f}, {kModL1T4, -0.3f}
        });

        addPreset("Crushed Velvet", {
            {kDrive, 6.0f}, {kExhaustion, 1.0f}, {kThreshold, -8.0f}, {kAutoGain, 1.0f},
            {kQeActive, 1.0f}, {kQeDepth, 6.0f}, {kQeDownsample, 4.0f}, {kQeMix, 0.7f},
            {kFACutoff, 4200.0f}, {kFAReso, 0.3f}
        });

        addPreset("Rhythmic Gater", {
            {kDrive, 3.0f}, {kTgActive, 1.0f}, {kTgPattern, 4.0f}, {kTgRate, 0.0f},
            {kTgHold, 65.0f}, {kTgDepth, 90.0f}, {kDelayActive, 1.0f}, {kDelaySync, 1.0f},
            {kDelayTempo, 1.0f}, {kDelayPingPong, 1.0f}, {kDelayMix, 0.35f}
        });

        addPreset("Mushin Pumper", {
            {kDrive, 3.5f}, {kScActive, 1.0f}, {kScSource, 0.0f}, {kScTarget, 2.0f},
            {kScThresh, -18.0f}, {kScAmount, -80.0f}, {kScAttack, 5.0f}, {kScRelease, 160.0f},
            {kLimActive, 1.0f}, {kLimMode, 1.0f}, {kLimDrive, 6.0f}, {kLimCeiling, -0.5f}
        });
    }

    Waveshaper waveshaper_;
    MushinFilter filterA_, filterB_;
    mpc::dsp::LFO lfo1_, lfo2_;
    SidechainProcessor sidechain_;
    NoiseOscillator noiseOsc_;
    TranceGateProcessor tranceGate_;
    QuantizationErrorProcessor quantError_;
    DelayProcessor delay_;
    LimiterProcessor limiter_;

    mpc::dsp::Smoother smoothedGain_, smoothedMix_;
    mpc::dsp::Smoother smoothedQeDepth_, smoothedQeMix_;
    mpc::dsp::Smoother smoothedDelayTime_, smoothedDelayFb_, smoothedDelayMix_;
    mpc::dsp::Smoother smoothedLimDrive_, smoothedLimCeil_, smoothedLimRel_, smoothedLimMix_;

    float baseValues_[kNumParams] = {};
    bool updatingFromMacro_ = false;
};

MPC_PLUGIN_ENTRY(Mushin)
