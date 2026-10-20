/* Cutscenes for the whole room: when a player starts a stage cutscene, the other players of the
 * same stage and room are brought to that player, where their own game starts it.
 *
 * The game (decomp, verified): a stage's events (its event list, dEvDtEvent_c: mName[32] at +0)
 * start in dEvent_manager_c::startProc (02543C6C, d_event_manager.cpp: this, the event).
 *
 * 0.3.23-0.3.29 ordered the received event in the other games with the local Link
 * (fopAcM_orderOtherEvent2). Seen with AMORI_DEMO_2 (Forbidden Woods' Bokoblin): a stage event is
 * acted out by its actors, and the receiver's Bokoblin, which had asked for nothing, did not act:
 * the other player was stuck (0.3.28), then the event ended with the starter's but with a wrong
 * camera, and the receiver's Bokoblin started the event once more, properly, the player being near
 * it after the teleport. So since 0.3.30 only the cause is shared, the players' places: nothing is
 * ordered; the receiver's own actors start the cutscene as they do for their player (when the
 * cutscene is not started by the place, a switch or an item, it plays only for its starter).
 *
 * Here: an entry hook on startProc sends the event's name with the local stage and room
 * (CLIENT_PLAYER_EVENT, mp_protocol.h "cutscenes"), but not the game's generic events (names
 * "DEFAULT_...": talks, doors, chests, items, which stay each player's own). A received one, same
 * stage and room, the local Link alive, no event running here, not riding the ship: the local Link
 * is put at the place of the player who started it (its last received ActorPlayerSync transform),
 * as the revive does (death.h dh_place: setPlayerPosAndAngle's kept copies, procWait), at its next
 * step. A cutscene started or received in the last EV_RECENT_STEPS is neither sent nor followed
 * again (the receiver's own start of it is not sent back to the room).
 *
 * 0.3.31: placed, the Link did not start the cutscene (a trigger the starter walked into, which the
 * placed Link stands in without moving). So after the place it walks forward, in the starter's
 * heading, for EV_WALK_STEPS: after daPy_lk_c::setStickData (023F32E4, death.h dh_after_stick, the
 * player's own inputs, d_a_player_main_03.cpp) its stick is set, mStickDistance (+0x6A08) and the
 * world move angle m34E8 (+0x6930, camera angle + stick angle there), as the game's demo walk does
 * (DEMO_N_WALK: mStickDistance 1, m34E8 the demo's move angle). Stopped once an event runs. */
#pragma once
#include "wwhd_guest.h"
#include "wwhd/bindings.h"

#define EV_RECENT_STEPS 600            /* 20 s: the receiver's own start, after its walk */
#define EV_WAIT_STEPS 150              /* 5 s for an event running here to end */
#define EV_RECENT 8
#define EV_WALK_STEPS 30               /* 1 s of walking after the place */
#define EV_WALK_STICK 0.6f             /* a walk, not a run */

static int ev_pending;                 /* a received cutscene: go to its starter at the next step */
static mp_u32 ev_pending_from;
static int ev_pending_wait;
static char ev_pending_stage[STAGE_NAME_BYTES + 1]; /* where it was received */
static signed char ev_pending_room;
static int ev_pending_has_place;       /* 0.3.32: the starter's place sent with the cutscene */
static mp_transform ev_pending_place;
static int ev_walk;                    /* steps of walking still to do */
static s16 ev_walk_angle;
static char ev_recent[EV_RECENT][MP_EVENT_NAME_MAX + 1];
static unsigned long long ev_recent_step[EV_RECENT];

static int ev_same(const char* a, const char* b) {
    for (int i = 0; i != MP_EVENT_NAME_MAX + 1; ++i) {
        if (a[i] != b[i]) return 0;
        if (!a[i]) return 1;
    }
    return 1;
}

/* 0.3.51: the player's own actions are not cutscenes of the room. Log 0.3.50 (instance 3): the
 * other player's door ("KNOB_START", "KNOB_START_B", ten times) was followed, and one received just
 * before an area change placed the Link in the new area at the old area's coordinates (teleported
 * anywhere). Left out, besides the game's generic "DEFAULT_..." events, the names holding a door,
 * chest, talk, item, warp or fall word (decomp event names: KNOB00, SHUTTER_DOOR, TREASURE, TALK,
 * SPEAK, GET_ITEM, DEFAULT_WARP, DEFAULT_PITFALL; seen: AJ_SPEAK, FALL_START). */
static int ev_contains(const char* name, const char* word) {
    for (int i = 0; name[i]; ++i) {
        int k = 0;
        while (word[k] && name[i + k] == word[k]) ++k;
        if (!word[k]) return 1;
    }
    return 0;
}

static int ev_generic(const char* name) {
    static const char* const own[] = {"DEFAULT_", "KNOB", "SHUTTER", "DOOR", "TREASURE", "TBOX", "TALK",
                                       "SPEAK", "GET", "OPEN", "WARP", "PITFALL", "FALL"};
    for (u32 k = 0; k != sizeof(own) / sizeof(own[0]); ++k)
        if (ev_contains(name, own[k])) return 1;
    return 0;
}

static int ev_is_recent(const char* name) {
    for (int i = 0; i != EV_RECENT; ++i)
        if (ev_recent[i][0] && ev_same(name, ev_recent[i]) && wwhd_logic_step() - ev_recent_step[i] < EV_RECENT_STEPS)
            return 1;
    return 0;
}

static void ev_note_recent(const char* name) {
    int k = 0;
    for (int i = 1; i != EV_RECENT; ++i)
        if (ev_recent_step[i] < ev_recent_step[k]) k = i;
    for (int i = 0; i != MP_EVENT_NAME_MAX + 1; ++i) ev_recent[k][i] = name[i];
    ev_recent_step[k] = wwhd_logic_step();
}

WWHD_HOOK(0x02543C6C, ev_start, (u32 self, u32 event)) {
    (void)self;
    if (!event) return;
    char name[MP_EVENT_NAME_MAX + 1];
    int n = 0;
    for (; n != MP_EVENT_NAME_MAX; ++n) {
        char c = *(const volatile char*)(event + (u32)n);
        if (!c) break;
        if (c < 0x20 || c > 0x7E) return; /* not a name: nothing sent */
        name[n] = c;
    }
    name[n] = 0;
    if (!n || ev_generic(name) || ev_is_recent(name)) return;
    ev_note_recent(name);
    const u8* play = wwhd_play_get();
    const fopAc_ac_c* link = dh_link ? (const fopAc_ac_c*)dh_link : 0;
    if (!play || !link || state != READY || local_uid == MP_NO_ID || session.client_player_event == MP_NO_ID) return;
    mp_event e;
    e.sender = local_uid;
    map_stage(play, e.stage);
    e.room = link->current.roomNo;
    for (int i = 0; i != n + 1; ++i) e.name[i] = name[i];
    e.has_place = 1; /* where it starts: the trigger the others walk into */
    e.pos[0] = link->current.pos.x;
    e.pos[1] = link->current.pos.y;
    e.pos[2] = link->current.pos.z;
    e.heading = link->shape_angle.y;
    mp_writer body = mp_writer_make(payload, sizeof(payload));
    mp_write_event(&body, &e);
    if (send_payload(session.client_player_event, &body)) {
        wwhd_log("multiplayer: cutscene sent to the room:");
        wwhd_log(name);
    }
}

/* SERVER_PLAYER_EVENT. */
static void ev_received(const mp_u8* body, mp_u32 length) {
    mp_event e;
    if (!mp_read_event(body, length, &e)) {
        wwhd_log("multiplayer: malformed cutscene skipped");
        return;
    }
    const u8* play = wwhd_play_get();
    const fopAc_ac_c* link = dh_link ? (const fopAc_ac_c*)dh_link : 0;
    if (!play || !link || e.sender == local_uid || ev_generic(e.name)) return;
    char stage[STAGE_NAME_BYTES + 1];
    map_stage(play, stage);
    if (!map_same_stage(stage, e.stage) || link->current.roomNo != e.room) return; /* not this room */
    if (dh_ghost || dh_dying) {
        wwhd_log("multiplayer: cutscene of the room not followed: dead here");
        return;
    }
    if (ev_is_recent(e.name)) {
        wwhd_log("multiplayer: cutscene of the room not followed: played here a moment ago");
        return;
    }
    ev_note_recent(e.name); /* its start here, by the local actors, is not sent back */
    ev_pending = 1;
    ev_pending_from = e.sender;
    for (int i = 0; i != STAGE_NAME_BYTES + 1; ++i) ev_pending_stage[i] = stage[i];
    ev_pending_room = e.room;
    ev_pending_wait = EV_WAIT_STEPS;
    ev_pending_has_place = e.has_place;
    if (e.has_place) {
        for (int i = 0; i != 3; ++i) ev_pending_place.pos[i] = e.pos[i];
        ev_pending_place.rot[0] = ev_pending_place.rot[2] = 0;
        ev_pending_place.rot[1] = e.heading;
    }
    wwhd_log("multiplayer: a cutscene of another player of the room:");
    wwhd_log(e.name);
}

/* Each step of the local Link (after its execute): to the starter of a received cutscene. */
static void ev_step(void* link_actor) {
    if (!ev_pending || !link_actor || (u32)link_actor != dh_link) return;
    const u8* play = wwhd_play_get();
    if (!play) return;
    if (play[WWHD_PLAY_EVENT_RUNNING_OFFSET]) { /* an event runs here: wait for its end */
        if (--ev_pending_wait < 0) {
            ev_pending = 0;
            wwhd_log("multiplayer: cutscene of the room not followed: an event ran here");
        }
        return;
    }
    ev_pending = 0;
    { /* 0.3.51: still where it was received (an area change between: dropped) */
        char here[STAGE_NAME_BYTES + 1];
        map_stage(play, here);
        if (!map_same_stage(here, ev_pending_stage) || ((const fopAc_ac_c*)link_actor)->current.roomNo != ev_pending_room) {
            wwhd_log("multiplayer: cutscene of the room not followed: the area changed meanwhile");
            return;
        }
    }
    if (*(const volatile u32*)(play + PUPPET_PLAY_STATUS0) & PUPPET_STATUS0_RIDE) {
        wwhd_log("multiplayer: cutscene of the room not followed: on the ship");
        return;
    }
    const mp_transform* place = ev_pending_has_place ? &ev_pending_place : 0;
    if (!place) { /* an older client: its last place received */
        const remote_player* r = find_remote(ev_pending_from, 0);
        if (r && r->has_transform) place = &r->transform;
    }
    if (!place) {
        wwhd_log("multiplayer: cutscene of the room not followed: the player's place is not known");
        return;
    }
    dh_place(link_actor, place);
    ev_walk = EV_WALK_STEPS;
    ev_walk_angle = place->rot[1];
    wwhd_log_int("multiplayer: cutscene: placed where the player started it, uid", (int)ev_pending_from);
}

/* After setStickData of the local Link: the walk after the place. */
static void ev_after_stick(void* self) {
    if (ev_walk <= 0) return;
    const u8* play = wwhd_play_get();
    if (!play || play[WWHD_PLAY_EVENT_RUNNING_OFFSET] || dh_ghost || dh_dying) { /* it started: done */
        ev_walk = 0;
        return;
    }
    --ev_walk;
    daPy_lk_c* link = (daPy_lk_c*)self;
    link->mStickDistance = EV_WALK_STICK;
    link->m34E8 = ev_walk_angle;
}
