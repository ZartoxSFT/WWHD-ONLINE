/* A player who joins the server comes back where it was when it left (0.3.66, kept by the server
 * since 0.3.70).
 *
 * The place (stage, room, layer, position, heading) is sent every LP_SAVE_STEPS to the server as a
 * cutscene packet named "#LP_ll" (ll the layer, hex; the packet carries stage, room and the place:
 * item_share.h transport, mp_protocol.h "cutscenes"). The server keeps it per player (its uid,
 * WWClient LAST_PLACES, User/LastPlaces.txt), relays it to nobody, and sends it back at that
 * player's next join. Only the place, never the save. Not sent while riding the ship, dead, in an
 * event or during a teleport (a place one could not stand at).
 *
 * The game also keeps the last place sent or received in "last_place.bin" (the mod's Data folder,
 * with the server it belongs to), for the first load of a game: the save's stage is replaced before
 * it is loaded, no second load. dScnName_c::changeGameScene (025ADC60, d_s_name.cpp) calls
 * dComIfGs_gameStart (02522198, d_com_inf_game.cpp: dComIfGp_setNextStage with the save's return
 * place), asks for the play scene, then clears the save's restart room parameter: after it (a
 * return hook, an entry hook on gameStart telling it ran), the next stage, the restart place and its
 * parameter are set as the launcher map's teleport sets them (mod.c teleport_go: restart place
 * dSv_restart_c, setNextStage 0252012C point -1, restart parameter room | 0xC9 << 24).
 *
 * Connected, the server's place counts: LP_WAIT_STEPS after the connection, if the local Link is
 * not already in that room within LP_NEAR, it is brought there with teleport_go (no price). */
#pragma once
#include "wwhd_guest.h"

#define LP_FILE "last_place.bin"
#define LP_MAGIC 0x4C504C32u      /* "LPL2" */
#define LP_SAVE_STEPS 150         /* 5 s */
#define LP_WAIT_STEPS 60          /* 2 s for the server's place after the connection */
#define LP_GIVE_UP_STEPS 900      /* 30 s */
#define LP_NEAR 1000.0f

typedef struct lp_record {
    u32 magic;
    char server[72];
    char stage[STAGE_NAME_BYTES + 1];
    s8 room, layer;
    s16 heading;
    f32 pos[3];
} lp_record;

static int lp_done;                  /* the return was tried for this connection */
static unsigned long long lp_ready_step;
static int lp_was_ready;
static u32 lp_save_clock;
static lp_record lp_server_place;    /* the server's answer at this connection */
static int lp_server_has;
static int lp_game_starting;         /* dComIfGs_gameStart ran in this changeGameScene */

static void lp_server_key(char* out, u32 size) {
    u32 n = 0;
    for (u32 i = 0; cfg.has_host && cfg.host[i] && n + 1 < size; ++i) out[n++] = cfg.host[i];
    unsigned int port = cfg.has_port ? cfg.port : 10014u;
    char digits[6];
    u32 d = 0;
    do {
        digits[d++] = (char)('0' + port % 10u);
        port /= 10u;
    } while (port && d != sizeof(digits));
    if (n + 1 < size) out[n++] = ':';
    while (d && n + 1 < size) out[n++] = digits[--d];
    while (n < size) out[n++] = 0;
}

static int lp_same_text(const char* a, const char* b, u32 size) {
    for (u32 i = 0; i != size; ++i) {
        if (a[i] != b[i]) return 0;
        if (!a[i]) return 1;
    }
    return 1;
}

static int lp_can_stand(const u8* play) {
    return !play[WWHD_PLAY_EVENT_RUNNING_OFFSET] && !teleport_stage_wait && !dh_local_dead() &&
           !(*(const volatile u32*)(play + PUPPET_PLAY_STATUS0) & PUPPET_STATUS0_RIDE);
}

static void lp_keep(const lp_record* r) {
    lp_record k = *r;
    k.magic = LP_MAGIC;
    lp_server_key(k.server, sizeof(k.server));
    wwhd_file_write(LP_FILE, &k, sizeof(k));
}

static int lp_read_kept(lp_record* r) {
    if (wwhd_file_read(LP_FILE, r, sizeof(*r)) != (s32)sizeof(*r) || r->magic != LP_MAGIC) return 0;
    char key[sizeof(r->server)];
    lp_server_key(key, sizeof(key));
    if (!lp_same_text(key, r->server, sizeof(key))) return 0; /* another server's place */
    r->stage[STAGE_NAME_BYTES] = 0;
    return r->stage[0] && r->room >= 0;
}

/* Sent to the server (and kept here). */
static void lp_send(const fopAc_ac_c* link, const u8* play) {
    lp_record r;
    for (u32 i = 0; i != sizeof(r); ++i) ((u8*)&r)[i] = 0;
    map_stage(play, r.stage);
    r.room = link->current.roomNo;
    r.layer = *(const volatile s8*)(play + PLAY_START_LAYER);
    r.heading = link->shape_angle.y;
    r.pos[0] = link->current.pos.x;
    r.pos[1] = link->current.pos.y;
    r.pos[2] = link->current.pos.z;
    if (!r.stage[0] || r.room < 0) return;
    lp_keep(&r);
    if (state != READY || local_uid == MP_NO_ID || session.client_player_event == MP_NO_ID) return;
    static const char hex[] = "0123456789ABCDEF";
    mp_event e;
    e.sender = local_uid;
    for (int i = 0; i != STAGE_NAME_BYTES + 1; ++i) e.stage[i] = r.stage[i];
    e.room = r.room;
    char name[7] = {'#', 'L', 'P', '_', hex[((u8)r.layer >> 4) & 0xF], hex[(u8)r.layer & 0xF], 0};
    for (int i = 0; i != 7; ++i) e.name[i] = name[i];
    e.has_place = 1;
    for (int i = 0; i != 3; ++i) e.pos[i] = r.pos[i];
    e.heading = r.heading;
    mp_writer body = mp_writer_make(payload, sizeof(payload));
    mp_write_event(&body, &e);
    send_payload(session.client_player_event, &body);
}

/* item_share.h share_received: "#LP_ll" from the server, this player's last place. */
static void lp_received(const mp_event* e) {
    const char* n = e->name;
    int a = share_hex(n[4]), b = share_hex(n[5]);
    if (n[3] != '_' || a < 0 || b < 0 || !e->has_place || !e->stage[0]) return;
    lp_record r;
    for (u32 i = 0; i != sizeof(r); ++i) ((u8*)&r)[i] = 0;
    for (int i = 0; i != STAGE_NAME_BYTES; ++i) r.stage[i] = e->stage[i];
    r.room = (s8)e->room;
    r.layer = (s8)(a << 4 | b);
    r.heading = e->heading;
    for (int i = 0; i != 3; ++i) r.pos[i] = e->pos[i];
    lp_server_place = r;
    lp_server_has = 1;
    lp_keep(&r); /* the next game's first load starts there */
    wwhd_log("multiplayer: last place received from the server:");
    wwhd_log(r.stage);
}

/* 1 when the return is done or not needed, 0 to try again later. */
static int lp_return(const fopAc_ac_c* link, const u8* play, const lp_record* r) {
    char here[STAGE_NAME_BYTES + 1];
    map_stage(play, here);
    int same_stage = map_same_stage(here, r->stage);
    f32 dx = link->current.pos.x - r->pos[0], dy = link->current.pos.y - r->pos[1], dz = link->current.pos.z - r->pos[2];
    if (same_stage && link->current.roomNo == r->room && dx * dx + dy * dy + dz * dz < LP_NEAR * LP_NEAR) return 1;
    if (!lp_can_stand(play)) return 0;
    if (!teleport_go(r->stage, r->pos, r->heading, r->room, r->layer, same_stage)) return 0;
    wwhd_log("multiplayer: back where this player left the server:");
    wwhd_log(r->stage);
    return 1;
}

/* Every step of the local Link (mod.c). */
static void lp_step(const fopAc_ac_c* link, const u8* play) {
    if (!link || !play) return;
    if (state != READY) {
        lp_was_ready = 0;
        lp_done = 0;
        lp_server_has = 0;
        return;
    }
    unsigned long long now = wwhd_logic_step();
    if (!lp_was_ready) {
        lp_was_ready = 1;
        lp_ready_step = now;
    }
    if (!lp_done) {
        if (!lp_server_has && now - lp_ready_step < LP_WAIT_STEPS) return;
        if (!lp_server_has || lp_return(link, play, &lp_server_place) || now - lp_ready_step > LP_GIVE_UP_STEPS) {
            lp_done = 1;
            lp_save_clock = 0;
        }
        return;
    }
    if (++lp_save_clock < LP_SAVE_STEPS) return;
    if (!lp_can_stand(play)) return; /* tried again next step */
    lp_save_clock = 0;
    lp_send(link, play);
}

/* ---- the game's first load: straight to the kept place ---- */
WWHD_HOOK(0x02522198, lp_game_start, (void)) { lp_game_starting = 1; }

WWHD_HOOK_RETURN(0x025ADC60, lp_change_game_scene, (u32 unused)) {
    (void)unused;
    if (!lp_game_starting) return;
    lp_game_starting = 0;
    read_launcher_cfg();
    if (!cfg.has_host) return; /* not an online player */
    lp_record r;
    u32 save = WWHD_GAME_DATA(SAVE_POINTER, u32);
    if (!save || !lp_read_kept(&r)) return;
    for (int i = 0; i != STAGE_NAME_BYTES + 1; ++i) teleport_stage[i] = r.stage[i];
    teleport_pos.x = r.pos[0];
    teleport_pos.y = r.pos[1];
    teleport_pos.z = r.pos[2];
    teleport_set_restart(save + SAVE_RESTART, (u32)&teleport_pos, (u32)(s32)r.heading, (u32)(s32)r.room);
    if (!wwhd_dComIfGp_setNextStage((u32)teleport_stage, 0xFFFFFFFFu, (u32)(s32)r.room, (u32)(s32)r.layer, 0.0f,
                                    TELEPORT_MODE, 0, 0))
        return;
    *(volatile u32*)(save + SAVE_RESTART_PARAM) = ((u32)(s32)r.room & 0x3Fu) | (TELEPORT_EVENT_INFO << 24);
    wwhd_log("multiplayer: the game starts where this player left the server:");
    wwhd_log(r.stage);
}
