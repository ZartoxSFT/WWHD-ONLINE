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

enum { HV_NONE, HV_PUSH, HV_LIFT, HV_SWITCH };

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

/* ---- the count over the other players' heavy things (0.2.91) ----
 * Each game also shows the count where a remote player pushes or pulls a block (its proc
 * PUSH_PULL_WAIT, PUSH_MOVE, PULL_MOVE) or strains at a heavy actor (GRAB_MISS only: GRAB_READY is
 * also every light lift), from what it already receives (ActorPlayerSync proc, position): the
 * helpers are that player, the other remote players doing the same within HV_HELP_RADIUS of it, the
 * local Link when it does too, and the push bot; the needed number is the one of the local Link's
 * room, as the pusher's own game counts it. No protocol change. Shown over the nearest synchronised
 * block (actor_sync.h slots, process 0x2B) within HV_BOX_REACH of the player, else over the player;
 * not twice for one thing (players of the same thing, or the local Link's own count). */
#define HV_REMOTE_SHOW_DISTANCE 3000.0f /* 30 m from the local Link */
#define HV_BOX_REACH 250.0f
#define HV_SAME_THING 120.0f

typedef struct hv_shown {
    f32 at[3];
    u32 count, needed;
} hv_shown;
static hv_shown hv_remote_shown[MAX_REMOTES];
static u32 hv_remote_count;

static f32 hv_dist2(const f32* a, const f32* b) {
    f32 dx = a[0] - b[0], dy = a[1] - b[1], dz = a[2] - b[2];
    return dx * dx + dy * dy + dz * dz;
}

static const f32* hv_remote_pos(const remote_player* r) { return r->has_display ? r->display.pos : r->transform.pos; }

/* The kind a remote player's proc starts a count for (HV_NONE: none). */
static int hv_remote_kind(mp_u32 proc) {
    if (hv_proc_is(proc, HV_PUSH)) return HV_PUSH;
    return proc == HV_PROC_GRAB_MISS ? HV_LIFT : HV_NONE;
}

/* The nearest synchronised block within HV_BOX_REACH of pos, written to out: 1 when found. */
static int hv_block_near(const f32* pos, f32* out) {
    f32 best = HV_BOX_REACH * HV_BOX_REACH;
    int found = 0;
    for (u32 i = 0; i != AS_MAX; ++i) {
        const as_slot* s = &as_slots[i];
        if (!s->used || !s->actor || *(const volatile u32*)(s->actor + AS_PC_ID) != s->pcid) continue;
        if (*(const volatile s16*)(s->actor + AS_PROC_NAME) != AS_PROC_MOVEBOX) continue;
        const fopAc_ac_c* a = (const fopAc_ac_c*)s->actor;
        f32 at[3] = {a->current.pos.x, a->current.pos.y, a->current.pos.z};
        f32 d = hv_dist2(at, pos);
        if (d > best) continue;
        best = d;
        out[0] = at[0];
        out[1] = at[1];
        out[2] = at[2];
        found = 1;
    }
    return found;
}

/* 0.3.13: the floor switches' counts for every player of the room. The switches of the stage are
 * learned from their execute (daObjSwpush::Act_c::Mthd_Execute 02398014, one argument, SDK binding;
 * an entry hook), forgotten when they stop running (HV_SWITCH_FORGET steps). A switch with a player
 * on it (HV_SWITCH_RADIUS, HV_SWITCH_HEIGHT; the local Link or a living remote one) shows its count
 * in every game, the same count hv_switch_ride gives, not twice over the local Link's own. */
#define HV_SWITCHES 16
#define HV_SWITCH_FORGET 30
#define HV_SWITCH_RADIUS 140.0f
#define HV_SWITCH_HEIGHT 120.0f
typedef struct hv_switch {
    u32 actor;
    unsigned long long seen;
} hv_switch;
static hv_switch hv_switches[HV_SWITCHES];

WWHD_HOOK(0x02398014, hv_switch_execute, (void* self)) {
    unsigned long long step = wwhd_logic_step();
    hv_switch* free_slot = 0;
    for (int i = 0; i != HV_SWITCHES; ++i) {
        hv_switch* s = &hv_switches[i];
        if (s->actor == (u32)self) { s->seen = step; return; }
        if (!free_slot && (!s->actor || step - s->seen > HV_SWITCH_FORGET)) free_slot = s;
    }
    if (free_slot) { free_slot->actor = (u32)self; free_slot->seen = step; }
}

/* 0.3.35: a player counts only standing ON the switch, as the game's own test (rideCB, decomp
 * d_a_obj_swpush.cpp): its top is the quad of the vertices 6, 10, 11, 7 (swpushOpVertices) of the
 * switch's background, Act_c::mpBgW (+0x3BC), whose vertex table pm_vtx_tbl (+0x90, world places,
 * 0xC each; count in pm_bgd +0x94 -> m_v_num) the switch moves with its top (Mthd_Execute writes
 * their y). The player's feet must be over that quad and within HV_TOP_BELOW / HV_TOP_ABOVE of its
 * height: a player pressed against the side, on the floor, no longer counts (the cylinder of
 * HV_SWITCH_RADIUS and HV_SWITCH_HEIGHT stays only for a switch whose background is not known). */
#define HV_SWITCH_BG 0x3BC
#define HV_BG_VERTICES 0x90
#define HV_BG_DATA 0x94
#define HV_TOP_BELOW 25.0f
#define HV_TOP_ABOVE 60.0f
static const u8 hv_switch_top[4] = {6, 10, 11, 7};

static int hv_switch_quad(const fopAc_ac_c* sw, f32 q[4][3]) {
    u32 bg = *(const volatile u32*)((const u8*)sw + HV_SWITCH_BG);
    u32 vtx = bg ? *(const volatile u32*)(bg + HV_BG_VERTICES) : 0;
    u32 data = bg ? *(const volatile u32*)(bg + HV_BG_DATA) : 0;
    if (!vtx || !data || *(const volatile s32*)data <= 11) return 0;
    for (int i = 0; i != 4; ++i)
        for (int c = 0; c != 3; ++c) q[i][c] = *(const volatile f32*)(vtx + hv_switch_top[i] * 0xCu + (u32)c * 4u);
    return 1;
}

static int hv_on_switch(const f32* pos, const fopAc_ac_c* sw) {
    f32 q[4][3];
    if (!hv_switch_quad(sw, q)) {
        f32 dx = pos[0] - sw->current.pos.x, dz = pos[2] - sw->current.pos.z, dy = pos[1] - sw->current.pos.y;
        return dx * dx + dz * dz <= HV_SWITCH_RADIUS * HV_SWITCH_RADIUS && dy > -HV_SWITCH_HEIGHT && dy < HV_SWITCH_HEIGHT;
    }
    f32 top = (q[0][1] + q[1][1] + q[2][1] + q[3][1]) * 0.25f, dy = pos[1] - top;
    if (dy < -HV_TOP_BELOW || dy > HV_TOP_ABOVE) return 0;
    int positive = 0, negative = 0; /* over the convex quad: the same side of its four edges */
    for (int i = 0; i != 4; ++i) {
        const f32* a = q[i];
        const f32* b = q[(i + 1) & 3];
        f32 cross = (b[0] - a[0]) * (pos[2] - a[2]) - (b[2] - a[2]) * (pos[0] - a[0]);
        if (cross > 0) positive = 1;
        if (cross < 0) negative = 1;
    }
    return !(positive && negative);
}

/* The players on that switch: the local Link and the living remote ones. */
static u32 hv_switch_players(const fopAc_ac_c* sw, const f32* here) {
    u32 on = hv_on_switch(here, sw) ? 1u : 0u;
    for (int i = 0; i != MAX_REMOTES; ++i) {
        const remote_player* r = &remotes[i];
        if (r->active && r->has_transform && hv_alive_look(r) && hv_on_switch(hv_remote_pos(r), sw)) ++on;
    }
    return on;
}

static void hv_remote_switches(const f32* here, u32 needed) {
    unsigned long long step = wwhd_logic_step();
    for (int i = 0; i != HV_SWITCHES && hv_remote_count != MAX_REMOTES; ++i) {
        const hv_switch* s = &hv_switches[i];
        if (!s->actor || step - s->seen > 2) continue;
        const fopAc_ac_c* sw = (const fopAc_ac_c*)s->actor;
        f32 at[3] = {sw->current.pos.x, sw->current.pos.y, sw->current.pos.z};
        if (hv_dist2(at, here) > HV_REMOTE_SHOW_DISTANCE * HV_REMOTE_SHOW_DISTANCE) continue;
        if (hv_show > 0 && hv_dist2(at, hv_anchor) < HV_SAME_THING * HV_SAME_THING) continue; /* its own */
        u32 on = hv_switch_players(sw, here);
        if (!on) continue;
        hv_shown* shown = &hv_remote_shown[hv_remote_count++];
        shown->at[0] = at[0];
        shown->at[1] = at[1];
        shown->at[2] = at[2];
        shown->count = on;
        shown->needed = needed;
    }
}

static void hv_remote_step(const daPy_lk_c* link) {
    hv_remote_count = 0;
    if (state != READY || local_uid == MP_NO_ID) return;
    const fopAc_ac_c* la = (const fopAc_ac_c*)link;
    f32 here[3] = {la->current.pos.x, la->current.pos.y, la->current.pos.z};
    u32 needed = hv_needed_for(hv_room_players(la));
    if (needed <= 1) return;
    for (int i = 0; i != MAX_REMOTES; ++i) {
        const remote_player* r = &remotes[i];
        if (!r->active || !r->has_look || !r->has_transform || !hv_alive_look(r)) continue;
        int kind = hv_remote_kind(r->look.proc);
        const f32* pos = hv_remote_pos(r);
        if (kind == HV_NONE || hv_dist2(pos, here) > HV_REMOTE_SHOW_DISTANCE * HV_REMOTE_SHOW_DISTANCE) continue;
        f32 at[3] = {pos[0], pos[1], pos[2]};
        if (kind == HV_PUSH) hv_block_near(pos, at);
        /* one count per thing: not over the local Link's own, not twice for the same thing */
        if (hv_show > 0 && hv_dist2(at, hv_anchor) < HV_SAME_THING * HV_SAME_THING) continue;
        int twice = 0;
        for (u32 k = 0; k != hv_remote_count && !twice; ++k)
            twice = hv_dist2(at, hv_remote_shown[k].at) < HV_SAME_THING * HV_SAME_THING;
        if (twice) continue;
        u32 count = 1;
        for (int j = 0; j != MAX_REMOTES; ++j) {
            const remote_player* o = &remotes[j];
            if (j == i || !o->active || !o->has_look || !o->has_transform || !hv_alive_look(o) ||
                !hv_proc_is(o->look.proc, kind))
                continue;
            if (hv_dist2(hv_remote_pos(o), pos) <= HV_HELP_RADIUS * HV_HELP_RADIUS) ++count;
        }
        if (hv_proc_is((mp_u32)link->mCurProc, kind) && hv_dist2(here, pos) <= HV_HELP_RADIUS * HV_HELP_RADIUS) ++count;
        if (kind == HV_PUSH && pb_pushing_near_pos(pos, HV_HELP_RADIUS)) ++count;
        hv_shown* s = &hv_remote_shown[hv_remote_count++];
        s->at[0] = at[0];
        s->at[1] = at[1];
        s->at[2] = at[2];
        s->count = count;
        s->needed = needed;
    }
    hv_remote_switches(here, needed);
}

/* 0.3.11: floor switches take the room too. The yellow floor switch daObjSwpush (d_a_obj_swpush.cpp,
 * its iron-boots kind included: FLAG_REQ_HEAVY) learns that an actor stands on it from its ride
 * callback rideCB (0239705C: background, switch, rider; SDK binding, the same in the EU build): a
 * rider within the switch's top sets mRidingMode (+0x480), and the switch goes down after its delay
 * (set_push_flag, mode_upper). Replaced: for the local Link, the callback counts only while enough
 * players of the room (hv_needed_for) stand on the switch: the local Link and the living remote players
 * within HV_SWITCH_RADIUS of its centre and HV_SWITCH_HEIGHT of its height; the count shows over it.
 * Every game does the same with its own Link: the switch goes down in each once they are all there.
 * Other riders (a pot, a block) press it as in the game. */
WWHD_GAME_ORIGINAL(0x0239705C, void, hv_switch_ride_original, (void* bg, void* sw, void* rider));
WWHD_REPLACE(0x0239705C, void, hv_switch_ride, (void* bg, void* sw, void* rider)) {
    if (!sw || !rider || (u32)rider != hv_link || puppet_is(rider)) {
        hv_switch_ride_original(bg, sw, rider);
        return;
    }
    const fopAc_ac_c* link = (const fopAc_ac_c*)rider;
    if ((state != READY || local_uid == MP_NO_ID) && !pb_room_player()) {
        hv_switch_ride_original(bg, sw, rider);
        return;
    }
    u32 players = hv_room_players(link);
    if (players <= 1) {
        hv_switch_ride_original(bg, sw, rider);
        return;
    }
    const fopAc_ac_c* s = (const fopAc_ac_c*)sw;
    f32 none[3] = {s->current.pos.x, s->current.pos.y + 10.0f * HV_SWITCH_HEIGHT, s->current.pos.z};
    u32 on = 1 + hv_switch_players(s, none); /* the local Link, riding, and the remote ones */
    hv_kind = HV_SWITCH;
    hv_needed = hv_needed_for(players);
    hv_count = on;
    hv_show = HV_SHOW_SECONDS;
    hv_anchor_at((u32)sw);
    if (on >= hv_needed) hv_switch_ride_original(bg, sw, rider);
    else {
        static u32 logged;
        if (logged++ < 3) wwhd_log("multiplayer: a floor switch waits for the players of the room");
    }
}

/* Every step of the local Link: a straining Link lifts once enough players help and it is its turn. */
static void hv_step(void* link_actor, double dt) {
    hv_link = (u32)link_actor;
    if (hv_show > 0) hv_show -= dt;
    hv_remote_step((const daPy_lk_c*)link_actor);
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

static void hv_draw_count(u32 list, const f32* at, u32 count, u32 needed) {
    f32 x, y;
    if (!dh_project(at, HV_COUNT_LIFT, &x, &y)) return;
    char text[24];
    u32 n = hv_number(text, count);
    text[n++] = '/';
    n += hv_number(text + n, needed);
    name_draw(list, x, y, text, n, count >= needed ? 0x6FE08Au : 0xF9C74Fu, 1.2f, 1.0f);
}

static void hv_draw(u32 list) {
    const u8* play = wwhd_play_get();
    if (!play || play[WWHD_PLAY_EVENT_RUNNING_OFFSET]) return;
    if (hv_show > 0 && hv_kind != HV_NONE) hv_draw_count(list, hv_anchor, hv_count, hv_needed);
    for (u32 i = 0; i != hv_remote_count; ++i) /* the other players' heavy things */
        hv_draw_count(list, hv_remote_shown[i].at, hv_remote_shown[i].count, hv_remote_shown[i].needed);
}
