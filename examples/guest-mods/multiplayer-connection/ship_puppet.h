/* Remote King of Red Lions puppets (with the option "puppet", like the Link puppets).
 *
 * Each remote player whose stage has its ship (ActorPlayerSync ship block, mp_protocol.h) gets a
 * second real daShip_c (process name fpcNm_SHIP_e 0xA5) carrying SHIP_PUPPET_PARAMS, placed where
 * that player's ship is, with that player's part deployed (sail, cannon or crane, through the
 * game's own daShip_c::setPartAnimeInit 0247BC44), its own sail cloth and its own paint
 * (boat_paint.h). Any other ship runs the game's code unchanged.
 *
 * What a second ship would change, from decomp/wwhd_src (d_a_ship_create.cpp, d_a_ship.cpp,
 * d_a_ship_exec.cpp, d_a_grid.cpp), and how it is prevented:
 *  - create (0247C358) ends with dComIfGp's ship actor = this (play + 0x5B3C, ShipCreatePlay):
 *    the pointer is saved before and restored after every call of the puppet's create;
 *  - the wind arrow (fopKyM_create 025DADA4, five arguments as in d_a_fan.cpp, fpcNm_WIND_ARROW_e
 *    0x16) is not created for a puppet;
 *  - on the sea, create also starts the sea events of the ship (fopAcM_createChild 025D5A20,
 *    fpcNm_Coming2_e 0x10E). fopAcM_createChild takes NINE arguments (decomp d_a_arrow.cpp, its
 *    ninth, the create callback, on the stack; the SDK binding lists eight): replacing it with
 *    eight passed a stray callback to every child the game created (0.2.13: "indirect branch to
 *    unknown address" while a stage loaded). It is left alone: a Coming2 whose parent
 *    (fopAc_ac_c::parentActorID +0x2E8, which daComing2 _execute 021138E0 follows) is a puppet
 *    ship deletes itself at its first execute;
 *  - the sail cloth (daGrid_c, fpcNm_GRID_e 0xAB, created by create, its process id in the ship's
 *    mGridID +0x6F8) reads its ship from ONE global (d_a_grid.h grid_shipAddress 1046438C, the
 *    first ship found by name when a cloth is created): while a puppet's cloth is created,
 *    executed or drawn (daGrid_Create 021655E0, daGrid_Execute 021644BC, daGrid_Draw 02162F68),
 *    the global holds its puppet ship and gets its value back after. The ship's execute places its
 *    cloth (d_a_ship_exec.cpp): position at body joint 7 J_FN_SAIL1, angles of the ship, scale.y
 *    the length of that joint's z axis (|SR * (0, 0, -365)| / 365), +0x2B78 one minus the length of
 *    joint 8 J_FN_SAIL2's x axis; the puppet does the same. The cloth is deleted with its ship;
 *  - execute (daShip_Execute 0247AE78) moves the ship from the pad, the camera, the waves and
 *    Link: the puppet runs only the matrix tail of execute (base matrix from the position and ZXY
 *    angles, mpBodyAnm play/calc, the head on body joint 4 J_FN_GATTAI, mpHeadAnm play/calc), its
 *    tiller top (+0x720 = joint 0xA J_FN_STEER1 * tiller_top_offset 101D03C0, where a riding Link
 *    puts his hand) and its cloth; the cannon (+0x418) and crane (+0x41C) models are placed by
 *    the body's mast joint callback and calculated while deployed; the crane's hook (+0x420, placed
 *    by setRopePos with the rope physics and splashes) is folded to a point;
 *  - shipDelete (daShip_Delete 0247AF74) clears the SAIL bit of player status 1 (play + 0x5CDC,
 *    0x400): that word is saved and restored around the puppet's deletion.
 * The ship's draw (daShip_Draw 02472AAC) runs unchanged, between the swap of its paint: it reads
 * only the puppet's own members; its head is hidden like the local one while the local Link rides
 * in first person, and the light reflection of the shared hull material follows the last ship
 * drawn. Not shown on a puppet: the cannon's aim, the rope, the wake and splashes. */
#pragma once
#include "wwhd_guest.h"
#include "wwhd/bindings.h"
#include "wwhd/actor.h"
#include "wwhd/data.h"
#include "mp_protocol.h"
#include "link_access.h"
#include "boat_paint.h"

#define SHIP_PUPPET_MAX 4
#define SHIP_PUPPET_PARAMS 0xFFAB0000u /* sail byte 0: created with the mast down */
#define SHIP_PROC 0xA5
#define SHIP_PROC_WIND_ARROW 0x16
#define SHIP_NO_PROC 0xFFFFFFFFu
#define SHIP_RETRIES 3
#define SHIP_PLAY_SHIP_ACTOR 0x5B3C
#define SHIP_PLAY_STATUS0 0x5CD8
#define SHIP_PLAY_STATUS1 0x5CDC
#define SHIP_STATUS0_RIDE 0x10000u
#define SHIP_BODY_ANM 0x3B4
#define SHIP_HEAD_ANM 0x3B8
#define SHIP_MORF_MODEL 0x90
#define SHIP_STATE_FLAG 0x644
#define SHIP_STATE_SAIL_ON 0x200u
#define SHIP_PART 0x637                 /* u8 mPart: 0 none, 1 sail, 2 cannon, 3 crane */
#define SHIP_PART_CANNON 2
#define SHIP_PART_CRANE 3
#define SHIP_CANNON_MODEL 0x418
#define SHIP_CRANE_MODEL 0x41C
#define SHIP_HOOK_MODEL 0x420
#define SHIP_GRID_ID 0x6F8
#define SHIP_TILLER_TOP 0x720
#define SHIP_JNT_GATTAI 4
#define SHIP_JNT_SAIL1 7
#define SHIP_JNT_SAIL2 8
#define SHIP_JNT_STEER1 0xA
#define SHIP_MODEL_BASE_TR 0xC8
#define SHIP_MODEL_ANM_BLOCK 0x2C
#define SHIP_GRID_SAIL_SHAPE 0x2B78     /* ShipExecGrid field_0x2200 */
#define SHIP_PROCESS_ID 0x04            /* base_process_class::mBsPcId (f_pc_base.h) */
#define SHIP_GRID_GLOBAL 0x1046438C     /* d_a_grid.h grid_shipAddress */
#define SHIP_TILLER_OFFSET 0x101D03C0   /* d_a_ship_create.cpp glue tiller_top_offset */
/* what the joint callbacks read (d_a_ship.cpp bodyJointCallBack, cannonJointCallBack,
 * craneJointCallBack; d_a_ship.h offsets): */
#define SHIP_SAIL_ANGLE 0x64C           /* mSailAngle: the sail's turn (also read by its cloth) */
#define SHIP_TILLER 0x64E               /* m0366: the tiller's turn */
#define SHIP_CANNON_PITCH 0x67C         /* m0394 */
#define SHIP_CANNON_YAW 0x67E           /* m0396 */
#define SHIP_CRANE_ANGLE 0x680          /* m0398; the arm turns by m0398 + m039C */
#define SHIP_CRANE_EXTRA 0x684          /* m039C */
#define SHIP_ROPE_COUNT 0x686           /* mRopeCnt: rope points drawn while >= 2 */
#define SHIP_ROPE_LINE 0x424            /* mRopeLine (mDoExt_3DlineMat1_c): lines at +0x184 */
#define SHIP_ROPE_LINES 0x184           /* each line 0x18 bytes: +0 its points (cXyz) */
#define SHIP_ROPE_MAX 0x7D              /* createHeap: ropeInit(1 line, 125 points) */
#define SHIP_ROPE_OFFSET 0x101D03CC     /* d_a_ship.cpp ship_ropeOffset {160, 0, 0} */
#define SHIP_JNT_CRANE 1                /* VFNCR_JNT_V_CRANE_ROTATION_e */
#define SHIP_NECK_PITCH 0x688           /* m03A0: headJointCallBack1, head joints 2..7 */
#define SHIP_NECK_TURN 0x68A            /* m03A2: the turn of each neck joint */

WWHD_GAME_ORIGINAL(0x0247C358, s32, ship_create_original, (void* self));
WWHD_GAME_ORIGINAL(0x0247AE78, s32, ship_execute_original, (void* i_this));
WWHD_GAME_ORIGINAL(0x0247AF74, s32, ship_delete_original, (void* i_this));
WWHD_GAME_ORIGINAL(0x02472AAC, s32, ship_draw_original, (void* i_this));
WWHD_GAME_ORIGINAL(0x021655E0, s32, ship_grid_create_original, (void* actor));
WWHD_GAME_ORIGINAL(0x021644BC, s32, ship_grid_execute_original, (void* a));
WWHD_GAME_ORIGINAL(0x02162F68, s32, ship_grid_draw_original, (void* a));
WWHD_GAME_ORIGINAL(0x021138E0, s32, ship_coming2_execute_original, (void* actor));
WWHD_GAME_ORIGINAL(0x025DADA4, u32, ship_ky_create_original, (s32 name, s32 param, void* pos, void* scale, void* callback));

typedef struct ship_puppet_slot {
    int used;
    mp_u32 uid;
    u32 proc;
    u32 actor;
    int retries;
    int has_target;
    mp_ship_state target;  /* last received */
    mp_ship_state display; /* smoothed towards target once per frame (ship_slot_advance) */
    f32 mtx[12];           /* the ship's base matrix from display, the same for the ship and its rider */
    int ship_ahead;        /* the ship advanced first this frame: its rider takes the same matrix */
    int link_ahead;        /* its riding Link puppet advanced first: the ship takes the same matrix */
} ship_puppet_slot;

static ship_puppet_slot ship_slots[SHIP_PUPPET_MAX];
static int ship_puppet_creating;
static int ship_puppet_failed;
static u32 ship_actor_out;
static cXyz ship_create_pos;
static csXyz ship_create_angle;
static f32 ship_mtx[12] __attribute__((aligned(16)));
static f32 ship_rot_mtx[12] __attribute__((aligned(16)));
#define SHIP_UPDATE_PERIOD 0.1 /* remote ship updates come at most every 0.1 s */

/* One frame of a puppet ship: the shown state one step towards the received one, then the base
 * matrix (mDoMtx_stack_c::transS(pos); mDoMtx_ZXYrotM(angles), as the tail of execute). Called once
 * per frame by whichever of the ship's execute or its riding Link puppet's comes first, so both
 * use the same matrix: the rider stays exactly on its deck whatever the actors' order. */
static void ship_slot_advance(ship_puppet_slot* s) {
    f32 blend = (f32)(wwhd_logic_dt() / SHIP_UPDATE_PERIOD);
    if (!(blend > 0.0f)) blend = 0.0f;
    if (blend > 1.0f) blend = 1.0f;
    for (int k = 0; k != 3; ++k) {
        s->display.pos[k] += (s->target.pos[k] - s->display.pos[k]) * blend;
        s16 d = (s16)(s->target.rot[k] - s->display.rot[k]); /* shortest way round */
        s->display.rot[k] = (s16)(s->display.rot[k] + (s16)((f32)d * blend));
        s->display.hook[k] += (s->target.hook[k] - s->display.hook[k]) * blend;
    }
    s16* now[7] = {&s->display.tiller, &s->display.sail, &s->display.cannon_pitch, &s->display.cannon_yaw,
                   &s->display.crane, &s->display.neck_pitch, &s->display.neck_turn};
    const s16 want[7] = {s->target.tiller, s->target.sail, s->target.cannon_pitch, s->target.cannon_yaw,
                         s->target.crane, s->target.neck_pitch, s->target.neck_turn};
    for (int k = 0; k != 7; ++k) *now[k] = (s16)(*now[k] + (s16)((f32)(s16)(want[k] - *now[k]) * blend));
    s->display.rope = s->target.rope;
    s->display.flags = s->target.flags;
    wwhd_mDoMtx_ZXYrotS((u32)ship_mtx, s->display.rot[0], s->display.rot[1], s->display.rot[2]);
    ship_mtx[3] = s->display.pos[0];
    ship_mtx[7] = s->display.pos[1];
    ship_mtx[11] = s->display.pos[2];
    memcpy(s->mtx, ship_mtx, sizeof(s->mtx));
}

static int ship_puppet_is(const void* actor) {
    if (!actor) return 0;
    const fopAc_ac_c* a = (const fopAc_ac_c*)actor;
    return a->mParameters == SHIP_PUPPET_PARAMS && *(const volatile s16*)((const u8*)actor + 8) == SHIP_PROC;
}

static u32 shipp_ld32(u32 address) { return *(const volatile u32*)address; }

/* The local Link's ship (dComIfGp ship actor), 0 when the stage has none. */
static u32 ship_local(void) {
    const u8* play = wwhd_play_get();
    u32 ship = play ? shipp_ld32((u32)play + SHIP_PLAY_SHIP_ACTOR) : 0;
    return ship && !(ship & 3) && !ship_puppet_is((const void*)ship) ? ship : 0;
}

/* The local ship's state for ActorPlayerSync. */
static mp_ship_state ship_local_state(void) {
    mp_ship_state s;
    s.flags = 0;
    for (int i = 0; i != 3; ++i) {
        s.pos[i] = 0.0f;
        s.rot[i] = 0;
        s.color[i] = 0;
    }
    u32 ship = ship_local();
    const u8* play = wwhd_play_get();
    if (!ship || !play) return s;
    const fopAc_ac_c* a = (const fopAc_ac_c*)ship;
    s.flags = MP_SHIP_PRESENT;
    if (shipp_ld32((u32)play + SHIP_PLAY_STATUS0) & SHIP_STATUS0_RIDE) s.flags |= MP_SHIP_RIDING;
    if (shipp_ld32(ship + SHIP_STATE_FLAG) & SHIP_STATE_SAIL_ON) s.flags |= MP_SHIP_SAIL;
    s.pos[0] = a->current.pos.x;
    s.pos[1] = a->current.pos.y;
    s.pos[2] = a->current.pos.z;
    s.rot[0] = a->shape_angle.x;
    s.rot[1] = a->shape_angle.y;
    s.rot[2] = a->shape_angle.z;
    u32 part = *(const volatile u8*)(ship + SHIP_PART);
    s.flags |= (mp_u8)((part & 3) << MP_SHIP_PART_SHIFT);
    if (ship_color_custom) { /* the launcher's boat colour (ship_material.h, boat_paint.h) */
        s.flags |= MP_SHIP_COLOR;
        s.color[0] = (mp_u8)(ship_color_rgb >> 16);
        s.color[1] = (mp_u8)(ship_color_rgb >> 8);
        s.color[2] = (mp_u8)ship_color_rgb;
    }
    s.flags |= MP_SHIP_DETAIL;
    s.tiller = *(const volatile s16*)(ship + SHIP_TILLER);
    s.sail = *(const volatile s16*)(ship + SHIP_SAIL_ANGLE);
    s.cannon_pitch = *(const volatile s16*)(ship + SHIP_CANNON_PITCH);
    s.cannon_yaw = *(const volatile s16*)(ship + SHIP_CANNON_YAW);
    s.crane = (s16)(*(const volatile s16*)(ship + SHIP_CRANE_ANGLE) + *(const volatile s16*)(ship + SHIP_CRANE_EXTRA));
    s.rope = *(const volatile s16*)(ship + SHIP_ROPE_COUNT);
    s.neck_pitch = *(const volatile s16*)(ship + SHIP_NECK_PITCH);
    s.neck_turn = *(const volatile s16*)(ship + SHIP_NECK_TURN);
    u32 hook = shipp_ld32(ship + SHIP_HOOK_MODEL);
    if (hook) {
        const volatile f32* m = (const volatile f32*)(hook + SHIP_MODEL_BASE_TR);
        s.hook[0] = m[3];
        s.hook[1] = m[7];
        s.hook[2] = m[11];
    }
    return s;
}

/* A remote player's puppet ship matrix as its execute sets it this step (from the same smoothed
 * place), for a riding Link puppet; 0 when there is none. */
static int ship_puppet_matrix(mp_u32 uid, f32* m) {
    for (int i = 0; i != SHIP_PUPPET_MAX; ++i) {
        ship_puppet_slot* s = &ship_slots[i];
        if (!s->used || s->uid != uid || !s->actor || !s->has_target) continue;
        if (s->ship_ahead) {
            s->ship_ahead = 0; /* the ship already advanced this frame: its matrix */
        } else {
            ship_slot_advance(s);
            s->link_ahead = 1;
        }
        memcpy(m, s->mtx, sizeof(s->mtx));
        return 1;
    }
    return 0;
}

/* The slot of a puppet ship actor, -1 for any other. */
static int ship_slot_of_actor(u32 actor) {
    for (int i = 0; i != SHIP_PUPPET_MAX; ++i)
        if (ship_slots[i].used && ship_slots[i].actor == actor) return i;
    return -1;
}

/* A remote player's puppet ship (0 when none) and whether that player rides it, for its Link
 * puppet (puppet.h). */
static u32 ship_puppet_of(mp_u32 uid, int* riding) {
    *riding = 0;
    for (int i = 0; i != SHIP_PUPPET_MAX; ++i) {
        const ship_puppet_slot* s = &ship_slots[i];
        if (!s->used || s->uid != uid || !s->actor || !s->has_target) continue;
        *riding = (s->target.flags & MP_SHIP_RIDING) != 0;
        return s->actor;
    }
    return 0;
}

/* The puppet ship a sail cloth belongs to (its process id is that ship's mGridID), 0 otherwise. */
static u32 ship_grid_owner(u32 grid) {
    u32 id = grid ? shipp_ld32(grid + SHIP_PROCESS_ID) : SHIP_NO_PROC;
    for (int i = 0; i != SHIP_PUPPET_MAX; ++i) {
        u32 ship = ship_slots[i].actor;
        if (ship_slots[i].used && ship && shipp_ld32(ship + SHIP_GRID_ID) == id) return ship;
    }
    return 0;
}

/* ------------------------------------------------------------------ creation */

WWHD_REPLACE(0x0247C358, s32, ship_puppet_create, (void* self)) {
    if (!ship_puppet_is(self)) return ship_create_original(self);
    u8* play = wwhd_play_get();
    if (!play) return 0; /* cPhs_INIT: wait */
    volatile u32* ship_actor = (volatile u32*)(play + SHIP_PLAY_SHIP_ACTOR);
    u32 saved = *ship_actor;
    puppet_status_keep status; /* player 0's ride and sail status (puppet.h): a new ship froze the sailing Link */
    puppet_status_save(&status);
    ++ship_puppet_creating;
    s32 result = ship_create_original(self);
    --ship_puppet_creating;
    *ship_actor = saved;
    puppet_status_restore(&status);
    return result;
}

static u32 ship_coming2_parent;

WWHD_REPLACE(0x021138E0, s32, ship_coming2_execute, (void* actor)) {
    u32 parent_id = ((const fopAc_ac_c*)actor)->parentActorID;
    ship_coming2_parent = 0;
    if (parent_id != SHIP_NO_PROC && wwhd_fopAcM_SearchByID(parent_id, (u32)&ship_coming2_parent) &&
        ship_puppet_is((const void*)ship_coming2_parent)) {
        wwhd_fopAcM_delete_actor((u32)actor); /* a puppet ship's sea events: gone */
        return 1;
    }
    return ship_coming2_execute_original(actor);
}

WWHD_REPLACE(0x025DADA4, u32, ship_ky_create, (s32 name, s32 param, void* pos, void* scale, void* callback)) {
    if (ship_puppet_creating && name == SHIP_PROC_WIND_ARROW) return SHIP_NO_PROC;
    return ship_ky_create_original(name, param, pos, scale, callback);
}

WWHD_REPLACE(0x0247AF74, s32, ship_puppet_delete, (void* i_this)) {
    if (!ship_puppet_is(i_this)) return ship_delete_original(i_this);
    puppet_status_keep status; /* both status words (puppet.h), SAIL included */
    puppet_status_save(&status);
    u32 grid = shipp_ld32((u32)i_this + SHIP_GRID_ID);
    if (grid != SHIP_NO_PROC) wwhd_fopAcM_delete_id(grid); /* its cloth goes with it */
    s32 result = ship_delete_original(i_this);
    puppet_status_restore(&status);
    for (int i = 0; i != SHIP_PUPPET_MAX; ++i)
        if (ship_slots[i].actor == (u32)i_this) {
            ship_slots[i].actor = 0;
            ship_slots[i].proc = SHIP_NO_PROC;
        }
    return result;
}

/* ---- a puppet's sail cloth: the grid global holds its ship during each of its calls ---- */

/* Any cloth's create sets the global to the first ship found by name, which may be a puppet: the
 * global never stays on a puppet (the local ship's cloth reads it every step). */
WWHD_REPLACE(0x021655E0, s32, ship_grid_create, (void* actor)) {
    u32 owner = ship_grid_owner((u32)actor);
    volatile u32* global = &WWHD_GAME_DATA(SHIP_GRID_GLOBAL, u32);
    u32 saved = *global;
    if (owner) *global = owner;
    s32 result = ship_grid_create_original(actor);
    if (owner) *global = saved;
    else if (*global && ship_puppet_is((const void*)*global)) *global = ship_local() ? ship_local() : saved;
    return result;
}

WWHD_REPLACE(0x021644BC, s32, ship_grid_execute, (void* a)) {
    u32 owner = ship_grid_owner((u32)a);
    if (!owner) return ship_grid_execute_original(a);
    volatile u32* global = &WWHD_GAME_DATA(SHIP_GRID_GLOBAL, u32);
    u32 saved = *global;
    *global = owner;
    s32 result = ship_grid_execute_original(a);
    *global = saved;
    return result;
}

WWHD_REPLACE(0x02162F68, s32, ship_grid_draw, (void* a)) {
    u32 owner = ship_grid_owner((u32)a);
    if (!owner) return ship_grid_draw_original(a);
    volatile u32* global = &WWHD_GAME_DATA(SHIP_GRID_GLOBAL, u32);
    u32 saved = *global;
    *global = owner;
    s32 result = ship_grid_draw_original(a);
    *global = saved;
    return result;
}

/* ---- a puppet ship in its owner's paint ---- */

WWHD_REPLACE(0x02472AAC, s32, ship_puppet_draw, (void* i_this)) {
    if (!ship_puppet_is(i_this)) return ship_draw_original(i_this);
    int slot = ship_slot_of_actor((u32)i_this);
    boat_swap_begin(slot, (u32)i_this);
    s32 result = ship_draw_original(i_this);
    boat_swap_end();
    return result;
}

/* ------------------------------------------------------------------ per-step update */

static int ship_is_orphan(u32 actor) {
    if (!puppet_enabled || ship_puppet_failed) return 1;
    for (int i = 0; i != SHIP_PUPPET_MAX; ++i) {
        const ship_puppet_slot* s = &ship_slots[i];
        if (!s->used) continue;
        if (s->actor == actor) return 0;
        if (s->proc != SHIP_NO_PROC && s->actor == 0) return 0;
    }
    return 1;
}

static u32 ship_morf_model(u32 ship, u32 offset) {
    u32 morf = shipp_ld32(ship + offset);
    return morf ? shipp_ld32(morf + SHIP_MORF_MODEL) : 0;
}

static f32 ship_length(f32 x, f32 y, f32 z);
static f32 ship_hook[3];
static int ship_has_hook;

/* The crane's rope and hook (the game's setRopePos simulates the rope, with splashes): the hook at
 * its owner's place, with the ship's turn, and the rope straight from the arm's tip (the arm's
 * rotation joint * rope_offset, as setRopePos) down to it, point 0 at the hook. Without the hook's
 * place (an older sender) the hook is folded to a point and no rope is drawn. */
static void ship_place_rope(u32 ship, u32 crane) {
    u32 hook = shipp_ld32(ship + SHIP_HOOK_MODEL);
    s16 count = *(const volatile s16*)(ship + SHIP_ROPE_COUNT);
    if (!ship_has_hook) {
        if (hook) {
            memset((void*)(hook + SHIP_MODEL_BASE_TR), 0, sizeof(ship_mtx));
            puppet_j3d_model_calc(hook);
        }
        *(volatile s16*)(ship + SHIP_ROPE_COUNT) = 0;
        return;
    }
    u32 block = shipp_ld32(crane + SHIP_MODEL_ANM_BLOCK);
    u32 matrices = block ? shipp_ld32(block + 0x10) : 0;
    if (!matrices) {
        *(volatile s16*)(ship + SHIP_ROPE_COUNT) = 0;
        return;
    }
    *(volatile u16*)(block + 4) |= 0x10;
    const f32* t = (const f32*)(matrices + SHIP_JNT_CRANE * 0x30);
    const volatile f32* o = &WWHD_GAME_DATA(SHIP_ROPE_OFFSET, f32);
    f32 tip[3];
    for (int r = 0; r != 3; ++r) tip[r] = t[r * 4] * o[0] + t[r * 4 + 1] * o[1] + t[r * 4 + 2] * o[2] + t[r * 4 + 3];
    if (hook) {
        /* setRopePos (d_a_ship.cpp): the hook turned along the rope's last segment c (point 0 minus
         * point 1; here the straight rope, hook minus tip) seen in the ship's heading frame:
         * ZXY(atan2s(cz', cy), heading, atan2s(-cx', |(cy, cz')|)) then XrotM(-0x4000), at point 0 */
        f32 cx = ship_hook[0] - tip[0], cy = ship_hook[1] - tip[1], cz = ship_hook[2] - tip[2];
        s16 heading = ((const fopAc_ac_c*)ship)->shape_angle.y;
        wwhd_mDoMtx_YrotS((u32)ship_rot_mtx, heading); /* [0] cos, [2] sin of the heading */
        f32 cs = ship_rot_mtx[0], sn = ship_rot_mtx[2];
        f32 bcx = cs * cx - sn * cz, bcz = sn * cx + cs * cz, bcy = cy;
        s16 ax = wwhd_cM_atan2s_hd(bcz, bcy);
        s16 az = wwhd_cM_atan2s_hd(-bcx, ship_length(0.0f, bcy, bcz));
        wwhd_mDoMtx_ZXYrotS((u32)ship_rot_mtx, ax, heading, az);
        f32 m[12];
        for (int r = 0; r != 3; ++r) { /* * XrotM(-0x4000): columns (c0, -c2, c1) */
            m[r * 4] = ship_rot_mtx[r * 4];
            m[r * 4 + 1] = -ship_rot_mtx[r * 4 + 2];
            m[r * 4 + 2] = ship_rot_mtx[r * 4 + 1];
            m[r * 4 + 3] = ship_hook[r];
        }
        memcpy((void*)(hook + SHIP_MODEL_BASE_TR), m, sizeof(m));
        puppet_j3d_model_calc(hook);
    }
    u32 lines = shipp_ld32(ship + SHIP_ROPE_LINE + SHIP_ROPE_LINES);
    u32 points = lines ? shipp_ld32(lines) : 0;
    if (!points || count < 2) {
        *(volatile s16*)(ship + SHIP_ROPE_COUNT) = 0;
        return;
    }
    for (s16 k = 0; k != count; ++k) {
        f32 a = (f32)k / (f32)(count - 1); /* 0 at the hook, 1 at the tip */
        f32* p = (f32*)(points + (u32)k * 12);
        for (int r = 0; r != 3; ++r) p[r] = ship_hook[r] + (tip[r] - ship_hook[r]) * a;
    }
}

/* Length of a vector of matrix axis values (0..a few): Newton, the 750 has no fsqrt. */
static f32 ship_length(f32 x, f32 y, f32 z) {
    f32 v = x * x + y * y + z * z;
    if (!(v > 1.0e-8f)) return 0.0f;
    f32 r = v > 1.0f ? v : 1.0f;
    for (int i = 0; i != 24; ++i) r = 0.5f * (r + v / r); /* from max(v, 1): enough up to rope lengths */
    return r;
}

/* Replaces daShip_Execute for a puppet: the remote ship's place, part and paint, then the matrices
 * of the tail of execute, its tiller top and its cloth. */
WWHD_REPLACE(0x0247AE78, s32, ship_puppet_execute, (void* i_this)) {
    if (!ship_puppet_is(i_this)) return ship_execute_original(i_this);
    u32 ship = (u32)i_this;
    fopAc_ac_c* actor = (fopAc_ac_c*)i_this;
    if (ship_is_orphan(ship)) {
        wwhd_fopAcM_delete_actor(ship);
        return 1;
    }
    u32 body_morf = shipp_ld32(ship + SHIP_BODY_ANM), head_morf = shipp_ld32(ship + SHIP_HEAD_ANM);
    u32 body = ship_morf_model(ship, SHIP_BODY_ANM), head = ship_morf_model(ship, SHIP_HEAD_ANM);
    if (!body || !head) return 1;

    const f32* frame_mtx = 0;
    for (int i = 0; i != SHIP_PUPPET_MAX; ++i) {
        ship_puppet_slot* s = &ship_slots[i];
        if (!s->used || s->actor != ship || !s->has_target) continue;
        if (s->link_ahead) {
            s->link_ahead = 0; /* its rider already advanced this frame */
        } else {
            ship_slot_advance(s);
            s->ship_ahead = 1;
        }
        frame_mtx = s->mtx;
        actor->old.pos = actor->current.pos;
        actor->current.pos.x = s->display.pos[0];
        actor->current.pos.y = s->display.pos[1];
        actor->current.pos.z = s->display.pos[2];
        actor->shape_angle.x = s->display.rot[0];
        actor->shape_angle.y = s->display.rot[1];
        actor->shape_angle.z = s->display.rot[2];
        actor->current.angle.y = s->display.rot[1];
        /* the owner's part: the game lowers the mast, then raises it with the new part */
        u8 part = (u8)((s->target.flags & MP_SHIP_PART_MASK) >> MP_SHIP_PART_SHIFT);
        wwhd_daShip_c__setPartAnimeInit_0247BC44(i_this, part);
        if (s->target.flags & MP_SHIP_DETAIL) { /* read by the joint callbacks during the calcs below */
            *(volatile s16*)(ship + SHIP_TILLER) = s->display.tiller;
            *(volatile s16*)(ship + SHIP_SAIL_ANGLE) = s->display.sail;
            *(volatile s16*)(ship + SHIP_CANNON_PITCH) = s->display.cannon_pitch;
            *(volatile s16*)(ship + SHIP_CANNON_YAW) = s->display.cannon_yaw;
            *(volatile s16*)(ship + SHIP_CRANE_ANGLE) = s->display.crane;
            *(volatile s16*)(ship + SHIP_CRANE_EXTRA) = 0;
            *(volatile s16*)(ship + SHIP_NECK_PITCH) = s->display.neck_pitch;
            *(volatile s16*)(ship + SHIP_NECK_TURN) = s->display.neck_turn;
            s16 rope = s->target.rope < 0 ? 0 : s->target.rope > SHIP_ROPE_MAX ? SHIP_ROPE_MAX : s->target.rope;
            *(volatile s16*)(ship + SHIP_ROPE_COUNT) = part == SHIP_PART_CRANE ? rope : 0;
            ship_hook[0] = s->display.hook[0];
            ship_hook[1] = s->display.hook[1];
            ship_hook[2] = s->display.hook[2];
            ship_has_hook = 1;
        } else {
            ship_has_hook = 0;
        }
        boat_prepare_slot(i, (s->target.flags & MP_SHIP_COLOR) != 0,
                          ((u32)s->target.color[0] << 16) | ((u32)s->target.color[1] << 8) | s->target.color[2]);
    }

    /* mDoMtx_stack_c::transS(pos); mDoMtx_ZXYrotM(angles) */
    if (frame_mtx) {
        memcpy(ship_mtx, frame_mtx, sizeof(ship_mtx));
    } else {
        wwhd_mDoMtx_ZXYrotS((u32)ship_mtx, actor->shape_angle.x, actor->shape_angle.y, actor->shape_angle.z);
        ship_mtx[3] = actor->current.pos.x;
        ship_mtx[7] = actor->current.pos.y;
        ship_mtx[11] = actor->current.pos.z;
    }
    memcpy((void*)(body + SHIP_MODEL_BASE_TR), ship_mtx, sizeof(ship_mtx));
    wwhd_ext_play_025E535C((void*)body_morf, 0, 0, 0);
    wwhd_ext_55A0_025E55A0((void*)body_morf);
    u32 block = shipp_ld32(body + SHIP_MODEL_ANM_BLOCK);
    u32 matrices = block ? shipp_ld32(block + 0x10) : 0;
    if (!matrices) return 1;
    *(volatile u16*)(block + 4) |= 0x10; /* as model_getAnmMtx */
    memcpy((void*)(head + SHIP_MODEL_BASE_TR), (const void*)(matrices + SHIP_JNT_GATTAI * 0x30), sizeof(ship_mtx));
    wwhd_ext_play_025E535C((void*)head_morf, 0, 0, 0);
    wwhd_ext_55A0_025E55A0((void*)head_morf);

    /* the tiller top, where a riding Link puts his hand */
    {
        const f32* m = (const f32*)(matrices + SHIP_JNT_STEER1 * 0x30);
        const volatile f32* o = &WWHD_GAME_DATA(SHIP_TILLER_OFFSET, f32);
        f32* top = (f32*)(ship + SHIP_TILLER_TOP);
        for (int r = 0; r != 3; ++r) top[r] = m[r * 4] * o[0] + m[r * 4 + 1] * o[1] + m[r * 4 + 2] * o[2] + m[r * 4 + 3];
    }

    /* the cannon and crane, placed by the mast joint callback, while deployed */
    u8 part = *(const volatile u8*)(ship + SHIP_PART);
    if (part == SHIP_PART_CANNON && shipp_ld32(ship + SHIP_CANNON_MODEL))
        puppet_j3d_model_calc(shipp_ld32(ship + SHIP_CANNON_MODEL));
    if (part == SHIP_PART_CRANE && shipp_ld32(ship + SHIP_CRANE_MODEL)) {
        u32 crane = shipp_ld32(ship + SHIP_CRANE_MODEL);
        puppet_j3d_model_calc(crane);
        ship_place_rope(ship, crane);
    } else {
        *(volatile s16*)(ship + SHIP_ROPE_COUNT) = 0;
    }

    /* its sail cloth, as the ship's execute places it */
    ship_actor_out = 0;
    u32 grid_id = shipp_ld32(ship + SHIP_GRID_ID);
    if (grid_id != SHIP_NO_PROC && wwhd_fopAcM_SearchByID(grid_id, (u32)&ship_actor_out) && ship_actor_out) {
        fopAc_ac_c* grid = (fopAc_ac_c*)ship_actor_out;
        const f32* sail1 = (const f32*)(matrices + SHIP_JNT_SAIL1 * 0x30);
        const f32* sail2 = (const f32*)(matrices + SHIP_JNT_SAIL2 * 0x30);
        grid->current.pos.x = sail1[3];
        grid->current.pos.y = sail1[7];
        grid->current.pos.z = sail1[11];
        grid->current.angle.x = actor->shape_angle.x;
        grid->current.angle.y = actor->shape_angle.y;
        grid->current.angle.z = actor->shape_angle.z;
        grid->scale.y = ship_length(sail1[2], sail1[6], sail1[10]);
        *(volatile f32*)(ship_actor_out + SHIP_GRID_SAIL_SHAPE) = 1.0f - ship_length(sail2[0], sail2[4], sail2[8]);
    }
    return 1;
}

/* ------------------------------------------------------------------ lifecycle */

static ship_puppet_slot* ship_slot_for(mp_u32 uid, int create) {
    ship_puppet_slot* free_slot = 0;
    for (int i = 0; i != SHIP_PUPPET_MAX; ++i) {
        if (ship_slots[i].used && ship_slots[i].uid == uid) return &ship_slots[i];
        if (!ship_slots[i].used && !free_slot) free_slot = &ship_slots[i];
    }
    if (!create || !free_slot) return 0;
    free_slot->used = 1;
    free_slot->uid = uid;
    free_slot->proc = SHIP_NO_PROC;
    free_slot->actor = 0;
    free_slot->retries = 0;
    free_slot->has_target = 0;
    free_slot->ship_ahead = free_slot->link_ahead = 0;
    return free_slot;
}

/* Like puppet_release: only a ship found created is deleted here; one still being created
 * deletes itself as an orphan once it runs. Its paint copies are freed. */
static void ship_release(ship_puppet_slot* s) {
    if (s->proc != SHIP_NO_PROC && s->actor != 0) wwhd_fopAcM_delete_id(s->proc);
    boat_free_slot((int)(s - ship_slots));
    s->used = 0;
    s->actor = 0;
    s->proc = SHIP_NO_PROC;
}

static void ship_puppet_remove(mp_u32 uid) {
    ship_puppet_slot* s = ship_slot_for(uid, 0);
    if (s) ship_release(s);
}

static void ship_puppet_clear_all(void) {
    for (int i = 0; i != SHIP_PUPPET_MAX; ++i)
        if (ship_slots[i].used) ship_release(&ship_slots[i]);
}

/* A remote player's ship state (ActorPlayerSync); no ship: its puppet goes. */
static void ship_puppet_set(mp_u32 uid, const mp_ship_state* state) {
    if (!(state->flags & MP_SHIP_PRESENT)) {
        ship_puppet_remove(uid);
        return;
    }
    if (!puppet_enabled || ship_puppet_failed) return;
    ship_puppet_slot* s = ship_slot_for(uid, 1);
    if (!s) return;
    if (!s->has_target) s->display = *state;
    s->target = *state;
    s->has_target = 1;
}

/* Creates missing ship puppets; only while the local stage has its own ship (its archive is
 * loaded and a ship belongs here) and outside events. */
static void ship_puppet_step(void) {
    if (!puppet_enabled || ship_puppet_failed) return;
    const u8* play = wwhd_play_get();
    if (!play || play[WWHD_PLAY_EVENT_RUNNING_OFFSET] || !ship_local()) return;
    for (int i = 0; i != SHIP_PUPPET_MAX; ++i) {
        ship_puppet_slot* s = &ship_slots[i];
        if (!s->used || !s->has_target || s->retries > SHIP_RETRIES) continue; /* gave up for this player */
        if (s->proc != SHIP_NO_PROC) {
            ship_actor_out = 0;
            s32 found = wwhd_fopAcM_SearchByID(s->proc, (u32)&ship_actor_out);
            if (found && ship_actor_out) {
                if (!ship_puppet_is((const void*)ship_actor_out)) {
                    wwhd_log("multiplayer: ship puppet id resolved to another actor, ship puppets disabled");
                    ship_puppet_failed = 1;
                    ship_puppet_clear_all();
                    return;
                }
                s->actor = ship_actor_out;
                s->retries = 0;
                continue;
            }
            if (found) continue; /* still being created */
            s->actor = 0;
            s->proc = SHIP_NO_PROC;
            if (++s->retries > SHIP_RETRIES) {
                wwhd_log("multiplayer: ship puppet creation failed repeatedly for one player");
                continue;
            }
        }
        ship_create_pos.x = s->display.pos[0];
        ship_create_pos.y = s->display.pos[1];
        ship_create_pos.z = s->display.pos[2];
        ship_create_angle.x = 0;
        ship_create_angle.y = s->display.rot[1];
        ship_create_angle.z = 0;
        /* room -1, like the ship's own children: not deleted when the local Link changes room */
        s->proc = wwhd_fopAcM_create(SHIP_PROC, SHIP_PUPPET_PARAMS, (u32)&ship_create_pos, -1,
                                     (u32)&ship_create_angle, 0, 0xFFFFFFFFu, 0);
        if (s->proc == SHIP_NO_PROC) wwhd_log("multiplayer: ship puppet creation request refused");
        else wwhd_log_int("multiplayer: ship puppet requested for uid", (int)s->uid);
    }
}
