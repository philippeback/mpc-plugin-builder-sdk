few subtle edge cases in the code are worth tightening before you take it onto a club PA:

Stereo-unlinked Limiter (LimiterProcessor::processSample): env_[ch] and smoothedGR_[ch] run independently per channel. On a live PA—especially with your L/R or M/S presets—unlinked limiting will pull the stereo image toward the quieter side whenever one channel hits the ceiling. Driving a single shared envelope from std::max(std::fabs(in[0]), std::fabs(in[1])) keeps the center locked.

GATE CARVE macro pattern desync (applyMacros): When mGate > 0.05f forces kTgPattern to 4.0f (4-On-Floor), updatingFromMacro_ is true, so parameterChanged() never unpacks kGateMasks[4] into kTgStep0..15. Because process() builds patternMask strictly from kTgStep0..15, twisting GATE CARVE up on an Init patch will still fire kGateMasks[0] (Straight 16) instead of 4-On-Floor.

Stale delay buffer on live macro sweeps (DelayProcessor::processSample): The early if (mix <= 0.001f) return; freezes the delay line in place. If you sweep RESONATOR back to zero and bring it up two minutes later in a set, stale audio from the previous track or section will burst out of the buffer. Continuing to write/decay (or clearing on zero-mix) avoids that surprise.

Waveshaper autogain_ double-decay (Waveshaper::processSample): inputPeak_ is updated inside processSample(), which is called twice per stereo frame (wet[0] then wet[1]). That halves the 100 ms release to 50 ms and applies slightly different gain compensation to Left vs. Right on asymmetric stereo signals. Computing inputPeak_ once per frame from std::max(std::fabs(wet[0]), std::fabs(wet[1])) fixes both.

Type::Digital filter path (MushinFilter): Currently Type::Digital executes the exact same SVF path as Type::Clean (the only difference is bypass_ is disabled). If you intended a sharper/non-linear digital character there, it isn't wired up yet.

Everything else—especially the 15 ms transient settle on InputGainStager, the reversible Filt -> Sat stage, and the 120+ structured presets—is dialed in for real-time hardware performance.