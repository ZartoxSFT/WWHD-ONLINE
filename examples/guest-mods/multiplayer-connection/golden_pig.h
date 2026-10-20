/* The shop's Golden Pig (0.3.3): bought in the launcher's shop (one at a time, bonus.h bonus_pigs),
 * called with the D-pad's down. It appears in Link's hands as a bomb he takes out would; thrown, it
 * charges the enemies very fast for GP_SECONDS, then goes. Local to this game (not synchronised: the
 * other players do not see it; the enemies it hurts follow the usual health synchronisation).
 *
 * The game (decomp, verified):
 *  - the pig is kb_class (d_a_kb.cpp, process 0xDC fpcNm_KB_e, d_a_player_main_01.h): parameters
 *    shape bits 0-3, m401 bits 4-7, mbCanBeBigPig bits 8-15 (0xFF: never the big pig of Outset), path
 *    bits 16-23 (0xFF: none). Its mode m41E (+0x766, WWHD_OFFSET): 0 free, 1 carried, 2 swimming,
 *    3 attacking (attack_move, d_a_kb_move.cpp), with its step m420 (+0x768): 0x23 then 0x24 runs at
 *    the player (fopAcM_searchPlayerAngleY, dComIfGp_getPlayer(0)) with its attack sphere on, a hit on
 *    the player bounces it back (0x22) and it runs again. Its sphere mSph (+0xBD4, between mStts
 *    +0xB98 and m968 +0xD00, WWHD_OFFSETs) has the source co_sph_src (101B7FFC, read in the EU
 *    cking.rpx: AT type 0x800, damage 4, AtSPrm 0xD = Set | VsPlayer | VsOther); an enemy's hit
 *    collider takes it (the Bokoblin's TG type 0xFF1DFEFF has 0x800, TgSPrm 3) once VsEnemy (2) is on
 *    (cCcD_AtSPrm_VsEnemy_e 2, VsPlayer 4: d_a_ki.cpp, d_a_bk_move.cpp). Its health falling to 0 makes
 *    every pig angry (pl_attack_hit_check, l_kbStatics.ALL_ANGER): it is kept at 10.
 *  - daPy_lk_c::procGrabReady_init (023ED0F0, d_a_player_main_02.cpp; heavy.h hv_grab_ready) takes
 *    mpAttnActorA, sets it carried and keeps it: Link lifts it as any carried thing, A throws it.
 *  - Link's procs where his hands are free: WAIT 4, FREE_WAIT 5, MOVE 6, ATN_MOVE 7.
 * The mod: while the golden pig attacks, its attack_move (021947AC, replaced; it reads player 0 for
 * its place and to tell its hit only) runs with the nearest living enemy as player 0 and link player
 * (play + 0x5B2C, + 0x5B34, as actor_sync.h does for a weapon copy), so the game's own charge goes at
 * it. Not the whole execute (0.3.3): speed_pos_set (02194654) asks player 0's daPy_getGrabMissActor,
 * a virtual of Link: on an enemy, a branch to its data (crash-20261019-210919.log, 0x42900000). The
 * execute (daKb_Execute 02190CE0, replaced) keeps its health, its sphere on enemies and off players,
 * turns it to the charge once loose and sets the run to GP_SPEED. Gold as the yellow hearts
 * (elite.h el_draw_begin, gp_is_golden, painted first). */
#pragma once
#include "wwhd_guest.h"

#define GP_PROC_KB 0xDC
#define GP_PARAMS 0xFFFFFF00u    /* pink, no big pig, no path */
#define GP_MODE 0x766            /* kb_class::m41E */
#define GP_STEP 0x768            /* kb_class::m420 */
#define GP_SPH 0xBD4             /* kb_class::mSph */
#define GP_MODE_FREE 0
#define GP_MODE_ATTACK 3
#define GP_STEP_TURN 0x23
#define GP_STEP_RUN 0x24
#define GP_AT_VS_ENEMY 2u
#define GP_AT_VS_PLAYER 4u
#define GP_STATUS_CARRY 0x2000u  /* fopAcStts_CARRY_e */
#define GP_SECONDS 30.0
#define GP_SPEED 32.0f           /* the game's charge: 15 */
#define GP_RANGE 3000.0f         /* enemies it goes for, 30 m */
#define GP_HAND_LIFT 60.0f
#define GP_CREATE_STEPS 150
#define GP_BUTTON_DOWN 0x0100u   /* VPAD D-pad down */
#define GP_PROC_WAIT 4
#define GP_PROC_FREE_WAIT 5
#define GP_PROC_MOVE 6
#define GP_PROC_ATN_MOVE 7

enum { GP_NONE, GP_MAKING, GP_HELD, GP_LOOSE };

static int gp_state;
static u32 gp_proc = 0xFFFFFFFFu;
static u32 gp_actor;
static u32 gp_out;
static int gp_wait;
static double gp_clock;
static u32 gp_buttons_before;
static cXyz gp_pos;
static csXyz gp_angle;

/* elite.h: the golden pig is drawn gold. */
static int gp_is_golden(const void* actor) { return gp_actor && (u32)actor == gp_actor; }

static void gp_end(const char* why) {
    if (gp_proc != 0xFFFFFFFFu) wwhd_fopAcM_delete_id(gp_proc);
    gp_proc = 0xFFFFFFFFu;
    gp_actor = 0;
    gp_state = GP_NONE;
    wwhd_log(why);
}

/* The nearest living enemy within GP_RANGE (the stage's enemies and their copies, actor_sync.h). */
static u32 gp_target(const fopAc_ac_c* pig) {
    u32 best = 0;
    f32 best_d = GP_RANGE * GP_RANGE;
    for (u32 pass = 0; pass != 2; ++pass) {
        u32 n = pass ? AS_MAX_ENEMY_COPIES : AS_MAX_GROUPS;
        for (u32 i = 0; i != n; ++i) {
            u32 actor = pass ? as_enemy_copies[i].actor : as_groups[i].actor;
            u32 pcid = pass ? as_enemy_copies[i].pcid : as_groups[i].pcid;
            u8 used = pass ? as_enemy_copies[i].used : as_groups[i].used;
            if (!used || !actor || *(const volatile u32*)(actor + AS_PC_ID) != pcid) continue;
            const fopAc_ac_c* a = (const fopAc_ac_c*)actor;
            if (a->health <= 0) continue;
            f32 dx = a->current.pos.x - pig->current.pos.x, dy = a->current.pos.y - pig->current.pos.y;
            f32 dz = a->current.pos.z - pig->current.pos.z;
            f32 d = dx * dx + dy * dy + dz * dz;
            if (d < best_d) {
                best_d = d;
                best = actor;
            }
        }
    }
    return best;
}

static u32 gp_charging; /* the enemy the loose golden pig goes for this step, 0 none */

/* daKb_Execute: the golden pig, loose, charges the nearest enemy. */
WWHD_GAME_ORIGINAL(0x02190CE0, s32, gp_kb_execute_original, (void* i_this));
WWHD_REPLACE(0x02190CE0, s32, gp_kb_execute, (void* i_this)) {
    if (!i_this || (u32)i_this != gp_actor) return gp_kb_execute_original(i_this);
    fopAc_ac_c* pig = (fopAc_ac_c*)i_this;
    u8* p = (u8*)i_this;
    pig->health = 10; /* never "killed": every pig would turn on the player */
    volatile u32* at_sprm = (volatile u32*)(p + GP_SPH + AS_OBJ_AT_SPRM);
    *at_sprm = (*at_sprm | GP_AT_VS_ENEMY) & ~GP_AT_VS_PLAYER;
    gp_charging = 0;
    if (gp_state == GP_LOOSE) {
        volatile s16* mode = (volatile s16*)(p + GP_MODE);
        gp_charging = gp_target(pig);
        if (gp_charging && *mode == GP_MODE_FREE) { /* landed: to the charge */
            *mode = GP_MODE_ATTACK;
            *(volatile s16*)(p + GP_STEP) = GP_STEP_TURN;
        }
    }
    s32 r = gp_kb_execute_original(i_this);
    gp_charging = 0;
    *at_sprm = (*at_sprm | GP_AT_VS_ENEMY) & ~GP_AT_VS_PLAYER;
    if (gp_state == GP_LOOSE && *(const volatile s16*)(p + GP_STEP) == GP_STEP_RUN && pig->speedF > 0.0f)
        pig->speedF = GP_SPEED;
    return r;
}

/* attack_move (021947AC, one argument, d_a_kb_move.cpp): the golden pig's charge, at its enemy. */
WWHD_GAME_ORIGINAL(0x021947AC, void, gp_attack_move_original, (void* i_this));
WWHD_REPLACE(0x021947AC, void, gp_attack_move, (void* i_this)) {
    u8* play = wwhd_play_get();
    if (!i_this || (u32)i_this != gp_actor || !gp_charging || !play) {
        gp_attack_move_original(i_this);
        return;
    }
    volatile u32* player0 = (volatile u32*)(play + AS_PLAY_PLAYER0);
    volatile u32* link = (volatile u32*)(play + AS_PLAY_LINK);
    u32 old_player0 = *player0, old_link = *link, target = gp_charging;
    *player0 = target;
    *link = target;
    gp_attack_move_original(i_this);
    if (*player0 == target) *player0 = old_player0;
    if (*link == target) *link = old_link;
}

static int gp_hands_free(const daPy_lk_c* link, const u8* play) {
    if (play[WWHD_PLAY_EVENT_RUNNING_OFFSET]) return 0;
    if (*(const volatile u32*)(play + PUPPET_PLAY_STATUS0) & PUPPET_STATUS0_RIDE) return 0;
    if (((const fopAc_ac_c*)link)->actor_status & GP_STATUS_CARRY) return 0;
    s32 proc = link->mCurProc;
    return proc == GP_PROC_WAIT || proc == GP_PROC_FREE_WAIT || proc == GP_PROC_MOVE || proc == GP_PROC_ATN_MOVE;
}

/* The pig on the HUD's D-pad down arrow while one waits to be called (assets/pig.png, the user's picture since 0.3.7;
 * tools/multiplayer/make_pig_icon.py): opposite the Wind Waker's icon, (8, -39) from the D-pad's
 * centre in layout units, as big (118 x 0.6), with the D-pad's scale and alpha (bonus.h).
 * 0.3.16: seen too big and to the right (its left edge where its centre should be): smaller, and its
 * centre half the former width (70.8 / 2) to the left. */
#define GP_ICON_DX (8.0f - 35.4f)
#define GP_ICON_DY 39.0f   /* down, on the TV */
#define GP_ICON_SIZE 50.0f
static u32 gp_icon;
static unsigned long long gp_icon_epoch = ~0ull;

static void gp_draw(u32 list) {
    if (!bonus_pigs || gp_state != GP_NONE || !bonus_ptr(bonus_cross)) return;
    if (wwhd_logic_step() > bonus_cross_step + 1) return; /* the HUD is not drawn */
    const u8* play = wwhd_play_get();
    if (!play || play[WWHD_PLAY_EVENT_RUNNING_OFFSET]) return;
    unsigned long long epoch = wwhd_hud_epoch();
    if (epoch != gp_icon_epoch) {
        gp_icon_epoch = epoch;
        gp_icon = wwhd_hud_texture(WWHD_HUD_PACKAGE, "assets/pig.png");
        if (!gp_icon) wwhd_log("multiplayer: assets/pig.png not loaded");
    }
    f32 alpha = bonus_alpha(bonus_cross);
    if (!gp_icon || !(alpha > 0.0f)) return;
    f32 x, y, scale;
    bonus_center(bonus_cross, &x, &y, &scale);
    f32 size = GP_ICON_SIZE * scale;
    x += GP_ICON_DX * scale;
    y += GP_ICON_DY * scale;
    bonus_emit(list, gp_icon, x - size * 0.5f, y - size * 0.5f, size, size, 1.0f, 0xFFFFFFu, alpha > 1.0f ? 1.0f : alpha);
}

/* Every step of the local Link (mod.c, after his execute). */
static void gp_step(void* link_actor, double dt) {
    daPy_lk_c* link = (daPy_lk_c*)link_actor;
    const fopAc_ac_c* la = (const fopAc_ac_c*)link_actor;
    u8* play = wwhd_play_get();
    if (!link || !play) return;
    wwhd_input_state input;
    wwhd_input_read(&input);
    int pressed = (input.buttons & GP_BUTTON_DOWN) && !(gp_buttons_before & GP_BUTTON_DOWN);
    gp_buttons_before = input.buttons;

    if (gp_state == GP_NONE) {
        if (!pressed) return;
        bonus_load();
        if (!bonus_pigs) return;
        if (dh_local_dead() || !gp_hands_free(link, play)) return;
        f32 sn = as_sin(la->shape_angle.y), cs = as_sin((s16)(la->shape_angle.y + 0x4000));
        gp_pos.x = la->current.pos.x + sn * 40.0f;
        gp_pos.y = la->current.pos.y + GP_HAND_LIFT;
        gp_pos.z = la->current.pos.z + cs * 40.0f;
        gp_angle.x = 0;
        gp_angle.y = la->shape_angle.y;
        gp_angle.z = 0;
        u8 saved = as_context;
        as_context = AS_CTX_NONE; /* this game's own */
        gp_proc = wwhd_fopAcM_create(GP_PROC_KB, GP_PARAMS, (u32)&gp_pos, la->current.roomNo, (u32)&gp_angle, 0,
                                     0xFFFFFFFFu, 0);
        as_context = saved;
        if (gp_proc == 0xFFFFFFFFu) {
            wwhd_log("multiplayer: golden pig: the pig was refused");
            return;
        }
        gp_state = GP_MAKING;
        gp_wait = 0;
        return;
    }

    gp_out = 0;
    s32 found = wwhd_fopAcM_SearchByID(gp_proc, (u32)&gp_out);
    if (!found) {
        gp_end("multiplayer: golden pig: gone");
        return;
    }
    if (gp_state == GP_MAKING) {
        if (!gp_out) {
            if (++gp_wait == GP_CREATE_STEPS) gp_end("multiplayer: golden pig: not made");
            return;
        }
        fopAc_ac_c* pig = (fopAc_ac_c*)gp_out;
        if (pig->health <= 0) return; /* its create not done yet */
        if (!gp_hands_free(link, play)) {
            gp_end("multiplayer: golden pig: Link's hands are busy");
            return;
        }
        gp_actor = gp_out;
        pig->current.pos.x = gp_pos.x;
        pig->current.pos.y = gp_pos.y;
        pig->current.pos.z = gp_pos.z;
        link->mpAttnActorA = gp_actor;
        hv_grab_ready(link_actor); /* Link takes it up, as a bomb taken out */
        bonus_pigs = 0;            /* called: used up */
        bonus_write();
        gp_state = GP_HELD;
        wwhd_log("multiplayer: golden pig called");
        return;
    }
    fopAc_ac_c* pig = (fopAc_ac_c*)gp_out;
    if (gp_state == GP_HELD) {
        if (pig->actor_status & GP_STATUS_CARRY) return;
        gp_state = GP_LOOSE; /* thrown or put down: its time starts */
        gp_clock = GP_SECONDS;
        wwhd_log("multiplayer: golden pig thrown");
        return;
    }
    gp_clock -= dt;
    if (gp_clock <= 0) gp_end("multiplayer: golden pig: its time is over");
}
