/* 0.3.92: a passenger on another player's King of Red Lions.
 *
 * Near another player's ship while that player rides it (its puppet ship, ship_puppet.h, flag
 * MP_SHIP_RIDING of its ActorPlayerSync ship block), the local Link standing, walking or swimming
 * gets the prompt "A: board"; A seats it on that ship beside its pilot, facing the ship's heading,
 * and keeps it there every step (0.3.93: seated, beside; 0.3.92 stood in front). B (0.3.94; A in 0.3.92-0.3.93) jumps off at any time (as a carried Link does,
 * carry.h); it also gets off when the owner leaves the ship, the ship goes, an event begins, the
 * Link dies or the stage changes.
 *
 * Held as a carried Link is (carry.h, verified there): before its execute, the place and the angles
 * are written into the Link and into the copies daPy_lk_c::execute starts from (l_debug_keep_pos
 * 1046CD48, l_debug_shape_angle 1046CD10, l_debug_current_angle 1046CD08, d_a_player_main_04.cpp),
 * with ModeFlg_CAUGHT (0x10000000: the place posMove gives is kept, no ground correction).
 * Seated (0.3.93) as the game seats Link on his own ship in a cutscene, daPy_lk_c::dProcShipSit_init
 * (02425E98, d_a_player_main_06.cpp): proc daPyProc_DEMO_SHIP_SIT_e 0xD7 (commonProcInit) and
 * ANM_KOSHIKAKE 0xDC (setSingleMoveAnime 1.0, 0, -1; recorded and sent like any animation, so the
 * puppets sit too). dProcShipSit_init itself is not called: it marks the local player's own ship
 * (onLinkSit, setPaddleMove). Its proc dProcShipSit (02425FD8, one argument: code generated, r3
 * only; setShipRidePos on the local ship, then evmng cutEnd) does nothing for the passenger.
 *
 * The place: the ship's base matrix (the puppet ship's, the same matrix as its riding Link puppet:
 * ship_slot_sync) * SR_SEAT, the pilot standing at l_ship_offset {0, 15, -35} (10034F84,
 * setShipRidePos 023E287C, puppet.h). The others: "#SB_<owner's uid, 8 hex>" (event of the room,
 * item_share.h transport) sent at boarding and every SR_RESEND_STEPS, "#SL" when getting off; each
 * game puts that passenger's puppet on the owner's ship from its own copy of it (the owner's game:
 * its own ship), not at the received places, which come later than the ship's. */
#pragma once
#include "wwhd_guest.h"
#include "wwhd/bindings.h"

/* 0.3.95: the other side and the other turn than 0.3.94 (-48, heading - 0x4000), which the user saw
 * on the pilot's left, turned right: now +48, heading + 0x4000, seen in game as the right side,
 * turned left. (The sides deduced from the axes in 0.3.94 were the wrong way round.) */
/* 0.3.96: seen in game with 0.3.95 (log: build of 23:45 loaded): +48 is the ship's LEFT. The right
 * is -48; the turn stays + 0x4000 (left, as asked). */
#define SR_SEAT_X -48.0f           /* beside the pilot (l_ship_offset {0, 15, -35}) */
#define SR_TURN 0x4000
#define SR_TRIGGER_B 0x02u         /* mItemTrigger B (checkSubjectEnd: BTN_A 1 | BTN_B 2, d_a_player_main_04b.cpp) */
/* 0.3.94: the camera went to a ship when boarding. Its boat camera (d_camera_mode.cpp: mCamTypeBoat,
 * then rideCamera d_camera_engine2.cpp, which follows the first ship found by process name) is
 * chosen from player 0's status: status0 0x1000000 | SHIP_RIDE 0x10000 (play + 0x5CD8), status1 0x80
 * (+0x5CDC; rideCamera also reads 2 and 4). What set them for the passenger is not found in the
 * sources: they are logged at boarding and cleared every step while the local Link is a passenger
 * (it never rides its own ship then: sr_can_ride). */
#define SR_STATUS0_BOAT 0x1010000u
#define SR_STATUS1_BOAT 0x86u
#define SR_SEAT_Y 7.0f             /* 0.3.99: a little lower (15 sat too high, seen in game) */
#define SR_SEAT_Z -35.0f
#define SR_PROC_SHIP_SIT 0xD7      /* daPyProc_DEMO_SHIP_SIT_e (d_a_player_main_06.h) */
#define SR_ANM_KOSHIKAKE 0xDC      /* ANM_KOSHIKAKE (d_a_player_main_06.h) */
#define SR_REACH 320.0f            /* from the ship's place, horizontally */
#define SR_REACH_HEIGHT 260.0f
#define SR_RESEND_STEPS 60
#define SR_PAIRS 16
#define SR_PROC_SWIM_UP 0x35       /* daPyProc_SWIM_UP_e .. SWIM_MOVE_e (d_a_player_main_03.h, _01.h) */
#define SR_PROC_SWIM_MOVE 0x37

typedef struct sr_pair {
    int used;
    mp_u32 passenger, owner;
} sr_pair;

static mp_u32 sr_owner = MP_NO_ID; /* the player whose ship the local Link is on */
static int sr_caught;
static unsigned long long sr_sent_step;
static sr_pair sr_pairs[SR_PAIRS];
static int sr_prompt;              /* a ship in reach: the prompt shows */
static f32 sr_prompt_at[3];
static char sr_prompt_name[MP_NAME_MAX + 1];
static f32 sr_mtx[12] __attribute__((aligned(16)));

/* ---------------------------------------------------------------- the ships */

static ship_puppet_slot* sr_ship_slot(mp_u32 owner) {
    for (int i = 0; i != SHIP_PUPPET_MAX; ++i)
        if (ship_slots[i].used && ship_slots[i].uid == owner && ship_slots[i].actor && ship_slots[i].has_target)
            return &ship_slots[i];
    return 0;
}

static int sr_riding(const ship_puppet_slot* s) { return s && (s->target.flags & MP_SHIP_RIDING); }

/* The base matrix of the owner's ship as it is this step, and its heading; 0 when there is none
 * (or its owner does not ride it). The owner may be the local player (its own ship). */
static int sr_ship_matrix(mp_u32 owner, f32* m, s16* heading) {
    if (owner == local_uid && owner != MP_NO_ID) {
        u32 ship = ship_local();
        const u8* play = wwhd_play_get();
        if (!ship || !play || !(shipp_ld32((u32)play + SHIP_PLAY_STATUS0) & SHIP_STATUS0_RIDE)) return 0;
        const fopAc_ac_c* a = (const fopAc_ac_c*)ship;
        wwhd_mDoMtx_ZXYrotS((u32)sr_mtx, a->shape_angle.x, a->shape_angle.y, a->shape_angle.z);
        sr_mtx[3] = a->current.pos.x;
        sr_mtx[7] = a->current.pos.y;
        sr_mtx[11] = a->current.pos.z;
        memcpy(m, sr_mtx, sizeof(sr_mtx));
        *heading = a->shape_angle.y;
        return 1;
    }
    ship_puppet_slot* s = sr_ship_slot(owner);
    if (!sr_riding(s)) return 0;
    ship_slot_sync(s);
    memcpy(m, s->mtx, sizeof(s->mtx));
    *heading = s->display.rot[1];
    return 1;
}

static void sr_seat(const f32* m, f32* out) {
    for (int r = 0; r != 3; ++r)
        out[r] = m[r * 4] * SR_SEAT_X + m[r * 4 + 1] * SR_SEAT_Y + m[r * 4 + 2] * SR_SEAT_Z + m[r * 4 + 3];
}

/* ---------------------------------------------------------------- the others */

static void sr_send(const char* name) {
    const u8* play = wwhd_play_get();
    const fopAc_ac_c* link = (const fopAc_ac_c*)dh_link;
    if (!play || !link || state != READY || local_uid == MP_NO_ID || session.client_player_event == MP_NO_ID) return;
    mp_event e;
    e.sender = local_uid;
    map_stage(play, e.stage);
    e.room = link->current.roomNo;
    e.has_place = 0;
    int n = 0;
    for (; name[n] && n != MP_EVENT_NAME_MAX; ++n) e.name[n] = name[n];
    e.name[n] = 0;
    mp_writer body = mp_writer_make(payload, sizeof(payload));
    mp_write_event(&body, &e);
    send_payload(session.client_player_event, &body);
}

static void sr_send_board(void) {
    static const char hex[] = "0123456789ABCDEF";
    char name[16] = "#SB_";
    for (int i = 0; i != 8; ++i) name[4 + i] = hex[(sr_owner >> (28 - 4 * i)) & 0xFu];
    name[12] = 0;
    sr_send(name);
    sr_sent_step = wwhd_logic_step();
}

static void sr_pair_set(mp_u32 passenger, mp_u32 owner) {
    sr_pair* free_pair = 0;
    for (int i = 0; i != SR_PAIRS; ++i) {
        if (sr_pairs[i].used && sr_pairs[i].passenger == passenger) {
            sr_pairs[i].owner = owner;
            return;
        }
        if (!sr_pairs[i].used && !free_pair) free_pair = &sr_pairs[i];
    }
    if (free_pair) {
        free_pair->used = 1;
        free_pair->passenger = passenger;
        free_pair->owner = owner;
    }
}

static void sr_pair_drop(mp_u32 passenger) {
    for (int i = 0; i != SR_PAIRS; ++i)
        if (sr_pairs[i].used && sr_pairs[i].passenger == passenger) sr_pairs[i].used = 0;
}

/* item_share.h share_received: "#SB_xxxxxxxx" (on board that player's ship) or "#SL" (got off). */
static void sr_received(const mp_event* e) {
    const char* n = e->name;
    if (n[2] == 'L' && !n[3]) {
        sr_pair_drop(e->sender);
        return;
    }
    if (n[2] != 'B' || n[3] != '_') return;
    mp_u32 owner = 0;
    for (int i = 0; i != 8; ++i) {
        int v = share_hex(n[4 + i]);
        if (v < 0) return;
        owner = owner << 4 | (mp_u32)v;
    }
    int known = 0;
    for (int i = 0; i != SR_PAIRS; ++i) known |= sr_pairs[i].used && sr_pairs[i].passenger == e->sender;
    sr_pair_set(e->sender, owner);
    if (!known) wwhd_log_int("multiplayer: boat: another player boarded the ship of uid", (int)owner);
}

/* puppet.h puppet_execute: a passenger's puppet on the ship it is on (1), else 0. */
static int sr_place_puppet(mp_u32 uid, fopAc_ac_c* actor) {
    for (int i = 0; i != SR_PAIRS; ++i) {
        if (!sr_pairs[i].used || sr_pairs[i].passenger != uid) continue;
        f32 m[12], seat[3];
        s16 heading;
        if (!sr_ship_matrix(sr_pairs[i].owner, m, &heading)) return 0; /* its ship is not here (yet) */
        sr_seat(m, seat);
        actor->old.pos = actor->current.pos;
        actor->current.pos.x = seat[0];
        actor->current.pos.y = seat[1];
        actor->current.pos.z = seat[2];
        actor->shape_angle.y = actor->current.angle.y = (s16)(heading + SR_TURN);
        return 1;
    }
    return 0;
}

/* ---------------------------------------------------------------- the local Link */

WWHD_GAME_ORIGINAL(0x02425FD8, s32, sr_ship_sit_original, (void* self));
WWHD_REPLACE(0x02425FD8, s32, sr_ship_sit, (void* self)) {
    if (sr_owner != MP_NO_ID && self && (u32)self == dh_link && !puppet_is(self)) return 1; /* seated by the mod */
    return sr_ship_sit_original(self);
}

/* Seated as dProcShipSit_init seats Link. */
/* 0.3.96: the camera stayed on the ship, not on the passenger (0.3.94-0.3.95), with no boat status
 * set (log: status0 and status1 0 at boarding, never cleared): not the boat camera. The one change
 * left from a normal Link was the cutscene proc DEMO_SHIP_SIT 0xD7. Now the passenger stays in the
 * normal waiting proc (procWait_init), and its animation calls are refused while seated
 * (anim_sync.h sr_anim_locked), the sitting one kept: procWait's own setMoveAnime every step would
 * stand it up. Deduced, to test: the camera follows the passenger in procWait. */
static int sr_seated;
static int sr_diag;                       /* 0.3.98: steps still logged after boarding */
static unsigned long long sr_board_step;  /* 0.3.98: "B: jump off" shown 2 s from it */
#define SR_HINT_STEPS 60
static void sr_sit(void* self) {
    sr_seated = 1; /* 0.3.98: procWait_init's own animation calls refused too */
    wwhd_daPy_lk_c__procWait_init(self);
    anim_single_hook(self, SR_ANM_KOSHIKAKE, 1.0f, 0.0f, -1, 5.0f); /* recorded: the puppets sit too */
    ((daPy_lk_c*)self)->mNormalSpeed = 0.0f;
}

/* anim_sync.h: an animation call refused on the seated passenger (the sitting one passes). */
static int sr_anim_locked(const void* self, int anm) {
    return sr_seated && sr_owner != MP_NO_ID && self && (u32)self == dh_link && anm != SR_ANM_KOSHIKAKE;
}

/* 0.3.97: seen in game with 0.3.96, the camera right but Link standing. procWait (d_a_player_main_04b.cpp)
 * calls daPy_lk_c::setBlendMoveAnime (023E14A0, d_a_player_main_02.cpp; code generated: this and one
 * float, nothing returned) every step: it blends the wait and walk animations itself (frame controls
 * and ratios of the under slots), not only through setMoveAnime. Skipped while seated. */
WWHD_GAME_ORIGINAL(0x023E14A0, void, sr_blend_move_original, (void* self, f32 blend));
WWHD_REPLACE(0x023E14A0, void, sr_blend_move, (void* self, f32 blend)) {
    if (sr_anim_locked(self, -1)) return;
    sr_blend_move_original(self, blend);
}

static void sr_free(void) {
    if (sr_caught && dh_link) ((daPy_lk_c*)dh_link)->mModeFlg &= ~CY_MODE_CAUGHT;
    sr_seated = 0; /* its animations free again (B jumps right after) */
    if (sr_owner != MP_NO_ID && dh_link) wwhd_daPy_lk_c__procWait_init((void*)dh_link);
    sr_caught = 0;
    if (sr_owner != MP_NO_ID) {
        sr_send("#SL");
        wwhd_log("multiplayer: boat: got off the other player's ship");
    }
    sr_owner = MP_NO_ID;
}

static int sr_can_ride(const fopAc_ac_c* link) {
    const u8* play = wwhd_play_get();
    return state == READY && local_uid != MP_NO_ID && cy_can_act(link) && cy_carried_by == MP_NO_ID &&
           cy_carrying == MP_NO_ID && play && !(shipp_ld32((u32)play + SHIP_PLAY_STATUS0) & SHIP_STATUS0_RIDE);
}

/* The ship (its owner) the local Link may board now, MP_NO_ID when none. */
static mp_u32 sr_ship_in_reach(const daPy_lk_c* link) {
    const fopAc_ac_c* a = (const fopAc_ac_c*)link;
    s32 proc = link->mCurProc;
    if (proc != CY_PROC_WAIT && proc != CY_PROC_MOVE && proc != CY_PROC_ATN_MOVE && proc != CY_PROC_WAIT_TURN &&
        !(proc >= SR_PROC_SWIM_UP && proc <= SR_PROC_SWIM_MOVE))
        return MP_NO_ID;
    mp_u32 best = MP_NO_ID;
    f32 best_d2 = SR_REACH * SR_REACH;
    for (int i = 0; i != SHIP_PUPPET_MAX; ++i) {
        const ship_puppet_slot* s = &ship_slots[i];
        if (!s->used || !s->actor || !s->has_target || !sr_riding(s) || !find_remote(s->uid, 0)) continue;
        const fopAc_ac_c* ship = (const fopAc_ac_c*)s->actor;
        f32 dx = ship->current.pos.x - a->current.pos.x, dz = ship->current.pos.z - a->current.pos.z;
        f32 dy = ship->current.pos.y - a->current.pos.y;
        f32 d2 = dx * dx + dz * dz;
        if (d2 >= best_d2 || dy > SR_REACH_HEIGHT || dy < -SR_REACH_HEIGHT) continue;
        best = s->uid;
        best_d2 = d2;
        sr_prompt_at[0] = ship->current.pos.x;
        sr_prompt_at[1] = ship->current.pos.y;
        sr_prompt_at[2] = ship->current.pos.z;
    }
    return best;
}

/* The place on the ship, written before the Link's execute (0: no ship now). */
static int sr_hold(void* self) {
    fopAc_ac_c* a = (fopAc_ac_c*)self;
    daPy_lk_c* link = (daPy_lk_c*)self;
    f32 m[12], seat[3];
    s16 heading;
    if (!sr_ship_matrix(sr_owner, m, &heading)) return 0;
    heading = (s16)(heading + SR_TURN);
    sr_seat(m, seat);
    a->current.pos.x = a->old.pos.x = seat[0];
    a->current.pos.y = a->old.pos.y = seat[1];
    a->current.pos.z = a->old.pos.z = seat[2];
    a->speed.x = a->speed.y = a->speed.z = 0.0f;
    a->speedF = 0.0f;
    link->mNormalSpeed = 0.0f;
    a->shape_angle.y = a->current.angle.y = heading;
    volatile f32* keep = &WWHD_GAME_DATA(CY_KEEP_POS, f32);
    keep[0] = seat[0];
    keep[1] = seat[1];
    keep[2] = seat[2];
    (&WWHD_GAME_DATA(CY_KEEP_SHAPE_ANGLE, s16))[1] = heading;
    (&WWHD_GAME_DATA(CY_KEEP_CURRENT_ANGLE, s16))[1] = heading;
    link->mModeFlg |= CY_MODE_CAUGHT;
    sr_caught = 1;
    return 1;
}

/* The boat camera's statuses of player 0 (see SR_STATUS0_BOAT), cleared; logged when log is set. */
static void sr_camera_statuses(int log) {
    u8* play = wwhd_play_get();
    if (!play) return;
    volatile u32* s0 = (volatile u32*)(play + SHIP_PLAY_STATUS0);
    volatile u32* s1 = (volatile u32*)(play + SHIP_PLAY_STATUS1);
    if (log) {
        wwhd_log_hex("multiplayer: boat: player status0 at boarding", *s0);
        wwhd_log_hex("multiplayer: boat: player status1 at boarding", *s1);
    }
    static int logged;
    if (((*s0 & SR_STATUS0_BOAT) || (*s1 & SR_STATUS1_BOAT)) && logged++ < 4) {
        wwhd_log_hex("multiplayer: boat: boat camera statuses cleared, status0", *s0);
        wwhd_log_hex("multiplayer: boat:   status1", *s1);
    }
    *s0 &= ~SR_STATUS0_BOAT;
    *s1 &= ~SR_STATUS1_BOAT;
}

/* death.h dh_after_stick, before carry.h: A boards a ship in reach; B jumps off it at any time (0.3.94: A before). */
static int sr_after_stick(void* self) {
    daPy_lk_c* link = (daPy_lk_c*)self;
    fopAc_ac_c* a = (fopAc_ac_c*)self;
    volatile u8* trigger = (volatile u8*)self + DH_LINK_ITEM_TRIGGER;
    if (sr_owner != MP_NO_ID) {
        *trigger = (u8)(*trigger & ~CY_TRIGGER_A); /* seated: A does nothing */
        if (!(*trigger & SR_TRIGGER_B)) return 1;
        *trigger = (u8)(*trigger & ~SR_TRIGGER_B); /* not the sword */
        sr_free();
        a->speedF = CY_AUTO_JUMP_SPEED;
        wwhd_daPy_lk_c__procAutoJump_init(self);
        a->speed.y = CY_JUMP_SPEED_Y;
        link->mNormalSpeed = CY_JUMP_SPEED_F;
        return 1;
    }
    sr_prompt = 0;
    if (!sr_can_ride(a)) return 0;
    mp_u32 owner = sr_ship_in_reach(link);
    if (owner == MP_NO_ID) return 0;
    sr_prompt = 1;
    remote_player* r = find_remote(owner, 0);
    ck_copy(sr_prompt_name, r && r->has_look ? r->look.name : "", sizeof(sr_prompt_name));
    if (!(*trigger & CY_TRIGGER_A)) return 0;
    *trigger = (u8)(*trigger & ~CY_TRIGGER_A);
    sr_owner = owner;
    if (!sr_hold(self)) {
        sr_owner = MP_NO_ID;
        return 1;
    }
    sr_prompt = 0;
    sr_sit(self);
    sr_diag = 8;
    sr_board_step = wwhd_logic_step();
    sr_camera_statuses(1);
    sr_send_board();
    wwhd_log_int("multiplayer: boat: boarded the ship of uid", (int)owner);
    return 1;
}

/* mod.c, before the local Link's execute: held on the ship. */
static void sr_before(void* self) {
    if (!self || puppet_is(self) || (u32)self != dh_link || sr_owner == MP_NO_ID) return;
    if (!sr_can_ride((const fopAc_ac_c*)self) || !sr_hold(self)) {
        wwhd_log("multiplayer: boat: the ship's owner left it (or an event, a death): off the ship");
        sr_free();
        return;
    }
    /* 0.3.98: back to procWait with its animation calls refused (0.3.96-0.3.97 ran procWait_init
     * unlocked: if the game left procWait each step, Link stood up each step), then seated again */
    if (((const daPy_lk_c*)self)->mCurProc != CY_PROC_WAIT) {
        if (sr_diag > 0) wwhd_log_int("multiplayer: boat: the game took Link to proc", (int)((const daPy_lk_c*)self)->mCurProc);
        sr_sit(self);
    }
    sr_camera_statuses(0);
}

/* mod.c, after the local Link's execute: the others told again now and then (a player who came). */
static void sr_after(void* self) {
    if (!self || puppet_is(self) || (u32)self != dh_link || sr_owner == MP_NO_ID) return;
    if (sr_diag > 0) { /* 0.3.98: why Link does not stay seated */
        --sr_diag;
        const daPy_lk_c* l = (const daPy_lk_c*)self;
        wwhd_log_int("multiplayer: boat: seated, proc after execute", (int)l->mCurProc);
        wwhd_log_int("multiplayer: boat:   under animation rate x100", (int)(l->mFrameCtrlUnder[0].mRate * 100.0f));
        wwhd_log_int("multiplayer: boat:   under animation frame", (int)l->mFrameCtrlUnder[0].mFrame);
        wwhd_log_int("multiplayer: boat:   under animation end", (int)l->mFrameCtrlUnder[0].mEnd);
        wwhd_log_hex("multiplayer: boat:   under animation resource", *(const volatile u32*)(l->mAnmRatioUnder + 4));
    }
    sr_camera_statuses(0); /* also after Link's own execute, before the camera's */
    if (wwhd_logic_step() - sr_sent_step >= SR_RESEND_STEPS) sr_send_board();
}

/* mod.c clear_area_state: another area, everything forgotten. */
static void sr_reset(void) {
    if (sr_caught && dh_link) ((daPy_lk_c*)dh_link)->mModeFlg &= ~CY_MODE_CAUGHT;
    sr_caught = 0;
    sr_owner = MP_NO_ID;
    sr_seated = 0;
    sr_prompt = 0;
    for (int i = 0; i != SR_PAIRS; ++i) sr_pairs[i].used = 0;
}

/* mod.c draw_markers: the prompt over the ship in reach. */
static void sr_draw(u32 list) {
    const u8* play = wwhd_play_get();
    if (!play || play[WWHD_PLAY_EVENT_RUNNING_OFFSET]) return;
    if (sr_owner != MP_NO_ID && dh_link) { /* 0.3.94: seated, how to get off (0.3.98: for 2 s) */
        if (wwhd_logic_step() - sr_board_step >= SR_HINT_STEPS) return;
        const fopAc_ac_c* a = (const fopAc_ac_c*)dh_link;
        f32 at[3] = {a->current.pos.x, a->current.pos.y, a->current.pos.z};
        f32 hx, hy;
        if (dh_project(at, 170.0f, &hx, &hy)) name_draw(list, hx, hy, "B: jump off", 11, 0xF9C74Fu, 0.8f, 0.85f);
        return;
    }
    if (!sr_prompt) return;
    f32 x, y;
    if (!dh_project(sr_prompt_at, 260.0f, &x, &y)) return;
    static char text[MP_NAME_MAX + 24];
    const char* head = "A: board ";
    u32 n = 0;
    for (; head[n]; ++n) text[n] = head[n];
    if (sr_prompt_name[0]) {
        for (u32 i = 0; sr_prompt_name[i] && n < sizeof(text) - 8; ++i) text[n++] = sr_prompt_name[i];
        const char* tail = "'s boat";
        for (u32 i = 0; tail[i]; ++i) text[n++] = tail[i];
    } else {
        const char* tail = "the boat";
        for (u32 i = 0; tail[i]; ++i) text[n++] = tail[i];
    }
    name_draw(list, x, y, text, n, 0xF9C74Fu, 0.9f, 1.0f);
}
