/* Boat colour on the King of Red Lions' materials (daShip_c: body mpBodyAnm +0x3B4, head mpHeadAnm
 * +0x3B8, McaMorf model +0x90, J3DModel::mModelData +0xAC; decomp include/d/actor/d_a_ship.h,
 * include/m_Do/m_Do_ext.h, include/JSystem/J3DGraph.h).
 *
 * In game the textures of both models hold no red (boat_paint.h found none) and the HD uniform
 * slots read 0 outside drawing (logged): the red is a GX material colour. HD materials keep the GX
 * colours in their TEV block (material + 0x18, vtable at +4: setTevColor +0x24, getTevColor +0x34
 * returning GXColorS10*, setTevKColor +0x3C, getTevKColor +0x4C returning GXColor*) and a copy for
 * the shader in their uniform block (material + 0xA0: dirty bits at +0, one per slot; TEV colour n is
 * slot 4 + n, konst colour n slot 7 + n, 027F9F0C returns the slot). The game changes one exactly
 * like this, at draw time (d_a_bgn_draw.cpp: tev colour 0 -> uniform 4 with bit 0x10;
 * d_a_ghostship.cpp: konst colour 3 -> uniform 10 with bit 0x400; d_a_obj_ikada.cpp and the ship's
 * own shadow: tev colour 1 -> uniform 5 with bit 0x20): set the GX colour, call the TEV block's
 * setter, convert the colour with 0274D458(dst, src, 1.0f) and write the slot with its dirty bit.
 *
 * A register is paint when its GX colour is red-dominant (r >= 1.15 g and b, r >= 40). The
 * original is read once per materials address (model data is shared between ship instances, so a
 * later instance would show our colour) and every register of every material is logged once.
 * The new colour is the chosen one at the original's brightest channel. */
#pragma once
#include "wwhd_guest.h"

#define SHIP_BODY_MORF_OFFSET 0x3B4
#define SHIP_HEAD_MORF_OFFSET 0x3B8
#define MORF_MODEL_OFFSET 0x90
#define MODEL_DATA_OFFSET 0xAC
#define MODEL_MATERIAL_COUNT_OFFSET 0x0C
#define MODEL_MATERIALS_OFFSET 0x10
#define MATERIAL_SIZE 0x39C
#define MATERIAL_TEV_BLOCK 0x18
#define MATERIAL_UNIFORMS 0xA0
#define TEV_BLOCK_VTABLE 0x04
#define TEV_SET_COLOR 0x24
#define TEV_GET_COLOR 0x34
#define TEV_SET_KCOLOR 0x3C
#define TEV_GET_KCOLOR 0x4C
#define SHIP_TEV_COLORS 3   /* C0..C2 */
#define SHIP_KONST_COLORS 4 /* K0..K3 */
#define SHIP_REGISTERS (SHIP_TEV_COLORS + SHIP_KONST_COLORS)
#define SHIP_MODELS 2
#define SHIP_MAX_MATERIALS 16

WWHD_GAME_FUNC(0x027F9F0C, u32, ship_uniform_slot, (u32 uniforms, s32 index));
WWHD_GAME_FUNC(0x0274D458, void, ship_convert_color, (u32 dst, u32 src, f32 intensity));

typedef u32 (*ship_tev_get)(u32 tev, u32 index);
typedef void (*ship_tev_set)(u32 tev, u32 index, u32 color);

typedef struct ship_paint_model {
    u32 model;                                       /* last J3DModel seen (lighting hook filter) */
    u32 materials;                                   /* materials whose originals are known */
    u32 count;
    u8 painted[SHIP_MAX_MATERIALS];                  /* bit per register */
    s16 original[SHIP_MAX_MATERIALS][SHIP_REGISTERS][3];
} ship_paint_model;

static ship_paint_model ship_paint[SHIP_MODELS];
static int ship_color_custom;
static u32 ship_color_rgb;
static int ship_paint_applied;
/* arguments of the colour conversion, passed to game code by address */
static f32 ship_convert_in[4] __attribute__((aligned(16)));
static f32 ship_convert_out[4] __attribute__((aligned(16)));

static u32 ship_ld32(u32 address) { return *(const volatile u32*)address; }

/* Guest address of register r's GX colour (r < 3: GXColorS10, else GXColor); 0 when none. */
static u32 ship_register_color(u32 tev, int r) {
    u32 vtable = ship_ld32(tev + TEV_BLOCK_VTABLE);
    if (!vtable) return 0;
    ship_tev_get get = (ship_tev_get)ship_ld32(vtable + (r < SHIP_TEV_COLORS ? TEV_GET_COLOR : TEV_GET_KCOLOR));
    return get(tev, (u32)(r < SHIP_TEV_COLORS ? r : r - SHIP_TEV_COLORS));
}

static void ship_read_rgb(u32 color, int r, s16* rgb) {
    for (int k = 0; k != 3; ++k)
        rgb[k] = r < SHIP_TEV_COLORS ? *(const volatile s16*)(color + 2 * (u32)k)
                                     : (s16) * (const volatile u8*)(color + (u32)k);
}

/* GX colour, TEV block setter, then the shader's copy, as the game does. */
static void ship_write_register(u32 material, u32 tev, int r, u32 color, const s16* rgb) {
    for (int k = 0; k != 3; ++k) {
        if (r < SHIP_TEV_COLORS) *(volatile s16*)(color + 2 * (u32)k) = rgb[k];
        else *(volatile u8*)(color + (u32)k) = (u8)(rgb[k] < 0 ? 0 : rgb[k] > 255 ? 255 : rgb[k]);
    }
    u32 vtable = ship_ld32(tev + TEV_BLOCK_VTABLE);
    ship_tev_set set = (ship_tev_set)ship_ld32(vtable + (r < SHIP_TEV_COLORS ? TEV_SET_COLOR : TEV_SET_KCOLOR));
    set(tev, (u32)(r < SHIP_TEV_COLORS ? r : r - SHIP_TEV_COLORS), color);

    f32 alpha = r < SHIP_TEV_COLORS ? (f32) * (const volatile s16*)(color + 6) : (f32) * (const volatile u8*)(color + 3);
    for (int k = 0; k != 3; ++k) ship_convert_in[k] = (f32)rgb[k] / 255.0f;
    ship_convert_in[3] = alpha / 255.0f;
    ship_convert_color((u32)ship_convert_out, (u32)ship_convert_in, 1.0f);
    s32 slot = r < SHIP_TEV_COLORS ? 4 + r : 7 + (r - SHIP_TEV_COLORS);
    u32 uniforms = material + MATERIAL_UNIFORMS;
    *(volatile u32*)uniforms |= 1u << slot;
    f32* out = (f32*)ship_uniform_slot(uniforms, slot);
    if (!out) return;
    out[0] = ship_convert_out[0];
    out[1] = ship_convert_out[1];
    out[2] = ship_convert_out[2];
    out[3] = alpha / 255.0f;
}

static void ship_learn(ship_paint_model* slot, u32 materials, u32 count) {
    slot->materials = materials;
    slot->count = count;
    wwhd_log_int("multiplayer: boat model materials", (int)count);
    for (u32 i = 0; i != count; ++i) {
        u32 tev = ship_ld32(materials + i * MATERIAL_SIZE + MATERIAL_TEV_BLOCK);
        slot->painted[i] = 0;
        if (!tev) continue;
        for (int r = 0; r != SHIP_REGISTERS; ++r) {
            u32 color = ship_register_color(tev, r);
            if (!color) continue;
            s16* o = slot->original[i][r];
            ship_read_rgb(color, r, o);
            int paint = o[0] >= 40 && o[0] * 20 >= o[1] * 23 && o[0] * 20 >= o[2] * 23;
            if (paint) slot->painted[i] |= (u8)(1u << r);
            wwhd_log_int(r < SHIP_TEV_COLORS ? "multiplayer: boat material C" : "multiplayer: boat material K",
                         (int)(i * 10 + (u32)(r < SHIP_TEV_COLORS ? r : r - SHIP_TEV_COLORS)));
            wwhd_log_int("multiplayer: boat   r", o[0]);
            wwhd_log_int("multiplayer: boat   g", o[1]);
            wwhd_log_int("multiplayer: boat   b", o[2]);
            if (paint) wwhd_log("multiplayer: boat   -> paint");
        }
    }
}

static void apply_ship_model_color(ship_paint_model* slot, u32 model) {
    if (!model || (!ship_color_custom && !ship_paint_applied)) return;
    u32 model_data = ship_ld32(model + MODEL_DATA_OFFSET);
    if (!model_data) return;
    u32 count = ship_ld32(model_data + MODEL_MATERIAL_COUNT_OFFSET);
    u32 materials = ship_ld32(model_data + MODEL_MATERIALS_OFFSET);
    if (count == 0 || !materials) return;
    if (count > SHIP_MAX_MATERIALS) count = SHIP_MAX_MATERIALS;
    slot->model = model;
    if (slot->materials != materials || slot->count != count) ship_learn(slot, materials, count);

    for (u32 i = 0; i != count; ++i) {
        if (!slot->painted[i]) continue;
        u32 material = materials + i * MATERIAL_SIZE;
        u32 tev = ship_ld32(material + MATERIAL_TEV_BLOCK);
        if (!tev) continue;
        for (int r = 0; r != SHIP_REGISTERS; ++r) {
            if (!(slot->painted[i] & (1u << r))) continue;
            u32 color = ship_register_color(tev, r);
            if (!color) continue;
            const s16* o = slot->original[i][r];
            s16 rgb[3] = {o[0], o[1], o[2]};
            if (ship_color_custom) {
                s32 peak = o[0] > o[1] ? (o[0] > o[2] ? o[0] : o[2]) : (o[1] > o[2] ? o[1] : o[2]);
                for (int k = 0; k != 3; ++k)
                    rgb[k] = (s16)((s32)((ship_color_rgb >> (16 - 8 * k)) & 0xFF) * peak / 255);
            }
            ship_write_register(material, tev, r, color, rgb);
        }
    }
}

static void apply_ship_color(void* actor) {
    if (!actor) return;
    static const u32 morfs[SHIP_MODELS] = {SHIP_BODY_MORF_OFFSET, SHIP_HEAD_MORF_OFFSET};
    for (int m = 0; m != SHIP_MODELS; ++m) {
        u32 morf = ship_ld32((u32)actor + morfs[m]);
        if (morf) apply_ship_model_color(&ship_paint[m], ship_ld32(morf + MORF_MODEL_OFFSET));
    }
    ship_paint_applied = ship_color_custom;
}
