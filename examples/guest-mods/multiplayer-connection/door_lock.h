/* Locked doors wait for the whole room (0.3.2): a door with a padlock (a small key or the boss key)
 * opens only when every living player of the local Link's room is within DL_RADIUS (15 m) of it.
 *
 * The game (decomp, verified): the dungeon doors with a lock are daDoor10_c (stone sliding doors,
 * d_a_door10.cpp), daDoor12_c (double doors, d_a_door12.cpp) and daKddoor_c (d_a_kddoor.cpp). Each
 * frame their setEventPrm (02127080, 0212919C, 0219807C; one argument, SDK bindings, the same in the
 * EU build) offers the door to Link (eventInfo.mCondition |= dEvtCnd_CANDOOR_e, the event and map
 * tool of the side) when he stands in its area and, for a locked door (the lock's mbEnabled: +0 of
 * mKeyLock at +0x424, +0x400, of mKey at +0x42C, the headers' WWHD_OFFSETs), has a small key
 * (dSv_memBit_c::mKeyNum) or the boss key. Replaced: on a locked door, while a living player of the
 * room is farther than DL_RADIUS, the original is not called (the door offers nothing, as without a
 * key) and the count ("1/3") shows on the door while the local Link is near it. The players of the
 * room: the local Link and the living remote players whose directory room is his (el_remote_room;
 * without the directory, every remote player of the area). Alone or offline, the game as it is. */
#pragma once
#include "wwhd_guest.h"

#define DL_RADIUS 1500.0f       /* 15 m */
#define DL_SHOW_RADIUS 500.0f   /* the count shows while the local Link is this near the door */
#define DL_SHOW_SECONDS 0.3
#define DL_COUNT_LIFT 220.0f
#define DL_LOCK_DOOR10 0x424
#define DL_LOCK_DOOR12 0x400
#define DL_LOCK_KDDOOR 0x42C

WWHD_GAME_ORIGINAL(0x02127080, void, dl_door10_original, (void* self));
WWHD_GAME_ORIGINAL(0x0212919C, void, dl_door12_original, (void* self));
WWHD_GAME_ORIGINAL(0x0219807C, void, dl_kddoor_original, (void* self));

/* 0.3.83: the count shows over every locked door one player of the room waits at (any player within
 * DL_SHOW_RADIUS, the local one or another), so that everyone sees it, not only the one in front. */
#define DL_SHOWN 4
typedef struct { f32 at[3]; u32 near, total; double show; } dl_shown_door;
static dl_shown_door dl_shown[DL_SHOWN];

static f32 dl_dist2(const f32* a, f32 x, f32 y, f32 z) {
    f32 dx = a[0] - x, dy = a[1] - y, dz = a[2] - z;
    return dx * dx + dy * dy + dz * dz;
}

/* 1 when a player of the room (the local one or another, alive) is within r of pos. */
static int dl_someone_near(const f32* pos, f32 r) {
    const fopAc_ac_c* link = (const fopAc_ac_c*)as_link;
    if (link && dl_dist2(pos, link->current.pos.x, link->current.pos.y, link->current.pos.z) <= r * r) return 1;
    for (int i = 0; i != MAX_REMOTES; ++i) {
        const remote_player* rp = &remotes[i];
        if (!rp->active || !rp->has_transform) continue;
        if (rp->has_look && (rp->look.death & (MP_DEATH_GHOST | MP_DEATH_DOWN))) continue;
        s8 room;
        if (link && el_remote_room(rp, &room) && room != link->current.roomNo) continue;
        const f32* p = rp->has_display ? rp->display.pos : rp->transform.pos;
        if (dl_dist2(pos, p[0], p[1], p[2]) <= r * r) return 1;
    }
    return 0;
}

/* A door's count, shown for DL_SHOW_SECONDS (its slot, or the oldest). */
static void dl_show_count(const f32* pos, u32 near, u32 total) {
    dl_shown_door* slot = &dl_shown[0];
    for (u32 i = 0; i != DL_SHOWN; ++i) {
        dl_shown_door* d = &dl_shown[i];
        if (d->show > 0 && dl_dist2(pos, d->at[0], d->at[1], d->at[2]) < 1.0f) {
            slot = d;
            break;
        }
        if (d->show < slot->show) slot = d;
    }
    for (u32 i = 0; i != 3; ++i) slot->at[i] = pos[i];
    slot->near = near;
    slot->total = total;
    slot->show = DL_SHOW_SECONDS;
}

/* 1 when the door may open: every living player of the local room near it (the count is kept). */
static int dl_everyone_near(const fopAc_ac_c* door) {
    const fopAc_ac_c* link = (const fopAc_ac_c*)as_link;
    if (state != READY || local_uid == MP_NO_ID || !link) return 1;
    f32 pos[3] = {door->current.pos.x, door->current.pos.y, door->current.pos.z};
    u32 total = 1, near = dl_dist2(pos, link->current.pos.x, link->current.pos.y, link->current.pos.z) <=
                              DL_RADIUS * DL_RADIUS;
    for (int i = 0; i != MAX_REMOTES; ++i) {
        const remote_player* r = &remotes[i];
        if (!r->active || !r->has_transform) continue;
        if (r->has_look && (r->look.death & (MP_DEATH_GHOST | MP_DEATH_DOWN))) continue;
        s8 room;
        if (el_remote_room(r, &room) && room != link->current.roomNo) continue;
        ++total;
        const f32* p = r->has_display ? r->display.pos : r->transform.pos;
        near += dl_dist2(pos, p[0], p[1], p[2]) <= DL_RADIUS * DL_RADIUS;
    }
    if (total <= 1) return 1;
    if (dl_someone_near(pos, DL_SHOW_RADIUS)) dl_show_count(pos, near, total);
    return near >= total;
}

static int ck_boss_blocked(const fopAc_ac_c* door); /* color_key.h: a player lacks colour keys */
#define DL_TYPE_DOOR10_BOSS 1u /* dDoor_info_c::getType: the boss key's door (d_a_door10.cpp setEventPrm) */
#define DL_TYPE_DOOR12_BOSS 3u /* (d_a_door12.cpp setEventPrm) */

static int dl_locked(const void* door, u32 lock) {
    return door && *(const volatile u8*)((const u8*)door + lock) != 0;
}

static void dl_wait_log(void) {
    static u32 logged;
    if (logged++ < 3) wwhd_log("multiplayer: a locked door waits for every player of the room");
}

static void dl_note(void* self, u32 lock); /* the doors seen, for "#DK" (0.3.82) */
WWHD_REPLACE(0x02127080, void, dl_door10, (void* self)) {
    dl_note(self, DL_LOCK_DOOR10);
    if (dl_locked(self, DL_LOCK_DOOR10) && wwhd_door_type(self) == DL_TYPE_DOOR10_BOSS &&
        ck_boss_blocked((const fopAc_ac_c*)self))
        return; /* the colour keys first (0.3.81) */
    if (dl_locked(self, DL_LOCK_DOOR10) && !dl_everyone_near((const fopAc_ac_c*)self)) {
        dl_wait_log();
        return;
    }
    dl_door10_original(self);
}

WWHD_REPLACE(0x0212919C, void, dl_door12, (void* self)) {
    dl_note(self, DL_LOCK_DOOR12);
    if (dl_locked(self, DL_LOCK_DOOR12) && wwhd_door_type(self) == DL_TYPE_DOOR12_BOSS &&
        ck_boss_blocked((const fopAc_ac_c*)self))
        return; /* the colour keys first (0.3.81) */
    if (dl_locked(self, DL_LOCK_DOOR12) && !dl_everyone_near((const fopAc_ac_c*)self)) {
        dl_wait_log();
        return;
    }
    dl_door12_original(self);
}

WWHD_REPLACE(0x0219807C, void, dl_kddoor, (void* self)) {
    dl_note(self, DL_LOCK_KDDOOR);
    if (dl_locked(self, DL_LOCK_KDDOOR) && !dl_everyone_near((const fopAc_ac_c*)self)) {
        dl_wait_log();
        return;
    }
    dl_kddoor_original(self);
}

/* Every step of the local Link (mod.c): the count fades when no door asks. */
static void dl_step(double dt) {
    for (u32 i = 0; i != DL_SHOWN; ++i)
        if (dl_shown[i].show > 0) dl_shown[i].show -= dt;
}

/* The count on the door (mod.c draw_markers), like the heavy things' (heavy.h). */
static void dl_draw(u32 list) {
    const u8* play = wwhd_play_get();
    if (!play || play[WWHD_PLAY_EVENT_RUNNING_OFFSET]) return;
    for (u32 i = 0; i != DL_SHOWN; ++i) {
        const dl_shown_door* d = &dl_shown[i];
        f32 x, y;
        if (d->show <= 0 || !dh_project(d->at, DL_COUNT_LIFT, &x, &y)) continue;
        static char text[DL_SHOWN][24];
        u32 n = hv_number(text[i], d->near);
        text[i][n++] = '/';
        n += hv_number(text[i] + n, d->total);
        name_draw(list, x, y, text[i], n, d->near >= d->total ? 0x6FE08Au : 0xF9C74Fu, 1.2f, 1.0f);
    }
}

/* ---- 0.3.82: a door opened with a key opens for everyone of the room ----
 * The game (decomp, verified): a locked door's lock (dDoor_key2_c, mbEnabled at +0) is made at the
 * room's load from its switch (daDoor10_c::setKey 02127554); the door's event, ordered by Link's A
 * (d_a_player_main_03.cpp: fopAcM_orderDoorEvent 025D7710 (Link, door), then changeWaitProc
 * 023E32AC), unlocks it with dDoor_key2_c::keyInit (0252B4A4, (lock, door)): the switch set, one key
 * less (mItemKeyNumCount), the lock's sound. The switch and the key count reach the others (save_sync.h),
 * but their door keeps its lock until the room loads again, and their key is gone: they were stuck.
 * Now the game where keyInit unlocks sends "#DK" with its Link's place; in the others of that stage
 * and room the nearest locked door (a door seen by the setEventPrm replacements above) loses its
 * lock without a key spent, the Link is placed where the opener stood, and the door's event is
 * ordered as Link's A does: the door opens for everyone and they go through. A player in an event,
 * dead or on the ship only has the lock removed. */
#define DL_DOORS 32
#define DL_FIND_RADIUS 600.0f
#define DL_ORDER_STEPS 3    /* steps after the place: the door's setEventPrm has offered itself */

typedef struct { u32 actor, pcid; u16 lock; } dl_door;
static dl_door dl_doors[DL_DOORS];
static int dl_forcing;
static u32 dl_order_door;   /* the door to go through, by process id */
static int dl_order_steps;

static void dl_note(void* self, u32 lock) {
    u32 pcid = *(const volatile u32*)((const u8*)self + AS_PC_ID);
    dl_door* free_one = 0;
    for (u32 i = 0; i != DL_DOORS; ++i) {
        dl_door* d = &dl_doors[i];
        if (d->actor == (u32)self && d->pcid == pcid) return;
        if (d->actor) {
            u32 out = 0;
            if (!wwhd_fopAcM_SearchByID(d->pcid, (u32)&out) || out != d->actor) d->actor = 0;
        }
        if (!d->actor && !free_one) free_one = d;
    }
    if (!free_one) return;
    free_one->actor = (u32)self;
    free_one->pcid = pcid;
    free_one->lock = (u16)lock;
}

/* The local Link's game unlocked a door: the others open theirs. */
WWHD_HOOK(0x0252B4A4, dl_key_init, (void* lock, void* door)) {
    (void)door;
    if (dl_forcing || !lock || !*(const volatile u8*)lock) return;
    const fopAc_ac_c* link = (const fopAc_ac_c*)dh_link;
    const u8* play = wwhd_play_get();
    if (!link || !play || state != READY || local_uid == MP_NO_ID || session.client_player_event == MP_NO_ID) return;
    mp_event e;
    e.sender = local_uid;
    map_stage(play, e.stage);
    e.room = link->current.roomNo;
    e.has_place = 1;
    e.pos[0] = link->current.pos.x;
    e.pos[1] = link->current.pos.y;
    e.pos[2] = link->current.pos.z;
    e.heading = link->shape_angle.y;
    e.name[0] = '#';
    e.name[1] = 'D';
    e.name[2] = 'K';
    e.name[3] = 0;
    mp_writer body = mp_writer_make(payload, sizeof(payload));
    mp_write_event(&body, &e);
    send_payload(session.client_player_event, &body);
    wwhd_log("multiplayer: a door unlocked with a key: opened for the room");
}

/* item_share.h share_received: "#DK" of another player. */
static void dl_key_received(const mp_event* e) {
    const u8* play = wwhd_play_get();
    fopAc_ac_c* link = (fopAc_ac_c*)dh_link;
    if (!play || !link || !e->has_place) return;
    char stage[STAGE_NAME_BYTES + 1];
    map_stage(play, stage);
    if (!map_same_stage(stage, e->stage) || link->current.roomNo != e->room) return;
    dl_door* best = 0;
    f32 best_d = DL_FIND_RADIUS * DL_FIND_RADIUS;
    for (u32 i = 0; i != DL_DOORS; ++i) {
        dl_door* d = &dl_doors[i];
        u32 out = 0;
        if (!d->actor || !wwhd_fopAcM_SearchByID(d->pcid, (u32)&out) || out != d->actor) continue;
        if (!*(const volatile u8*)(d->actor + d->lock)) continue; /* not locked here */
        const fopAc_ac_c* a = (const fopAc_ac_c*)d->actor;
        f32 dx = a->current.pos.x - e->pos[0], dy = a->current.pos.y - e->pos[1], dz = a->current.pos.z - e->pos[2];
        f32 d2 = dx * dx + dy * dy + dz * dz;
        if (d2 < best_d) {
            best_d = d2;
            best = d;
        }
    }
    if (!best) {
        wwhd_log("multiplayer: a door another player unlocked: no locked door here");
        return;
    }
    *(volatile u8*)(best->actor + best->lock) = 0; /* dDoor_key2_c::keyOff: no key spent here */
    if (play[WWHD_PLAY_EVENT_RUNNING_OFFSET] || dh_ghost || dh_dying ||
        (*(const volatile u32*)(play + PUPPET_PLAY_STATUS0) & PUPPET_STATUS0_RIDE)) {
        wwhd_log("multiplayer: a door another player unlocked: its lock removed here");
        return;
    }
    mp_transform place;
    for (u32 i = 0; i != 3; ++i) place.pos[i] = e->pos[i];
    place.rot[0] = place.rot[2] = 0;
    place.rot[1] = e->heading;
    dh_place(link, &place);
    dl_order_door = best->pcid;
    dl_order_steps = DL_ORDER_STEPS;
    wwhd_log("multiplayer: a door another player unlocked: going through it with them");
}

/* Every step of the local Link: the door's event ordered, as Link's A does. */
static void dl_door_step(void* link_actor) {
    if (!dl_order_door || --dl_order_steps > 0) return;
    u32 door = 0, pcid = dl_order_door;
    dl_order_door = 0;
    const u8* play = wwhd_play_get();
    if (!play || play[WWHD_PLAY_EVENT_RUNNING_OFFSET] || !link_actor || (u32)link_actor != dh_link) return;
    if (!wwhd_fopAcM_SearchByID(pcid, (u32)&door) || !door) return;
    dl_forcing = 1;
    wwhd_fopAcM_orderDoorEvent((u32)link_actor, door);
    wwhd_daPy_lk_c__changeWaitProc(link_actor);
    dl_forcing = 0;
}

/* ---- 0.3.82: a chest another player opened ----
 * Its open flag reaches this game (save_sync.h, the stage memory) but the chest, made closed, still
 * offers itself: opened again here, its small key came twice. daTbox_c::boxCheck (024B72C8, the
 * chest's offer to Link in actionOpenWait, d_a_tbox.cpp) now answers no when checkOpen (024B4AB4:
 * its flag in the save) says it is open. */
WWHD_GAME_ORIGINAL(0x024B72C8, s32, dl_box_check_original, (void* self));
WWHD_REPLACE(0x024B72C8, s32, dl_box_check, (void* self)) {
    if (self && state == READY && wwhd_checkOpen(self)) {
        static u32 logged;
        if (logged++ < 3) wwhd_log("multiplayer: a chest another player opened: not opened again");
        return 0;
    }
    return dl_box_check_original(self);
}
