/* 0.3.66: the switches the save does not keep, shared with the room.
 *
 * The game's switches (decomp d_save_5.cpp dSv_info_c::onSwitch 025B9E38, offSwitch 025B9F7C,
 * revSwitch 025BA20C, isSwitch 025BA0C0; this, number, room; d_save_readable.h MEMORY_SWITCH 128,
 * DAN_SWITCH 64, ZONE_SWITCH 48):
 *  - 0-127, the stage memory: saved, already shared by the progression (save_sync.h);
 *  - 128-191, the dungeon's (dSv_danBit_c): for the stay in the stage, not saved;
 *  - 192-239, the room's zone (dSv_zoneBit_c of the room's zone): for the stay in the room.
 * The last two never left a game: a door opened by a switch (two players on a heavy one,
 * heavy.h, or one on a floor switch) stayed closed for the others.
 *
 * Here: an entry hook on each of the three sends a real change of a switch 128-239 of the game's
 * own dSv_info_c (dComIfGs_info: the save pointer + 0x20, save_sync.h sv_info) to the room, through
 * the cutscene packet with a '#' name (item_share.h transport, the server relays it unchanged):
 * "#SW1_nn_rr" on, "#SW0_nn_rr" off (number and room in hex). A received one, same stage (a dungeon
 * switch) and same room (a zone switch), is set with the game's own onSwitch / offSwitch. A switch
 * just received is not sent back for SW_QUIET_STEPS (a switch a game sets again from its own state,
 * a floor switch nobody stands on here, would bounce between the games). */
#pragma once
#include "wwhd_guest.h"
#include "wwhd/bindings.h"

#define SW_FIRST 128             /* MEMORY_SWITCH: the dungeon and zone switches follow */
#define SW_ZONE_FIRST 192        /* MEMORY_SWITCH + DAN_SWITCH */
#define SW_END 240               /* + ZONE_SWITCH */
#define SW_QUIET_STEPS 30        /* 1 s */
#define SW_SENDS_PER_STEP 8

static int sw_receiving;
static unsigned long long sw_quiet_until[SW_END - SW_FIRST];
static unsigned long long sw_send_step;
static u32 sw_sent_this_step;
static u32 sw_logged;

static void sw_note(u32 info, s32 no, s32 room, int on) {
    if (sw_receiving || state != READY || no < SW_FIRST || no >= SW_END || info != sv_info()) return;
    if (room < 0 || room >= 64) return;
    unsigned long long now = wwhd_logic_step();
    if (now < sw_quiet_until[no - SW_FIRST]) return; /* the received value coming back */
    int was = wwhd_dSv_info_c_isSwitch(info, no, room) != 0;
    if (was == on) return; /* no change */
    if (now != sw_send_step) {
        sw_send_step = now;
        sw_sent_this_step = 0;
    }
    if (sw_sent_this_step++ >= SW_SENDS_PER_STEP) return;
    static const char hex[] = "0123456789ABCDEF";
    char name[11] = {'#', 'S', 'W', on ? '1' : '0', '_', hex[(no >> 4) & 0xF], hex[no & 0xF], '_',
                     hex[(room >> 4) & 0xF], hex[room & 0xF], 0};
    share_send(name);
    if (sw_logged++ < 8) wwhd_log_int(on ? "multiplayer: switch on, sent to the room" : "multiplayer: switch off, sent to the room", (int)no);
}

WWHD_HOOK(0x025B9E38, sw_on_hook, (u32 info, s32 no, s32 room)) { sw_note(info, no, room, 1); }
WWHD_HOOK(0x025B9F7C, sw_off_hook, (u32 info, s32 no, s32 room)) { sw_note(info, no, room, 0); }
WWHD_HOOK(0x025BA20C, sw_rev_hook, (u32 info, s32 no, s32 room)) {
    if (no < SW_FIRST || no >= SW_END || room < 0 || room >= 64 || info != sv_info()) return;
    sw_note(info, no, room, !wwhd_dSv_info_c_isSwitch(info, no, room));
}

/* item_share.h share_received: "#SW..." from another player. */
static void sw_received(const mp_event* e) {
    const char* n = e->name;
    if (n[3] != '0' && n[3] != '1') return;
    int a = share_hex(n[5]), b = share_hex(n[6]), c = share_hex(n[8]), d = share_hex(n[9]);
    if (n[4] != '_' || n[7] != '_' || a < 0 || b < 0 || c < 0 || d < 0) return;
    s32 no = a << 4 | b, room = c << 4 | d;
    if (no < SW_FIRST || no >= SW_END || room >= 64) return;
    const u8* play = wwhd_play_get();
    const fopAc_ac_c* link = dh_link ? (const fopAc_ac_c*)dh_link : 0;
    u32 info = sv_info();
    if (!play || !link || !info) return;
    char stage[STAGE_NAME_BYTES + 1];
    map_stage(play, stage);
    if (!map_same_stage(stage, e->stage)) return;
    if (no >= SW_ZONE_FIRST && link->current.roomNo != room) return; /* a room's own switch */
    sw_quiet_until[no - SW_FIRST] = wwhd_logic_step() + SW_QUIET_STEPS;
    sw_receiving = 1;
    if (n[3] == '1') wwhd_dSv_info_c_onSwitch(info, no, room);
    else wwhd_dSv_info_c_offSwitch(info, no, room);
    sw_receiving = 0;
    static u32 logged;
    if (logged++ < 8) wwhd_log_int("multiplayer: a switch of another player of the room, number", (int)no);
}
