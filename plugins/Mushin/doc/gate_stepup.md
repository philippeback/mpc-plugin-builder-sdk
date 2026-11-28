 am not satisfied with the gate.

I would love to be able to go faster than 16 divisions per bar. for better stutters and glitching.

Appending faster divisions to the end of tg_rate keeps indices 0–2 intact so none of your existing presets break. Adding triplets alongside 1/32, 1/64, and 1/128 gives you everything from tight psytrance/neuro rolls up to ~79 Hz audio-rate buzz at 148 BPM.

1. Expand tg_rate in the constructor
C++
addParam(choiceParam("tg_rate", "Rate",
    {"1/16", "1/8", "1/4", "1/16T", "1/32", "1/32T", "1/64", "1/128"}, 0));
2. Map rateFactor in process()
Replace the rateIdx if/else block with a lookup table (divisions per quarter note):

C++
static constexpr double kRateFactors[8] = {
    4.0,   // 0: 1/16  (16/bar)
    2.0,   // 1: 1/8   (8/bar)
    1.0,   // 2: 1/4   (4/bar)
    6.0,   // 3: 1/16T (24/bar)
    8.0,   // 4: 1/32  (32/bar)
    12.0,  // 5: 1/32T (48/bar)
    16.0,  // 6: 1/64  (64/bar)
    32.0   // 7: 1/128 (128/bar)
};
const int rateIdx = mpc::clamp(getInt(kTgRate), 0, 7);
const double rateFactor = kRateFactors[rateIdx];
3. Throttle kTgPlayStep notifications
Right now kTgPlayStep calls notifyHostParameterChanged(kTgPlayStep) on every block whenever the step changes. At 1/64 or 1/128, that fires 40–80 times per second and can clog the MPC UI thread. Move it inside your existing 30 Hz samplesSinceNotify_ block:

C++
// Inside if (samplesSinceNotify_ >= notifyInterval) { ... }
notifyIfChanged(kTgPlayStep, tgPlayStep01, lastSentTgStep_, 0.005f);
4. Envelope behavior at high speeds
Your existing decayStart < attackEnd guard already prevents clicks or negative envelopes when stepSamples shrinks below Start + End (it collapses into a symmetric triangle). At 1/64 and 1/128, keep tg_start and tg_end around 0.5–1.5 ms with tg_hold at 40–60% to preserve sharp, percussive glitch transients instead of a soft tremolo.

Delay same boat, not having dotted delays suck

Appending dotted and triplet divisions after index 3 preserves all existing preset indices (1/16, 1/8, 1/4, 1/2).

1. Expand delay_tempo in the constructor
C++
addParam(choiceParam("delay_tempo", "Tempo", {
    "1/16", "1/8", "1/4", "1/2",
    "1/16D", "1/8D", "1/4D",
    "1/16T", "1/8T", "1/4T",
    "1/32", "1/32D"
}, 2));
2. Replace the syncType switch in DelayProcessor::processSample()
C++
if (sync) {
    const float bpm = syncBpm > 1.0f ? syncBpm : 120.0f;
    const float quarterSec = 60.0f / bpm;
    static constexpr float kSyncFactors[12] = {
        0.25f,       // 0: 1/16
        0.5f,        // 1: 1/8
        1.0f,        // 2: 1/4
        2.0f,        // 3: 1/2
        0.375f,      // 4: 1/16D
        0.75f,       // 5: 1/8D  (classic dub / psy / techno bounce)
        1.5f,        // 6: 1/4D
        0.1666667f,  // 7: 1/16T
        0.3333333f,  // 8: 1/8T
        0.6666667f,  // 9: 1/4T
        0.125f,      // 10: 1/32
        0.1875f      // 11: 1/32D
    };
    const float factor = kSyncFactors[clamp(syncType, 0, 11)];
    targetTimeMs = quarterSec * factor * 1000.0f;
}
3. Smooth synced time changes (optional, for live Q-Link switching)
Right now smoothedDelayTime_ only smooths free-running kDelayTime. When delay_sync is on, switching delay_tempo mid-bar jumps baseSamples instantaneously and can click. If you compute targetTimeMs in Mushin::process() (before the sample loop) and feed that into smoothedDelayTime_.setTarget(targetTimeMs), twisting delay_tempo live gives you smooth tape-pitch sweeps instead of clicks. Your existing 2.15s buffer already covers 1/4D down to 42 BPM without needing more RAM.