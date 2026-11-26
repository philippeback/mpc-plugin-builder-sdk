// gen_orbs.cpp - Generates 22-frame cyber-mecha unfolding FilmStrips (180x3960 RGBA)
// for Mushin's 8 MAIN macro orbs, combining high-res 3D sphere textures from
// orbs_sheet.jpg with procedural ray-lit 3D shutters, iris blades, gimbal rings,
// split blast hatches, telescoping coils, cogwheels, hydraulic jaws, and rotating CAD bezels.

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <string>
#include <vector>

namespace {

constexpr double kPi = 3.14159265358979323846;
constexpr int kOutSize = 180;
constexpr int kSsaa = 2; // 2x2 MSAA per pixel (360x360 shader evaluation -> 180x180)
constexpr int kNumFrames = 22;

struct Vec3 {
    float r, g, b;
    Vec3 operator+(const Vec3& o) const { return {r + o.r, g + o.g, b + o.b}; }
    Vec3 operator-(const Vec3& o) const { return {r - o.r, g - o.g, b - o.b}; }
    Vec3 operator*(float s) const { return {r * s, g * s, b * s}; }
    Vec3 operator*(const Vec3& o) const { return {r * o.r, g * o.g, b * o.b}; }
};

struct Rgba {
    float r, g, b, a;
};

static inline float clampf(float v, float lo, float hi)
{
    return v < lo ? lo : (v > hi ? hi : v);
}

static inline float smoothstep(float e0, float e1, float x)
{
    float t = clampf((x - e0) / (e1 - e0), 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
}

static inline float mixf(float a, float b, float t)
{
    return a + (b - a) * t;
}

static inline Vec3 mixv(const Vec3& a, const Vec3& b, float t)
{
    return {mixf(a.r, b.r, t), mixf(a.g, b.g, t), mixf(a.b, b.b, t)};
}

static inline Rgba over(const Rgba& dst, const Vec3& srcRgb, float srcA)
{
    float sa = clampf(srcA, 0.0f, 1.0f);
    if (sa <= 0.0001f) return dst;
    float da = dst.a;
    float outA = sa + da * (1.0f - sa);
    if (outA <= 0.0001f) return {0.0f, 0.0f, 0.0f, 0.0f};
    Vec3 d = {dst.r, dst.g, dst.b};
    Vec3 out = (srcRgb * sa + d * (da * (1.0f - sa))) * (1.0f / outA);
    return {out.r, out.g, out.b, outA};
}

static inline Rgba addGlow(const Rgba& dst, const Vec3& glowRgb, float alphaBoost)
{
    float b = clampf(alphaBoost, 0.0f, 1.0f);
    if (b <= 0.0001f) return dst;
    float outA = std::max(dst.a, b);
    return {
        clampf(dst.r * (dst.a / std::max(outA, 1e-4f)) + glowRgb.r * (b / std::max(outA, 1e-4f)), 0.0f, 1.45f),
        clampf(dst.g * (dst.a / std::max(outA, 1e-4f)) + glowRgb.g * (b / std::max(outA, 1e-4f)), 0.0f, 1.45f),
        clampf(dst.b * (dst.a / std::max(outA, 1e-4f)) + glowRgb.b * (b / std::max(outA, 1e-4f)), 0.0f, 1.45f),
        outA
    };
}

struct SheetImage {
    int width = 0;
    int height = 0;
    std::vector<uint8_t> bgra;

    Vec3 sampleCell(int row, int col, float nx, float ny) const
    {
        static constexpr float kCellCx[2][4] = {
            {176.0f, 173.0f, 171.0f, 168.0f},
            {173.0f, 172.0f, 170.0f, 169.0f},
        };
        static constexpr float kCellCy[2][4] = {
            {199.0f, 201.0f, 200.0f, 201.0f},
            {182.0f, 182.0f, 182.0f, 182.0f},
        };
        static constexpr float kCellRad = 126.0f;

        const float cellW = float(width) * 0.25f;
        const float cellH = float(height) * 0.50f;
        const float px = float(col) * cellW + kCellCx[row][col] + nx * kCellRad;
        const float py = float(row) * cellH + kCellCy[row][col] + ny * kCellRad;

        const float fx = clampf(px, 0.0f, float(width - 2));
        const float fy = clampf(py, 0.0f, float(height - 2));
        const int x0 = (int)fx;
        const int y0 = (int)fy;
        const float tx = fx - float(x0);
        const float ty = fy - float(y0);

        auto fetch = [&](int x, int y) -> Vec3 {
            const size_t idx = ((size_t)y * (size_t)width + (size_t)x) * 4;
            return {
                bgra[idx + 2] / 255.0f,
                bgra[idx + 1] / 255.0f,
                bgra[idx + 0] / 255.0f
            };
        };

        const Vec3 c00 = fetch(x0, y0);
        const Vec3 c10 = fetch(x0 + 1, y0);
        const Vec3 c01 = fetch(x0, y0 + 1);
        const Vec3 c11 = fetch(x0 + 1, y0 + 1);
        return mixv(mixv(c00, c10, tx), mixv(c01, c11, tx), ty);
    }
};

static inline Vec3 brushMetal(float x, float y, float z, float grainCoord, float darkness = 1.0f)
{
    float invL = 1.0f / std::sqrt(0.55f * 0.55f + 0.65f * 0.65f + 0.52f * 0.52f);
    float lx = -0.55f * invL, ly = -0.65f * invL, lz = 0.52f * invL;
    float ndotl = std::max(0.0f, x * lx + y * ly + z * lz);

    float grain = 0.90f + 0.10f * std::sin(grainCoord * 75.0f) * std::cos(grainCoord * 19.0f);
    float hx = lx, hy = ly, hz = lz + 1.0f;
    float hLen = std::sqrt(hx * hx + hy * hy + hz * hz);
    float ndoth = std::max(0.0f, (x * hx + y * hy + z * hz) / hLen);
    float spec = std::pow(ndoth, 20.0f) * 0.58f;

    Vec3 base = Vec3{0.09f, 0.115f, 0.15f} * darkness * grain;
    Vec3 diff = Vec3{0.27f, 0.31f, 0.37f} * ndotl * darkness * grain;
    Vec3 rim  = Vec3{0.10f, 0.13f, 0.18f} * smoothstep(0.55f, 0.95f, 1.0f - z);
    return base + diff + rim + Vec3{spec, spec * 1.03f, spec * 1.08f};
}

static inline void rot2(float x, float y, float ang, float& ox, float& oy)
{
    float c = std::cos(ang), s = std::sin(ang);
    ox = c * x - s * y;
    oy = s * x + c * y;
}

static inline float wrapAnglePi(float a)
{
    while (a > (float)kPi) a -= (float)(2.0 * kPi);
    while (a < -(float)kPi) a += (float)(2.0 * kPi);
    return a;
}

Rgba renderPixel(const SheetImage& sheet, int orbIdx, float t, float px, float py)
{
    const bool isCyan = (orbIdx < 4);
    const Vec3 accent = isCyan ? Vec3{0.00f, 0.90f, 1.00f} : Vec3{1.00f, 0.30f, 0.55f};
    const Vec3 hotAccent = isCyan ? Vec3{0.62f, 0.98f, 1.00f} : Vec3{1.00f, 0.70f, 0.86f};
    const Vec3 subAccent = isCyan ? Vec3{0.75f, 0.25f, 0.95f} : Vec3{0.25f, 0.85f, 1.00f};

    const float cx = px - 90.0f;
    const float cy = py - 90.0f;
    const float r = std::sqrt(cx * cx + cy * cy);
    const float ang = std::atan2(cy, cx);

    const float coreR = 66.5f;
    const float rn = r / coreR;
    const float nx = cx / coreR;
    const float ny = cy / coreR;

    Rgba col = {0.0f, 0.0f, 0.0f, 0.0f};

    // -----------------------------------------------------------------------
    // 1. Socket shadow, corner mounting lugs + hex bolts, titanium chassis
    // -----------------------------------------------------------------------
    if (r < 89.0f) {
        float shAlpha = smoothstep(89.0f, 75.0f, r) * 0.58f;
        col = over(col, Vec3{0.01f, 0.015f, 0.025f}, shAlpha);
    }

    {
        float boltDist = 999.0f;
        for (int k = 0; k < 4; ++k) {
            float ba = (float)kPi * (0.25f + 0.5f * k);
            float bx = std::cos(ba) * 79.8f, by = std::sin(ba) * 79.8f;
            float d = std::sqrt((cx - bx) * (cx - bx) + (cy - by) * (cy - by));
            if (d < boltDist) boltDist = d;
        }
        if (boltDist < 5.2f) {
            float lugMask = smoothstep(5.2f, 4.3f, boltDist);
            Vec3 m = brushMetal(nx * 0.35f, ny * 0.35f, 0.93f, r * 0.1f, 0.85f);
            col = over(col, m, lugMask);
            if (boltDist < 2.2f) {
                float slotMask = smoothstep(2.2f, 1.6f, boltDist);
                col = over(col, accent * (0.12f + 0.55f * t), slotMask);
            }
        }
    }

    if (r > 71.0f && r < 82.0f) {
        float ringMask = smoothstep(71.0f, 72.2f, r) * smoothstep(82.0f, 80.8f, r);
        float bevel = 1.0f - std::fabs((r - 76.5f) / 4.5f);
        Vec3 chassis = brushMetal(nx * 0.4f, ny * 0.4f, 0.9f, r * 0.25f, 0.52f + 0.36f * bevel);
        col = over(col, chassis, ringMask);
    }

    // -----------------------------------------------------------------------
    // 2. 270-degree Segmented LED Arc Meter (r = 74.0 .. 79.2, -225 .. +45 deg)
    // -----------------------------------------------------------------------
    if (r >= 73.2f && r <= 80.5f) {
        float relAng = ang - 0.75f * (float)kPi;
        if (relAng < 0.0f) relAng += 2.0f * (float)kPi;
        const float sweep = 1.5f * (float)kPi;
        if (relAng >= 0.0f && relAng <= sweep) {
            float normA = relAng / sweep;
            const float numSegs = 32.0f;
            float segPos = normA * numSegs;
            float segFrac = segPos - std::floor(segPos);
            float segMask = smoothstep(0.14f, 0.24f, segFrac) * smoothstep(0.86f, 0.76f, segFrac);
            float radMask = smoothstep(73.6f, 74.4f, r) * smoothstep(79.5f, 78.7f, r);
            float m = segMask * radMask;
            if (m > 0.01f) {
                float segIndexCenter = (std::floor(segPos) + 0.5f) / numSegs;
                bool lit = (t > 0.015f) && (segIndexCenter <= t + 0.012f);
                if (lit) {
                    float tipDist = std::fabs(segIndexCenter - t);
                    float hot = smoothstep(0.08f, 0.0f, tipDist);
                    Vec3 c = mixv(accent, hotAccent, 0.35f + 0.65f * hot) * (0.95f + 0.65f * t);
                    col = over(col, c, m);
                } else {
                    col = over(col, Vec3{0.07f, 0.09f, 0.12f}, m * 0.85f);
                }
            }
        }
    }

    // -----------------------------------------------------------------------
    // 3. Rotating Mecha Vernier Collar (r = 66.8 .. 72.4)
    // -----------------------------------------------------------------------
    if (r >= 66.0f && r <= 73.0f) {
        float colMask = smoothstep(66.2f, 67.2f, r) * smoothstep(72.8f, 71.8f, r);
        float vang = wrapAnglePi(ang + t * 1.45f);
        float notch12 = std::fmod(std::fabs(vang) * 12.0f / (float)(2.0 * kPi), 1.0f);
        float isTick = smoothstep(0.12f, 0.04f, std::min(notch12, 1.0f - notch12));
        float isCardinal = 0.0f;
        for (int k = 0; k < 4; ++k) {
            float ca = std::fabs(wrapAnglePi(vang - (float)kPi * 0.5f * k));
            if (ca < 0.09f) isCardinal = std::max(isCardinal, smoothstep(0.09f, 0.05f, ca));
        }
        Vec3 collar = brushMetal(nx * 0.6f, ny * 0.6f, 0.8f, r * 0.4f, 0.60f);
        collar = mixv(collar, accent * (0.28f + 1.00f * t), isCardinal * 0.85f);
        collar = mixv(collar, Vec3{0.55f, 0.65f, 0.75f}, isTick * 0.32f * smoothstep(67.5f, 70.0f, r));
        col = over(col, collar, colMask);
    }

    // -----------------------------------------------------------------------
    // 4. Inner Sphere Bay & Bespoke Cyber-Mecha Animation
    // -----------------------------------------------------------------------
    if (r <= coreR + 1.5f) {
        const int row = orbIdx / 4;
        const int colIdx = orbIdx % 4;

        float sphereMask = smoothstep(coreR + 1.0f, coreR - 0.6f, r);
        float zSq = clampf(1.0f - rn * rn, 0.0f, 1.0f);
        float nz = std::sqrt(zSq);

        // Dark bay floor
        col = over(col, Vec3{0.02f, 0.03f, 0.05f}, sphereMask);
        float dormantLum = 0.48f + 0.72f * smoothstep(0.0f, 0.85f, t);

        // ===================================================================
        // ORB 0: EXHAUSTION — 6 Tectonic Obsidian Plates Splitting Apart
        // ===================================================================
        if (orbIdx == 0) {
            float openAmt = smoothstep(0.0f, 1.0f, t);
            float rotAng = ang - openAmt * 0.28f;
            float sectorW = (float)(2.0 * kPi) / 6.0f;
            float sectorIdx = std::floor((rotAng + (float)kPi) / sectorW);
            float sectorCenterAng = -(float)kPi + (sectorIdx + 0.5f) * sectorW;
            float sectorLocalAng = wrapAnglePi(rotAng - sectorCenterAng);

            float shift = 0.16f * openAmt;
            float sx = nx - std::cos(sectorCenterAng) * shift;
            float sy = ny - std::sin(sectorCenterAng) * shift;
            float sr = std::sqrt(sx * sx + sy * sy);

            float seamDist = sr * std::fabs(std::sin(sectorLocalAng));
            float seamHalfW = 0.008f + 0.038f * openAmt;
            float eyeRadius = 0.04f + 0.21f * openAmt;

            float plateMask = smoothstep(seamHalfW - 0.008f, seamHalfW + 0.014f, seamDist)
                            * smoothstep(eyeRadius - 0.015f, eyeRadius + 0.02f, sr)
                            * smoothstep(1.01f, 0.96f, rn);

            // Exposed plasma core inside seams & center eye (kept crisp & dramatic!)
            float plasmaBed = smoothstep(eyeRadius + 0.09f, 0.0f, rn) * (0.25f + 1.05f * openAmt)
                            + smoothstep(seamHalfW + 0.045f, 0.0f, seamDist) * (0.85f * openAmt);
            float filament = 0.65f + 0.35f * std::sin(ang * 12.0f + rn * 20.0f - t * 10.0f);
            Vec3 magma = mixv(accent * 0.75f, hotAccent * 1.25f, clampf(plasmaBed * filament, 0.0f, 1.0f));
            col = over(col, magma, sphereMask * clampf(0.30f + plasmaBed, 0.0f, 1.0f));

            Vec3 tex = sheet.sampleCell(row, colIdx, sx * 0.96f, sy * 0.96f);
            float fissure = clampf((tex.b - tex.r) * 2.2f, 0.0f, 1.0f);
            tex = tex * dormantLum + accent * fissure * (0.90f * openAmt);
            float bevelEdge = smoothstep(seamHalfW + 0.045f, seamHalfW + 0.012f, seamDist)
                            + smoothstep(eyeRadius + 0.050f, eyeRadius + 0.012f, sr);
            tex = tex + accent * bevelEdge * (0.55f * openAmt + 0.12f);
            col = over(col, tex, sphereMask * plateMask);

            if (openAmt > 0.10f && rn < eyeRadius + 0.08f) {
                float arcWave = std::sin(ang * 6.0f + t * 14.0f + rn * 22.0f);
                float arcLine = smoothstep(0.86f, 0.99f, std::fabs(arcWave)) * smoothstep(eyeRadius + 0.03f, 0.02f, rn);
                col = addGlow(col, hotAccent * 1.15f, arcLine * openAmt * 0.80f);
            }
        }
        // ===================================================================
        // ORB 1: CHOKE — 8 Overlapping Sculpted Metallic Iris Shutter Blades
        // ===================================================================
        else if (orbIdx == 1) {
            float openAmt = smoothstep(0.0f, 1.0f, t);
            float tx, ty;
            rot2(nx, ny, -t * 1.45f, tx, ty);
            Vec3 tex = sheet.sampleCell(row, colIdx, tx * mixf(0.95f, 0.86f, openAmt), ty * mixf(0.95f, 0.86f, openAmt));
            float eyeGlow = smoothstep(0.42f, 0.04f, rn);
            tex = tex * (0.55f + 0.80f * openAmt)
                + accent * eyeGlow * (0.35f + 1.10f * openAmt)
                + subAccent * smoothstep(0.24f, 0.06f, rn) * (0.45f + 0.90f * openAmt);
            col = over(col, tex, sphereMask);

            float apertureR = mixf(0.06f, 0.78f, openAmt);
            if (rn > apertureR - 0.03f && rn < 0.99f) {
                const int N = 8;
                float pivotAOffset = mixf(0.22f, 1.14f, openAmt);
                for (int b = 0; b < N; ++b) {
                    float baseA = (float)(2.0 * kPi * b) / N + t * 0.45f;
                    float dAng = wrapAnglePi(ang - baseA);
                    if (dAng < 0.0f) dAng += (float)(2.0 * kPi);
                    const float bladeArc = 1.10f;
                    if (dAng < bladeArc) {
                        float localInnerR = apertureR + (dAng / bladeArc) * 0.50f * pivotAOffset;
                        float radialCov = smoothstep(localInnerR - 0.015f, localInnerR + 0.015f, rn)
                                        * smoothstep(0.995f, 0.94f, rn);
                        float leadSeam = smoothstep(0.0f, 0.03f, dAng);
                        float trailSeam = smoothstep(bladeArc, bladeArc - 0.04f, dAng);
                        float m = radialCov * leadSeam * trailSeam;
                        if (m > 0.02f) {
                            // Sharp spiral seam highlight + cast under-blade shadow so closed shutter looks sculpted!
                            float leadLip = smoothstep(0.09f, 0.02f, dAng);
                            float innerLip = smoothstep(localInnerR + 0.045f, localInnerR + 0.01f, rn);
                            Vec3 bm = brushMetal(nx * 0.7f, ny * 0.7f, 0.75f, (rn * 2.0f - dAng * 0.8f), 0.80f + 0.25f * openAmt);
                            bm = bm + Vec3{0.25f, 0.30f, 0.38f} * leadLip * 0.55f;
                            bm = bm + accent * innerLip * (0.20f + 0.65f * openAmt);

                            // Pivot pin near outer rim of blade
                            float pinX = std::cos(baseA + 0.28f) * 0.84f;
                            float pinY = std::sin(baseA + 0.28f) * 0.84f;
                            float pinD = std::sqrt((nx - pinX) * (nx - pinX) + (ny - pinY) * (ny - pinY));
                            if (pinD < 0.05f) {
                                bm = mixv(bm, accent * (0.35f + 0.75f * openAmt), smoothstep(0.05f, 0.02f, pinD));
                            }
                            col = over(col, bm, sphereMask * m * smoothstep(1.01f, 0.94f, rn));
                        }
                    }
                }
            }
        }
        // ===================================================================
        // ORB 2: INFO DECAY — Exploded Polyhedral Shards + Wireframe HUD
        // ===================================================================
        else if (orbIdx == 2) {
            float openAmt = smoothstep(0.0f, 1.0f, t);
            float sectW = (float)(2.0 * kPi) / 10.0f;
            float sIdx = std::floor((ang + (float)kPi) / sectW);
            int isOdd = ((int)sIdx) & 1;
            float sCen = -(float)kPi + (sIdx + 0.5f) * sectW;
            float sLoc = wrapAnglePi(ang - sCen);

            float radialPush = openAmt * (isOdd ? 0.14f : 0.07f);
            float twist = openAmt * (isOdd ? 0.12f : -0.09f);
            float sx, sy;
            rot2(nx - std::cos(sCen) * radialPush, ny - std::sin(sCen) * radialPush, twist, sx, sy);
            float sr = std::sqrt(sx * sx + sy * sy);

            float scale = mixf(0.86f, 1.05f, openAmt);
            float gapW = 0.008f + 0.030f * openAmt;
            float shardDist = sr * std::fabs(std::sin(sLoc));
            float shardMask = smoothstep(gapW - 0.008f, gapW + 0.014f, shardDist)
                            * smoothstep(1.01f, 0.94f, sr / scale);

            float hexWire = std::fabs(std::sin(nx * 16.0f + ny * 9.0f) * std::cos(ny * 16.0f - nx * 9.0f));
            float wireLine = smoothstep(0.18f, 0.02f, hexWire);
            Vec3 subWire = Vec3{0.02f, 0.05f, 0.08f} + accent * (wireLine * (0.40f + 1.15f * openAmt) + smoothstep(0.40f, 0.0f, rn) * openAmt);
            col = over(col, subWire, sphereMask);

            Vec3 tex = sheet.sampleCell(row, colIdx, sx / scale, sy / scale);
            float edgeGlow = clampf((tex.b - tex.r) * 2.5f, 0.0f, 1.0f);
            tex = tex * (0.50f + 0.78f * openAmt) + accent * edgeGlow * (0.90f * openAmt);
            float splitLip = smoothstep(gapW + 0.04f, gapW + 0.01f, shardDist);
            tex = tex + hotAccent * splitLip * (0.45f * openAmt);
            col = over(col, tex, sphereMask * shardMask);

            if (openAmt > 0.05f) {
                float rx, ry;
                rot2(nx, ny, t * 0.95f, rx, ry);
                float octR = std::max(std::fabs(rx), std::fabs(ry)) * 0.707f + std::min(std::fabs(rx), std::fabs(ry)) * 0.293f;
                float ringR = mixf(0.32f, 0.72f, openAmt);
                float hudLine = smoothstep(0.022f, 0.006f, std::fabs(octR - ringR * 0.707f));
                col = addGlow(col, accent * 1.1f, hudLine * openAmt * 0.80f);
            }
        }
        // ===================================================================
        // ORB 3: CORRUPT MIX — Armored Diagonal Blast-Shields + Cryo Vortex Dome
        // ===================================================================
        else if (orbIdx == 3) {
            float openAmt = smoothstep(0.0f, 1.0f, t);
            float spinAng = -t * 3.8f;
            float zoom = mixf(1.16f, 0.85f, openAmt);
            float vx, vy;
            rot2(nx, ny, spinAng, vx, vy);
            Vec3 vortex = sheet.sampleCell(row, colIdx, vx * zoom, vy * zoom);
            float glow = clampf(vortex.g + vortex.b - vortex.r * 0.5f, 0.0f, 1.4f);
            vortex = vortex * (0.58f + 0.85f * openAmt) + hotAccent * glow * (0.48f * openAmt);
            float rimG = smoothstep(0.74f, 0.98f, rn) * (0.35f + 0.60f * openAmt);
            float lensSpec = smoothstep(0.28f, 0.02f, std::sqrt((nx + 0.30f) * (nx + 0.30f) + (ny + 0.32f) * (ny + 0.32f)));
            vortex = vortex + accent * rimG * 0.55f + Vec3{0.85f, 0.96f, 1.0f} * lensSpec * 0.35f;
            col = over(col, vortex, sphereMask);

            // Stepped zig-zag interlocking diagonal blast shutters sliding open
            float dDiag = (nx - ny) * 0.7071f;
            float alongSeam = (nx + ny) * 0.7071f;
            float zigzag = (std::fabs(alongSeam) < 0.35f) ? 0.07f : 0.0f;
            float signedSeam = dDiag + zigzag * (dDiag >= 0.0f ? -1.0f : 1.0f);
            float shieldGap = mixf(0.022f, 1.05f, openAmt);

            if (std::fabs(dDiag) + (std::fabs(alongSeam) < 0.35f ? 0.06f : 0.0f) > shieldGap) {
                float effD = std::fabs(dDiag) + (std::fabs(alongSeam) < 0.35f ? 0.06f : 0.0f);
                float edgeMask = smoothstep(shieldGap, shieldGap + 0.025f, effD);
                float edgeLip = smoothstep(shieldGap + 0.06f, shieldGap + 0.015f, effD);
                Vec3 shieldMetal = brushMetal(nx * 0.7f, ny * 0.7f, nz * 0.9f, alongSeam * 1.1f, 0.85f);
                // Panel structural ribs
                float rib = smoothstep(0.02f, 0.005f, std::fabs(std::fabs(alongSeam) - 0.42f));
                shieldMetal = mixv(shieldMetal, Vec3{0.02f, 0.03f, 0.045f}, rib * 0.75f);
                shieldMetal = shieldMetal + accent * edgeLip * (0.45f + 0.65f * openAmt);
                col = over(col, shieldMetal, sphereMask * edgeMask);
            }
        }
        // ===================================================================
        // ORB 4: KINETIC MOD — Twisted Chrome Wave + Depth-Sorted 3D Gimbals
        // ===================================================================
        else if (orbIdx == 4) {
            float openAmt = smoothstep(0.0f, 1.0f, t);

            for (int g = 0; g < 2; ++g) {
                float tilt = mixf(0.04f, (g == 0 ? 0.52f : -0.58f), openAmt);
                float roll = (g == 0 ? 0.42f : -0.45f) + t * (g == 0 ? 0.85f : -1.05f);
                float rx, ry;
                rot2(nx, ny, -roll, rx, ry);
                float squish = std::max(0.14f, std::fabs(std::sin(tilt)));
                float ey = ry / squish;
                float er = std::sqrt(rx * rx + ey * ey);
                float ringRad = (g == 0) ? 0.92f : 0.80f;
                if (ry < 0.0f) {
                    float rm = smoothstep(0.045f, 0.012f, std::fabs(er - ringRad)) * smoothstep(1.04f, 0.98f, rn);
                    col = over(col, mixv(Vec3{0.18f, 0.14f, 0.22f}, accent, 0.55f * openAmt), rm * (0.45f + 0.55f * openAmt));
                }
            }

            float sphereR = mixf(0.92f, 0.78f, openAmt);
            float coreMask = smoothstep(sphereR + 0.015f, sphereR - 0.015f, rn);
            float waveShift = 0.06f * openAmt * std::sin(ny * 7.5f - t * 6.0f);
            float sx, sy;
            rot2((nx + waveShift) / sphereR, ny / sphereR, t * 0.9f, sx, sy);
            Vec3 tex = sheet.sampleCell(row, colIdx, sx, sy);
            float magBoost = clampf((tex.r - tex.g) * 1.8f, 0.0f, 1.0f);
            tex = tex * (0.50f + 0.80f * openAmt) + accent * magBoost * (0.85f * openAmt);
            col = over(col, tex, sphereMask * coreMask);

            for (int g = 0; g < 2; ++g) {
                float tilt = mixf(0.04f, (g == 0 ? 0.52f : -0.58f), openAmt);
                float roll = (g == 0 ? 0.42f : -0.45f) + t * (g == 0 ? 0.85f : -1.05f);
                float rx, ry;
                rot2(nx, ny, -roll, rx, ry);
                float squish = std::max(0.14f, std::fabs(std::sin(tilt)));
                float ey = ry / squish;
                float er = std::sqrt(rx * rx + ey * ey);
                float ringRad = (g == 0) ? 0.92f : 0.80f;
                if (ry >= 0.0f && openAmt > 0.03f) {
                    float dR = std::fabs(er - ringRad);
                    float rm = smoothstep(0.048f, 0.014f, dR) * smoothstep(1.04f, 0.97f, rn);
                    Vec3 gimbal = mixv(Vec3{0.45f, 0.48f, 0.55f}, hotAccent * 1.25f, smoothstep(0.045f, 0.0f, dR) * openAmt);
                    col = over(col, gimbal, rm * smoothstep(0.02f, 0.25f, openAmt));
                    col = addGlow(col, accent * 0.85f, smoothstep(0.09f, 0.0f, dR) * openAmt * 0.50f);
                }
            }
        }
        // ===================================================================
        // ORB 5: RESONATOR — Telescoping Harmonic Piston Rings + Acoustic Flare
        // ===================================================================
        else if (orbIdx == 5) {
            float openAmt = smoothstep(0.0f, 1.0f, t);
            float ringWave = 0.045f * openAmt * std::sin(rn * 22.0f - t * 8.0f);
            float warpScale = mixf(1.12f, 0.88f, openAmt) + ringWave;
            float rx, ry;
            rot2(nx * warpScale, ny * warpScale, t * 0.45f, rx, ry);
            Vec3 tex = sheet.sampleCell(row, colIdx, rx, ry);

            float slitMod = 0.5f + 0.5f * std::cos((rn - 0.06f * openAmt) * 28.0f);
            float activeZone = smoothstep(0.15f + 0.95f * openAmt, 0.0f, rn);
            float magF = clampf((tex.r - tex.g) * 1.8f, 0.0f, 1.0f);
            tex = tex * (0.42f + 0.78f * openAmt) + accent * magF * activeZone * (1.15f * openAmt);
            float tierEdge = smoothstep(0.85f, 1.0f, std::fabs(std::sin(rn * 15.708f)));
            tex = tex + hotAccent * tierEdge * activeZone * (0.65f * openAmt) * slitMod;
            float centerCore = smoothstep(mixf(0.10f, 0.26f, openAmt), 0.0f, rn);
            tex = mixv(tex, hotAccent * 1.35f, centerCore * openAmt);
            col = over(col, tex, sphereMask);

            if (openAmt > 0.05f && rn > 0.76f && rn < 0.99f) {
                float spoke = std::fmod((ang + (float)kPi) * 24.0f / (float)(2.0 * kPi), 1.0f);
                float spokeMask = smoothstep(0.30f, 0.14f, std::min(spoke, 1.0f - spoke));
                float barAmp = 0.78f + 0.20f * openAmt * (0.5f + 0.5f * std::sin(ang * 6.0f + t * 10.0f));
                float barMask = smoothstep(barAmp, barAmp - 0.02f, rn) * smoothstep(0.76f, 0.80f, rn);
                col = addGlow(col, accent * 1.1f, spokeMask * barMask * openAmt * 0.85f);
            }
        }
        // ===================================================================
        // ORB 6: GATE CARVE — Chamfered 4-Quadrant Hatch + 16-Tooth Escapement Gear
        // ===================================================================
        else if (orbIdx == 6) {
            float openAmt = smoothstep(0.0f, 1.0f, t);

            float gx, gy;
            rot2(nx, ny, t * 2.15f, gx, gy);
            Vec3 tex = sheet.sampleCell(row, colIdx, gx * 0.94f, gy * 0.94f);
            float magGear = clampf((tex.r - tex.g) * 2.0f, 0.0f, 1.0f);
            tex = tex * (0.50f + 0.80f * openAmt) + accent * magGear * (1.05f * openAmt);

            if (rn < 0.54f) {
                float gearAng = std::atan2(gy, gx);
                float teeth = std::cos(gearAng * 16.0f) * 0.048f;
                float gearOuter = 0.44f + teeth;
                float gm = smoothstep(gearOuter + 0.015f, gearOuter - 0.015f, rn)
                         * smoothstep(0.14f, 0.18f, rn);
                // Etched radial gear spokes
                float spoke = std::fabs(std::sin(gearAng * 4.0f));
                Vec3 gCol = brushMetal(gx, gy, 0.85f, rn * 2.2f, 0.88f + 0.28f * openAmt);
                gCol = mixv(gCol, Vec3{0.05f, 0.04f, 0.07f}, smoothstep(0.35f, 0.12f, spoke) * smoothstep(0.35f, 0.22f, rn) * 0.65f);
                tex = mixv(tex, gCol, gm * (0.55f + 0.45f * openAmt));
                float hubGlow = smoothstep(0.20f, 0.0f, rn);
                tex = mixv(tex, hotAccent * 1.25f, hubGlow * openAmt);
            }
            col = over(col, tex, sphereMask);

            // Chamfered 4-Quadrant blast doors (bevelled octagonal inner aperture as they retract!)
            float gap = mixf(0.018f, 0.44f, openAmt);
            float ax = std::fabs(nx), ay = std::fabs(ny);
            float chamfer = ax + ay - (gap * 2.0f + 0.18f);
            if (ax > gap && ay > gap && chamfer > 0.0f) {
                float doorEdge = std::min(std::min(ax - gap, ay - gap), chamfer);
                float doorMask = smoothstep(0.0f, 0.025f, doorEdge) * smoothstep(1.01f, 0.95f, rn);
                float lip = smoothstep(0.055f, 0.015f, doorEdge);
                Vec3 hatch = brushMetal(nx * 0.65f, ny * 0.65f, nz * 0.9f, (ax + ay), 0.78f);
                hatch = hatch + accent * lip * (0.35f + 0.75f * openAmt);
                col = over(col, hatch, sphereMask * doorMask);
            }

            if (rn > 0.82f && rn < 0.98f && openAmt > 0.04f) {
                float stepA = ang + 0.5f * (float)kPi;
                if (stepA < 0.0f) stepA += 2.0f * (float)kPi;
                float sNorm = stepA / (2.0f * (float)kPi);
                float sPos = sNorm * 16.0f;
                float sFrac = sPos - std::floor(sPos);
                float sWin = smoothstep(0.18f, 0.28f, sFrac) * smoothstep(0.82f, 0.72f, sFrac)
                           * smoothstep(0.84f, 0.87f, rn) * smoothstep(0.96f, 0.93f, rn);
                if (std::floor(sPos) / 16.0f <= openAmt) {
                    col = over(col, hotAccent * 1.25f, sWin * 0.95f);
                }
            }
        }
        // ===================================================================
        // ORB 7: SMASH — Interlocking Hydraulic Crusher Jaws & Chrome Pistons
        // ===================================================================
        else {
            float openAmt = smoothstep(0.0f, 1.0f, t);

            float rx, ry;
            rot2(nx, ny, t * 0.55f, rx, ry);
            float compress = mixf(1.04f, 0.93f, openAmt);
            Vec3 tex = sheet.sampleCell(row, colIdx, rx * compress, ry * compress);
            float crackF = clampf((tex.r - tex.g) * 1.85f, 0.0f, 1.0f);
            tex = tex * (0.48f + 0.85f * openAmt) + hotAccent * crackF * (1.25f * openAmt);
            float shockR = 0.25f + 0.65f * openAmt;
            float shock = smoothstep(0.055f, 0.0f, std::fabs(rn - shockR)) * openAmt;
            tex = tex + hotAccent * shock * 0.95f;
            col = over(col, tex, sphereMask);

            float ay = std::fabs(ny), ax = std::fabs(nx);
            float jawGap = mixf(0.035f, 0.86f, openAmt);
            float jawDepth = 0.52f;
            float serration = (ax < 0.30f) ? (0.045f * std::cos(nx * 24.0f)) : 0.0f;
            float localGap = jawGap + serration;

            // Dual chrome hydraulic push-rods in outer housing tracks behind/outside the clamping jaws
            if (ay > localGap + jawDepth * 0.65f && rn < 0.99f) {
                float rodX = std::fabs(ax - 0.36f);
                if (rodX < 0.075f) {
                    float rodMask = smoothstep(0.075f, 0.050f, rodX);
                    float cylZ = std::sqrt(clampf(1.0f - (rodX / 0.075f) * (rodX / 0.075f), 0.0f, 1.0f));
                    Vec3 chrome = brushMetal(0.25f, -0.30f, cylZ, ny * 2.2f, 1.30f);
                    col = over(col, chrome, sphereMask * rodMask);
                }
            }

            // Interlocking serrated hydraulic crusher anvil blocks retracting cleanly toward outer bay
            if (ay > localGap && ay < localGap + jawDepth && ax < 0.80f) {
                float jawMask = smoothstep(localGap, localGap + 0.025f, ay)
                              * smoothstep(localGap + jawDepth, localGap + jawDepth - 0.03f, ay)
                              * smoothstep(0.80f, 0.72f, ax)
                              * smoothstep(1.01f, 0.95f, rn);
                float lip = smoothstep(localGap + 0.065f, localGap + 0.015f, ay);
                Vec3 anvil = brushMetal(nx * 0.45f, ny * 0.75f, nz * 0.85f, nx * 1.4f, 0.78f);
                anvil = anvil + accent * lip * (0.45f + 0.82f * openAmt);
                col = over(col, anvil, sphereMask * jawMask);
            }
        }
    }

    // -----------------------------------------------------------------------
    // 5. Inner Bezel Rim Lip
    // -----------------------------------------------------------------------
    if (r > coreR - 1.5f && r < coreR + 2.2f) {
        float lip = smoothstep(1.8f, 0.0f, std::fabs(r - coreR));
        Vec3 rimCol = mixv(Vec3{0.25f, 0.30f, 0.38f}, accent, 0.25f + 0.55f * t);
        col = over(col, rimCol, lip * 0.65f);
    }

    return {
        clampf(col.r, 0.0f, 1.0f),
        clampf(col.g, 0.0f, 1.0f),
        clampf(col.b, 0.0f, 1.0f),
        clampf(col.a, 0.0f, 1.0f)
    };
}

} // namespace

int main(int argc, char** argv)
{
    if (argc < 5) {
        std::fprintf(stderr, "Usage: gen_orbs <orbs_raw_bgra> <width> <height> <out_dir>\n");
        return 1;
    }
    const char* rawPath = argv[1];
    SheetImage sheet;
    sheet.width = std::atoi(argv[2]);
    sheet.height = std::atoi(argv[3]);
    const std::string outDir = argv[4];

    const size_t expected = (size_t)sheet.width * (size_t)sheet.height * 4;
    sheet.bgra.resize(expected);
    {
        std::ifstream f(rawPath, std::ios::binary);
        if (!f.read(reinterpret_cast<char*>(sheet.bgra.data()), (std::streamsize)expected)) {
            std::fprintf(stderr, "Failed to read raw image %s\n", rawPath);
            return 1;
        }
    }

    const int stripH = kOutSize * kNumFrames;
    std::vector<uint8_t> outBgra((size_t)kOutSize * (size_t)stripH * 4);

    for (int orb = 0; orb < 8; ++orb) {
        for (int frame = 0; frame < kNumFrames; ++frame) {
            const float t = float(frame) / float(kNumFrames - 1);
            for (int y = 0; y < kOutSize; ++y) {
                for (int x = 0; x < kOutSize; ++x) {
                    float sr = 0, sg = 0, sb = 0, sa = 0;
                    for (int sy = 0; sy < kSsaa; ++sy) {
                        for (int sx = 0; sx < kSsaa; ++sx) {
                            const float px = float(x) + (float(sx) + 0.5f) / float(kSsaa);
                            const float py = float(y) + (float(sy) + 0.5f) / float(kSsaa);
                            const Rgba c = renderPixel(sheet, orb, t, px, py);
                            sr += c.r * c.a;
                            sg += c.g * c.a;
                            sb += c.b * c.a;
                            sa += c.a;
                        }
                    }
                    const float invN = 1.0f / float(kSsaa * kSsaa);
                    const float a = sa * invN;
                    float r = 0, g = 0, b = 0;
                    if (a > 1e-4f) {
                        r = clampf((sr * invN) / a, 0.0f, 1.0f);
                        g = clampf((sg * invN) / a, 0.0f, 1.0f);
                        b = clampf((sb * invN) / a, 0.0f, 1.0f);
                    }
                    const size_t outY = (size_t)frame * (size_t)kOutSize + (size_t)y;
                    const size_t idx = (outY * (size_t)kOutSize + (size_t)x) * 4;
                    outBgra[idx + 0] = (uint8_t)std::lround(b * 255.0f);
                    outBgra[idx + 1] = (uint8_t)std::lround(g * 255.0f);
                    outBgra[idx + 2] = (uint8_t)std::lround(r * 255.0f);
                    outBgra[idx + 3] = (uint8_t)std::lround(a * 255.0f);
                }
            }
        }
        const std::string outPath = outDir + "/orb_strip_" + std::to_string(orb) + ".bgra";
        std::ofstream of(outPath, std::ios::binary);
        of.write(reinterpret_cast<const char*>(outBgra.data()), (std::streamsize)outBgra.size());
        std::printf("Generated orb_strip_%d (%dx%d, %d frames)\n", orb, kOutSize, stripH, kNumFrames);
    }

    return 0;
}
