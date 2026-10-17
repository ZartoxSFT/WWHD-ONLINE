/* Tunic recolouring, freestanding C (no libc, no game headers): shared by the PowerPC guest and
 * the native tests (runtime/tools/mp_protocol_test.c).
 *
 * Pixel rule: the launcher preview's (WindWakerServer LauncherUI.recolorGreenPixel), so the game
 * shows what the launcher previews. A pixel is tunic green when its HSB hue is in [65, 165], its
 * saturation >= 0.22 and its brightness >= 0.08; hue < 100 is the "sunny lime" family (secondary
 * colour, reference #A5D73E), the rest the green family (primary colour, reference #0DBD2F). The
 * new pixel takes the chosen hue, and the chosen saturation and brightness scaled by the pixel's
 * ratio to its family reference.
 *
 * Boat rule (LauncherUI.recolorRedPixel, same values): one family, hue in [335, 360) or
 * [0, 42] (red to orange: the King of Red Lions is rather orange; the yellow trim, hue ~50, is
 * left), saturation >= 0.30, brightness >= 0.08, reference #E63946 (BOAT_RED).
 *
 * Texture data (GX2 surfaces as the HD renderer uploads them, runtime/src/gfx/vulkan/formats.cpp):
 *  - BC1/BC2/BC3: little-endian RGB565 endpoints in each colour block; recolouring the two
 *    endpoints recolours the block (tiling moves whole blocks, so it does not matter here);
 *  - 8_8_8_8: R, G, B, A bytes. */
#pragma once

typedef unsigned char tc_u8;
typedef unsigned short tc_u16;
typedef unsigned int tc_u32;

#define TC_HUE_MIN 65.0f
#define TC_HUE_MAX 165.0f
#define TC_HUE_SPLIT 100.0f
#define TC_SATURATION_MIN 0.22f
#define TC_BRIGHTNESS_MIN 0.08f
#define TC_LINK_GREEN 0x0DBD2Fu      /* LauncherUI LINK_GREEN, primary reference */
#define TC_LINK_SUNNY_LIME 0xA5D73Eu /* LauncherUI LINK_SUNNY_LIME, secondary reference */
#define TC_BOAT_HUE_MIN 335.0f       /* the band wraps through 0 */
#define TC_BOAT_HUE_MAX 42.0f
#define TC_BOAT_SATURATION_MIN 0.30f
#define TC_BOAT_RED 0xE63946u        /* LauncherUI BOAT_RED */

/* GX2 surface formats (Cemu LatteReg.h E_GX2SURFFMT, low 6 bits = hardware format). */
#define TC_FMT_8_8_8_8 0x1Au
#define TC_FMT_BC1 0x31u
#define TC_FMT_BC2 0x32u
#define TC_FMT_BC3 0x33u

typedef struct tc_hsb {
    float h, s, b; /* h in [0, 360), s and b in [0, 1] */
} tc_hsb;

typedef struct tc_palette {
    tc_hsb primary, secondary;           /* chosen colours */
    tc_hsb primary_ref, secondary_ref;   /* LINK_GREEN, LINK_SUNNY_LIME */
    float hue_min, hue_max;              /* pixels recoloured; hue_min > hue_max wraps through 0 */
    float split;                         /* hue < split: secondary family (negative: none) */
    float saturation_min, brightness_min;
} tc_palette;

/* java.awt.Color.RGBtoHSB, which javafx Color.getHue/getSaturation/getBrightness match. */
static inline tc_hsb tc_rgb_to_hsb(tc_u32 rgb) {
    float r = (float)((rgb >> 16) & 0xFF) / 255.0f;
    float g = (float)((rgb >> 8) & 0xFF) / 255.0f;
    float b = (float)(rgb & 0xFF) / 255.0f;
    float max = r > g ? (r > b ? r : b) : (g > b ? g : b);
    float min = r < g ? (r < b ? r : b) : (g < b ? g : b);
    float d = max - min;
    tc_hsb out;
    out.b = max;
    out.s = max > 0.0f ? d / max : 0.0f;
    if (d <= 0.0f) {
        out.h = 0.0f;
    } else {
        float h;
        if (max == r) h = (g - b) / d;
        else if (max == g) h = 2.0f + (b - r) / d;
        else h = 4.0f + (r - g) / d;
        h *= 60.0f;
        if (h < 0.0f) h += 360.0f;
        out.h = h;
    }
    return out;
}

static inline tc_u32 tc_unit_to_byte(float v) {
    if (v <= 0.0f) return 0;
    if (v >= 1.0f) return 255;
    return (tc_u32)(v * 255.0f + 0.5f);
}

/* javafx Color.hsb (HSB to RGB). */
static inline tc_u32 tc_hsb_to_rgb(float h, float s, float v) {
    float r, g, b;
    if (s <= 0.0f) {
        r = g = b = v;
    } else {
        float sector = h / 60.0f;
        while (sector >= 6.0f) sector -= 6.0f;
        while (sector < 0.0f) sector += 6.0f;
        int i = (int)sector;
        float f = sector - (float)i;
        float p = v * (1.0f - s), q = v * (1.0f - s * f), t = v * (1.0f - s * (1.0f - f));
        switch (i) {
        case 0: r = v; g = t; b = p; break;
        case 1: r = q; g = v; b = p; break;
        case 2: r = p; g = v; b = t; break;
        case 3: r = p; g = q; b = v; break;
        case 4: r = t; g = p; b = v; break;
        default: r = v; g = p; b = q; break;
        }
    }
    return (tc_unit_to_byte(r) << 16) | (tc_unit_to_byte(g) << 8) | tc_unit_to_byte(b);
}

static inline float tc_clamp01(float v) { return v < 0.0f ? 0.0f : v > 1.0f ? 1.0f : v; }

static inline tc_palette tc_palette_make(tc_u32 primary_rgb, tc_u32 secondary_rgb) {
    tc_palette p;
    p.primary = tc_rgb_to_hsb(primary_rgb);
    p.secondary = tc_rgb_to_hsb(secondary_rgb);
    p.primary_ref = tc_rgb_to_hsb(TC_LINK_GREEN);
    p.secondary_ref = tc_rgb_to_hsb(TC_LINK_SUNNY_LIME);
    p.hue_min = TC_HUE_MIN;
    p.hue_max = TC_HUE_MAX;
    p.split = TC_HUE_SPLIT;
    p.saturation_min = TC_SATURATION_MIN;
    p.brightness_min = TC_BRIGHTNESS_MIN;
    return p;
}

/* The red/orange paint of the boat, recoloured to one colour. */
static inline tc_palette tc_palette_make_boat(tc_u32 rgb) {
    tc_palette p;
    p.primary = p.secondary = tc_rgb_to_hsb(rgb);
    p.primary_ref = p.secondary_ref = tc_rgb_to_hsb(TC_BOAT_RED);
    p.hue_min = TC_BOAT_HUE_MIN;
    p.hue_max = TC_BOAT_HUE_MAX;
    p.split = -1.0f;
    p.saturation_min = TC_BOAT_SATURATION_MIN;
    p.brightness_min = TC_BRIGHTNESS_MIN;
    return p;
}

static inline int tc_in_band(const tc_palette* p, tc_hsb px) {
    int hue = p->hue_min <= p->hue_max ? px.h >= p->hue_min && px.h <= p->hue_max
                                       : px.h >= p->hue_min || px.h <= p->hue_max;
    return hue && px.s >= p->saturation_min && px.b >= p->brightness_min;
}

/* The boat's paint made grey at the same brightness: for a lighting ramp in the paint colour (the
 * head's toonEX, pure red), which multiplies the painted texture; recoloured, the head would get
 * the chosen colour twice (darker than the body, whose ramp is neutral). */
static inline tc_palette tc_palette_make_boat_neutral(void) {
    tc_palette p = tc_palette_make_boat(TC_BOAT_RED);
    p.primary.h = p.secondary.h = 0.0f;
    p.primary.s = p.secondary.s = 0.0f;
    p.primary.b = p.secondary.b = p.primary_ref.b;
    return p;
}

/* A lighting ramp in the paint colour: most of its paint is full saturation and brightness
 * (toonEX: median 1.00 / 1.00; the painted textures stay at or under 0.88 / 0.97). */
#define TC_RAMP_LEVEL 0.95f
static inline int tc_ramp_endpoint(const tc_palette* p, tc_u32 rgb, int* paint) {
    tc_hsb px = tc_rgb_to_hsb(rgb);
    *paint = tc_in_band(p, px);
    return *paint && px.s >= TC_RAMP_LEVEL && px.b >= TC_RAMP_LEVEL;
}

/* 1 and *out set when rgb is in the palette's band (tunic green, boat red); 0 (out untouched)
 * otherwise. */
static inline int tc_recolor_rgb(const tc_palette* p, tc_u32 rgb, tc_u32* out) {
    tc_hsb px = tc_rgb_to_hsb(rgb);
    if (!tc_in_band(p, px)) return 0;
    int lime = px.h < p->split;
    const tc_hsb* chosen = lime ? &p->secondary : &p->primary;
    const tc_hsb* ref = lime ? &p->secondary_ref : &p->primary_ref;
    float s = tc_clamp01(chosen->s * (px.s / ref->s));
    float b = tc_clamp01(chosen->b * (px.b / ref->b));
    *out = tc_hsb_to_rgb(chosen->h, s, b);
    return 1;
}

static inline tc_u32 tc_565_to_888(tc_u16 c) {
    tc_u32 r = (c >> 11) & 0x1F, g = (c >> 5) & 0x3F, b = c & 0x1F;
    return (((r << 3) | (r >> 2)) << 16) | (((g << 2) | (g >> 4)) << 8) | ((b << 3) | (b >> 2));
}

/* Rounded (the tunic gradients are smooth: truncation would darken them). */
static inline tc_u16 tc_888_to_565(tc_u32 rgb) {
    tc_u32 r = ((rgb >> 16) & 0xFF) * 31 + 127, g = ((rgb >> 8) & 0xFF) * 63 + 127, b = (rgb & 0xFF) * 31 + 127;
    return (tc_u16)(((r / 255) << 11) | ((g / 255) << 5) | (b / 255));
}

static inline int tc_recolor_565(const tc_palette* p, tc_u16 c, tc_u16* out) {
    tc_u32 rgb;
    if (!tc_recolor_rgb(p, tc_565_to_888(c), &rgb)) return 0;
    *out = tc_888_to_565(rgb);
    return 1;
}

/* One BC colour block (8 bytes: c0, c1 little-endian, 32 bits of 2-bit indices).
 * In BC1 the endpoint order selects the mode (c0 > c1: four colours; else three colours and
 * transparent black, index 3), so the new endpoints are swapped and the indices remapped when
 * recolouring would flip the order. BC2/BC3 colour blocks always decode with four colours. */
static inline void tc_recolor_color_block(const tc_palette* p, tc_u8* block, int bc1) {
    tc_u16 c0 = (tc_u16)(block[0] | (block[1] << 8)), c1 = (tc_u16)(block[2] | (block[3] << 8));
    tc_u16 n0 = c0, n1 = c1;
    int changed = tc_recolor_565(p, c0, &n0);
    changed |= tc_recolor_565(p, c1, &n1);
    if (!changed) return;
    tc_u32 idx = (tc_u32)block[4] | ((tc_u32)block[5] << 8) | ((tc_u32)block[6] << 16) | ((tc_u32)block[7] << 24);
    if (bc1) {
        int four = c0 > c1;
        if (four && n0 == n1) {
            idx = 0; /* one colour: index 0 everywhere, never the transparent index */
        } else if ((four && n0 < n1) || (!four && n0 > n1)) {
            tc_u16 t = n0;
            n0 = n1;
            n1 = t;
            for (int i = 0; i != 16; ++i) {
                tc_u32 v = (idx >> (2 * i)) & 3u;
                tc_u32 nv = four ? (v ^ 1u) : (v < 2 ? (v ^ 1u) : v); /* 4: 0<->1 2<->3; 3: 0<->1 */
                idx = (idx & ~(3u << (2 * i))) | (nv << (2 * i));
            }
        }
    }
    block[0] = (tc_u8)n0; block[1] = (tc_u8)(n0 >> 8);
    block[2] = (tc_u8)n1; block[3] = (tc_u8)(n1 >> 8);
    block[4] = (tc_u8)idx; block[5] = (tc_u8)(idx >> 8);
    block[6] = (tc_u8)(idx >> 16); block[7] = (tc_u8)(idx >> 24);
}

static inline int tc_format_supported(tc_u32 format) {
    tc_u32 hw = format & 0x3Fu;
    return hw == TC_FMT_BC1 || hw == TC_FMT_BC2 || hw == TC_FMT_BC3 || hw == TC_FMT_8_8_8_8;
}

/* Recolours data[0..size) of a surface of the given GX2 format in place; 0 when unsupported. */
static inline int tc_recolor_surface(const tc_palette* p, tc_u32 format, tc_u8* data, tc_u32 size) {
    tc_u32 hw = format & 0x3Fu, i;
    if (hw == TC_FMT_BC1) {
        for (i = 0; i + 8 <= size; i += 8) tc_recolor_color_block(p, data + i, 1);
    } else if (hw == TC_FMT_BC2 || hw == TC_FMT_BC3) {
        for (i = 0; i + 16 <= size; i += 16) tc_recolor_color_block(p, data + i + 8, 0);
    } else if (hw == TC_FMT_8_8_8_8) {
        for (i = 0; i + 4 <= size; i += 4) {
            tc_u32 rgb = ((tc_u32)data[i] << 16) | ((tc_u32)data[i + 1] << 8) | data[i + 2], out;
            if (!tc_recolor_rgb(p, rgb, &out)) continue;
            data[i] = (tc_u8)(out >> 16);
            data[i + 1] = (tc_u8)(out >> 8);
            data[i + 2] = (tc_u8)out;
        }
    } else {
        return 0;
    }
    return 1;
}
