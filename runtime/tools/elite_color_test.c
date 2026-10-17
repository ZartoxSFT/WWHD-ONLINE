/* Native test of examples/guest-mods/multiplayer-connection/elite_color.h (the elites' colour):
 * the primary colour of a model and its reference, the grey fallback, the palette (white and dark
 * grey), the band recoloured and nothing else, the BC1 endpoint order kept, RGBA8 texels, the
 * materials' colours painted. */
#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "elite_color.h"

static tc_u16 rgb565(tc_u32 rgb) { return tc_888_to_565(rgb); }

static void bc1_block(tc_u8* b, tc_u16 c0, tc_u16 c1, tc_u32 idx) {
    b[0] = (tc_u8)c0; b[1] = (tc_u8)(c0 >> 8);
    b[2] = (tc_u8)c1; b[3] = (tc_u8)(c1 >> 8);
    b[4] = (tc_u8)idx; b[5] = (tc_u8)(idx >> 8); b[6] = (tc_u8)(idx >> 16); b[7] = (tc_u8)(idx >> 24);
}

static tc_hsb hsb_of(tc_u16 c) { return tc_rgb_to_hsb(tc_565_to_888(c)); }

static int near(float a, float b, float e) { return a - b < e && b - a < e; }

/* the palette index of a colour */
static tc_u32 index_of(tc_u32 rgb) {
    for (tc_u32 i = 0; i != EC_PALETTE; ++i)
        if (ec_palette[i] == rgb) return i;
    assert(0);
    return 0;
}

static void test_red_model(void) {
    /* 6 red blocks, 2 blue, 2 dark grey: the primary colour is red */
    static tc_u8 data[10 * 8];
    tc_u16 red_hi = rgb565(0xD02020), red_lo = rgb565(0x801010), blue = rgb565(0x2040D0), grey = rgb565(0x303030);
    for (int i = 0; i != 6; ++i) bc1_block(data + i * 8, red_hi, red_lo, 0x1B1B1B1Bu);
    bc1_block(data + 6 * 8, blue, rgb565(0x102080), 0);
    bc1_block(data + 7 * 8, blue, rgb565(0x102080), 0);
    bc1_block(data + 8 * 8, grey, rgb565(0x202020), 0);
    bc1_block(data + 9 * 8, grey, rgb565(0x202020), 0);
    ec_hist h;
    ec_hist_clear(&h);
    assert(ec_hist_surface(&h, TC_FMT_BC1, data, sizeof(data)));
    ec_plan p = ec_dominant(&h);
    assert(p.mode == EC_HUE && near(ec_delta(p.hue, 0.0f), 0.0f, 10.0f));
    assert(p.ref_s > 0.6f && p.ref_b > 0.5f);

    /* the emerald elite: the red blocks turn green, keep their order; blue and grey stay */
    ec_set_target(&p, index_of(0x1E9E5Au));
    static tc_u8 copy[sizeof(data)];
    memcpy(copy, data, sizeof(data));
    assert(ec_recolor_surface(&p, TC_FMT_BC1, copy, sizeof(copy)) == 6);
    for (int i = 0; i != 6; ++i) {
        tc_u8* b = copy + i * 8;
        tc_u16 c0 = (tc_u16)(b[0] | (b[1] << 8)), c1 = (tc_u16)(b[2] | (b[3] << 8));
        assert(c0 > c1);
        assert(near(ec_delta(hsb_of(c0).h, p.target.h), 0.0f, 12.0f));
        assert(hsb_of(c0).b > hsb_of(c1).b); /* the shading stays */
    }
    assert(!memcmp(copy + 6 * 8, data + 6 * 8, 4 * 8));

    /* the white elite: the red parts become light greys */
    ec_set_target(&p, index_of(0xF5F5F5u));
    memcpy(copy, data, sizeof(data));
    assert(ec_recolor_surface(&p, TC_FMT_BC1, copy, sizeof(copy)) == 6);
    tc_hsb w = hsb_of((tc_u16)(copy[0] | (copy[1] << 8)));
    assert(w.s < 0.1f && w.b > 0.85f);

    /* the dark grey elite: dark and grey */
    ec_set_target(&p, index_of(0x3B3D42u));
    memcpy(copy, data, sizeof(data));
    ec_recolor_surface(&p, TC_FMT_BC1, copy, sizeof(copy));
    tc_hsb g = hsb_of((tc_u16)(copy[0] | (copy[1] << 8)));
    assert(g.s < 0.2f && g.b < 0.4f);

    /* crimson and orange are the model's own red: the next entry (gold) instead */
    ec_set_target(&p, index_of(0xD7263Du));
    assert(near(p.target.h, tc_rgb_to_hsb(0xFFC300u).h, 1.0f));
}

static void test_grey_model(void) {
    static tc_u8 data[8 * 8];
    for (int i = 0; i != 8; ++i) bc1_block(data + i * 8, rgb565(0xB0B0B0), rgb565(0x707070), 0x55555555u);
    ec_hist h;
    ec_hist_clear(&h);
    ec_hist_surface(&h, TC_FMT_BC1, data, sizeof(data));
    ec_plan p = ec_dominant(&h);
    assert(p.mode == EC_GREY);
    ec_set_target(&p, index_of(0x9BE931u)); /* lime */
    assert(ec_recolor_surface(&p, TC_FMT_BC1, data, sizeof(data)) == 8);
    tc_hsb px = hsb_of((tc_u16)(data[0] | (data[1] << 8)));
    assert(px.s > 0.5f && near(ec_delta(px.h, p.target.h), 0.0f, 12.0f));
}

static void test_bc1_three_colour(void) {
    ec_plan p;
    p.mode = EC_HUE;
    p.hue = 0.0f;
    p.ref_s = 1.0f;
    p.ref_b = 1.0f;
    p.target = tc_rgb_to_hsb(0x2F80EDu);
    tc_u8 b[8];
    tc_u16 c0 = rgb565(0x800000), c1 = rgb565(0xFF2020);
    assert(c0 <= c1);
    bc1_block(b, c0, c1, 0xE4E4E4E4u);
    assert(ec_recolor_color_block(&p, b, 1));
    tc_u16 n0 = (tc_u16)(b[0] | (b[1] << 8)), n1 = (tc_u16)(b[2] | (b[3] << 8));
    tc_u32 idx = (tc_u32)b[4] | ((tc_u32)b[5] << 8) | ((tc_u32)b[6] << 16) | ((tc_u32)b[7] << 24);
    assert(n0 <= n1);
    for (int i = 0; i != 16; ++i) {
        tc_u32 before = (0xE4E4E4E4u >> (2 * i)) & 3u, after = (idx >> (2 * i)) & 3u;
        if (before >= 2) assert(after == before);
    }
}

static void test_rgba(void) {
    tc_u8 px[3 * 4] = {0xE0, 0x30, 0x30, 0xFF, 0x30, 0x30, 0xE0, 0xFF, 0xD0, 0x20, 0x20, 0x80};
    ec_hist h;
    ec_hist_clear(&h);
    ec_hist_surface(&h, TC_FMT_8_8_8_8, px, sizeof(px));
    ec_plan p = ec_dominant(&h);
    assert(p.mode == EC_HUE && near(ec_delta(p.hue, 0.0f), 0.0f, 15.0f));
    ec_set_target(&p, index_of(0xFFC300u));
    assert(ec_recolor_surface(&p, TC_FMT_8_8_8_8, px, sizeof(px)) == 2);
    assert(px[3] == 0xFF && px[11] == 0x80);
    assert(px[4] == 0x30 && px[5] == 0x30 && px[6] == 0xE0);
}

static void test_paint_materials(void) {
    /* a ChuChu's registers: body, shade, highlight greens of several hues; a grey light */
    ec_hist h;
    ec_hist_clear(&h);
    tc_u32 body = 0x30C040u, shade = 0x1A6030u, highlight = 0xA0F070u, light = 0xD8D8D0u;
    ec_hist_rgb(&h, body);
    ec_hist_rgb(&h, shade);
    ec_hist_rgb(&h, highlight);
    ec_plan p = ec_dominant(&h);
    assert(p.mode == EC_HUE);
    ec_set_target(&p, index_of(0xE040C8u)); /* magenta */
    tc_u32 out;
    assert(ec_paint_rgb(&p, body, &out) && near(ec_delta(tc_rgb_to_hsb(out).h, p.target.h), 0.0f, 25.0f));
    assert(ec_paint_rgb(&p, highlight, &out) && near(ec_delta(tc_rgb_to_hsb(out).h, p.target.h), 0.0f, 40.0f));
    assert(tc_rgb_to_hsb(out).b > 0.85f); /* the highlight stays bright */
    assert(!ec_paint_rgb(&p, light, &out));
}

static void test_unsupported(void) {
    ec_plan p;
    p.mode = EC_HUE;
    p.hue = 0.0f;
    p.ref_s = p.ref_b = 1.0f;
    p.target = tc_rgb_to_hsb(0x1E9E5Au);
    tc_u8 d[16] = {0};
    ec_hist h;
    ec_hist_clear(&h);
    assert(!ec_hist_surface(&h, 0x01u, d, sizeof(d)));
    assert(ec_recolor_surface(&p, 0x01u, d, sizeof(d)) == -1);
    p.mode = EC_NONE;
    assert(ec_recolor_surface(&p, TC_FMT_BC1, d, sizeof(d)) == 0);
}

int main(void) {
    test_red_model();
    test_grey_model();
    test_bc1_three_colour();
    test_rgba();
    test_paint_materials();
    test_unsupported();
    puts("elite_color_test: ok");
    return 0;
}
