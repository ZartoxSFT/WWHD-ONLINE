/* The shared progression (the historical SaveServer, save/SaveServer.java; the GameCube client's
 * SaveServer.cpp): the save's dSv_save_c (0x778 bytes, decomp d_save.h) at *(0x101F84DC) + 0x20
 * (dComIfGs_info, the save base of d_save.h; the port's story code reads it the same way), compared
 * every step with a copy of what the server last had (the GameCube client's mirror), along the map
 * of the SaveServer initializer (mp_protocol.h mp_save_elements). A value that changed goes to the
 * server (CLIENT_SAVE_UPDATE), which checks its offset and sends it to the other players
 * (SERVER_SAVE_UPDATE, after SERVER_SAVE_FROM with the player's name).
 *
 * Each player's own values never travel and are never overwritten (sv_local_ranges): life, rupees,
 * magic, the selected items, the clock, date and wind (the world's clock comes from the server,
 * world_sync.h), the restart place, the bottles' contents, the pictures, arrows and bombs carried,
 * the bags and their counts, the player's name and options, the restart (priest), the old statuses
 * and the reserve. The rest is shared: hearts, wallet and magic meters, the items, the item flags,
 * quiver and bomb bag sizes, collection (songs, pearls, triforce), the charts, the 16 stage memories
 * (switches, chests, keys, rooms visited), the ocean and the event flags.
 *
 * The stage memory the game uses is dSv_info_c::mMemory (+0x778), copied from / to the save's slot
 * of the stage on stage loads (d_stage_3.cpp getSave, d_stage_4.cpp putSave; slot =
 * stage_stag_info_class::mProp (+9) >> 1 & 0x7F, the stag info from play + 0x5150's
 * dStage_stageDt_c::getStagInfo 025C5040): as the GameCube client did, the current memory goes into
 * the save before the comparison (putSave 025B9D24) and comes back after an update that touched
 * that slot (getSave 025B9C9C).
 *
 * Once connected, the whole shared part goes to the server in CLIENT_SAVE_MERGE (WWHD): the server
 * keeps the union of every player's progression and sends back what this player lacks.
 *
 * Notices (sv_notices): what another player got, with the game's own icon of the item
 * (itemForSlot 0262F880 gives its icon frame, 0x37 for none; wwhd_hud_game_icon reads that icon
 * from the player's own 2D pack). Bottom left: a new item in a place, a song, a pearl, a triforce
 * shard, a chest opened. Bottom right, the upgrades: a heart piece or container, a bigger wallet,
 * quiver or bomb bag, more magic, the swords and the Master Sword's power (dSv_player_collect_c
 * mCollect[0] at 0xB4, bits 0-3: d_item_2.cpp item_func_sword / master_sword / lv3_sword /
 * master_sword_ex), the shields (mCollect[1], bits 0-1), the Power Bracelets (mCollect[2] bit 0,
 * d_item.cpp item_func_pwr_groove), the charms (mCollect[3] / [4] bit 0, item_func_pirates_omamori /
 * heros_omamori) and the hero's clothes (event 0x2A80, outfit.h outfit_hero_unlocked). */
#pragma once
#include "wwhd_guest.h"
#include "wwhd/bindings.h"
#include "mp_protocol.h"

#define SV_SAVE_POINTER 0x101F84DC
#define SV_INFO 0x20                /* dSv_info_c from the save pointer */
#define SV_CURRENT_MEMORY 0x778     /* dSv_info_c::mMemory */
#define SV_MEMORY 0x380             /* dSv_save_c::mMemory[16] */
#define SV_MEMORY_BYTES 0x24
#define SV_STAGES 16
#define SV_PLAY_STAGE_DATA 0x5150
#define SV_STAG_PROP 0x09
#define SV_MAX_ELEMENTS 2048
#define SV_PACKET_LIMIT 3000        /* body bytes per message (payload buffer: 4096) */
#define SV_ICON_NONE 0x37           /* itemForSlot: no icon */

WWHD_GAME_FUNC(0x0262F880, u32, sv_icon_frame, (u32 self, u32 item));

static mp_save_element sv_elements[SV_MAX_ELEMENTS];
static u32 sv_count;
static u8 sv_mirror[MP_SAVE_BYTES];
static u8 sv_local[MP_SAVE_BYTES];
static int sv_state;                /* 0 off, 1 to merge, 2 running */
static u32 sv_merge_next;           /* the merge goes in several messages */
static char sv_from[MP_NAME_MAX + 1];
static u32 sv_sent, sv_received;

/* [first, end) byte ranges of dSv_save_c that stay each player's (see the header) */
static const u16 sv_local_ranges[][2] = {
    {0x002, 0x00E}, /* life, rupees, two unknown fields, the selected items */
    {0x014, 0x018}, /* magic and three unknown bytes */
    {0x018, 0x030}, /* dSv_player_status_b_c: date, clock, wind */
    {0x030, 0x03C}, /* the restart place */
    {0x04A, 0x04E}, /* items 14-17: the bottles' contents */
    {0x066, 0x06E}, /* dSv_player_item_record_c: pictures, arrows, bombs, bottles carried */
    {0x076, 0x0B4}, /* the bags, their flags and counts */
    {0x0C1, 0x0C4}, /* padding */
    {0x148, 0x380}, /* name, options, priest, the old statuses */
    {0x724, 0x778}, /* the reserve */
};

static u32 sv_info(void) {
    u32 save = WWHD_GAME_DATA(SV_SAVE_POINTER, u32);
    return save ? save + SV_INFO : 0;
}

/* The save slot of the stage being played, or -1. */
static s32 sv_stage_slot(const u8* play) {
    if (!play) return -1;
    u32 stag = wwhd_dStage_stageDt_c_getStagInfo((u32)(play + SV_PLAY_STAGE_DATA));
    if (!stag || (stag & 3)) return -1;
    s32 slot = (*(const volatile u8*)(stag + SV_STAG_PROP) >> 1) & 0x7F;
    return slot < SV_STAGES ? slot : -1;
}

/* The map of this session (SaveServer initializer), checked; 0 when the save sync is off. */
static int sv_open(const mp_session* s) {
    sv_state = 0;
    if (s->client_save_update == MP_NO_ID || s->server_save_update == MP_NO_ID) return 0;
    sv_count = mp_save_elements(s, sv_elements, SV_MAX_ELEMENTS);
    if (!sv_count || sv_elements[sv_count - 1].offset + sv_elements[sv_count - 1].size != MP_SAVE_BYTES) {
        wwhd_log("multiplayer: save map unusable (not dSv_save_c), progression not shared");
        sv_count = 0;
        return 0;
    }
    for (u32 i = 0; i != MP_SAVE_BYTES; ++i) sv_local[i] = 0;
    for (u32 r = 0; r != sizeof(sv_local_ranges) / sizeof(sv_local_ranges[0]); ++r)
        for (u32 i = sv_local_ranges[r][0]; i != sv_local_ranges[r][1]; ++i) sv_local[i] = 1;
    sv_state = 1;
    sv_merge_next = 0;
    sv_from[0] = 0;
    wwhd_log_int("multiplayer: save sync on, map elements", (int)sv_count);
    return 1;
}

static void sv_close(void) { sv_state = 0; }

/* ------------------------------------------------------------------ notices */

#define SV_SIDE_NOTICES 6
#define SV_NOTICES (2 * SV_SIDE_NOTICES) /* left: items and chests, right: upgrades */
#define SV_LEFT 0
#define SV_RIGHT 0 /* 0.3.18: every notice bottom left (the right side was the upgrades') */
#define SV_NOTICE_STEPS 150
typedef struct sv_notice {
    char text[48];
    u32 bytes;
    u32 item;          /* the item whose icon shows, 0xFF none */
    int steps;
    u8 player;         /* a player who joined: the map's Link icon in their colour instead */
    u16 color;         /* their primary tunic colour, RGB565 */
    u8 dead;           /* a player who died: the skull (assets/dead.png) instead */
} sv_notice;
static sv_notice sv_notices[SV_NOTICES];

/* mod.c: the sea chart's Link icon in a player's colours for notice slot k, and the tint to draw it
 * with (white, or the colour itself for the map arrow while the icon is not decoded yet). */
static u32 sv_player_icon(u32 k, u16 color, u32* tint);

/* The skull of the dead players (assets/dead.png, 50 x 44, the server's images/dead.png): their
 * notice, and above a ghost instead of its name (mod.c draw_markers). 0 when not loaded (logged). */
static u32 sv_dead_texture;
static unsigned long long sv_dead_epoch = ~0ull;
static u32 sv_dead_icon(void) {
    unsigned long long epoch = wwhd_hud_epoch();
    if (epoch != sv_dead_epoch) {
        sv_dead_epoch = epoch;
        sv_dead_texture = wwhd_hud_texture(WWHD_HUD_PACKAGE, "assets/dead.png");
        if (!sv_dead_texture) wwhd_log("multiplayer: assets/dead.png not loaded, the dead keep their names");
    }
    return sv_dead_texture;
}

/* English names of the items a notice can name (d_item_data.h numbers) */
static const char* sv_item_name(u32 item) {
    switch (item) {
    case 0x07: case 0x17: case 0x18: case 0x19: case 0x3F: return "a Piece of Heart";
    case 0x08: return "a Heart Container";
    case 0x20: return "the Telescope";
    case 0x21: return "the Tingle Tuner";
    case 0x22: return "the Wind Waker";
    case 0x23: return "the Picto Box";
    case 0x24: return "the Spoils Bag";
    case 0x25: return "the Grappling Hook";
    case 0x26: return "the Deluxe Picto Box";
    case 0x27: return "the Hero's Bow";
    case 0x28: return "the Power Bracelets";
    case 0x29: return "the Iron Boots";
    case 0x2A: return "the Magic Armor";
    case 0x2C: return "the Bait Bag";
    case 0x2D: return "the Boomerang";
    case 0x2F: return "the Hookshot";
    case 0x30: return "the Delivery Bag";
    case 0x31: return "Bombs";
    case 0x33: return "the Skull Hammer";
    case 0x34: return "the Deku Leaf";
    case 0x35: return "Fire and Ice Arrows";
    case 0x36: return "Light Arrows";
    case 0x38: return "the Hero's Sword";
    case 0x39: case 0x3A: case 0x3E: return "the Master Sword";
    case 0x3B: return "the Hero's Shield";
    case 0x3C: return "the Mirror Shield";
    case 0x42: return "the Pirate's Charm";
    case 0x43: return "the Hero's Charm";
    case 0x50: return "a Bottle";
    case 0x78: return "the Sail";
    case 0xAB: case 0xAC: return "a bigger Wallet";
    case 0xAD: case 0xAE: return "a bigger Bomb Bag";
    case 0xAF: case 0xB0: return "a bigger Quiver";
    case 0xB1: case 0xB2: return "more Magic";
    case 0x61: return "a Triforce Shard";
    case 0x69: return "a Pearl";
    case 0x6D: return "a song";
    default: return "a new item";
    }
}

static void sv_notice_add(const char* what, u32 item, int side) {
    /* the oldest notice of that side makes room */
    u32 first = (u32)side * SV_SIDE_NOTICES, slot = first;
    for (u32 i = first; i != first + SV_SIDE_NOTICES; ++i)
        if (sv_notices[i].steps < sv_notices[slot].steps) slot = i;
    sv_notice* n = &sv_notices[slot];
    u32 k = 0;
    const char* who = sv_from[0] ? sv_from : "A player";
    for (u32 i = 0; who[i] && k + 1 < sizeof(n->text); ++i) n->text[k++] = who[i];
    const char* mid = ": ";
    for (u32 i = 0; mid[i] && k + 1 < sizeof(n->text); ++i) n->text[k++] = mid[i];
    for (u32 i = 0; what[i] && k + 1 < sizeof(n->text); ++i) n->text[k++] = what[i];
    n->text[k] = 0;
    n->bytes = k;
    n->item = item;
    n->steps = SV_NOTICE_STEPS;
    n->player = 0;
    n->dead = 0;
}

/* Bottom left: "<name> joined the game", with the map's Link icon in the player's colour (mod.c,
 * a new id in the server's player directory). */
static void sv_notice_joined(const char* name, u16 color) {
    u32 slot = 0;
    for (u32 i = 0; i != SV_SIDE_NOTICES; ++i)
        if (sv_notices[i].steps < sv_notices[slot].steps) slot = i;
    sv_notice* n = &sv_notices[slot];
    u32 k = 0;
    const char* who = name && name[0] ? name : "A player";
    for (u32 i = 0; who[i] && k + 1 < sizeof(n->text); ++i)
        n->text[k++] = (who[i] < 0x20 || who[i] > 0x7E) ? '?' : who[i];
    const char* tail = " joined the game";
    for (u32 i = 0; tail[i] && k + 1 < sizeof(n->text); ++i) n->text[k++] = tail[i];
    n->text[k] = 0;
    n->bytes = k;
    n->item = 0xFF;
    n->steps = SV_NOTICE_STEPS;
    n->player = 1;
    n->color = color;
    n->dead = 0;
}

/* Bottom right: "<name> left the game", with the map's Link icon in the player's colour (mod.c, an
 * id of the last player directory missing from the new one). */
static void sv_notice_left(const char* name, u16 color) {
    u32 slot = SV_SIDE_NOTICES;
    for (u32 i = SV_SIDE_NOTICES; i != SV_NOTICES; ++i)
        if (sv_notices[i].steps < sv_notices[slot].steps) slot = i;
    sv_notice* n = &sv_notices[slot];
    u32 k = 0;
    const char* who = name && name[0] ? name : "A player";
    for (u32 i = 0; who[i] && k + 1 < sizeof(n->text); ++i)
        n->text[k++] = (who[i] < 0x20 || who[i] > 0x7E) ? '?' : who[i];
    const char* tail = " left the game";
    for (u32 i = 0; tail[i] && k + 1 < sizeof(n->text); ++i) n->text[k++] = tail[i];
    n->text[k] = 0;
    n->bytes = k;
    n->item = 0xFF;
    n->steps = SV_NOTICE_STEPS;
    n->player = 1;
    n->color = color;
    n->dead = 0;
}

/* Bottom left: "<name> died", with the skull (mod.c: a player's directory entry turned MP_DIR_DEAD). */
static void sv_notice_died(const char* name) {
    u32 slot = 0;
    for (u32 i = 0; i != SV_SIDE_NOTICES; ++i)
        if (sv_notices[i].steps < sv_notices[slot].steps) slot = i;
    sv_notice* n = &sv_notices[slot];
    u32 k = 0;
    const char* who = name && name[0] ? name : "A player";
    for (u32 i = 0; who[i] && k + 1 < sizeof(n->text); ++i)
        n->text[k++] = (who[i] < 0x20 || who[i] > 0x7E) ? '?' : who[i];
    const char* tail = " died";
    for (u32 i = 0; tail[i] && k + 1 < sizeof(n->text); ++i) n->text[k++] = tail[i];
    n->text[k] = 0;
    n->bytes = k;
    n->item = 0xFF;
    n->steps = SV_NOTICE_STEPS;
    n->player = 0;
    n->dead = 1;
}

static u32 sv_bits_new(u32 before, u32 after) { return after & ~before; }

/* What a received value means for the notices: before / after are its old and new bytes. */
static int sv_notice_for(u32 offset, u32 length, const u8* before, const u8* after) {
    if (offset >= 0x3C && offset < 0x51 && length == 1) { /* an item place */
        if (after[0] != before[0] && after[0] != 0xFF) {
            sv_notice_add(sv_item_name(after[0]), after[0], SV_LEFT);
            return 1;
        }
        return 0;
    }
    if (offset == 0x000 && length == 2) { /* maxLife, quarter hearts */
        u32 a = ((u32)before[0] << 8) | before[1], b = ((u32)after[0] << 8) | after[1];
        if (b > a) {
            sv_notice_add(b - a >= 4 ? "a Heart Container" : "a Piece of Heart", b - a >= 4 ? 0x08 : 0x07, SV_RIGHT);
            return 1;
        }
        return 0;
    }
    if (offset >= 0x0B4 && offset <= 0x0B8 && length == 1) { /* dSv_player_collect_c::mCollect[0..4] */
        u32 got = sv_bits_new(before[0], after[0]);
        if (!got) return 0;
        static const char* const swords[4] = {"the Hero's Sword", "the Master Sword",
                                              "the Master Sword, half power", "the Master Sword, full power"};
        static const u8 sword_items[4] = {0x38, 0x39, 0x3A, 0x3E};
        u32 bit = 0;
        while (bit != 8 && !(got & (1u << bit))) ++bit;
        switch (offset) {
        case 0x0B4:
            if (bit < 4) sv_notice_add(swords[bit], sword_items[bit], SV_RIGHT);
            else return 0;
            return 1;
        case 0x0B5:
            if (bit > 1) return 0;
            sv_notice_add(bit ? "the Mirror Shield" : "the Hero's Shield", bit ? 0x3C : 0x3B, SV_RIGHT);
            return 1;
        case 0x0B6: sv_notice_add("the Power Bracelets", 0x28, SV_RIGHT); return 1;
        case 0x0B7: sv_notice_add("the Pirate's Charm", 0x42, SV_RIGHT); return 1;
        default: sv_notice_add("the Hero's Charm", 0x43, SV_RIGHT); return 1;
        }
    }
    if (length != 1 || after[0] <= before[0]) {
        if (length != 1 || !sv_bits_new(before[0], after[0])) return 0;
    }
    switch (offset) {
    case 0x012: sv_notice_add("a bigger Wallet", 0xAB, SV_RIGHT); return 1;
    case 0x013: sv_notice_add("more Magic", 0xB1, SV_RIGHT); return 1;
    case 0x06F: sv_notice_add("a bigger Quiver", 0x27, SV_RIGHT); return 1;
    case 0x070: sv_notice_add("a bigger Bomb Bag", 0x31, SV_RIGHT); return 1;
    case 0x0BD: sv_notice_add("a new song", 0x22, SV_LEFT); return 1;      /* collect tact */
    case 0x0BE: sv_notice_add("a Triforce Shard", 0xFF, SV_LEFT); return 1;
    case 0x0BF: sv_notice_add("a Pearl", 0xFF, SV_LEFT); return 1;         /* collect symbol */
    default: break;
    }
    return 0;
}

/* a chest opened: a new bit of a stage memory's tbox (dSv_memBit_c +0, an int) */
static int sv_chest(u32 offset, u32 length, const u8* before, const u8* after) {
    if (offset < SV_MEMORY || offset >= SV_MEMORY + SV_STAGES * SV_MEMORY_BYTES || length != 4) return 0;
    if ((offset - SV_MEMORY) % SV_MEMORY_BYTES != 0) return 0;
    for (u32 i = 0; i != 4; ++i)
        if (sv_bits_new(before[i], after[i])) return 1;
    return 0;
}

/* ------------------------------------------------------------------ the server's updates */

/* SERVER_SAVE_FROM: who made the next update */
static void sv_from_received(const mp_u8* body, mp_u32 length) {
    if (!mp_read_save_from(body, length, sv_from)) sv_from[0] = 0;
}

/* SERVER_SAVE_UPDATE: written into the save (and the mirror: not sent back), never a local byte. */
static void sv_update_received(const mp_u8* body, mp_u32 length) {
    u32 info = sv_info();
    if (sv_state == 0 || !info) return;
    u8* save = (u8*)info;
    const u8* play = wwhd_play_get();
    s32 slot = sv_stage_slot(play);
    if (slot >= 0) wwhd_dSv_info_c_putSave(info, (u32)slot); /* the current memory first */
    mp_reader r = mp_reader_make(body, length);
    mp_u32 count = mp_read_save_count(&r);
    int touched = 0, notices = 0, chests = 0;
    int clothes_before = outfit_hero_unlocked();
    for (mp_u32 i = 0; r.ok && i != count; ++i) {
        mp_u32 offset, size;
        const mp_u8* bytes;
        if (!mp_read_save_entry(&r, &offset, &size, &bytes)) break;
        int local = 0;
        for (u32 k = 0; k != size; ++k) local |= sv_local[offset + k];
        if (local || !size) continue;
        notices += sv_notice_for(offset, size, save + offset, bytes);
        chests += sv_chest(offset, size, save + offset, bytes);
        for (u32 k = 0; k != size; ++k) {
            save[offset + k] = bytes[k];
            sv_mirror[offset + k] = bytes[k];
        }
        if (slot >= 0 && offset < SV_MEMORY + (u32)(slot + 1) * SV_MEMORY_BYTES &&
            offset + size > SV_MEMORY + (u32)slot * SV_MEMORY_BYTES)
            touched = 1;
        ++sv_received;
    }
    if (!r.ok) wwhd_log("multiplayer: malformed save update, the rest skipped");
    if (chests && !notices) sv_notice_add("opened a chest", 0xFF, SV_LEFT);
    if (!clothes_before && outfit_hero_unlocked()) sv_notice_add("the Hero's Clothes", 0xFF, SV_RIGHT);
    if (touched) wwhd_dSv_info_c_getSave(info, (u32)slot); /* this stage's switches and chests now */
    sv_from[0] = 0;
    static int logged;
    if (!logged++) wwhd_log_int("multiplayer: first shared progression received, values", (int)sv_received);
}

/* ------------------------------------------------------------------ this player's changes */

static int sv_send(mp_u32 id, const u8* save, int all) {
    static u8 body[SV_PACKET_LIMIT + 64];
    static u16 picked[SV_MAX_ELEMENTS];
    u32 n = 0, bytes = 0, i = all ? sv_merge_next : 0;
    for (; i != sv_count && bytes < SV_PACKET_LIMIT; ++i) {
        const mp_save_element* e = &sv_elements[i];
        if (sv_local[e->offset]) continue;
        if (!all) {
            int same = 1;
            for (u32 k = 0; k != e->size && same; ++k) same = save[e->offset + k] == sv_mirror[e->offset + k];
            if (same) continue;
        }
        picked[n++] = (u16)i;
        bytes += 3u + e->size;
    }
    if (all) sv_merge_next = i;
    if (!n) return 1;
    mp_writer w = mp_writer_make(body, sizeof(body));
    mp_write_vlq(&w, n);
    for (u32 k = 0; k != n; ++k) {
        const mp_save_element* e = &sv_elements[picked[k]];
        mp_write_save_element(&w, e, save + e->offset);
        for (u32 b = 0; b != e->size; ++b) sv_mirror[e->offset + b] = save[e->offset + b];
    }
    if (!w.ok) return 0;
    sv_sent += n;
    return send_payload(id, &w);
}

/* Every step of the local Link while connected. */
static void sv_step(const u8* play) {
    u32 info = sv_info();
    if (sv_state == 0 || !info || !play || play[WWHD_PLAY_EVENT_RUNNING_OFFSET]) return;
    s32 slot = sv_stage_slot(play);
    if (slot < 0) return;
    wwhd_dSv_info_c_putSave(info, (u32)slot);
    const u8* save = (const u8*)info;
    if (sv_state == 1) { /* the merge, a message per step */
        if (!sv_send(session.client_save_merge != MP_NO_ID ? session.client_save_merge : session.client_save_update, save, 1))
            return;
        if (sv_merge_next == sv_count) {
            sv_state = 2;
            wwhd_log_int("multiplayer: progression merged with the server, values", (int)sv_sent);
        }
        return;
    }
    sv_send(session.client_save_update, save, 0);
}

/* ------------------------------------------------------------------ drawing */

static u32 sv_icons[SV_ICON_NONE];
static unsigned long long sv_icons_epoch;

static u32 sv_icon(u32 item) {
    if (item == 0xFF) return 0;
    u32 at = sv_icon_frame(0, item);
    if (at >= SV_ICON_NONE) return 0;
    unsigned long long epoch = wwhd_hud_epoch();
    if (epoch != sv_icons_epoch) {
        sv_icons_epoch = epoch;
        for (u32 i = 0; i != SV_ICON_NONE; ++i) sv_icons[i] = 0; /* old handles are gone */
    }
    if (!sv_icons[at]) sv_icons[at] = wwhd_hud_game_icon(at);
    return sv_icons[at];
}

static wwhd_hud_element sv_element;

/* One side's notices, bottom left or right of the TV picture (1280 x 720), the newest at the bottom. */
static void sv_draw_side(u32 list, int side) {
    int order[SV_SIDE_NOTICES], shown = 0;
    for (int i = side * SV_SIDE_NOTICES; i != (side + 1) * SV_SIDE_NOTICES; ++i)
        if (sv_notices[i].steps > 0) order[shown++] = i;
    for (int a = 0; a < shown; ++a) /* oldest first, from the top */
        for (int b = a + 1; b < shown; ++b)
            if (sv_notices[order[b]].steps > sv_notices[order[a]].steps) {
                int t = order[a]; order[a] = order[b]; order[b] = t;
            }
    for (int k = 0; k != shown; ++k) {
        sv_notice* s = &sv_notices[order[k]];
        --s->steps;
        f32 alpha = s->steps < 30 ? (f32)s->steps / 30.0f : 1.0f;
        f32 grow = s->steps > SV_NOTICE_STEPS - 10 ? (f32)(SV_NOTICE_STEPS - s->steps) / 10.0f : 1.0f;
        f32 y = 720.0f - 40.0f - (f32)(shown - 1 - k) * 56.0f;
        f32 scale = 0.42f;
        f32 width = name_width(s->text, s->bytes, scale) + 76.0f;
        f32 x = side == SV_LEFT ? 24.0f - (1.0f - grow) * width /* slides in from the edge */
                                : 1280.0f - 24.0f - width + (1.0f - grow) * width;
        sv_element = (wwhd_hud_element){.kind = WWHD_HUD_RECT, .anchor = WWHD_HUD_TOP_LEFT, .x = x, .y = y - 24.0f,
                                       .w = width, .h = 48.0f, .thickness = 1, .u1 = 1, .v1 = 1,
                                       .rgba = 0x10202C00u | (u32)(alpha * 190.0f)};
        wwhd_hud_emit(list, &sv_element);
        u32 tint = 0xFFFFFFu;
        u32 icon = s->dead ? sv_dead_icon() : s->player ? sv_player_icon((u32)order[k], s->color, &tint) : sv_icon(s->item);
        if (icon) {
            sv_element = (wwhd_hud_element){.kind = WWHD_HUD_IMAGE, .anchor = WWHD_HUD_TOP_LEFT, .blend = WWHD_HUD_ALPHA,
                                           .x = x + 6.0f, .y = y - (s->dead ? 18.5f : 21.0f), .w = 42.0f, .h = s->dead ? 37.0f : 42.0f, .thickness = 1,
                                           .u1 = 1, .v1 = 1, .rgba = (tint << 8) | (u32)(alpha * 255.0f), .image = icon};
        } else { /* no icon: a small gold mark */
            sv_element = (wwhd_hud_element){.kind = WWHD_HUD_CIRCLE, .anchor = WWHD_HUD_CENTER, .x = x + 27.0f, .y = y,
                                           .size = 9.0f, .thickness = 1, .u1 = 1, .v1 = 1,
                                           .rgba = ((s->player ? (u32)mp_rgb565_to_888(s->color) : 0xF9C74Fu) << 8) |
                                                   (u32)(alpha * 255.0f)};
        }
        wwhd_hud_emit(list, &sv_element);
        f32 text_width = name_width(s->text, s->bytes, scale);
        name_draw(list, x + 58.0f + text_width * 0.5f, y + (f32)NAME_FONT_CAP * scale * 0.5f, s->text, s->bytes, 0xF9C74Fu,
                  scale, alpha);
    }
}

/* The notices, both sides; called every step. */
static void sv_draw(u32 list) {
    sv_draw_side(list, SV_LEFT);
    sv_draw_side(list, SV_RIGHT);
}
