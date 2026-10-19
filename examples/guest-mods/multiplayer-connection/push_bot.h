/* The launcher's Debug page "push bot": a crate in front of the local Link and a bot at it, to test
 * heavy.h (a block moves only with enough players pushing) alone, connected or not.
 *
 * debug.cfg "bot=1": a bot that pushes (counted as a room player and as a helper: 2/2, the crate
 * moves when the local Link pushes it too); "bot=2": a bot that stands by the crate without pushing
 * (a room player only: 1/2, the crate does not move); "bot=0": the bot and its crate are removed.
 * A new request removes the previous bot and crate first.
 *
 * The crate (decomp/wwhd_src d_a_obj_movebox.cpp / .h, verified): daObjMovebox, process name 0x2B
 * (AS_PROC_MOVEBOX, actor_sync.h), created with fopAcM_create (025D5834, as actor_sync.h and
 * puppet.h do). Parameters: type bits 24-27 (PRM_TYPE_S 0x18, W 4) = 0 (M_arcname[0], the wooden
 * crate "Kkiba" of the server's list); switch bits 8-15 = 0xFF (none: chk_appear true, path_init
 * returns); item bits 0-5 = 0x3F and item switch bits 16-22 = 0x7F (make_item, on a break: no item
 * deduced, as the game's own unused values); bit 30 = 0 (mPrmZ from angle.z, given 0xFFFF: no path);
 * bit 31 = 1 (no buoy child 0x1D1). Deduced, not verified in HD: the creation loads the archive
 * itself (the GameCube Mthd_Create's dComIfG_resLoad; the HD create is not in the decomp), so a
 * crate is made anywhere; the cell size (attribute +0x0C, M_attr 101CB424) places it.
 *
 * The bot: a puppet (puppet.h, needs the "puppet" option; without it only its label shows) in its
 * own slot PB_UID, kept by puppet_clear_all, standing at the crate's near side and turned with it,
 * playing ANM_WALKPUSH (0x7F, setSingleMoveAnime(ANM_WALKPUSH, 1, 0, -1, 5) as procPushMove,
 * d_a_player_main_07.cpp) when it pushes. It follows the crate when it moves. heavy.h asks
 * pb_room_player and pb_pushing_near. */
#pragma once
#include "wwhd_guest.h"

#define PB_UID PUPPET_BOT_UID       /* the bot's puppet slot (puppet.h) */
#define PB_PARAMS 0x807FFF3Fu       /* type 0, no switch, no item, no buoy (above) */
#define PB_ANM_WALKPUSH 0x7F
#define PB_GAP 150.0f               /* from the local Link to the crate's near side */
#define PB_BOT_BACK 40.0f           /* the bot's feet behind the crate's near side */
#define PB_BOT_SIDE 0.3f            /* the bot's offset to the side, in crates */
#define PB_CREATE_STEPS 600         /* a crate not made after this many steps is given up */
#define PB_LABEL_LIFT 160.0f
#define PB_NO_PROC 0xFFFFFFFFu

enum { PB_OFF, PB_PUSH, PB_IDLE };

static int pb_mode;                 /* PB_OFF, PB_PUSH or PB_IDLE */
static int pb_wanted = -1;          /* a request of debug.cfg, done at the next step of Link */
static u32 pb_box_proc = PB_NO_PROC;
static u32 pb_box;                  /* the crate once made */
static u32 pb_box_out;
static int pb_box_wait;
static f32 pb_half = 75.0f;         /* half the crate's cell */
static s16 pb_angle;                /* the push direction (the local Link's facing at the request) */
static mp_transform pb_bot;         /* where the bot stands */
static u32 pb_posed;                /* the puppet actor given the push animation */
static cXyz pb_box_pos;
static csXyz pb_box_angle;
static mp_anim_events pb_pose;
static int pb_no_puppet_logged;

static void pb_request(u32 mode) { pb_wanted = mode == 1 ? PB_PUSH : mode == 2 ? PB_IDLE : PB_OFF; }

static void pb_remove(void) {
    if (pb_box_proc != PB_NO_PROC) wwhd_fopAcM_delete_id(pb_box_proc); /* safe for an id that is gone */
    puppet_remove(PB_UID);
    pb_box_proc = PB_NO_PROC;
    pb_box = 0;
    pb_posed = 0;
    pb_mode = PB_OFF;
}

static void pb_summon(const fopAc_ac_c* link, int mode) {
    pb_remove();
    pb_angle = link->shape_angle.y;
    u32 type = 0;
    f32 cell = *(const volatile f32*)(AS_MOVEBOX_ATTR + type * AS_MOVEBOX_ATTR_SIZE + AS_MOVEBOX_CELL_SIZE);
    pb_half = cell > 1.0f && cell < 2000.0f ? cell * 0.5f : 75.0f;
    f32 sn = as_sin(pb_angle), cs = as_sin((s16)(pb_angle + 0x4000));
    f32 d = PB_GAP + pb_half;
    pb_box_pos.x = link->current.pos.x + sn * d;
    pb_box_pos.y = link->current.pos.y;
    pb_box_pos.z = link->current.pos.z + cs * d;
    pb_box_angle.x = 0;
    pb_box_angle.y = pb_angle;
    pb_box_angle.z = -1; /* mPrmZ 0xFFFF: no path */
    pb_box_proc = wwhd_fopAcM_create(AS_PROC_MOVEBOX, PB_PARAMS, (u32)&pb_box_pos, link->current.roomNo,
                                     (u32)&pb_box_angle, 0, 0xFFFFFFFFu, 0);
    if (pb_box_proc == PB_NO_PROC) {
        wwhd_log("multiplayer: debug: push bot: the crate was refused");
        return;
    }
    pb_box_wait = 0;
    pb_mode = mode;
    wwhd_log_int(mode == PB_PUSH ? "multiplayer: debug: push bot summoned (pushing), crate cell"
                                 : "multiplayer: debug: push bot summoned (standing by), crate cell",
                 (int)(pb_half * 2.0f));
}

/* The bot at the crate's near side, turned the push direction. */
static void pb_place_bot(void) {
    const fopAc_ac_c* box = (const fopAc_ac_c*)pb_box;
    f32 sn = as_sin(pb_angle), cs = as_sin((s16)(pb_angle + 0x4000));
    f32 back = pb_half + PB_BOT_BACK, side = pb_half * 2.0f * PB_BOT_SIDE;
    pb_bot.pos[0] = box->current.pos.x - sn * back + cs * side;
    pb_bot.pos[1] = box->current.pos.y;
    pb_bot.pos[2] = box->current.pos.z - cs * back - sn * side;
    pb_bot.rot[0] = pb_bot.rot[2] = 0;
    pb_bot.rot[1] = pb_angle;
}

static void pb_animate(void) {
    puppet_slot* s = puppet_slot_for(PB_UID, 0);
    if (!s || !s->actor || pb_mode != PB_PUSH) return;
    const daPy_lk_c* p = (const daPy_lk_c*)s->actor;
    if (pb_posed == s->actor && p->mFrameCtrlUnder[0].mRate >= 0.01f) return; /* still playing */
    mp_anim_events_clear(&pb_pose);
    pb_pose.single_count = 1;
    pb_pose.single[0].anm = PB_ANM_WALKPUSH;
    pb_pose.single[0].rate = 1.0f;
    pb_pose.single[0].start = 0.0f;
    pb_pose.single[0].end = -1;
    pb_pose.single[0].morf = 5.0f;
    puppet_queue_events(PB_UID, &pb_pose);
    pb_posed = s->actor;
}

/* Every step of the local Link (after its execute). */
static void pb_step(void* link_actor) {
    const fopAc_ac_c* link = (const fopAc_ac_c*)link_actor;
    if (pb_wanted >= 0) {
        int mode = pb_wanted;
        pb_wanted = -1;
        if (mode == PB_OFF) {
            if (pb_mode != PB_OFF) wwhd_log("multiplayer: debug: push bot removed");
            pb_remove();
        } else {
            pb_summon(link, mode);
        }
    }
    if (pb_mode == PB_OFF) return;
    pb_box_out = 0;
    s32 found = wwhd_fopAcM_SearchByID(pb_box_proc, (u32)&pb_box_out);
    if (!found) { /* deleted by the game (area left, crate broken) */
        wwhd_log("multiplayer: debug: push bot: its crate is gone, bot removed");
        pb_box_proc = PB_NO_PROC;
        pb_remove();
        return;
    }
    if (!pb_box_out) { /* still being made */
        pb_box = 0;
        if (++pb_box_wait == PB_CREATE_STEPS) {
            wwhd_log("multiplayer: debug: push bot: the crate was not made, bot removed");
            pb_remove();
        }
        return;
    }
    pb_box = pb_box_out;
    pb_place_bot();
    if (!puppet_enabled || puppet_disabled_by_error) {
        if (!pb_no_puppet_logged++) wwhd_log("multiplayer: debug: push bot: puppets are off, only its label shows");
        puppet_remove(PB_UID);
        return;
    }
    puppet_set_target(PB_UID, &pb_bot);
    if (state != READY) puppet_step(link); /* connected, mod.c steps the puppets */
    pb_animate();
}

/* heavy.h: the bot is a player of the local Link's room. */
static int pb_room_player(void) { return pb_mode != PB_OFF && pb_box; }

/* heavy.h: the bot pushes within `radius` of a place. */
static int pb_pushing_near_pos(const f32* pos, f32 radius) {
    if (pb_mode != PB_PUSH || !pb_box) return 0;
    f32 dx = pb_bot.pos[0] - pos[0], dy = pb_bot.pos[1] - pos[1], dz = pb_bot.pos[2] - pos[2];
    return dx * dx + dy * dy + dz * dz <= radius * radius;
}

/* heavy.h: the bot pushes within `radius` of the local Link. */
static int pb_pushing_near(const fopAc_ac_c* link, f32 radius) {
    f32 pos[3] = {link->current.pos.x, link->current.pos.y, link->current.pos.z};
    return pb_pushing_near_pos(pos, radius);
}

static void pb_draw(u32 list) {
    const u8* play = wwhd_play_get();
    if (!pb_room_player() || !play || play[WWHD_PLAY_EVENT_RUNNING_OFFSET]) return;
    f32 x, y;
    if (!dh_project(pb_bot.pos, PB_LABEL_LIFT, &x, &y)) return;
    static const char push[] = "Bot (pushing)", idle[] = "Bot (not pushing)";
    if (pb_mode == PB_PUSH) name_draw(list, x, y, push, sizeof(push) - 1, 0x9AD0FFu, 0.5f, 1.0f);
    else name_draw(list, x, y, idle, sizeof(idle) - 1, 0xC0C0C0u, 0.5f, 1.0f);
}
