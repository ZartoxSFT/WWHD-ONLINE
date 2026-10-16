/* Boat paint colour: the red/orange parts of the King of Red Lions' textures, recoloured like the
 * tunic (tunic.h, tunic_color.h boat rule, the same as the launcher's boat preview).
 *
 * The ship (daShip_c) draws a body and a head model: mpBodyAnm (+0x3B4) and mpHeadAnm (+0x3B8) are
 * mDoExt_McaMorf, whose model is at +0x90 (decomp include/m_Do/m_Do_ext.h); J3DModel::mModelData is
 * at +0xAC (include/JSystem/J3DGraph.h) and the model data points to its texture table at +0x30
 * (ModelData::mTexture, d_a_player_main_04b.h: {u16 count, u32 headers}, 0x24 bytes per header, HD
 * texture object at +0x20, GX2Surface inside it at +4). Every texture with red/orange paint gets a pristine copy in
 * the mod heap and is recoloured in place; the renderer re-uploads written texture memory.
 *
 * Textures are only written while the ship's model data still points to them (rescanned before
 * every write): the archive memory of a ship left behind may be reused by the game. One no longer
 * used keeps its copy (never written) until the space is needed: the ship may come back with the
 * same memory, still painted by us. A texture the game reloaded (hash of its first bytes changed)
 * is copied again.
 *
 * A texture the local Link's model data also uses (mpCLModelData +0x444, the same texture table at
 * +0x30) is never painted: in 0.2.10 the boat colour darkened Link (one of the two 1024x1024 BC1
 * textures the ship lists, 24FBC164, lies outside the ship archive). Each texture is logged with its
 * name: model data +0x34 is the JUTNameTab (ShipCreateModelTextures::mTextureName,
 * d_a_ship_create.cpp), read with 027ED1F0 as daShip_replaceSeaETextures and playerInit do. */
#pragma once
#include "wwhd_guest.h"
#include "tunic.h"

#define BOAT_BODY_MORF 0x3B4
#define BOAT_HEAD_MORF 0x3B8
#define BOAT_MORF_MODEL 0x90
#define BOAT_MODEL_DATA 0xAC
#define BOAT_MAX_TEXTURES 24
#define BOAT_TEXTURE_MAX_BYTES (2u << 20)  /* image + mips of one texture */
#define BOAT_BUDGET_BYTES (4u << 20)       /* all pristine copies (heap_size 8 MiB, tunic included) */
#define BOAT_SCAN_PERIOD 15                /* ship executes between two scans (0.5 s) */
#define BOAT_PAINT_MIN_SHARE 10            /* per mille of paint blocks to call a texture painted */
#define MODEL_DATA_TEXTURE_NAMES 0x34      /* JUTNameTab* (d_a_ship_create.h mTextureName) */
#define LINK_MODEL_DATA 0x444              /* daPy_lk_c::mpCLModelData (SDK link.h) */

WWHD_GAME_FUNC(0x027ED1F0, u32, boat_texture_name, (u32 names, u32 index));

typedef struct boat_texture {
    tunic_texture t;     /* surface, pristine copy, live hash */
    int seen;            /* found by the last scan: the ship uses it, it may be written */
} boat_texture;

static boat_texture boat_textures[BOAT_MAX_TEXTURES];
static u32 boat_texture_count;
static u32 boat_budget_used;
static int boat_wanted_custom, boat_applied_custom;
static u32 boat_wanted_rgb, boat_applied_rgb;
static int boat_scan_clock;
static int boat_budget_logged;
static int boat_remote_wanted; /* a remote ship wants another paint: the textures are copied too */

/* Per mille of the colour blocks / pixels of the live image that are red/orange paint. */
static u32 boat_paint_share(const tunic_texture* t, const tc_palette* p) {
    tc_u32 hw = t->format & 0x3Fu, step = hw == TC_FMT_BC1 ? 8u : hw == TC_FMT_8_8_8_8 ? 4u : 16u;
    tc_u32 offset = hw == TC_FMT_BC2 || hw == TC_FMT_BC3 ? 8u : 0u, out;
    const volatile u8* data = (const volatile u8*)t->image;
    u32 total = 0, paint = 0;
    for (u32 i = 0; i + step <= t->image_size; i += step) {
        const volatile u8* b = data + i + offset;
        ++total;
        if (hw == TC_FMT_8_8_8_8) {
            paint += (u32)tc_recolor_rgb(p, ((tc_u32)b[0] << 16) | ((tc_u32)b[1] << 8) | b[2], &out);
        } else {
            tc_u16 c0 = (tc_u16)(b[0] | (b[1] << 8)), c1 = (tc_u16)(b[2] | (b[3] << 8));
            paint += (u32)(tc_recolor_rgb(p, tc_565_to_888(c0), &out) || tc_recolor_rgb(p, tc_565_to_888(c1), &out));
        }
    }
    return total ? paint * 1000u / total : 0; /* at most 2 MiB / 8 blocks: no overflow */
}

/* Every texture of the ship is logged once: what it is and how much of it is paint. */
#define BOAT_LOGGED_MAX 32
static u32 boat_logged[BOAT_LOGGED_MAX];
static u32 boat_logged_count;

static void boat_log_texture(u32 gx2, u32 name, int supported, u32 share) {
    for (u32 i = 0; i != boat_logged_count; ++i)
        if (boat_logged[i] == gx2) return;
    if (boat_logged_count != BOAT_LOGGED_MAX) boat_logged[boat_logged_count++] = gx2;
    wwhd_log_hex("multiplayer: boat texture", gx2);
    if (name) wwhd_log((const char*)name);
    wwhd_log_hex("multiplayer: boat   format", tunic_ld32(gx2 + GX2_SURFACE_FORMAT));
    wwhd_log_int("multiplayer: boat   width", (int)tunic_ld32(gx2 + GX2_SURFACE_WIDTH));
    wwhd_log_int("multiplayer: boat   height", (int)tunic_ld32(gx2 + GX2_SURFACE_HEIGHT));
    wwhd_log_int("multiplayer: boat   bytes", (int)tunic_ld32(gx2 + GX2_SURFACE_IMAGE_SIZE));
    wwhd_log_int("multiplayer: boat   recolourable format", supported);
    if (supported) wwhd_log_int("multiplayer: boat   paint per mille", (int)share);
}

/* Frees the copy of a texture the ship no longer uses; 0 when there is none. */
static int boat_evict_one(void) {
    for (u32 i = 0; i != boat_texture_count; ++i) {
        if (boat_textures[i].seen) continue;
        boat_budget_used -= boat_textures[i].t.image_size + boat_textures[i].t.mip_size;
        wwhd_free(boat_textures[i].t.pristine);
        boat_textures[i] = boat_textures[--boat_texture_count];
        return 1;
    }
    return 0;
}

static boat_texture* boat_find(const tunic_texture* t) {
    for (u32 i = 0; i != boat_texture_count; ++i)
        if (tunic_same_surface(&boat_textures[i].t, t) && boat_textures[i].t.gx2 == t->gx2)
            return &boat_textures[i];
    return 0;
}

/* 1 when the local Link's model data lists this texture object or its image. */
static int boat_used_by_link(u32 gx2, u32 image) {
    u32 link = puppet_local_player();
    u32 data = link ? tunic_ld32(link + LINK_MODEL_DATA) : 0;
    u32 table = data ? tunic_ld32(data + MODEL_DATA_TEXTURE_TABLE) : 0;
    if (!table) return 0;
    u32 count = *(const volatile u16*)table;
    u32 headers = tunic_ld32(table + 4);
    if (!headers || count > TEX_TABLE_MAX) return 0;
    for (u32 k = 0; k != count; ++k) {
        u32 other = tunic_ld32(headers + k * TEX_HEADER_BYTES + TEX_HEADER_HD);
        if (!other || (other & 3)) continue;
        if (other == gx2 || tunic_ld32(other + GX2_SURFACE_IMAGE) == image) return 1;
    }
    return 0;
}

/* Adds the painted textures of one model; returns 1 when a new one was copied. */
static int boat_scan_model(u32 model, const tc_palette* reference) {
    if (!model) return 0;
    u32 data = tunic_ld32(model + BOAT_MODEL_DATA);
    if (!data) return 0;
    /* +0x30 holds a POINTER to the TextureTable (decomp d_a_player_main_04b.h ModelData::mTexture,
     * read by playerInit as getTexture()); 0.2.9 read the pointer as the table ("9050 textures") */
    u32 table = tunic_ld32(data + MODEL_DATA_TEXTURE_TABLE);
    if (!table) return 0;
    u32 count = *(const volatile u16*)table;
    u32 headers = tunic_ld32(table + 4);
    u32 names = tunic_ld32(data + MODEL_DATA_TEXTURE_NAMES);
    static u32 logged_data[2];
    if (logged_data[0] != data && logged_data[1] != data) {
        logged_data[logged_data[0] ? 1 : 0] = data;
        wwhd_log_int("multiplayer: boat model textures", (int)count);
    }
    if (!headers || count > TEX_TABLE_MAX) return 0;
    int added = 0;
    for (u32 k = 0; k != count; ++k) {
        u32 gx2 = tunic_ld32(headers + k * TEX_HEADER_BYTES + TEX_HEADER_HD);
        if (!gx2 || (gx2 & 3)) continue;
        tunic_texture t;
        int supported = tunic_read_surface(gx2, &t);
        u32 name = names ? boat_texture_name(names, k) : 0;
        if (!supported || t.image_size + t.mip_size > BOAT_TEXTURE_MAX_BYTES) {
            boat_log_texture(gx2, name, 0, 0);
            continue;
        }
        if (boat_used_by_link(gx2, t.image)) {
            if (!boat_find(&t)) {
                boat_log_texture(gx2, name, 1, 0);
                static u32 shared_logged;
                if (shared_logged != gx2) wwhd_log_hex("multiplayer: boat texture shared with Link, left as is", gx2);
                shared_logged = gx2;
            }
            continue;
        }
        boat_texture* known = boat_find(&t);
        if (known) {
            known->seen = 1;
            continue;
        }
        u32 share = boat_paint_share(&t, reference);
        boat_log_texture(gx2, name, 1, share);
        if (share < BOAT_PAINT_MIN_SHARE) continue;
        while ((boat_texture_count == BOAT_MAX_TEXTURES ||
                boat_budget_used + t.image_size + t.mip_size > BOAT_BUDGET_BYTES) && boat_evict_one()) {}
        if (boat_texture_count == BOAT_MAX_TEXTURES ||
            boat_budget_used + t.image_size + t.mip_size > BOAT_BUDGET_BYTES) {
            if (!boat_budget_logged) wwhd_log("multiplayer: boat texture budget full, some paint keeps its colour");
            boat_budget_logged = 1;
            continue;
        }
        t.pristine = (u8*)wwhd_malloc(t.image_size + t.mip_size);
        if (!t.pristine) continue;
        memcpy(t.pristine, (const void*)t.image, t.image_size);
        if (t.mip_size) memcpy(t.pristine + t.image_size, (const void*)t.mip, t.mip_size);
        t.live_hash = tunic_hash(t.image, t.image_size);
        boat_budget_used += t.image_size + t.mip_size;
        boat_textures[boat_texture_count].t = t;
        boat_textures[boat_texture_count].seen = 1;
        ++boat_texture_count;
        wwhd_log_hex("multiplayer: boat painted texture", gx2);
        wwhd_log_hex("multiplayer: boat   format", t.format);
        wwhd_log_int("multiplayer: boat   width", (int)tunic_ld32(gx2 + GX2_SURFACE_WIDTH));
        added = 1;
    }
    return added;
}

static u32 boat_morf_model(u32 ship, u32 offset) {
    u32 morf = tunic_ld32(ship + offset);
    return morf ? tunic_ld32(morf + BOAT_MORF_MODEL) : 0;
}

/* Writes the wanted paint into every texture the ship uses now. */
static void boat_write_all(void) {
    tc_palette p = tc_palette_make_boat(boat_wanted_rgb);
    for (u32 i = 0; i != boat_texture_count; ++i) {
        if (!boat_textures[i].seen) continue;
        tunic_texture* t = &boat_textures[i].t;
        memcpy((void*)t->image, t->pristine, t->image_size);
        if (t->mip_size) memcpy((void*)t->mip, t->pristine + t->image_size, t->mip_size);
        if (boat_wanted_custom) {
            tc_recolor_surface(&p, t->format, (u8*)t->image, t->image_size);
            if (t->mip_size) tc_recolor_surface(&p, t->format, (u8*)t->mip, t->mip_size);
        }
        t->live_hash = tunic_hash(t->image, t->image_size);
    }
    boat_applied_custom = boat_wanted_custom;
    boat_applied_rgb = boat_wanted_rgb;
}

static void boat_set_paint(int custom, u32 rgb) {
    boat_wanted_custom = custom;
    boat_wanted_rgb = rgb;
}

/* From the ship's execute: rescans its textures every BOAT_SCAN_PERIOD calls (at once when the
 * colour changed) and keeps them painted. */
static void boat_refresh(u32 ship) {
    if (!ship) return;
    int changed = boat_wanted_custom != boat_applied_custom ||
                  (boat_wanted_custom && boat_wanted_rgb != boat_applied_rgb);
    if (!changed && --boat_scan_clock > 0) return;
    boat_scan_clock = BOAT_SCAN_PERIOD;
    /* never painted and no remote ship in another paint: nothing to do */
    if (!boat_wanted_custom && !boat_applied_custom && !boat_remote_wanted) return;

    for (u32 i = 0; i != boat_texture_count; ++i) boat_textures[i].seen = 0;
    tc_palette reference = tc_palette_make_boat(TC_BOAT_RED);
    int added = boat_scan_model(boat_morf_model(ship, BOAT_BODY_MORF), &reference);
    added |= boat_scan_model(boat_morf_model(ship, BOAT_HEAD_MORF), &reference);

    /* the game reloaded one: its memory holds the original again */
    int reloaded = 0;
    for (u32 i = 0; i != boat_texture_count; ++i) {
        if (!boat_textures[i].seen) continue;
        tunic_texture* t = &boat_textures[i].t;
        if (tunic_hash(t->image, t->image_size) == t->live_hash) continue;
        memcpy(t->pristine, (const void*)t->image, t->image_size);
        if (t->mip_size) memcpy(t->pristine + t->image_size, (const void*)t->mip, t->mip_size);
        t->live_hash = tunic_hash(t->image, t->image_size);
        reloaded = 1;
    }
    if (changed || added || reloaded) {
        boat_write_all();
        if (changed) wwhd_log(boat_wanted_custom ? "multiplayer: boat paint applied" : "multiplayer: original boat paint restored");
    }
}

/* ------------------------------------------------------------------ remote ships' own paint
 *
 * The ship textures are shared by every ship (the puppets use the ship archive's model data), so
 * a remote ship in another paint gets its own copy of each painted texture: the HD texture object
 * (0xC0 bytes, HD_TEXTURE_BYTES) and its image and mips at the same address modulo 64 KiB
 * (tiling), made from the pristine copy and recoloured with that player's paint (none: the
 * original). While that puppet runs daShip_Draw its copies replace the texture objects in the
 * shared texture tables of the body and head model data, with the table's dirty mask set like
 * the game's own setResTIMG (tunic_mark_dirty), and the originals come back after. As for the
 * remote tunics (tunic.h), whether the HD J3D binds the texture while the model is entered
 * (daShip_Draw) or later is not in the sources: if later, every ship shows the local paint. The
 * mod heap (8 MiB at most, runtime/src/mods/packages.cpp) holds the copies of about three ships;
 * a ship without room keeps the shared paint (logged). */
#define BOAT_CLONE_SLOTS 4 /* SHIP_PUPPET_MAX */
#define BOAT_SWAP_MAX 16

typedef struct boat_clone {
    int ready;
    u32 source_gx2;     /* the shared texture object it stands for */
    int custom;
    u32 rgb;
    u8* gx2;            /* texture object copy */
    u8* image_raw;
    u8* mip_raw;
    u32 image, mip;
} boat_clone;

static boat_clone boat_clones[BOAT_CLONE_SLOTS][BOAT_MAX_TEXTURES];
static int boat_clone_full[BOAT_CLONE_SLOTS];
static u32 boat_swap_table[BOAT_SWAP_MAX], boat_swap_index[BOAT_SWAP_MAX], boat_swap_original[BOAT_SWAP_MAX];
static u32 boat_swap_count;

static void boat_free_clone(boat_clone* c) {
    if (c->gx2) wwhd_free(c->gx2);
    if (c->image_raw) wwhd_free(c->image_raw);
    if (c->mip_raw) wwhd_free(c->mip_raw);
    c->gx2 = c->image_raw = c->mip_raw = 0;
    c->ready = 0;
    c->source_gx2 = 0;
}

static void boat_free_slot(int slot) {
    if (slot < 0 || slot >= BOAT_CLONE_SLOTS) return;
    for (u32 k = 0; k != BOAT_MAX_TEXTURES; ++k) boat_free_clone(&boat_clones[slot][k]);
    boat_clone_full[slot] = 0;
}

static boat_clone* boat_clone_for(int slot, u32 gx2, int make) {
    boat_clone* empty = 0;
    for (u32 k = 0; k != BOAT_MAX_TEXTURES; ++k) {
        boat_clone* c = &boat_clones[slot][k];
        if (c->gx2 && c->source_gx2 == gx2) return c;
        if (!c->gx2 && !empty) empty = c;
    }
    return make ? empty : 0;
}

/* Keeps slot's copies in paint (custom, rgb); no copies while it is the local paint. */
static void boat_prepare_slot(int slot, int custom, u32 rgb) {
    if (slot < 0 || slot >= BOAT_CLONE_SLOTS) return;
    int same = custom == boat_applied_custom && (!custom || rgb == boat_applied_rgb);
    if (same) {
        for (u32 k = 0; k != BOAT_MAX_TEXTURES; ++k) boat_clones[slot][k].ready = 0;
        return;
    }
    boat_remote_wanted = 1;
    if (boat_clone_full[slot]) return;
    /* copies of textures the ship no longer uses go first */
    for (u32 k = 0; k != BOAT_MAX_TEXTURES; ++k) {
        boat_clone* c = &boat_clones[slot][k];
        if (!c->gx2) continue;
        int used = 0;
        for (u32 i = 0; i != boat_texture_count; ++i) used |= boat_textures[i].seen && boat_textures[i].t.gx2 == c->source_gx2;
        if (!used) boat_free_clone(c);
    }
    tc_palette p = tc_palette_make_boat(rgb);
    for (u32 i = 0; i != boat_texture_count; ++i) {
        if (!boat_textures[i].seen) continue;
        const tunic_texture* t = &boat_textures[i].t;
        boat_clone* c = boat_clone_for(slot, t->gx2, 1);
        if (!c) return;
        if (c->ready && c->custom == custom && c->rgb == rgb) continue;
        if (!c->gx2) {
            c->gx2 = (u8*)wwhd_malloc(HD_TEXTURE_BYTES);
            c->image_raw = tunic_alloc_congruent(t->image_size, t->image, &c->image);
            if (t->mip_size) c->mip_raw = tunic_alloc_congruent(t->mip_size, t->mip, &c->mip);
            if (!c->gx2 || !c->image_raw || (t->mip_size && !c->mip_raw)) {
                boat_free_slot(slot);
                boat_clone_full[slot] = 1;
                wwhd_log_int("multiplayer: no mod heap left for one more ship paint, shared paint kept for ship", slot);
                return;
            }
            c->source_gx2 = t->gx2;
        }
        memcpy((void*)c->image, t->pristine, t->image_size);
        if (t->mip_size) memcpy((void*)c->mip, t->pristine + t->image_size, t->mip_size);
        if (custom) {
            tc_recolor_surface(&p, t->format, (u8*)c->image, t->image_size);
            if (t->mip_size) tc_recolor_surface(&p, t->format, (u8*)c->mip, t->mip_size);
        }
        memcpy(c->gx2, (const void*)t->gx2, HD_TEXTURE_BYTES);
        tunic_st32((u32)c->gx2 + GX2_SURFACE_IMAGE, c->image);
        tunic_st32((u32)c->gx2 + GX2_SURFACE_MIP, t->mip_size ? c->mip : 0);
        c->custom = custom;
        c->rgb = rgb;
        c->ready = 1;
        static int logged;
        if (!logged) {
            logged = 1;
            wwhd_log("multiplayer: a remote ship has its own paint copies");
        }
    }
}

/* Around a puppet ship's daShip_Draw: its copies in the shared tables of one model. */
static void boat_swap_model(int slot, u32 model) {
    u32 data = model ? tunic_ld32(model + BOAT_MODEL_DATA) : 0;
    u32 table = data ? tunic_ld32(data + MODEL_DATA_TEXTURE_TABLE) : 0;
    if (!table) return;
    u32 count = *(const volatile u16*)table;
    u32 headers = tunic_ld32(table + 4);
    if (!headers || count > TEX_TABLE_MAX) return;
    for (u32 j = 0; j != count && boat_swap_count != BOAT_SWAP_MAX; ++j) {
        u32 header = headers + j * TEX_HEADER_BYTES;
        u32 gx2 = tunic_ld32(header + TEX_HEADER_HD);
        boat_clone* c = gx2 ? boat_clone_for(slot, gx2, 0) : 0;
        if (!c || !c->ready) continue;
        boat_swap_table[boat_swap_count] = table;
        boat_swap_index[boat_swap_count] = j;
        boat_swap_original[boat_swap_count++] = gx2;
        tunic_st32(header + TEX_HEADER_HD, (u32)c->gx2);
        tunic_mark_dirty(table, j);
    }
}

static void boat_swap_begin(int slot, u32 ship) {
    boat_swap_count = 0;
    if (slot < 0 || slot >= BOAT_CLONE_SLOTS || !ship) return;
    boat_swap_model(slot, boat_morf_model(ship, BOAT_BODY_MORF));
    boat_swap_model(slot, boat_morf_model(ship, BOAT_HEAD_MORF));
}

static void boat_swap_end(void) {
    for (u32 k = 0; k != boat_swap_count; ++k) {
        u32 headers = tunic_ld32(boat_swap_table[k] + 4);
        tunic_st32(headers + boat_swap_index[k] * TEX_HEADER_BYTES + TEX_HEADER_HD, boat_swap_original[k]);
        tunic_mark_dirty(boat_swap_table[k], boat_swap_index[k]);
    }
    boat_swap_count = 0;
}
