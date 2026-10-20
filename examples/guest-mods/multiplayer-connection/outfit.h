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

/* 0.3.27: the clothes are written, never swapped. The model data is shared by every Link and each
 * Link's playerInit / playerDelete swaps it with its own copies (+0x48C): a puppet created or deleted
 * while the table held other clothes left mixed headers (the pyjama texture on the hero's clothes).
 * So the mod keeps, for each listed entry k, a header of each clothes, told apart by their HD texture
 * object (the casual one is getRes("Link", 0x71)'s, outfit_casual_texture), learnt from the local
 * Link's entries and copies; the casual one, when not seen, is made as playerInit makes it
 * (setResTIMG: the 0x71 header, palette and image offsets moved to the entry, its HD object).
 * Headers hold offsets from their own entry (playerInit copies them in place): a header kept for
 * entry k is written only to entry k of the same model data. Then:
 *  - each step of the local Link: its entries get its clothes, its copies the other ones (what
 *    playerDelete expects), its flag (hat) follows;
 *  - around a puppet's draw: the entries get the puppet's clothes, then the local Link's again.
 * A header is written only when it differs (the table's dirty mask as setResTIMG does). */
static u8 outfit_kept[2][OUTFIT_MAX][OUTFIT_HEADER]; /* [casual][entry] */
static u32 outfit_known[2];                           /* bit per entry */
static u32 outfit_data;                               /* the model data they belong to */
static u32 outfit_casual_res;                         /* getRes("Link", 0x71), puppet.h */
static u32 outfit_casual_texture(void);              /* its HD object, puppet.h */

typedef struct { u32 table, res, headers, count; } outfit_list;

static int outfit_list_of(u32 link, outfit_list* o) {
    const daPy_lk_c* l = (const daPy_lk_c*)link;
    u32 list = link ? l->mpCurrLinktex : 0;
    u32 data = link ? (u32)l->mpCLModelData : 0;
    if (!list || !data) return 0;
    o->table = outfit_ld32(data + OUTFIT_TABLE);
    o->res = o->table ? outfit_ld32(o->table + 4) : 0;
    o->count = outfit_ld32(list);
    o->headers = outfit_ld32(list + 8);
    if (!o->res || !o->headers || !o->count) return 0;
    if (o->count > OUTFIT_MAX) o->count = OUTFIT_MAX;
    if (data != outfit_data) { /* another model data: its own headers */
        outfit_data = data;
        outfit_known[0] = outfit_known[1] = 0;
    }
    return 1;
}

static u32 outfit_entry(const outfit_list* o, u32 k) {
    u32 entry = outfit_ld32(o->headers + k * 4);
    if (!entry || entry < o->res || (entry - o->res) % OUTFIT_HEADER) return 0;
    return entry;
}

/* 1 casual, 0 hero, -1 neither (no HD object). */
static int outfit_kind(u32 header, u32 casual) {
    u32 hd = outfit_ld32(header + 0x20);
    if (!hd || !casual) return -1;
    return hd == casual;
}

static void outfit_learn(u32 header, u32 k, u32 casual) {
    int kind = outfit_kind(header, casual);
    if (kind < 0 || (outfit_known[kind] >> k & 1)) return;
    for (u32 w = 0; w != OUTFIT_HEADER; w += 4) *(u32*)&outfit_kept[kind][k][w] = outfit_ld32(header + w);
    outfit_known[kind] |= 1u << k;
}

/* playerInit's casual header for an entry (d_a_player_main_04b.cpp, setResTIMG). */
static void outfit_make_casual(u32 entry, u32 k) {
    u32 src = outfit_casual_res;
    if (!src || (outfit_known[1] >> k & 1)) return;
    u32* dst = (u32*)outfit_kept[1][k];
    for (u32 w = 0; w != OUTFIT_HEADER; w += 4) dst[w / 4] = outfit_ld32(src + w);
    dst[0x0C / 4] += src - entry; /* paletteOffset */
    dst[0x1C / 4] += src - entry; /* imageOffset */
    outfit_known[1] |= 1u << k;
}

static int outfit_write(u32 at, const u8* header) {
    int changed = 0;
    for (u32 w = 0; w != OUTFIT_HEADER; w += 4) {
        u32 v = *(const u32*)&header[w];
        if (outfit_ld32(at + w) != v) {
            outfit_st32(at + w, v);
            changed = 1;
        }
    }
    return changed;
}

/* The entries of the Link's model data get the given clothes (and the Link's copies the other ones
 * when copies is set); 0 when a header is not known. */
static int outfit_force(u32 link, int casual, int copies) {
    outfit_list o;
    if (!outfit_list_of(link, &o)) return 0;
    for (u32 k = 0; k != o.count; ++k) {
        if (!outfit_entry(&o, k)) continue;
        if (!(outfit_known[casual] >> k & 1)) return 0;
    }
    for (u32 k = 0; k != o.count; ++k) {
        u32 entry = outfit_entry(&o, k);
        if (!entry) continue;
        if (outfit_write(entry, outfit_kept[casual][k])) tunic_mark_dirty(o.table, (entry - o.res) / OUTFIT_HEADER);
        if (copies && (outfit_known[!casual] >> k & 1))
            outfit_write(link + OUTFIT_COPIES + k * OUTFIT_HEADER, outfit_kept[!casual][k]);
    }
    return 1;
}

/* Learns both clothes from the Link's entries and copies. */
static void outfit_learn_link(u32 link) {
    u32 casual = outfit_casual_texture();
    outfit_list o;
    if (!casual || !outfit_list_of(link, &o)) return;
    for (u32 k = 0; k != o.count; ++k) {
        u32 entry = outfit_entry(&o, k);
        if (!entry) continue;
        outfit_learn(entry, k, casual);
        outfit_learn(link + OUTFIT_COPIES + k * OUTFIT_HEADER, k, casual);
        outfit_make_casual(entry, k);
    }
}

/* The local Link's step: the chosen clothes (auto: the game's flag). */
static void outfit_local_step(u32 link) {
    outfit_local = link;
    if (!link) return;
    outfit_learn_link(link);
    int wanted = outfit_wanted;
    if (wanted == OUTFIT_HERO && !outfit_hero_unlocked()) wanted = OUTFIT_CASUAL_WANTED; /* not unlocked yet */
    int casual = wanted == OUTFIT_AUTO ? outfit_is_casual(link) : wanted == OUTFIT_CASUAL_WANTED;
    if (!outfit_force(link, casual, 1)) return;
    volatile u32* flags = (volatile u32*)(link + OUTFIT_FLG1);
    if (casual != outfit_is_casual(link)) {
        if (casual) *flags |= OUTFIT_CASUAL;
        else *flags &= ~OUTFIT_CASUAL;
        if (outfit_logged++ < 4) wwhd_log_int("multiplayer: clothes set, casual", casual);
    }
}

/* Around a puppet's draw: its clothes in the shared entries. Returns 1 when written
 * (outfit_puppet_end must follow). */
static int outfit_puppet_begin(int remote_casual) {
    if (!outfit_local) return 0;
    return outfit_force(outfit_local, remote_casual, 0);
}

static void outfit_puppet_end(int written) {
    if (written) outfit_force(outfit_local, outfit_is_casual(outfit_local), 0);
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
