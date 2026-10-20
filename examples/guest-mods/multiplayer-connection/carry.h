/* 0.3.11: the players catch, carry and throw each other.
 *
 * The hookshot catches a player. daHookshot_c::procShot (02178DDC, d_a_hookshot.cpp; SDK binding,
 * one argument) pulls the player to an actor its tip hits when that actor has the status 0x200000
 * (fopAcStts_UNK200000_e): it keeps the actor's id (mCarryActorID +0xD774) and the tip's offset
 * from it (mCarryOffset +0xD768, m2A4 +0xD500 its facing), sets the pull (mParameters 3, procPlayerPull
 * 02179888 in the ptmf +0xD7A8) and procPlayerPull then follows that actor. A puppet registers no
 * collider: before the game's own step, the tip's move of the step (mMoveVec +0xD51C, 15 times) is
 * tested against every living player's puppet (a cylinder CY_HOOK_RADIUS wide, CY_HOOK_TOP high)
 * and a hit takes that same course. Layout: include/d/actor/d_a_hookshot.h (WWHD_OFFSET checked).
 * Only the local Link's hookshot: a copy of another's (actor_sync.h, a replica) runs no proc. (0.3.12
 * skipped every hookshot actor_sync follows, the local one included, which it sends: no catch.)
 *
 * Carrying. A by a player's puppet (in front, CY_LIFT_RADIUS, standing or walking, hands free)
 * lifts it the game's way: mpAttnActorA (+0x65B8) is the puppet and procGrabReady_init (023ED0F0,
 * heavy.h hv_grab_ready_original) keeps it in mActorKeepGrab (+0x65A0: id, actor; setCarryNow); the
 * game's grab procs (d_a_player_main_07.cpp) then lift it, carry it (setGrabItemPos 0240A41C puts it
 * between Link's hands), put it down or throw it (procGrabThrow 0242E010: A while carrying). The
 * carried player's game hears it through CLIENT_PLAYER_CARRY (mp_protocol.h "carry", WWClient.java
 * sends it to every other client): its Link stays between the carrier's puppet's hands
 * (cl_LhandA 8 and cl_RhandA 12 of its model, as actor_sync.h as_rope_visual), placed after its
 * execute. Thrown, it jumps forward: procAutoJump_init (023F7958, d_a_player_main_03.cpp: the jump
 * off a ledge, speed and height from speedF) in the carrier's facing, then the speeds procGrabThrow
 * gives a thrown thing (speed.y 34, forward 19). Carried, A jumps off (a smaller jump), and the
 * carrier's game lets go (freeGrabItem 023DCF8C). A carried player carrying another keeps it (the
 * tower: lift the top one first). Every client notes who carries whom (cy_pairs): a carried player
 * is not lifted by a second one. */
#pragma once
#include "wwhd_guest.h"
#include "wwhd/bindings.h"

#define CY_PROC_WAIT 0x04
#define CY_PROC_MOVE 0x06
#define CY_PROC_ATN_MOVE 0x07
#define CY_PROC_WAIT_TURN 0x17
#define CY_PROC_GRAB_THROW 0x71
#define CY_GRAB_ACTOR 0x65A4        /* daPy_lk_c::mActorKeepGrab.mActor (d_a_player_main.h) */
#define CY_TRIGGER_A 0x01u          /* mItemTrigger: A (death.h dh_after_stick) */
#define CY_LIFT_RADIUS 100.0f
#define CY_LIFT_HEIGHT 60.0f
#define CY_LIFT_CONE 0x3000         /* the puppet within 67 degrees of Link's facing */
#define CY_THROW_SPEED_Y 34.0f      /* procGrabThrow: a thrown thing's speed.y and speedF */
#define CY_THROW_SPEED_F 19.0f
#define CY_JUMP_SPEED_Y 28.0f       /* carried, A: a hop off */
#define CY_JUMP_SPEED_F 6.0f
#define CY_AUTO_JUMP_SPEED 17.0f    /* procAutoJump_init's speedF ceiling */
#define CY_HANDS_DOWN 20.0f         /* the carried Link's feet under the carrier's hands */
#define CY_CARRY_HEIGHT 110.0f      /* at least this over the carrier's feet: on its head */
#define CY_KEEP_POS 0x1046CD48              /* l_debug_keep_pos (cXyz) */
#define CY_KEEP_SHAPE_ANGLE 0x1046CD10      /* l_debug_shape_angle (csXyz) */
#define CY_KEEP_CURRENT_ANGLE 0x1046CD08    /* l_debug_current_angle (csXyz) */
#define CY_PAIRS 8
#define CY_STATUS_NO_LIFT 0x08010000u /* fopAcStts_UNK8000000_e | fopAcStts_UNK10000_e (procGrabReady) */

#define CY_HOOK_RADIUS 45.0f
#define CY_HOOK_TOP 150.0f
#define CY_HOOK_SAMPLES 8
#define CY_HS_MODE_SHOT 1u
#define CY_HS_MODE_PULL 3u
#define CY_HS_SHIP_RIDE 0xD4FE
#define CY_HS_FACING 0xD500         /* m2A4 */
#define CY_HS_HOOK_ANGLE 0xD510
#define CY_HS_MOVE 0xD51C
#define CY_HS_CARRY_OFFSET 0xD768
#define CY_HS_CARRY_ID 0xD774
#define CY_HS_PROC 0xD7A8           /* ptmf: s16 d, s16 i, u32 f */
#define CY_HS_PROC_PULL 0x02179888u
#define CY_PC_ID 0x04               /* base_process_class::mBsPcId */

typedef struct cy_pair {
    int used;
    mp_u32 carried, carrier;
} cy_pair;

static mp_u32 cy_carried_by = MP_NO_ID;  /* who carries the local Link */
static mp_u32 cy_carrying = MP_NO_ID;    /* the player the local Link carries */
static u32 cy_carrying_actor;
/* 0.3.16: the carried Link's daPy_lk_c::mModeFlg (+0x6A70) ModeFlg_CAUGHT (0x10000000, no code of
 * Link sets it; d_a_player_main_04.cpp execute: with it, as on a rope, a ladder or the ship, the place
 * posMove gives is kept and mAcch.CrrPos's ground correction is dropped). Set by the mod while
 * carried, taken off when freed. */
#define CY_MODE_CAUGHT 0x10000000u
static int cy_caught;
static void cy_free_local(void) {
    cy_carried_by = MP_NO_ID;
    if (cy_caught && dh_link) ((daPy_lk_c*)dh_link)->mModeFlg &= ~CY_MODE_CAUGHT;
    cy_caught = 0;
}
static int cy_jump;                      /* a jump to start in Link's next execute */
static s16 cy_jump_angle;
static f32 cy_jump_y, cy_jump_f;
static cy_pair cy_pairs[CY_PAIRS];

WWHD_GAME_ORIGINAL(0x02178DDC, s32, cy_hook_shot_original, (void* self));
static int cy_hands(u32 link_actor, f32* out);

static void cy_pair_set(mp_u32 carried, mp_u32 carrier) {
    cy_pair* free_pair = 0;
    for (int i = 0; i != CY_PAIRS; ++i) {
        if (cy_pairs[i].used && cy_pairs[i].carried == carried) { cy_pairs[i].carrier = carrier; return; }
        if (!cy_pairs[i].used && !free_pair) free_pair = &cy_pairs[i];
    }
    if (free_pair) { free_pair->used = 1; free_pair->carried = carried; free_pair->carrier = carrier; }
}

static void cy_pair_drop(mp_u32 carried) {
    for (int i = 0; i != CY_PAIRS; ++i)
        if (cy_pairs[i].used && cy_pairs[i].carried == carried) cy_pairs[i].used = 0;
}

static int cy_is_carried(mp_u32 uid) {
    for (int i = 0; i != CY_PAIRS; ++i)
        if (cy_pairs[i].used && cy_pairs[i].carried == uid) {
            const remote_player* carrier = find_remote(cy_pairs[i].carrier, 0);
            if (carrier || cy_pairs[i].carrier == local_uid) return 1;
            cy_pairs[i].used = 0; /* its carrier left */
        }
    return 0;
}

static void cy_send(mp_u32 target, mp_u8 action, s16 angle) {
    if (state != READY || local_uid == MP_NO_ID || session.client_player_carry == MP_NO_ID) return;
    mp_carry c;
    c.target = target;
    c.sender = local_uid;
    c.action = action;
    c.angle = angle;
    mp_writer body = mp_writer_make(payload, sizeof(payload));
    mp_write_carry(&body, &c);
    send_payload(session.client_player_carry, &body);
}

/* A living remote player's puppet; 0 otherwise. */
static u32 cy_player_puppet(const puppet_slot* s) {
    if (!s->used || !s->actor || s->ghost || (s->uid & (PUPPET_BODY_UID | PUPPET_BOT_UID))) return 0;
    const remote_player* r = find_remote(s->uid, 0);
    if (!r || !r->has_transform || !hv_alive_look(r)) return 0;
    return puppet_is((const void*)s->actor) ? s->actor : 0;
}

static u32 cy_puppet_of(mp_u32 uid) {
    for (int i = 0; i != PUPPET_MAX; ++i)
        if (puppet_slots[i].used && puppet_slots[i].uid == uid) return cy_player_puppet(&puppet_slots[i]);
    return 0;
}

/* ---- the hookshot ---- */
WWHD_REPLACE(0x02178DDC, s32, cy_hook_shot, (void* self)) {
    fopAc_ac_c* h = (fopAc_ac_c*)self;
    const as_slot* copy = self ? as_find(self) : 0;
    if (!self || h->mParameters != CY_HS_MODE_SHOT || (copy && copy->replica)) return cy_hook_shot_original(self);
    u32 b = (u32)self;
    const volatile f32* move = (const volatile f32*)(b + CY_HS_MOVE);
    f32 step[3] = {move[0] * 15.0f, move[1] * 15.0f, move[2] * 15.0f}; /* procShot's move this step */
    for (int i = 0; i != PUPPET_MAX; ++i) {
        u32 actor = cy_player_puppet(&puppet_slots[i]);
        if (!actor || cull_player_far((const void*)actor)) continue;
        const fopAc_ac_c* p = (const fopAc_ac_c*)actor;
        for (int k = 1; k <= CY_HOOK_SAMPLES; ++k) {
            f32 t = (f32)k / (f32)CY_HOOK_SAMPLES;
            f32 x = h->current.pos.x + step[0] * t, y = h->current.pos.y + step[1] * t, z = h->current.pos.z + step[2] * t;
            f32 dx = x - p->current.pos.x, dz = z - p->current.pos.z, dy = y - p->current.pos.y;
            if (dx * dx + dz * dz > CY_HOOK_RADIUS * CY_HOOK_RADIUS || dy < 0.0f || dy > CY_HOOK_TOP) continue;
            /* procShot's course for an actor with the status 0x200000 */
            h->current.pos.x = x;
            h->current.pos.y = y;
            h->current.pos.z = z;
            volatile f32* offset = (volatile f32*)(b + CY_HS_CARRY_OFFSET);
            offset[0] = dx;
            offset[1] = dy;
            offset[2] = dz;
            *(volatile u32*)(b + CY_HS_CARRY_ID) = *(const volatile u32*)(actor + CY_PC_ID);
            *(volatile s16*)(b + CY_HS_FACING) = p->shape_angle.y;
            volatile s16* hook = (volatile s16*)(b + CY_HS_HOOK_ANGLE);
            hook[0] = h->shape_angle.x;
            hook[1] = h->shape_angle.y;
            hook[2] = 0;
            volatile f32* mv = (volatile f32*)(b + CY_HS_MOVE);
            mv[0] = mv[1] = mv[2] = 0.0f;
            h->mParameters = CY_HS_MODE_PULL;
            *(volatile u8*)(b + CY_HS_SHIP_RIDE) = 0;
            *(volatile s16*)(b + CY_HS_PROC) = 0;
            *(volatile s16*)(b + CY_HS_PROC + 2) = -1;
            *(volatile u32*)(b + CY_HS_PROC + 4) = CY_HS_PROC_PULL;
            static u32 logged;
            if (logged++ < 4) wwhd_log("multiplayer: the hookshot caught a player");
            return 1;
        }
    }
    return cy_hook_shot_original(self);
}

/* ---- carrying ---- */
static int cy_can_act(const fopAc_ac_c* link) {
    const u8* play = wwhd_play_get();
    return play && !play[WWHD_PLAY_EVENT_RUNNING_OFFSET] && !dh_ghost && !dh_dying && link;
}

/* The puppet in front of the local Link it may lift; 0 when none. */
static u32 cy_lift_target(const daPy_lk_c* link, mp_u32* uid) {
    const fopAc_ac_c* a = (const fopAc_ac_c*)link;
    u32 best = 0;
    f32 best_d2 = CY_LIFT_RADIUS * CY_LIFT_RADIUS;
    for (int i = 0; i != PUPPET_MAX; ++i) {
        const puppet_slot* s = &puppet_slots[i];
        u32 actor = cy_player_puppet(s);
        if (!actor || s->uid == cy_carried_by || cy_is_carried(s->uid)) continue;
        const fopAc_ac_c* p = (const fopAc_ac_c*)actor;
        f32 dx = p->current.pos.x - a->current.pos.x, dz = p->current.pos.z - a->current.pos.z;
        f32 dy = p->current.pos.y - a->current.pos.y;
        f32 d2 = dx * dx + dz * dz;
        if (d2 >= best_d2 || dy > CY_LIFT_HEIGHT || dy < -CY_LIFT_HEIGHT) continue;
        s16 toward = wwhd_cM_atan2s_hd(dx, dz);
        s16 off = (s16)(toward - a->shape_angle.y);
        if (off > CY_LIFT_CONE || off < -CY_LIFT_CONE) continue;
        best = actor;
        best_d2 = d2;
        *uid = s->uid;
    }
    return best;
}

/* daPy_lk_c::setStickData (death.h dh_after_stick), the local Link, before its proc runs: the
 * jump of a thrown or freed Link; carried, A jumps off; else A by a player lifts it. */
static void cy_after_stick(void* self) {
    daPy_lk_c* link = (daPy_lk_c*)self;
    fopAc_ac_c* a = (fopAc_ac_c*)self;
    volatile u8* trigger = (volatile u8*)self + DH_LINK_ITEM_TRIGGER;
    if (cy_jump) {
        cy_jump = 0;
        a->shape_angle.y = cy_jump_angle;
        a->current.angle.y = cy_jump_angle;
        a->speedF = CY_AUTO_JUMP_SPEED;
        wwhd_daPy_lk_c__procAutoJump_init(self);
        a->speed.y = cy_jump_y;
        link->mNormalSpeed = cy_jump_f;
        *trigger = (u8)(*trigger & ~CY_TRIGGER_A);
        return;
    }
    if (cy_carried_by != MP_NO_ID) {
        if (!(*trigger & CY_TRIGGER_A)) return;
        *trigger = (u8)(*trigger & ~CY_TRIGGER_A);
        cy_send(cy_carried_by, MP_CARRY_JUMP, a->shape_angle.y);
        cy_pair_drop(local_uid);
        cy_free_local();
        a->speedF = CY_AUTO_JUMP_SPEED;
        wwhd_daPy_lk_c__procAutoJump_init(self);
        a->speed.y = CY_JUMP_SPEED_Y;
        link->mNormalSpeed = CY_JUMP_SPEED_F;
        wwhd_log("multiplayer: jumped off the player carrying us");
        return;
    }
    if (!(*trigger & CY_TRIGGER_A) || !cy_can_act(a) || state != READY) return;
    s32 proc = link->mCurProc;
    if (proc != CY_PROC_WAIT && proc != CY_PROC_MOVE && proc != CY_PROC_ATN_MOVE && proc != CY_PROC_WAIT_TURN) return;
    if (*(const volatile u32*)((u32)self + CY_GRAB_ACTOR)) return;
    mp_u32 uid = MP_NO_ID;
    u32 target = cy_lift_target(link, &uid);
    if (!target) return;
    /* the statuses that make procGrabReady refuse (UNK10000 heavy, UNK8000000): a player is light */
    ((fopAc_ac_c*)target)->actor_status &= ~CY_STATUS_NO_LIFT;
    link->mpAttnActorA = target;
    hv_grab_ready_original(self);
    if (*(const volatile u32*)((u32)self + CY_GRAB_ACTOR) != target) return;
    *trigger = (u8)(*trigger & ~CY_TRIGGER_A);
    cy_carrying = uid;
    cy_carrying_actor = target;
    cy_pair_set(uid, local_uid);
    cy_send(uid, MP_CARRY_LIFT, a->shape_angle.y);
    wwhd_log("multiplayer: lifting a player");
}

/* 0.3.14: the carried Link was placed after its execute only: the game had already set its model's
 * matrices (it stayed where it stood on its own screen) and its next execute's ground check took it
 * back down. 0.3.15 placed it before its execute too: the log showed it in WAIT (proc 4) and back on
 * the ground (y 190 given, 168 after) every frame: mAcch.CrrPos put it down (the carrier's hands were
 * only 20 units above the ground). 0.3.16: placed before its execute only, with ModeFlg_CAUGHT
 * (cy_free_local): the game keeps that place, and Link's items, its camera and its jump when thrown
 * start from there. */
static void cy_place_local(void* self);
static f32 cy_target[3];      /* the place given before the execute */
static int cy_diag;           /* 0.3.15: frames of a carry still logged */
static void cy_before(void* self) {
    if (!self || puppet_is(self) || (u32)self != dh_link || cy_carried_by == MP_NO_ID) return;
    cy_place_local(self);
    if (cy_carried_by != MP_NO_ID) {
        ((daPy_lk_c*)self)->mModeFlg |= CY_MODE_CAUGHT;
        cy_caught = 1;
    }
}

/* After the local Link's execute: the carrier sees its burden go (thrown or put down); the carried
 * Link stays between its carrier's hands. */
static void cy_step(void* self) {
    if (!self || puppet_is(self) || (u32)self != dh_link) return;
    daPy_lk_c* link = (daPy_lk_c*)self;
    fopAc_ac_c* a = (fopAc_ac_c*)self;
    if (cy_carrying != MP_NO_ID) {
        u32 grab = *(const volatile u32*)((u32)self + CY_GRAB_ACTOR);
        if (grab != cy_carrying_actor) {
            int thrown = link->mCurProc == CY_PROC_GRAB_THROW;
            cy_send(cy_carrying, thrown ? MP_CARRY_THROW : MP_CARRY_PUT, a->shape_angle.y);
            cy_pair_drop(cy_carrying);
            cy_carrying = MP_NO_ID;
            cy_carrying_actor = 0;
        }
    }
    if (cy_carried_by == MP_NO_ID) return;
    if (cy_diag > 0) { /* where the game left Link, against the place given before its execute */
        --cy_diag;
        wwhd_log_int("multiplayer: carried: proc after execute", (int)link->mCurProc);
        wwhd_log_int("multiplayer:   y given before", (int)cy_target[1]);
        wwhd_log_int("multiplayer:   y after execute", (int)a->current.pos.y);
        wwhd_log_int("multiplayer:   x moved by execute", (int)(a->current.pos.x - cy_target[0]));
    }
    if (!cy_puppet_of(cy_carried_by) || !cy_can_act(a) || state != READY) cy_place_local(self); /* frees it */
}

static void cy_place_local(void* self) {
    daPy_lk_c* link = (daPy_lk_c*)self;
    fopAc_ac_c* a = (fopAc_ac_c*)self;
    u32 carrier = cy_puppet_of(cy_carried_by);
    if (!carrier || !cy_can_act(a) || state != READY) { /* its carrier left or died, an event began */
        cy_send(cy_carried_by, MP_CARRY_JUMP, a->shape_angle.y);
        cy_pair_drop(local_uid);
        cy_free_local();
        return;
    }
    const fopAc_ac_c* c = (const fopAc_ac_c*)carrier;
    f32 hands[3];
    f32 x, y, z;
    if (cy_hands(carrier, hands)) {
        x = hands[0];
        y = hands[1];
        z = hands[2];
    } else {
        x = c->current.pos.x;
        y = c->current.pos.y + 120.0f;
        z = c->current.pos.z;
    }
    a->current.pos.x = a->old.pos.x = x;
    a->current.pos.y = a->old.pos.y = y;
    a->current.pos.z = a->old.pos.z = z;
    cy_target[0] = x;
    cy_target[1] = y;
    cy_target[2] = z;
    a->speed.x = a->speed.y = a->speed.z = 0.0f;
    a->speedF = 0.0f;
    link->mNormalSpeed = 0.0f;
    a->shape_angle.y = c->shape_angle.y;
    a->current.angle.y = c->shape_angle.y;
    /* 0.3.22: daPy_lk_c::execute (d_a_player_main_04.cpp) begins with current.pos =
     * l_debug_keep_pos (1046CD48) and the angles from l_debug_shape_angle (1046CD10) and
     * l_debug_current_angle (1046CD08), which its end saves: a place written between two executes
     * was thrown away (log 0.3.15-0.3.21: y 809 given, 697, 695, 692... after: the carried Link
     * sank slowly from where it was, ModeFlg_CAUGHT dropping the ground). Given there too. */
    volatile f32* keep = &WWHD_GAME_DATA(CY_KEEP_POS, f32);
    keep[0] = x;
    keep[1] = y;
    keep[2] = z;
    (&WWHD_GAME_DATA(CY_KEEP_SHAPE_ANGLE, s16))[1] = c->shape_angle.y;
    (&WWHD_GAME_DATA(CY_KEEP_CURRENT_ANGLE, s16))[1] = c->shape_angle.y;
}

/* The middle of a Link's hands (cl_LhandA 8, cl_RhandA 12 of its model), CY_HANDS_DOWN lower. */
static int cy_hands(u32 link_actor, f32* out) {
    const daPy_lk_c* pl = (const daPy_lk_c*)link_actor;
    const f32* lh = pl->mpCLModel ? puppet_anm_mtx(pl->mpCLModel, 8) : 0;
    const f32* rh = pl->mpCLModel ? puppet_anm_mtx(pl->mpCLModel, 12) : 0;
    if (!lh || !rh) return 0;
    out[0] = (lh[3] + rh[3]) * 0.5f;
    out[1] = (lh[7] + rh[7]) * 0.5f - CY_HANDS_DOWN;
    out[2] = (lh[11] + rh[11]) * 0.5f;
    /* 0.3.16: a carrier's hands 40 units over its feet were logged (its puppet's pose): never lower
     * than over its head */
    const fopAc_ac_c* c = (const fopAc_ac_c*)link_actor;
    if (out[1] < c->current.pos.y + CY_CARRY_HEIGHT) {
        out[0] = c->current.pos.x;
        out[1] = c->current.pos.y + CY_CARRY_HEIGHT;
        out[2] = c->current.pos.z;
    }
    return 1;
}

/* 0.3.13, puppet.h puppet_execute: a carried player's puppet is drawn in its carrier's hands (the
 * local Link's, or the carrier's puppet's), not at the place its own game sends (late by the
 * network, and before the carry packet arrives there: only the name went up). 1 when placed. */
static int cy_place_puppet(mp_u32 uid, fopAc_ac_c* actor) {
    u32 carrier = 0;
    if (uid == cy_carrying && dh_link && cy_carrying_actor == (u32)actor) {
        carrier = dh_link;
    } else {
        for (int i = 0; i != CY_PAIRS && !carrier; ++i)
            if (cy_pairs[i].used && cy_pairs[i].carried == uid && cy_pairs[i].carrier != local_uid)
                carrier = cy_puppet_of(cy_pairs[i].carrier);
    }
    f32 at[3];
    if (!carrier || carrier == (u32)actor || !cy_hands(carrier, at)) return 0;
    actor->current.pos.x = at[0];
    actor->current.pos.y = at[1];
    actor->current.pos.z = at[2];
    actor->old.pos = actor->current.pos;
    actor->shape_angle.y = ((const fopAc_ac_c*)carrier)->shape_angle.y;
    actor->current.angle.y = actor->shape_angle.y;
    return 1;
}

/* SERVER_PLAYER_CARRY. */
static void cy_received(const mp_u8* body, mp_u32 length) {
    mp_carry c;
    if (!mp_read_carry(body, length, &c)) {
        wwhd_log("multiplayer: malformed carry skipped");
        return;
    }
    if (c.action == MP_CARRY_LIFT) cy_pair_set(c.target, c.sender);
    else if (c.action == MP_CARRY_JUMP) cy_pair_drop(c.sender);
    else cy_pair_drop(c.target);
    if (local_uid == MP_NO_ID || c.target != local_uid) return;
    switch (c.action) {
    case MP_CARRY_LIFT:
        if (cy_carrying == c.sender) { /* both lifted the other: neither */
            cy_send(c.sender, MP_CARRY_JUMP, 0);
            cy_pair_drop(local_uid);
            return;
        }
        cy_carried_by = c.sender;
        cy_diag = 4;
        wwhd_log("multiplayer: another player lifts us");
        if (!cy_puppet_of(c.sender)) wwhd_log("multiplayer:   but its puppet is not here: not carried");
        break;
    case MP_CARRY_PUT:
        if (cy_carried_by == c.sender) cy_free_local(); /* falls from the hands */
        break;
    case MP_CARRY_THROW:
        if (cy_carried_by != c.sender) break;
        cy_free_local();
        cy_jump = 1;
        cy_jump_angle = c.angle;
        cy_jump_y = CY_THROW_SPEED_Y;
        cy_jump_f = CY_THROW_SPEED_F;
        wwhd_log("multiplayer: thrown by another player");
        break;
    case MP_CARRY_JUMP: /* the player the local Link carries jumped off */
        if (cy_carrying == c.sender) {
            if (dh_link && *(const volatile u32*)(dh_link + CY_GRAB_ACTOR) == cy_carrying_actor)
                wwhd_daPy_lk_c__freeGrabItem((void*)dh_link);
            cy_carrying = MP_NO_ID;
            cy_carrying_actor = 0;
        }
        break;
    }
}

static void cy_reset(void) {
    cy_free_local();
    cy_carried_by = cy_carrying = MP_NO_ID;
    cy_carrying_actor = 0;
    cy_jump = 0;
    for (int i = 0; i != CY_PAIRS; ++i) cy_pairs[i].used = 0;
}
