/* Tunic colours in the HD game (engine side of tunic_color.h).
 *
 * The GameCube client recoloured an RGB565 ResTIMG and patched the texture pointer of each
 * Link's display list (Hooks.cpp PlayerUseAlternativeTexture). In HD the Link body texture
 * headers carry an HD texture object (ResTIMG + 0x20, "mHDTexture", decomp d_a_player_main_04b.h
 * TextureHeader) and the renderer re-uploads a texture whose guest memory the CPU wrote
 * (runtime/src/write_watch.h). So:
 *
 *  Local Link: the hero clothes texture ("linktexS3TC", d_a_player_main_04b.cpp playerInit) is
 *  recoloured in place from a pristine copy kept in the mod heap. Every Link drawn from this
 *  model data shows it (the local player and, unless the remote swap below works, the puppets).
 *
 *  Remote Link puppets (experimental): each puppet whose colours differ gets its own copy of the
 *  texture (texture object copy pointing to its own recoloured image), swapped into the shared texture
 *  table only while that puppet runs daPy_lk_c::draw, and the table's dirty bit is set like the
 *  game's own setResTIMG does (playerInit). Whether the HD J3D reads the header during draw
 *  (GameCube: display list patched at entry) is not in the sources: if it reads it later, the
 *  puppet shows the local colours and nothing else changes.
 *
 * Finding the hero texture (decomp d_a_player_main_04b.cpp playerInit, d_a_player.h):
 *  - mpCurrLinktex (SDK, +0x454) points to the originals list at +0x458 {s32 count, s32 capacity,
 *    u32 headers}: the table entries that held "linktexS3TC";
 *  - in the casual clothes (mNoResetFlg1 +0x3BC, daPyFlg1_CASUAL_CLOTHES 0x8) those entries hold
 *    the casual texture and the hero headers are the copies at +0x48C (0x24 each, at most 10);
 *    otherwise the entries themselves are the hero headers.
 * The header's HD pointer is not a GX2Texture but a 0x90-byte texture object: u32 flags, then a
 * GX2Surface at +4 (decomp include/d/actor/d_a_boomerang.h boomerangTextureSurface_c, filled by
 * 02773798 from the same pointer of the "Link" archive image 0x72; confirmed in game: dimension 1
 * at +4, width at +8, image and mip pointers at +0x28 / +0x30). GX2Surface field order:
 * runtime/third_party/cemu GX2_Surface.h. */
#pragma once
#include "wwhd_guest.h"
#include "wwhd/link.h"
#include "link_access.h"
#include "tunic_color.h"

#define TUNIC_MAX_TEXTURES 2
#define TUNIC_MAX_SLOTS 4               /* PUPPET_MAX */
#define TUNIC_MAX_BYTES (3u << 20)      /* image + mips of one texture */
#define TUNIC_HASH_BYTES 4096u
#define TUNIC_ADDRESS_SPAN 0x10000u     /* copies keep the low 16 address bits (tiling swizzle) */

#define LINK_NO_RESET_FLG1 0x3BC
#define LINK_CASUAL_CLOTHES 0x8u
#define LINK_TEX_COPIES 0x48C
#define LINK_TEX_MAX 10
#define TEX_HEADER_BYTES 0x24
#define TEX_HEADER_HD 0x20
#define MODEL_DATA_TEXTURE_TABLE 0x30   /* TextureTable {u16 num, u32 res, u32 dirty[4], u32 seed[2]} */
#define TEX_TABLE_DIRTY 0x08
#define TEX_TABLE_SEED 0x18
#define TEX_TABLE_MAX 64

/* The game allocates 192 bytes for such an object (d_a_tbox_env.cpp envShapeSet, Tbox_allocateTexture(192)
 * then its constructor); copying only the 0x90-byte surface part made the renderer read past the
 * copy (in game: "Unsupported GX2 surface format 512" when a puppet was drawn). */
#define HD_TEXTURE_BYTES 0xC0
#define HD_TEXTURE_SURFACE 0x04       /* GX2Surface inside it */
#define GX2_SURFACE_WIDTH (HD_TEXTURE_SURFACE + 0x04)
#define GX2_SURFACE_HEIGHT (HD_TEXTURE_SURFACE + 0x08)
#define GX2_SURFACE_FORMAT (HD_TEXTURE_SURFACE + 0x14)
#define GX2_SURFACE_IMAGE_SIZE (HD_TEXTURE_SURFACE + 0x20)
#define GX2_SURFACE_IMAGE (HD_TEXTURE_SURFACE + 0x24)
#define GX2_SURFACE_MIP_SIZE (HD_TEXTURE_SURFACE + 0x28)
#define GX2_SURFACE_MIP (HD_TEXTURE_SURFACE + 0x2C)
#define GX2_SURFACE_TILE_MODE (HD_TEXTURE_SURFACE + 0x30)

#define TUNIC_DEFAULT_PRIMARY 0x5D89u   /* mp_player_look_default: the original tunic */
#define TUNIC_DEFAULT_SECONDARY 0xAF08u

static inline u32 tunic_ld32(u32 address) { return *(const volatile u32*)address; }
static inline void tunic_st32(u32 address, u32 value) { *(volatile u32*)address = value; }

typedef struct tunic_look {
    int custom;          /* 0: the original texture */
    u16 primary, secondary; /* RGB565, as sent in ActorPlayerSync colors[0..1] */
} tunic_look;

typedef struct tunic_texture {
    u32 gx2;             /* HD texture object guest address */
    u32 format, image, image_size, mip, mip_size;
    u8* pristine;        /* image bytes, then mip bytes */
    u32 live_hash;       /* first bytes of the live image after our last write */
} tunic_texture;

typedef struct tunic_clone {
    int ready;
    u32 source_image;    /* tunic_texture.image it was made from */
    tunic_look look;
    u8* gx2;             /* HD texture object copy */
    u8* image_raw;
    u8* mip_raw;
    u32 image, mip;
} tunic_clone;

static tunic_texture tunic_textures[TUNIC_MAX_TEXTURES];
static u32 tunic_texture_count;
static tunic_look tunic_local_wanted, tunic_local_applied;
static int tunic_disabled;   /* unusable texture: logged once, nothing written */
static int tunic_remote_enabled; /* option remote_tunics, off by default (experimental) */
static tunic_clone tunic_clones[TUNIC_MAX_SLOTS][TUNIC_MAX_TEXTURES];
static int tunic_clone_failed;
static u32 tunic_swap_table;
static u32 tunic_swap_count;
static u32 tunic_swap_index[TEX_TABLE_MAX];
static u32 tunic_swap_original[TEX_TABLE_MAX];

static tunic_look tunic_look_make(u16 primary, u16 secondary) {
    tunic_look l;
    l.primary = primary;
    l.secondary = secondary;
    l.custom = !(primary == TUNIC_DEFAULT_PRIMARY && secondary == TUNIC_DEFAULT_SECONDARY);
    return l;
}

static int tunic_same_look(const tunic_look* a, const tunic_look* b) {
    if (!a->custom || !b->custom) return a->custom == b->custom;
    return a->primary == b->primary && a->secondary == b->secondary;
}

static u32 tunic_hash(u32 address, u32 size) {
    u32 h = 2166136261u;
    if (size > TUNIC_HASH_BYTES) size = TUNIC_HASH_BYTES;
    const volatile u8* p = (const volatile u8*)address;
    for (u32 i = 0; i != size; ++i) h = (h ^ p[i]) * 16777619u;
    return h;
}

/* The HD texture objects of the hero clothes, from the local Link; 0 when none. */
static u32 tunic_find_hero(u32 local, u32* out) {
    const daPy_lk_c* link = (const daPy_lk_c*)local;
    u32 list = link->mpCurrLinktex;
    if (!list) return 0;
    u32 count = tunic_ld32(list);
    u32 headers = tunic_ld32(list + 8);
    if (count > LINK_TEX_MAX) count = LINK_TEX_MAX;
    int casual = (tunic_ld32(local + LINK_NO_RESET_FLG1) & LINK_CASUAL_CLOTHES) != 0;
    u32 found = 0;
    for (u32 k = 0; k != count; ++k) {
        u32 header = casual ? local + LINK_TEX_COPIES + k * TEX_HEADER_BYTES
                            : (headers ? tunic_ld32(headers + k * 4) : 0);
        if (!header) continue;
        u32 gx2 = tunic_ld32(header + TEX_HEADER_HD);
        if (!gx2 || (gx2 & 3)) continue;
        int known = 0;
        for (u32 i = 0; i != found; ++i) known |= out[i] == gx2;
        if (!known && found != TUNIC_MAX_TEXTURES) out[found++] = gx2;
    }
    return found;
}

static void tunic_log_texture(const char* what, u32 gx2) {
    wwhd_log(what);
    wwhd_log_hex("multiplayer: tunic   texture object", gx2);
    wwhd_log_hex("multiplayer: tunic   format", tunic_ld32(gx2 + GX2_SURFACE_FORMAT));
    wwhd_log_hex("multiplayer: tunic   image", tunic_ld32(gx2 + GX2_SURFACE_IMAGE));
    wwhd_log_int("multiplayer: tunic   width", (int)tunic_ld32(gx2 + GX2_SURFACE_WIDTH));
    wwhd_log_int("multiplayer: tunic   height", (int)tunic_ld32(gx2 + GX2_SURFACE_HEIGHT));
    wwhd_log_int("multiplayer: tunic   image bytes", (int)tunic_ld32(gx2 + GX2_SURFACE_IMAGE_SIZE));
    wwhd_log_int("multiplayer: tunic   mip bytes", (int)tunic_ld32(gx2 + GX2_SURFACE_MIP_SIZE));
    wwhd_log_int("multiplayer: tunic   tile mode", (int)tunic_ld32(gx2 + GX2_SURFACE_TILE_MODE));
}

/* Reads an HD texture object's surface; 0 when it cannot be recoloured. */
static int tunic_read_surface(u32 gx2, tunic_texture* t) {
    t->gx2 = gx2;
    t->format = tunic_ld32(gx2 + GX2_SURFACE_FORMAT);
    t->image = tunic_ld32(gx2 + GX2_SURFACE_IMAGE);
    t->image_size = tunic_ld32(gx2 + GX2_SURFACE_IMAGE_SIZE);
    t->mip = tunic_ld32(gx2 + GX2_SURFACE_MIP);
    t->mip_size = t->mip ? tunic_ld32(gx2 + GX2_SURFACE_MIP_SIZE) : 0;
    if (!t->mip_size) t->mip = 0;
    return tc_format_supported(t->format) && t->image && t->image_size &&
           t->image_size <= TUNIC_MAX_BYTES && t->mip_size <= TUNIC_MAX_BYTES - t->image_size;
}

static int tunic_same_surface(const tunic_texture* a, const tunic_texture* b) {
    return a->format == b->format && a->image == b->image && a->image_size == b->image_size &&
           a->mip == b->mip && a->mip_size == b->mip_size;
}

static void tunic_forget_clones(void) {
    for (u32 s = 0; s != TUNIC_MAX_SLOTS; ++s)
        for (u32 t = 0; t != TUNIC_MAX_TEXTURES; ++t) tunic_clones[s][t].ready = 0;
}

/* Copies the pristine data into dst (image, mip) and recolours it for look. */
static void tunic_render(const tunic_texture* t, const tunic_look* look, u32 image, u32 mip) {
    memcpy((void*)image, t->pristine, t->image_size);
    if (t->mip_size) memcpy((void*)mip, t->pristine + t->image_size, t->mip_size);
    if (!look->custom) return;
    tc_palette p = tc_palette_make(tc_565_to_888(look->primary), tc_565_to_888(look->secondary));
    tc_recolor_surface(&p, t->format, (u8*)image, t->image_size);
    if (t->mip_size) tc_recolor_surface(&p, t->format, (u8*)mip, t->mip_size);
}

static void tunic_write_local(tunic_texture* t, const tunic_look* look) {
    tunic_render(t, look, t->image, t->mip);
    t->live_hash = tunic_hash(t->image, t->image_size);
}

/* Takes a pristine copy of a texture seen for the first time. */
static int tunic_capture(tunic_texture* t) {
    t->pristine = (u8*)wwhd_malloc(t->image_size + t->mip_size);
    if (!t->pristine) {
        wwhd_log("multiplayer: tunic texture copy does not fit the mod heap, colours disabled");
        return 0;
    }
    memcpy(t->pristine, (const void*)t->image, t->image_size);
    if (t->mip_size) memcpy(t->pristine + t->image_size, (const void*)t->mip, t->mip_size);
    t->live_hash = tunic_hash(t->image, t->image_size);
    return 1;
}

/* Periodic check from the local Link's step: finds the hero texture, keeps the pristine copy
 * in step with the game's memory and applies the wanted local colours. */
static void tunic_refresh(u32 local) {
    if (tunic_disabled || !local) return;
    int rewrite = 0;
    u32 found[TUNIC_MAX_TEXTURES];
    u32 count = tunic_find_hero(local, found);
    if (!count) return;

    tunic_texture now[TUNIC_MAX_TEXTURES];
    for (u32 i = 0; i != count; ++i) {
        if (!tunic_read_surface(found[i], &now[i])) {
            tunic_log_texture("multiplayer: tunic texture not recolourable, colours disabled", found[i]);
            tunic_disabled = 1;
            return;
        }
    }

    int same = count == tunic_texture_count;
    for (u32 i = 0; same && i != count; ++i) same = tunic_same_surface(&now[i], &tunic_textures[i]);
    if (!same) {
        /* A surface we already copied keeps its pristine data (its memory may hold our colours). */
        tunic_texture next[TUNIC_MAX_TEXTURES];
        int carried = 0;
        for (u32 i = 0; i != count; ++i) {
            next[i] = now[i];
            next[i].pristine = 0;
            for (u32 k = 0; k != tunic_texture_count; ++k)
                if (tunic_textures[k].pristine && tunic_same_surface(&now[i], &tunic_textures[k])) {
                    next[i].pristine = tunic_textures[k].pristine;
                    next[i].live_hash = tunic_textures[k].live_hash;
                    tunic_textures[k].pristine = 0;
                    carried = 1;
                }
        }
        for (u32 k = 0; k != tunic_texture_count; ++k)
            if (tunic_textures[k].pristine) wwhd_free(tunic_textures[k].pristine);
        tunic_texture_count = 0;
        for (u32 i = 0; i != count; ++i) {
            if (!next[i].pristine) {
                tunic_log_texture("multiplayer: tunic texture found", next[i].gx2);
                if (!tunic_capture(&next[i])) {
                    for (u32 k = 0; k != i; ++k) wwhd_free(next[k].pristine);
                    tunic_disabled = 1;
                    return;
                }
            }
            tunic_textures[i] = next[i];
        }
        tunic_texture_count = count;
        tunic_forget_clones();
        /* freshly copied textures hold the original colours; a kept one may hold ours */
        if (carried) rewrite = 1;
        else tunic_local_applied = tunic_look_make(TUNIC_DEFAULT_PRIMARY, TUNIC_DEFAULT_SECONDARY);
    }

    /* The game reloaded the texture data (new pristine content at the same address). */
    for (u32 i = 0; i != count; ++i) {
        tunic_texture* t = &tunic_textures[i];
        if (tunic_hash(t->image, t->image_size) == t->live_hash) continue;
        memcpy(t->pristine, (const void*)t->image, t->image_size);
        if (t->mip_size) memcpy(t->pristine + t->image_size, (const void*)t->mip, t->mip_size);
        t->live_hash = tunic_hash(t->image, t->image_size);
        tunic_forget_clones();
        tunic_local_applied = tunic_look_make(TUNIC_DEFAULT_PRIMARY, TUNIC_DEFAULT_SECONDARY);
        rewrite |= tunic_local_wanted.custom;
        wwhd_log("multiplayer: tunic texture reloaded by the game");
    }

    if (!rewrite && tunic_same_look(&tunic_local_wanted, &tunic_local_applied)) return;
    for (u32 i = 0; i != count; ++i) tunic_write_local(&tunic_textures[i], &tunic_local_wanted);
    tunic_local_applied = tunic_local_wanted;
    wwhd_log(tunic_local_wanted.custom ? "multiplayer: tunic colours applied" : "multiplayer: original tunic restored");
}

static void tunic_set_local(u16 primary, u16 secondary) {
    tunic_local_wanted = tunic_look_make(primary, secondary);
}

/* ------------------------------------------------------------------ remote puppets */

static u8* tunic_alloc_congruent(u32 size, u32 like, u32* aligned) {
    u8* raw = (u8*)wwhd_malloc(size + TUNIC_ADDRESS_SPAN);
    if (!raw) return 0;
    u32 base = (u32)raw;
    *aligned = base + ((like - base) & (TUNIC_ADDRESS_SPAN - 1));
    return raw;
}

static void tunic_free_clone(tunic_clone* c) {
    if (c->gx2) wwhd_free(c->gx2);
    if (c->image_raw) wwhd_free(c->image_raw);
    if (c->mip_raw) wwhd_free(c->mip_raw);
    c->gx2 = c->image_raw = c->mip_raw = 0;
    c->ready = 0;
}

/* Builds or updates slot's copy of texture t for look. */
static int tunic_build_clone(tunic_clone* c, const tunic_texture* t, const tunic_look* look) {
    if (c->ready && c->source_image == t->image && tunic_same_look(&c->look, look)) return 1;
    if (!c->gx2 || c->source_image != t->image) {
        tunic_free_clone(c);
        c->gx2 = (u8*)wwhd_malloc(HD_TEXTURE_BYTES);
        c->image_raw = tunic_alloc_congruent(t->image_size, t->image, &c->image);
        if (t->mip_size) c->mip_raw = tunic_alloc_congruent(t->mip_size, t->mip, &c->mip);
        if (!c->gx2 || !c->image_raw || (t->mip_size && !c->mip_raw)) {
            tunic_free_clone(c);
            return 0;
        }
    }
    c->source_image = t->image;
    tunic_render(t, look, c->image, c->mip);
    memcpy(c->gx2, (const void*)t->gx2, HD_TEXTURE_BYTES);
    tunic_st32((u32)c->gx2 + GX2_SURFACE_IMAGE, c->image);
    tunic_st32((u32)c->gx2 + GX2_SURFACE_MIP, t->mip_size ? c->mip : 0);
    c->look = *look;
    c->ready = 1;
    return 1;
}

/* Called each step for a created puppet: prepares its texture copies when its colours differ
 * from the local ones; a slot without a copy draws with the shared (local) texture. */
static void tunic_prepare_slot(int slot, const tunic_look* look) {
    if (!tunic_remote_enabled || tunic_disabled || tunic_clone_failed || slot < 0 || slot >= TUNIC_MAX_SLOTS) return;
    for (u32 i = 0; i != tunic_texture_count; ++i) {
        tunic_clone* c = &tunic_clones[slot][i];
        if (tunic_same_look(look, &tunic_local_applied)) {
            c->ready = 0;
            continue;
        }
        if (!tunic_build_clone(c, &tunic_textures[i], look)) {
            wwhd_log("multiplayer: no mod heap left for remote tunic textures, remote colours disabled");
            tunic_clone_failed = 1;
            return;
        }
    }
}

/* PowerPC slw/srw: the shift amount is taken modulo 64, 32..63 give 0. */
static inline u32 tunic_slw(u32 v, u32 s) { s &= 0x3F; return s >= 32 ? 0 : v << s; }
static inline u32 tunic_srw(u32 v, u32 s) { s &= 0x3F; return s >= 32 ? 0 : v >> s; }

/* Dirty mask update after a header change, exactly as playerInit after setResTIMG
 * (d_a_player_main_04b.cpp): the 64-bit seed shifted by the entry index into the 128-bit mask. */
static void tunic_mark_dirty(u32 table, u32 index) {
    u32 hi = tunic_ld32(table + TEX_TABLE_SEED), lo = tunic_ld32(table + TEX_TABLE_SEED + 4);
    u32 sh = index < 64 ? index : index - 64;
    u32 mask = table + TEX_TABLE_DIRTY + (index < 64 ? 0 : 8);
    u32 high = tunic_slw(hi, sh) | tunic_srw(lo, 32 - sh) | tunic_slw(lo, sh + 32);
    tunic_st32(mask, tunic_ld32(mask) | high);
    tunic_st32(mask + 4, tunic_ld32(mask + 4) | tunic_slw(lo, sh));
}

/* Around a puppet's daPy_lk_c::draw: its texture copies replace the hero headers' texture object. */
static void tunic_swap_begin(int slot, u32 model_data) {
    tunic_swap_count = 0;
    tunic_swap_table = 0;
    if (!tunic_remote_enabled || tunic_disabled || slot < 0 || slot >= TUNIC_MAX_SLOTS || !model_data) return;
    u32 table = tunic_ld32(model_data + MODEL_DATA_TEXTURE_TABLE);
    if (!table) return;
    u32 count = *(const volatile u16*)table;
    u32 res = tunic_ld32(table + 4);
    if (!res) return;
    if (count > TEX_TABLE_MAX) count = TEX_TABLE_MAX;
    for (u32 j = 0; j != count; ++j) {
        u32 header = res + j * TEX_HEADER_BYTES;
        u32 gx2 = tunic_ld32(header + TEX_HEADER_HD);
        for (u32 i = 0; i != tunic_texture_count; ++i) {
            const tunic_clone* c = &tunic_clones[slot][i];
            if (!c->ready || gx2 != tunic_textures[i].gx2) continue;
            tunic_swap_index[tunic_swap_count] = j;
            tunic_swap_original[tunic_swap_count++] = gx2;
            tunic_st32(header + TEX_HEADER_HD, (u32)c->gx2);
            tunic_mark_dirty(table, j);
            break;
        }
    }
    if (tunic_swap_count) tunic_swap_table = table;
}

static void tunic_swap_end(void) {
    if (!tunic_swap_table) return;
    u32 res = tunic_ld32(tunic_swap_table + 4);
    for (u32 k = 0; k != tunic_swap_count; ++k) {
        tunic_st32(res + tunic_swap_index[k] * TEX_HEADER_BYTES + TEX_HEADER_HD, tunic_swap_original[k]);
        tunic_mark_dirty(tunic_swap_table, tunic_swap_index[k]);
    }
    tunic_swap_count = 0;
    tunic_swap_table = 0;
}
