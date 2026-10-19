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

static u32 dl_near, dl_total;
static double dl_show;
static f32 dl_at[3];

static f32 dl_dist2(const f32* a, f32 x, f32 y, f32 z) {
    f32 dx = a[0] - x, dy = a[1] - y, dz = a[2] - z;
    return dx * dx + dy * dy + dz * dz;
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
    if (dl_dist2(pos, link->current.pos.x, link->current.pos.y, link->current.pos.z) <= DL_SHOW_RADIUS * DL_SHOW_RADIUS) {
        dl_near = near;
        dl_total = total;
        dl_show = DL_SHOW_SECONDS;
        dl_at[0] = pos[0];
        dl_at[1] = pos[1];
        dl_at[2] = pos[2];
    }
    return near >= total;
}

static int dl_locked(const void* door, u32 lock) {
    return door && *(const volatile u8*)((const u8*)door + lock) != 0;
}

static void dl_wait_log(void) {
    static u32 logged;
    if (logged++ < 3) wwhd_log("multiplayer: a locked door waits for every player of the room");
}

WWHD_REPLACE(0x02127080, void, dl_door10, (void* self)) {
    if (dl_locked(self, DL_LOCK_DOOR10) && !dl_everyone_near((const fopAc_ac_c*)self)) {
        dl_wait_log();
        return;
    }
    dl_door10_original(self);
}

WWHD_REPLACE(0x0212919C, void, dl_door12, (void* self)) {
    if (dl_locked(self, DL_LOCK_DOOR12) && !dl_everyone_near((const fopAc_ac_c*)self)) {
        dl_wait_log();
        return;
    }
    dl_door12_original(self);
}

WWHD_REPLACE(0x0219807C, void, dl_kddoor, (void* self)) {
    if (dl_locked(self, DL_LOCK_KDDOOR) && !dl_everyone_near((const fopAc_ac_c*)self)) {
        dl_wait_log();
        return;
    }
    dl_kddoor_original(self);
}

/* Every step of the local Link (mod.c): the count fades when no door asks. */
static void dl_step(double dt) {
    if (dl_show > 0) dl_show -= dt;
}

/* The count on the door (mod.c draw_markers), like the heavy things' (heavy.h). */
static void dl_draw(u32 list) {
    const u8* play = wwhd_play_get();
    if (dl_show <= 0 || !play || play[WWHD_PLAY_EVENT_RUNNING_OFFSET]) return;
    f32 x, y;
    if (!dh_project(dl_at, DL_COUNT_LIFT, &x, &y)) return;
    char text[24];
    u32 n = hv_number(text, dl_near);
    text[n++] = '/';
    n += hv_number(text + n, dl_total);
    name_draw(list, x, y, text, n, dl_near >= dl_total ? 0x6FE08Au : 0xF9C74Fu, 1.2f, 1.0f);
}
