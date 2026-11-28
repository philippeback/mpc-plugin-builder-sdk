// gen_scope.cpp - High-speed 32-bit float procedural shader renderer for Mushin's
// multi-theme SCOPE visualizer (SOLAR, CYBER, VOID backgrounds, FilmStrips & cores).
// Compiled on demand by plugins/Mushin/bin/make_visualizer.py via Zig C++.

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <fstream>
#include <string>
#include <vector>

namespace {

constexpr double kPi = 3.14159265358979323846;
constexpr float kTwoPi = 6.28318530717958647692f;

struct RGBA {
    float r, g, b, a;
};

static inline float clampf(float v, float lo, float hi) { return v < lo ? lo : (v > hi ? hi : v); }
static inline float lerpf(float a, float b, float t) { return a + (b - a) * t; }
static inline float smoothstep(float e0, float e1, float x) {
    float t = clampf((x - e0) / (e1 - e0), 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
}

static inline float hash21(int ix, int iy) {
    uint32_t n = (uint32_t)(ix * 374761393 + iy * 668265263);
    n = (n ^ (n >> 13)) * 1274126177u;
    return float((n ^ (n >> 16)) & 0xFFFF) / 65535.0f;
}

static inline float noise2(float x, float y) {
    int ix = (int)std::floor(x);
    int iy = (int)std::floor(y);
    float fx = x - (float)ix;
    float fy = y - (float)iy;
    float ux = fx * fx * (3.0f - 2.0f * fx);
    float uy = fy * fy * (3.0f - 2.0f * fy);
    float a = hash21(ix, iy);
    float b = hash21(ix + 1, iy);
    float c = hash21(ix, iy + 1);
    float d = hash21(ix + 1, iy + 1);
    return lerpf(lerpf(a, b, ux), lerpf(c, d, ux), uy);
}

static inline float fbm2(float x, float y, int octaves = 3) {
    float sum = 0.0f;
    float amp = 0.5f;
    float freq = 1.0f;
    for (int i = 0; i < octaves; ++i) {
        sum += amp * noise2(x * freq, y * freq);
        freq *= 2.03f;
        amp *= 0.5f;
    }
    return sum;
}

static inline float segDist(float px, float py, float ax, float ay, float bx, float by) {
    float vx = bx - ax, vy = by - ay;
    float wx = px - ax, wy = py - ay;
    float len2 = vx * vx + vy * vy;
    float t = (len2 > 1e-8f) ? clampf((wx * vx + wy * vy) / len2, 0.0f, 1.0f) : 0.0f;
    float dx = px - (ax + t * vx);
    float dy = py - (ay + t * vy);
    return std::sqrt(dx * dx + dy * dy);
}

static constexpr float kBayer[4][4] = {
    {-0.46875f,  0.03125f, -0.34375f,  0.15625f},
    { 0.28125f, -0.21875f,  0.40625f, -0.09375f},
    {-0.28125f,  0.21875f, -0.40625f,  0.09375f},
    { 0.46875f, -0.03125f,  0.34375f, -0.15625f},
};

void writeRaw(const std::string& path, int w, int h, const std::vector<RGBA>& pixels, bool opaque) {
    std::vector<uint8_t> out((size_t)w * (size_t)h * 4);
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            const RGBA& c = pixels[(size_t)y * (size_t)w + (size_t)x];
            float d = kBayer[y & 3][x & 3];
            size_t i = ((size_t)y * (size_t)w + (size_t)x) * 4;
            out[i + 0] = (uint8_t)clampf(std::floor(clampf(c.r, 0.0f, 1.0f) * 255.0f + 0.5f + d), 0.0f, 255.0f);
            out[i + 1] = (uint8_t)clampf(std::floor(clampf(c.g, 0.0f, 1.0f) * 255.0f + 0.5f + d), 0.0f, 255.0f);
            out[i + 2] = (uint8_t)clampf(std::floor(clampf(c.b, 0.0f, 1.0f) * 255.0f + 0.5f + d), 0.0f, 255.0f);
            out[i + 3] = opaque ? 255 : (uint8_t)clampf(std::floor(clampf(c.a, 0.0f, 1.0f) * 255.0f + 0.5f), 0.0f, 255.0f);
        }
    }
    std::ofstream f(path, std::ios::binary);
    f.write(reinterpret_cast<const char*>(out.data()), (std::streamsize)out.size());
}

// ============================================================================
// 1. BACKGROUNDS (1280 x 628)
// ============================================================================

void renderBgSolar(const std::string& outDir) {
    const int W = 1280, H = 628;
    std::vector<RGBA> px((size_t)W * H);
    for (int y = 0; y < H; ++y) {
        float dy = (y - H * 0.5f) / (H * 0.5f);
        float scan = ((y & 3) == 0) ? 0.87f : 1.0f;
        for (int x = 0; x < W; ++x) {
            float dx = (x - W * 0.5f) / (W * 0.5f);
            float r = std::sqrt(dx * dx + dy * dy);
            float glow = std::max(0.0f, 1.0f - r * 0.82f);
            float nebula = fbm2(x * 0.0055f + 4.1f, y * 0.0055f + 9.7f, 3);
            float dust = nebula * 0.055f * (1.0f - r * 0.5f);

            // Faint astronomical solar azimuth rings
            float cr = std::hypot((float)x - 640.0f, ((float)y - 314.0f) / 0.92f);
            float rings = 0.0f;
            for (float rad : {105.0f, 205.0f, 305.0f}) {
                float dr = std::fabs(cr - rad);
                rings += std::exp(-(dr * dr) / 1.1f) * 0.14f;
            }

            float rr = (0.032f + 0.095f * glow * glow + dust * 1.6f + rings * 0.65f) * scan;
            float gg = (0.012f + 0.028f * glow * glow + dust * 0.55f + rings * 0.25f) * scan;
            float bb = (0.016f + 0.024f * glow * glow + dust * 0.18f + rings * 0.10f) * scan;
            px[(size_t)y * W + x] = {rr, gg, bb, 1.0f};
        }
    }
    writeRaw(outDir + "/bg_scope_solar.rgba", W, H, px, true);
}

void renderBgCyber(const std::string& outDir) {
    const int W = 1280, H = 628;
    std::vector<RGBA> px((size_t)W * H);
    for (int y = 0; y < H; ++y) {
        float dy = (y - H * 0.5f) / (H * 0.5f);
        float scan = ((y & 1) == 0) ? 0.88f : 1.0f;
        for (int x = 0; x < W; ++x) {
            float dx = (x - W * 0.5f) / (W * 0.5f);
            float d2 = dx * dx + dy * dy;
            float glow = std::max(0.0f, 1.0f - d2 * 0.85f);

            // Subtle perspective CRT floor & ceiling gridlines
            float grid = 0.0f;
            float ady = std::fabs(dy);
            if (ady > 0.10f) {
                float gz = 1.0f / ady;
                float gx = dx * gz * 1.4f;
                float lineX = std::fabs(gx - std::round(gx));
                float lineZ = std::fabs(gz * 0.85f - std::round(gz * 0.85f));
                float gxGlow = std::exp(-(lineX * lineX) * 900.0f);
                float gzGlow = std::exp(-(lineZ * lineZ) * 900.0f);
                float fade = smoothstep(0.10f, 0.55f, ady) * (1.0f - smoothstep(0.65f, 1.15f, std::fabs(dx)));
                grid = (gxGlow + gzGlow) * 0.045f * fade;
            }

            float rr = (0.008f + 0.012f * glow + grid * 0.25f) * scan;
            float gg = (0.024f + 0.055f * glow + grid * 1.15f) * scan;
            float bb = (0.016f + 0.032f * glow + grid * 0.55f) * scan;
            px[(size_t)y * W + x] = {rr, gg, bb, 1.0f};
        }
    }
    writeRaw(outDir + "/bg_scope_cyber.rgba", W, H, px, true);
}

void renderBgVoid(const std::string& outDir) {
    const int W = 1280, H = 628;
    std::vector<RGBA> px((size_t)W * H);
    for (int y = 0; y < H; ++y) {
        float dy = (y - H * 0.5f) / (H * 0.5f);
        float scan = ((y & 3) == 0) ? 0.91f : 1.0f;
        for (int x = 0; x < W; ++x) {
            float dx = (x - W * 0.5f) / (W * 0.5f);
            float r = std::sqrt(dx * dx + dy * dy);
            float glow = std::max(0.0f, 1.0f - r * 0.80f);

            float wx = fbm2(x * 0.0045f + 2.3f, y * 0.0045f + 7.1f, 3);
            float wy = fbm2(x * 0.0045f + 11.4f, y * 0.0045f + 3.9f, 3);
            float silk = fbm2(x * 0.0050f + wx * 2.2f, y * 0.0050f + wy * 2.2f, 3);
            float cloud = smoothstep(0.32f, 0.85f, silk) * 0.085f * (1.0f - r * 0.45f);

            // Micro starfield
            float st = hash21(x / 2, y / 2);
            float star = ((x & 1) == 0 && (y & 1) == 0 && st > 0.9965f) ? (st - 0.9965f) * 90.0f : 0.0f;

            float rr = (0.012f + 0.025f * glow + cloud * 0.85f + star * 0.70f) * scan;
            float gg = (0.012f + 0.022f * glow + cloud * 0.45f + star * 0.85f) * scan;
            float bb = (0.036f + 0.075f * glow + cloud * 1.45f + star * 1.00f) * scan;
            px[(size_t)y * W + x] = {rr, gg, bb, 1.0f};
        }
    }
    writeRaw(outDir + "/bg_scope_void.rgba", W, H, px, true);
}

// ============================================================================
// 2. TUNNEL STRIPS (240 x 120 x 24 = 240 x 2880 <= 4096 px)
// ============================================================================

void renderSolarTunnel(const std::string& outDir) {
    const int FW = 240, FH = 120, NF = 24;
    std::vector<RGBA> px((size_t)FW * FH * NF);
    const float cx = FW * 0.5f, cy = FH * 0.5f;

    for (int f = 0; f < NF; ++f) {
        const float ph = (float)f / (float)NF;
        const float ang = ph * kTwoPi;
        const float ct = std::cos(ang), st = std::sin(ang);
        const float bx = ct * 1.6f, by = st * 1.6f;

        for (int py = 0; py < FH; ++py) {
            float dy = (py + 0.5f - cy) / (FH * 0.5f);
            for (int pxX = 0; pxX < FW; ++pxX) {
                float dx = (pxX + 0.5f - cx) / (FW * 0.5f);
                float r = std::hypot(dx * 1.77f, dy);
                float theta = std::atan2(dy, dx * 1.77f);

                float invR = 1.0f / std::max(r, 0.11f);
                // Logarithmic solar plasma arms twisting smoothly over [0, 2pi]
                float spiral = std::sin(5.0f * theta - 2.4f * invR - ang * 2.0f);
                float swirl = fbm2(dx * 3.8f + bx + 0.4f * spiral, dy * 2.4f + by - 0.4f * spiral, 3);
                float band = std::cos(invR * 4.5f - ang + swirl * 2.2f);
                float wave = 0.5f + 0.5f * std::sin(spiral * 1.8f + band * 1.4f + swirl * 3.0f);

                float edge = clampf(1.0f - (r - 0.15f) * 0.52f, 0.0f, 1.0f) * smoothstep(0.06f, 0.34f, r);
                float hot = std::pow(clampf(wave, 0.0f, 1.0f), 1.45f) * edge;

                // Incandescent palette: crimson -> solar orange -> molten gold -> white-gold
                float rr = clampf(hot * 1.25f + edge * 0.25f * swirl, 0.0f, 1.0f);
                float gg = clampf(hot * hot * 0.92f + hot * 0.28f, 0.0f, 1.0f);
                float bb = clampf(hot * hot * hot * 0.48f + (1.0f - wave) * edge * 0.14f, 0.0f, 1.0f);
                float aa = clampf(hot * 0.85f + edge * 0.18f, 0.0f, 0.92f);
                px[((size_t)f * FH + py) * FW + pxX] = {rr, gg, bb, aa};
            }
        }
    }
    writeRaw(outDir + "/scope_solar_tunnel_strip.rgba", FW, FH * NF, px, false);
}

void renderCyberTunnel(const std::string& outDir) {
    const int FW = 240, FH = 120, NF = 24;
    std::vector<RGBA> px((size_t)FW * FH * NF);

    const float aspect = 1.78f;
    // Precompute octagon vertices on unit circle
    float ux[8], uy[8];
    for (int i = 0; i < 8; ++i) {
        float a = (i + 0.5f) * (kTwoPi / 8.0f);
        ux[i] = std::cos(a);
        uy[i] = std::sin(a);
    }

    for (int f = 0; f < NF; ++f) {
        const float ph = (float)f / (float)NF; // 0..1 forward travel across 1 rib spacing
        const size_t baseOff = (size_t)f * FH * FW;

        for (int py = 0; py < FH; ++py) {
            float y = (py + 0.5f - FH * 0.5f) / (FH * 0.5f);
            for (int pxX = 0; pxX < FW; ++pxX) {
                float x = ((pxX + 0.5f - FW * 0.5f) / (FW * 0.5f)) * aspect;

                float colG = 0.0f, colR = 0.0f, colB = 0.0f;

                // Subtle 8 longitudinal corner rails toward vanishing center
                for (int k = 0; k < 8; ++k) {
                    float d = segDist(x, y, ux[k] * 0.14f, uy[k] * 0.14f, ux[k] * 1.95f, uy[k] * 1.95f);
                    float rail = std::exp(-(d * d) / 0.00045f) * 0.55f;
                    colG += rail;
                    colB += rail * 0.45f;
                }

                // Scrolling perspective octagon ribs rushing toward camera
                for (int ring = 0; ring < 9; ++ring) {
                    float z = ((float)ring + (1.0f - ph)) * 0.72f + 0.35f;
                    float rad = 1.38f / z;
                    float depthFade = clampf(1.35f - z * 0.17f, 0.08f, 1.0f);
                    float dmin = 1e6f;
                    for (int k = 0; k < 8; ++k) {
                        int k2 = (k + 1) & 7;
                        dmin = std::min(dmin, segDist(x, y, ux[k] * rad, uy[k] * rad, ux[k2] * rad, uy[k2] * rad));
                    }
                    float ribGlow = std::exp(-(dmin * dmin) / 0.00055f) * depthFade;
                    float acidShift = clampf(1.0f - z * 0.22f, 0.0f, 1.0f);
                    colR += ribGlow * acidShift * 0.75f;
                    colG += ribGlow * 1.00f;
                    colB += ribGlow * (1.0f - acidShift) * 0.55f;
                }

                float lum = clampf(std::max({colR, colG, colB}), 0.0f, 1.0f);
                px[baseOff + (size_t)py * FW + pxX] = {
                    clampf(colR, 0.0f, 1.0f),
                    clampf(colG, 0.0f, 1.0f),
                    clampf(colB, 0.0f, 1.0f),
                    clampf(lum * 0.95f, 0.0f, 0.95f)
                };
            }
        }
    }
    writeRaw(outDir + "/scope_cyber_tunnel_strip.rgba", FW, FH * NF, px, false);
}

void renderVoidTunnel(const std::string& outDir) {
    const int FW = 240, FH = 120, NF = 24;
    std::vector<RGBA> px((size_t)FW * FH * NF);
    const float cx = FW * 0.5f, cy = FH * 0.5f;

    for (int f = 0; f < NF; ++f) {
        const float ang = ((float)f / (float)NF) * kTwoPi;
        const float ca = std::cos(ang), sa = std::sin(ang);

        for (int py = 0; py < FH; ++py) {
            float dy = (py + 0.5f - cy) / (FH * 0.5f);
            for (int pxX = 0; pxX < FW; ++pxX) {
                float dx = (pxX + 0.5f - cx) / (FW * 0.5f);
                float r = std::hypot(dx * 1.65f, dy);
                float a = std::atan2(dy, dx * 1.65f);

                // Flowing organic underwater caustic filaments
                float w1 = std::sin(3.0f * a + ang + 2.2f * std::sin(r * 4.0f - ang));
                float w2 = std::cos(dx * 4.5f + ca * 1.4f + std::sin(dy * 4.2f + sa * 1.4f));
                float w3 = fbm2(dx * 2.8f + ca * 1.2f, dy * 2.8f + sa * 1.2f, 3);
                float ribbon1 = std::exp(-std::fabs(w1 + 0.55f * w2) * 3.4f);
                float ribbon2 = std::exp(-std::fabs(std::sin(r * 6.5f - ang + w3 * 3.0f)) * 4.2f);

                float edge = smoothstep(0.08f, 0.35f, r) * clampf(1.05f - r * 0.46f, 0.0f, 1.0f);
                float u = ribbon1 * 0.78f + ribbon2 * 0.65f;

                float rr = clampf((ribbon1 * 0.62f + ribbon2 * 0.30f + u * u * 0.35f) * edge, 0.0f, 1.0f);
                float gg = clampf((ribbon2 * 0.75f + u * u * 0.55f) * edge, 0.0f, 1.0f);
                float bb = clampf((ribbon1 * 1.05f + ribbon2 * 0.98f + 0.25f * w3) * edge, 0.0f, 1.0f);
                float aa = clampf(u * 0.85f * edge, 0.0f, 0.90f);
                px[((size_t)f * FH + py) * FW + pxX] = {rr, gg, bb, aa};
            }
        }
    }
    writeRaw(outDir + "/scope_void_tunnel_strip.rgba", FW, FH * NF, px, false);
}

// ============================================================================
// 3. CENTER CORES (192 x 192 x 20 = 192 x 3840 <= 4096 px)
// ============================================================================

void renderSolarCore(const std::string& outDir) {
    const int FW = 192, FH = 192, NF = 20;
    std::vector<RGBA> px((size_t)FW * FH * NF);
    const float c = FW * 0.5f;

    for (int f = 0; f < NF; ++f) {
        const float t = (float)f / (float)(NF - 1);
        const float rot = t * 0.85f;
        const float rOcc = 0.22f + 0.06f * t;

        for (int py = 0; py < FH; ++py) {
            float ny = (py + 0.5f - c) / c;
            for (int pxX = 0; pxX < FW; ++pxX) {
                float nx = (pxX + 0.5f - c) / c;
                float r = std::hypot(nx, ny);
                if (r > 0.99f) continue;
                float a = std::atan2(ny, nx);

                // Blazing 12-point prominence corona + expanding dual shock rings
                float nFlare = fbm2(std::cos(a) * 2.5f + t * 2.0f, std::sin(a) * 2.5f - t * 2.0f, 2);
                float spikes = 0.5f + 0.5f * std::cos(12.0f * (a + rot));
                float spikes2 = 0.5f + 0.5f * std::cos(7.0f * (a - rot * 1.4f));
                float coronaReach = rOcc + 0.14f + (0.28f + 0.34f * t) * (0.45f * spikes + 0.35f * spikes2 + 0.40f * nFlare);

                float radialGlow = std::exp(-std::max(0.0f, r - rOcc) / std::max(0.05f, (coronaReach - rOcc) * 0.60f));
                float drRim = r - rOcc;
                float rim = std::exp(-(drRim * drRim) / 0.0018f) * (0.65f + 0.45f * t);

                float rShock = 0.36f + 0.54f * t;
                float dShock = r - rShock;
                float shock = std::exp(-(dShock * dShock) / 0.0020f) * (0.70f * (1.0f - 0.35f * t));

                // Eclipse dark center occultation
                float occMask = smoothstep(rOcc - 0.025f, rOcc + 0.015f, r);
                float edgeMask = smoothstep(0.98f, 0.72f, r);

                float hot = (radialGlow * occMask * (0.42f + 0.65f * t) + rim + shock) * edgeMask;
                float coreEmber = std::exp(-(r * r) / (rOcc * rOcc * 0.28f)) * (0.20f + 0.45f * t) * (1.0f - occMask);

                float rr = clampf(hot * 1.25f + coreEmber * 1.1f, 0.0f, 1.0f);
                float gg = clampf(hot * hot * 0.88f + rim * 0.65f + coreEmber * 0.35f, 0.0f, 1.0f);
                float bb = clampf(rim * 0.65f + hot * hot * hot * 0.38f, 0.0f, 1.0f);
                float aa = clampf(hot * 0.96f + (1.0f - occMask) * 0.85f * smoothstep(rOcc + 0.02f, rOcc - 0.02f, r), 0.0f, 1.0f);

                px[((size_t)f * FH + py) * FW + pxX] = {rr, gg, bb, aa};
            }
        }
    }
    writeRaw(outDir + "/scope_solar_core_strip.rgba", FW, FH * NF, px, false);
}

void renderCyberCore(const std::string& outDir) {
    const int FW = 192, FH = 192, NF = 20;
    std::vector<RGBA> px((size_t)FW * FH * NF);
    const float c = FW * 0.5f;

    // 3D Octahedron (6 verts, 12 edges) + 3D Cube inside (8 verts, 12 edges)
    static constexpr float octV[6][3] = {
        { 1, 0, 0}, {-1, 0, 0}, {0,  1, 0}, {0, -1, 0}, {0, 0,  1}, {0, 0, -1}
    };
    static constexpr int octE[12][2] = {
        {0,2},{0,3},{0,4},{0,5}, {1,2},{1,3},{1,4},{1,5}, {2,4},{4,3},{3,5},{5,2}
    };
    static constexpr float cubV[8][3] = {
        {-1,-1,-1},{1,-1,-1},{1,1,-1},{-1,1,-1},
        {-1,-1, 1},{1,-1, 1},{1,1, 1},{-1,1, 1}
    };
    static constexpr int cubE[12][2] = {
        {0,1},{1,2},{2,3},{3,0}, {4,5},{5,6},{6,7},{7,4}, {0,4},{1,5},{2,6},{3,7}
    };

    for (int f = 0; f < NF; ++f) {
        const float t = (float)f / (float)(NF - 1);
        const float ry = t * 1.57f + 0.35f;
        const float rx = t * 0.95f + 0.42f;
        const float cyr = std::cos(ry), syr = std::sin(ry);
        const float cxr = std::cos(rx), sxr = std::sin(rx);
        const float scOct = 0.48f + 0.36f * t;
        const float scCub = 0.22f + 0.16f * t;

        float pOct[6][2], pCub[8][2];
        auto proj = [&](float vx, float vy, float vz, float sc, float& ox, float& oy) {
            float x1 = vx * cyr + vz * syr;
            float z1 = -vx * syr + vz * cyr;
            float y2 = vy * cxr - z1 * sxr;
            float z2 = vy * sxr + z1 * cxr;
            float p = sc * (2.6f / (2.6f - z2 * 0.45f));
            ox = x1 * p;
            oy = y2 * p;
        };
        for (int i = 0; i < 6; ++i) proj(octV[i][0], octV[i][1], octV[i][2], scOct, pOct[i][0], pOct[i][1]);
        for (int i = 0; i < 8; ++i) proj(cubV[i][0], cubV[i][1], cubV[i][2], scCub, pCub[i][0], pCub[i][1]);

        const float ringR = 0.40f + 0.50f * t;

        for (int py = 0; py < FH; ++py) {
            float ny = (py + 0.5f - c) / c;
            for (int pxX = 0; pxX < FW; ++pxX) {
                float nx = (pxX + 0.5f - c) / c;
                float r = std::hypot(nx, ny);
                if (r > 0.99f) continue;

                float dOct = 1e6f;
                for (int e = 0; e < 12; ++e) {
                    dOct = std::min(dOct, segDist(nx, ny, pOct[octE[e][0]][0], pOct[octE[e][0]][1],
                                                  pOct[octE[e][1]][0], pOct[octE[e][1]][1]));
                }
                float dCub = 1e6f;
                for (int e = 0; e < 12; ++e) {
                    dCub = std::min(dCub, segDist(nx, ny, pCub[cubE[e][0]][0], pCub[cubE[e][0]][1],
                                                  pCub[cubE[e][1]][0], pCub[cubE[e][1]][1]));
                }

                float gOct = std::exp(-(dOct * dOct) / 0.00045f) * (0.45f + 0.65f * t);
                float gCub = std::exp(-(dCub * dCub) / 0.00038f) * (0.55f + 0.55f * t);
                float dr = r - ringR;
                float gRing = std::exp(-(dr * dr) / 0.00060f) * (0.45f + 0.45f * t);

                // Radar ticks at 90 degrees
                float dCross = std::min(
                    segDist(nx, ny, -0.90f, 0.0f, -0.74f, 0.0f),
                    std::min(segDist(nx, ny, 0.74f, 0.0f, 0.90f, 0.0f),
                    std::min(segDist(nx, ny, 0.0f, -0.90f, 0.0f, -0.74f),
                             segDist(nx, ny, 0.0f, 0.74f, 0.0f, 0.90f)))
                );
                float gTick = std::exp(-(dCross * dCross) / 0.00025f) * (0.35f + 0.55f * t);

                float rr = clampf(gCub * 0.72f + gRing * 0.25f + gTick * 0.20f + gOct * 0.18f, 0.0f, 1.0f);
                float gg = clampf(gOct * 1.05f + gCub * 1.00f + gRing * 0.90f + gTick * 0.95f, 0.0f, 1.0f);
                float bb = clampf(gOct * 0.50f + gCub * 0.85f + gRing * 0.65f + gTick * 0.45f, 0.0f, 1.0f);
                float aa = clampf(std::max({gOct, gCub, gRing, gTick}), 0.0f, 1.0f);

                if (aa > 0.01f) {
                    px[((size_t)f * FH + py) * FW + pxX] = {rr, gg, bb, aa};
                }
            }
        }
    }
    writeRaw(outDir + "/scope_cyber_core_strip.rgba", FW, FH * NF, px, false);
}

void renderVoidCore(const std::string& outDir) {
    const int FW = 192, FH = 192, NF = 20;
    std::vector<RGBA> px((size_t)FW * FH * NF);
    const float c = FW * 0.5f;

    for (int f = 0; f < NF; ++f) {
        const float t = (float)f / (float)(NF - 1);
        const float rot = t * 0.65f;
        const float baseScale = 0.38f + 0.44f * t;

        for (int py = 0; py < FH; ++py) {
            float ny = (py + 0.5f - c) / c;
            for (int pxX = 0; pxX < FW; ++pxX) {
                float nx = (pxX + 0.5f - c) / c;
                float r = std::hypot(nx, ny);
                if (r > 0.99f) continue;
                float a = std::atan2(ny, nx);

                // Two interlocking bioluminescent lotus membranes
                float rPet1 = baseScale * (0.74f + 0.26f * std::cos(8.0f * (a + rot)));
                float rPet2 = baseScale * 0.72f * (0.78f + 0.24f * std::sin(8.0f * (a - rot * 1.25f)));
                float d1 = r - rPet1;
                float d2 = r - rPet2;
                float g1 = std::exp(-(d1 * d1) / 0.0022f);
                float g2 = std::exp(-(d2 * d2) / 0.0018f);

                // Soft glowing inner nebula mist inside petals
                float fill = smoothstep(rPet1 + 0.05f, 0.0f, r) * (0.14f + 0.22f * t);
                float nucleus = std::exp(-(r * r) / (0.015f + 0.045f * t));

                // 4 orbiting pearl micro-moons
                float moons = 0.0f;
                float orbitR = 0.34f + 0.44f * t;
                for (int m = 0; m < 4; ++m) {
                    float ma = rot * 2.2f + m * (kTwoPi / 4.0f);
                    float mx = std::cos(ma) * orbitR;
                    float my = std::sin(ma) * orbitR;
                    float dm2 = (nx - mx) * (nx - mx) + (ny - my) * (ny - my);
                    moons += std::exp(-dm2 / 0.0016f);
                }

                float e = 0.38f + 0.68f * t;
                float rr = clampf((g1 * 0.72f + g2 * 0.38f + fill * 0.45f + nucleus * 0.85f + moons * 0.78f) * e, 0.0f, 1.0f);
                float gg = clampf((g1 * 0.32f + g2 * 0.68f + fill * 0.32f + nucleus * 0.92f + moons * 0.92f) * e, 0.0f, 1.0f);
                float bb = clampf((g1 * 1.00f + g2 * 1.00f + fill * 0.95f + nucleus * 1.00f + moons * 1.00f) * e, 0.0f, 1.0f);
                float aa = clampf((g1 * 0.92f + g2 * 0.88f + fill * 0.70f + nucleus * 0.90f + moons * 0.95f) * e, 0.0f, 1.0f);

                if (aa > 0.01f) {
                    px[((size_t)f * FH + py) * FW + pxX] = {rr, gg, bb, aa};
                }
            }
        }
    }
    writeRaw(outDir + "/scope_void_core_strip.rgba", FW, FH * NF, px, false);
}

} // namespace

int main(int argc, char** argv) {
    if (argc < 2) {
        std::fprintf(stderr, "Usage: gen_scope <out_dir>\n");
        return 1;
    }
    const std::string outDir = argv[1];
    renderBgSolar(outDir);
    renderBgCyber(outDir);
    renderBgVoid(outDir);
    renderSolarTunnel(outDir);
    renderCyberTunnel(outDir);
    renderVoidTunnel(outDir);
    renderSolarCore(outDir);
    renderCyberCore(outDir);
    renderVoidCore(outDir);
    std::printf("Rendered all 3 theme shader suites cleanly.\n");
    return 0;
}
