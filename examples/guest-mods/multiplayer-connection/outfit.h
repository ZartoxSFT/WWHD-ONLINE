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
 * outfit_swap does that swap (whole headers, then the table's dirty mask as setResTIMG does,
 * tunic.h tunic_mark_dirty), which keeps the game's own rule, entries = clothes worn:
 *  - the local Link: when the chosen clothes differ from the worn ones (each step), the swap and
 *    the flag toggle are kept (playerDelete then restores the table as usual);
 *  - a puppet whose player wears the other clothes: the local Link's swap is done only while the
 *    puppet draws (puppet.h puppet_draw, like the tunic copies), and the puppet's own flag says
 *    its clothes (its hat). The worn clothes go in ActorPlayerSync extras bit MP_EXTRA_CASUAL.
 *
 * The hero's clothes are worn only once they are the player's (playerInit's own rule: save event
 * 0x2A80, read with the game's dSv_event_c::isEventBit 025B8B94 on *(0x101F84DC) + 0x644): until
 * then a player who chose "hero" stays in the casual clothes. */
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
static u32 outfit_logged;

#define OUTFIT_EVENT 0x644          /* dSv_event_c from the save pointer (playerInit) */
#define OUTFIT_HERO_EVENT 0x2A80u   /* the hero's clothes received */

WWHD_GAME_FUNC(0x025B8B94, u32, outfit_is_event_bit, (u32 self, u32 flag));

static int outfit_hero_unlocked(void) {
    u32 save = WWHD_GAME_DATA(0x101F84DC, u32);
    return save && outfit_is_event_bit(save + OUTFIT_EVENT, OUTFIT_HERO_EVENT) != 0;
}

static u32 outfit_ld32(u32 a) { return *(const volatile u32*)a; }
static void outfit_st32(u32 a, u32 v) { *(volatile u32*)a = v; }

static int outfit_is_casual(u32 link) {
    return link && (outfit_ld32(link + OUTFIT_FLG1) & OUTFIT_CASUAL) != 0;
}

/* Swaps the Link's worn and other clothes headers in its model data; 1 when done. */
static int outfit_swap(u32 link) {
    const daPy_lk_c* l = (const daPy_lk_c*)link;
    u32 list = l->mpCurrLinktex;
    u32 data = (u32)l->mpCLModelData;
    if (!list || !data) return 0;
    u32 table = outfit_ld32(data + OUTFIT_TABLE);
    if (!table) return 0;
    u32 res = outfit_ld32(table + 4);
    u32 count = outfit_ld32(list);
    u32 headers = outfit_ld32(list + 8);
    if (!res || !headers || !count) return 0;
    if (count > OUTFIT_MAX) count = OUTFIT_MAX;
    for (u32 k = 0; k != count; ++k) {
        u32 entry = outfit_ld32(headers + k * 4);
        if (!entry || entry < res || (entry - res) % OUTFIT_HEADER) continue;
        u32 copy = link + OUTFIT_COPIES + k * OUTFIT_HEADER;
        for (u32 w = 0; w != OUTFIT_HEADER; w += 4) {
            u32 t = outfit_ld32(entry + w);
            outfit_st32(entry + w, outfit_ld32(copy + w));
            outfit_st32(copy + w, t);
        }
        tunic_mark_dirty(table, (entry - res) / OUTFIT_HEADER);
    }
    return 1;
}

/* The local Link's step: the chosen clothes. */
static void outfit_local_step(u32 link) {
    outfit_local = link;
    int wanted = outfit_wanted;
    if (wanted == OUTFIT_HERO && !outfit_hero_unlocked()) wanted = OUTFIT_CASUAL_WANTED; /* not unlocked yet */
    if (!link || wanted == OUTFIT_AUTO) return;
    int casual = outfit_is_casual(link);
    if (casual == (wanted == OUTFIT_CASUAL_WANTED)) return;
    if (!outfit_swap(link)) return;
    volatile u32* flags = (volatile u32*)(link + OUTFIT_FLG1);
    *flags ^= OUTFIT_CASUAL;
    if (!outfit_logged++) wwhd_log_int("multiplayer: clothes changed, casual", !casual);
}

/* Around a puppet's draw: the local Link's other clothes while the puppet wears them. Returns
 * 1 when swapped (outfit_puppet_end must follow). */
static int outfit_puppet_begin(int remote_casual) {
    if (!outfit_local || remote_casual == outfit_is_casual(outfit_local)) return 0;
    return outfit_swap(outfit_local);
}

static void outfit_puppet_end(int swapped) {
    if (swapped) outfit_swap(outfit_local);
}

/* "auto", "hero" or "casual" (option and launcher.cfg "outfit"). */
static int outfit_parse(const char* text, int fallback) {
    if (text[0] == 'h' && text[1] == 'e' && text[2] == 'r' && text[3] == 'o' && !text[4]) return OUTFIT_HERO;
    if (text[0] == 'c' && text[1] == 'a' && text[2] == 's' && text[3] == 'u' && text[4] == 'a' && text[5] == 'l' &&
        !text[6])
        return OUTFIT_CASUAL_WANTED;
    if (text[0] == 'a' && text[1] == 'u' && text[2] == 't' && text[3] == 'o' && !text[4]) return OUTFIT_AUTO;
    return fallback;
}
