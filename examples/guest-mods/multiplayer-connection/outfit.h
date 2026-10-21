/* Hero's clothes or casual clothes ("pyjama"), chosen by each player (option / launcher.cfg
 * "outfit": auto, hero, casual) and shown on its Link puppet in the other games.
 *
 * The game (decomp d_a_player_main_04b.cpp daPy_lk_c::playerInit 02410BE8): every texture
 * "linktexS3TC" of the SHARED Link model data gets the casual header (archive "Link" resource 0x71);
 * mpCurrLinktex (+0x454) lists those table entries; unless the casual clothes are worn
 * (save event 0x2A80 not set, or save + 0x1C0 non-zero: mNoResetFlg1 +0x3BC |=
 * daPyFlg1_CASUAL_CLOTHES 0x8), the entries are swapped with the 0x24-byte header copies at +0x48C.
 * So, for a Link, the entries hold the clothes worn and the copies the other ones; playerDelete
 * swaps back when the flag is set (d_a_player_main_04.cpp). daPy_lk_c::draw (023D9820) hides the
 * hat and buckle while the flag is set (hideHatAndBackle).
 *
 * The mod writes the headers (outfit_force, 0.3.27; the table's dirty mask as setResTIMG does,
 * tunic.h tunic_mark_dirty), keeping the game's own rule, entries = clothes worn:
 *  - the local Link: its chosen clothes each step, the flag following (playerDelete then restores
 *    the table as usual);
 *  - a puppet: its player's clothes are written only while the puppet draws (puppet.h
 *    puppet_draw, like the tunic copies), and the puppet's own flag says its clothes (its hat). The worn clothes go in ActorPlayerSync extras bit MP_EXTRA_CASUAL.
 *
 * The hero's clothes are worn only once they are the player's (playerInit's own rule: save event
 * 0x2A80, read with the game's dSv_event_c::isEventBit 025B8B94 on *(0x101F84DC) + 0x644): until
 * then a player who chose "hero" stays in the casual clothes.
 *
 * 0.3.63 (the user's decision after the friends' session: pyjama and hero's clothes mixed, at random,
 * at the pyjama -> hero's clothes moment): no choice any more. outfit_wanted is always OUTFIT_AUTO
 * (the game's own flag, set by playerInit from save event 0x2A80, which the shared progression gives
 * every player as soon as one has it), and the puppets no longer write their player's clothes
 * around their draw (outfit_puppet_begin / outfit_puppet_end unused): every Link of a game wears its
 * local Link's clothes. The local step still writes the clothes its flag says into the shared
 * entries (the repair of the 0.3.27 note: a puppet's playerInit / playerDelete swapping them). The
 * launcher's pyjama box is locked and shows hero_clothes. */
#pragma once
#include "wwhd_guest.h"
#include "wwhd/link.h"
#include "mp_protocol.h"
#include "tunic.h" /* tunic_mark_dirty */

#define OUTFIT_FLG1 0x3BC          /* daPy_lk_c::mNoResetFlg1 */
#define OUTFIT_CASUAL 0x8u         /* daPyFlg1_CASUAL_CLOTHES */
#define OUTFIT_COPIES 0x48C        /* the other clothes' headers, 0x24 each */
#define OUTFIT_MAX 10
#define OUTFIT_HEADER 0x24
#define OUTFIT_TABLE 0x30          /* J3DModelData texture table: u16 num, u32 res, dirty, seed */

enum { OUTFIT_AUTO = -1, OUTFIT_HERO = 0, OUTFIT_CASUAL_WANTED = 1 };

static int outfit_wanted = OUTFIT_AUTO;
static u32 outfit_local;    /* the local Link, from its step */

#define OUTFIT_EVENT 0x644          /* dSv_event_c from the save pointer (playerInit) */
#define OUTFIT_HERO_EVENT 0x2A80u   /* the hero's clothes received */

WWHD_GAME_FUNC(0x025B8B94, u32, outfit_is_event_bit, (u32 self, u32 flag));

static int outfit_hero_unlocked(void) {
    u32 save = WWHD_GAME_DATA(0x101F84DC, u32);
    return save && outfit_is_event_bit(save + OUTFIT_EVENT, OUTFIT_HERO_EVENT) != 0;
}

static u32 outfit_ld32(u32 a) { return *(const volatile u32*)a; }

static int outfit_is_casual(u32 link) {
    return link && (outfit_ld32(link + OUTFIT_FLG1) & OUTFIT_CASUAL) != 0;
}

/* 0.3.69: nothing is written any more. Seen after the hero's clothes came (friends' session,
 * 2026-10-22): the hero's model with the pyjama's textures. The game keeps the shared table right by
 * itself for one Link (playerInit copies the table's headers, writes the casual one, swaps back for
 * the hero's clothes; playerDelete swaps back for the casual ones), and the puppets' creation and
 * deletion keep it untouched (puppet.h puppet_phase_2 / puppet_delete: puppet_texture_table saves
 * the headers and puts them back). What still wrote into it was this file's outfit_local_step,
 * every step: headers learnt once per model data (outfit_known, cleared only when the model data
 * pointer changed), with their absolute HD texture object at +0x20, written again after the Link
 * archive came back at the same address (an area change, the hero's clothes): stale headers. So
 * the local Link's clothes are the game's, untouched; this file only reads them. */
static u32 outfit_casual_res;                         /* getRes("Link", 0x71), puppet.h */
static u32 outfit_casual_texture(void);              /* its HD object, puppet.h */

/* The local Link's step: nothing to do any more (kept for mod.c). */
static void outfit_local_step(u32 link) { outfit_local = link; }

/* 0.3.71: each puppet drawn in the clothes of its own flag (its playerInit: the save's event
 * 0x2A80, the same for every player with the shared progression; the flag also hides or shows its
 * hat). Seen (2026-10-22, both games): after the hero's clothes came, the puppets with their hat but
 * the pyjama's textures. The shared table a puppet draws with is the local Link's only when the
 * local Link wears no skin: a skinned local Link changes the clothes of its skin's table (the
 * cutscene's swap, d_a_player_main_03.cpp setDemoData), and nothing changes the shared one (the
 * puppets' creation and deletion keep it, puppet.h). So around a puppet's draw only the HD texture
 * object of each clothes entry (+0x20, as tunic.h swaps its colour copies) is the one of its
 * clothes: the casual object (getRes("Link", 0x71), outfit_casual_texture) or the hero's, taken
 * from the entry or the puppet's own copies (+0x48C), whichever is not the casual one, and kept
 * per entry for the model data. Put back after the draw. */
#define OUTFIT_LIST_MAX 10
#define OUTFIT_HEADER_BYTES 0x24
#define OUTFIT_HD 0x20
#define OUTFIT_TABLE_RES 4
/* 0.3.74: the hero's objects per model data (Link's and each skin's) */
#define OUTFIT_DATAS 16
static u32 outfit_hero_data[OUTFIT_DATAS];
static u32 outfit_hero_all[OUTFIT_DATAS][OUTFIT_LIST_MAX];
static u32 outfit_hero_next;
static u32 outfit_swap_table, outfit_swap_count;
static u32 outfit_swap_entry[OUTFIT_LIST_MAX], outfit_swap_old[OUTFIT_LIST_MAX], outfit_swap_index[OUTFIT_LIST_MAX];

static u32* outfit_hero_for(u32 data) {
    for (u32 i = 0; i != OUTFIT_DATAS; ++i)
        if (outfit_hero_data[i] == data) return outfit_hero_all[i];
    u32 i = outfit_hero_next++ % OUTFIT_DATAS;
    outfit_hero_data[i] = data;
    for (u32 k = 0; k != OUTFIT_LIST_MAX; ++k) outfit_hero_all[i][k] = 0;
    return outfit_hero_all[i];
}

/* casual_object: the casual clothes' object of the model data the puppet draws with (Link's,
 * outfit_casual_texture, or its skin's, skin_casual_object). 0.3.74: also for the skinned puppets
 * (before, only the others: a skinned puppet draws with its skin's table, which its own playerInit
 * set once, in the clothes of that moment). */
static void outfit_puppet_draw_begin(u32 puppet, int casual, u32 casual_object) {
    outfit_swap_table = outfit_swap_count = 0;
    if (puppet) { /* 0.3.73: its flag (hat and buckle, daPy_lk_c::draw) follows its player's clothes */
        volatile u32* flags = (volatile u32*)(puppet + OUTFIT_FLG1);
        if (casual) *flags |= OUTFIT_CASUAL;
        else *flags &= ~OUTFIT_CASUAL;
    }
    const daPy_lk_c* l = (const daPy_lk_c*)puppet;
    u32 data = puppet ? (u32)l->mpCLModelData : 0;
    u32 list = puppet ? l->mpCurrLinktex : 0;
    { /* 0.3.74: logged at each change of a model's wanted clothes (the skinned puppets' hunt) */
        static u32 last_data, last_state = 0xFFFFFFFFu, logged;
        u32 state = (u32)casual | (casual_object ? 2u : 0u) | (list ? 4u : 0u);
        if ((data != last_data || state != last_state) && logged++ < 24) {
            wwhd_log_hex("multiplayer: clothes: puppet model data", data);
            wwhd_log_int("multiplayer: clothes:   casual wanted", casual);
            wwhd_log_hex("multiplayer: clothes:   casual object", casual_object);
            wwhd_log_int("multiplayer: clothes:   clothes entries listed", list ? (int)outfit_ld32(list) : -1);
        }
        last_data = data;
        last_state = state;
    }
    if (!data || !list || !casual_object) return;
    u32 table = outfit_ld32(data + OUTFIT_TABLE);
    u32 res = table ? outfit_ld32(table + OUTFIT_TABLE_RES) : 0;
    u32 count = outfit_ld32(list), headers = outfit_ld32(list + 8);
    if (!res || !headers) return;
    if (count > OUTFIT_LIST_MAX) count = OUTFIT_LIST_MAX;
    u32* outfit_hero = outfit_hero_for(data);
    for (u32 k = 0; k != count; ++k) {
        u32 entry = outfit_ld32(headers + k * 4);
        if (!entry || entry < res || (entry - res) % OUTFIT_HEADER_BYTES) continue;
        u32 now = outfit_ld32(entry + OUTFIT_HD);
        u32 copy = outfit_ld32(puppet + OUTFIT_COPIES + k * OUTFIT_HEADER_BYTES + OUTFIT_HD);
        if (now && now != casual_object) outfit_hero[k] = now; /* learnt from what the table shows */
        else if (copy && copy != casual_object) outfit_hero[k] = copy;
        u32 want = casual ? casual_object : outfit_hero[k];
        if (!want || want == now) continue;
        outfit_swap_entry[outfit_swap_count] = entry;
        outfit_swap_old[outfit_swap_count] = now;
        outfit_swap_index[outfit_swap_count] = (entry - res) / OUTFIT_HEADER_BYTES;
        ++outfit_swap_count;
        *(volatile u32*)(entry + OUTFIT_HD) = want;
        tunic_mark_dirty(table, (entry - res) / OUTFIT_HEADER_BYTES);
    }
    if (outfit_swap_count) outfit_swap_table = table;
}

static void outfit_puppet_draw_end(void) {
    if (!outfit_swap_table) return;
    for (u32 k = 0; k != outfit_swap_count; ++k) {
        *(volatile u32*)(outfit_swap_entry[k] + OUTFIT_HD) = outfit_swap_old[k];
        tunic_mark_dirty(outfit_swap_table, outfit_swap_index[k]);
    }
    outfit_swap_table = outfit_swap_count = 0;
}
