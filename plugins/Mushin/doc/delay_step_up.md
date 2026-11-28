The delay has a ping pong switch but could do better than just that.

Converting delay_pingpong from a boolParam to a choiceParam with 0 = STEREO and 1 = P-PONG preserves every existing preset while opening up the delay engine into a proper multi-topology unit—using the exact same DelayLine buffers and minimal ARM CPU.

1. Upgrade the parameter in the constructor
C++
addParam(choiceParam("delay_pingpong", "Mode",
    {"STEREO", "P-PONG", "3:4 X", "MULTI", "DIFFUSE"}, 0));
2. Five topologies inside DelayProcessor
0: STEREO — Existing straight L/R feedback.

1: P-PONG — Existing mono-summed L→R cross-bounce.

2: 3:4 X (Asymmetric Cross-Bounce) — Left reads at 1.0x time, Right reads at 0.75x (dotted relationship against the main time), with cross-feedback (L → R, R → L). Gives instant polyrhythmic psytrance/dub movement from a single time knob.

3: MULTI (3-Head Tape Echo) — Reads three taps from the existing buffers (0.33x, 0.66x, 1.0x) with alternating stereo weights, feeding only the main 1.0x tap back into the loop. Classic Space Echo density for ~4 extra multiplies per sample.

4: DIFFUSE (Dub Reverb / Smear) — Runs two short allpass diffusers (~11 ms and ~17 ms) in the feedback path. Combined with kDelayColor, negative values turn it into a dark dub-techno cavern and positive values turn it into a bright shimmer wash.

3. Drop-in logic for DelayProcessor::processSample()
Change bool pingpong to int mode (passing getInt(kDelayPingPong)):

C++
float delayedL = 0.0f, delayedR = 0.0f;

if (mode == 2) {
    // 3:4 X — Asymmetric polyrhythmic cross-bounce
    const float dL = clamp(baseSamples + modOffset, 1.0f, maxD);
    const float dR = clamp(baseSamples * 0.75f - modOffset, 1.0f, maxD);
    delayedL = delayL_.readLinear(dL);
    delayedR = delayR_.readLinear(dR);
    delayL_.write(left + lastFbR_ * feedback);
    delayR_.write(right + lastFbL_ * feedback);
} else if (mode == 3) {
    // MULTI — 3-head tape echo (33% / 66% / 100%)
    const float t1L = delayL_.readLinear(clamp(baseSamples * 0.333f + modOffset, 1.0f, maxD));
    const float t2R = delayR_.readLinear(clamp(baseSamples * 0.666f - modOffset, 1.0f, maxD));
    const float t3L = delayL_.readLinear(clamp(baseSamples + modOffset, 1.0f, maxD));
    const float t3R = delayR_.readLinear(clamp(baseSamples - modOffset, 1.0f, maxD));
    delayedL = 0.45f * t1L + 0.25f * t2R + 0.55f * t3L;
    delayedR = 0.25f * t1L + 0.45f * t2R + 0.55f * t3R;
    delayL_.write(left + lastFbL_ * feedback);
    delayR_.write(right + lastFbR_ * feedback);
} else if (mode == 1) {
    // P-PONG
    delayedL = delayL_.readLinear(clamp(baseSamples + modOffset, 1.0f, maxD));
    delayedR = delayR_.readLinear(clamp(baseSamples - modOffset, 1.0f, maxD));
    const float ppSeed = 0.5f * (left + right);
    delayL_.write(ppSeed + lastFbR_ * feedback);
    delayR_.write(lastFbL_ * feedback);
} else {
    // STEREO (0) and DIFFUSE (4)
    delayedL = delayL_.readLinear(clamp(baseSamples + modOffset, 1.0f, maxD));
    delayedR = delayR_.readLinear(clamp(baseSamples - modOffset, 1.0f, maxD));
    if (mode == 4) {
        // 2-stage Schroeder allpass smear (ap1L_, ap2L_, ap1R_, ap2R_, g = 0.55f)
        delayedL = ap2L_.process(ap1L_.process(delayedL, 0.55f), 0.55f);
        delayedR = ap2R_.process(ap1R_.process(delayedR, 0.55f), 0.55f);
        delayL_.write(left + lastFbR_ * feedback);
        delayR_.write(right + lastFbL_ * feedback);
    } else {
        delayL_.write(left + lastFbL_ * feedback);
        delayR_.write(right + lastFbR_ * feedback);
    }
}
4. Minimal Allpass helper for DIFFUSE
A tiny fixed-buffer allpass (prepared at ~11.3 ms / ~17.1 ms for L and ~12.7 ms / ~19.3 ms for R so stereo decorrelates cleanly):

C++
struct Allpass {
    std::vector<float> buf;
    int idx = 0;
    void prepare(int len) { buf.assign((size_t)std::max(1, len), 0.0f); idx = 0; }
    void reset() { std::fill(buf.begin(), buf.end(), 0.0f); idx = 0; }
    float process(float x, float g) noexcept {
        const float d = buf[idx];
        const float v = x + g * d;
        buf[idx] = v;
        if (++idx >= (int)buf.size()) idx = 0;
        return d - g * v;
    }
};
