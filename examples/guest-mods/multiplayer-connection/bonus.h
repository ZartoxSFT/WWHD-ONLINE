/* Yellow hearts and yellow magic: bonuses bought in the launcher's shop (mod.c "the launcher's
 * shop", items SHOP_YELLOW_HEART / SHOP_YELLOW_MAGIC), this player's only.
 *
 * Like Breath of the Wild's temporary hearts: on top of the game's life and magic, spent first and
 * never refilled. Kept in this mod's Data folder (bonus.cfg: "hearts=" quarter hearts, "magic="
 * units) until spent, so neither the save nor its synchronisation (save_sync.h shares the heart and
 * magic meters) knows them.
 *
 * Spent first (decomp, verified): every change of life and magic goes through pending counters of
 * dComIfG_play_c, mItemLifeCount (play + 0x5B44, f32 quarter hearts: daPy_lk_c::setDamagePoint
 * 023F51D0 adds the damage there, hero mode doubled, d_a_player_main_03.cpp) and mItemMagicCount
 * (play + 0x5B60, s16: the arrows' cost, d_a_arrow.cpp, and every other user), which dMeter_Execute
 * (d_meter_01.cpp) adds to the save and clears. The yellow hearts take what setDamagePoint adds (the
 * damage taken; other losses, as a fall into the void, are not covered); the yellow magic takes a
 * negative magic count just before dMeter_magicGaugeMove applies it.
 * Limit: an item the game refuses for want of magic (save mMagic below its cost) is still refused
 * while the bonus holds some: the game tests the meter before spending.
 *
 * Drawn with the game's own heart and magic meter, recoloured yellow, at the places of the HD HUD's
 * panes (see "drawing" below). Not drawn while the HUD is hidden, during an event or in a menu. */
#pragma once
#include "wwhd_guest.h"
#include "wwhd/save.h"

#define BONUS_HEARTS_MAX 20      /* quarter hearts: 5 hearts */
#define BONUS_HEART_STEP 4       /* one heart a purchase */
#define BONUS_MAGIC_MAX 16       /* one small magic meter */
#define BONUS_MAGIC_STEP 4       /* a quarter of it a purchase */
#define BONUS_PLAY_LIFE 0x5B44   /* dComIfG_play_c::mItemLifeCount, f32 */
#define BONUS_PLAY_MAGIC 0x5B60  /* dComIfG_play_c::mItemMagicCount, s16 */
#define BONUS_SAVE_MAX_MAGIC 0x13 /* dSv_player_status_a_c::mMaxMagic (d_save.h) */
#define BONUS_WRITE_STEPS 30     /* a spent bonus is written at most once a second */

static int bonus_loaded;
static u32 bonus_hearts, bonus_magic;
/* The local Link, noted by its execute (mod.c multiplayer_daPy_Execute, not a puppet): actor_sync.h
 * as_link is only set while connected with the actors synchronised (0.2.55 drew nothing without). */
static u32 bonus_link;
static unsigned long long bonus_link_step;

static void bonus_note_link(void* link) {
    bonus_link = (u32)link;
    bonus_link_step = wwhd_logic_step();
}

static int bonus_link_alive(void) { return bonus_link && wwhd_logic_step() - bonus_link_step < 4; }
static int bonus_dirty;
static unsigned long long bonus_written_step;
static char bonus_text[64];

static u32 bonus_number(const char* text, u32 size, const char* key) {
    u32 klen = 0;
    while (key[klen]) ++klen;
    for (u32 i = 0; i + klen < size; ++i) {
        if ((i && text[i - 1] != '\n') || text[i + klen] != '=') continue;
        u32 k = 0;
        while (k != klen && text[i + k] == key[k]) ++k;
        if (k != klen) continue;
        u32 v = 0;
        for (u32 j = i + klen + 1; j < size && text[j] >= '0' && text[j] <= '9' && v < 100000u; ++j)
            v = v * 10u + (u32)(text[j] - '0');
        return v;
    }
    return 0;
}

static void bonus_load(void) {
    if (bonus_loaded) return;
    bonus_loaded = 1;
    s32 got = wwhd_file_read("bonus.cfg", bonus_text, sizeof(bonus_text));
    if (got <= 0) return;
    bonus_hearts = bonus_number(bonus_text, (u32)got, "hearts");
    bonus_magic = bonus_number(bonus_text, (u32)got, "magic");
    if (bonus_hearts > BONUS_HEARTS_MAX) bonus_hearts = BONUS_HEARTS_MAX;
    if (bonus_magic > BONUS_MAGIC_MAX) bonus_magic = BONUS_MAGIC_MAX;
}

static void bonus_put(char* out, u32* n, const char* key, u32 v) {
    while (*key) out[(*n)++] = *key++;
    char digits[8];
    int d = 0;
    do { digits[d++] = (char)('0' + v % 10u); v /= 10u; } while (v && d != 8);
    while (d) out[(*n)++] = digits[--d];
    out[(*n)++] = '\n';
}

static void bonus_write(void) {
    u32 n = 0;
    bonus_put(bonus_text, &n, "hearts=", bonus_hearts);
    bonus_put(bonus_text, &n, "magic=", bonus_magic);
    wwhd_file_write("bonus.cfg", bonus_text, n);
    bonus_dirty = 0;
    bonus_written_step = wwhd_logic_step();
}

/* The local Link's step (mod.c map_step): what was spent, written now and then. */
static void bonus_tick(void) {
    bonus_load();
    if (bonus_dirty && wwhd_logic_step() - bonus_written_step >= BONUS_WRITE_STEPS) bonus_write();
}

static int bonus_save_ready(void) {
    if (!WWHD_GAME_DATA(WWHD_ADDR_save_info_pointer, u32)) return 0;
    u16 max_life = dComIfGs_player_status_a.max_life;
    return max_life >= 4 && max_life <= 80;
}

static u32 bonus_max_magic(void) {
    u32 status = WWHD_GAME_DATA(WWHD_ADDR_save_info_pointer, u32);
    return status ? *(const volatile u8*)(status + WWHD_OFFSET_save_info + BONUS_SAVE_MAX_MAGIC) : 0;
}

/* Purchases (shop_try): "ok", or why not. */
static const char* bonus_buy_heart(void) {
    bonus_load();
    if (bonus_hearts + BONUS_HEART_STEP > BONUS_HEARTS_MAX) return "full";
    bonus_hearts += BONUS_HEART_STEP;
    bonus_write();
    return "ok";
}

static const char* bonus_buy_magic(void) {
    bonus_load();
    if (!bonus_max_magic()) return "no_magic";
    if (bonus_magic + BONUS_MAGIC_STEP > BONUS_MAGIC_MAX) return "full";
    bonus_magic += BONUS_MAGIC_STEP;
    bonus_write();
    return "ok";
}

static int bonus_heart_full(void) { bonus_load(); return bonus_hearts + BONUS_HEART_STEP > BONUS_HEARTS_MAX; }
static int bonus_magic_full(void) { bonus_load(); return bonus_magic + BONUS_MAGIC_STEP > BONUS_MAGIC_MAX; }

/* daPy_lk_c::setDamagePoint (023F51D0, this and f32 amount: wwhd/bindings.h, d_a_player_main_03.cpp)
 * of the local Link: what it added to the pending life (hero mode and invincibility included) is
 * taken from the yellow hearts first. */
WWHD_GAME_ORIGINAL(0x023F51D0, s32, bonus_damage_point_original, (void* self, f32 amount));
static int dh_damage_blocked(u32 self); /* death.h: a ghost takes no damage */
WWHD_REPLACE(0x023F51D0, s32, bonus_damage_point, (void* self, f32 amount)) {
    if (dh_damage_blocked((u32)self)) return 0;
    u8* play = wwhd_play_get();
    if (!play || !bonus_hearts || !self || (u32)self != bonus_link || amount >= 0.0f)
        return bonus_damage_point_original(self, amount);
    volatile f32* life = (volatile f32*)(play + BONUS_PLAY_LIFE);
    f32 before = *life;
    s32 result = bonus_damage_point_original(self, amount);
    f32 lost = before - *life;
    if (lost <= 0.0f) return result;
    u32 need = (u32)lost;
    if ((f32)need < lost) ++need;
    u32 take = need < bonus_hearts ? need : bonus_hearts;
    f32 back = (f32)take > lost ? lost : (f32)take;
    *life = *life + back;
    bonus_hearts -= take;
    bonus_dirty = 1;
    return result;
}

/* dMeter_magicGaugeMove (0259B698, the meter: one argument, d_meter_02.cpp), called by dMeter_Execute
 * before it adds the pending magic to the save. (dMeter_Execute itself, 02593B10, differs between
 * the USA and EU builds: the runtime refuses a mod hooking it, 0.2.55's loading error.) */
WWHD_HOOK(0x0259B698, bonus_magic_gauge, (u32 meter)) {
    (void)meter;
    if (!bonus_loaded || !bonus_magic || !bonus_link_alive() || !bonus_save_ready()) return;
    u8* play = wwhd_play_get();
    if (!play) return;
    volatile s16* magic = (volatile s16*)(play + BONUS_PLAY_MAGIC);
    s16 cost = *magic;
    if (bonus_magic && cost < 0) {
        u32 need = (u32)(-(s32)cost);
        u32 take = need < bonus_magic ? need : bonus_magic;
        *magic = (s16)(cost + (s16)take);
        bonus_magic -= take;
        bonus_dirty = 1;
    }
}

/* ------------------------------------------------------------------ drawing
 *
 * With the game's own HUD art, recoloured: the HD HUD is the layout Main_00 (nw::lyt). Read in the
 * game's layouts (permanent_2d_*.pack, local analysis): its part L_HeartAll_00 (layout HeartAll_00)
 * holds the 20 heart slots L_Heart_00..19 (10 a row), each a Heart_00 with the pictures P_Base_00
 * (HeartBase_00, white ffffffc8) and P_Heart_00 (HeartFull_00, white ffffffff), origin at their
 * centre; its part L_MagicBar_00 (MagicBar_00) has N_Magic_00 (meter of 16: P_MagicBase_00 196 wide,
 * P_Magic_00 190 wide) and N_Magic_01 (meter of 32: 386 and 380), origin at their left, the meter's
 * green from the material (black 00ff00, white 00a600) over a grey texture.
 * nw::lyt::Pane (docs/decomp-notes.md, runtime story_notebook_game.cpp): +0x0C parent, +0x14 child
 * list sentinel (+0x00 sibling link), +0x3C size, +0x44 flags (bit 0 visible), +0x45 alpha, +0x48
 * global matrix 3x4 (layout space: centre origin, y up, 1 unit = 1 TV pixel), +0x80 name; a picture's
 * material at +0xF4, its first texture (GX2Texture) at material +0x28; hdParts_c: layout +0x44,
 * layout +4 the nw::lyt::Layout, whose root pane is at +0xC.
 * Found from hdParts_c::draw (026F8B54, this and viewport: runtime story_lyt_screen.cpp), the screen
 * whose tree has L_HeartAll_00. Each logic step the slots' matrices give the places; the textures are
 * decoded once (wwhd_hud_game_pixels), the heart's reds turned yellow and the meter's grey given a
 * yellow ramp, and drawn with HUD v2: the yellow hearts in the slots right after the last heart the
 * player has (the life the meter shows, play.mItemNowLife +0x5BAC, quarter hearts, d_meter_00.cpp:
 * 7 hearts -> the 8th slot, 7 and a quarter or a half -> the 9th), over the empty containers and on
 * past them (past the 20th, on to the right of the second row), the yellow meter as a second meter
 * right after the game's. */

#define BONUS_PANE_PARENT 0x0C
#define BONUS_PANE_CHILDREN 0x14
#define BONUS_PANE_SIZE 0x3C
#define BONUS_PANE_FLAGS 0x44
#define BONUS_PANE_ALPHA 0x45
#define BONUS_PANE_MTX 0x48
#define BONUS_PANE_NAME 0x80
#define BONUS_PLAY_NOW_LIFE 0x5BAC /* dComIfG_play_c::mItemNowLife (play + 0x12A0 + 0x490C), u16 */
#define BONUS_PICTURE_MATERIAL 0xF4
#define BONUS_MATERIAL_TEXTURE 0x28
#define BONUS_PARTS_LAYOUT 0x44
#define BONUS_LAYOUT_LYT 4
#define BONUS_LYT_ROOT 0xC
#define BONUS_SLOTS 20
#define BONUS_MAGIC_UNIT 11.875f /* layout units a magic point: 190 / 16 = 380 / 32 */
#define BONUS_TEXTURE_MAX (256u * 256u * 4u)

static u32 bonus_parts, bonus_root;
static u32 bonus_heart[BONUS_SLOTS], bonus_base[BONUS_SLOTS];
static u32 bonus_light[BONUS_SLOTS], bonus_gloss[BONUS_SLOTS]; /* P_Light_00 (the glow), P_Gloss_00 */
static int bonus_current = -1, bonus_light_hidden = -1;         /* the game's big heart, its hidden glow */
static f32 bonus_small_scale, bonus_big_scale, bonus_light_alpha, bonus_light_scale;
static u32 bonus_light_image;
static u32 bonus_meter[2], bonus_fill[2], bonus_frame[2]; /* N_Magic_0x, P_Magic_0x, P_MagicBase_0x */
static int bonus_missing_logged;
static u32 bonus_heart_image, bonus_base_image, bonus_fill_image, bonus_frame_image;
static unsigned long long bonus_image_epoch = ~0ull;
static u32 bonus_image_source; /* the heart texture the images were made from */
static wwhd_hud_element bonus_element;

static int bonus_ptr(u32 p) { return p >= 0x10000000u && p < 0x50000000u && !(p & 3u); }

static int bonus_name_is(u32 pane, const char* name) {
    const char* s = (const char*)(pane + BONUS_PANE_NAME);
    for (u32 i = 0; i != 24; ++i) {
        if (s[i] != name[i]) return 0;
        if (!name[i]) return 1;
    }
    return 0;
}

/* A pane of the tree by name, depth first. */
static u32 bonus_find(u32 pane, const char* name, int depth) {
    if (!bonus_ptr(pane) || depth < 0) return 0;
    if (bonus_name_is(pane, name)) return pane;
    u32 sentinel = pane + BONUS_PANE_CHILDREN, child = *(const volatile u32*)sentinel;
    for (u32 guard = 0; child && child != sentinel && guard != 256; ++guard, child = *(const volatile u32*)child) {
        u32 found = bonus_find(child, name, depth - 1);
        if (found) return found;
    }
    return 0;
}

static u32 bonus_root_of(u32 parts) {
    if (!bonus_ptr(parts)) return 0;
    u32 layout = *(const volatile u32*)(parts + BONUS_PARTS_LAYOUT);
    u32 lyt = bonus_ptr(layout) ? *(const volatile u32*)(layout + BONUS_LAYOUT_LYT) : 0;
    u32 root = bonus_ptr(lyt) ? *(const volatile u32*)(lyt + BONUS_LYT_ROOT) : 0;
    return bonus_ptr(root) ? root : 0;
}

static int bonus_layout_valid(void) {
    return bonus_parts && bonus_root_of(bonus_parts) == bonus_root && bonus_name_is(bonus_heart[0], "P_Heart_00");
}

static int bonus_link_alive(void);
static int bonus_save_ready(void);
static void bonus_shrink_current(void);
static void bonus_discover(u32 self, u32 root, u32 all);

/* hdParts_c::draw (026F8B54): the HUD screen, found once (again after a state load); before the game
 * draws it, its big heart is made small while yellow hearts follow (bonus_shrink_current). */
WWHD_HOOK(0x026F8B54, bonus_parts_draw, (u32 self, u32 viewport)) {
    (void)viewport;
    if (!bonus_hearts && !bonus_magic && bonus_light_hidden < 0) return;
    if (!bonus_layout_valid() && (bonus_hearts || bonus_magic)) {
        u32 root = bonus_root_of(self);
        u32 all = root ? bonus_find(root, "L_HeartAll_00", 3) : 0;
        if (all) bonus_discover(self, root, all);
    }
    bonus_shrink_current();
}

static void bonus_discover(u32 self, u32 root, u32 all) {
    u32 heart[BONUS_SLOTS], base[BONUS_SLOTS], light[BONUS_SLOTS], gloss[BONUS_SLOTS];
    char name[12] = "L_Heart_00";
    for (u32 i = 0; i != BONUS_SLOTS; ++i) {
        name[8] = (char)('0' + i / 10u);
        name[9] = (char)('0' + i % 10u);
        u32 slot = bonus_find(all, name, 3);
        heart[i] = slot ? bonus_find(slot, "P_Heart_00", 4) : 0;
        base[i] = slot ? bonus_find(slot, "P_Base_00", 4) : 0;
        light[i] = slot ? bonus_find(slot, "P_Light_00", 4) : 0;
        gloss[i] = slot ? bonus_find(slot, "P_Gloss_00", 4) : 0;
        if (!heart[i] || !base[i]) {
            if (!bonus_missing_logged++) wwhd_log("multiplayer: yellow hearts: the HUD's heart slots were not found");
            return;
        }
    }
    u32 bar = bonus_find(root, "L_MagicBar_00", 4);
    for (u32 k = 0; k != 2; ++k) {
        bonus_meter[k] = bar ? bonus_find(bar, k ? "N_Magic_01" : "N_Magic_00", 4) : 0;
        bonus_fill[k] = bonus_meter[k] ? bonus_find(bonus_meter[k], k ? "P_Magic_01" : "P_Magic_00", 2) : 0;
        bonus_frame[k] = bonus_meter[k] ? bonus_find(bonus_meter[k], k ? "P_MagicBase_01" : "P_MagicBase_00", 2) : 0;
    }
    for (u32 i = 0; i != BONUS_SLOTS; ++i) {
        bonus_heart[i] = heart[i];
        bonus_base[i] = base[i];
        bonus_light[i] = light[i];
        bonus_gloss[i] = gloss[i];
    }
    bonus_parts = self;
    bonus_root = root;
    bonus_current = bonus_light_hidden = -1; /* a new screen: nothing of ours is changed in it */
    wwhd_log("multiplayer: yellow hearts: the HUD's hearts and magic meter found");
}

static void bonus_show_light(void) {
    if (bonus_light_hidden >= 0 && bonus_layout_valid() && bonus_light[bonus_light_hidden])
        *(volatile u8*)(bonus_light[bonus_light_hidden] + BONUS_PANE_FLAGS) |= 1u;
    bonus_light_hidden = -1;
}

/* The game draws its current heart bigger (Heart_00_Max scales N_Heart_00) with a pulsing glow
 * (Heart_00_Loop: P_Light_00's alpha and scale). With yellow hearts after it, that heart takes the
 * size of the others and its glow is hidden; the glow goes to the last yellow heart (bonus_draw).
 * Run before each draw of the screen, maybe several per logic step: the heart's panes get the
 * linear part of another slot's matrices (their places stay), which gives the same result twice. */
static void bonus_shrink_current(void) {
    if (!bonus_layout_valid()) {
        bonus_current = bonus_light_hidden = -1;
        return;
    }
    if (!bonus_loaded || !bonus_hearts || !bonus_link_alive() || !bonus_save_ready()) {
        bonus_show_light();
        bonus_current = -1;
        return;
    }
    u32 first = dComIfGs_player_status_a.max_life / 4u;
    u32 n = first >= 2u && first <= BONUS_SLOTS ? first : 0u;
    if (!n) {
        bonus_show_light();
        return;
    }
    f32 small = 0.0f, big = 0.0f;
    int big_slot = -1;
    for (u32 k = 0; k != n; ++k) {
        const volatile f32* m = (const volatile f32*)(bonus_heart[k] + BONUS_PANE_MTX);
        f32 s = m[0] < 0.0f ? -m[0] : m[0];
        if (!(s > 0.01f && s < 100.0f)) continue;
        if (small == 0.0f || s < small) small = s;
        if (s > big) {
            big = s;
            big_slot = (int)k;
        }
    }
    if (big_slot >= 0 && big > small * 1.05f) { /* the game's matrices of this step, not changed yet */
        bonus_current = big_slot;
        bonus_small_scale = small;
        bonus_big_scale = big;
        if (bonus_light[big_slot]) {
            const volatile f32* lm = (const volatile f32*)(bonus_light[big_slot] + BONUS_PANE_MTX);
            bonus_light_scale = lm[0] < 0.0f ? -lm[0] : lm[0];
        }
    } else if (bonus_current < 0 || (u32)bonus_current >= n) {
        return;
    }
    int cur = bonus_current;
    u32 ref = cur == 0 ? 1u : 0u;
    u32 from[3] = {bonus_heart[ref], bonus_base[ref], bonus_gloss[ref]};
    u32 to[3] = {bonus_heart[cur], bonus_base[cur], bonus_gloss[cur]};
    for (u32 p = 0; p != 3; ++p) {
        if (!from[p] || !to[p]) continue;
        const volatile f32* a = (const volatile f32*)(from[p] + BONUS_PANE_MTX);
        volatile f32* b = (volatile f32*)(to[p] + BONUS_PANE_MTX);
        for (u32 row = 0; row != 3; ++row)
            for (u32 col = 0; col != 3; ++col) b[row * 4u + col] = a[row * 4u + col];
    }
    if (bonus_light[cur]) {
        bonus_light_alpha = (f32)*(const volatile u8*)(bonus_light[cur] + BONUS_PANE_ALPHA) / 255.0f;
        if (bonus_light_hidden != cur) {
            bonus_show_light();
            *(volatile u8*)(bonus_light[cur] + BONUS_PANE_FLAGS) &= (u8)~1u;
            bonus_light_hidden = cur;
        }
    }
}

static u32 bonus_texture_of(u32 picture) {
    u32 material = bonus_ptr(picture) ? *(const volatile u32*)(picture + BONUS_PICTURE_MATERIAL) : 0;
    u32 texture = bonus_ptr(material) ? *(const volatile u32*)(material + BONUS_MATERIAL_TEXTURE) : 0;
    return bonus_ptr(texture) ? texture : 0;
}

enum { BONUS_AS_IS, BONUS_RED_TO_YELLOW, BONUS_GREY_TO_YELLOW };

/* One of the game's textures, decoded and recoloured into a HUD image (0 when it cannot be read). */
static u32 bonus_image_of(u32 picture, int recolour) {
    u32 texture = bonus_texture_of(picture);
    if (!texture) return 0;
    u32 dims = wwhd_hud_game_pixels(texture, 0, 0);
    u32 w = dims >> 16, h = dims & 0xFFFFu;
    if (!w || !h || w * h * 4u > BONUS_TEXTURE_MAX) return 0;
    u8* px = (u8*)wwhd_malloc(w * h * 4u);
    if (!px) return 0;
    u32 image = 0;
    if (wwhd_hud_game_pixels(texture, px, w * h * 4u)) {
        for (u32 i = 0; i != w * h * 4u; i += 4) {
            u32 r = px[i], g = px[i + 1], b = px[i + 2];
            if (recolour == BONUS_RED_TO_YELLOW && r > g + 24u && r > b + 24u) {
                /* the heart's reds: same brightness, yellow (the white gloss stays) */
                px[i + 1] = (u8)(r * 214u / 255u + g * 41u / 255u);
                px[i + 2] = (u8)(b / 4u);
            } else if (recolour == BONUS_GREY_TO_YELLOW) {
                /* as the material does for the green (black + (white - black) * grey), in yellow */
                u32 t = (r + g + b) / 3u;
                px[i] = (u8)(255u - t * 40u / 255u);
                px[i + 1] = (u8)(236u - t * 76u / 255u);
                px[i + 2] = (u8)(40u - t * 40u / 255u);
            }
        }
        image = wwhd_hud_image(px, w, h);
    }
    wwhd_free(px);
    return image;
}

/* A pane's alpha as drawn (its own and its parents', 0 when one is hidden). */
static f32 bonus_alpha(u32 pane) {
    f32 a = 1.0f;
    for (u32 depth = 0; bonus_ptr(pane) && depth != 16; ++depth) {
        u8 flags = *(const volatile u8*)(pane + BONUS_PANE_FLAGS);
        if (!(flags & 1u)) return 0.0f;
        a *= (f32)*(const volatile u8*)(pane + BONUS_PANE_ALPHA) / 255.0f;
        pane = *(const volatile u32*)(pane + BONUS_PANE_PARENT);
    }
    return a;
}

/* A picture in TV coordinates: left, top, width, height (origin given: centre or left). */
static void bonus_rect(u32 pane, int left_origin, f32* x, f32* y, f32* w, f32* h) {
    const volatile f32* m = (const volatile f32*)(pane + BONUS_PANE_MTX);
    const volatile f32* size = (const volatile f32*)(pane + BONUS_PANE_SIZE);
    *w = size[0] * m[0];
    *h = size[1] * m[5];
    if (*w < 0.0f) *w = -*w;
    if (*h < 0.0f) *h = -*h;
    *x = 640.0f + m[3] - (left_origin ? 0.0f : *w * 0.5f);
    *y = 360.0f - m[7] - *h * 0.5f;
}

/* One element; anything the runtime would refuse (it then drops the mod's whole list, names and
 * crowns included: guest_hud.h geometry) is not sent. */
/* A pane's centre on the TV and its drawn scale (x axis of its global matrix). */
static void bonus_center(u32 pane, f32* x, f32* y, f32* scale) {
    const volatile f32* m = (const volatile f32*)(pane + BONUS_PANE_MTX);
    *x = 640.0f + m[3];
    *y = 360.0f - m[7];
    *scale = m[0] < 0.0f ? -m[0] : m[0];
}

static void bonus_emit(u32 list, u32 image, f32 x, f32 y, f32 w, f32 h, f32 part, u32 rgb, f32 alpha) {
    if (!image || !(alpha > 0.0f) || alpha > 1.0f) return;
    if (!(x > -4096.0f && x < 4096.0f && y > -4096.0f && y < 4096.0f)) return;      /* NaN too */
    if (!(w > 0.0f && w < 4096.0f && h > 0.0f && h < 4096.0f && part > 0.0f && part <= 1.0f)) return;
    bonus_element = (wwhd_hud_element){.kind = WWHD_HUD_IMAGE, .anchor = WWHD_HUD_TOP_LEFT, .blend = WWHD_HUD_ALPHA,
                                       .x = x, .y = y, .w = w * part, .h = h, .thickness = 1.0f, .u1 = part,
                                       .v1 = 1.0f, .rgba = (rgb << 8) | (u32)(alpha * 255.0f),
                                       .image = image};
    wwhd_hud_emit(list, &bonus_element);
}

/* Part of an image, (u0, v0)-(u1, v1) of the rectangle x, y, w, h. */
static void bonus_emit_uv(u32 list, u32 image, f32 x, f32 y, f32 w, f32 h, f32 u0, f32 v0, f32 u1, f32 v1, f32 alpha) {
    if (!image || !(alpha > 0.0f) || alpha > 1.0f) return;
    if (!(x > -4096.0f && x < 4096.0f && y > -4096.0f && y < 4096.0f)) return;
    if (!(w > 0.0f && w < 4096.0f && h > 0.0f && h < 4096.0f)) return;
    bonus_element = (wwhd_hud_element){.kind = WWHD_HUD_IMAGE, .anchor = WWHD_HUD_TOP_LEFT, .blend = WWHD_HUD_ALPHA,
                                       .x = x + w * u0, .y = y + h * v0, .w = w * (u1 - u0), .h = h * (v1 - v0),
                                       .thickness = 1.0f, .u0 = u0, .v0 = v0, .u1 = u1, .v1 = v1,
                                       .rgba = 0xFFFFFF00u | (u32)(alpha * 255.0f), .image = image};
    wwhd_hud_emit(list, &bonus_element);
}

/* The game's partial hearts (Heart_00 HeartDamage_02/01/00, 60 x 50, checked offline): quarters of
 * the heart around texel (31, 24): 1/4 the top left, 2/4 the left half, 3/4 all but the top right. */
#define BONUS_CUT_U (31.0f / 60.0f)
#define BONUS_CUT_V (24.0f / 50.0f)

static void bonus_emit_quarters(u32 list, u32 image, f32 x, f32 y, f32 w, f32 h, u32 quarters, f32 alpha) {
    if (quarters >= 4u) {
        bonus_emit_uv(list, image, x, y, w, h, 0.0f, 0.0f, 1.0f, 1.0f, alpha);
        return;
    }
    if (quarters >= 1u) bonus_emit_uv(list, image, x, y, w, h, 0.0f, 0.0f, BONUS_CUT_U, BONUS_CUT_V, alpha);
    if (quarters >= 2u) bonus_emit_uv(list, image, x, y, w, h, 0.0f, BONUS_CUT_V, BONUS_CUT_U, 1.0f, alpha);
    if (quarters >= 3u) bonus_emit_uv(list, image, x, y, w, h, BONUS_CUT_U, BONUS_CUT_V, 1.0f, 1.0f, alpha);
}

static void bonus_draw(u32 list) {
    if (!bonus_loaded || (!bonus_hearts && !bonus_magic) || !bonus_link_alive() || !bonus_save_ready()) return;
    const u8* play = wwhd_play_get();
    if (!play || play[WWHD_PLAY_EVENT_RUNNING_OFFSET] || hud_hidden || chart_menu_open()) return;
    if (!bonus_layout_valid()) return;
    unsigned long long epoch = wwhd_hud_epoch();
    u32 source = bonus_texture_of(bonus_heart[0]);
    if (epoch != bonus_image_epoch || source != bonus_image_source) {
        if (epoch == bonus_image_epoch) { /* older epochs' handles are gone already */
            if (bonus_heart_image) wwhd_hud_release(bonus_heart_image);
            if (bonus_base_image) wwhd_hud_release(bonus_base_image);
            if (bonus_fill_image) wwhd_hud_release(bonus_fill_image);
            if (bonus_frame_image) wwhd_hud_release(bonus_frame_image);
            if (bonus_light_image) wwhd_hud_release(bonus_light_image);
        }
        bonus_image_epoch = epoch;
        bonus_image_source = source;
        bonus_heart_image = bonus_image_of(bonus_heart[0], BONUS_RED_TO_YELLOW);
        bonus_base_image = bonus_image_of(bonus_base[0], BONUS_AS_IS);
        bonus_fill_image = bonus_image_of(bonus_fill[0], BONUS_GREY_TO_YELLOW);
        bonus_frame_image = bonus_image_of(bonus_frame[0], BONUS_AS_IS);
        bonus_light_image = bonus_light[0] ? bonus_image_of(bonus_light[0], BONUS_RED_TO_YELLOW) : 0;
        if (!bonus_heart_image) wwhd_log("multiplayer: yellow hearts: the game's heart texture could not be read");
    }
    if (bonus_hearts) {
        f32 alpha = bonus_alpha(bonus_heart[0]);
        u32 first = dComIfGs_player_status_a.max_life / 4u; /* the heart containers */
        u32 shown = *(const volatile u16*)(play + BONUS_PLAY_NOW_LIFE);
        u32 start = (shown + 3u) / 4u; /* the first slot after the last (even partial) heart */
        if (start > first) start = first;
        u32 count = (bonus_hearts + 3u) / 4u;
        f32 x0, y0, x1, y1, s;
        bonus_center(bonus_heart[0], &x0, &y0, &s);
        bonus_center(bonus_heart[1], &x1, &y1, &s);
        /* The game draws its hearts small but its current one: the yellow hearts are small but the
         * last, which also takes the glow (the game's big heart lost both: bonus_shrink_current). */
        f32 small = bonus_small_scale, big = bonus_big_scale;
        if (bonus_current < 0 || !(small > 0.0f)) {
            small = big = 0.0f;
            u32 measured = first >= 2u && first <= BONUS_SLOTS ? first : BONUS_SLOTS;
            for (u32 k = 0; k != measured; ++k) {
                f32 cx, cy, sk;
                bonus_center(bonus_heart[k], &cx, &cy, &sk);
                if (!(sk > 0.01f && sk < 100.0f)) continue;
                if (small == 0.0f || sk < small) small = sk;
                if (sk > big) big = sk;
            }
        }
        const volatile f32* heart_size = (const volatile f32*)(bonus_heart[0] + BONUS_PANE_SIZE);
        const volatile f32* base_size = (const volatile f32*)(bonus_base[0] + BONUS_PANE_SIZE);
        for (u32 i = 0; i != count; ++i) {
            u32 slot = start + i, past = 0;
            if (slot >= BONUS_SLOTS) { /* past the 20th: on to the right of the last slot */
                past = slot - (BONUS_SLOTS - 1u);
                slot = BONUS_SLOTS - 1u;
            }
            f32 cx, cy, unused;
            bonus_center(bonus_heart[slot], &cx, &cy, &unused);
            cx += (x1 - x0) * (f32)past;
            f32 k = i + 1u == count ? big : small;
            f32 w = heart_size[0] * k, h = heart_size[1] * k;
            f32 bw = base_size[0] * k, bh = base_size[1] * k;
            u32 quarters = bonus_hearts - i * 4u;
            if (quarters > 4u) quarters = 4u;
            if (i + 1u == count && bonus_current >= 0 && bonus_light[0] && bonus_light_scale > 0.0f) {
                const volatile f32* light_size = (const volatile f32*)(bonus_light[0] + BONUS_PANE_SIZE);
                f32 lw = light_size[0] * bonus_light_scale, lh = light_size[1] * bonus_light_scale;
                bonus_emit(list, bonus_light_image, cx - lw * 0.5f, cy - lh * 0.5f, lw, lh, 1.0f, 0xFFFFFFu,
                           alpha * bonus_light_alpha);
            }
            if (start + i >= first) /* an empty container already shows the game's base */
                bonus_emit(list, bonus_base_image, cx - bw * 0.5f, cy - bh * 0.5f, bw, bh, 1.0f, 0xFFFFFFu,
                           alpha * (200.0f / 255.0f));
            bonus_emit_quarters(list, bonus_heart_image, cx - w * 0.5f, cy - h * 0.5f, w, h, quarters, alpha);
        }
    }
    if (bonus_magic) {
        int k = bonus_meter[1] && (*(const volatile u8*)(bonus_meter[1] + BONUS_PANE_FLAGS) & 1u) ? 1 : 0;
        if (bonus_fill[k] && bonus_frame[k]) {
            f32 alpha = bonus_alpha(bonus_frame[k]);
            f32 fx, fy, fw, fh, gx, gy, gw, gh;
            bonus_rect(bonus_frame[k], 1, &fx, &fy, &fw, &fh);
            bonus_rect(bonus_fill[k], 1, &gx, &gy, &gw, &gh);
            const volatile f32* m = (const volatile f32*)(bonus_fill[k] + BONUS_PANE_MTX);
            f32 unit = BONUS_MAGIC_UNIT * m[0];
            f32 start = fx + fw + 4.0f * m[0];   /* a second meter right after the game's frame */
            f32 inner = gx - fx;                 /* the fill's inset in its frame */
            bonus_emit(list, bonus_frame_image, start, fy, unit * (f32)BONUS_MAGIC_MAX + 2.0f * inner, fh, 1.0f, 0x2A2000u,
                       alpha * (220.0f / 255.0f));
            bonus_emit(list, bonus_fill_image, start + inner, gy, unit * (f32)bonus_magic, gh, 1.0f, 0xFFFFFFu, alpha);
        }
    }
}
