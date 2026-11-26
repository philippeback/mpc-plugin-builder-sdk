// gen_panels.cpp - Organic procedural studio-surface textures (1280x628) with 4x4 Bayer
// dithering for Mushin's 8 themed pages (excluding SCOPE).
// Zero geometric shapes, lines, arcs, or grids: pure multi-scale domain-warped FBM
// mist, cellular caustics, mineral grain, brushed satin, and soft edge-faded accents.

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <fstream>
#include <string>
#include <vector>

namespace {

constexpr int W = 1280;
constexpr int H = 628;
constexpr double kPi = 3.14159265358979323846;

struct Vec3 {
    float r, g, b;
    Vec3 operator+(const Vec3& o) const { return {r + o.r, g + o.g, b + o.b}; }
    Vec3 operator*(float s) const { return {r * s, g * s, b * s}; }
};

static inline float clampf(float v, float lo, float hi) { return v < lo ? lo : (v > hi ? hi : v); }
static inline float smoothstep(float e0, float e1, float x) {
    float t = clampf((x - e0) / (e1 - e0), 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
}

static inline float hash21(int ix, int iy) {
    uint32_t n = (uint32_t)(ix * 374761393 + iy * 668265263);
    n = (n ^ (n >> 13)) * 1274126177u;
    return float((n ^ (n >> 16)) & 0xFFFF) / 65535.0f;
}

static inline void hash22(int ix, int iy, float& ox, float& oy) {
    ox = hash21(ix * 3 + 17, iy * 5 + 31);
    oy = hash21(ix * 7 + 53, iy * 11 + 71);
}

static inline float noise2(float x, float y) {
    int ix = (int)std::floor(x);
    int iy = (int)std::floor(y);
    float fx = x - float(ix), fy = y - float(iy);
    float ux = fx * fx * (3.0f - 2.0f * fx);
    float uy = fy * fy * (3.0f - 2.0f * fy);
    float a = hash21(ix, iy), b = hash21(ix + 1, iy);
    float c = hash21(ix, iy + 1), d = hash21(ix + 1, iy + 1);
    return a + (b - a) * ux + (c - a) * uy + (a - b - c + d) * ux * uy;
}

static inline float fbm4(float x, float y) {
    float v = 0.0f;
    float amp = 0.50f;
    for (int i = 0; i < 4; ++i) {
        v += amp * noise2(x, y);
        // Slightly rotated octave stepping to break any axis alignment
        float nx = x * 1.72f - y * 1.05f + 13.7f;
        float ny = x * 1.05f + y * 1.72f + 7.3f;
        x = nx; y = ny;
        amp *= 0.50f;
    }
    return v;
}

// Smooth organic domain-warped silk / marble smoke (zero hard edges or geometry)
static inline float warpedSilk(float x, float y, float seed) {
    float wx = fbm4(x + seed, y + seed * 1.7f);
    float wy = fbm4(x + 4.3f - seed, y + 2.9f + seed);
    return fbm4(x + 2.6f * wx, y + 2.6f * wy);
}

// Soft Worley cellular patina (smooth rounded stone / caustic pores, never sharp polygons)
static inline float softCells(float x, float y) {
    int ix = (int)std::floor(x), iy = (int)std::floor(y);
    float fx = x - float(ix), fy = y - float(iy);
    float minD = 4.0f;
    for (int dy = -1; dy <= 1; ++dy) {
        for (int dx = -1; dx <= 1; ++dx) {
            float px, py;
            hash22(ix + dx, iy + dy, px, py);
            float d = std::hypot(float(dx) + px - fx, float(dy) + py - fy);
            if (d < minD) minD = d;
        }
    }
    return clampf(minD / 1.2f, 0.0f, 1.0f);
}

static inline float rboxSdf(float px, float py, float x0, float y0, float w, float h, float rad = 10.0f) {
    float cx = x0 + w * 0.5f, cy = y0 + h * 0.5f;
    float hx = w * 0.5f - rad, hy = h * 0.5f - rad;
    float dx = std::fabs(px - cx) - hx;
    float dy = std::fabs(py - cy) - hy;
    return std::hypot(std::max(dx, 0.0f), std::max(dy, 0.0f)) + std::min(std::max(dx, dy), 0.0f) - rad;
}

Vec3 shadePage(int pageIdx, float x, float y) {
    const Vec3 kCyan    = {0.00f, 0.90f, 1.00f};
    const Vec3 kTeal    = {0.03f, 0.48f, 0.66f};
    const Vec3 kMagenta = {1.00f, 0.30f, 0.55f};
    const Vec3 kPlum    = {0.62f, 0.16f, 0.44f};
    const Vec3 kBg      = {13.0f / 255.0f, 15.0f / 255.0f, 18.0f / 255.0f};
    const Vec3 kPanel   = {16.0f / 255.0f, 20.0f / 255.0f, 26.0f / 255.0f};

    float dTop = 100.0f, dBot = 100.0f;
    if (pageIdx == 0) {
        dTop = rboxSdf(x, y, 8, 24, 1264, 288);
        dBot = rboxSdf(x, y, 8, 324, 1264, 296);
    } else if (pageIdx == 4) {
        dTop = std::min(rboxSdf(x, y, 8, 12, 950, 300), rboxSdf(x, y, 966, 12, 306, 300));
        dBot = std::min(rboxSdf(x, y, 8, 320, 950, 302), rboxSdf(x, y, 966, 320, 306, 302));
    } else {
        dTop = rboxSdf(x, y, 8, 12, 1264, 300);
        dBot = rboxSdf(x, y, 8, 320, 1264, 302);
    }

    const float maskTop = smoothstep(0.5f, -1.0f, dTop);
    const float maskBot = smoothstep(0.5f, -1.0f, dBot);
    Vec3 col = (pageIdx == 0) ? Vec3{8.0f / 255.0f, 10.0f / 255.0f, 14.0f / 255.0f} : kBg;
    if (maskTop <= 0.001f && maskBot <= 0.001f) return col;

    // Natural interior vignette fading textures smoothly to pure obsidian at panel borders
    const float edgeFadeTop = smoothstep(-2.0f, -68.0f, dTop);
    const float edgeFadeBot = smoothstep(-2.0f, -68.0f, dBot);
    const float u = x / float(W);

    // Subtle organic slate grain shared softly across textures
    float grain = (fbm4(x * 0.14f + 3.1f * pageIdx, y * 0.14f) - 0.5f) * 0.012f;

    Vec3 topAdd = {0, 0, 0};
    Vec3 botAdd = {0, 0, 0};

    // =======================================================================
    // 0: MAIN — Deep Volcanic Suede Texture + Subtle Dark Recessed Well Fog
    // =======================================================================
    if (pageIdx == 0) {
        if (maskTop > 0.001f) {
            float silk = warpedSilk(x * 0.0032f, y * 0.0040f, 1.3f);
            float t = smoothstep(0.28f, 0.78f, silk) * 0.055f * edgeFadeTop;
            topAdd = kTeal * (t + grain * 0.5f);
        }
        if (maskBot > 0.001f) {
            float silk = warpedSilk(x * 0.0032f, y * 0.0040f, 4.7f);
            float t = smoothstep(0.28f, 0.78f, silk) * 0.058f * edgeFadeBot;
            botAdd = kPlum * (t + grain * 0.5f);
        }
    }
    // =======================================================================
    // 1: SATURATION — Warm Liquid Obsidian Heat Haze & Granular Patina
    // =======================================================================
    else if (pageIdx == 1) {
        if (maskTop > 0.001f) {
            // Molten silk thermal drift rising diagonally across S-CURVE
            float heat = warpedSilk(x * 0.0038f - y * 0.0018f, y * 0.0042f, 8.2f);
            float ember = smoothstep(0.35f, 0.82f, heat) * 0.068f * edgeFadeTop;
            topAdd = (kTeal * 0.75f + kCyan * 0.25f) * (ember + grain);
        }
        if (maskBot > 0.001f) {
            // Crackle / patina erosion texture fading gently in INFO DECAY & INPUT STAGE
            float pat = warpedSilk(x * 0.0055f, y * 0.0055f, 12.4f);
            float dust = fbm4(x * 0.032f, y * 0.032f);
            float val = smoothstep(0.32f, 0.82f, pat * 0.72f + dust * 0.28f) * 0.070f * edgeFadeBot;
            botAdd = (kPlum * 0.75f + kMagenta * 0.25f) * (val + grain * 1.2f);
        }
    }
    // =======================================================================
    // 2: FILTER — Liquid Acoustic Caustics & Velvety Nebula Silk
    // =======================================================================
    else if (pageIdx == 2) {
        if (maskTop > 0.001f) {
            // Underwater caustic mist for Filter A
            float w1 = warpedSilk(x * 0.0042f, y * 0.0048f, 19.1f);
            float w2 = 1.0f - softCells(x * 0.009f + w1 * 0.7f, y * 0.009f);
            float caustic = smoothstep(0.32f, 0.86f, w1 * 0.65f + w2 * 0.35f) * 0.068f * edgeFadeTop;
            topAdd = (kTeal * 0.70f + kCyan * 0.30f) * (caustic + grain * 0.7f);
        }
        if (maskBot > 0.001f) {
            float w1 = warpedSilk(x * 0.0040f, y * 0.0046f, 23.6f);
            float w2 = 1.0f - softCells(x * 0.0085f + w1 * 0.6f, y * 0.0085f);
            float mixRouting = smoothstep(0.72f, 0.94f, u);
            Vec3 tint = kPlum * (1.0f - mixRouting * 0.6f) + kTeal * (mixRouting * 0.6f);
            float val = smoothstep(0.32f, 0.86f, w1 * 0.65f + w2 * 0.35f) * 0.068f * edgeFadeBot;
            botAdd = tint * (val + grain * 0.7f);
        }
    }
    // =======================================================================
    // 3: LFO & NOISE — Drifting Bioluminescent Fog & Analog Tape Cloud
    // =======================================================================
    else if (pageIdx == 3) {
        if (maskTop > 0.001f) {
            // Soft cloud morphing gradually from Cyan left to Magenta right
            float cloud = warpedSilk(x * 0.0036f, y * 0.0050f, 31.4f);
            Vec3 gradCol = kTeal * (1.0f - u) + kPlum * u;
            float val = smoothstep(0.30f, 0.84f, cloud) * 0.072f * edgeFadeTop;
            topAdd = gradCol * (val + grain * 0.7f);
        }
        if (maskBot > 0.001f) {
            // Fine tape-emulsion dust & velvety plum smoke fading smoothly across entire bay
            float smoke = warpedSilk(x * 0.0048f, y * 0.0048f, 37.9f);
            float tapeGrain = fbm4(x * 0.055f, y * 0.025f);
            float val = smoothstep(0.32f, 0.85f, smoke * 0.72f + tapeGrain * 0.28f) * 0.068f * edgeFadeBot;
            botAdd = (kPlum * 0.78f + kMagenta * 0.22f) * (val + grain * 1.1f);
        }
    }
    // =======================================================================
    // 4: MATRIX — Horizontal Brushed Obsidian Fiber & Soft Aurora Mist
    // =======================================================================
    else if (pageIdx == 4) {
        if (maskTop > 0.001f) {
            float fiber = fbm4(x * 0.0022f, y * 0.022f);
            float mist = warpedSilk(x * 0.0040f, y * 0.0045f, 41.2f);
            float val = smoothstep(0.30f, 0.82f, mist * 0.70f + fiber * 0.30f) * 0.068f * edgeFadeTop;
            topAdd = (kTeal * 0.78f + kCyan * 0.22f) * (val + grain * 0.6f);
        }
        if (maskBot > 0.001f) {
            float fiber = fbm4(x * 0.0022f, y * 0.022f + 5.1f);
            float mist = warpedSilk(x * 0.0040f, y * 0.0045f, 45.8f);
            float val = smoothstep(0.30f, 0.82f, mist * 0.70f + fiber * 0.30f) * 0.068f * edgeFadeBot;
            botAdd = (kPlum * 0.78f + kMagenta * 0.22f) * (val + grain * 0.6f);
        }
    }
    // =======================================================================
    // 5: GATE — Frosted Glacial Haze & Soft Plum Velvet Texture
    // =======================================================================
    else if (pageIdx == 5) {
        if (maskTop > 0.001f) {
            float frost = warpedSilk(x * 0.0048f, y * 0.0040f, 53.0f);
            float sheen = fbm4(x * 0.018f, y * 0.006f);
            float val = smoothstep(0.34f, 0.85f, frost * 0.70f + sheen * 0.30f) * 0.065f * edgeFadeTop;
            topAdd = kTeal * (val + grain * 0.8f);
        }
        if (maskBot > 0.001f) {
            float velvet = warpedSilk(x * 0.0042f, y * 0.0052f, 59.3f);
            float val = smoothstep(0.30f, 0.82f, velvet) * 0.068f * edgeFadeBot;
            botAdd = (kPlum * 0.78f + kMagenta * 0.22f) * (val + grain * 0.8f);
        }
    }
    // =======================================================================
    // 6: PUMP — Deep Ocean Swell Texture & Warm Basalt Sub-Texture
    // =======================================================================
    else if (pageIdx == 6) {
        if (maskTop > 0.001f) {
            // Smooth fluid swell turbulence without harsh lines
            float swell = warpedSilk(x * 0.0034f, y * 0.0046f, 67.1f);
            float val = smoothstep(0.28f, 0.82f, swell) * 0.072f * edgeFadeTop;
            topAdd = (kTeal * 0.72f + kCyan * 0.28f) * (val + grain * 0.6f);
        }
        if (maskBot > 0.001f) {
            // Dark warm volcanic stone + soft cellular warmth
            float basalt = softCells(x * 0.012f, y * 0.012f);
            float mist = warpedSilk(x * 0.0045f, y * 0.0045f, 71.7f);
            float val = smoothstep(0.30f, 0.86f, mist * 0.75f + (1.0f - basalt) * 0.25f) * 0.068f * edgeFadeBot;
            botAdd = kPlum * (val + grain * 0.8f);
        }
    }
    // =======================================================================
    // 7: DLY + LIM — Atmospheric Canyon Drift Mist & Hammered Titanium Heat
    // =======================================================================
    else if (pageIdx == 7) {
        if (maskTop > 0.001f) {
            // Ethereal drifting canyon mist gently thinning left-to-right like natural decay
            float mist = warpedSilk(x * 0.0032f, y * 0.0045f, 83.4f);
            float trailFade = 1.0f - 0.35f * u;
            float val = smoothstep(0.28f, 0.84f, mist) * 0.072f * trailFade * edgeFadeTop;
            topAdd = (kTeal * 0.72f + kCyan * 0.28f) * (val + grain * 0.6f);
        }
        if (maskBot > 0.001f) {
            // Dense hammered obsidian-titanium patina with subtle molten magenta glow
            float hammer = softCells(x * 0.016f, y * 0.016f);
            float ember = warpedSilk(x * 0.0044f, y * 0.0048f, 89.2f);
            float val = smoothstep(0.32f, 0.85f, ember * 0.72f + (1.0f - hammer) * 0.28f) * 0.068f * edgeFadeBot;
            botAdd = (kPlum * 0.75f + kMagenta * 0.25f) * (val + grain * 0.9f);
        }
    }

    Vec3 interiorTop = kPanel + topAdd;
    Vec3 interiorBot = kPanel + botAdd;
    Vec3 out = col;
    if (maskTop > 0.001f) {
        out = {
            col.r * (1.0f - maskTop) + interiorTop.r * maskTop,
            col.g * (1.0f - maskTop) + interiorTop.g * maskTop,
            col.b * (1.0f - maskTop) + interiorTop.b * maskTop
        };
    }
    if (maskBot > 0.001f) {
        out = {
            out.r * (1.0f - maskBot) + interiorBot.r * maskBot,
            out.g * (1.0f - maskBot) + interiorBot.g * maskBot,
            out.b * (1.0f - maskBot) + interiorBot.b * maskBot
        };
    }
    return {clampf(out.r, 0.0f, 1.0f), clampf(out.g, 0.0f, 1.0f), clampf(out.b, 0.0f, 1.0f)};
}

} // namespace

int main(int argc, char** argv) {
    if (argc < 2) {
        std::fprintf(stderr, "Usage: gen_panels <out_dir>\n");
        return 1;
    }
    const std::string outDir = argv[1];
    static constexpr const char* kNames[8] = {
        "bg_main", "bg_scurve", "bg_filter", "bg_lfo",
        "bg_matrix", "bg_gate", "bg_pump", "bg_dly_lim"
    };
    std::vector<uint8_t> rgba((size_t)W * (size_t)H * 4);

    static constexpr float kBayer[4][4] = {
        {-0.46875f,  0.03125f, -0.34375f,  0.15625f},
        { 0.28125f, -0.21875f,  0.40625f, -0.09375f},
        {-0.28125f,  0.21875f, -0.40625f,  0.09375f},
        { 0.46875f, -0.03125f,  0.34375f, -0.15625f},
    };

    for (int p = 0; p < 8; ++p) {
        for (int y = 0; y < H; ++y) {
            for (int x = 0; x < W; ++x) {
                Vec3 c = shadePage(p, float(x) + 0.5f, float(y) + 0.5f);
                float d = kBayer[y & 3][x & 3];
                const size_t idx = ((size_t)y * (size_t)W + (size_t)x) * 4;
                rgba[idx + 0] = (uint8_t)clampf(std::floor(c.r * 255.0f + 0.5f + d), 0.0f, 255.0f);
                rgba[idx + 1] = (uint8_t)clampf(std::floor(c.g * 255.0f + 0.5f + d), 0.0f, 255.0f);
                rgba[idx + 2] = (uint8_t)clampf(std::floor(c.b * 255.0f + 0.5f + d), 0.0f, 255.0f);
                rgba[idx + 3] = 255;
            }
        }
        const std::string outPath = outDir + "/" + kNames[p] + ".rgba";
        std::ofstream of(outPath, std::ios::binary);
        of.write(reinterpret_cast<const char*>(rgba.data()), (std::streamsize)rgba.size());
        std::printf("Rendered %s.rgba\n", kNames[p]);
    }
    return 0;
}
