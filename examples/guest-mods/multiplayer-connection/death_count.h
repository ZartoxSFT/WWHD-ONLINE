/* 0.3.92: every player's deaths, for the launcher's World Map.
 *
 * A death: the local Link's life out for good (death.h dh_local_dead: a ghost, or the game's own
 * death with no fairy in a bottle), counted once when it begins. Sent as events of the room
 * (item_share.h transport): "#DN_<name>" once a connection (the player is listed, 0 deaths at
 * first), "#DT_<name>" for each death. The server (WWClient.java) keeps the counts by name in
 * User/Deaths.txt and sends "#DC_<count>_<name>" to every player for each change, and all of them to
 * a player who joins: every player who ever came to the server. players.txt lists them
 * ("deaths=count|name"), the launcher shows them left of the map. */
#pragma once
#include "wwhd_guest.h"

#define DC_MAX 48

typedef struct dc_entry {
    char name[MP_NAME_MAX + 1];
    u32 count;
} dc_entry;

static dc_entry dc_entries[DC_MAX];
static u32 dc_count_used;
static int dc_was_dead, dc_registered, dc_was_ready;

static void dc_send(char kind) {
    const u8* play = wwhd_play_get();
    const fopAc_ac_c* link = (const fopAc_ac_c*)dh_link;
    if (!play || !link || state != READY || local_uid == MP_NO_ID || session.client_player_event == MP_NO_ID ||
        !local_look.name[0])
        return;
    mp_event e;
    e.sender = local_uid;
    map_stage(play, e.stage);
    if (!e.stage[0]) return;
    e.room = link->current.roomNo;
    e.has_place = 0;
    u32 n = 0;
    e.name[n++] = '#';
    e.name[n++] = 'D';
    e.name[n++] = kind;
    e.name[n++] = '_';
    for (u32 i = 0; i != MP_NAME_MAX && local_look.name[i] && n != MP_EVENT_NAME_MAX; ++i) e.name[n++] = local_look.name[i];
    e.name[n] = 0;
    mp_writer body = mp_writer_make(payload, sizeof(payload));
    mp_write_event(&body, &e);
    send_payload(session.client_player_event, &body);
}

/* item_share.h share_received: "#DC_<count>_<name>". */
static void dc_received(const mp_event* e) {
    const char* n = e->name;
    u32 i = 4, count = 0;
    if (n[3] != '_' || n[i] < '0' || n[i] > '9') return;
    while (n[i] >= '0' && n[i] <= '9' && count < 1000000u) count = count * 10u + (u32)(n[i++] - '0');
    if (n[i] != '_' || !n[i + 1]) return;
    const char* name = n + i + 1;
    dc_entry* slot = 0;
    for (u32 k = 0; k != dc_count_used && !slot; ++k)
        if (ck_same(dc_entries[k].name, name, MP_NAME_MAX + 1)) slot = &dc_entries[k];
    if (!slot) {
        if (dc_count_used == DC_MAX) return;
        slot = &dc_entries[dc_count_used++];
        ck_copy(slot->name, name, sizeof(slot->name));
    }
    slot->count = count;
}

/* mod.c, every step of the local Link. */
static void dc_step(void) {
    int ready = state == READY && local_uid != MP_NO_ID;
    if (state != READY) dc_registered = 0; /* the next connection lists this player again */
    if (ready && !dc_registered && local_look.name[0]) {
        dc_registered = 1;
        dc_send('N');
    }
    int dead = dh_local_dead();
    if (ready && dc_was_ready && dead && !dc_was_dead) {
        dc_send('T');
        wwhd_log("multiplayer: deaths: the local Link died, counted");
    }
    dc_was_dead = dead;
    dc_was_ready = ready;
}

/* mod.c map_write: "deaths=count|name" for each player the server told. */
static void dc_put(map_out* o) {
    for (u32 i = 0; i != dc_count_used; ++i) {
        map_put(o, "deaths=");
        map_put_uint(o, dc_entries[i].count);
        map_put(o, "|");
        map_put_name(o, dc_entries[i].name, MP_NAME_MAX);
        map_put(o, "\n");
    }
}
