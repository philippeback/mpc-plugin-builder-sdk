// gen_orbs.cpp - Generates 22-frame organic + cyber-mecha unfolding FilmStrips (180x3960 RGBA)
// for Mushin's 8 MAIN macro orbs: larger 76.5px sculpted spheres resting inside a soft
// concave recessed socket with a clean 270-deg glowing halo arc and bespoke 3D mechanical
// unfolding (fracturing basalt plates, iris aperture, crystal burst, parting dome shields,
// liquid chrome gimbals, telescoping resonance rings, retracting hatches + gear, anvil clamps).

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
constexpr int kSsaa = 2;
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
        clampf(dst.r * (dst.a / std::max(outA, 1e-4f)) + glowRgb.r * (b / std::max(outA, 1e-4f)), 0.0f, 1.35f),
        clampf(dst.g * (dst.a / std::max(outA, 1e-4f)) + glowRgb.g * (b / std::max(outA, 1e-4f)), 0.0f, 1.35f),
        clampf(dst.b * (dst.a / std::max(outA, 1e-4f)) + glowRgb.b * (b / std::max(outA, 1e-4f)), 0.0f, 1.35f),
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

// Rich organic satin gunmetal shading (warm studio key light + subtle brushing)
static inline Vec3 satinMetal(float x, float y, float z, float grainCoord, float darkness = 1.0f)
{
    float invL = 1.0f / std::sqrt(0.50f * 0.50f + 0.65f * 0.65f + 0.57f * 0.57f);
    float lx = -0.50f * invL, ly = -0.65f * invL, lz = 0.57f * invL;
    float ndotl = std::max(0.0f, x * lx + y * ly + z * lz);

    float grain = 0.94f + 0.06f * std::sin(grainCoord * 48.0f);
    float hx = lx, hy = ly, hz = lz + 1.0f;
    float hLen = std::sqrt(hx * hx + hy * hy + hz * hz);
    float ndoth = std::max(0.0f, (x * hx + y * hy + z * hz) / hLen);
    float spec = std::pow(ndoth, 24.0f) * 0.42f;

    Vec3 base = Vec3{0.075f, 0.095f, 0.125f} * darkness * grain;
    Vec3 diff = Vec3{0.24f, 0.27f, 0.32f} * ndotl * darkness * grain;
    Vec3 rim  = Vec3{0.07f, 0.09f, 0.13f} * smoothstep(0.50f, 0.92f, 1.0f - z);
    return base + diff + rim + Vec3{spec, spec * 1.02f, spec * 1.06f};
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
    const Vec3 hotAccent = isCyan ? Vec3{0.58f, 0.97f, 1.00f} : Vec3{1.00f, 0.66f, 0.84f};
    const Vec3 subAccent = isCyan ? Vec3{0.72f, 0.25f, 0.95f} : Vec3{0.25f, 0.85f, 1.00f};

    const float cx = px - 90.0f;
    const float cy = py - 90.0f;
    const float r = std::sqrt(cx * cx + cy * cy);
    const float ang = std::atan2(cy, cx);

    // Larger organic sphere core + surrounding dark concave studio recess (76.5 -> 89 px)
    const float coreR = 76.5f;
    const float rn = r / coreR;
    const float nx = cx / coreR;
    const float ny = cy / coreR;

    Rgba col = {0.0f, 0.0f, 0.0f, 0.0f};

    // -----------------------------------------------------------------------
    // 1. Soft Concave Studio Recess & Ambient Shadow Pocket (r = 74..89)
    // -----------------------------------------------------------------------
    if (r < 89.5f) {
        // Concave well shadow fading organically into surrounding panel bg
        float wellAlpha = smoothstep(89.5f, 79.0f, r) * 0.78f;
        // Bottom-right drop shadow subtly heavier for depth
        float directional = 0.85f + 0.15f * clampf((nx + ny) * 0.7f, -1.0f, 1.0f);
        Vec3 wellCol = mixv(Vec3{0.015f, 0.020f, 0.030f}, accent * 0.09f, (0.15f + 0.85f * t) * smoothstep(86.0f, 75.5f, r));
        col = over(col, wellCol, wellAlpha * directional);

        // Subtle satin concave bezel lip at r ~ 78.5..80.2
        float lipRing = smoothstep(2.2f, 0.0f, std::fabs(r - 79.2f));
        Vec3 lipShade = mixv(Vec3{0.10f, 0.13f, 0.17f}, Vec3{0.20f, 0.25f, 0.32f}, smoothstep(0.8f, -0.8f, nx + ny));
        col = over(col, lipShade, lipRing * 0.55f);
    }

    // -----------------------------------------------------------------------
    // 2. Smooth Minimalist 270-Degree Glowing Halo Arc (r = 81.5 .. 84.5)
    // -----------------------------------------------------------------------
    if (r >= 78.5f && r <= 88.5f) {
        float relAng = ang - 0.75f * (float)kPi;
        if (relAng < 0.0f) relAng += 2.0f * (float)kPi;
        const float sweep = 1.5f * (float)kPi;
        if (relAng <= sweep) {
            float normA = relAng / sweep;
            // Subtle dim groove for full 270 deg
            float groove = smoothstep(2.0f, 0.4f, std::fabs(r - 82.8f));
            col = over(col, Vec3{0.04f, 0.055f, 0.08f}, groove * 0.55f);

            if (t > 0.012f && normA <= t + 0.01f) {
                float tipFade = smoothstep(t + 0.01f, t - 0.02f, normA);
                float coreArc = smoothstep(1.8f, 0.2f, std::fabs(r - 82.8f)) * tipFade;
                float haloArc = smoothstep(5.5f, 0.0f, std::fabs(r - 82.8f)) * tipFade * 0.34f * t;
                Vec3 arcC = mixv(accent, hotAccent, 0.25f + 0.75f * smoothstep(0.15f, 0.0f, t - normA));
                col = over(col, arcC * (0.70f + 0.55f * t), coreArc * (0.45f + 0.55f * t));
                col = addGlow(col, accent * 0.95f, haloArc);
            }
        }
    }

    // -----------------------------------------------------------------------
    // 3. Sculpted Organic Sphere + Cyber-Mecha Unfolding (r <= coreR)
    // -----------------------------------------------------------------------
    if (r <= coreR + 1.0f) {
        const int row = orbIdx / 4;
        const int colIdx = orbIdx % 4;

        float sphereMask = smoothstep(coreR + 0.8f, coreR - 0.8f, r);
        float zSq = clampf(1.0f - rn * rn, 0.0f, 1.0f);
        float nz = std::sqrt(zSq);
        // Organic edge AO darkening around sphere rim so it settles naturally inside the pocket
        float sphereRimAO = mixf(0.45f, 1.0f, smoothstep(1.0f, 0.72f, rn));

        // Deep glowing interior cavity floor
        col = over(col, Vec3{0.02f, 0.028f, 0.042f}, sphereMask);
        float baseLum = (0.48f + 0.68f * smoothstep(0.0f, 0.85f, t)) * sphereRimAO;

        // ===================================================================
        // ORB 0: EXHAUSTION — Organic Tectonic Basalt Chunks Separating + Plasma Magma
        // ===================================================================
        if (orbIdx == 0) {
            float openAmt = smoothstep(0.0f, 1.0f, t);
            // Curved organic seam modulation so fractures feel geological, not pie-sliced
            float warpAng = ang - openAmt * 0.22f + 0.14f * std::sin(rn * 5.5f + ang * 2.0f);
            float sectorW = (float)(2.0 * kPi) / 6.0f;
            float sectorIdx = std::floor((warpAng + (float)kPi) / sectorW);
            float sectorCen = -(float)kPi + (sectorIdx + 0.5f) * sectorW;
            float sectorLoc = wrapAnglePi(warpAng - sectorCen);

            float shift = 0.145f * openAmt;
            float sx = nx - std::cos(sectorCen) * shift;
            float sy = ny - std::sin(sectorCen) * shift;
            float sr = std::sqrt(sx * sx + sy * sy);

            float seamDist = sr * std::fabs(std::sin(sectorLoc));
            float seamHalfW = 0.004f + 0.032f * openAmt;
            float eyeRadius = 0.02f + 0.19f * openAmt;

            float plateMask = smoothstep(seamHalfW - 0.006f, seamHalfW + 0.015f, seamDist)
                            * smoothstep(eyeRadius - 0.015f, eyeRadius + 0.025f, sr)
                            * smoothstep(1.01f, 0.95f, rn);

            // Soft glowing bioluminescent cyan magma deep inside widening fissures
            float magmaBed = smoothstep(eyeRadius + 0.10f, 0.0f, rn) * (0.20f + 0.95f * openAmt)
                           + smoothstep(seamHalfW + 0.055f, 0.0f, seamDist) * (0.85f * openAmt);
            float nebulaFlow = 0.70f + 0.30f * std::sin(ang * 5.0f + rn * 12.0f - t * 6.0f);
            Vec3 magmaCol = mixv(accent * 0.65f, hotAccent * 1.15f, clampf(magmaBed * nebulaFlow, 0.0f, 1.0f));
            col = over(col, magmaCol, sphereMask * clampf(0.25f + magmaBed, 0.0f, 1.0f));

            Vec3 tex = sheet.sampleCell(row, colIdx, sx * 0.96f, sy * 0.96f);
            float fissure = clampf((tex.b - tex.r) * 2.2f, 0.0f, 1.0f);
            tex = tex * baseLum + accent * fissure * (0.95f * openAmt);
            float bevelEdge = smoothstep(seamHalfW + 0.045f, seamHalfW + 0.010f, seamDist)
                            + smoothstep(eyeRadius + 0.048f, eyeRadius + 0.010f, sr);
            tex = tex + accent * bevelEdge * (0.48f * openAmt + 0.08f);
            col = over(col, tex, sphereMask * plateMask);
        }
        // ===================================================================
        // ORB 1: CHOKE — 8 Sculpted Brushed-Titanium Iris Shutter Blades Opening
        // ===================================================================
        else if (orbIdx == 1) {
            float openAmt = smoothstep(0.0f, 1.0f, t);
            float tx, ty;
            rot2(nx, ny, -t * 1.45f, tx, ty);
            Vec3 tex = sheet.sampleCell(row, colIdx, tx * mixf(0.95f, 0.88f, openAmt), ty * mixf(0.95f, 0.88f, openAmt));
            float eyeGlow = smoothstep(0.45f, 0.04f, rn);
            tex = tex * ((0.55f + 0.78f * openAmt) * sphereRimAO)
                + accent * eyeGlow * (0.30f + 1.05f * openAmt)
                + subAccent * smoothstep(0.24f, 0.06f, rn) * (0.40f + 0.85f * openAmt);
            col = over(col, tex, sphereMask);

            float apertureR = mixf(0.05f, 0.80f, openAmt);
            if (rn > apertureR - 0.03f && rn < 0.995f) {
                const int N = 8;
                float pivotAOffset = mixf(0.20f, 1.15f, openAmt);
                for (int b = 0; b < N; ++b) {
                    float baseA = (float)(2.0 * kPi * b) / N + t * 0.45f;
                    float dAng = wrapAnglePi(ang - baseA);
                    if (dAng < 0.0f) dAng += (float)(2.0 * kPi);
                    const float bladeArc = 1.10f;
                    if (dAng < bladeArc) {
                        float localInnerR = apertureR + (dAng / bladeArc) * 0.50f * pivotAOffset;
                        float radialCov = smoothstep(localInnerR - 0.015f, localInnerR + 0.015f, rn)
                                        * smoothstep(0.995f, 0.94f, rn);
                        float leadSeam = smoothstep(0.0f, 0.025f, dAng);
                        float trailSeam = smoothstep(bladeArc, bladeArc - 0.035f, dAng);
                        float m = radialCov * leadSeam * trailSeam;
                        if (m > 0.02f) {
                            float leadLip = smoothstep(0.08f, 0.015f, dAng);
                            float innerLip = smoothstep(localInnerR + 0.042f, localInnerR + 0.008f, rn);
                            Vec3 bm = satinMetal(nx * 0.75f, ny * 0.75f, nz * 0.85f, (rn * 1.8f - dAng * 0.7f), (0.85f + 0.25f * openAmt) * sphereRimAO);
                            bm = bm + Vec3{0.18f, 0.22f, 0.28f} * leadLip * 0.55f;
                            bm = bm + accent * innerLip * (0.16f + 0.58f * openAmt);
                            col = over(col, bm, sphereMask * m * smoothstep(1.01f, 0.94f, rn));
                        }
                    }
                }
            }
        }
        // ===================================================================
        // ORB 2: INFO DECAY — Exploded Crystal Polyhedron Unfolding
        // ===================================================================
        else if (orbIdx == 2) {
            float openAmt = smoothstep(0.0f, 1.0f, t);
            float sectW = (float)(2.0 * kPi) / 10.0f;
            float sIdx = std::floor((ang + (float)kPi) / sectW);
            int isOdd = ((int)sIdx) & 1;
            float sCen = -(float)kPi + (sIdx + 0.5f) * sectW;
            float sLoc = wrapAnglePi(ang - sCen);

            float radialPush = openAmt * (isOdd ? 0.135f : 0.065f);
            float twist = openAmt * (isOdd ? 0.11f : -0.08f);
            float sx, sy;
            rot2(nx - std::cos(sCen) * radialPush, ny - std::sin(sCen) * radialPush, twist, sx, sy);
            float sr = std::sqrt(sx * sx + sy * sy);

            float scale = mixf(0.88f, 1.03f, openAmt);
            float gapW = 0.004f + 0.026f * openAmt;
            float shardDist = sr * std::fabs(std::sin(sLoc));
            float shardMask = smoothstep(gapW - 0.008f, gapW + 0.014f, shardDist)
                            * smoothstep(1.01f, 0.94f, sr / scale);

            // Deep cyan plasma core exposed between exploded obsidian facets (no noisy grid)
            float coreGlow = smoothstep(0.75f, 0.0f, rn) * (0.20f + 0.95f * openAmt);
            Vec3 coreBed = mixv(Vec3{0.02f, 0.04f, 0.06f}, hotAccent * 1.15f, coreGlow);
            col = over(col, coreBed, sphereMask);

            Vec3 tex = sheet.sampleCell(row, colIdx, sx / scale, sy / scale);
            float edgeGlow = clampf((tex.b - tex.r) * 2.4f, 0.0f, 1.0f);
            tex = tex * ((0.50f + 0.78f * openAmt) * sphereRimAO) + accent * edgeGlow * (0.95f * openAmt);
            float splitLip = smoothstep(gapW + 0.040f, gapW + 0.008f, shardDist);
            tex = tex + hotAccent * splitLip * (0.42f * openAmt);
            col = over(col, tex, sphereMask * shardMask);
        }
        // ===================================================================
        // ORB 3: CORRUPT MIX — Curved Satin Blast Shields Parting Over Cryo Vortex
        // ===================================================================
        else if (orbIdx == 3) {
            float openAmt = smoothstep(0.0f, 1.0f, t);
            float spinAng = -t * 3.8f;
            float zoom = mixf(1.14f, 0.86f, openAmt);
            float vx, vy;
            rot2(nx, ny, spinAng, vx, vy);
            Vec3 vortex = sheet.sampleCell(row, colIdx, vx * zoom, vy * zoom);
            float glow = clampf(vortex.g + vortex.b - vortex.r * 0.5f, 0.0f, 1.3f);
            vortex = vortex * ((0.60f + 0.85f * openAmt) * sphereRimAO) + hotAccent * glow * (0.42f * openAmt);
            float rimG = smoothstep(0.76f, 0.98f, rn) * (0.30f + 0.55f * openAmt);
            float lensSpec = smoothstep(0.30f, 0.02f, std::sqrt((nx + 0.30f) * (nx + 0.30f) + (ny + 0.32f) * (ny + 0.32f)));
            vortex = vortex + accent * rimG * 0.45f + Vec3{0.85f, 0.96f, 1.0f} * lensSpec * 0.30f;
            col = over(col, vortex, sphereMask);

            // Smooth stepped interlocking blast-shields sliding diagonally apart
            float dDiag = (nx - ny) * 0.7071f;
            float alongSeam = (nx + ny) * 0.7071f;
            float stepOffset = (std::fabs(alongSeam) < 0.32f) ? 0.055f : 0.0f;
            float shieldGap = mixf(0.015f, 1.06f, openAmt);
            float effD = std::fabs(dDiag) + stepOffset;
            if (effD > shieldGap) {
                float edgeMask = smoothstep(shieldGap, shieldGap + 0.025f, effD);
                float edgeLip = smoothstep(shieldGap + 0.055f, shieldGap + 0.012f, effD);
                Vec3 shieldMetal = satinMetal(nx * 0.7f, ny * 0.7f, nz * 0.9f, alongSeam * 0.8f, 0.88f * sphereRimAO);
                shieldMetal = shieldMetal + accent * edgeLip * (0.35f + 0.60f * openAmt);
                col = over(col, shieldMetal, sphereMask * edgeMask);
            }
        }
        // ===================================================================
        // ORB 4: KINETIC MOD — Twisting Liquid Chrome + Clean 3D Gimbal Halos
        // ===================================================================
        else if (orbIdx == 4) {
            float openAmt = smoothstep(0.0f, 1.0f, t);

            // Back-half of 2 smooth depth-sorted 3D orbital gyroscope rings
            for (int g = 0; g < 2; ++g) {
                float tilt = mixf(0.04f, (g == 0 ? 0.50f : -0.56f), openAmt);
                float roll = (g == 0 ? 0.40f : -0.44f) + t * (g == 0 ? 0.85f : -1.05f);
                float rx, ry;
                rot2(nx, ny, -roll, rx, ry);
                float squish = std::max(0.14f, std::fabs(std::sin(tilt)));
                float ey = ry / squish;
                float er = std::sqrt(rx * rx + ey * ey);
                float ringRad = (g == 0) ? 0.94f : 0.83f;
                if (ry < 0.0f) {
                    float rm = smoothstep(0.040f, 0.010f, std::fabs(er - ringRad)) * smoothstep(1.03f, 0.97f, rn);
                    col = over(col, mixv(Vec3{0.18f, 0.14f, 0.22f}, accent, 0.55f * openAmt), rm * (0.40f + 0.55f * openAmt));
                }
            }

            float sphereR = mixf(0.95f, 0.81f, openAmt);
            float coreMask = smoothstep(sphereR + 0.015f, sphereR - 0.015f, rn);
            float waveShift = 0.055f * openAmt * std::sin(ny * 7.5f - t * 6.0f);
            float sx, sy;
            rot2((nx + waveShift) / sphereR, ny / sphereR, t * 0.9f, sx, sy);
            Vec3 tex = sheet.sampleCell(row, colIdx, sx, sy);
            float magBoost = clampf((tex.r - tex.g) * 1.8f, 0.0f, 1.0f);
            tex = tex * ((0.50f + 0.78f * openAmt) * sphereRimAO) + accent * magBoost * (0.85f * openAmt);
            col = over(col, tex, sphereMask * coreMask);

            for (int g = 0; g < 2; ++g) {
                float tilt = mixf(0.04f, (g == 0 ? 0.50f : -0.56f), openAmt);
                float roll = (g == 0 ? 0.40f : -0.44f) + t * (g == 0 ? 0.85f : -1.05f);
                float rx, ry;
                rot2(nx, ny, -roll, rx, ry);
                float squish = std::max(0.14f, std::fabs(std::sin(tilt)));
                float ey = ry / squish;
                float er = std::sqrt(rx * rx + ey * ey);
                float ringRad = (g == 0) ? 0.94f : 0.83f;
                if (ry >= 0.0f && openAmt > 0.03f) {
                    float dR = std::fabs(er - ringRad);
                    float rm = smoothstep(0.042f, 0.012f, dR) * smoothstep(1.03f, 0.97f, rn);
                    Vec3 gimbal = mixv(Vec3{0.42f, 0.46f, 0.54f}, hotAccent * 1.18f, smoothstep(0.040f, 0.0f, dR) * openAmt);
                    col = over(col, gimbal, rm * smoothstep(0.02f, 0.25f, openAmt));
                    col = addGlow(col, accent * 0.80f, smoothstep(0.085f, 0.0f, dR) * openAmt * 0.42f);
                }
            }
        }
        // ===================================================================
        // ORB 5: RESONATOR — Telescoping Concentric Acoustic Coils (Smooth Organic)
        // ===================================================================
        else if (orbIdx == 5) {
            float openAmt = smoothstep(0.0f, 1.0f, t);
            float ringWave = 0.042f * openAmt * std::sin(rn * 22.0f - t * 8.0f);
            float warpScale = mixf(1.08f, 0.88f, openAmt) + ringWave;
            float rx, ry;
            rot2(nx * warpScale, ny * warpScale, t * 0.42f, rx, ry);
            Vec3 tex = sheet.sampleCell(row, colIdx, rx, ry);

            float slitMod = 0.55f + 0.45f * std::cos((rn - 0.06f * openAmt) * 28.0f);
            float activeZone = smoothstep(0.15f + 0.95f * openAmt, 0.0f, rn);
            float magF = clampf((tex.r - tex.g) * 1.8f, 0.0f, 1.0f);
            tex = tex * ((0.45f + 0.78f * openAmt) * sphereRimAO) + accent * magF * activeZone * (1.12f * openAmt);
            float tierEdge = smoothstep(0.86f, 1.0f, std::fabs(std::sin(rn * 15.708f)));
            tex = tex + hotAccent * tierEdge * activeZone * (0.55f * openAmt) * slitMod;
            float centerCore = smoothstep(mixf(0.10f, 0.25f, openAmt), 0.0f, rn);
            tex = mixv(tex, hotAccent * 1.25f, centerCore * openAmt);
            col = over(col, tex, sphereMask);
        }
        // ===================================================================
        // ORB 6: GATE CARVE — Sculpted Quadrant Armor Hatches + 16-Tooth Escapement Gear
        // ===================================================================
        else if (orbIdx == 6) {
            float openAmt = smoothstep(0.0f, 1.0f, t);

            float gx, gy;
            rot2(nx, ny, t * 2.15f, gx, gy);
            Vec3 tex = sheet.sampleCell(row, colIdx, gx * 0.95f, gy * 0.95f);
            float magGear = clampf((tex.r - tex.g) * 2.0f, 0.0f, 1.0f);
            tex = tex * ((0.52f + 0.80f * openAmt) * sphereRimAO) + accent * magGear * (1.05f * openAmt);

            if (rn < 0.56f) {
                float gearAng = std::atan2(gy, gx);
                float teeth = std::cos(gearAng * 16.0f) * 0.048f;
                float gearOuter = 0.45f + teeth;
                float gm = smoothstep(gearOuter + 0.015f, gearOuter - 0.015f, rn)
                         * smoothstep(0.14f, 0.18f, rn);
                float spoke = std::fabs(std::sin(gearAng * 4.0f));
                Vec3 gCol = satinMetal(gx, gy, nz * 0.9f, rn * 2.0f, 0.92f + 0.24f * openAmt);
                gCol = mixv(gCol, Vec3{0.05f, 0.04f, 0.07f}, smoothstep(0.35f, 0.12f, spoke) * smoothstep(0.36f, 0.22f, rn) * 0.65f);
                tex = mixv(tex, gCol, gm * (0.55f + 0.45f * openAmt));
                float hubGlow = smoothstep(0.20f, 0.0f, rn);
                tex = mixv(tex, hotAccent * 1.20f, hubGlow * openAmt);
            }
            col = over(col, tex, sphereMask);

            // 4 sculpted chamfered armor quadrants retracting cleanly
            float gap = mixf(0.015f, 0.48f, openAmt);
            float ax = std::fabs(nx), ay = std::fabs(ny);
            float chamfer = ax + ay - (gap * 2.0f + 0.18f);
            if (ax > gap && ay > gap && chamfer > 0.0f) {
                float doorEdge = std::min(std::min(ax - gap, ay - gap), chamfer);
                float doorMask = smoothstep(0.0f, 0.025f, doorEdge) * smoothstep(1.01f, 0.94f, rn);
                float lip = smoothstep(0.048f, 0.010f, doorEdge);
                Vec3 hatch = satinMetal(nx * 0.65f, ny * 0.65f, nz * 0.9f, (ax + ay), 0.85f * sphereRimAO);
                hatch = hatch + accent * lip * (0.28f + 0.65f * openAmt);
                col = over(col, hatch, sphereMask * doorMask);
            }
        }
        // ===================================================================
        // ORB 7: SMASH — Hydraulic Titanium Crusher Anvil Blocks Over Molten Core
        // ===================================================================
        else {
            float openAmt = smoothstep(0.0f, 1.0f, t);

            float rx, ry;
            rot2(nx, ny, t * 0.55f, rx, ry);
            float compress = mixf(1.04f, 0.94f, openAmt);
            Vec3 tex = sheet.sampleCell(row, colIdx, rx * compress, ry * compress);
            float crackF = clampf((tex.r - tex.g) * 1.85f, 0.0f, 1.0f);
            tex = tex * ((0.50f + 0.85f * openAmt) * sphereRimAO) + hotAccent * crackF * (1.20f * openAmt);
            float shockR = 0.25f + 0.65f * openAmt;
            float shock = smoothstep(0.055f, 0.0f, std::fabs(rn - shockR)) * openAmt;
            tex = tex + hotAccent * shock * 0.85f;
            col = over(col, tex, sphereMask);

            float ay = std::fabs(ny), ax = std::fabs(nx);
            float jawGap = mixf(0.030f, 0.92f, openAmt);
            float jawDepth = 0.54f;
            float serration = (ax < 0.30f) ? (0.042f * std::cos(nx * 22.0f)) : 0.0f;
            float localGap = jawGap + serration;

            if (ay > localGap + jawDepth * 0.65f && rn < 0.99f) {
                float rodX = std::fabs(ax - 0.36f);
                if (rodX < 0.072f) {
                    float rodMask = smoothstep(0.072f, 0.048f, rodX);
                    float cylZ = std::sqrt(clampf(1.0f - (rodX / 0.072f) * (rodX / 0.072f), 0.0f, 1.0f));
                    Vec3 chrome = satinMetal(0.25f, -0.30f, cylZ, ny * 2.0f, 1.25f * sphereRimAO);
                    col = over(col, chrome, sphereMask * rodMask);
                }
            }

            if (ay > localGap && ay < localGap + jawDepth && ax < 0.82f) {
                float jawMask = smoothstep(localGap, localGap + 0.025f, ay)
                              * smoothstep(localGap + jawDepth, localGap + jawDepth - 0.03f, ay)
                              * smoothstep(0.82f, 0.72f, ax)
                              * smoothstep(1.01f, 0.94f, rn);
                float lip = smoothstep(localGap + 0.055f, localGap + 0.012f, ay);
                Vec3 anvil = satinMetal(nx * 0.45f, ny * 0.75f, nz * 0.85f, nx * 1.4f, 0.85f * sphereRimAO);
                anvil = anvil + accent * lip * (0.35f + 0.72f * openAmt);
                col = over(col, anvil, sphereMask * jawMask);
            }
        }
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
