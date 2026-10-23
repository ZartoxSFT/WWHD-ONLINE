/* Death in company: no game over while another player of the stage lives.
 *
 * When the local Link's life is out (daPy_lk_c::changeDeadProc 023F7820, d_a_player_main_03.cpp: no
 * event, life 0, not in a damage reaction; a fairy in a bottle is used first, as the game does) and
 * another player of the same stage (the players of our ActorSync area, mod.c remotes) is alive, Link
 * does not die: its body stays where it fell and Link walks on as a ghost, with no life.
 *  - A ghost does nothing: its buttons are cleared after setStickData reads them (023F32E4: mItemTrigger
 *    +0x690D, mItemButton +0x690E; the stick stays), it registers no collider (setCollision 024076CC
 *    not run: nothing hits it, it pushes and picks up nothing), takes no damage (setDamagePoint,
 *    bonus.h), takes no exit (changeSceneExitId 025C38B8 refused) and, instead of the game's return
 *    after a fall into the void or lava (startRestartRoom 023FD4E4, dProcLavaDamage_init 023FE6B8), it
 *    goes back to its body. Enemies go for a living player rather than a ghost (puppet_sense.h).
 *  - The other players see its body (a puppet in the death animation, ANM_DIELONG 0x62) and the ghost
 *    translucent, as it sees itself (puppet.h ghost_model_drawn). The death block of ActorPlayerSync
 *    (mp_protocol.h MP_DEATH_GHOST) carries the body's place and the dead player's directory id.
 *  - A living player by a body holds A for DH_REVIVE_SECONDS (a prompt and a gauge): CLIENT_PLAYER_REVIVE
 *    goes to the dead player (WWClient.java), whose Link gets DH_REVIVE_LIFE quarter hearts back and
 *    stands on its body again. The life is given the game's way, as the pending life the meter adds
 *    (mItemLifeCount, d_meter_01.cpp), not written into the save: the meter's own count of the hearts
 *    shown (mCurrHP) only follows that path, and the game reads it back as play.mItemNowLife
 *    (+0x5BAC), which turns off the exits (checkFallCode, d_a_player_main_03.cpp) and every event
 *    order but priority 1 (EventControl::order 0253EC0C, d_event.cpp: doors, talks) while it is 0.
 *    (0.2.67-0.2.68 wrote the save's life: the revived player could open no door and take no exit.)
 *  - When no living player is left in the stage (or the connection is lost), the ghost dies as the
 *    game would have let it (changeDeadProc's own course): the game over. A player who dies the
 *    game's way (no other survivor, or a ghost dying) sends MP_DEATH_DOWN while its life is 0 with no
 *    fairy in a bottle: it is no survivor for the ghosts of its stage, which then die too. */
#pragma once
#include "wwhd_guest.h"
#include "wwhd/save.h"
#include "wwhd/objects.h" /* WWHD_ADDR_Link_debug_* */

#define DH_REVIVE_SECONDS 6.0
#define DH_REVIVE_RADIUS 150.0f   /* a body within 1.5 m, horizontally */
#define DH_REVIVE_HEIGHT 120.0f
#define DH_REVIVE_LIFE 12         /* quarter hearts: three hearts */
#define DH_REVIVE_COOLDOWN 3.0
#define DH_LOCAL_BODY (PUPPET_BODY_UID | 0x3FFFFFFFu)
#define DH_ANM_DIELONG 0x62
#define DH_BOTTLE_FAIRY 0x57
#define DH_SAVE_ITEMS (0x20 + 0x3C) /* dSv_player_c::mPlayerItem (d_save.h) */
#define DH_MODE_MIDAIR 0x2u
#define DH_MODE_DAMAGE 0x8u
#define DH_LINK_ITEM_TRIGGER 0x690D
#define DH_LINK_ITEM_BUTTON 0x690E
#define DH_PLAY_LIFE 0x5B44         /* the pending life, f32 quarter hearts (bonus.h) */
#define DH_REVIVE_WAIT_STEPS 150    /* the meter adds the pending life at its next execute */
#define DH_PROC_DEMO_DEAD 0xB2      /* daPyProc_DEMO_DEAD_e (dProcDead_init, d_a_player_main_03.cpp) */
#define DH_DEATH_ANIM_SECONDS 4.0   /* ANM_DIELONG: Link has fallen (its voice at frame 129) */
#define DH_FAIRY_LINK_DOWN 5        /* daNpc_Fa1_c::Type_LINK_DOWN_e (d_a_npc_fa1.h) */
#define DH_REVIVE_CLAMP_SECONDS 3.0 /* the fairy's own 10 hearts are cut to the revive's life */
#define DH_BUTTON_A 0x8000u         /* wwhd_input_state buttons (VPAD) */

static mp_player_look current_look(const daPy_lk_c* link); /* mod.c */
static u32 format_label(char* out, mp_u32 uid);             /* mod.c */

static int dh_ghost, dh_dying;
static int dh_reviving;               /* steps left for the meter to give the revive's life */
static int dh_dying_anim;             /* the death animation plays, then the ghost (or the game over) */
static double dh_dying_time, dh_clamp_time;
static cXyz dh_fairy_pos;             /* passed to the game by address: in mod memory */
static u32 dh_link;
static mp_transform dh_body;
static mp_u32 dh_target = MP_NO_ID;   /* the remote body the local Link stands by */
static double dh_progress, dh_cooldown;
static int dh_prompt;                 /* a body in reach: the prompt shows, A does nothing else */
static int dh_cursed;                 /* 0.3.1: a cursed elite lives in the room: no revive (elite.h) */
static char dh_target_name[MP_NAME_MAX + 1];
static char dh_notice[48];
static u32 dh_notice_bytes;
static int dh_notice_steps;
static mp_anim_events dh_pose;        /* static: the event lists are too large for the stack */

WWHD_GAME_FUNC(0x025B5C80, s32, dh_check_bottle, (u32 items, u32 item));
WWHD_GAME_ORIGINAL(0x023F7820, s32, dh_change_dead_original, (void* self));
WWHD_GAME_ORIGINAL(0x024076CC, void, dh_set_collision_original, (void* self));
WWHD_GAME_ORIGINAL(0x023FD4E4, s32, dh_restart_room_original, (void* self, u32 mode, s32 info, f32 damage, s32 point));
WWHD_GAME_ORIGINAL(0x023FE6B8, s32, dh_lava_original, (void* self));
WWHD_GAME_ORIGINAL(0x025C38B8, s32, dh_exit_original, (u32 ground, f32 speed, s32 type, s32 room));
WWHD_GAME_ORIGINAL(0x02422104, s32, dh_proc_dead_original, (void* self));

static void dh_note_link(void* link) { dh_link = (u32)link; }

static int dh_is_local(void* self) { return self && (u32)self == dh_link && !puppet_is(self); }

static int dh_damage_blocked(u32 self) { return (dh_ghost || dh_reviving || dh_dying_anim) && self == dh_link; }

static void dh_set_notice(const char* a, const char* b) {
    u32 n = 0;
    for (const char* s = a; *s && n != sizeof(dh_notice) - 1; ++s) dh_notice[n++] = *s;
    for (const char* s = b; s && *s && n != sizeof(dh_notice) - 1; ++s) dh_notice[n++] = *s;
    dh_notice_bytes = n;
    dh_notice_steps = 150;
}

/* The players of the stage still alive (one whose state is not known yet counts as alive). */
static int dh_alive_others(void) {
    int n = 0;
    for (int i = 0; i != MAX_REMOTES; ++i)
        if (remotes[i].active &&
            (!remotes[i].has_look || !(remotes[i].look.death & (MP_DEATH_GHOST | MP_DEATH_DOWN))))
            ++n;
    return n;
}

static int dh_shared(void) { return state == READY && local_uid != MP_NO_ID && dh_alive_others() > 0; }

/* daPy_lk_c::execute starts by putting back the position and angles it kept at its end
 * (d_a_player_main_04.cpp: current.pos = l_debug_keep_pos, l_debug_shape_angle,
 * l_debug_current_angle; written back at the end of execute): a position written between two
 * executes (the revive, received after Link's execute) was undone. The kept copies are written too,
 * as setPlayerPosAndAngle does (02443A28, lk_setDebugKeep, d_a_player_main_08.cpp). */
static void dh_place(void* self, const mp_transform* t) {
    fopAc_ac_c* a = (fopAc_ac_c*)self;
    a->current.pos.x = a->old.pos.x = t->pos[0];
    a->current.pos.y = a->old.pos.y = t->pos[1];
    a->current.pos.z = a->old.pos.z = t->pos[2];
    a->speed.x = a->speed.y = a->speed.z = 0.0f;
    a->speedF = 0.0f;
    a->shape_angle.y = a->current.angle.y = t->rot[1];
    volatile f32* keep = &WWHD_GAME_DATA(WWHD_ADDR_Link_debug_position, f32);
    keep[0] = t->pos[0];
    keep[1] = t->pos[1];
    keep[2] = t->pos[2];
    volatile s16* shape = &WWHD_GAME_DATA(WWHD_ADDR_Link_debug_shape_angle, s16);
    volatile s16* angle = &WWHD_GAME_DATA(WWHD_ADDR_Link_debug_current_angle, s16);
    shape[0] = a->shape_angle.x;
    shape[1] = a->shape_angle.y;
    shape[2] = a->shape_angle.z;
    angle[0] = a->current.angle.x;
    angle[1] = a->current.angle.y;
    angle[2] = a->current.angle.z;
    wwhd_daPy_lk_c__procWait_init(self);
}

/* daPy_lk_c::changeDeadProc: the local Link's death becomes a ghost while the stage has a living player. */
WWHD_REPLACE(0x023F7820, s32, dh_change_dead, (void* self)) {
    if (!dh_is_local(self)) return dh_change_dead_original(self);
    if (dh_reviving) return 0; /* revived: the meter gives the life at its next execute */
    if (dh_dying_anim) return 1; /* falling down (dh_step turns it into a ghost) */
    if (dh_ghost) {
        if (!dh_dying) return 0;
        dh_ghost = puppet_local_ghost = 0; /* the game's own death now: the game over */
        return dh_change_dead_original(self);
    }
    const u8* play = wwhd_play_get();
    u32 save = WWHD_GAME_DATA(0x101F84DC, u32);
    const daPy_lk_c* link = (const daPy_lk_c*)self;
    if (dComIfGs_player_status_a.life != 0 || !play || play[WWHD_PLAY_EVENT_RUNNING_OFFSET] ||
        (link->mModeFlg & DH_MODE_DAMAGE) || !dh_shared())
        return dh_change_dead_original(self);
    if (save && dh_check_bottle(save + DH_SAVE_ITEMS, DH_BOTTLE_FAIRY)) return dh_change_dead_original(self);
    if (link->mModeFlg & DH_MODE_MIDAIR) return 0; /* the body lies where Link lands */
    const fopAc_ac_c* a = (const fopAc_ac_c*)self;
    dh_body.pos[0] = a->current.pos.x;
    dh_body.pos[1] = a->current.pos.y;
    dh_body.pos[2] = a->current.pos.z;
    dh_body.rot[0] = dh_body.rot[2] = 0;
    dh_body.rot[1] = a->shape_angle.y;
    /* the game's death animation first, as dProcDead_init does (commonProcInit 0xB2, ANM_DIELONG,
     * d_a_player_main_03.cpp) without its compulsory event: no game over object, music or camera */
    wwhd_daPy_lk_c__commonProcInit(self, DH_PROC_DEMO_DEAD);
    wwhd_daPy_lk_c__setSingleMoveAnime(self, DH_ANM_DIELONG, 1.0f, 0.0f, -1, 2.0f);
    dh_dying_anim = 1;
    dh_dying_time = 0;
    wwhd_log("multiplayer: the local Link died: the death animation, then a ghost");
    return 1;
}

/* daPy_lk_c::dProcDead (02422104, d_a_player_main_05.cpp): during our death animation the game's
 * course (game over object, its fade, the restart) is not run; the animation plays on. */
WWHD_REPLACE(0x02422104, s32, dh_proc_dead, (void* self)) {
    if (dh_dying_anim && dh_is_local(self)) return 1;
    return dh_proc_dead_original(self);
}

/* The death animation is over: a ghost on its body, or the game over when no one lives here now. */
static void dh_after_death_anim(void* link) {
    dh_dying_anim = 0;
    wwhd_daPy_lk_c__procWait_init(link);
    if (!dh_shared()) { /* the next changeDeadProc runs the game's own death */
        wwhd_log("multiplayer: no living player left in the stage: the game's death (game over)");
        return;
    }
    dh_ghost = puppet_local_ghost = 1;
    dh_dying = 0;
    dh_set_notice("You died: a friend can revive you", 0);
    wwhd_log("multiplayer: the local Link is a ghost until a player revives it");
}

static void cy_after_stick(void* self); /* carry.h */
static void ev_after_stick(void* self); /* event_sync.h */
static int sr_after_stick(void* self);  /* ship_ride.h (0.3.92) */

/* daPy_lk_c::setStickData: a ghost's buttons do nothing; by a body, A only revives; else the
 * players carry each other (carry.h). */
WWHD_HOOK_RETURN(0x023F32E4, dh_after_stick, (void* self)) {
    if (!dh_is_local(self)) return;
    volatile u8* trigger = (volatile u8*)self + DH_LINK_ITEM_TRIGGER;
    volatile u8* button = (volatile u8*)self + DH_LINK_ITEM_BUTTON;
    if (dh_ghost) {
        *trigger = 0;
        *button = 0;
    } else if (dh_prompt) {
        *trigger = (u8)(*trigger & ~1u);
    } else if (!sr_after_stick(self)) { /* A boards a friend's ship, or jumps off it (ship_ride.h) */
        cy_after_stick(self);
        ev_after_stick(self);
    }
}

WWHD_REPLACE(0x024076CC, void, dh_set_collision, (void* self)) {
    if (dh_ghost && dh_is_local(self)) return;
    dh_set_collision_original(self);
}

WWHD_REPLACE(0x023FD4E4, s32, dh_restart_room, (void* self, u32 mode, s32 info, f32 damage, s32 point)) {
    if (dh_ghost && !dh_dying && dh_is_local(self)) {
        dh_place(self, &dh_body);
        return 0;
    }
    return dh_restart_room_original(self, mode, info, damage, point);
}

WWHD_REPLACE(0x023FE6B8, s32, dh_lava, (void* self)) {
    if (dh_ghost && !dh_dying && dh_is_local(self)) {
        dh_place(self, &dh_body);
        return 0;
    }
    return dh_lava_original(self);
}

/* 0.3.7: a ghost opens the Start menu. The HD UI's state machine checks, before it opens a menu,
 * player 0's mNoResetFlg1 & 0x200, no event (play + 0x5292), play + 0x5CD8 & 0x10, and the life the
 * meter shows (play.mItemNowLife, play + 0x5BAC) not 0 (disassembled in the EU cking.rpx: functions
 * USA 026A716C and 0270D70C (0.3.7 hooked 4 bytes after their mflr: the runtime refused the mod, "hook target 026A7874 is not a game function"), not named in the decomp; the only UI reads of + 0x5BAC). A ghost's life
 * is 0: no menu. While those two run for a ghost, the shown life is 1, put back when they return
 * (entry and return hooks: their arguments are left alone). Everything else still sees 0 (no exit,
 * no event order: checkFallCode, dEvt_control_c::order). */
#define DH_PLAY_NOW_LIFE 0x5BAC
static int dh_menu_lifted;
static u16 dh_menu_life;
static void dh_menu_enter(void) {
    u8* play = wwhd_play_get();
    if (!dh_ghost || dh_menu_lifted || !play) return;
    volatile u16* life = (volatile u16*)(play + DH_PLAY_NOW_LIFE);
    dh_menu_life = *life;
    if (dh_menu_life) return;
    *life = 1;
    dh_menu_lifted = 1;
}
static void dh_menu_leave(void) {
    u8* play = wwhd_play_get();
    if (!dh_menu_lifted) return;
    dh_menu_lifted = 0;
    if (play) *(volatile u16*)(play + DH_PLAY_NOW_LIFE) = dh_menu_life;
}
WWHD_HOOK(0x026A716C, dh_menu_gate_a, (void)) { dh_menu_enter(); }
WWHD_HOOK_RETURN(0x026A716C, dh_menu_gate_a_end, (void)) { dh_menu_leave(); }
WWHD_HOOK(0x0270D70C, dh_menu_gate_b, (void)) { dh_menu_enter(); }
WWHD_HOOK_RETURN(0x0270D70C, dh_menu_gate_b_end, (void)) { dh_menu_leave(); }

WWHD_REPLACE(0x025C38B8, s32, dh_exit, (u32 ground, f32 speed, s32 type, s32 room)) {
    if (dh_ghost && !dh_dying) return 0;
    return dh_exit_original(ground, speed, type, room);
}

/* SERVER_PLAYER_REVIVE: three hearts, back on the body. */
static void dh_revived(const mp_u8* body, mp_u32 length) {
    char reviver[MP_NAME_MAX + 1];
    if (!mp_read_revive(body, length, reviver)) {
        wwhd_log("multiplayer: malformed revive skipped");
        return;
    }
    if (!dh_ghost || dh_dying || !dh_link) return;
    u8* play = wwhd_play_get();
    u16 max = dComIfGs_player_status_a.max_life;
    u16 life = max < DH_REVIVE_LIFE ? max : DH_REVIVE_LIFE;
    if (play) {
        *(volatile f32*)(play + DH_PLAY_LIFE) = (f32)life; /* the meter adds it to the save's 0 */
        dh_reviving = DH_REVIVE_WAIT_STEPS;
    } else {
        dComIfGs_player_status_a.life = life;
    }
    dh_place((void*)dh_link, &dh_body);
    /* the fairy of a bottle on a fallen Link (changeDeadProc: makeFairy 023F73B8, type LINK_DOWN):
     * it circles the local Link, now on its body; its own 10 hearts (item_func_recover_faily,
     * d_item.cpp) are cut back to the revive's life for DH_REVIVE_CLAMP_SECONDS (dh_step) */
    dh_fairy_pos.x = dh_body.pos[0];
    dh_fairy_pos.y = dh_body.pos[1];
    dh_fairy_pos.z = dh_body.pos[2];
    if (!wwhd_daPy_lk_c__makeFairy((void*)dh_link, &dh_fairy_pos, DH_FAIRY_LINK_DOWN))
        wwhd_log("multiplayer: revive: the fairy was not made");
    dh_clamp_time = DH_REVIVE_CLAMP_SECONDS;
    if (play) *(volatile f32*)(play + DH_PLAY_LIFE) = (f32)life;
    dh_ghost = puppet_local_ghost = 0;
    puppet_remove(DH_LOCAL_BODY);
    dh_set_notice(reviver[0] ? reviver : "A friend", " revived you!");
    wwhd_log("multiplayer: the local Link was revived by");
    wwhd_log(reviver[0] ? reviver : "?");
}

/* The death block of the local player's ActorPlayerSync (mod.c current_look). */
static int dh_down(void) {
    if (!dh_link || dh_reviving || dComIfGs_player_status_a.life != 0) return 0;
    u32 save = WWHD_GAME_DATA(0x101F84DC, u32);
    return !(save && dh_check_bottle(save + DH_SAVE_ITEMS, DH_BOTTLE_FAIRY)); /* a fairy revives it */
}

/* Dead for the others (the directory's MP_DIR_DEAD: the launcher's skull, no teleport to us). */
static int dh_local_dead(void) { return dh_ghost || dh_down(); }

static void dh_fill_look(mp_player_look* look) {
    if (!dh_ghost) {
        if (state == READY && dh_down()) look->death = MP_DEATH_DOWN; /* the game's death: no survivor */
        return;
    }
    look->death = MP_DEATH_GHOST;
    look->death_dir = directory_valid ? directory.self : 0;
    for (int i = 0; i != 3; ++i) look->corpse[i] = dh_body.pos[i];
    look->corpse_angle = dh_body.rot[1];
}

/* A body: a puppet in its player's colours and skin, given the death animation once made. */
static void dh_body_slot(mp_u32 uid, const mp_transform* t, const mp_player_look* look) {
    puppet_set_target(uid, t);
    puppet_set_look(uid, (mp_u16)look->colors[0], (mp_u16)look->colors[1], PUPPET_NO_PROC, PUPPET_ITEM_NONE,
                    (mp_u8)(look->extras & MP_EXTRA_CASUAL), look->skin);
    puppet_slot* s = puppet_slot_for(uid, 0);
    if (!s || !s->actor || s->posed == s->actor) return;
    mp_anim_events_clear(&dh_pose);
    dh_pose.single_count = 1;
    dh_pose.single[0].anm = DH_ANM_DIELONG;
    dh_pose.single[0].rate = 1.0f;
    dh_pose.single[0].start = 0.0f;
    dh_pose.single[0].end = -1;
    dh_pose.single[0].morf = 0.0f;
    puppet_queue_events(uid, &dh_pose);
    s->posed = s->actor;
}

static void dh_bodies_step(const daPy_lk_c* link) {
    if (dh_ghost) {
        mp_player_look look = current_look(link);
        dh_body_slot(DH_LOCAL_BODY, &dh_body, &look);
    } else {
        puppet_remove(DH_LOCAL_BODY);
    }
    for (int i = 0; i != MAX_REMOTES; ++i) {
        const remote_player* r = &remotes[i];
        if (!r->active) continue;
        int dead = r->has_look && (r->look.death & MP_DEATH_GHOST);
        puppet_set_ghost(r->uid, dead);
        if (!dead) {
            puppet_remove(PUPPET_BODY_UID | r->uid);
            continue;
        }
        mp_transform t;
        for (int k = 0; k != 3; ++k) t.pos[k] = r->look.corpse[k];
        t.rot[0] = t.rot[2] = 0;
        t.rot[1] = r->look.corpse_angle;
        dh_body_slot(PUPPET_BODY_UID | r->uid, &t, &r->look);
    }
    for (int i = 0; i != PUPPET_MAX; ++i) { /* the bodies of players who left */
        puppet_slot* s = &puppet_slots[i];
        if (!s->used || !(s->uid & PUPPET_BODY_UID) || s->uid == DH_LOCAL_BODY) continue;
        if (!find_remote(s->uid & ~PUPPET_BODY_UID, 0)) puppet_release(s);
    }
}

/* The living local Link by a remote body: hold A for DH_REVIVE_SECONDS. */
static void dh_revive_step(const fopAc_ac_c* link, double dt) {
    dh_prompt = 0;
    if (dh_cooldown > 0) dh_cooldown -= dt;
    const u8* play = wwhd_play_get();
    if (dh_ghost || state != READY || session.client_player_revive == MP_NO_ID || !play ||
        play[WWHD_PLAY_EVENT_RUNNING_OFFSET]) {
        dh_progress = 0;
        dh_target = MP_NO_ID;
        return;
    }
    const remote_player* best = 0;
    f32 best_d2 = DH_REVIVE_RADIUS * DH_REVIVE_RADIUS;
    for (int i = 0; i != MAX_REMOTES; ++i) {
        const remote_player* r = &remotes[i];
        if (!r->active || !r->has_look || !(r->look.death & MP_DEATH_GHOST) || !r->look.death_dir) continue;
        f32 dx = r->look.corpse[0] - link->current.pos.x, dz = r->look.corpse[2] - link->current.pos.z;
        f32 dy = r->look.corpse[1] - link->current.pos.y;
        f32 d2 = dx * dx + dz * dz;
        if (d2 < best_d2 && dy < DH_REVIVE_HEIGHT && dy > -DH_REVIVE_HEIGHT) {
            best_d2 = d2;
            best = r;
        }
    }
    if (!best) {
        dh_progress = 0;
        dh_target = MP_NO_ID;
        return;
    }
    if (best->uid != dh_target) {
        dh_target = best->uid;
        dh_progress = 0;
    }
    dh_prompt = 1;
    dh_cursed = el_cursed_in_room(link->current.roomNo);
    u32 n = 0;
    while (n != MP_NAME_MAX && best->look.name[n]) {
        dh_target_name[n] = best->look.name[n];
        ++n;
    }
    if (!n) n = format_label(dh_target_name, best->uid);
    dh_target_name[n] = 0;
    wwhd_input_state input;
    wwhd_input_read(&input);
    if (dh_cursed || dh_cooldown > 0 || !(input.buttons & DH_BUTTON_A)) {
        dh_progress -= dt * 2.0;
        if (dh_progress < 0) dh_progress = 0;
        return;
    }
    dh_progress += dt;
    if (dh_progress < DH_REVIVE_SECONDS) return;
    dh_progress = 0;
    dh_cooldown = DH_REVIVE_COOLDOWN;
    mp_writer body = mp_writer_make(payload, sizeof(payload));
    char me[MP_NAME_MAX + 1];
    u32 m = 0;
    while (m != MP_NAME_MAX && local_look.name[m]) {
        me[m] = local_look.name[m];
        ++m;
    }
    if (!m && local_uid != MP_NO_ID) m = format_label(me, local_uid);
    me[m] = 0;
    mp_write_revive_request(&body, best->look.death_dir, me);
    if (send_payload(session.client_player_revive, &body)) {
        dh_set_notice("You revived ", dh_target_name);
        wwhd_log("multiplayer: revive sent to");
        wwhd_log(dh_target_name);
    }
}

/* Every step of the local Link, connected or not. */
static void dh_step(void* link, double dt) {
    if (dh_dying_anim && (dh_dying_time += dt) >= DH_DEATH_ANIM_SECONDS) dh_after_death_anim(link);
    if (dh_clamp_time > 0) { /* the revive's life, not the fairy's */
        dh_clamp_time -= dt;
        u8* play = wwhd_play_get();
        u16 max = dComIfGs_player_status_a.max_life;
        s32 want = max < DH_REVIVE_LIFE ? max : DH_REVIVE_LIFE, have = dComIfGs_player_status_a.life;
        if (play && (f32)have + *(volatile f32*)(play + DH_PLAY_LIFE) > (f32)want)
            *(volatile f32*)(play + DH_PLAY_LIFE) = (f32)(want > have ? want - have : 0);
    }
    if (dh_ghost && !dh_dying && !dh_shared()) {
        dh_dying = 1;
        puppet_remove(DH_LOCAL_BODY);
        wwhd_log("multiplayer: no living player left in the stage: the ghost dies (game over)");
    }
    if (!dh_ghost) dh_dying = 0;
    if (dh_reviving) {
        if (dComIfGs_player_status_a.life != 0) {
            dh_reviving = 0;
        } else if (--dh_reviving == 0) { /* the meter did not run (logged, not hidden) */
            u16 max = dComIfGs_player_status_a.max_life;
            dComIfGs_player_status_a.life = max < DH_REVIVE_LIFE ? max : DH_REVIVE_LIFE;
            wwhd_log("multiplayer: revive: the meter gave no life, the save's life was written (doors may stay shut)");
        }
    }
    if (dh_notice_steps > 0) --dh_notice_steps;
    if (state == READY) dh_bodies_step((const daPy_lk_c*)link);
    dh_revive_step((const fopAc_ac_c*)link, dt);
}

/* Screen point of a world position (as mod.c update_markers): 0 when behind or off screen. */
static int dh_project(const float* pos, f32 lift, f32* x, f32* y) {
    const u8* play = wwhd_play_get();
    f32 screen_w = WWHD_GAME_DATA(ADDR_mDoLib_screenWidth, f32), screen_h = WWHD_GAME_DATA(ADDR_mDoLib_screenHeight, f32);
    if (!play || !*(const volatile u32*)(play + PLAY_VIEW_OFFSET) || screen_w <= 0.0f || screen_h <= 0.0f) return 0;
    project_in.x = pos[0];
    project_in.y = pos[1] + lift;
    project_in.z = pos[2];
    wwhd_mDoLib_pos2camera((u32)&project_in, (u32)&project_out);
    if (-project_out.z < 20.0f) return 0;
    wwhd_mDoLib_project((u32)&project_in, (u32)&project_out);
    *x = project_out.x / screen_w * HUD_WIDTH;
    *y = project_out.y / screen_h * HUD_HEIGHT;
    int shown = *x > -40.0f && *x < HUD_WIDTH + 40.0f && *y > -40.0f && *y < HUD_HEIGHT + 40.0f;
    *x = hud_projected_x(*x); /* mod.c: any aspect ratio (0.3.102), drawn anchored at the centre */
    return shown;
}

static wwhd_hud_element dh_element;

static void dh_rect(u32 list, f32 x, f32 y, f32 w, f32 h, u32 rgba) {
    if (!(w > 0.0f && h > 0.0f)) return;
    dh_element = (wwhd_hud_element){.kind = WWHD_HUD_RECT, .anchor = WWHD_HUD_CENTER, /* centred bars (0.3.102) */ .blend = WWHD_HUD_ALPHA,
                                    .x = x, .y = y, .w = w, .h = h, .thickness = 1.0f, .rgba = rgba};
    wwhd_hud_emit(list, &dh_element);
}

static u32 dh_text(char* out, u32 cap, const char* a, const char* b) {
    u32 n = 0;
    for (const char* s = a; *s && n != cap; ++s) out[n++] = *s;
    for (const char* s = b; s && *s && n != cap; ++s) out[n++] = *s;
    return n;
}

/* The bodies' markers, the revive prompt and gauge, the ghost's banner (mod.c draw_markers). */
static void dh_draw(u32 list) {
    const u8* play = wwhd_play_get();
    if (state != READY || !play || play[WWHD_PLAY_EVENT_RUNNING_OFFSET] || chart_menu_open()) return;
    char text[64];
    f32 x, y;
    for (int i = 0; i != MAX_REMOTES; ++i) {
        const remote_player* r = &remotes[i];
        if (!r->active || !r->has_look || !(r->look.death & MP_DEATH_GHOST)) continue;
        if (!dh_project(r->look.corpse, 40.0f, &x, &y)) continue;
        u32 n = 0;
        while (n != MP_NAME_MAX && r->look.name[n]) {
            text[n] = r->look.name[n];
            ++n;
        }
        if (!n) n = format_label(text, r->uid);
        n += dh_text(text + n, sizeof(text) - n, " (down)", 0);
        name_draw(list, x, y, text, n, 0xB0B0B0u, 0.4f, 0.9f);
    }
    if (dh_ghost && dh_project(dh_body.pos, 40.0f, &x, &y))
        name_draw(list, x, y, "Your body", 9, 0xB0B0B0u, 0.4f, 0.9f);
    if (dh_ghost)
        name_draw(list, HUD_WIDTH * 0.5f, 90.0f, dh_dying ? "Game over" : "You are a ghost: wait for a friend", dh_dying ? 9 : 34,
                  0xC8D8FFu, 0.55f, 1.0f);
    if (dh_prompt && dh_cursed) {
        static const char cursed[] = "A cursed elite is in the room: no revive";
        name_draw(list, HUD_WIDTH * 0.5f, 560.0f, cursed, sizeof(cursed) - 1, 0xC890FFu, 0.5f, 1.0f);
    } else if (dh_prompt) {
        u32 n = dh_text(text, sizeof(text), "Hold A to revive ", dh_target_name);
        name_draw(list, HUD_WIDTH * 0.5f, 560.0f, text, n, 0xFFFFFFu, 0.5f, 1.0f);
        f32 w = 300.0f, k = (f32)(dh_progress / DH_REVIVE_SECONDS);
        if (k > 1.0f) k = 1.0f;
        dh_rect(list, HUD_WIDTH * 0.5f - w * 0.5f - 3.0f, 577.0f, w + 6.0f, 18.0f, 0x000000B0u);
        dh_rect(list, HUD_WIDTH * 0.5f - w * 0.5f, 580.0f, w * k, 12.0f, 0x6FE08AFFu);
    }
    if (dh_notice_steps > 0)
        name_draw(list, HUD_WIDTH * 0.5f, 150.0f, dh_notice, dh_notice_bytes, 0xF9C74Fu, 0.5f,
                  dh_notice_steps < 30 ? (f32)dh_notice_steps / 30.0f : 1.0f);
}
