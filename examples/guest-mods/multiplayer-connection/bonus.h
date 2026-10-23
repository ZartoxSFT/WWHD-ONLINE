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
static u32 bonus_long_hookshot; /* the shop's Long Hookshot, bought for good (0.3.3) */
static u32 bonus_pigs;          /* the shop's Golden Pig: one to call (golden_pig.h), 0.3.3 */
static u32 bonus_arrows[4];       /* elemental arrows kept: [1] fire, [2] ice, [3] light ([0] unused) */
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
static char bonus_text[128];

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
    bonus_arrows[1] = bonus_number(bonus_text, (u32)got, "fire");
    bonus_arrows[2] = bonus_number(bonus_text, (u32)got, "ice");
    bonus_arrows[3] = bonus_number(bonus_text, (u32)got, "light");
    bonus_long_hookshot = bonus_number(bonus_text, (u32)got, "hookshot") != 0;
    bonus_pigs = bonus_number(bonus_text, (u32)got, "pig") != 0;
    for (int t = 1; t != 4; ++t)
        if (bonus_arrows[t] > 99) bonus_arrows[t] = 99;
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
    bonus_put(bonus_text, &n, "fire=", bonus_arrows[1]);
    bonus_put(bonus_text, &n, "ice=", bonus_arrows[2]);
    bonus_put(bonus_text, &n, "light=", bonus_arrows[3]);
    bonus_put(bonus_text, &n, "hookshot=", bonus_long_hookshot);
    bonus_put(bonus_text, &n, "pig=", bonus_pigs);
    wwhd_file_write("bonus.cfg", bonus_text, n);
    bonus_dirty = 0;
    bonus_written_step = wwhd_logic_step();
}

/* ------------------------------------------------------------------ elemental arrows
 *
 * The bow's arrow type is l_keepType (101D5F43, u8: 0 normal, 1 fire, 2 ice, 3 light; d_a_arrow.cpp,
 * daArrow_c::changeArrowType 0205584C, which also checks the magic and the arrows the player owns:
 * mItems[12] 0x35 fire and ice, 0x36 light). A shot takes one arrow from the save
 * (mItemRecord2.mArrowNum, save + 0x20 + 0x69, through mItemArrowNumCount play + 0x5B68) and the
 * arrow's magic (arrowUseMp). Elemental arrows are kept apart (bonus.cfg fire=, ice=, light=, at most
 * the quiver's size, save + 0x20 + 0x6F): while Link aims the bow (BOW_SUBJECT 0x94, BOW_MOVE 0x95)
 * with an elemental type chosen, the save's arrow count holds that type's stock, so the HUD shows it
 * and the game takes the shots from it (and the magic, as before); the normal arrows come back as
 * soon as he stops aiming or picks the normal type, so a save never keeps elemental arrows as
 * normal ones. A normal arrow pack picked up meanwhile goes to the normal arrows. */
#define BONUS_KEEP_TYPE 0x101D5F43
#define BONUS_SAVE_ARROWS (0x20 + 0x69)
#define BONUS_SAVE_QUIVER (0x20 + 0x6F)
#define BONUS_SAVE_BOW_SLOT (0x20 + 0x3C + 12) /* dSv_player_item_c::mItems[12] */
#define BONUS_PLAY_ARROWS 0x5B68
#define BONUS_PROC_BOW_SUBJECT 0x94
#define BONUS_PROC_BOW_MOVE 0x95
#define BONUS_BOW_FIRE_ICE 0x35
#define BONUS_BOW_LIGHT 0x36
static u32 bonus_arrow_shown;     /* the type whose count the save holds now (0: the normal arrows) */
static u32 bonus_arrow_normal;    /* the normal arrows while an elemental type is shown */

static u32 bonus_save_addr(void) { return WWHD_GAME_DATA(WWHD_ADDR_save_info_pointer, u32); }

static int bonus_arrow_owned(u32 type) {
    u32 save = bonus_save_addr();
    if (!save) return 0;
    u8 bow = *(const volatile u8*)(save + BONUS_SAVE_BOW_SLOT);
    return type == 3 ? bow == BONUS_BOW_LIGHT : (bow == BONUS_BOW_FIRE_ICE || bow == BONUS_BOW_LIGHT);
}

static u32 bonus_quiver(void) {
    u32 save = bonus_save_addr();
    return save ? *(const volatile u8*)(save + BONUS_SAVE_QUIVER) : 0;
}

/* 0.3.16: at most 10 elemental arrows of each type with the small quiver (the save's maximum 30), 20
 * with the middle one (60), 30 with the big one (99). */
static u32 bonus_elemental_cap(void) {
    u32 q = bonus_quiver();
    return q <= 30u ? 10u : q <= 60u ? 20u : 30u;
}

static void bonus_arrows_step(void) {
    u32 save = bonus_save_addr();
    u8* play = wwhd_play_get();
    if (!save || !play) return;
    u32 cap = bonus_elemental_cap();
    for (u32 t = 1; t != 4; ++t)
        if (bonus_arrows[t] > cap) {
            bonus_arrows[t] = cap;
            bonus_dirty = 1;
        }
    if (bonus_arrow_shown && *(const volatile s16*)(play + BONUS_PLAY_ARROWS) == 0) {
        volatile u8* shown = (volatile u8*)(save + BONUS_SAVE_ARROWS);
        if (*shown > cap) *shown = (u8)cap; /* the shown type's stock, in the save while aiming */
    }
    u32 want = 0;
    if (bonus_link_alive()) {
        const daPy_lk_c* link = (const daPy_lk_c*)bonus_link;
        u8 type = WWHD_GAME_DATA(BONUS_KEEP_TYPE, u8);
        if ((link->mCurProc == BONUS_PROC_BOW_SUBJECT || link->mCurProc == BONUS_PROC_BOW_MOVE) && type >= 1 && type <= 3)
            want = type;
    }
    if (want == bonus_arrow_shown || *(const volatile s16*)(play + BONUS_PLAY_ARROWS) != 0) return;
    volatile u8* count = (volatile u8*)(save + BONUS_SAVE_ARROWS);
    if (bonus_arrow_shown) bonus_arrows[bonus_arrow_shown] = *count; /* what the shots left */
    else bonus_arrow_normal = *count;
    *count = (u8)(want ? (bonus_arrows[want] > cap ? cap : bonus_arrows[want]) : bonus_arrow_normal);
    bonus_arrow_shown = want;
    bonus_dirty = 1;
}

/* 0.2.99: no elemental type with no arrows of it. The bow cycles normal -> fire -> ice -> light -> normal
 * (d_a_arrow.cpp): daArrow_c::changeArrowType (0205584C, the nocked arrow: a new arrow of the next
 * type, the old one deleted) and changeArrowTypeNotReady (0205578C, no argument, l_keepType alone);
 * the game checks only the magic (dSv_player_status_a_c::mMagic +0x14, d_save.h) and the arrows
 * owned (mItems[12]): with the stock at 0 the type was taken anyway (switched while drawing the bow,
 * and shot). The types whose stock is 0 are skipped (the normal arrows always taken). */
#define BONUS_SAVE_MAGIC 0x14
#define BONUS_ARROW_TYPE 0x71D /* daArrow_c::mArrowType (d_a_arrow.h, verified) */
WWHD_GAME_ORIGINAL(0x0205584C, u32, bonus_change_arrow_original, (void* self));
WWHD_GAME_ORIGINAL(0x0205578C, void, bonus_change_arrow_not_ready_original, (void));

static u32 bonus_magic_now(void) {
    u32 save = bonus_save_addr();
    return save ? *(const volatile u8*)(save + WWHD_OFFSET_save_info + BONUS_SAVE_MAGIC) : 0;
}

/* The game's next type from t (changeArrowType's rules). */
static u32 bonus_arrow_next(u32 t) {
    u32 magic = bonus_magic_now();
    if (t == 0 || t == 1) return magic == 0 || !bonus_arrow_owned(1) ? 0 : t + 1;
    if (t == 2) return magic < 2 || !bonus_arrow_owned(3) ? 0 : 3;
    return 0;
}

/* Arrows of an elemental type now (the save holds the shown type's count). */
static u32 bonus_arrow_stock(u32 t) {
    if (t == bonus_arrow_shown) {
        u32 save = bonus_save_addr();
        return save ? *(const volatile u8*)(save + BONUS_SAVE_ARROWS) : 0;
    }
    return bonus_arrows[t & 3];
}

/* The next type after t that has arrows (the normal ones at worst). */
static u32 bonus_arrow_target(u32 t) {
    u32 next = t;
    for (int k = 0; k != 4; ++k) {
        next = bonus_arrow_next(next);
        if (next == 0 || bonus_arrow_stock(next)) return next;
    }
    return 0;
}

WWHD_REPLACE(0x0205584C, u32, bonus_change_arrow, (void* self)) {
    if (!self) return bonus_change_arrow_original(self);
    volatile u8* type = (volatile u8*)self + BONUS_ARROW_TYPE;
    u32 from = *type, to = bonus_arrow_target(from);
    if (from > 3 || bonus_arrow_next(from) == to) return bonus_change_arrow_original(self);
    if (to == from) return (u32)self; /* nothing else to take: the arrow stays */
    /* the game makes the type after mArrowType: it is given the one before the wanted type */
    u32 before = to == 0 ? 3 : to - 1;
    *type = (u8)before;
    u32 ret = bonus_change_arrow_original(self);
    if (ret == (u32)self) { /* not made: as it was */
        *type = (u8)from;
        WWHD_GAME_DATA(BONUS_KEEP_TYPE, u8) = (u8)from;
    }
    return ret;
}

WWHD_REPLACE(0x0205578C, void, bonus_change_arrow_not_ready, (void)) {
    volatile u8* keep = &WWHD_GAME_DATA(BONUS_KEEP_TYPE, u8);
    for (int k = 0; k != 4; ++k) {
        u32 before = *keep;
        bonus_change_arrow_not_ready_original();
        if (*keep == before || *keep == 0 || *keep > 3 || bonus_arrow_stock(*keep)) return;
    }
}

/* The Long Hookshot (0.3.3): its reach doubled. daHookshot_c's reach is one float constant,
 * 1500.0f (d_a_hookshot.cpp: the sight's end 1500 units ahead, 021789B4, and the shot's return at
 * 1500 units from the root, 02178DDC), read by lfs from 0x10011724 at both places and nowhere else
 * (the EU cking.rpx's code scanned for lis/lfs to it; the same address in the USA build, below the
 * first data step of tools/recomp/builds/eu.json). The recompiler translates lfs to a load from
 * guest memory (tools/recomp/ppc2c.py), so the constant is written: 3000.0f with the upgrade. It is
 * only written when it holds one of the two values (another build: left alone, logged). */
#define BONUS_HOOKSHOT_REACH 0x10011724
#define BONUS_HOOKSHOT_GAME 1500.0f
#define BONUS_HOOKSHOT_LONG 3000.0f
static void bonus_hookshot_step(void) {
    volatile f32* reach = &WWHD_GAME_DATA(BONUS_HOOKSHOT_REACH, f32);
    f32 want = bonus_long_hookshot ? BONUS_HOOKSHOT_LONG : BONUS_HOOKSHOT_GAME, now = *reach;
    if (now == want) return;
    if (now != BONUS_HOOKSHOT_GAME && now != BONUS_HOOKSHOT_LONG) {
        static int logged;
        if (!logged++) wwhd_log("multiplayer: the hookshot's reach is not at its known place, left alone");
        return;
    }
    *reach = want;
    wwhd_log_int("multiplayer: hookshot reach (units)", (int)want);
}

/* Purchase (shop_try_upgrade): "ok" once the price is checked by the caller, "full" when owned. */
static const char* bonus_buy_hookshot(void) {
    bonus_load();
    if (bonus_long_hookshot) return "full";
    bonus_long_hookshot = 1;
    bonus_write();
    bonus_hookshot_step();
    return "ok";
}

/* Purchase of the Golden Pig: one at a time ("full" while one waits to be called). */
static const char* bonus_buy_pig(void) {
    bonus_load();
    if (bonus_pigs) return "full";
    bonus_pigs = 1;
    bonus_write();
    return "ok";
}

/* The local Link's step (mod.c map_step): what was spent, written now and then. */
static void bonus_fx_step(void);
static void bonus_tick(void) {
    bonus_load();
    bonus_arrows_step();
    bonus_hookshot_step();
    bonus_fx_step();
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

/* ------------------------------------------------------------------ yellow loot
 *
 * A loot (decomp, verified: fopAcM_createItemFromTable 025D8120 for pots, grass, stones and flowers,
 * d_a_tsubo_dmg.cpp, d_grass.cpp, d_a_stone.cpp, d_flower.cpp; fopAcM_createIball 025D9154 for a
 * dying enemy's item ball, d_a_disappear.cpp, f_op_actor_mng_4.cpp) that made an item comes, very
 * rarely, with a yellow heart (BONUS_DROP_HEART per thousand) or a yellow magic jar
 * (BONUS_DROP_MAGIC per thousand), whatever the life: a heart
 * (dItemNo_HEART_e 0x00) or small magic (0x09) item made at the same place with
 * fopAcM_createItem (025D8870, as d_a_disappear makes its heart: bit -1, action 4). The game leaves
 * hearts out of its tables at full life (getItemNoByLife): the yellow one comes whatever the life,
 * the yellow magic whatever the magic (only with the magic meter); neither when that bonus is at
 * its maximum. Made outside any synchronisation context (as_context NONE): each game rolls its own
 * and keeps them (another player's game neither gets nor removes them).
 * The game picks an item up in daItem_c::itemGetExecute (0217DE18, d_a_item.cpp: its sound, then
 * execItemGet(m_itemNo), 0254DA38): for a yellow one, execItemGet gives the yellow heart or magic
 * (bonus_buy_*) instead of the red heart or the magic. Drawn recoloured in gold like an elite
 * (elite.h el_draw_begin: the heart's red, the jar's main colour). */
#define BONUS_DROP_HEART 5          /* per thousand loots that dropped something (0.3.90: 0.5 %; before 4; 0.2.83: 3 % of
                                     * every call: cutting grass gave many) */
#define BONUS_DROP_MAGIC 5
/* the yellow heart's chance per thousand loots: BONUS_DROP_HEART, changed for tests from the
 * launcher's Debug page (debug.cfg yellow_heart=, 0-1000; back to the default at the next start) */
static u32 bonus_drop_heart = BONUS_DROP_HEART;
/* the yellow magic jar's, the same way (debug.cfg yellow_magic=); rolled apart from the heart's
 * since 0.2.96, so each setting is its own chance (before, magic took the next values of one roll) */
static u32 bonus_drop_magic = BONUS_DROP_MAGIC;
#define BONUS_ITEM_HEART 0x00
#define BONUS_ITEM_MAGIC 0x09
#define BONUS_YELLOW_ITEMS 16
#define BONUS_ITEM_ARROW_10 0x10   /* dItemNo_ARROW_10/20/30_e: 0x10..0x12 */
#define BONUS_ITEM_ARROW_30 0x12
#define BONUS_ELEMENTAL_PERCENT 40  /* of the arrow packs a loot drops (0.2.86-0.2.88: 15) */
#define BONUS_PROC_ITEM 0xFF        /* fpcNm_ITEM_e */
#define BONUS_PROC_NAME 0x08
static u32 bonus_elemental_ids[BONUS_YELLOW_ITEMS];
static u8 bonus_elemental_types[BONUS_YELLOW_ITEMS];
static u32 bonus_elemental_next;
static u32 bonus_picking_type;      /* an elemental pack is being picked up: its type */
#define BONUS_PROCESS_ID 0x04       /* base_process_class id (wwhd/objects.h WWHD_OFFSET_actor_process_id) */
#define BONUS_ITEM_NO 0x74E         /* daItemBase_c::m_itemNo (d_a_itembase.h) */
WWHD_GAME_ORIGINAL(0x025D9154, u32, bonus_iball_original, (u32 pos, u32 table, u32 room, u32 angle, u32 bit));
WWHD_GAME_ORIGINAL(0x0217DE18, void, bonus_item_get_original, (void* self));
WWHD_GAME_ORIGINAL(0x0254DA38, void, bonus_exec_item_get_original, (u32 item));

static u32 bonus_yellow_ids[BONUS_YELLOW_ITEMS];
static u32 bonus_yellow_next;
static int bonus_drop_depth;
static int bonus_picking;           /* a yellow item is being picked up */
static u32 bonus_rng = 0x6D2B79F5u;

/* splitmix32 (0.2.97): a counter stepped by the golden ratio, stirred by the logic step, through a
 * full mix. Before, a xorshift32 whose state was xored with the step at every call: its successive
 * draws were not independent, and 15 elemental packs in a log were 11 fire, 4 light, no ice. */
static u32 bonus_random(void) {
    bonus_rng += 0x9E3779B9u + (u32)wwhd_logic_step();
    u32 z = bonus_rng;
    z = (z ^ (z >> 16)) * 0x85EBCA6Bu;
    z = (z ^ (z >> 13)) * 0xC2B2AE35u;
    return z ^ (z >> 16);
}

/* 0 .. n-1 from the high bits (even chances for a small n) */
static u32 bonus_random_below(u32 n) { return (u32)(((unsigned long long)bonus_random() * n) >> 32); }

static int bonus_yellow_item(const void* actor) {
    if (!actor) return 0;
    u32 id = *(const volatile u32*)((const u8*)actor + BONUS_PROCESS_ID);
    if (id == 0 || id == 0xFFFFFFFFu) return 0;
    for (u32 i = 0; i != BONUS_YELLOW_ITEMS; ++i)
        if (bonus_yellow_ids[i] == id) return 1;
    return 0;
}

/* Loots (0.2.87). Every item a loot makes goes through one of the game's item makers, which take its
 * number: fopAcM_fastCreateItem2 (025D7F20: the item tables of pots, grass, stones, flowers) and
 * fopAcM_fastCreateItem (025D8AB0: an enemy's table, the crystal ball's items when it breaks,
 * daIball_c::createItem 0217BF6C, d_a_ib.cpp) return the actor; fopAcM_createItem (025D8870: a
 * dying enemy's puff, d_a_disappear.cpp, action 4) returns its process id. A loot window is open
 * while fopAcM_createItemFromTable (025D8120, actor_sync.h), fopAcM_createIball (025D9154) or
 * daIball_c::createItem runs, and a puff's item is a loot of its own: each arrow pack made there may
 * turn elemental (BONUS_ELEMENTAL_PERCENT, a type the player owns), and each loot that made at least
 * one item may bring a yellow heart or magic (per thousand). 0.2.83-0.2.86 rolled only after the
 * table and ball calls, on what they returned: the crystal ball returns the ball, its items come
 * when it breaks: Darknuts and the like never gave elemental arrows. */
WWHD_GAME_ORIGINAL(0x025D7F20, u32, bonus_fast_item2_original,
                   (u32 pos, u32 item, u32 bit, u32 room, u32 type, u32 angle, u32 action, u32 scale));
WWHD_GAME_ORIGINAL(0x025D8AB0, u32, bonus_fast_item_original,
                   (u32 pos, u32 item, u32 room, u32 angle, u32 scale, f32 speed_f, f32 speed_y, f32 gravity, u32 bit,
                    u32 create_func));
WWHD_GAME_ORIGINAL(0x025D8870, u32, bonus_create_item_original,
                   (u32 pos, u32 item, u32 bit, u32 room, u32 type, u32 angle, u32 action, u32 scale));
WWHD_GAME_ORIGINAL(0x0217BF6C, s32, bonus_ball_items_original, (void* self));
static u32 bonus_window_items;    /* items the open loot made */
static int bonus_making;          /* this mod makes a yellow item: not a loot */
#define BONUS_PUFF_ACTION 4       /* fopAcM_createItem's action from a dying enemy's puff */

static void bonus_drop_begin(void) {
    if (bonus_drop_depth++ == 0) bonus_window_items = 0;
}

/* The elemental type of an arrow pack on the ground, 0 for none. */
static u32 bonus_elemental_of(const void* actor) {
    if (!actor) return 0;
    u32 id = *(const volatile u32*)((const u8*)actor + BONUS_PROCESS_ID);
    if (id == 0 || id == 0xFFFFFFFFu) return 0;
    for (u32 i = 0; i != BONUS_YELLOW_ITEMS; ++i)
        if (bonus_elemental_ids[i] == id) return bonus_elemental_types[i];
    return 0;
}

/* An item a loot made (its process id and number): an arrow pack sometimes turns elemental. */
static void bonus_loot_item(u32 id, u32 item) {
    if (id == 0 || id == 0xFFFFFFFFu) return;
    ++bonus_window_items;
    if (item < BONUS_ITEM_ARROW_10 || item > BONUS_ITEM_ARROW_30 || !bonus_arrow_owned(1)) return;
    if (bonus_random() % 100u >= BONUS_ELEMENTAL_PERCENT) return;
    u32 types = bonus_arrow_owned(3) ? 3u : 2u;
    u32 type = 1u + bonus_random_below(types); /* a third each with the light arrows, else half */
    bonus_elemental_ids[bonus_elemental_next] = id;
    bonus_elemental_types[bonus_elemental_next] = (u8)type;
    bonus_elemental_next = (bonus_elemental_next + 1) % BONUS_YELLOW_ITEMS;
    static u32 dropped[4];
    ++dropped[type];
    wwhd_log_int("multiplayer: elemental arrows dropped, type", (int)type);
    wwhd_log_int("multiplayer:   fire so far", (int)dropped[1]);
    wwhd_log_int("multiplayer:   ice so far", (int)dropped[2]);
    wwhd_log_int("multiplayer:   light so far", (int)dropped[3]);
}

/* Its element on an elemental pack (0.2.90): the glow of a nocked elemental arrow, the actor
 * daArrow_Lighteff (process 0x1DA, d_a_arrow_lighteff.cpp, parameter = the arrow type): the burning
 * tip, its model from the "Link" archive (GARWFI00 fire, GARWFI01 ice, GARWG00 light) with its btk
 * and brk, its particles (fire ID_IT_JN_ARWF_HINOKO00 0x299; ice ARWI_KIRAKIRA00 0x29C and REIKI00
 * 0x29D; light ARWG_TSUBU00 0x29F and FLASH00 0x2A0, following its tip) and its point light. One is
 * made per elemental pack at its first draw (fopAcM_create, no synchronisation context). Its own
 * execute (02057930) follows a parent arrow and deletes itself without one: for these, it is replaced
 * by this mod's (bonus_glow_execute): the btk played (mDoExt_baseAnm::play 025E742C on +0x448), the
 * brk frame raised to its end (+0x4BC, brk at +0x3D0: frame +4, end +0xA), the actor upright under
 * the pack (angle x -0x4000: set_mtx 02056EB8 without a parent moves the model 62 units along its
 * facing), the tip (+0x3B8, where the particles and the light are) at the pack, setPointLight
 * (0205777C) and set_mtx; when the pack is no longer drawn (picked up, gone) it deletes itself, ending
 * its particles the game's way. 0.2.89 made only the particles, 0.2.88 the hit particles. */
#define BONUS_PROC_LIGHTEFF 0x1DA
#define BONUS_GLOW_GONE_STEPS 3
#define BONUS_GLOW_TOP 0x3B8
#define BONUS_GLOW_BRK 0x3D0
#define BONUS_GLOW_BTK 0x448
#define BONUS_GLOW_BRK_FRAME 0x4BC
#define BONUS_GLOW_LIFT 62.0f
WWHD_GAME_ORIGINAL(0x02057930, s32, bonus_glow_execute_original, (void* self));
WWHD_GAME_FUNC(0x025E742C, s32, bonus_anm_play, (u32 anm));
WWHD_GAME_FUNC(0x0205777C, void, bonus_glow_point_light, (void* self));
WWHD_GAME_FUNC(0x02056EB8, void, bonus_glow_set_mtx, (void* self));
/* 0.2.92: the glow at the tip of the pack's arrows, along them. The pack's models turn with the
 * item's current.angle.y plus mHDRotY (+0x76C, s16; daItem_c::set_mtx / set_mtx_base,
 * d_a_item.cpp, d_a_item.h). Where the tip is in the model is not in the decomp: estimated from a
 * screenshot (arrows leaning about 40 degrees), tunable live from the launcher's Debug page
 * (debug.cfg glow_yaw= degrees added to the pack's turn, glow_pitch= degrees above the ground,
 * glow_len= units from the pack's place to the tip, glow_base= height of the arrows' foot). */
#define BONUS_ITEM_HD_ROT_Y 0x76C
static u32 bonus_glow_yaw_deg = 270, bonus_glow_pitch_deg = 40, bonus_glow_len = 40, bonus_glow_base = 15; /* set in game (0.2.95) */
/* 0.2.94: the glow's model only (its mesh is drawn away from its origin, seen in game), from the tip
 * where the particles and the light stay: along the arrows (forward +) and to their side (right +),
 * in units; debug.cfg glow_fwd= and glow_side= carry the value + 100 (the file takes no sign). */
static s32 bonus_glow_fwd = 0, bonus_glow_side = 0;
typedef struct bonus_glow {
    u32 item, effect;             /* the pack's and the glow's process ids */
    cXyz pos;                     /* the pack, at its last draw */
    s16 turn;                     /* the pack's models' turn (angle.y + mHDRotY) */
    unsigned long long seen;
    u8 used;
} bonus_glow;
static bonus_glow bonus_glows[BONUS_YELLOW_ITEMS];
static cXyz bonus_glow_pos;
static csXyz bonus_glow_angle;

static void bonus_elemental_fx(const void* actor) {
    u32 id = *(const volatile u32*)((const u8*)actor + BONUS_PROCESS_ID);
    u32 type = 0;
    for (u32 i = 0; i != BONUS_YELLOW_ITEMS; ++i)
        if (bonus_elemental_ids[i] == id) type = bonus_elemental_types[i] & 3u;
    if (!type) return;
    const fopAc_ac_c* a = (const fopAc_ac_c*)actor;
    bonus_glow* g = 0;
    for (u32 i = 0; i != BONUS_YELLOW_ITEMS && !g; ++i)
        if (bonus_glows[i].used && bonus_glows[i].item == id) g = &bonus_glows[i];
    if (!g) {
        for (u32 i = 0; i != BONUS_YELLOW_ITEMS && !g; ++i)
            if (!bonus_glows[i].used) g = &bonus_glows[i];
        if (!g) return;
        bonus_glow_pos.x = a->current.pos.x;
        bonus_glow_pos.y = a->current.pos.y;
        bonus_glow_pos.z = a->current.pos.z;
        bonus_glow_angle.x = -0x4000;
        bonus_glow_angle.y = 0;
        bonus_glow_angle.z = 0;
        u8 saved = as_context;
        as_context = AS_CTX_NONE; /* this game's own */
        u32 effect = wwhd_fopAcM_create(BONUS_PROC_LIGHTEFF, type, (u32)&bonus_glow_pos, (s32)a->current.roomNo,
                                        (u32)&bonus_glow_angle, 0, 0xFFFFFFFFu, 0);
        as_context = saved;
        if (effect == 0 || effect == 0xFFFFFFFFu) return;
        g->used = 1;
        g->item = id;
        g->effect = effect;
    }
    g->pos.x = a->current.pos.x;
    g->pos.y = a->current.pos.y;
    g->pos.z = a->current.pos.z;
    g->turn = (s16)(a->current.angle.y + *(const volatile s16*)((const u8*)actor + BONUS_ITEM_HD_ROT_Y));
    g->seen = wwhd_logic_step();
}

static s16 bonus_deg(u32 deg) { return (s16)(u16)((deg % 360u) * 65536u / 360u); }

/* daArrow_Lighteff_c::_execute: a glow of this mod follows its pack. */
WWHD_REPLACE(0x02057930, s32, bonus_glow_execute, (void* self)) {
    u32 id = self ? *(const volatile u32*)((const u8*)self + BONUS_PROCESS_ID) : 0;
    bonus_glow* g = 0;
    for (u32 i = 0; i != BONUS_YELLOW_ITEMS && !g; ++i)
        if (bonus_glows[i].used && bonus_glows[i].effect == id) g = &bonus_glows[i];
    if (!g) return bonus_glow_execute_original(self);
    u8* e = (u8*)self;
    if (wwhd_logic_step() - g->seen > BONUS_GLOW_GONE_STEPS) {
        g->used = 0;
        wwhd_fopAcM_delete_actor((u32)self);
        return 1;
    }
    bonus_anm_play((u32)(e + BONUS_GLOW_BTK));
    volatile f32* brk_frame = (volatile f32*)(e + BONUS_GLOW_BRK_FRAME);
    f32 end = (f32)*(const volatile s16*)(e + BONUS_GLOW_BRK + 0xA);
    if (*brk_frame < end) *brk_frame = *brk_frame + 1.0f > end ? end : *brk_frame + 1.0f;
    *(volatile f32*)(e + BONUS_GLOW_BRK + 4) = *brk_frame;
    fopAc_ac_c* a = (fopAc_ac_c*)self;
    /* along the arrows: yaw = the pack's turn + glow_yaw, pitch glow_pitch above the ground (angle.x
     * negative is up: -0x4000 stood the glow upright); the tip glow_len from the arrows' foot */
    s16 yaw = (s16)(g->turn + bonus_deg(bonus_glow_yaw_deg));
    s16 pitch = bonus_deg(bonus_glow_pitch_deg > 90 ? 90 : bonus_glow_pitch_deg);
    f32 up = as_sin(pitch), flat = as_sin((s16)(pitch + 0x4000));
    f32 dir[3] = {flat * as_sin(yaw), up, flat * as_sin((s16)(yaw + 0x4000))};
    f32 tip[3];
    for (int k = 0; k != 3; ++k) tip[k] = (&g->pos.x)[k] + dir[k] * (f32)bonus_glow_len;
    tip[1] += (f32)bonus_glow_base;
    /* the model's origin: the tip moved along the arrows and to their right (horizontal, a quarter
     * turn clockwise from the yaw seen from above); set_mtx without a parent puts the model
     * BONUS_GLOW_LIFT units along its facing */
    f32 right[3] = {-as_sin((s16)(yaw + 0x4000)), 0.0f, as_sin(yaw)};
    f32 origin[3];
    for (int k = 0; k != 3; ++k)
        origin[k] = tip[k] + dir[k] * (f32)bonus_glow_fwd + right[k] * (f32)bonus_glow_side;
    a->current.pos.x = origin[0] - dir[0] * BONUS_GLOW_LIFT;
    a->current.pos.y = origin[1] - dir[1] * BONUS_GLOW_LIFT;
    a->current.pos.z = origin[2] - dir[2] * BONUS_GLOW_LIFT;
    a->current.angle.x = (s16)-pitch;
    a->current.angle.y = yaw;
    volatile f32* top = (volatile f32*)(e + BONUS_GLOW_TOP);
    top[0] = tip[0];
    top[1] = tip[1];
    top[2] = tip[2];
    bonus_glow_point_light(self);
    bonus_glow_set_mtx(self);
    return 1;
}

/* Every step: a glow whose stage is gone (no execute any more) frees its entry. */
static void bonus_fx_step(void) {
    unsigned long long step = wwhd_logic_step();
    for (u32 i = 0; i != BONUS_YELLOW_ITEMS; ++i)
        if (bonus_glows[i].used && step - bonus_glows[i].seen > 120) bonus_glows[i].used = 0;
}

static u32 bonus_actor_id(u32 actor) { return actor ? *(const volatile u32*)(actor + BONUS_PROCESS_ID) : 0; }

static void bonus_yellow_make(u32 pos, u32 room, u32 item) {
    u8 saved = as_context;
    as_context = AS_CTX_NONE; /* this game's own */
    bonus_making = 1;
    u32 id = wwhd_fopAcM_createItem(pos, item, 0xFFFFFFFFu, room, 0, 0, BONUS_PUFF_ACTION, 0);
    bonus_making = 0;
    as_context = saved;
    if (id == 0 || id == 0xFFFFFFFFu) return;
    bonus_yellow_ids[bonus_yellow_next] = id;
    bonus_yellow_next = (bonus_yellow_next + 1) % BONUS_YELLOW_ITEMS;
    wwhd_log(item == BONUS_ITEM_HEART ? "multiplayer: a yellow heart dropped" : "multiplayer: a yellow magic jar dropped");
}

/* A loot that made something: sometimes a yellow heart, sometimes a yellow magic jar at its place.
 * 0.3.12: two rolls apart, both may drop (before, a yellow heart left no roll to the magic). */
static void bonus_yellow_roll(u32 pos, u32 room) {
    if (!pos || !bonus_save_ready()) return;
    bonus_load();
    int heart = bonus_random() % 1000u < bonus_drop_heart;
    int magic = bonus_random() % 1000u < bonus_drop_magic;
    if (heart && !bonus_heart_full()) bonus_yellow_make(pos, room, BONUS_ITEM_HEART);
    if (magic && bonus_max_magic() && !bonus_magic_full()) bonus_yellow_make(pos, room, BONUS_ITEM_MAGIC);
}

/* The loot window closes (the outermost call). */
static void bonus_drop_end(u32 pos, u32 room, u32 made) {
    (void)made;
    if (--bonus_drop_depth != 0 || !bonus_window_items) return;
    bonus_yellow_roll(pos, room);
}

WWHD_REPLACE(0x025D7F20, u32, bonus_fast_item2,
             (u32 pos, u32 item, u32 bit, u32 room, u32 type, u32 angle, u32 action, u32 scale)) {
    u32 actor = bonus_fast_item2_original(pos, item, bit, room, type, angle, action, scale);
    if (bonus_drop_depth) bonus_loot_item(bonus_actor_id(actor), item);
    return actor;
}

WWHD_REPLACE(0x025D8AB0, u32, bonus_fast_item,
             (u32 pos, u32 item, u32 room, u32 angle, u32 scale, f32 speed_f, f32 speed_y, f32 gravity, u32 bit,
              u32 create_func)) {
    u32 actor = bonus_fast_item_original(pos, item, room, angle, scale, speed_f, speed_y, gravity, bit, create_func);
    if (bonus_drop_depth) bonus_loot_item(bonus_actor_id(actor), item);
    return actor;
}

WWHD_REPLACE(0x025D8870, u32, bonus_create_item,
             (u32 pos, u32 item, u32 bit, u32 room, u32 type, u32 angle, u32 action, u32 scale)) {
    u32 id = bonus_create_item_original(pos, item, bit, room, type, angle, action, scale);
    if (bonus_making) return id;
    if (bonus_drop_depth) {
        bonus_loot_item(id, item);
    } else if (action == BONUS_PUFF_ACTION && id != 0 && id != 0xFFFFFFFFu) { /* a puff's own loot */
        bonus_drop_begin();
        bonus_loot_item(id, item);
        bonus_drop_end(pos, room, id);
    }
    return id;
}

/* daIball_c::createItem: the crystal ball breaks into its items. */
WWHD_REPLACE(0x0217BF6C, s32, bonus_ball_items, (void* self)) {
    const fopAc_ac_c* ball = (const fopAc_ac_c*)self;
    bonus_drop_begin();
    s32 result = bonus_ball_items_original(self);
    bonus_drop_end(self ? (u32)&ball->current.pos : 0, self ? (u32)(s32)ball->current.roomNo : 0, 1);
    return result;
}

/* fopAcM_createIball: a dying enemy's loot (its table, or the ball that keeps it). */
WWHD_REPLACE(0x025D9154, u32, bonus_iball, (u32 pos, u32 table, u32 room, u32 angle, u32 bit)) {
    bonus_drop_begin();
    u32 result = bonus_iball_original(pos, table, room, angle, bit);
    bonus_drop_end(pos, room, result);
    return result;
}

/* daItem_c::itemGetExecute: a yellow item gives its bonus. 0.3.59: every pickup on the ground is
 * counted in bonus_ground_pickup (item_share.h: what is picked up is not shared). */
static int bonus_ground_pickup;
static void bonus_item_get_inner(void* self);
WWHD_REPLACE(0x0217DE18, void, bonus_item_get, (void* self)) {
    ++bonus_ground_pickup;
    bonus_item_get_inner(self);
    --bonus_ground_pickup;
}

static void bonus_item_get_inner(void* self) {
    u32 elemental = bonus_elemental_of(self);
    if (elemental) {
        bonus_picking_type = elemental;
        bonus_item_get_original(self);
        bonus_picking_type = 0;
        return;
    }
    if (!bonus_yellow_item(self)) {
        bonus_item_get_original(self);
        return;
    }
    bonus_picking = 1;
    bonus_item_get_original(self);
    bonus_picking = 0;
}

/* execItemGet: the yellow heart or magic instead of the red heart or the magic. */
static void share_item_given(u32 item); /* item_share.h: a purchase or a gift shared (0.3.59) */
WWHD_REPLACE(0x0254DA38, void, bonus_exec_item_get, (u32 item)) {
    share_item_given(item);
    if (item >= BONUS_ITEM_ARROW_10 && item <= BONUS_ITEM_ARROW_30 && (bonus_picking_type || bonus_arrow_shown)) {
        u32 count = (item - BONUS_ITEM_ARROW_10 + 1u) * 10u, quiver = bonus_quiver(), cap = bonus_elemental_cap();
        u32 type = bonus_picking_type;
        bonus_picking_type = 0;
        bonus_load();
        if (type) { /* elemental: half the pack, to that type's stock */
            count /= 2u;
            u8* play = wwhd_play_get();
            u32 save = bonus_save_addr();
            if (type == bonus_arrow_shown && play && save) { /* shown now: the game counts it, up to the cap */
                volatile s16* pending = (volatile s16*)(play + BONUS_PLAY_ARROWS);
                u32 now = *(const volatile u8*)(save + BONUS_SAVE_ARROWS) + (u32)(*pending > 0 ? *pending : 0);
                u32 room = now >= cap ? 0u : cap - now;
                *pending = (s16)(*pending + (s16)(count > room ? room : count));
            } else {
                bonus_arrows[type] = bonus_arrows[type] + count > cap ? cap : bonus_arrows[type] + count;
            }
            bonus_dirty = 1;
            wwhd_log_int("multiplayer: elemental arrows picked up, type", (int)type);
        } else { /* normal arrows while an elemental stock is shown */
            bonus_arrow_normal = bonus_arrow_normal + count > quiver ? quiver : bonus_arrow_normal + count;
        }
        return;
    }
    if (!bonus_picking) {
        bonus_exec_item_get_original(item);
        return;
    }
    bonus_picking = 0;
    const char* result = item == BONUS_ITEM_HEART ? bonus_buy_heart() : bonus_buy_magic();
    wwhd_log(item == BONUS_ITEM_HEART ? "multiplayer: yellow heart picked up" : "multiplayer: yellow magic picked up");
    if (result[0] != 'o') wwhd_log(result); /* at its maximum meanwhile: nothing more */
}

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

static int bonus_ptr(u32 p) { return p >= 0x10000000u && p < 0x60000000u && !(p & 3u); } /* runtime MEM2 end (0.3.50) */

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

/* 0.3.5: the D-pad of the HUD's top left (Shortcut_00.szs of the game's 2D pack, read offline: the
 * picture CrossKeyBtn_00, 58 x 58, the Wind Waker's icon P_Baton_00 above it at (8, 39), 0.6 of 118;
 * nothing on its down arrow), found in the screen that has it while a Golden Pig waits to be called;
 * golden_pig.h draws the pig there. The step it was last drawn: no icon over a HUD not drawn. */
static u32 bonus_cross, bonus_cross_parts, bonus_cross_root;
static unsigned long long bonus_cross_step;

static void bonus_cross_draw(u32 self) {
    u32 root = bonus_root_of(self);
    if (!root) return;
    if (!bonus_ptr(bonus_cross) || !bonus_name_is(bonus_cross, "CrossKeyBtn_00") ||
        bonus_root_of(bonus_cross_parts) != bonus_cross_root) {
        u32 cross = bonus_find(root, "CrossKeyBtn_00", 7);
        if (!cross) return;
        bonus_cross = cross;
        bonus_cross_parts = self;
        bonus_cross_root = root;
        wwhd_log("multiplayer: golden pig: the HUD's D-pad found");
    }
    if (self == bonus_cross_parts) bonus_cross_step = wwhd_logic_step();
}

/* hdParts_c::draw (026F8B54): the HUD screen, found once (again after a state load); before the game
 * draws it, its big heart is made small while yellow hearts follow (bonus_shrink_current). */
static void ck_parts_draw(u32 self); /* color_key.h: the HUD's rupees (0.3.81) */
WWHD_HOOK(0x026F8B54, bonus_parts_draw, (u32 self, u32 viewport)) {
    (void)viewport;
    ck_parts_draw(self);
    if (bonus_pigs) bonus_cross_draw(self);
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

/* 0.3.101: layout space to the mod's HUD at any aspect ratio. The runtime (aspect.cpp) keeps the
 * HD layouts' 16:9 space centred in the TV picture, moves the HUD panes out to the new edges (their
 * global matrices hold the moved places) and fits the layout space: a wider screen shows more layout
 * units across at the same height, a taller one (16:10, 4:3) shows 720 * ky units down, ky = (16/9) /
 * aspect. The mod's HUD (guest_hud.h anchored) is 720 units tall whatever the shape and, anchored at
 * the centre, keeps x 640 at the picture's centre: a layout point (lx, ly) is at 640 + lx / ky,
 * 360 - ly / ky, sizes / ky. The aspect: the main camera's View::mAspect (play + 0x5FA4 mpView,
 * +0xD8, d_drawlist.h), which the runtime sets to the game's aspect every step (aspect.cpp
 * store_aspect). Until 0.3.100 the places were taken as 1280 x 720 from the top left: right only at
 * 16:9. Every element placed from the game's panes is drawn anchored at the centre. */
#define BONUS_ASPECT_16_9 (16.0f / 9.0f)
static f32 bonus_hud_k(void) {
    f32 a = hud_view_aspect(); /* mod.c */
    return a < BONUS_ASPECT_16_9 ? BONUS_ASPECT_16_9 / a : 1.0f;
}

/* A picture in TV coordinates: left, top, width, height (origin given: centre or left). */
static void bonus_rect(u32 pane, int left_origin, f32* x, f32* y, f32* w, f32* h) {
    const volatile f32* m = (const volatile f32*)(pane + BONUS_PANE_MTX);
    const volatile f32* size = (const volatile f32*)(pane + BONUS_PANE_SIZE);
    f32 k = bonus_hud_k();
    *w = size[0] * m[0] / k;
    *h = size[1] * m[5] / k;
    if (*w < 0.0f) *w = -*w;
    if (*h < 0.0f) *h = -*h;
    *x = 640.0f + m[3] / k - (left_origin ? 0.0f : *w * 0.5f);
    *y = 360.0f - m[7] / k - *h * 0.5f;
}

/* One element; anything the runtime would refuse (it then drops the mod's whole list, names and
 * crowns included: guest_hud.h geometry) is not sent. */
/* A pane's centre on the TV and its drawn scale (x axis of its global matrix). */
static void bonus_center(u32 pane, f32* x, f32* y, f32* scale) {
    const volatile f32* m = (const volatile f32*)(pane + BONUS_PANE_MTX);
    f32 k = bonus_hud_k();
    *x = 640.0f + m[3] / k;
    *y = 360.0f - m[7] / k;
    *scale = (m[0] < 0.0f ? -m[0] : m[0]) / k;
}

static void bonus_emit(u32 list, u32 image, f32 x, f32 y, f32 w, f32 h, f32 part, u32 rgb, f32 alpha) {
    if (!image || !(alpha > 0.0f) || alpha > 1.0f) return;
    if (!(x > -4096.0f && x < 4096.0f && y > -4096.0f && y < 4096.0f)) return;      /* NaN too */
    if (!(w > 0.0f && w < 4096.0f && h > 0.0f && h < 4096.0f && part > 0.0f && part <= 1.0f)) return;
    bonus_element = (wwhd_hud_element){.kind = WWHD_HUD_IMAGE, .anchor = WWHD_HUD_CENTER, .blend = WWHD_HUD_ALPHA,
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
    bonus_element = (wwhd_hud_element){.kind = WWHD_HUD_IMAGE, .anchor = WWHD_HUD_CENTER, .blend = WWHD_HUD_ALPHA,
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

/* 0.3.13: the centre of heart slot j. The game lays out (and draws) only what it shows: the matrix
 * of a pane it hides is stale (seen: yellow hearts at the screen's bottom left, or a slot too far).
 * 0.3.14: still a slot too far at times (a screenshot: three hearts, the yellow one a slot and a half
 * on), from the fill of an empty or partial heart, hidden. Every slot is now placed from the
 * containers' bases (P_Base_00) of slots 0 and 1 (and 10), shown whenever the player has those
 * containers (three at least): ten a row (L_Heart_00..09, then 10..19 under them), the step between
 * slots 0 and 1, between rows the one between slots 0 and 10 when shown, else that step downwards. */
#define BONUS_ROW 10u
static void bonus_slot_center(u32 j, u32 first, f32* x, f32* y) {
    f32 s;
    if (first < 2u) {
        bonus_center(bonus_base[j < BONUS_SLOTS ? j : BONUS_SLOTS - 1u], x, y, &s);
        return;
    }
    f32 x0, y0, x1, y1;
    bonus_center(bonus_base[0], &x0, &y0, &s);
    bonus_center(bonus_base[1], &x1, &y1, &s);
    f32 sx = x1 - x0, sy = y1 - y0;
    f32 rx = 0.0f, ry = sx < 0.0f ? -sx : sx;
    if (first > BONUS_ROW) {
        f32 x10, y10;
        bonus_center(bonus_base[BONUS_ROW], &x10, &y10, &s);
        rx = x10 - x0;
        ry = y10 - y0;
    }
    u32 row = j / BONUS_ROW, col = j % BONUS_ROW;
    if (row > 1u) { /* past the 20th: on along the second row */
        col += (row - 1u) * BONUS_ROW;
        row = 1u;
    }
    *x = x0 + sx * (f32)col + rx * (f32)row;
    *y = y0 + sy * (f32)col + ry * (f32)row;
}

/* 0.3.100: why the yellow hearts are not drawn this step (mod.c hud_diag): 0 drawn, 1 nothing to
 * draw, 2 no Link this step, 3 the save not ready, 4 an event, 5 the HUD hidden, 6 a menu, 7 the
 * HUD layout not read */
static u32 bonus_skip;
static void bonus_draw(u32 list) {
    bonus_skip = 1;
    if (!bonus_loaded || (!bonus_hearts && !bonus_magic)) return;
    bonus_skip = 2;
    if (!bonus_link_alive()) return;
    bonus_skip = 3;
    if (!bonus_save_ready()) return;
    const u8* play = wwhd_play_get();
    bonus_skip = 4;
    if (!play || play[WWHD_PLAY_EVENT_RUNNING_OFFSET]) return;
    bonus_skip = hud_hidden ? 5 : 6;
    if (hud_hidden || chart_menu_open()) return;
    bonus_skip = 7;
    if (!bonus_layout_valid()) return;
    bonus_skip = 0;
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
        /* 0.3.15: the containers the HUD shows (their bases visible), when it shows fewer: seen, the
         * save's maximum 20 hearts, life 13 quarters, three hearts on screen; the yellow heart went
         * after a quarter heart the HUD did not show (a slot too far) */
        u32 visible = 0;
        while (visible != BONUS_SLOTS && bonus_alpha(bonus_base[visible]) > 0.0f) ++visible;
        if (visible >= 1u && visible < first) first = visible;
        u32 shown = *(const volatile u16*)(play + BONUS_PLAY_NOW_LIFE);
        if (shown > first * 4u) shown = first * 4u;
        u32 start = (shown + 3u) / 4u; /* the first slot after the last (even partial) heart */
        if (start > first) start = first;
        u32 count = (bonus_hearts + 3u) / 4u;
        { /* the placement's inputs, logged when they change (0.3.14: a yellow heart a slot too far) */
            static u32 logged_first = ~0u, logged_start = ~0u, logged_count;
            if (logged_count < 12 && (first != logged_first || start != logged_start)) {
                ++logged_count;
                logged_first = first;
                logged_start = start;
                f32 hx, hy, bx, by, sk, px, py;
                wwhd_log_int("multiplayer: yellow hearts: containers", (int)first);
                wwhd_log_int("multiplayer:   life shown (quarters)", (int)shown);
                wwhd_log_int("multiplayer:   first yellow slot", (int)start);
                for (u32 k = 0; k != 4 && k != BONUS_SLOTS; ++k) {
                    bonus_center(bonus_heart[k], &hx, &hy, &sk);
                    bonus_center(bonus_base[k], &bx, &by, &sk);
                    wwhd_log_int("multiplayer:   slot", (int)k);
                    wwhd_log_int("multiplayer:     fill x", (int)hx);
                    wwhd_log_int("multiplayer:     base x", (int)bx);
                }
                bonus_slot_center(start, first, &px, &py);
                wwhd_log_int("multiplayer:   yellow x", (int)px);
                wwhd_log_int("multiplayer:   yellow y", (int)py);
            }
        }
        /* The game draws its hearts small but its current one: the yellow hearts are small but the
         * last, which also takes the glow (the game's big heart lost both: bonus_shrink_current). */
        f32 hud_k = bonus_hud_k();
        f32 small = bonus_small_scale / hud_k, big = bonus_big_scale / hud_k; /* layout scales (bonus_shrink_current) */
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
            f32 cx, cy;
            bonus_slot_center(start + i, first, &cx, &cy);
            f32 k = i + 1u == count ? big : small;
            f32 w = heart_size[0] * k, h = heart_size[1] * k;
            f32 bw = base_size[0] * k, bh = base_size[1] * k;
            u32 quarters = bonus_hearts - i * 4u;
            if (quarters > 4u) quarters = 4u;
            if (i + 1u == count && bonus_current >= 0 && bonus_light[0] && bonus_light_scale > 0.0f) {
                const volatile f32* light_size = (const volatile f32*)(bonus_light[0] + BONUS_PANE_SIZE);
                f32 lw = light_size[0] * bonus_light_scale / hud_k, lh = light_size[1] * bonus_light_scale / hud_k;
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
            f32 mk = m[0] / bonus_hud_k();       /* layout units to HUD units */
            f32 unit = BONUS_MAGIC_UNIT * mk;
            f32 start = fx + fw + 4.0f * mk;     /* a second meter right after the game's frame */
            f32 inner = gx - fx;                 /* the fill's inset in its frame */
            bonus_emit(list, bonus_frame_image, start, fy, unit * (f32)BONUS_MAGIC_MAX + 2.0f * inner, fh, 1.0f, 0x2A2000u,
                       alpha * (220.0f / 255.0f));
            bonus_emit(list, bonus_fill_image, start + inner, gy, unit * (f32)bonus_magic, gh, 1.0f, 0xFFFFFFu, alpha);
        }
    }
}
