/* Heavy things take several players: the objects that need the Power Bracelets (lifting a heavy
 * actor, pushing or pulling a heavy block) move only when enough players of the room do it together.
 *
 * Needed: every player of the room when they are two, two thirds of them (rounded up) from three on;
 * alone, the game as it is. The players of the room: the local one and the server directory's
 * players in the same stage and room, not dead (MP_DIR_DEAD); without the directory, the remote
 * players of the area. A helper: a living remote Link within HV_HELP_RADIUS of the local Link whose
 * current proc (ActorPlayerSync proc, mod.c current_look) is the same action. The count ("1/3")
 * shows above the heavy thing while the local Link does it. A Link that strains (below) keeps
 * straining HV_STRAIN_SECONDS, ANM_GRABNG (0x67) started again whenever it ends, so the others
 * have time to come: procGrabMiss lets go once mProcVar0 (+0x6916, 12 steps) ran out after the
 * animation (d_a_player_main_07.cpp); it is kept at 12 meanwhile.
 *
 * The game (decomp, verified):
 *  - lifting: daPy_lk_c::procGrabReady_init (023ED0F0, d_a_player_main_02.cpp) carries an actor
 *    with the heavy status bit (fopAcStts 0x10000) only with the Power Bracelets equipped
 *    (dComIfGs_getSelectEquip(2) == 0x28: save + 0x20 + 0x0E + 2, d_save.h); without them Link
 *    holds it and strains (mActorKeepRope, procGrabMiss: GRAB_MISS 0x70, ANM_GRABNG, d_a_player_main_06/07.cpp).
 *    Replaced: while too few players help, the bracelets are taken off for that call, so Link
 *    strains (and counts as a helper for the others: proc GRAB_READY 0x6E or GRAB_MISS 0x70). With
 *    enough, one player lifts it, the helper with the lowest actor uid (two players carrying their
 *    own copy would fight over it: ActorSync gives a carried actor to its carrier); the others
 *    strain. A straining Link whose turn it is lifts at once (hv_step).
 *  - pushing and pulling: setPushPullKeepData (0243CEE0, d_a_player_main_07.cpp) adds PPLABEL_HEAVY
 *    (8) with the bracelets; a block's push callback daObjMovebox::Act_c::PPCallBack (023775DC,
 *    d_a_obj_movebox.cpp) takes a push only with that label when its type's attribute m9A is set
 *    (M_attr 101CB424, 13 entries of 0x9C, m9A at +0x9A; the type at the actor + 0x710). Replaced:
 *    on any block or crate (0.2.79: all of them, the user's choice; 0.2.76-0.2.78 the heavy ones,
 *    by taking PPLABEL_HEAVY off, which a light block ignores) the label loses its push direction
 *    (bits 0-1: the callback then counts no push) and PPLABEL_HEAVY while too few players push or pull
 *    (PUSH_PULL_WAIT 0x32, PUSH_MOVE 0x33, PULL_MOVE 0x34). */
#pragma once
#include "wwhd_guest.h"

#define HV_HELP_RADIUS 400.0f      /* 4 m from the local Link */
#define HV_STATUS_HEAVY 0x10000u
#define HV_SAVE_BRACELET (0x20 + 0x0E + 2)
#define HV_BRACELET 0x28
#define HV_NO_EQUIP 0xFF
#define HV_MOVEBOX_ATTR 0x101CB424u
#define HV_MOVEBOX_ATTR_SIZE 0x9C
#define HV_MOVEBOX_HEAVY 0x9A
#define HV_MOVEBOX_TYPE 0x710
#define HV_MOVEBOX_TYPES 13
#define HV_PROC_PUSH_PULL_WAIT 0x32
#define HV_PROC_PUSH_MOVE 0x33
#define HV_PROC_PULL_MOVE 0x34
#define HV_PROC_GRAB_READY 0x6E
#define HV_PROC_GRAB_MISS 0x70
#define HV_SHOW_SECONDS 0.3        /* the count stays this long after the last check */
#define HV_STRAIN_SECONDS 6.0      /* a Link straining at a heavy actor waits this long for help */
#define HV_ANM_GRABNG 0x67
#define HV_MISS_HOLD 12            /* procGrabMiss's own count (mProcVar0) */
#define HV_COUNT_LIFT 150.0f       /* the count's height above the thing */

enum { HV_NONE, HV_PUSH, HV_LIFT };

WWHD_GAME_ORIGINAL(0x023ED0F0, s32, hv_grab_ready_original, (void* self));
WWHD_GAME_ORIGINAL(0x023775DC, u32, hv_movebox_pp_original, (u32 target, u32 unused, s16 angle, u32 label));

static u32 hv_link;                /* the local Link (daPy_Execute) */
static int hv_kind;                /* what the count shows */
static u32 hv_count, hv_needed;
static double hv_show;
static u32 hv_lift_target;         /* the heavy actor the local Link strains at */
static double hv_strain;           /* how long it has strained */
static f32 hv_anchor[3];           /* where the count shows: the heavy thing */
static u8 hv_type_logged[HV_MOVEBOX_TYPES];

static void hv_anchor_at(u32 actor) {
    const fopAc_ac_c* a = (const fopAc_ac_c*)actor;
    hv_anchor[0] = a->current.pos.x;
    hv_anchor[1] = a->current.pos.y;
    hv_anchor[2] = a->current.pos.z;
}

static int hv_alive_look(const remote_player* r) {
    return !r->has_look || !(r->look.death & (MP_DEATH_GHOST | MP_DEATH_DOWN));
}

/* The players of the local Link's room, the local one included. */
static u32 hv_room_players(const fopAc_ac_c* link) {
    u32 n = 1;
    const u8* play = wwhd_play_get();
    if (directory_valid && play) {
        char stage[STAGE_NAME_BYTES + 1];
        map_stage(play, stage);
        for (mp_u32 i = 0; i != directory.count; ++i) {
            const mp_dir_entry* e = &directory.entry[i];
            if (e->id == directory.self || (e->flags & MP_DIR_DEAD) || !map_same_stage(e->stage, stage)) continue;
            if (e->room == link->current.roomNo) ++n;
        }
        return n + (u32)pb_room_player();
    }
    for (int i = 0; i != MAX_REMOTES; ++i)
        if (remotes[i].active && hv_alive_look(&remotes[i])) ++n;
    return n + (u32)pb_room_player(); /* the Debug page's bot (push_bot.h) */
}

static u32 hv_needed_for(u32 players) {
    if (players <= 1) return 1;
    if (players == 2) return 2;
    return (2 * players + 2) / 3; /* two thirds, rounded up */
}

static int hv_proc_is(mp_u32 proc, int kind) {
    if (kind == HV_PUSH) return proc == HV_PROC_PUSH_PULL_WAIT || proc == HV_PROC_PUSH_MOVE || proc == HV_PROC_PULL_MOVE;
    return proc == HV_PROC_GRAB_READY || proc == HV_PROC_GRAB_MISS;
}

/* The local player and the remote ones doing `kind` by the local Link; *lowest: the lowest uid. */
static u32 hv_helpers(const fopAc_ac_c* link, int kind, mp_u32* lowest) {
    u32 n = 1;
    *lowest = local_uid;
    for (int i = 0; i != MAX_REMOTES; ++i) {
        const remote_player* r = &remotes[i];
        if (!r->active || !r->has_look || !r->has_transform || !hv_alive_look(r) || !hv_proc_is(r->look.proc, kind)) continue;
        f32 dx = r->transform.pos[0] - link->current.pos.x, dy = r->transform.pos[1] - link->current.pos.y;
        f32 dz = r->transform.pos[2] - link->current.pos.z;
        if (dx * dx + dy * dy + dz * dz > HV_HELP_RADIUS * HV_HELP_RADIUS) continue;
        ++n;
        if (r->uid < *lowest) *lowest = r->uid;
    }
    if (kind == HV_PUSH && pb_pushing_near(link, HV_HELP_RADIUS)) ++n; /* never the lifter */
    return n;
}

/* 1 when the local Link may do `kind` alone or with the helpers there; the count is shown. */
static int hv_enough(const fopAc_ac_c* link, int kind, int* lifter) {
    *lifter = 1;
    if ((state != READY || local_uid == MP_NO_ID) && !pb_room_player()) return 1; /* the bot: tested alone */
    u32 players = hv_room_players(link);
    if (players <= 1) return 1;
    mp_u32 lowest;
    hv_kind = kind;
    hv_needed = hv_needed_for(players);
    hv_count = hv_helpers(link, kind, &lowest);
    hv_show = HV_SHOW_SECONDS;
    *lifter = lowest == local_uid;
    return hv_count >= hv_needed;
}

static volatile u8* hv_bracelet(void) {
    u32 save = WWHD_GAME_DATA(0x101F84DC, u32);
    return save ? (volatile u8*)(save + HV_SAVE_BRACELET) : 0;
}

/* procGrabReady_init: a heavy actor is lifted only with enough helpers, by one of them. */
WWHD_REPLACE(0x023ED0F0, s32, hv_grab_ready, (void* self)) {
    const daPy_lk_c* link = (const daPy_lk_c*)self;
    volatile u8* bracelet = hv_bracelet();
    u32 actor = self && (u32)self == hv_link && !puppet_is(self) ? link->mpAttnActorA : 0;
    if (!actor || !bracelet || *bracelet != HV_BRACELET ||
        !(((const fopAc_ac_c*)actor)->actor_status & HV_STATUS_HEAVY))
        return hv_grab_ready_original(self);
    int lifter;
    hv_anchor_at(actor);
    if (hv_enough((const fopAc_ac_c*)self, HV_LIFT, &lifter) && lifter) {
        hv_lift_target = 0;
        return hv_grab_ready_original(self);
    }
    hv_lift_target = actor; /* strain at it: the game's lift without the bracelets */
    hv_strain = 0;
    *bracelet = HV_NO_EQUIP;
    s32 result = hv_grab_ready_original(self);
    *bracelet = HV_BRACELET;
    return result;
}

/* daObjMovebox::Act_c::PPCallBack: a heavy block moves only with enough players pushing or pulling. */
WWHD_REPLACE(0x023775DC, u32, hv_movebox_pp, (u32 target, u32 unused, s16 angle, u32 label)) {
    if (target && (label & 3) && hv_link) {
        u32 type = *(const volatile u32*)(target + HV_MOVEBOX_TYPE);
        int heavy = type < HV_MOVEBOX_TYPES &&
                    *(const volatile u8*)(HV_MOVEBOX_ATTR + type * HV_MOVEBOX_ATTR_SIZE + HV_MOVEBOX_HEAVY);
        if (type < HV_MOVEBOX_TYPES && !hv_type_logged[type]) { /* which blocks need the bracelets */
            hv_type_logged[type] = 1;
            wwhd_log_int(heavy ? "multiplayer: heavy: block type needing the bracelets (several players)"
                               : "multiplayer: heavy: block type anyone pushes alone",
                         (int)type);
        }
        if (type < HV_MOVEBOX_TYPES) { /* every block and crate (user's choice), not the heavy ones only */
            int lifter;
            hv_anchor_at(target);
            /* without the push direction (bits 0-1) the block counts no push at all, heavy or not */
            if (!hv_enough((const fopAc_ac_c*)hv_link, HV_PUSH, &lifter)) label &= ~(3u | 8u);
        }
    }
    return hv_movebox_pp_original(target, unused, angle, label);
}

/* Every step of the local Link: a straining Link lifts once enough players help and it is its turn. */
static void hv_step(void* link_actor, double dt) {
    hv_link = (u32)link_actor;
    if (hv_show > 0) hv_show -= dt;
    daPy_lk_c* link = (daPy_lk_c*)link_actor;
    if (!hv_lift_target) return;
    if (link->mCurProc != HV_PROC_GRAB_MISS || link->mpAttnActorA != hv_lift_target) {
        hv_lift_target = 0;
        return;
    }
    hv_strain += dt;
    if (link->mProcVar6 == 0 && hv_strain < HV_STRAIN_SECONDS) { /* still straining: wait for help */
        link->mProcVar0 = HV_MISS_HOLD;
        if (link->mFrameCtrlUnder[0].mRate < 0.01f)
            wwhd_daPy_lk_c__setSingleMoveAnime(link_actor, HV_ANM_GRABNG, 1.0f, 0.0f, 6, 1.0f);
    }
    hv_anchor_at(hv_lift_target);
    int lifter;
    if (hv_enough((const fopAc_ac_c*)link_actor, HV_LIFT, &lifter) && lifter) {
        wwhd_log("multiplayer: heavy: enough players, lifting");
        hv_grab_ready_original(link_actor);
        hv_lift_target = 0;
    }
}

static u32 hv_number(char* out, u32 v) {
    char digits[10];
    u32 n = 0, k = 0;
    do {
        digits[n++] = (char)('0' + v % 10);
        v /= 10;
    } while (v && n != sizeof(digits));
    while (n) out[k++] = digits[--n];
    return k;
}

static void hv_draw(u32 list) {
    const u8* play = wwhd_play_get();
    if (hv_show <= 0 || hv_kind == HV_NONE || !play || play[WWHD_PLAY_EVENT_RUNNING_OFFSET]) return;
    f32 x, y;
    if (!dh_project(hv_anchor, HV_COUNT_LIFT, &x, &y)) return;
    char text[24];
    u32 n = hv_number(text, hv_count);
    text[n++] = '/';
    n += hv_number(text + n, hv_needed);
    name_draw(list, x, y, text, n, hv_count >= hv_needed ? 0x6FE08Au : 0xF9C74Fu, 1.2f, 1.0f);
}
