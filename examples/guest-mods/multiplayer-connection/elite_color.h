/* Elite enemies' colour (elite.h), freestanding C (no libc, no game headers): shared by the
 * PowerPC guest and the native test (runtime/tools/elite_color_test.c).
 *
 * The "primary colour" of a model is the hue most of its saturated texels have: a histogram of
 * the hues (24 bins of 15 degrees) over every texture of the model (the endpoints of the BC colour
 * blocks, or the RGBA8 texels), the best window of three bins, its weighted mean hue and the mean
 * saturation and brightness of its texels (the reference). When few texels are saturated (a grey
 * or metal model: a Darknut's armour), the light greys are the primary colour instead.
 *
 * An elite takes one colour of EC_PALETTE (20, white and dark grey included): a texel of the
 * primary colour (a band of EC_BAND degrees around its hue) takes the elite's hue plus its own
 * offset to the primary hue, and the elite's saturation and brightness scaled by its ratio to the
 * reference (the tunic rule, tunic_color.h), so the shading stays and white or dark grey work;
 * in a grey model the light greys take the elite's colour the same way. The materials' colours
 * (elite.h) are painted with ec_paint_rgb: every coloured one, whatever its hue.
 *
 * Texture data as tunic_color.h reads it (BC1/BC2/BC3 little-endian RGB565 endpoints, BC1's
 * endpoint order kept by swapping and remapping the indices; RGBA8 bytes). */
#pragma once
#include "tunic_color.h"

#define EC_BINS 24
#define EC_BIN_DEGREES 15.0f
#define EC_SATURATION_MIN 0.25f    /* a texel that counts for the hue */
#define EC_BRIGHTNESS_MIN 0.12f
#define EC_GREY_SATURATION_MAX 0.18f
#define EC_GREY_BRIGHTNESS_MIN 0.25f
#define EC_COLOURED_PERCENT 12u    /* the best window holds at least this share of the samples */
#define EC_GREY_PERCENT 20u
#define EC_BAND 40.0f              /* degrees on each side of the primary hue */
#define EC_BAND_SATURATION_MIN 0.18f
#define EC_BAND_BRIGHTNESS_MIN 0.06f
#define EC_PAINT_SATURATION_MIN 0.12f /* a material colour that has a hue */
#define EC_SAMPLES_MAX 65536u      /* colour blocks (or texels) read per texture for the histogram */
#define EC_PALETTE 20

/* The elites' colours (0xRRGGBB), chosen to be told apart: index = the elite's seed. */
static const tc_u32 ec_palette[EC_PALETTE] = {
    0xD7263Du, /* crimson */  0xFF7F11u, /* orange */  0xFFC300u, /* gold */     0x9BE931u, /* lime */
    0x1E9E5Au, /* emerald */  0x14A098u, /* teal */    0x1FD1E0u, /* cyan */     0x2F80EDu, /* azure */
    0x2238C9u, /* royal */    0x4A2C9Eu, /* indigo */  0x9B45E4u, /* violet */   0xE040C8u, /* magenta */
    0xFF8FC1u, /* pink */     0xFF6F59u, /* coral */   0x7A4A24u, /* brown */    0x7D7F2Au, /* olive */
    0xF5F5F5u, /* white */    0xA9B0BAu, /* silver */  0x3B3D42u, /* dark grey */ 0x1B1B22u, /* obsidian */
};

/* EC_ALL (0.3.83, color_key.h): every texel and material colour takes the target at its own
 * brightness (ref_b: the brightness that gives the target's own), the whole model in one colour. */
enum { EC_NONE = 0, EC_HUE = 1, EC_GREY = 2, EC_ALL = 3 };

typedef struct ec_hist {
    tc_u32 bins[EC_BINS];
    float sum_s[EC_BINS], sum_b[EC_BINS];
    tc_u32 grey, total;
    float grey_b;
} ec_hist;

typedef struct ec_plan {
    int mode;           /* EC_NONE, EC_HUE, EC_GREY */
    float hue;          /* the primary hue (EC_HUE) */
    float ref_s, ref_b; /* its mean saturation and brightness (EC_GREY: the greys' brightness) */
    tc_hsb target;      /* the elite's colour */
} ec_plan;

static inline void ec_hist_clear(ec_hist* h) {
    for (int i = 0; i != EC_BINS; ++i) {
        h->bins[i] = 0;
        h->sum_s[i] = h->sum_b[i] = 0.0f;
    }
    h->grey = h->total = 0;
    h->grey_b = 0.0f;
}

static inline void ec_hist_rgb(ec_hist* h, tc_u32 rgb) {
    tc_hsb px = tc_rgb_to_hsb(rgb);
    ++h->total;
    if (px.s >= EC_SATURATION_MIN && px.b >= EC_BRIGHTNESS_MIN) {
        int bin = (int)(px.h / EC_BIN_DEGREES);
        if (bin < 0) bin = 0;
        if (bin >= EC_BINS) bin = EC_BINS - 1;
        ++h->bins[bin];
        h->sum_s[bin] += px.s;
        h->sum_b[bin] += px.b;
    } else if (px.s < EC_GREY_SATURATION_MAX && px.b >= EC_GREY_BRIGHTNESS_MIN) {
        ++h->grey;
        h->grey_b += px.b;
    }
}

/* Adds a surface's colours (format as tc_format_supported); 0 when unsupported. */
static inline int ec_hist_surface(ec_hist* h, tc_u32 format, const tc_u8* data, tc_u32 size) {
    tc_u32 hw = format & 0x3Fu, step, at = 0, block, n = 0;
    if (hw == TC_FMT_BC1) { block = 8; at = 0; }
    else if (hw == TC_FMT_BC2 || hw == TC_FMT_BC3) { block = 16; at = 8; }
    else if (hw == TC_FMT_8_8_8_8) { block = 4; }
    else return 0;
    tc_u32 blocks = size / block;
    step = blocks > EC_SAMPLES_MAX ? blocks / EC_SAMPLES_MAX : 1;
    for (tc_u32 i = 0; i < blocks && n != EC_SAMPLES_MAX; i += step, ++n) {
        const tc_u8* b = data + i * block;
        if (hw == TC_FMT_8_8_8_8) {
            ec_hist_rgb(h, ((tc_u32)b[0] << 16) | ((tc_u32)b[1] << 8) | b[2]);
        } else {
            ec_hist_rgb(h, tc_565_to_888((tc_u16)(b[at] | (b[at + 1] << 8))));
            ec_hist_rgb(h, tc_565_to_888((tc_u16)(b[at + 2] | (b[at + 3] << 8))));
        }
    }
    return 1;
}

static inline float ec_wrap(float h) {
    while (h >= 360.0f) h -= 360.0f;
    while (h < 0.0f) h += 360.0f;
    return h;
}

/* Signed hue difference a - b in (-180, 180]. */
static inline float ec_delta(float a, float b) {
    float d = ec_wrap(a - b);
    return d > 180.0f ? d - 360.0f : d;
}

/* The model's primary colour from its histogram (its target: ec_set_target). */
static inline ec_plan ec_dominant(const ec_hist* h) {
    ec_plan p;
    p.mode = EC_NONE;
    p.hue = 0.0f;
    p.ref_s = p.ref_b = 1.0f;
    p.target.h = p.target.s = p.target.b = 0.0f;
    if (!h->total) return p;
    tc_u32 best = 0;
    int centre = 0;
    for (int i = 0; i != EC_BINS; ++i) {
        tc_u32 sum = h->bins[(i + EC_BINS - 1) % EC_BINS] + h->bins[i] + h->bins[(i + 1) % EC_BINS];
        if (sum > best) {
            best = sum;
            centre = i;
        }
    }
    int coloured = best && best * 100u >= h->total * EC_COLOURED_PERCENT;
    int grey = h->grey * 100u >= h->total * EC_GREY_PERCENT;
    if (coloured || (best && !grey)) { /* a little colour on a dark model is still its colour */
        float weight = 0.0f, offset = 0.0f, s = 0.0f, b = 0.0f;
        for (int k = -1; k <= 1; ++k) {
            int bin = (centre + k + EC_BINS) % EC_BINS;
            float c = (float)h->bins[bin];
            weight += c;
            offset += c * (float)k * EC_BIN_DEGREES;
            s += h->sum_s[bin];
            b += h->sum_b[bin];
        }
        p.mode = EC_HUE;
        p.hue = ec_wrap(((float)centre + 0.5f) * EC_BIN_DEGREES + (weight > 0.0f ? offset / weight : 0.0f));
        p.ref_s = weight > 0.0f ? s / weight : 1.0f;
        p.ref_b = weight > 0.0f ? b / weight : 1.0f;
    } else if (grey) {
        p.mode = EC_GREY;
        p.ref_s = 1.0f;
        p.ref_b = h->grey ? h->grey_b / (float)h->grey : 1.0f;
    }
    if (p.ref_s < 0.05f) p.ref_s = 0.05f;
    if (p.ref_b < 0.05f) p.ref_b = 0.05f;
    return p;
}

/* The elite's colour: palette entry seed % EC_PALETTE, the next one when it is the primary
 * colour itself (a hue within 35 degrees, both saturated). */
static inline void ec_set_target(ec_plan* p, tc_u32 seed) {
    for (tc_u32 k = 0; k != EC_PALETTE; ++k) {
        tc_hsb t = tc_rgb_to_hsb(ec_palette[(seed + k) % EC_PALETTE]);
        float d = ec_delta(t.h, p->hue);
        int same = p->mode == EC_HUE && t.s >= EC_SATURATION_MIN && d < 35.0f && d > -35.0f;
        if (!same || k + 1 == EC_PALETTE) {
            p->target = t;
            return;
        }
    }
}

/* A colour of hue offset d to the primary, saturation s and brightness b, in the elite's colour. */
static inline tc_u32 ec_paint(const ec_plan* p, float d, float s, float b, int grey) {
    float ns = grey ? p->target.s : tc_clamp01(p->target.s * (s / p->ref_s));
    float nb = tc_clamp01(p->target.b * (b / p->ref_b));
    return tc_hsb_to_rgb(ec_wrap(p->target.h + d), ns, nb);
}

/* EC_ALL: any colour, in the target at its brightness; the brightest keep a light shine. */
static inline tc_u32 ec_paint_all(const ec_plan* p, tc_hsb px) {
    float nb = tc_clamp01(p->target.b * (px.b / p->ref_b));
    float ns = p->target.s;
    if (px.b > 0.85f && px.s < 0.2f) ns = tc_clamp01(ns * (1.0f - (px.b - 0.85f) * 3.0f));
    return tc_hsb_to_rgb(p->target.h, ns, nb);
}

/* 1 and *out set when rgb is of the primary colour; 0 otherwise (textures). */
static inline int ec_recolor_rgb(const ec_plan* p, tc_u32 rgb, tc_u32* out) {
    tc_hsb px = tc_rgb_to_hsb(rgb);
    if (p->mode == EC_ALL) {
        if (px.b < 0.03f) return 0; /* black stays */
        *out = ec_paint_all(p, px);
        return 1;
    }
    if (p->mode == EC_HUE) {
        float d = ec_delta(px.h, p->hue);
        if (px.s < EC_BAND_SATURATION_MIN || px.b < EC_BAND_BRIGHTNESS_MIN || d > EC_BAND || d < -EC_BAND) return 0;
        *out = ec_paint(p, d, px.s, px.b, 0);
        return 1;
    }
    if (p->mode == EC_GREY) {
        if (px.s >= EC_GREY_SATURATION_MAX || px.b < EC_GREY_BRIGHTNESS_MIN) return 0;
        *out = ec_paint(p, 0.0f, px.s, px.b, 1);
        return 1;
    }
    return 0;
}

/* A material colour (elite.h): every one that has a hue takes the elite's colour, keeping its
 * offset to the primary hue and its ratio to the reference; greys (the lights) stay. */
static inline int ec_paint_rgb(const ec_plan* p, tc_u32 rgb, tc_u32* out) {
    tc_hsb px = tc_rgb_to_hsb(rgb);
    if (p->mode == EC_ALL) {
        if (px.b < 0.03f) return 0;
        *out = ec_paint_all(p, px);
        return 1;
    }
    if (p->mode != EC_HUE || px.s < EC_PAINT_SATURATION_MIN || px.b < EC_BAND_BRIGHTNESS_MIN) return 0;
    *out = ec_paint(p, ec_delta(px.h, p->hue), px.s, px.b, 0);
    return 1;
}

static inline int ec_recolor_565(const ec_plan* p, tc_u16 c, tc_u16* out) {
    tc_u32 rgb;
    if (!ec_recolor_rgb(p, tc_565_to_888(c), &rgb)) return 0;
    *out = tc_888_to_565(rgb);
    return 1;
}

/* One BC colour block, as tc_recolor_color_block; 1 when it changed. */
static inline int ec_recolor_color_block(const ec_plan* p, tc_u8* block, int bc1) {
    tc_u16 c0 = (tc_u16)(block[0] | (block[1] << 8)), c1 = (tc_u16)(block[2] | (block[3] << 8));
    tc_u16 n0 = c0, n1 = c1;
    int changed = ec_recolor_565(p, c0, &n0);
    changed |= ec_recolor_565(p, c1, &n1);
    if (!changed || (n0 == c0 && n1 == c1)) return 0;
    tc_u32 idx = (tc_u32)block[4] | ((tc_u32)block[5] << 8) | ((tc_u32)block[6] << 16) | ((tc_u32)block[7] << 24);
    if (bc1) {
        int four = c0 > c1;
        if (four && n0 == n1) {
            idx = 0;
        } else if ((four && n0 < n1) || (!four && n0 > n1)) {
            tc_u16 t = n0;
            n0 = n1;
            n1 = t;
            for (int i = 0; i != 16; ++i) {
                tc_u32 v = (idx >> (2 * i)) & 3u;
                tc_u32 nv = four ? (v ^ 1u) : (v < 2 ? (v ^ 1u) : v);
                idx = (idx & ~(3u << (2 * i))) | (nv << (2 * i));
            }
        }
    }
    block[0] = (tc_u8)n0; block[1] = (tc_u8)(n0 >> 8);
    block[2] = (tc_u8)n1; block[3] = (tc_u8)(n1 >> 8);
    block[4] = (tc_u8)idx; block[5] = (tc_u8)(idx >> 8);
    block[6] = (tc_u8)(idx >> 16); block[7] = (tc_u8)(idx >> 24);
    return 1;
}

/* Recolours data[0..size) in place; the number of blocks (texels) changed, -1 when unsupported. */
static inline int ec_recolor_surface(const ec_plan* p, tc_u32 format, tc_u8* data, tc_u32 size) {
    tc_u32 hw = format & 0x3Fu, i;
    int changed = 0;
    if (p->mode == EC_NONE) return 0;
    if (hw == TC_FMT_BC1) {
        for (i = 0; i + 8 <= size; i += 8) changed += ec_recolor_color_block(p, data + i, 1);
    } else if (hw == TC_FMT_BC2 || hw == TC_FMT_BC3) {
        for (i = 0; i + 16 <= size; i += 16) changed += ec_recolor_color_block(p, data + i + 8, 0);
    } else if (hw == TC_FMT_8_8_8_8) {
        for (i = 0; i + 4 <= size; i += 4) {
            tc_u32 rgb = ((tc_u32)data[i] << 16) | ((tc_u32)data[i + 1] << 8) | data[i + 2], out;
            if (!ec_recolor_rgb(p, rgb, &out) || out == rgb) continue;
            data[i] = (tc_u8)(out >> 16);
            data[i + 1] = (tc_u8)(out >> 8);
            data[i + 2] = (tc_u8)out;
            ++changed;
        }
    } else {
        return -1;
    }
    return changed;
}
