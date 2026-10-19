/* Synchronised placed actors (enemies, props, objects of the server's ActorFilter list) and the
 * server's enemy multiplier. Option "sync_actors" (default on).
 *
 * Model: the historical one (WindWakerClient ActorSync, MMBServerCommon SyncedActorManager). The
 * server makes one player the owner of each actor, the nearest one (SyncedActorComponent.onRun,
 * every 2 s) unless a player holds it exclusively (CLIENT_ACTOR_EXCLUSIVE_ADD). The owner's game
 * runs the actor and sends its ActorBaseSync (position, shape angle) and ActorHealthSync (health,
 * actor_status); the other clients still run its execute (animations, attacks on their own Link)
 * but put it where the owner's updates say.
 *
 * Sword hits are replayed everywhere, as the GameCube client did with its hit list
 * (ActorPlayerSync.cpp sendPlayerHit / HitPacket): every client's copy takes the same hits, so an
 * enemy dies the normal way in every game (death animation, drops). The hitter's game records each
 * hit of its Link's sword colliders (mAtCps[0..2], mAtCyl) on a synchronised actor, after
 * cCcSState::SetAtTgCommonHitInf (0200CFA8, the function the GameCube hook called, decomp
 * SSystem/SComponent/c_cc_s.cpp), when the target collider took it (cCcD_ObjTg::mHitObj is the
 * sword's). The record (mp_protocol.h mp_strike) carries the attack values the damage code reads
 * (d_cc_uty.cpp at_power_check / cc_at_check: cCcD_ObjAt mSPrm, mType, mAtp; dCcD_GObjAt mSPrm,
 * mSe, mMtrl, mHitMark, mSpl, mVec) and the hit position. The others replay it right after
 * cCcSState::ChkAtTg (0200D19C), which clears the step's hit information first: a collider of
 * ours (dCcD_Cps, built like sword_swing.h's) takes the values, its status is the hitter's Link
 * puppet's (daPy_lk_c::mStts, so the attacker is a player as for the real sword), and
 * SetAtTgCommonHitInf is called on the target's collider. The target reacts in its next execute
 * exactly as to a hit of the local sword. The game's own continuous-hit filter
 * (dCcS::ChkAtTgHitAfterCross) runs on the target's status as for a real hit.
 *
 * Differences from the GameCube client, for what it could not do:
 *  - its non-owners lerped the actor after its execute only and forced the owner's health every
 *    frame. Here the network position and angle are set before the execute (the actor computes
 *    its matrices and colliders there) and again after it; the owner's health is applied only when
 *    lower than the copy's (a missed hit), never to heal a copy that took a hit first, never to a
 *    copy at 0 (dying). A health drop that is not a replayed hit is a hit of this player: the
 *    client asks for the actor exclusively, so it becomes the owner while it fights it;
 *  - a dead actor was never removed for the others: an owner (or the client that dealt the hit)
 *    whose enemy is deleted with a health of 0 or less sends CLIENT_ACTOR_SYNC_REMOVE. Another
 *    client's copy that is dying already (health 0 or less, from the replayed hits) finishes its
 *    death; one still alive (a hit was lost) is deleted after AS_REMOVE_WAIT seconds. Deletions
 *    with health left (room or stage unloaded) remove nothing for the others;
 *  - a prop that moves in a non-owner's game while its Link is near (carried, pushed) is asked
 *    for exclusively the same way.
 * The exclusivity is given back (CLIENT_ACTOR_EXCLUSIVE_REMOVE) AS_EXCLUSIVE_HOLD seconds after
 * the last hit or move; the server then gives the actor to the nearest player again.
 *
 * Identity of an actor between the clients (the game UID of CLIENT_ACTOR_SYNC_CREATE): like the
 * GameCube client (Hooks.cpp onActorCreate: the first 0x21 bytes of its fopAcM_prm_class), the
 * parameter block of a placed actor, here all of it (0x24 bytes, decomp
 * include/f_op/f_op_actor_mng.h): parameters, position, angle, setID, scale, gbaName, parent_id,
 * argument, room_no, and its two padding bytes, which dStage_actorCreate (025C1154, d_stage.cpp)
 * sets here to AS_TAG and the copy number of the multiplier. fopAcM_CreateAppend (025D5600,
 * f_op_actor_mng.cpp) clears the block, so an actor the game creates at run time (items, effects,
 * the Link puppets) has no tag and is never synchronised, as on the GameCube. The block is read
 * in fopAc_Create (025D4C70, f_op_actor.cpp) on its first call (mInitState 0, the block is
 * base_process_class::mpUserData +0xAC, decomp include/f_pc/f_pc_base.h). The name is
 * dStage_getName2(procName, argument) (025C1150), the name ActorSyncConfig.cpp
 * ACTORSYNC_GAME_ACTOR_TO_NAME uses, matched against the ActorFilter list and used as the factory
 * name (ActorSync::getFactory).
 *
 * The enemy multiplier (formerly the enemy-multiplier mod, which must not be installed with this
 * one: both replace dStage_actorCreate) uses the server's value (mp_session::enemy_multiplier,
 * the ActorFilter entry MP_MULTIPLIER_ENTRY), so every client creates the same copies at the same
 * places and their game UIDs match. Copies are on rings of AS_SPREAD units (8 per ring), a
 * constant for the same reason.
 * Live mutators: the server sends SERVER_MUTATORS when its multiplier or randomizer changes while
 * it runs (ActorFilter.applyMutators, mp_read_mutators), and the init's values may differ from the
 * area already loaded when the player connects. The loaded area then follows the new values at the
 * next step (as_apply_mutators): each placed enemy (an "original", copy 0) is remembered with its
 * stage name and parameter block (as_groups; the stage name comes from as_stage_create through
 * as_notes, keyed by the block's address, the block fopAc_Create reads), each copy with its number
 * (as_enemy_copies). The missing copies are made as at load time: a block from
 * fopAcM_CreateAppend, the same ring place and variant, then dStage_actorCreate with a
 * stage_actor_data_class holding only the name (d_stage.cpp: it reads only the name, then writes
 * argument and gbaName into the block, and requests the actor in fpcLy_CurrentLayer, as
 * fopAcM_create does for the spawned actors in as_step). Copies numbered from the new multiplier
 * on, or all of them when the randomizer changes, are deleted (fopAcM_delete_actor); their owner
 * removes them at the server (CLIENT_ACTOR_SYNC_REMOVE). A copy killed before is not made again
 * unless the randomizer changes. The variant hash skips gbaName and argument (0x1B, 0x20), which
 * dStage_actorCreate writes, so a copy made live and one made at load time are the same.
 *
 * Engine fields: fopAc_ac_c (SDK wwhd/actor.h) group 0x2DA, actor_status 0x2E0, current 0x314,
 * shape_angle 0x328, health 0x3A1 (s8); base_process_class mBsPcId +4, mProcName +8 (s16),
 * mInitState +0xC (s8), mpUserData +0xAC (decomp include/f_pc/f_pc_base.h). Replaced functions,
 * one argument each (bindings.h and the decomp's ext lines): fopAc_Create 025D4C70,
 * fopAc_Delete 025D4B4C (TRUE once the actor is gone), fopAc_Execute 025D475C (with the puppet
 * senses of puppet_sense.h); dStage_actorCreate 025C1154 has two. fopAcM_delete_actor 025D57E0
 * deletes a copy the server removed.
 *
 * Spawned actors (actors the game creates while playing; the GameCube TODO "FastCreate Item
 * Sync"): items dropped by enemies (daItem, process 0xFF), bombs (0x126, 0x127), arrows (0x1D8)
 * and boomerangs (0x1B0). They go through the server with no game UID and the factory "Spawned"
 * (ActorBaseSync, ActorSpawnSync: process name, parameter block fields, speeds, mp_protocol.h
 * mp_spawn); the server sends them to everyone in the area, also to the players who come later.
 *  - Creation context: while an actor runs its execute (fopAc_Execute) or its deletion
 *    (fopAc_Delete), the actors it creates are made for the local Link (LOCAL), for an enemy this
 *    player owns (OWNED) or for an enemy another player owns (FOREIGN); an actor created in an
 *    OWNED or FOREIGN context that is not itself synchronised (the death puff daDisappear, which
 *    drops the item from its own execute later, d_a_disappear.cpp) passes its context on to what
 *    it creates. fopAcM_CreateAppend (025D5600, every creation function's parameter block,
 *    f_op_actor_mng.cpp createAppend) is replaced to write the context into the block's padding
 *    (AS_RT_TAG, the context), which fopAc_Create reads like the placed actors' tag.
 *  - An item made in a FOREIGN context is deleted at its first execute: the owner's item comes
 *    instead (the drop tables are random, cM_rndF, so each game would drop another one). Items
 *    made in a LOCAL or OWNED context and weapons of the local Link (LOCAL) are announced
 *    (CLIENT_ACTOR_SYNC_CREATE); an enemy's own bomb stays in each game, where it hurts that
 *    game's Link. A weapon's creator asks for it exclusively, so it stays its owner.
 *  - The others create it again with fopAcM_create (025D5834, eight register arguments: the
 *    creation is a request; the id it returns is the new process's mBsPcId,
 *    f_pc_stdcreate_req.cpp), find it in fopAc_Create by that id, give an item its speeds, and
 *    show it where its owner's updates say, like a placed actor. A weapon copy runs its execute with
 *    its creator's Link puppet as player 0 and link player (play + 0x5B2C and + 0x5B34,
 *    d_com_inf_game.h mPlayerInfo / mpPlayerPtr: bomb, arrow and boomerang read them); without a
 *    puppet no weapon copy is made. A copy's attacks hit nothing (SetAtTgCommonHitInf is replaced
 *    and skips them): its creator's hits come as strikes (attacker = the weapon's uid) and are
 *    replayed with the copy's own collider, given the attack values for that call.
 *  - An item picked up (deleted with the local Link within AS_PICKUP_DISTANCE) is removed for
 *    everyone (CLIENT_ACTOR_SYNC_REMOVE); a weapon is removed when its creator's goes. Other
 *    deletions (timeouts) happen in each game on their own.
 *  - Cut grass and flowers (dGrass_data_c::WorkAt 0254A7C8, dFlower_data_c::WorkAt 025453B8, both
 *    given the cutting actor, decomp d_grass.cpp / d_flower.cpp) drop from the item table with the
 *    player's own health (fopAcM_createItemFromTable): the cut runs in the LOCAL context when the
 *    local Link cuts, FOREIGN when a Link puppet does (a replayed sword swing, sword_swing.h), so
 *    the cutter's item is the one everybody gets.
 *  - The enemies' item balls (daIball, process 0x18F, d_a_ib.cpp: hit, they make their items) are
 *    spawned actors like the items: only the enemy owner's ball exists everywhere, the hits on it
 *    go as strikes, so it breaks in every game and the items of its owner's game are the ones kept.
 *  - Arrows (d_a_arrow.cpp): the element is chosen at creation by setTypeByPlayer (02052420, from
 *    the local selection and magic) and a change of element makes a new arrow (changeArrowType);
 *    the arrow waits in the hand (procWait, its matrix from player 0's left hand) until Link sets
 *    its parameters to 1, then flies (procMove). The element goes in mp_spawn::variant
 *    (daArrow_c::mArrowType +0x71D), the copy takes it in setTypeByPlayer (replaced), the owner
 *    sends the parameters again when they change, the copy gets them and is shot. A copy does not
 *    use the local magic (arrowUseMp 02053AD0 replaced). The shot direction is current.angle,
 *    which procWait takes from player 0's left hand (setKeepMatrix); the puppet's arm does not
 *    aim (Link's aim turns his upper body in code, not by animation), so the copy's
 *    daArrow_c::arrowShooting (02053814) first takes the place and angles of the owner's arrow
 *    sent with the shot, then flies on its own (no longer put at the owner's places).
 *  - The ship's cannon makes its bomb in the ship's execute (d_a_ship_exec.cpp, fopAcM_fastCreate
 *    of a bomb, speeds given after): the local player's ship (play + 0x5B3C, mpPlayerPtr[2]) runs
 *    in the LOCAL context, so its bombs are the player's weapons; a copy gets the speeds.
 *
 * Carried actors (the GameCube PropsSync: pots, rocks, bombs). Link holding an actor sets its
 * fopAcStts_CARRY_e (0x2000) status bit, the hookshot fopAcStts_HOOK_CARRY_e (0x100000)
 * (decomp include/gc_api/f_op/f_op_actor.h); the actor then does not fall and lets its carrier
 * place it (d_a_tsubo_mode.cpp tests actor_status & 0x102000, d_a_bomb.cpp & 0x2000), and drops
 * or flies when the bit goes. The copies of an actor another player holds take these bits from
 * the owner: a placed actor's from its ActorHealthSync status, a spawned one's from
 * mp_spawn::variant (bit 0 carry, bit 1 hookshot carry; the owner sends it again when it changes).
 * The GameCube PropsSync's own fields (pot state, thrown and grounded bytes, model angle) were
 * GameCube daTsubo offsets: they are still written as zeros and not applied.
 *
 * Enemy variants (server setting "enemyRandomizer", MP_RANDOMIZER_ENTRY): the multiplier's
 * copies take, instead of the GameCube randomizer's arbitrary parameter words, a variant among the
 * values as_variants lists for that enemy, field by field, read in each enemy's create (decomp):
 *  - Bokoblin (d_a_bk.cpp daBk_Create): bits 0-3 the type, only among 0, 4 and 0xB (0xB: the pink
 *    one, m02DC, body BK_PINK in useHeapInit, made type 4, no weapon, not tougher; carrier, path and
 *    event types stay), bit 5 tougher (7 health, else 5), bits 6-7 its weapon (0x40 type 1, 0x80
 *    type 7, else 0);
 *  - ChuChu (d_a_cc.cpp): bits 8-15 the colour, CC_GREEN..CC_YELLOW (0-4, d_a_cc.h);
 *  - Darknut (d_a_tn.cpp): bits 0-3 the behaviour, kept unless roaming (0) or guarding (4)
 *    (d_a_tn.h BehaviorType), bits 4-7 the armour colour (0-5, clamped there);
 *  - Keese (d_a_ki.cpp): bit 15 fire damage (mDamageType);
 *  - Octorok (d_a_oq.cpp): bits 8-15 the projectile (0, 1; 0xFF means 0);
 *  - Pig ("Pig", d_a_kb.cpp daKb_Create, useHeapInit): bits 0-3 the shape, a body model per shape
 *    (kb_bdl_idx[shape & 3]: four colours 0-3); only the pigs whose bits 8-15 are 0xFF (Outset's
 *    big pig candidates, mbCanBeBigPig, keep theirs: its quest picks them by shape).
 * A field changes only when the original's value is one of its list (a scripted variant, a boss
 * flag, a special behaviour stays). The pick is a hash of the original parameter block and the
 * copy number, not the game's random generator: every client makes the same copy, so the copies'
 * game UIDs still match. Other enemies' copies keep the original's parameters.
 *
 * Natural deaths: a copy whose owner's health reached 0, or that its owner removed, while it still
 * has health (a lost or weaker hit), is given a lethal replayed hit: the last strike seen on it,
 * with 127 damage, through the same path as a real hit (as_replay), every AS_KILL_PERIOD while its
 * collider does not take hits, so it dies the game's way (death animation, drops). The owner's
 * health is never written below 1 any more: a copy put at 0 that way stayed alive at 0, since only
 * the damage code starts the death. A copy still alive AS_REMOVE_WAIT after the removal is
 * deleted. The Deku Leaf's gust (daPy_lk_c::mFanWindCps +0x7EC4, attack index 4) goes as strikes
 * too.
 *
 * Hit stop (option "hit_stop", off by default, as the GameCube client patched it out: project.py
 * "timer pause"): the game freezes every actor a few frames when the sword hits
 * (d_cc_uty.cpp cc_at_check writes dScnPly_ply_c's next pause timer, data 0x101EACB7;
 * dScnPly_Execute skips the scene while calcPauseTimer, 025B02E8, returns non-zero). Each client
 * froze on its own, out of step with the others. calcPauseTimer is replaced: both timers
 * (pauseTimer 0x101EACB6, nextPauseTimer 0x101EACB7, d_s_play.cpp l_timers) are cleared and 0 is
 * returned. */
#pragma once
#include "wwhd_guest.h"
#include "wwhd/bindings.h"
#include "wwhd/actor.h"
#include "wwhd/data.h"
#include "mp_protocol.h"
#include "puppet_sense.h"

#define AS_MAX 512
#define AS_BUCKETS 256
#define AS_NONE 0xFFFFu
#define AS_TAG 0xA5
#define AS_PRM_BYTES 0x24       /* fopAcM_prm_class */
#define AS_PRM_POSITION 0x04
#define AS_PRM_ARGUMENT 0x20
#define AS_PRM_TAG 0x22         /* mPadding[0] */
#define AS_PRM_COPY 0x23        /* mPadding[1] */
#define AS_PRM_PADDING_START 0x22
#define AS_PC_ID 0x04
#define AS_PROC_NAME 0x08
#define AS_INIT_STATE 0x0C
#define AS_USER_DATA 0xAC
#define AS_GROUP_ENEMY 2        /* fopAc_ENEMY_e */
#define AS_SOLVER_BASE 0x100000u /* the player's own solvers count from 0 (mod.c) */
#define AS_CREATES_PER_STEP 16
#define AS_PENDING_TIMEOUT 5.0  /* no answer to a create: sent again */
#define AS_UPDATE_PERIOD 0.1    /* ActorBaseSync::sUpdateRate */
#define AS_STALE 2.0            /* no owner update for this long: the actor runs on its own */
#define AS_HIT_GRACE 1.5        /* after a local hit or move, the network values wait */
#define AS_EXCLUSIVE_HOLD 3.0
#define AS_MOVE_EPSILON 1.0f    /* a non-owner prop moved by more than this in one execute */
#define AS_INTERACT_DISTANCE 250.0f
#define AS_SPREAD 60.0f
#define AS_REMOVE_WAIT 4.0      /* a removed copy still alive is deleted after this */
#define AS_KILL_PERIOD 0.3      /* a lethal replayed hit is tried this often */
#define AS_KILL_DAMAGE 127
#define AS_LINK_FAN_WIND 0x7EC4 /* daPy_lk_c::mFanWindCps */
#define AS_REPLAY_WINDOW 0.5    /* a health drop this soon after a replayed hit is that hit */
#define AS_MAX_STRIKES 32       /* received strikes waiting for the next ChkAtTg */
#define AS_MAX_TG_OFFSET 0x10000
/* collider layouts (decomp include/SSystem/SComponent/c_cc_d.h, include/d/d_cc_d.h) */
#define AS_OBJ_AT_SPRM 0x00     /* cCcD_Obj::mObjAt.mSPrm */
#define AS_OBJ_AT_RPRM 0x04
#define AS_OBJ_AT_HITOBJ 0x08
#define AS_OBJ_AT_TYPE 0x10
#define AS_OBJ_ATP 0x14
#define AS_OBJ_TG_SPRM 0x18     /* cCcD_Obj::mObjTg.mSPrm */
#define AS_OBJ_TG_HITOBJ 0x20
#define AS_OBJ_CO_SPRM 0x2C
#define AS_OBJ_STTS 0x44        /* cCcD_Obj::mStts */
#define AS_GAT 0x50             /* dCcD_GObjInf::mGObjAt */
#define AS_GAT_RPRM 0x04
#define AS_GAT_EFF_COUNTER 0x14
#define AS_GAT_SE 0x1C          /* mSe, mMtrl, mHitMark, mSpl */
#define AS_GAT_HIT_POS 0x20
#define AS_GAT_VEC 0x2C
#define AS_STTS_ACTOR 0x0C      /* cCcD_Stts::mp_actor */
#define AS_SPRM_SET 1u          /* cCcD_TgSPrm_Set_e */
#define AS_LINK_STTS 0x7620     /* daPy_lk_c::mStts (sword_swing.h) */
#define AS_LINK_AT_CYL 0x78BC   /* daPy_lk_c::mAtCyl */
#define AS_LINK_AT_CPS 0x7B1C   /* daPy_lk_c::mAtCps[3] */
#define AS_CPS_BYTES 0x138      /* sizeof(dCcD_Cps) */
#define AS_AT_CPS_SRC 0x101CEDA8 /* l_at_cps_src, Link's sword source data (sword_swing.h) */
#define AS_PLAY_CCS 0x26A4      /* dComIfG_play_c::mCcS from wwhd_play_get() (d_com_inf_game.h) */
#define AS_PAUSE_TIMERS 0x101EACB6 /* dScnPly l_timers: pauseTimer, nextPauseTimer (s8) */
#define AS_RT_TAG 0xA6          /* padding[0] of a block made in a creation context */
#define AS_PROC_ITEM 0xFF       /* fpcNm_ITEM_e (d_a_bk_main.h) */
#define AS_PROC_BOMB 0x126      /* fpcNm_BOMB_e (d_a_bk_local.h) */
#define AS_PROC_BOMB2 0x127     /* fpcNm_Bomb2_e (d_a_am.h) */
#define AS_PROC_ARROW 0x1D8     /* fpcNm_ARROW_e (d_a_arrow.h) */
#define AS_PROC_BOOMERANG 0x1B0 /* fpcNm_BOOMERANG_e (d_a_player_main.h) */
#define AS_PROC_IBALL 0x18F     /* fpcNm_Iball_e (d_a_disappear.h) */
#define AS_PROC_HOOKSHOT 0xA9   /* fpcNm_HOOKSHOT_e (d_a_hookshot.h) */
#define AS_PROC_ROPE 0x1BE      /* fpcNm_HIMO2_e, the Grappling Hook (d_a_player_main_01.h) */
/* daBoomerang_c (include/d/actor/d_a_boomerang.h) */
#define BOOM_IS_RETURNING 0x2626C
#define BOOM_CATCH_AND_DELETE 0x2626F
#define BOOM_THIRD_PERSON 0x26273
#define BOOM_PROC_ADJUST 0x264C0   /* s16 */
#define BOOM_PROC_SELECTOR 0x264C2 /* s16, -1: mProcTarget is the function */
#define BOOM_PROC_TARGET 0x264C4
/* daHookshot_c (include/d/actor/d_a_hookshot.h): mCurrProcFunc (GHS ptmf: s16 d, s16 i, u32 f) */
#define HOOK_PROC_INDEX 0xD7AA
/* himo2_class (include/d/actor/d_a_himo2.h) */
#define ROPE_M029E 0x3BA          /* s16: != 0 hides */
#define ROPE_START 0x3D0          /* m02B4: cXyz, the rope's first point */
#define ROPE_FIRST 0x3E8          /* m02CC: s32, first segment drawn - 1 */
#define ROPE_STATE 0x3F8          /* m02DC: s32, HIMO2_WAIT 0 .. HIMO2_HANG 11 */
#define ROPE_HANDS 0x408          /* m02EC[2]: left, right hand points */
#define ROPE_SEGMENTS 0x42C       /* m0310[100]: himo2_s, 0x24 each, m10 cXyz +0x10, m1C/m1E s16 */
#define ROPE_SEGMENT_BYTES 0x24
#define ROPE_LENGTH 0x2688        /* m2188: f32, rope paid out */
#define ROPE_SPIN 0x2B08          /* m24D8: s8, rope wound round a post */
#define ROPE_DEMO 0x2B09          /* m24D9: s8, < 0 hides */
#define ROPE_LAUNCH_FREE 2
#define AS_ARROW_TYPE 0x71D     /* daArrow_c::mArrowType */
#define AS_PARAMETERS 0xB0      /* base_process_class::mParameters */
#define AS_CARRY_BITS 0x102000u /* fopAcStts_CARRY_e | fopAcStts_HOOK_CARRY_e */
#define AS_PLAY_PLAYER0 0x5B2C  /* mPlayerInfo[0].mpPlayer */
#define AS_PLAY_LINK 0x5B34     /* mpPlayerPtr[0] */
#define AS_PLAY_SHIP 0x5B3C     /* mpPlayerPtr[2]: the local player's ship */
#define AS_PICKUP_DISTANCE 300.0f
#define AS_SPAWN_WAIT 3.0       /* a copy not created by then is given up */
#define AS_MAX_INHERIT 64
#define AS_MAX_KILL 32
#define AS_PRM_ANGLE 0x10
#define AS_PRM_SCALE 0x18

enum { AS_LOCAL, AS_PENDING, AS_SYNCED, AS_REMOVED, AS_SPAWNING };
enum { AS_CTX_NONE, AS_CTX_OWNED, AS_CTX_FOREIGN, AS_CTX_LOCAL };
enum { AS_KIND_PLACED, AS_KIND_SPAWNED };
enum { AS_CLS_NONE, AS_CLS_ITEM, AS_CLS_WEAPON, AS_CLS_BALL };

typedef struct as_slot {
    u32 actor, pcid;
    u16 next;            /* bucket chain */
    u8 used, state;
    u8 factory;          /* index in the session */
    u8 group;
    u8 local_hit;        /* this player hit or moved it lately */
    u8 exclusive;        /* CLIENT_ACTOR_EXCLUSIVE_ADD sent */
    u8 deleting;         /* removed by the server, deletion requested */
    u8 has_target, has_display, has_net_health, has_sent;
    u8 replayed;         /* a received hit was replayed on it lately */
    u8 broken;           /* a pot broken in this game (as_pot_damaged): removed for everyone */
    u8 kind;             /* AS_KIND_* */
    u8 cls;              /* spawned: AS_CLS_* */
    u8 replica;          /* spawned: created from the network */
    u8 requested;        /* replica: fopAcM_create called */
    u8 spawn_ready;      /* replica: speeds given */
    mp_u32 creator;      /* spawned replica: its creator's PlayerPuppet uid */
    u32 request_id;      /* replica: the process id fopAcM_create returned */
    u32 sent_params;     /* owner: mParameters last sent (an arrow is shot when they change) */
    u32 applied_params;  /* boomerang copy: the owner's mParameters last given to it */
    u32 wait_proc;       /* boomerang copy: its mProcTarget in procWait (this game's address) */
    u8 has_shot;         /* arrow copy: the owner's place and angles at the shot are known */
    u8 free_flight;      /* arrow copy: shot, flies on its own */
    mp_transform shot;
    mp_spawn spawn;      /* replica: how to create it */
    s8 max_health;       /* highest health seen */
    s8 health_before;
    u8 prm[AS_PRM_BYTES];
    char name[MP_ACTOR_NAME_MAX + 1];
    mp_u32 solver, uid, owner;
    mp_u8 ticket;
    mp_transform target, display, sent;
    f32 before_pos[3];   /* current.pos when its execute began */
    s16 net_health, sent_health;
    u8 net_tag, sent_tag;  /* the owner's elite draw (elite.h), with the health */
    u32 net_status;      /* the owner's actor_status (ActorHealthSync) */
    u8 sent_variant;     /* owner of a spawned actor: mp_spawn::variant last sent */
    u8 has_last_strike;  /* a strike was seen on it (to give it a lethal one) */
    mp_strike last_strike;
    double kill_clock;
    u32 sent_status;
    double age, hit_clock, send_clock, pending_clock, replay_clock, remove_clock;
} as_slot;

typedef struct as_pending_strike {
    mp_u32 player;   /* the hitter's PlayerPuppet uid */
    mp_strike strike;
} as_pending_strike;

static as_slot as_slots[AS_MAX];
static u16 as_buckets[AS_BUCKETS];
static int as_ready_init; /* as_buckets filled with AS_NONE */
static int as_enabled = 1;
static const mp_session* as_session; /* set once the server's init is parsed */
static mp_u32 as_next_solver = AS_SOLVER_BASE;
static mp_u32 as_local_uid = MP_NO_ID; /* our PlayerPuppet: the owner id the server sends */
static f32 as_link_pos[3];
static int as_have_link;
static u32 as_copies, as_synced_logged, as_removed_logged, as_owned_logged;
static mp_u8 as_payload[512];
static u32 as_link;      /* the local Link, from the last step */
static int as_hit_stop;  /* option "hit_stop": keep the game's hit stop */
static as_pending_strike as_strikes[AS_MAX_STRIKES];
static u32 as_strike_count;
static u32 as_strikes_sent, as_strikes_replayed;
static u8 as_ghost[AS_CPS_BYTES] __attribute__((aligned(32)));
static int as_ghost_built;
static cXyz as_hit_pos; /* passed to the game by guest address */
static u8 as_context;      /* AS_CTX_* of the actor running its execute or deletion */
static int as_spawning;    /* a copy is being created: its block is not tagged */
static int as_replaying;   /* a strike is being replayed: a copy's collider may hit */
typedef struct as_ctx_entry {
    u32 actor, pcid;
    u8 ctx;
} as_ctx_entry;
static as_ctx_entry as_inherit[AS_MAX_INHERIT]; /* actors passing their context on */
static u32 as_inherit_count;
static as_ctx_entry as_kill[AS_MAX_KILL];       /* items deleted at their first execute */
static u32 as_kill_count;
static u32 as_spawned_logged, as_replica_logged, as_suppressed_logged;
static cXyz as_spawn_pos, as_spawn_scale;  /* fopAcM_create arguments, by guest address */
static csXyz as_spawn_angle;

/* elite.h (included after this file): dynamic scaling and elite enemies */
typedef struct el_actor el_actor;
static void el_note_create(void* actor, u32 prm);
static el_actor* el_execute_begin(void* actor);
static void el_execute_end(el_actor* e);
static el_actor* el_delete_begin(const void* actor);
static void el_delete_end(el_actor* e, s8 health);
static el_actor* el_draw_begin(void* actor);
static void el_draw_end(void);
static u8 el_tag_of(const void* actor);
static void el_adopt(void* actor, u8 tag);

/* Live mutators (see the header): the placed enemies of the loaded areas. */
#define AS_MAX_GROUPS 256
#define AS_MAX_ENEMY_COPIES 2048
#define AS_MAX_NOTES 512        /* a power of two */
#define AS_STAGE_NAME 8         /* stage_actor_data_class::name */
#define AS_PRM_GBA_NAME 0x1B
typedef struct as_group {       /* an original, copy 0 */
    u32 actor, pcid, id;
    u8 used;
    u8 made;                    /* copies 1..made-1 were made */
    u8 randomized;              /* with the randomizer's variants */
    char name[AS_STAGE_NAME];
    u8 prm[AS_PRM_BYTES];
} as_group;
typedef struct as_copy_entry {
    u32 actor, pcid, group;
    u8 used, copy;
} as_copy_entry;
typedef struct as_note {        /* a block given to dStage_actorCreate, until its fopAc_Create */
    u32 prm, group;
    u8 copy, made, randomized;
    char name[AS_STAGE_NAME];
} as_note;
static as_group as_groups[AS_MAX_GROUPS];
static as_copy_entry as_enemy_copies[AS_MAX_ENEMY_COPIES];
static as_note as_notes[AS_MAX_NOTES];
static u32 as_note_next, as_group_next = 1;
static int as_mutators_dirty;
static u8 as_stage_data[0x20] __attribute__((aligned(4))); /* stage_actor_data_class, by guest address */
/* defined in mod.c */
static int send_payload(mp_u32 id, const mp_writer* body);

/* The 50 enemy names of the GameCube client (WindWakerClient/data/enemies.h). */
static const char* const as_enemies[] = {
    "Bk", "Bb", "mo2", "Tn", "gmos", "c_red", "c_green", "c_blue", "c_black", "c_kiiro",
    "Pig", "Puti", "wiz_r", "bbaba", "amos", "amos2", "bable", "bable_r", "pow", "Sss",
    "keeth", "Fkeeth", "Fmaster", "Fmastr1", "Fmastr2", "Gy", "magtail", "bwds", "kuro_s",
    "kuro_t", "Oq", "Oqw", "p_hat", "sea_hat", "nezumi", "Rdead1", "Rdead2", "Stal", "Oship",
    "Fganon", "Daiocta", "Btd", "Bst", "Bkm", "Bdk", "Dk", "big_pow", "Bwd", "Bgn", "Gnd",
};

/* Valid variants of the enemies' parameter fields (see the header). A field is in the parameter word
 * (AS_WORD_PARAMS) or in angle.x of the block (AS_WORD_ANGLE_X, 0x10), which some enemies read as
 * parameters (the Darknut's equipment). An entry applies only when params & require_mask equals
 * require_value. 0.2.44: Moblin, Wizzrobe, Peahat and the Darknut's equipment (context.md). */
#define AS_WORD_PARAMS 0
#define AS_WORD_ANGLE_X 1
typedef struct as_variant_field {
    u8 word;
    u8 shift;
    u8 count;
    u32 mask;          /* of the field, after the shift */
    u32 values[8];
} as_variant_field;

typedef struct as_enemy_variants {
    const char* name;
    u32 require_mask, require_value;
    u8 count;
    as_variant_field fields[3];
} as_enemy_variants;

#define AS_CHUCHU_COLOURS {AS_WORD_PARAMS, 8, 5, 0xFF, {0, 1, 2, 3, 4}}
static const as_enemy_variants as_variants[] = {
    {"Bk", 0, 0, 2, {{AS_WORD_PARAMS, 0, 3, 0xF, {0, 4, 0xB}},          /* type: 0xB the pink one */
                     {AS_WORD_PARAMS, 5, 6, 0x7, {0, 1, 2, 3, 4, 5}}}}, /* 0x00..0xA0: green body, weapon */
    {"c_red", 0, 0, 1, {AS_CHUCHU_COLOURS}},
    {"c_green", 0, 0, 1, {AS_CHUCHU_COLOURS}},
    {"c_blue", 0, 0, 1, {AS_CHUCHU_COLOURS}},
    {"c_black", 0, 0, 1, {AS_CHUCHU_COLOURS}},
    {"c_kiiro", 0, 0, 1, {AS_CHUCHU_COLOURS}},
    /* behaviour 0 or 4, armour colour 0..5, equipment (angle.x bits 5-7, d_a_tn.cpp create:
     * 0 none, 1 helmet, 2 shield, 3 helmet and shield, 4 shield and cape, 5 all three) */
    {"Tn", 0, 0, 3, {{AS_WORD_PARAMS, 0, 2, 0xF, {0, 4}},
                     {AS_WORD_PARAMS, 4, 6, 0xF, {0, 1, 2, 3, 4, 5}},
                     {AS_WORD_ANGLE_X, 5, 6, 0x7, {0, 1, 2, 3, 4, 5}}}},
    /* mode 1: the lantern Moblin of the Forsaken Fortress, its own darker model and its lantern
     * (d_a_mo2.cpp createHeap, kantera_get_init); 0 and 0xFF the ordinary one */
    {"mo2", 0, 0, 1, {{AS_WORD_PARAMS, 0, 3, 0xFF, {0, 1, 0xFF}}}},
    /* type 3: the ordinary Wizzrobe in its other colour (d_a_wz.cpp: brk frame 3, behaviour 0);
     * 1 and 2 summon from a room path, 12 and 13 are door scenes: left as placed */
    {"wiz_r", 0, 0, 1, {{AS_WORD_PARAMS, 0, 3, 0xFF, {0, 3, 0xFF}}}},
    /* the land Peahat only (type byte 0xFF; 1 is the Seahat): its notice and chase ranges in
     * hundreds of units (d_a_ph.cpp create, 0xFF the defaults 1000 and 500) */
    {"p_hat", 0xFF, 0xFF, 2, {{AS_WORD_PARAMS, 8, 4, 0xFF, {0xFF, 0x05, 0x0F, 0x1E}},
                              {AS_WORD_PARAMS, 16, 4, 0xFF, {0xFF, 0x05, 0x0F, 0x1E}}}},
    {"keeth", 0, 0, 1, {{AS_WORD_PARAMS, 15, 2, 0x1, {0, 1}}}},
    {"Fkeeth", 0, 0, 1, {{AS_WORD_PARAMS, 15, 2, 0x1, {0, 1}}}},
    {"Oq", 0, 0, 1, {{AS_WORD_PARAMS, 8, 3, 0xFF, {0, 1, 0xFF}}}},
    /* the pig's colour (d_a_kb.cpp), not Outset's big pig candidates */
    {"Pig", 0x0000FF00, 0x0000FF00, 1, {{AS_WORD_PARAMS, 0, 4, 0xF, {0, 1, 2, 3}}}},
    /* the regular Mothula (d_a_gm.cpp create): parameter byte 2 1 on the ground without wings
     * ("gmos_g", wing flags 15, health 2), 2 flying ("gmos_f"); 0 and 0xFF are the mini-boss and
     * byte 1 not 0 (0xFF counts as 0) a larva on a wall: those are left as placed */
    {"gmos", 0x00FFFF00, 0x00010000, 1, {{AS_WORD_PARAMS, 16, 2, 0xFF, {1, 2}}}},
    {"gmos", 0x00FFFF00, 0x00020000, 1, {{AS_WORD_PARAMS, 16, 2, 0xFF, {1, 2}}}},
    {"gmos", 0x00FFFF00, 0x0001FF00, 1, {{AS_WORD_PARAMS, 16, 2, 0xFF, {1, 2}}}},
    {"gmos", 0x00FFFF00, 0x0002FF00, 1, {{AS_WORD_PARAMS, 16, 2, 0xFF, {1, 2}}}},
};

static u32 as_hash(const u8* data, u32 size, u32 seed) {
    u32 h = 2166136261u ^ seed; /* FNV-1a */
    for (u32 i = 0; i != size; ++i) h = (h ^ data[i]) * 16777619u;
    return h;
}

static u32 as_ld32_block(const u8* b, u32 at) {
    return ((u32)b[at] << 24) | ((u32)b[at + 1] << 16) | ((u32)b[at + 2] << 8) | b[at + 3];
}

/* Gives a block (parameters and angle.x) listed variants, field by field, drawn from original's
 * bytes and the copy number. The hash leaves out gbaName and argument, which dStage_actorCreate
 * writes (a block before it and one after it give the same). block may be original. */
static void as_randomize(const char* name, const u8* original, u32 copy, u8* block) {
    u8 key[AS_PRM_PADDING_START];
    memcpy(key, original, sizeof(key));
    key[AS_PRM_GBA_NAME] = 0;
    key[AS_PRM_ARGUMENT] = 0;
    u32 words[2];
    words[AS_WORD_PARAMS] = as_ld32_block(block, 0);
    words[AS_WORD_ANGLE_X] = ((u32)block[AS_PRM_ANGLE] << 8) | block[AS_PRM_ANGLE + 1];
    for (u32 e = 0; e != sizeof(as_variants) / sizeof(as_variants[0]); ++e) {
        const as_enemy_variants* v = &as_variants[e];
        u32 i = 0;
        while (i != MP_ACTOR_NAME_MAX && v->name[i] && name[i] == v->name[i]) ++i;
        if (!(i == MP_ACTOR_NAME_MAX ? v->name[i] == 0 : (v->name[i] == 0 && name[i] == 0))) continue;
        /* an enemy may have several entries for several required values (gmos) */
        if ((words[AS_WORD_PARAMS] & v->require_mask) != v->require_value) continue;
        for (u32 f = 0; f != v->count; ++f) {
            const as_variant_field* field = &v->fields[f];
            u32* word = &words[field->word];
            u32 value = (*word >> field->shift) & field->mask;
            int listed = 0;
            for (u32 k = 0; k != field->count; ++k) listed |= field->values[k] == value;
            if (!listed) continue; /* a scripted or special value stays */
            u32 pick = as_hash(key, sizeof(key), copy * 31u + f) % field->count;
            *word = (*word & ~(field->mask << field->shift)) | (field->values[pick] << field->shift);
        }
        for (u32 b = 0; b != 4; ++b) block[b] = (u8)(words[AS_WORD_PARAMS] >> (24 - 8 * b));
        block[AS_PRM_ANGLE] = (u8)(words[AS_WORD_ANGLE_X] >> 8);
        block[AS_PRM_ANGLE + 1] = (u8)words[AS_WORD_ANGLE_X];
        return;
    }
}

/* cos/sin of k * 45 degrees */
static const f32 as_ring_x[8] = {1.0f, 0.7071f, 0.0f, -0.7071f, -1.0f, -0.7071f, 0.0f, 0.7071f};
static const f32 as_ring_z[8] = {0.0f, 0.7071f, 1.0f, 0.7071f, 0.0f, -0.7071f, -1.0f, -0.7071f};

WWHD_GAME_ORIGINAL(0x025C1154, void, as_stage_create_original, (u32 data, u32 prm));
WWHD_GAME_ORIGINAL(0x025D4C70, s32, as_create_original, (void* actor));
WWHD_GAME_ORIGINAL(0x025D4B4C, s32, as_delete_original, (void* actor));

static void as_init_tables(void) {
    if (as_ready_init) return;
    for (u32 i = 0; i != AS_BUCKETS; ++i) as_buckets[i] = AS_NONE;
    as_ready_init = 1;
}

static u32 as_bucket(u32 actor) {
    return (actor >> 4) & (AS_BUCKETS - 1);
}

static void as_free(as_slot* s) {
    u16 index = (u16)(s - as_slots);
    u16* link = &as_buckets[as_bucket(s->actor)];
    while (*link != AS_NONE && *link != index) link = &as_slots[*link].next;
    if (*link == index) *link = s->next;
    s->used = 0;
}

/* The slot of a live actor; a slot whose process id no longer matches (the memory holds another
 * process) is dropped. */
static as_slot* as_find(const void* actor) {
    if (!as_ready_init) return 0;
    u16 i = as_buckets[as_bucket((u32)actor)];
    while (i != AS_NONE) {
        as_slot* s = &as_slots[i];
        u16 next = s->next;
        if (s->actor == (u32)actor) {
            if (*(const volatile u32*)((const u8*)actor + AS_PC_ID) == s->pcid) return s;
            as_free(s);
            return 0;
        }
        i = next;
    }
    return 0;
}

static int as_name_is(const char* name, const char* wanted) {
    u32 i = 0;
    while (i != MP_ACTOR_NAME_MAX && wanted[i] && name[i] == wanted[i]) ++i;
    return i == MP_ACTOR_NAME_MAX ? wanted[i] == 0 : (wanted[i] == 0 && name[i] == 0);
}

static int as_is_enemy(const char* name) {
    for (u32 e = 0; e != sizeof(as_enemies) / sizeof(as_enemies[0]); ++e)
        if (as_name_is(name, as_enemies[e])) return 1;
    return 0;
}

static void as_read_values(const fopAc_ac_c* a, mp_actor_values* v) {
    v->has_transform = v->has_health = 1;
    v->transform.pos[0] = a->current.pos.x;
    v->transform.pos[1] = a->current.pos.y;
    v->transform.pos[2] = a->current.pos.z;
    v->transform.rot[0] = a->shape_angle.x;
    v->transform.rot[1] = a->shape_angle.y;
    v->transform.rot[2] = a->shape_angle.z;
    v->health = a->health;
    v->tag = el_tag_of(a); /* the owner's elite draw rides with the health (elite.h) */
    v->status = a->actor_status;
}

static int as_send(mp_u32 id, const mp_writer* w) {
    return id != MP_NO_ID && send_payload(id, w);
}

static void as_send_uid(mp_u32 id, mp_u32 uid) {
    mp_writer w = mp_writer_make(as_payload, sizeof(as_payload));
    mp_write_actor_uid(&w, uid);
    as_send(id, &w);
}

/* ------------------------------------------------------------------ the game's hooks */

/* A block given to dStage_actorCreate, found again in fopAc_Create (as_track_enemy). */
static void as_note_add(u32 prm, u32 group, u32 copy, u32 made, int randomized, const char* name) {
    as_note* n = &as_notes[as_note_next++ & (AS_MAX_NOTES - 1)];
    n->prm = prm;
    n->group = group;
    n->copy = (u8)copy;
    n->made = (u8)made;
    n->randomized = (u8)randomized;
    for (u32 i = 0; i != AS_STAGE_NAME; ++i) n->name[i] = name[i];
}

/* Copy number i of a placed enemy: its own block (fopAcM_CreateAppend, the stage loaders'
 * allocator), on its ring, with its variant; 0 when the game heap is exhausted. */
static u32 as_make_copy(const char* name, const u8* original, u32 i, int randomized, u32 group) {
    u32 copy = wwhd_fopAcM_CreateAppend();
    if (!copy) return 0;
    memcpy((void*)copy, original, AS_PRM_BYTES);
    f32* position = (f32*)(copy + AS_PRM_POSITION);
    f32 radius = AS_SPREAD * (f32)(1 + (i - 1) / 8);
    position[0] += radius * as_ring_x[(i - 1) & 7];
    position[2] += radius * as_ring_z[(i - 1) & 7];
    if (randomized) as_randomize(name, original, i, (u8*)copy);
    *(u8*)(copy + AS_PRM_TAG) = AS_TAG;
    *(u8*)(copy + AS_PRM_COPY) = (u8)i;
    if (group) as_note_add(copy, group, i, 0, randomized, name);
    if ((++as_copies & 63) == 1) wwhd_log_int("multiplayer: enemy copies created", (int)as_copies);
    return copy;
}

/* ---- the stage's actors and how their creation went (world_sync.h: day and night together) ----
 * Every block given to dStage_actorCreate (the stage's own actors and the multiplier's copies) is
 * noted with its stage, name and bytes (before dStage_actorCreate writes argument and gbaName,
 * which it writes again when asked again); fopAc_Create of the actor reading that block
 * (mpUserData) gives the result: cPhs_COMPLEATE_e 4 made, cPhs_ERROR_e 5 refused (an actor of the
 * other time of day, a switch, an event...). Only the actors whose creation asked the time of day
 * (dKy_daynight_check 02556D14 called while fopAc_Create runs for their block: the people of
 * Windfall, d_a_npc_*.cpp init_*) are "timed"; as_roster_retry asks again for the refused timed
 * ones of the stage and room being played, once per placed actor. The others (switches, events,
 * collected things...) are never asked again: made out of their context, some crash the game
 * (0.2.45: 176 actors asked again, a crash in 027E6664 when going back from night to day). */
#define AS_ROSTER 384
#define AS_PHASE_DONE 4
#define AS_PHASE_ERROR 5
typedef struct as_roster_entry {
    char stage[8];
    char name[8];
    u8 block[AS_PRM_BYTES];
    u32 address;          /* the block while its actor is being made */
    u8 state;             /* 0 free, 1 being made, 2 made, 3 refused */
    u8 timed;             /* its creation asked dKy_daynight_check */
} as_roster_entry;
static as_roster_entry as_roster[AS_ROSTER];
static u32 as_roster_next;
static u32 as_roster_creating;   /* the block of the fopAc_Create running, 0 outside */

/* dKy_daynight_check (decomp bindings.h, no argument), before it runs: the actor being created
 * depends on the time of day. */
WWHD_HOOK(0x02556D14, as_roster_daynight, (void)) {
    u32 prm = as_roster_creating;
    if (!prm) return;
    for (u32 i = 0; i != AS_ROSTER; ++i)
        if (as_roster[i].state == 1 && as_roster[i].address == prm) {
            as_roster[i].timed = 1;
            return;
        }
}

static void as_roster_note(u32 data, u32 prm) {
    const u8* play = wwhd_play_get();
    if (!play || !data || !prm) return;
    as_roster_entry* e = &as_roster[as_roster_next++ % AS_ROSTER];
    for (u32 i = 0; i != 8; ++i) {
        e->stage[i] = (char)play[WWHD_PLAY_START_STAGE_NAME_OFFSET + i];
        e->name[i] = ((const char*)data)[i];
    }
    memcpy(e->block, (const void*)prm, AS_PRM_BYTES);
    e->address = prm;
    e->state = 1;
    e->timed = 0;
}

static void as_roster_result(u32 prm, s32 result) {
    if (!prm || (result != AS_PHASE_DONE && result != AS_PHASE_ERROR)) return;
    for (u32 i = 0; i != AS_ROSTER; ++i) {
        as_roster_entry* e = &as_roster[i];
        if (e->state != 1 || e->address != prm) continue;
        e->state = result == AS_PHASE_DONE ? 2 : 3;
        e->address = 0;
        return;
    }
}

/* Placed actors: tag every parameter block, and create the server's number of copies of the
 * listed enemies. dStage_actorCreate takes ownership of the block (frees it or hands it to
 * fpcSCtRq_Request), so each copy gets its own. Copies keep the original's parameters (unless the
 * randomizer gives them a variant) and setID. The enemies are remembered for the live mutators,
 * also with no copies or no connection. */
WWHD_REPLACE(0x025C1154, void, as_stage_create, (u32 data, u32 prm)) {
    if (!data || !prm) {
        as_stage_create_original(data, prm);
        return;
    }
    int enemy = as_enabled && as_is_enemy((const char*)data);
    u32 copies = 1;
    int randomized = enemy && as_session && as_session->enemy_randomizer;
    if (enemy && as_session && as_session->enemy_multiplier > 1) copies = as_session->enemy_multiplier;
    u32 group = enemy ? as_group_next++ : 0;
    u32 i = 1;
    for (; i < copies; ++i) {
        u32 copy = as_make_copy((const char*)data, (const u8*)prm, i, randomized, group);
        if (!copy) break; /* game heap exhausted: keep what was created */
        as_roster_note(data, copy);
        as_stage_create_original(data, copy);
    }
    *(u8*)(prm + AS_PRM_TAG) = AS_TAG;
    *(u8*)(prm + AS_PRM_COPY) = 0;
    if (group) as_note_add(prm, group, 0, i, randomized, (const char*)data);
    as_roster_note(data, prm);
    as_stage_create_original(data, prm);
}

static int as_alive(u32 actor, u32 pcid) {
    return actor && *(const volatile u32*)((const u8*)actor + AS_PC_ID) == pcid;
}

/* fopAc_Create of a placed enemy (original or copy): remembered with its note. */
static void as_track_enemy(void* actor, u32 prm) {
    const u8* block = (const u8*)prm;
    if (block[AS_PRM_TAG] != AS_TAG) return;
    for (u32 k = 0; k != AS_MAX_NOTES; ++k) {
        as_note* n = &as_notes[(as_note_next - 1 - k) & (AS_MAX_NOTES - 1)];
        if (n->prm != prm || !n->group) continue;
        u32 pcid = *(const volatile u32*)((const u8*)actor + AS_PC_ID);
        n->prm = 0;
        if (n->copy == 0) {
            for (u32 g = 0; g != AS_MAX_GROUPS; ++g) {
                as_group* e = &as_groups[g];
                if (e->used && as_alive(e->actor, e->pcid)) continue;
                e->used = 1;
                e->actor = (u32)actor;
                e->pcid = pcid;
                e->id = n->group;
                e->made = n->made;
                e->randomized = n->randomized;
                for (u32 i = 0; i != AS_STAGE_NAME; ++i) e->name[i] = n->name[i];
                memcpy(e->prm, block, AS_PRM_BYTES);
                return;
            }
        } else {
            for (u32 c = 0; c != AS_MAX_ENEMY_COPIES; ++c) {
                as_copy_entry* e = &as_enemy_copies[c];
                if (e->used && as_alive(e->actor, e->pcid)) continue;
                e->used = 1;
                e->actor = (u32)actor;
                e->pcid = pcid;
                e->group = n->group;
                e->copy = n->copy;
                return;
            }
        }
        return;
    }
}

static void as_untrack_enemy(const void* actor) {
    for (u32 g = 0; g != AS_MAX_GROUPS; ++g)
        if (as_groups[g].used && as_groups[g].actor == (u32)actor) as_groups[g].used = 0;
    for (u32 c = 0; c != AS_MAX_ENEMY_COPIES; ++c)
        if (as_enemy_copies[c].used && as_enemy_copies[c].actor == (u32)actor) as_enemy_copies[c].used = 0;
}

static as_slot* as_alloc(void) {
    for (u32 i = 0; i != AS_MAX; ++i)
        if (!as_slots[i].used) {
            as_slot* s = &as_slots[i];
            as_slot blank = {0};
            *s = blank;
            s->used = 1;
            s->uid = MP_NO_ID;
            s->solver = MP_NO_ID;
            s->ticket = 0xFF;
            return s;
        }
    static int full_logged;
    if (!full_logged++) wwhd_log("multiplayer: synchronised actor table full, the rest stay local");
    return 0;
}

static void as_link_bucket(as_slot* s, u32 actor) {
    as_init_tables();
    s->actor = actor;
    s->pcid = *(const volatile u32*)((const u8*)actor + AS_PC_ID);
    u16 index = (u16)(s - as_slots);
    u16* head = &as_buckets[as_bucket(actor)];
    s->next = *head;
    *head = index;
}

/* The "Spawned" factory itself (mp_find_factory would fall back to "Default"). */
static mp_u32 as_spawn_factory(void) {
    if (!as_session) return MP_NO_ID;
    for (u32 i = 0; i != as_session->factory_count; ++i)
        if (mp_name_equals(as_session->factories[i].name, MP_SPAWN_FACTORY))
            return as_session->factories[i].usable ? i : MP_NO_ID;
    return MP_NO_ID;
}

static u8 as_spawn_class(s32 proc) {
    if (proc == AS_PROC_ITEM) return AS_CLS_ITEM;
    if (proc == AS_PROC_IBALL) return AS_CLS_BALL;
    if (proc == AS_PROC_BOMB || proc == AS_PROC_BOMB2 || proc == AS_PROC_ARROW || proc == AS_PROC_BOOMERANG ||
        proc == AS_PROC_HOOKSHOT || proc == AS_PROC_ROPE)
        return AS_CLS_WEAPON;
    return AS_CLS_NONE;
}

static const char* as_spawn_name(s32 proc) {
    if (proc == AS_PROC_ITEM) return "item";
    if (proc == AS_PROC_BOMB) return "Bomb";
    if (proc == AS_PROC_BOMB2) return "Bomb2";
    if (proc == AS_PROC_ARROW) return "Arrow";
    if (proc == AS_PROC_IBALL) return "Iball";
    if (proc == AS_PROC_HOOKSHOT) return "Hookshot";
    if (proc == AS_PROC_ROPE) return "Himo2";
    return "Boomrang";
}

static void as_ctx_add(as_ctx_entry* table, u32* count, u32 capacity, void* actor, u8 ctx) {
    u32 pcid = *(const volatile u32*)((const u8*)actor + AS_PC_ID);
    for (u32 i = 0; i != *count; ++i)
        if (table[i].actor == (u32)actor) {
            table[i].pcid = pcid;
            table[i].ctx = ctx;
            return;
        }
    if (*count == capacity) return;
    table[*count].actor = (u32)actor;
    table[*count].pcid = pcid;
    table[*count].ctx = ctx;
    ++*count;
}

/* The entry of a live actor (its process id still matches), 0 when none. */
static as_ctx_entry* as_ctx_find(as_ctx_entry* table, u32* count, const void* actor) {
    for (u32 i = 0; i != *count; ++i) {
        if (table[i].actor != (u32)actor) continue;
        if (table[i].pcid == *(const volatile u32*)((const u8*)actor + AS_PC_ID)) return &table[i];
        table[i] = table[--*count]; /* stale: another process at that address */
        return 0;
    }
    return 0;
}

static void as_ctx_drop(as_ctx_entry* table, u32* count, const void* actor) {
    for (u32 i = 0; i != *count; ++i)
        if (table[i].actor == (u32)actor) {
            table[i] = table[--*count];
            return;
        }
}

/* A spawned actor made in a LOCAL or OWNED context: announced at the next step. */
static void as_register_spawned(void* actor, s32 proc, u8 cls, u32 prm) {
    mp_u32 factory = as_spawn_factory();
    if (factory == MP_NO_ID) return;
    as_slot* s = as_alloc();
    if (!s) return;
    s->kind = AS_KIND_SPAWNED;
    s->cls = cls;
    s->state = AS_LOCAL;
    s->factory = (u8)factory;
    memcpy(s->prm, (const void*)prm, AS_PRM_BYTES);
    const char* name = as_spawn_name(proc);
    for (u32 i = 0; i != MP_ACTOR_NAME_MAX && name[i]; ++i) s->name[i] = name[i];
    s->spawn.proc = (mp_u16)proc;
    as_link_bucket(s, (u32)actor);
}

static void as_register(void* actor, u32 prm) {
    const u8* block = (const u8*)prm;
    if (block[AS_PRM_TAG] != AS_TAG) return;
    s16 proc = *(const volatile s16*)((const u8*)actor + AS_PROC_NAME);
    u32 text = wwhd_dStage_getName2((u32)(s32)proc, (u32)(s32)(s8)block[AS_PRM_ARGUMENT]);
    if (!text) return;
    char name[MP_ACTOR_NAME_MAX + 1];
    for (u32 i = 0; i != MP_ACTOR_NAME_MAX + 1; ++i) name[i] = 0;
    for (u32 i = 0; i != MP_ACTOR_NAME_MAX && ((const char*)text)[i]; ++i) name[i] = ((const char*)text)[i];
    if (!mp_filter_contains(as_session, name)) return;
    mp_u32 factory = mp_find_factory(as_session, name);
    if (factory == MP_NO_ID || !as_session->factories[factory].usable) return;
    as_slot* s = 0;
    for (u32 i = 0; i != AS_MAX; ++i)
        if (!as_slots[i].used) {
            s = &as_slots[i];
            break;
        }
    if (!s) {
        static int full_logged;
        if (!full_logged++) wwhd_log("multiplayer: synchronised actor table full, the rest stay local");
        return;
    }
    as_init_tables();
    as_slot blank = {0};
    *s = blank;
    s->used = 1;
    s->state = AS_LOCAL;
    s->actor = (u32)actor;
    s->pcid = *(const volatile u32*)((const u8*)actor + AS_PC_ID);
    s->factory = (u8)factory;
    s->uid = MP_NO_ID;
    s->solver = MP_NO_ID;
    s->ticket = 0xFF;
    memcpy(s->prm, block, AS_PRM_BYTES);
    memcpy(s->name, name, sizeof(name));
    u16 index = (u16)(s - as_slots);
    u16* head = &as_buckets[as_bucket(s->actor)];
    s->next = *head;
    *head = index;
}

/* A weapon copy runs (create and execute) with its creator's Link puppet as player 0 and link
 * player; the values are given back unless the game changed them meanwhile. */
typedef struct as_player_swap {
    volatile u32* player0;
    volatile u32* link;
    u32 old_player0, old_link, puppet;
} as_player_swap;

static void as_swap_in(const as_slot* s, as_player_swap* w) {
    w->player0 = w->link = 0;
    if (!s || s->kind != AS_KIND_SPAWNED || !s->replica || s->cls != AS_CLS_WEAPON) return;
    puppet_slot* ps = puppet_slot_for(s->creator, 0);
    u8* play = wwhd_play_get();
    if (!ps || !ps->actor || !play) return;
    w->puppet = ps->actor;
    w->player0 = (volatile u32*)(play + AS_PLAY_PLAYER0);
    w->link = (volatile u32*)(play + AS_PLAY_LINK);
    w->old_player0 = *w->player0;
    w->old_link = *w->link;
    *w->player0 = w->puppet;
    *w->link = w->puppet;
}

static void as_swap_out(const as_player_swap* w) {
    if (!w->player0) return;
    if (*w->player0 == w->puppet) *w->player0 = w->old_player0;
    if (*w->link == w->puppet) *w->link = w->old_link;
}

/* A copy created by fopAcM_create in as_step: found by the process id the request returned. */
static int as_bind_replica(void* actor) {
    u32 pcid = *(const volatile u32*)((const u8*)actor + AS_PC_ID);
    for (u32 i = 0; i != AS_MAX; ++i) {
        as_slot* s = &as_slots[i];
        if (!s->used || s->state != AS_SPAWNING || !s->requested || s->request_id != pcid) continue;
        as_link_bucket(s, (u32)actor);
        s->state = AS_SYNCED;
        if (!as_replica_logged++) wwhd_log_int("multiplayer: first spawned actor of another player, process", (int)s->spawn.proc);
        return 1;
    }
    return 0;
}

/* A block made in a creation context (as_create_append). */
static void as_register_runtime(void* actor, u32 prm) {
    const u8* block = (const u8*)prm;
    if (block[AS_PRM_TAG] != AS_RT_TAG) return;
    u8 ctx = block[AS_PRM_COPY];
    s32 proc = *(const volatile s16*)((const u8*)actor + AS_PROC_NAME);
    u8 cls = as_spawn_class(proc);
    if (cls != AS_CLS_NONE && as_spawn_factory() == MP_NO_ID) return; /* a server without "Spawned" */
    if ((cls == AS_CLS_ITEM || cls == AS_CLS_BALL) && ctx == AS_CTX_FOREIGN) {
        as_ctx_add(as_kill, &as_kill_count, AS_MAX_KILL, actor, ctx); /* the owner's item comes instead */
        if (!as_suppressed_logged++) wwhd_log("multiplayer: a drop of another player's enemy replaced by theirs");
        return;
    }
    /* items of the local Link or of an owned enemy; weapons of the local Link only (an enemy's
     * bomb stays in each game, where it can hurt that game's Link) */
    if (((cls == AS_CLS_ITEM || cls == AS_CLS_BALL) && (ctx == AS_CTX_LOCAL || ctx == AS_CTX_OWNED)) ||
        (cls == AS_CLS_WEAPON && ctx == AS_CTX_LOCAL)) {
        as_register_spawned(actor, proc, cls, prm);
        return;
    }
    if (cls == AS_CLS_NONE && (ctx == AS_CTX_OWNED || ctx == AS_CTX_FOREIGN))
        as_ctx_add(as_inherit, &as_inherit_count, AS_MAX_INHERIT, actor, ctx); /* e.g. the death puff */
}

/* The actors' memory, with the enemy multiplier. fopAcM_entrySolidHeap (025D63E8: actor, create-heap
 * callback, estimate; f_op_actor_mng_2.cpp) ignores the estimate in HD: it calls
 * fopAcM_entrySolidHeap_ (025D6174: actor, callback, 0, parent heap; f_op_actor_mng.cpp), whose solid
 * heap takes the parent's whole largest free block while the actor builds its models and is then cut
 * to what it used. The parent is mDoExt_getHeap(1) (025E2F64) on the sea or when stageHeapFlags
 * (1047E6C0) bit 0 is off, else mDoExt_getHeap(0); a few actors take the other one. When that block is
 * too small the actor's own code writes through an allocation that failed (a Moblin's line material,
 * crash-20261018-161706.log; a copy's weapon, daBoko_c::createHeap 020C1DA8, -171223/-202101.log).
 * So the size each actor kind really used is kept (its solid heap at fopAc_ac_c + 0xF4, getHeapSize
 * vtable +0x6C; the largest free block is getFreeSize, vtable +0x74, the heap's vtable at +0xC:
 * story_lyt_screen.cpp). A copy goes into whichever of the two actors' heaps has the largest block,
 * refused only when neither keeps room for it and AS_HEAP_KEEP more (the game's own failure: its create
 * returns cPhs_ERROR_e and the game deletes it, never asked again: as_roster). Any other actor whose
 * heap has no block for its known size goes into the other heap, and is refused only when neither has
 * one, instead of the crash. */
#define AS_HEAP_KEEP (1u << 20)        /* room a copy leaves for the stage's own actors */
#define AS_HEAP_UNKNOWN_COPY (1u << 20) /* a copy of a kind not built yet */
#define AS_HEAP_UNKNOWN (256u << 10)
#define AS_HEAP_VTABLE 0x0C
#define AS_HEAP_VT_SIZE 0x6C
#define AS_HEAP_VT_FREE_BLOCK 0x74
#define AS_ACTOR_HEAP 0xF4
#define AS_HEAP_KINDS 128
WWHD_GAME_ORIGINAL(0x025D63E8, s32, as_solid_heap_original, (u32 actor, u32 callback, u32 size));
WWHD_GAME_FUNC(0x025D6174, s32, as_entry_solid_heap, (u32 actor, u32 callback, u32 size, u32 parent));
WWHD_GAME_FUNC(0x025E2F64, u32, as_game_heap, (s32 index));
typedef u32 (*as_heap_query_fn)(u32 heap);
static u32 as_heap_refused, as_heap_refused_other, as_heap_moved;
static s16 as_kind_proc[AS_HEAP_KINDS];
static u32 as_kind_size[AS_HEAP_KINDS];
static u32 as_kind_count;

static u32 as_heap_query(u32 heap, u32 slot) {
    u32 vtable = heap ? *(const volatile u32*)(heap + AS_HEAP_VTABLE) : 0;
    as_heap_query_fn fn = vtable ? (as_heap_query_fn) * (const volatile u32*)(vtable + slot) : 0;
    return fn ? fn(heap) : 0;
}

/* The heap the game takes for most actors (first) and the other one. */
static void as_actor_heaps(u32* first, u32* other) {
    const u8* play = wwhd_play_get();
    int sea = play && play[WWHD_PLAY_START_STAGE_NAME_OFFSET] == 's' && play[WWHD_PLAY_START_STAGE_NAME_OFFSET + 1] == 'e' &&
              play[WWHD_PLAY_START_STAGE_NAME_OFFSET + 2] == 'a' && !play[WWHD_PLAY_START_STAGE_NAME_OFFSET + 3];
    u32 flags = WWHD_GAME_DATA(0x1047E6C0, u32);
    s32 index = sea || !(flags & 1u) ? 1 : 0;
    *first = as_game_heap(index);
    *other = as_game_heap(1 - index);
}

static u32 as_kind_known(s16 proc) {
    for (u32 i = 0; i != as_kind_count; ++i)
        if (as_kind_proc[i] == proc) return as_kind_size[i];
    return 0;
}

static void as_kind_note(u32 actor, s16 proc) {
    u32 heap = *(const volatile u32*)(actor + AS_ACTOR_HEAP);
    u32 used = heap ? as_heap_query(heap, AS_HEAP_VT_SIZE) : 0;
    if (!used) return;
    for (u32 i = 0; i != as_kind_count; ++i)
        if (as_kind_proc[i] == proc) {
            if (used > as_kind_size[i]) as_kind_size[i] = used;
            return;
        }
    if (as_kind_count == AS_HEAP_KINDS) return;
    as_kind_proc[as_kind_count] = proc;
    as_kind_size[as_kind_count++] = used;
}

static void as_heap_log_refusal(const char* what, s16 proc, u32 need, u32 largest) {
    wwhd_log_int(what, proc);
    wwhd_log_int("multiplayer:   room needed", (int)need);
    wwhd_log_int("multiplayer:   largest free block", (int)largest);
}

WWHD_REPLACE(0x025D63E8, s32, as_solid_heap, (u32 actor, u32 callback, u32 size)) {
    if (!as_enabled || !actor) return as_solid_heap_original(actor, callback, size);
    u32 first, other;
    as_actor_heaps(&first, &other);
    if (!first) return as_solid_heap_original(actor, callback, size);
    s16 proc = *(const volatile s16*)(actor + AS_PROC_NAME);
    u32 known = as_kind_known(proc);
    u32 need = known ? known + known / 4u + 0x10000u : 0;
    u32 l_first = as_heap_query(first, AS_HEAP_VT_FREE_BLOCK);
    u32 l_other = other && other != first ? as_heap_query(other, AS_HEAP_VT_FREE_BLOCK) : 0;
    const u8* block = (const u8*)as_roster_creating;
    s32 ret;
    if (block && block[AS_PRM_TAG] == AS_TAG && block[AS_PRM_COPY] != 0 && block[AS_PRM_COPY] != 0xFE) {
        if (!known) need = AS_HEAP_UNKNOWN_COPY;
        u32 best = l_other > l_first ? other : first;
        u32 largest = best == first ? l_first : l_other;
        if (largest < need + AS_HEAP_KEEP) {
            if ((as_heap_refused++ & 31) == 0) {
                as_heap_log_refusal("multiplayer: enemy copy not made, no room in the actors' heaps (proc)", proc, need,
                                    largest);
                wwhd_log_int("multiplayer:   copies refused so far", (int)as_heap_refused);
            }
            return 0;
        }
        ret = as_entry_solid_heap(actor, callback, 0, best);
    } else {
        if (!known) need = AS_HEAP_UNKNOWN;
        if (l_first >= need) {
            ret = as_solid_heap_original(actor, callback, size);
        } else if (l_other >= need && l_other > l_first) {
            if ((as_heap_moved++ & 15) == 0) wwhd_log_int("multiplayer: actor built in the other actors' heap (proc)", proc);
            ret = as_entry_solid_heap(actor, callback, 0, other);
        } else if (!known) {
            ret = as_solid_heap_original(actor, callback, size); /* its size is not known: the game tries */
        } else {
            if ((as_heap_refused_other++ & 15) == 0)
                as_heap_log_refusal("multiplayer: actor not made, no room in the actors' heaps (proc)", proc, need,
                                    l_first > l_other ? l_first : l_other);
            return 0;
        }
    }
    if (ret) as_kind_note(actor, proc);
    return ret;
}
/* fopAc_Create: the block is read before the original runs, on the first call only. */
WWHD_REPLACE(0x025D4C70, s32, as_create, (void* actor)) {
    if (actor && as_enabled && as_session && as_session->factories_complete &&
        *(const volatile s8*)((const u8*)actor + AS_INIT_STATE) == 0) {
        u32 prm = *(const volatile u32*)((const u8*)actor + AS_USER_DATA);
        if (!as_bind_replica(actor) && prm) {
            as_register(actor, prm);
            as_register_runtime(actor, prm);
        }
    }
    if (actor && *(const volatile s8*)((const u8*)actor + AS_INIT_STATE) == 0) {
        u32 prm = *(const volatile u32*)((const u8*)actor + AS_USER_DATA);
        if (prm) {
            as_track_enemy(actor, prm); /* live mutators, also before the connection */
            el_note_create(actor, prm); /* dynamic scaling and elites (elite.h) */
        }
    }
    as_player_swap swap;
    as_swap_in(actor ? as_find(actor) : 0, &swap); /* a weapon copy's create reads the player too */
    u32 user = actor ? *(const volatile u32*)((const u8*)actor + AS_USER_DATA) : 0;
    u32 outer = as_roster_creating;
    as_roster_creating = user;
    s32 result = as_create_original(actor);
    as_roster_creating = outer;
    as_swap_out(&swap);
    as_roster_result(user, result);
    return result;
}

WWHD_GAME_ORIGINAL(0x025D5600, u32, as_create_append_original, (void));

/* fopAcM_CreateAppend: the creation context goes into the new block's padding. */
WWHD_REPLACE(0x025D5600, u32, as_create_append, (void)) {
    u32 prm = as_create_append_original();
    if (prm && as_context != AS_CTX_NONE && !as_spawning) {
        *(volatile u8*)(prm + AS_PRM_TAG) = AS_RT_TAG;
        *(volatile u8*)(prm + AS_PRM_COPY) = as_context;
    }
    return prm;
}

static void as_on_spawned_deleted(as_slot* s, const fopAc_ac_c* a) {
    if (s->state != AS_SYNCED || s->uid == MP_NO_ID || !as_session) return;
    if (s->cls == AS_CLS_ITEM) {
        f32 dx = a->current.pos.x - as_link_pos[0], dy = a->current.pos.y - as_link_pos[1];
        f32 dz = a->current.pos.z - as_link_pos[2];
        if (as_have_link && dx * dx + dy * dy + dz * dz < AS_PICKUP_DISTANCE * AS_PICKUP_DISTANCE)
            as_send_uid(as_session->client_remove, s->uid); /* picked up here: gone for everyone */
        return;
    }
    if (s->replica) return; /* a copy ends on its own (a ball broken by a replayed hit, a weapon) */
    if (s->exclusive) as_send_uid(as_session->client_exclusive_remove, s->uid);
    as_send_uid(as_session->client_remove, s->uid);
}

static u8 as_context_of(const void* actor, const as_slot* s) {
    if (as_link && (u32)actor == as_link) return AS_CTX_LOCAL;
    const u8* play = wwhd_play_get();
    if (play && (u32)actor == *(const volatile u32*)(play + AS_PLAY_SHIP)) return AS_CTX_LOCAL; /* its cannon */
    if (s) {
        if (s->kind == AS_KIND_SPAWNED) return s->replica ? AS_CTX_FOREIGN : AS_CTX_OWNED;
        if (s->state != AS_SYNCED || s->group != AS_GROUP_ENEMY || !s->owner) return AS_CTX_NONE;
        return s->owner == as_local_uid ? AS_CTX_OWNED : AS_CTX_FOREIGN;
    }
    as_ctx_entry* e = as_inherit_count ? as_ctx_find(as_inherit, &as_inherit_count, actor) : 0;
    return e ? e->ctx : AS_CTX_NONE;
}

static void as_on_deleted(as_slot* s, s8 health) {
    if (s->state != AS_SYNCED || s->uid == MP_NO_ID || !as_session) return;
    int died = s->group == AS_GROUP_ENEMY && s->max_health > 0 && health <= 0;
    int responsible = (as_local_uid != MP_NO_ID && s->owner == as_local_uid) || s->local_hit;
    if (s->broken) { /* a pot broken here: every game breaks its copy (as_pot_damaged) */
        as_send_uid(as_session->client_remove, s->uid);
        return;
    }
    if (!died || !responsible) return;
    as_send_uid(as_session->client_remove, s->uid);
    if (!as_removed_logged++) wwhd_log("multiplayer: a synchronised enemy died, removed for everyone");
}

WWHD_REPLACE(0x025D4B4C, s32, as_delete, (void* actor)) {
    as_slot* s = actor ? as_find(actor) : 0;
    s8 health = actor ? ((const fopAc_ac_c*)actor)->health : 0;
    el_actor* elite = actor ? el_delete_begin(actor) : 0;
    u8 saved = as_context;
    if (actor) as_context = as_context_of(actor, s); /* drops made while dying */
    s32 result = as_delete_original(actor);
    as_context = saved;
    if (result) el_delete_end(elite, health); /* a dead elite explodes (elite.h) */
    if (actor && result) {
        if (as_inherit_count) as_ctx_drop(as_inherit, &as_inherit_count, actor);
        if (as_kill_count) as_ctx_drop(as_kill, &as_kill_count, actor);
        as_untrack_enemy(actor);
    }
    if (s && result) {
        if (s->kind == AS_KIND_SPAWNED) as_on_spawned_deleted(s, (const fopAc_ac_c*)actor);
        else as_on_deleted(s, health);
        as_free(s);
    }
    return result;
}

/* A non-owner shows the owner's values once they come regularly; after its own hit or move it
 * waits AS_HIT_GRACE seconds for the ownership. */
static int as_follows_owner(const as_slot* s) {
    return as_enabled && as_session && !s->free_flight && s->state == AS_SYNCED && s->owner != 0 && s->owner != as_local_uid && s->has_target &&
           s->has_display && s->age < AS_STALE && !(s->local_hit && s->hit_clock < AS_HIT_GRACE);
}

/* An actor its owner carries sits between the hands of the owner's puppet, where
 * daPy_lk_c::setGrabItemPos puts it (d_a_player_main_04.cpp: the middle of mLeftHandPos and
 * mRightHandPos, joints 8 and 12 of mpCLModel), not at its smoothed network place: it moves with
 * the puppet as a puppet moves with its ship. 0 without the puppet (then the network place). */
#define AS_JNT_LEFT_HAND 8
#define AS_JNT_RIGHT_HAND 12
static int as_carried_place(const as_slot* s, f32* out) {
    int carried = s->kind == AS_KIND_PLACED ? s->has_net_health && (s->net_status & 0x2000u)
                                            : s->replica && (s->spawn.variant & 1);
    if (!carried || !s->owner) return 0;
    puppet_slot* p = puppet_slot_for(s->owner, 0);
    if (!p || !p->actor) return 0;
    const daPy_lk_c* link = (const daPy_lk_c*)p->actor;
    if (!link->mpCLModel) return 0;
    const f32* l = puppet_anm_mtx(link->mpCLModel, AS_JNT_LEFT_HAND);
    const f32* r = puppet_anm_mtx(link->mpCLModel, AS_JNT_RIGHT_HAND);
    if (!l || !r) return 0;
    out[0] = (l[3] + r[3]) * 0.5f;
    out[1] = (l[7] + r[7]) * 0.5f;
    out[2] = (l[11] + r[11]) * 0.5f;
    return 1;
}

static void as_place(const as_slot* s, fopAc_ac_c* a) {
    f32 hands[3];
    if (as_carried_place(s, hands)) {
        a->current.pos.x = a->old.pos.x = hands[0];
        a->current.pos.y = a->old.pos.y = hands[1];
        a->current.pos.z = a->old.pos.z = hands[2];
    } else {
        a->current.pos.x = s->display.pos[0];
        a->current.pos.y = s->display.pos[1];
        a->current.pos.z = s->display.pos[2];
    }
    a->shape_angle.x = s->display.rot[0];
    a->shape_angle.y = s->display.rot[1];
    a->shape_angle.z = s->display.rot[2];
}

/* The owner's carry bits on a copy that follows it (see the header). */
static void as_apply_carry(const as_slot* s, fopAc_ac_c* a) {
    u32 bits;
    if (s->kind == AS_KIND_PLACED) {
        if (!s->has_net_health) return;
        bits = s->net_status & AS_CARRY_BITS;
    } else {
        if (!s->replica || s->spawn.proc == AS_PROC_ARROW) return;
        bits = ((s->spawn.variant & 1) ? 0x2000u : 0) | ((s->spawn.variant & 2) ? 0x100000u : 0);
    }
    a->actor_status = (a->actor_status & ~AS_CARRY_BITS) | bits;
}

/* A boomerang copy is thrown as the owner's: daBoomerang_c::procWait (020CF4CC) starts the flight
 * when mParameters is 1, which the owner's Link sets at the throw; procMove sets it back to 0 at the
 * catch. Before 0.2.41 only arrows got their owner's parameters: a boomerang copy stayed in procWait,
 * whose setKeepMatrix puts its model in the puppet's hand every frame, with no attack collider. Its
 * place then follows the owner's (as_follows_owner), its collider hits there (as_hit). */
static void as_boomerang_params(as_slot* s, fopAc_ac_c* a) {
    if (s->kind != AS_KIND_SPAWNED || !s->replica || s->spawn.proc != AS_PROC_BOOMERANG) return;
    u32 b = (u32)a;
    /* its procWait function as this game stores it (create() sets it): given back at the catch */
    if (!s->wait_proc && a->mParameters == 0 && *(const volatile s16*)(b + BOOM_PROC_SELECTOR) == -1)
        s->wait_proc = *(const volatile u32*)(b + BOOM_PROC_TARGET);
    /* it reached the puppet's hand before its owner's catch came: no self deletion */
    if (*(const volatile u8*)(b + BOOM_CATCH_AND_DELETE)) *(volatile u8*)(b + BOOM_CATCH_AND_DELETE) = 0;
    if (s->spawn.params == s->applied_params) return;
    s->applied_params = s->spawn.params;
    *(volatile u32*)(b + AS_PARAMETERS) = s->spawn.params;
    if (s->spawn.params == 0 && s->wait_proc) {
        /* caught by its owner: back in the hand as procMove does at the catch (020CEE10) */
        *(volatile u8*)(b + BOOM_IS_RETURNING) = 0;
        *(volatile u8*)(b + BOOM_THIRD_PERSON) = 0;
        *(volatile s16*)(b + BOOM_PROC_ADJUST) = 0;
        *(volatile s16*)(b + BOOM_PROC_SELECTOR) = -1;
        *(volatile u32*)(b + BOOM_PROC_TARGET) = s->wait_proc;
        a->speedF = 0.0f;
    }
    static u32 logged;
    if (logged++ < 3) wwhd_log_int("multiplayer: boomerang copy given its owner's parameters", (int)s->spawn.params);
}

/* Push blocks and crates (daObjMovebox, proc 0x2B: Kkiba, osiBLK0/1, DBLK0/1, MpwrB, Hbox1/2,
 * Hjump1, Hseki2/7, Mmrr, MkieBB, Ecube, d_a_obj_movebox.h). Their execute puts them back on their
 * grid cell every step (mode_wait, d_a_obj_movebox.cpp: home, turned by home.angle.y, plus m628 and
 * m62C cells of the type's attribute m0C, M_attr 101CB424 + type * 0x9C + 0xC; m628 at +0x740, m62C
 * at +0x744 with mType +0x710 and mSmokeCbs +0x768, decomp verify units): a network place written
 * over current.pos was undone. A copy that follows its owner takes the owner's cell instead (the
 * owner's place brought into the block's frame); it then stands on that cell as the owner's block
 * does, a cell at a time (the push between two cells is not shown). */
#define AS_PROC_MOVEBOX 0x2B
#define AS_MOVEBOX_TYPE 0x710
#define AS_MOVEBOX_CELL_X 0x740
#define AS_MOVEBOX_CELL_Z 0x744
#define AS_MOVEBOX_ATTR 0x101CB424u
#define AS_MOVEBOX_ATTR_SIZE 0x9C
#define AS_MOVEBOX_CELL_SIZE 0x0C
#define AS_MOVEBOX_TYPES 13

/* sine of a game angle (0x10000 a turn), within 1e-4: no libm in the guest */
static f32 as_sin(s16 angle) {
    f32 x = (f32)angle * (3.14159265f / 32768.0f); /* -pi..pi */
    if (x > 1.5707963f) x = 3.14159265f - x;
    else if (x < -1.5707963f) x = -3.14159265f - x;
    f32 x2 = x * x;
    return x * (1.0f - x2 / 6.0f * (1.0f - x2 / 20.0f * (1.0f - x2 / 42.0f * (1.0f - x2 / 72.0f))));
}

static s32 as_round(f32 v) { return v < 0.0f ? -(s32)(-v + 0.5f) : (s32)(v + 0.5f); }

static void as_movebox_cell(const as_slot* s, fopAc_ac_c* a) {
    if (*(const volatile s16*)((const u8*)a + AS_PROC_NAME) != AS_PROC_MOVEBOX) return;
    u32 type = *(const volatile u32*)((const u8*)a + AS_MOVEBOX_TYPE);
    if (type >= AS_MOVEBOX_TYPES) return;
    f32 cell = *(const volatile f32*)(AS_MOVEBOX_ATTR + type * AS_MOVEBOX_ATTR_SIZE + AS_MOVEBOX_CELL_SIZE);
    if (!(cell > 1.0f)) return;
    f32 dx = s->display.pos[0] - a->home.pos.x, dz = s->display.pos[2] - a->home.pos.z;
    s16 turn = a->home.angle.y;
    f32 sn = as_sin(turn), cs = as_sin((s16)(turn + 0x4000));
    /* mDoMtx YrotM(turn) then transM(m628 * cell, 0, m62C * cell): world = R * local */
    f32 lx = cs * dx - sn * dz, lz = sn * dx + cs * dz;
    *(volatile s32*)((u8*)a + AS_MOVEBOX_CELL_X) = as_round(lx / cell);
    *(volatile s32*)((u8*)a + AS_MOVEBOX_CELL_Z) = as_round(lz / cell);
}

static void as_before_execute(as_slot* s, fopAc_ac_c* a) {
    as_boomerang_params(s, a);
    if (as_follows_owner(s)) {
        as_place(s, a);
        as_movebox_cell(s, a);
        as_apply_carry(s, a);
        if (s->has_net_health && s->net_tag) el_adopt(a, s->net_tag); /* the owner's elite draw */
        /* a missed hit only: never heals a copy that was hit first, never touches a dying one */
        if (s->has_net_health && a->health > 1 && s->net_health < a->health)
            a->health = (s8)(s->net_health < 1 ? 1 : s->net_health); /* the death comes as a hit */
    }
    s->health_before = a->health;
    s->before_pos[0] = a->current.pos.x;
    s->before_pos[1] = a->current.pos.y;
    s->before_pos[2] = a->current.pos.z;
}

static void as_local_interaction(as_slot* s) {
    s->local_hit = 1;
    s->hit_clock = 0;
}

static void as_after_execute(as_slot* s, fopAc_ac_c* a) {
    if (s->state != AS_SYNCED) return;
    if (a->health < s->health_before) {
        if (s->replayed) s->replayed = 0; /* a replayed hit: every client takes it */
        else as_local_interaction(s);     /* a hit of this player (or of the world in its game) */
        return;
    }
    int following = as_follows_owner(s);
    if (s->kind == AS_KIND_PLACED && s->group != AS_GROUP_ENEMY && following && as_have_link) {
        f32 dx = a->current.pos.x - s->before_pos[0], dy = a->current.pos.y - s->before_pos[1];
        f32 dz = a->current.pos.z - s->before_pos[2];
        f32 lx = a->current.pos.x - as_link_pos[0], ly = a->current.pos.y - as_link_pos[1];
        f32 lz = a->current.pos.z - as_link_pos[2];
        if (dx * dx + dy * dy + dz * dz > AS_MOVE_EPSILON * AS_MOVE_EPSILON &&
            lx * lx + ly * ly + lz * lz < AS_INTERACT_DISTANCE * AS_INTERACT_DISTANCE) {
            as_local_interaction(s); /* carried, pushed or thrown by this player */
            return;
        }
    }
    /* a hookshot copy in the puppet's hand: procWait put it there (as_execute) */
    if (following && s->kind == AS_KIND_SPAWNED && s->spawn.proc == AS_PROC_HOOKSHOT && s->spawn.params == 0) return;
    if (following) {
        as_place(s, a);
        as_apply_carry(s, a);
    }
}

/* ---- the others' hookshot and Grappling Hook, shown only (0.2.42) ----
 * daHookshot_c::execute (02176D98, d_a_hookshot.cpp) runs its proc, then from its place and
 * player 0's mHookshotRootPos computes the chain (mChainCnt, shape_angle) and its tip's matrix;
 * draw (02176C50) lays the chain from the link player's mHookshotRootPos. Its procs pull the player
 * and catch actors: a copy runs procWait only, in its owner's waiting mode (0), which keeps it in
 * the puppet's hand; in the other modes (shot 1, return 2, pull 3) it is put at its owner's
 * hookshot's place and runs execute with the proc skipped (execute calls it only when the ptmf's
 * index is not 0), so only the chain and the tip are computed.
 * himo2_class (the Grappling Hook, d_a_himo2.cpp / d_a_himo2_move.cpp) reads the local camera
 * and pad and its hooking starts an event: a copy never runs its execute. Its owner's state
 * (m02DC, sent as the spawn variant) and place are enough for its draw (0216CA5C): waiting (0, 1),
 * the hook in the left hand and the coil in the right; out (2 and more), a line of segments from
 * the puppet's left hand to the owner's hook, drawn as the free flight (state 2) draws it
 * (himo2_draw: m02B4, then m0310[m02CC + 1 .. 97], the hook at m0310[98] turned by its m1E, m1C). */
/* Square root (the 750 has no fsqrt; no libm): a power-of-4 start, then Newton. */
static f32 as_sqrt(f32 x) {
    if (!(x > 0.0f)) return 0.0f;
    f32 g = 1.0f;
    f32 y = x;
    while (y > 4.0f) { y *= 0.25f; g *= 2.0f; }
    while (y < 0.25f) { y *= 4.0f; g *= 0.5f; }
    for (int i = 0; i != 6; ++i) g = 0.5f * (g + x / g);
    return g;
}

static void as_rope_point(const f32* m, f32* out) { /* the hand matrix times (4, 0, 0), as execute */
    out[0] = m[0] * 4.0f + m[3];
    out[1] = m[4] * 4.0f + m[7];
    out[2] = m[8] * 4.0f + m[11];
}

static void as_rope_visual(as_slot* s, fopAc_ac_c* a) {
    puppet_slot* ps = puppet_slot_for(s->creator, 0);
    if (!ps || !ps->actor) return;
    const daPy_lk_c* pl = (const daPy_lk_c*)ps->actor;
    const f32* lh = puppet_anm_mtx(pl->mpCLModel, 8);  /* cl_LhandA */
    const f32* rh = puppet_anm_mtx(pl->mpCLModel, 12); /* cl_RhandA */
    if (!lh || !rh) return;
    u32 r = (u32)a;
    f32 left[3], right[3];
    as_rope_point(lh, left);
    as_rope_point(rh, right);
    volatile f32* hands = (volatile f32*)(r + ROPE_HANDS);
    volatile f32* start = (volatile f32*)(r + ROPE_START);
    for (int c = 0; c != 3; ++c) {
        hands[c] = left[c];
        hands[3 + c] = right[c];
        start[c] = left[c];
    }
    *(volatile s16*)(r + ROPE_M029E) = 0;
    *(volatile s8*)(r + ROPE_SPIN) = 0;
    *(volatile s8*)(r + ROPE_DEMO) = 0;
    if (s->spawn.variant < ROPE_LAUNCH_FREE) {
        *(volatile s32*)(r + ROPE_STATE) = 0;
        *(volatile s32*)(r + ROPE_FIRST) = 97;
        *(volatile f32*)(r + ROPE_LENGTH) = 0.0f;
        a->current.pos.x = left[0];
        a->current.pos.y = left[1];
        a->current.pos.z = left[2];
        return;
    }
    f32 end[3] = {a->current.pos.x, a->current.pos.y, a->current.pos.z}; /* the owner's hook */
    f32 d[3] = {end[0] - left[0], end[1] - left[1], end[2] - left[2]};
    f32 square = d[0] * d[0] + d[1] * d[1] + d[2] * d[2];
    f32 dist = as_sqrt(square);
    s32 n = (s32)(dist / 40.0f);
    if (n < 2) n = 2;
    if (n > 90) n = 90;
    s32 first = 97 - n;
    for (s32 k = 1; k <= n + 1; ++k) {
        f32 t = (f32)k / (f32)(n + 1);
        volatile f32* p = (volatile f32*)(r + ROPE_SEGMENTS + (u32)(first + k) * ROPE_SEGMENT_BYTES + 0x10);
        for (int c = 0; c != 3; ++c) p[c] = left[c] + d[c] * t;
    }
    f32 horizontal = as_sqrt(d[0] * d[0] + d[2] * d[2]);
    u32 hook = r + ROPE_SEGMENTS + 98u * ROPE_SEGMENT_BYTES;
    *(volatile s16*)(hook + 0x1C) = (s16)-wwhd_cM_atan2s_hd(d[1], horizontal);
    *(volatile s16*)(hook + 0x1E) = wwhd_cM_atan2s_hd(d[0], d[2]);
    *(volatile s32*)(r + ROPE_FIRST) = first;
    *(volatile s32*)(r + ROPE_STATE) = ROPE_LAUNCH_FREE;
    *(volatile f32*)(r + ROPE_LENGTH) = dist;
}

/* The execute of a hookshot or Grappling Hook copy; 0 when it is not one. */
static int as_visual_execute(as_slot* s, void* actor, s32* result) {
    if (!s || s->kind != AS_KIND_SPAWNED || !s->replica || s->state != AS_SYNCED) return 0;
    fopAc_ac_c* a = (fopAc_ac_c*)actor;
    if (s->spawn.proc == AS_PROC_ROPE) {
        as_rope_visual(s, a);
        *result = 1;
        return 1;
    }
    if (s->spawn.proc != AS_PROC_HOOKSHOT) return 0;
    if (s->spawn.params == 0) {
        a->mParameters = 0; /* procWait: in the hand, never shot */
        *result = sense_execute(actor);
        return 1;
    }
    a->mParameters = s->spawn.params;
    volatile s16* index = (volatile s16*)((u32)actor + HOOK_PROC_INDEX);
    s16 saved = *index;
    *index = 0;
    *result = sense_execute(actor);
    *index = saved;
    return 1;
}

/* particle_sync.h: the emitters the local Link (or its ship) made in its execute, read after it */
static int ps_capturing;
static void ps_flush(void);

/* fopAc_Execute: the network values around the actor's execute, the puppet senses inside, the
 * creation context for what it creates; a weapon copy runs with its creator's puppet as player. */
WWHD_REPLACE(0x025D475C, s32, as_execute, (void* actor)) {
    if (!actor) return sense_execute(actor);
    as_ctx_entry* kill = as_kill_count ? as_ctx_find(as_kill, &as_kill_count, actor) : 0;
    if (kill) { /* frozen until its deletion (as_delete drops the entry) */
        if (kill->ctx != 0xFF) {
            kill->ctx = 0xFF;
            wwhd_fopAcM_delete_actor((u32)actor);
        }
        return 1;
    }
    as_slot* s = as_find(actor);
    u8 saved = as_context;
    as_context = as_context_of(actor, s);
    as_player_swap swap;
    as_swap_in(s, &swap);
    if (s) as_before_execute(s, (fopAc_ac_c*)actor);
    int local = as_context == AS_CTX_LOCAL;
    int saved_capture = ps_capturing;
    ps_capturing = local;
    s32 result;
    el_actor* elite = el_execute_begin(actor); /* an elite's animations go faster (elite.h) */
    if (!as_visual_execute(s, actor, &result)) result = sense_execute(actor);
    el_execute_end(elite);
    ps_capturing = saved_capture;
    if (local) ps_flush();
    if (s) as_after_execute(s, (fopAc_ac_c*)actor);
    as_swap_out(&swap);
    as_context = saved;
    return result;
}

/* fopAc_Draw (025D4654, one argument, f_op_actor.cpp): the boomerang, hookshot and Grappling
 * Hook draw from the player (the hookshot's chain from the link player's mHookshotRootPos, the
 * rope's coil and hook in player 0's hands): a copy draws with its creator's puppet as player. */
WWHD_GAME_ORIGINAL(0x025D4654, s32, as_draw_original, (void* actor));
WWHD_REPLACE(0x025D4654, s32, as_draw, (void* actor)) {
    as_slot* s = actor ? as_find(actor) : 0;
    if (!s || s->kind != AS_KIND_SPAWNED || !s->replica ||
        (s->spawn.proc != AS_PROC_BOOMERANG && s->spawn.proc != AS_PROC_HOOKSHOT && s->spawn.proc != AS_PROC_ROPE)) {
        el_draw_begin(actor); /* an elite draws with its own colours (elite.h) */
        s32 drawn = as_draw_original(actor);
        el_draw_end();
        return drawn;
    }
    as_player_swap swap;
    as_swap_in(s, &swap);
    s32 result = as_draw_original(actor);
    as_swap_out(&swap);
    return result;
}

/* ------------------------------------------------------------------ scenery and arrows */

/* The context of a scenery cut: the local Link's own, or a puppet's replayed swing. */
static u8 as_cutter_context(u32 actor) {
    if (!actor || !as_session) return AS_CTX_NONE;
    if (as_link && actor == as_link) return AS_CTX_LOCAL;
    if (puppet_is((const void*)actor)) return AS_CTX_FOREIGN;
    return AS_CTX_NONE;
}

WWHD_GAME_ORIGINAL(0x0254A7C8, void, as_grass_cut_original, (u32 self, u32 actor, u32 mode, s32 room, u32 hit));
WWHD_GAME_ORIGINAL(0x025453B8, void, as_flower_cut_original, (u32 self, u32 actor, u32 mask, s32 room, u32 hit));

WWHD_REPLACE(0x0254A7C8, void, as_grass_cut, (u32 self, u32 actor, u32 mode, s32 room, u32 hit)) {
    u8 saved = as_context;
    as_context = as_cutter_context(actor);
    as_grass_cut_original(self, actor, mode, room, hit);
    as_context = saved;
}

WWHD_REPLACE(0x025453B8, void, as_flower_cut, (u32 self, u32 actor, u32 mask, s32 room, u32 hit)) {
    u8 saved = as_context;
    as_context = as_cutter_context(actor);
    as_flower_cut_original(self, actor, mask, room, hit);
    as_context = saved;
}

static const as_slot* as_arrow_copy(const void* arrow) {
    const as_slot* s = as_find(arrow);
    return (s && s->kind == AS_KIND_SPAWNED && s->replica && s->spawn.proc == AS_PROC_ARROW) ? s : 0;
}

WWHD_GAME_ORIGINAL(0x02052420, void, as_arrow_type_original, (void* self));
WWHD_GAME_ORIGINAL(0x02053AD0, void, as_arrow_mp_original, (void* self));

/* daArrow_c::setTypeByPlayer: a copy takes its creator's element. */
WWHD_REPLACE(0x02052420, void, as_arrow_type, (void* self)) {
    const as_slot* s = as_arrow_copy(self);
    if (!s) {
        as_arrow_type_original(self);
        return;
    }
    u8 type = s->spawn.variant;
    *(volatile u8*)((u8*)self + AS_ARROW_TYPE) = type <= 3 ? type : 0;
}

WWHD_GAME_ORIGINAL(0x02053814, void, as_arrow_shoot_original, (void* self));

/* daArrow_c::arrowShooting: a copy is shot from where and how its owner's arrow was. */
WWHD_REPLACE(0x02053814, void, as_arrow_shoot, (void* self)) {
    as_slot* s = (as_slot*)as_arrow_copy(self);
    if (s && s->has_shot) {
        fopAc_ac_c* a = (fopAc_ac_c*)self;
        a->current.pos.x = s->shot.pos[0];
        a->current.pos.y = s->shot.pos[1];
        a->current.pos.z = s->shot.pos[2];
        a->shape_angle.x = s->shot.rot[0];
        a->shape_angle.y = s->shot.rot[1];
        a->shape_angle.z = s->shot.rot[2];
        a->current.angle.x = (s16)-s->shot.rot[0]; /* as setKeepMatrix sets them */
        a->current.angle.y = s->shot.rot[1];
    }
    as_arrow_shoot_original(self);
    if (s) s->free_flight = 1;
}

/* daArrow_c::arrowUseMp: a copy's shot does not use the local magic. */
WWHD_REPLACE(0x02053AD0, void, as_arrow_mp, (void* self)) {
    if (!as_arrow_copy(self)) as_arrow_mp_original(self);
}

/* ------------------------------------------------------------------ network */

static as_slot* as_find_uid(mp_u32 uid) {
    if (uid == MP_NO_ID) return 0;
    for (u32 i = 0; i != AS_MAX; ++i)
        if (as_slots[i].used && as_slots[i].uid == uid && as_slots[i].state != AS_LOCAL) return &as_slots[i];
    return 0;
}

static int as_same_bytes(const u8* a, const u8* b, u32 n) {
    for (u32 i = 0; i != n; ++i)
        if (a[i] != b[i]) return 0;
    return 1;
}

static as_slot* as_match_create(const mp_placed_actor* a) {
    for (u32 i = 0; i != AS_MAX; ++i) {
        as_slot* s = &as_slots[i];
        if (!s->used || s->state == AS_REMOVED) continue;
        if (a->has_solver && s->state == AS_PENDING && s->solver == a->solver) return s;
        if (a->game_uid_size == AS_PRM_BYTES && s->uid == MP_NO_ID && as_same_bytes(a->game_uid, s->prm, AS_PRM_BYTES))
            return s;
    }
    return 0;
}

static void as_apply_values(as_slot* s, const mp_placed_actor* a) {
    s->owner = a->owner;
    s->ticket = a->ticket;
    if (a->values.has_transform) {
        s->target = a->values.transform;
        s->has_target = 1;
        s->age = 0;
        if (!s->has_display) {
            s->display = s->target;
            s->has_display = 1;
        }
    }
    if (a->values.has_health) {
        s->net_health = a->values.health;
        s->net_tag = a->values.tag;
        s->net_status = a->values.status;
        s->has_net_health = 1;
        s->age = 0;
    }
    if (a->values.has_spawn && s->kind == AS_KIND_SPAWNED && s->replica) {
        s->spawn = a->values.spawn;
        /* an arrow copy is shot when its owner's parameters become 1 (daArrow_c::procWait), from
         * the owner's arrow's place and angles sent with them */
        if (s->spawn.proc == AS_PROC_ARROW && s->spawn.params == 1 && a->values.has_transform && !s->has_shot) {
            s->shot = a->values.transform;
            s->has_shot = 1;
        }
        if (s->state == AS_SYNCED && s->actor && s->spawn.proc == AS_PROC_ARROW)
            *(volatile u32*)(s->actor + AS_PARAMETERS) = s->spawn.params;
    }
}

/* SERVER_ACTOR_SYNC_CREATE of a placed actor: the answer to our create (our solver), or the actor
 * another player created first (same game UID; the server sends its existing one). Returns 0 when
 * malformed. */
/* Another player's spawned actor: a copy is created at the next step. */
static void as_add_replica(const mp_placed_actor* a) {
    if (!a->values.has_spawn || !a->values.has_transform || as_find_uid(a->uid)) return;
    u8 cls = as_spawn_class((s16)a->values.spawn.proc);
    if (cls == AS_CLS_NONE) return;
    if (cls == AS_CLS_WEAPON) {
        puppet_slot* ps = puppet_slot_for(a->owner, 0);
        if (!ps || !ps->actor) return; /* its code reads the player: only with the creator's puppet */
    }
    as_slot* s = as_alloc();
    if (!s) return;
    s->kind = AS_KIND_SPAWNED;
    s->cls = cls;
    s->replica = 1;
    s->state = AS_SPAWNING;
    s->factory = (u8)a->factory;
    s->uid = a->uid;
    s->creator = a->owner;
    s->spawn = a->values.spawn;
    s->applied_params = a->values.spawn.params; /* it is created with them */
    s->pending_clock = 0;
    as_apply_values(s, a);
}

/* ---- the launcher's shop: creatures summoned next to a player (mod.c shop_*) ----
 * A summoned creature is made like a placed actor of the stage: a block from fopAcM_CreateAppend
 * (025D5600: cleared, setID 0xFFFF, scale 10, parent -1, argument 0xFF), its parameters, place,
 * angles and room, then dStage_actorCreate (025C1154, the original: it writes argument and gbaName
 * from the name table and requests the process). The block is tagged (AS_TAG) with AS_SUMMON as
 * its copy number (a multiplier copy is < 16), so the actor is synchronised like the stage's
 * enemies, its game UID being the block. The server sends its creation to the other players of
 * the area (SyncedActorManager.broadcastActor); a game that has no actor with that block makes it
 * from the received block (as_summon_remote), then matches it as for a placed actor. Not for a
 * player who enters the area later (the server sends only the actors without a game UID then). */
#define AS_SUMMON 0xFE
#define AS_SUMMON_RECENT 8
#define AS_SUMMON_FORGET 150 /* logic steps a remote creature is not made twice */
#define AS_PRM_SET_ID 0x16
#define AS_PRM_ROOM 0x21

typedef struct as_summon_note {
    u8 block[AS_PRM_BYTES];
    unsigned long long step;
    int used;
} as_summon_note;
static as_summon_note as_summon_notes[AS_SUMMON_RECENT];
static u32 as_summon_next;

static void as_summon_create(const char* name, u32 block) {
    for (u32 n = 0; n != sizeof(as_stage_data); ++n) as_stage_data[n] = 0;
    for (u32 n = 0; n != AS_STAGE_NAME && name[n]; ++n) as_stage_data[n] = (u8)name[n];
    as_stage_create_original((u32)as_stage_data, block);
}

/* A creature of name next to a place: 1 when requested. */
#define AS_SUMMON_SEED 0x5E /* the "copy number" of a summoned creature's variants */
static int as_summon(const char* name, u32 params, s16 angle_x, s16 angle_z, const f32* pos, s16 yaw, s8 room,
                     int randomized) {
    u32 b = wwhd_fopAcM_CreateAppend();
    if (!b) return 0;
    *(volatile u32*)b = params;
    for (u32 c = 0; c != 3; ++c) *(volatile f32*)(b + AS_PRM_POSITION + c * 4) = pos[c];
    *(volatile s16*)(b + AS_PRM_ANGLE) = angle_x;
    *(volatile s16*)(b + AS_PRM_ANGLE + 2) = yaw;
    *(volatile s16*)(b + AS_PRM_ANGLE + 4) = angle_z;
    *(volatile s8*)(b + AS_PRM_ROOM) = room;
    *(volatile u8*)(b + AS_PRM_TAG) = AS_TAG;
    *(volatile u8*)(b + AS_PRM_COPY) = AS_SUMMON;
    /* variants drawn from this block (its place differs each time); the others get the result in
     * the block itself, their game UID */
    if (randomized) as_randomize(name, (const u8*)b, AS_SUMMON_SEED, (u8*)b);
    as_summon_create(name, b);
    wwhd_log("multiplayer: creature summoned:");
    wwhd_log(name);
    return 1;
}

/* Another player's summoned creature this game does not have: made from its block, once. */
static void as_summon_remote(const mp_placed_actor* a) {
    if (a->game_uid_size != AS_PRM_BYTES || a->game_uid[AS_PRM_TAG] != AS_TAG || a->game_uid[AS_PRM_COPY] != AS_SUMMON)
        return;
    if (!as_enabled || !mp_filter_contains(as_session, a->name)) return;
    unsigned long long now = wwhd_logic_step();
    for (u32 i = 0; i != AS_SUMMON_RECENT; ++i) {
        as_summon_note* n = &as_summon_notes[i];
        if (n->used && now - n->step < AS_SUMMON_FORGET && as_same_bytes(n->block, a->game_uid, AS_PRM_BYTES)) return;
    }
    u32 b = wwhd_fopAcM_CreateAppend();
    if (!b) return;
    memcpy((void*)b, a->game_uid, AS_PRM_BYTES);
    as_summon_note* n = &as_summon_notes[as_summon_next++ % AS_SUMMON_RECENT];
    memcpy(n->block, a->game_uid, AS_PRM_BYTES);
    n->step = now;
    n->used = 1;
    as_summon_create(a->name, b);
    wwhd_log("multiplayer: another player's summoned creature made here:");
    wwhd_log(a->name);
}


static int as_roster_same(const void* a, const void* b, u32 n) {
    for (u32 k = 0; k != n; ++k)
        if (((const u8*)a)[k] != ((const u8*)b)[k]) return 0;
    return 1;
}

/* The same placed actor (name and block) as entry i, made, being made or already asked. */
static int as_roster_twin(u32 i) {
    const as_roster_entry* e = &as_roster[i];
    for (u32 j = 0; j != AS_ROSTER; ++j) {
        const as_roster_entry* o = &as_roster[j];
        if (j == i || o->state == 0 || o->state == 3) continue;
        if (as_roster_same(o->name, e->name, 8) && as_roster_same(o->stage, e->stage, 8) &&
            as_roster_same(o->block, e->block, AS_PRM_BYTES))
            return 1;
    }
    return 0;
}

/* The refused timed actors of the stage and room being played, asked again (world_sync.h): the count. */
static u32 as_roster_retry(void) {
    const u8* play = wwhd_play_get();
    if (!play) return 0;
    u32 link = *(const volatile u32*)(play + WWHD_PLAY_PLAYER_OFFSET);
    s8 room = link ? ((const fopAc_ac_c*)link)->current.roomNo : -1;
    u32 asked = 0;
    for (u32 i = 0; i != AS_ROSTER; ++i) {
        as_roster_entry* e = &as_roster[i];
        if (e->state != 3 || !e->timed) continue;
        int same = 1;
        for (u32 k = 0; k != 8 && same; ++k) {
            if (e->stage[k] != (char)play[WWHD_PLAY_START_STAGE_NAME_OFFSET + k]) same = 0;
            if (!e->stage[k]) break;
        }
        s8 at = (s8)e->block[AS_PRM_ROOM];
        if (!same || (at != room && at != -1)) continue;
        if (as_roster_twin(i)) { /* noted twice (the room loaded again): one is enough */
            e->state = 0;
            continue;
        }
        u32 b = wwhd_fopAcM_CreateAppend();
        if (!b) break;
        memcpy((void*)b, e->block, AS_PRM_BYTES);
        char name[9];
        for (u32 k = 0; k != 8; ++k) name[k] = e->name[k];
        name[8] = 0;
        e->address = b;
        e->state = 1;
        e->timed = 0;
        as_summon_create(name, b);
        ++asked;
    }
    wwhd_log_int("multiplayer: day and night: timed stage actors asked again", (int)asked);
    return asked;
}

static int as_handle_create(const mp_u8* body, mp_u32 length) {
    static mp_placed_actor a;
    if (!as_session) return 1;
    if (!mp_read_placed_create(body, length, as_session, &a)) return 0;
    if (a.factory == MP_NO_ID) return 1;
    if (!a.has_solver && a.game_uid_size == 0) {
        if (a.factory == as_spawn_factory()) as_add_replica(&a);
        return 1;
    }
    as_slot* s = as_match_create(&a);
    if (!s) as_summon_remote(&a);
    if (!s || s->factory != a.factory) return 1;
    s->uid = a.uid;
    s->state = AS_SYNCED;
    s->has_target = s->has_display = s->has_net_health = 0;
    as_apply_values(s, &a);
    if (s->kind == AS_KIND_SPAWNED) {
        if (s->cls == AS_CLS_WEAPON && as_session->client_exclusive_add != MP_NO_ID) {
            as_send_uid(as_session->client_exclusive_add, s->uid); /* the creator keeps it */
            s->exclusive = 1;
        }
        if (!as_spawned_logged++) wwhd_log_int("multiplayer: first spawned actor shared, process", (int)s->spawn.proc);
    } else if (!as_synced_logged++) {
        wwhd_log("multiplayer: first placed actor synchronised");
    }
    return 1;
}

/* SERVER_ACTOR_SYNC_UPDATE: 1 handled, 0 not a placed actor of ours, -1 malformed. */
static int as_handle_update(const mp_u8* body, mp_u32 length) {
    static mp_placed_actor a;
    if (!as_session) return 0;
    as_slot* s = as_find_uid(mp_read_update_uid(body, length));
    if (!s) return 0;
    if (!mp_read_placed_update(body, length, &as_session->factories[s->factory], &a)) return -1;
    int was_owner = s->owner == as_local_uid;
    as_apply_values(s, &a);
    if (!was_owner && s->owner == as_local_uid && !as_owned_logged++)
        wwhd_log("multiplayer: this player now owns a synchronised actor");
    return 1;
}

/* SERVER_ACTOR_SYNC_REMOVE: 1 when it was a placed actor of ours. */
static int as_handle_remove(mp_u32 uid) {
    as_slot* s = as_find_uid(uid);
    if (!s) return 0;
    s->state = AS_REMOVED;
    s->remove_clock = 0;
    return 1;
}

/* ------------------------------------------------------------------ sword hits */

static u32 as_ld32(u32 address) { return *(const volatile u32*)address; }
static u8 as_ld8(u32 address) { return *(const volatile u8*)address; }
static f32 as_ldf(u32 address) { return *(const volatile f32*)address; }
static void as_st32(u32 address, u32 value) { *(volatile u32*)address = value; }
static void as_st8(u32 address, u8 value) { *(volatile u8*)address = value; }
static void as_stf(u32 address, f32 value) { *(volatile f32*)address = value; }

/* The synchronised actor whose collider tg is, with the collider's offset in it. */
static as_slot* as_collider_owner(u32 tg, u32* offset) {
    u32 stts = as_ld32(tg + AS_OBJ_STTS);
    if (!stts) return 0;
    u32 actor = as_ld32(stts + AS_STTS_ACTOR);
    if (!actor || tg < actor || tg - actor >= AS_MAX_TG_OFFSET) return 0;
    as_slot* s = as_find((const void*)actor);
    if (!s || s->state != AS_SYNCED || s->uid == MP_NO_ID) return 0;
    *offset = tg - actor;
    return s;
}

WWHD_GAME_ORIGINAL(0x0200CFA8, void, as_hit_original, (void* self, void* at, void* tg, void* pos));

/* The spawned weapon slot whose collider at is, with the collider's offset in it. */
static as_slot* as_weapon_of(u32 at, u32* offset) {
    u32 stts = as_ld32(at + AS_OBJ_STTS);
    if (!stts) return 0;
    u32 actor = as_ld32(stts + AS_STTS_ACTOR);
    if (!actor || at < actor || at - actor >= AS_MAX_TG_OFFSET) return 0;
    as_slot* s = as_find((const void*)actor);
    if (!s || s->kind != AS_KIND_SPAWNED || s->cls != AS_CLS_WEAPON) return 0;
    *offset = at - actor;
    return s;
}

/* The actor a collider belongs to (its status's actor), 0 when none. */
static u32 as_collider_actor(u32 obj) {
    u32 stts = as_ld32(obj + AS_OBJ_STTS);
    return stts ? as_ld32(stts + AS_STTS_ACTOR) : 0;
}

/* cCcSState::SetAtTgCommonHitInf. A weapon copy (another player's bomb, arrow, boomerang) hits
 * what its creator's game cannot hit for this one: the local Link (a bomb hurts every player near
 * it, as one's own bomb hurts oneself) and the actors that are not synchronised (pots, grass, an
 * enemy outside the server's list). Not a synchronised actor (its creator's hit comes as a strike,
 * replayed with this copy's collider: once only) nor a Link puppet (that player's own game says
 * whether it was hit). Before 0.2.41 a copy hit nothing.
 * The hitter's game: a hit of its Link's sword, or of its own spawned weapon, on a synchronised
 * actor joins the next ActorPlayerSync update. */
static u32 as_copy_hits_logged, as_copy_skips_logged;
WWHD_REPLACE(0x0200CFA8, void, as_hit, (void* self, void* at, void* tg, void* pos)) {
    u32 at_offset = 0;
    as_slot* weapon = (at && as_session) ? as_weapon_of((u32)at, &at_offset) : 0;
    if (weapon && weapon->replica && !as_replaying &&
        (weapon->spawn.proc == AS_PROC_HOOKSHOT || weapon->spawn.proc == AS_PROC_ROPE))
        return; /* shown only: the hooks of the others pull and catch nothing here */
    if (weapon && weapon->replica && !as_replaying) {
        u32 target = tg ? as_collider_actor((u32)tg) : 0, unused;
        if (!target || (tg && as_collider_owner((u32)tg, &unused)) || puppet_slot_of_actor(target) >= 0) {
            if (as_copy_skips_logged < 3 && target) {
                ++as_copy_skips_logged;
                wwhd_log_hex("multiplayer: weapon copy hit left to its creator's strike, target", target);
            }
            return;
        }
        as_hit_original(self, at, tg, pos);
        if (as_copy_hits_logged < 3) {
            ++as_copy_hits_logged;
            wwhd_log_hex(target == as_link ? "multiplayer: another player's weapon hit the local Link"
                                           : "multiplayer: another player's weapon hit an actor here",
                         target);
        }
        return;
    }
    as_hit_original(self, at, tg, pos);
    if (!as_enabled || !as_session || as_local_uid == MP_NO_ID || !as_link || !at || !tg || !pos) return;
    u32 a = (u32)at, t = (u32)tg;
    u32 index;
    mp_u32 attacker = 0;
    if (a == as_link + AS_LINK_AT_CYL) index = 3;
    else if (a == as_link + AS_LINK_FAN_WIND) index = 4;
    else if (a >= as_link + AS_LINK_AT_CPS && a < as_link + AS_LINK_AT_CPS + 3 * AS_CPS_BYTES &&
             (a - as_link - AS_LINK_AT_CPS) % AS_CPS_BYTES == 0)
        index = (a - as_link - AS_LINK_AT_CPS) / AS_CPS_BYTES;
    else if (weapon && !weapon->replica && weapon->state == AS_SYNCED && weapon->uid != MP_NO_ID) {
        index = 0;
        attacker = weapon->uid;
    } else return;
    if (as_ld32(t + AS_OBJ_TG_HITOBJ) != a) return; /* filtered (continuous hit) or no hit info */
    u32 offset;
    as_slot* s = as_collider_owner(t, &offset);
    if (!s) return;
    mp_strike k;
    k.target = s->uid;
    k.tg_offset = offset;
    k.attacker = attacker;
    k.at_offset = attacker ? at_offset : 0;
    k.at_index = (mp_u8)index;
    k.atp = as_ld8(a + AS_OBJ_ATP);
    k.se = as_ld8(a + AS_GAT + AS_GAT_SE);
    k.mtrl = as_ld8(a + AS_GAT + AS_GAT_SE + 1);
    k.hit_mark = as_ld8(a + AS_GAT + AS_GAT_SE + 2);
    k.spl = as_ld8(a + AS_GAT + AS_GAT_SE + 3);
    k.at_sprm = as_ld32(a + AS_OBJ_AT_SPRM);
    k.at_type = as_ld32(a + AS_OBJ_AT_TYPE);
    k.gat_sprm = as_ld32(a + AS_GAT);
    for (u32 c = 0; c != 3; ++c) {
        k.vec[c] = as_ldf(a + AS_GAT + AS_GAT_VEC + c * 4);
        k.pos[c] = as_ldf((u32)pos + c * 4);
    }
    anim_local.strike[mp_strike_slot(&anim_local)] = k;
    s->last_strike = k;
    s->has_last_strike = 1;
    if (!as_strikes_sent++) wwhd_log_int("multiplayer: first hit sent, damage", (int)k.atp);
    static u32 weapon_strikes_logged;
    if (attacker && weapon_strikes_logged < 3) {
        ++weapon_strikes_logged;
        wwhd_log_int("multiplayer: weapon hit sent, weapon uid", (int)attacker);
        wwhd_log_int("multiplayer:   damage", (int)k.atp);
    }
}

/* The attack values a strike carries, written into a collider (saved and restored around the
 * replay when the collider is a weapon copy's own). */
typedef struct as_at_values {
    u32 at_sprm, at_rprm, at_hitobj, at_type, gat_sprm, gat_rprm;
    u8 atp, eff, se[4];
    f32 vec[3];
} as_at_values;

static void as_save_at(u32 g, as_at_values* v) {
    v->at_sprm = as_ld32(g + AS_OBJ_AT_SPRM);
    v->at_rprm = as_ld32(g + AS_OBJ_AT_RPRM);
    v->at_hitobj = as_ld32(g + AS_OBJ_AT_HITOBJ);
    v->at_type = as_ld32(g + AS_OBJ_AT_TYPE);
    v->atp = as_ld8(g + AS_OBJ_ATP);
    v->gat_sprm = as_ld32(g + AS_GAT);
    v->gat_rprm = as_ld32(g + AS_GAT + AS_GAT_RPRM);
    v->eff = as_ld8(g + AS_GAT + AS_GAT_EFF_COUNTER);
    for (u32 c = 0; c != 4; ++c) v->se[c] = as_ld8(g + AS_GAT + AS_GAT_SE + c);
    for (u32 c = 0; c != 3; ++c) v->vec[c] = as_ldf(g + AS_GAT + AS_GAT_VEC + c * 4);
}

static void as_load_at(u32 g, const as_at_values* v) {
    as_st32(g + AS_OBJ_AT_SPRM, v->at_sprm);
    as_st32(g + AS_OBJ_AT_RPRM, v->at_rprm);
    as_st32(g + AS_OBJ_AT_HITOBJ, v->at_hitobj);
    as_st32(g + AS_OBJ_AT_TYPE, v->at_type);
    as_st8(g + AS_OBJ_ATP, v->atp);
    as_st32(g + AS_GAT, v->gat_sprm);
    as_st32(g + AS_GAT + AS_GAT_RPRM, v->gat_rprm);
    as_st8(g + AS_GAT + AS_GAT_EFF_COUNTER, v->eff);
    for (u32 c = 0; c != 4; ++c) as_st8(g + AS_GAT + AS_GAT_SE + c, v->se[c]);
    for (u32 c = 0; c != 3; ++c) as_stf(g + AS_GAT + AS_GAT_VEC + c * 4, v->vec[c]);
}

static void as_strike_values(const mp_strike* k, as_at_values* v) {
    v->at_sprm = k->at_sprm;
    v->at_rprm = 0;
    v->at_hitobj = 0;
    v->at_type = k->at_type;
    v->atp = k->atp;
    v->gat_sprm = k->gat_sprm;
    v->gat_rprm = 0;
    v->eff = 0;
    v->se[0] = k->se;
    v->se[1] = k->mtrl;
    v->se[2] = k->hit_mark;
    v->se[3] = k->spl;
    for (u32 c = 0; c != 3; ++c) v->vec[c] = k->vec[c];
}

/* The weapon copy's own collider for a strike of that weapon, 0 when there is none. */
static u32 as_weapon_collider(const mp_strike* k) {
    if (!k->attacker || k->at_offset >= AS_MAX_TG_OFFSET) return 0;
    as_slot* w = as_find_uid(k->attacker);
    if (!w || w->kind != AS_KIND_SPAWNED || !w->replica || w->state != AS_SYNCED) return 0;
    if (as_find((const void*)w->actor) != w) return 0;
    u32 at = w->actor + k->at_offset;
    u32 stts = as_ld32(at + AS_OBJ_STTS);
    if (!stts || as_ld32(stts + AS_STTS_ACTOR) != w->actor) return 0;
    return at;
}

/* The others: received strikes wait for the next ChkAtTg. */
static void as_queue_strikes(mp_u32 player, const mp_anim_events* e) {
    for (u32 i = 0; i != e->strike_count; ++i) {
        if (as_strike_count == AS_MAX_STRIKES) return;
        as_strikes[as_strike_count].player = player;
        as_strikes[as_strike_count].strike = e->strike[i];
        ++as_strike_count;
    }
}

static void as_build_ghost(void) {
    if (as_ghost_built) return;
    u32 cps = (u32)as_ghost;
    wwhd_dCcD_Cps_ct_l(cps);
    wwhd_dCcD_Cps_Set((void*)cps, (u32)&WWHD_GAME_DATA(AS_AT_CPS_SRC, u8));
    as_st32(cps + AS_OBJ_TG_SPRM, as_ld32(cps + AS_OBJ_TG_SPRM) & ~AS_SPRM_SET); /* attacks only */
    as_st32(cps + AS_OBJ_CO_SPRM, as_ld32(cps + AS_OBJ_CO_SPRM) & ~AS_SPRM_SET);
    as_ghost_built = 1;
}

static void as_replay(u32 ccs, const as_pending_strike* p) {
    const mp_strike* k = &p->strike;
    as_slot* s = as_find_uid(k->target);
    if (!s || (s->state != AS_SYNCED && s->state != AS_REMOVED) || !s->actor || as_find((const void*)s->actor) != s)
        return;
    if (k->tg_offset >= AS_MAX_TG_OFFSET) return;
    u32 tg = s->actor + k->tg_offset;
    u32 stts = as_ld32(tg + AS_OBJ_STTS);
    if (!stts || as_ld32(stts + AS_STTS_ACTOR) != s->actor) return; /* not one of its colliders */
    if (!(as_ld32(tg + AS_OBJ_TG_SPRM) & AS_SPRM_SET)) return;        /* not taking hits now */
    as_at_values wanted;
    as_strike_values(k, &wanted);
    as_hit_pos.x = k->pos[0];
    as_hit_pos.y = k->pos[1];
    as_hit_pos.z = k->pos[2];
    u32 g = as_weapon_collider(k); /* a bomb's or arrow's hit: with the copy's own collider */
    static u32 weapon_replays_logged;
    if (k->attacker && weapon_replays_logged < 6) {
        ++weapon_replays_logged;
        wwhd_log_int(g ? "multiplayer: weapon hit replayed with the copy's collider, damage"
                       : "multiplayer: weapon hit replayed without its copy (puppet's collider), damage",
                     (int)k->atp);
    }
    if (g) {
        as_at_values saved;
        as_save_at(g, &saved);
        as_load_at(g, &wanted);
        as_replaying = 1;
        wwhd_set_at_tg((void*)ccs, (void*)g, (void*)tg, &as_hit_pos);
        as_replaying = 0;
        u32 hit = as_ld32(tg + AS_OBJ_TG_HITOBJ) == g;
        as_load_at(g, &saved);
        if (!hit) {
            static u32 refused_logged;
            if (refused_logged++ < 3) wwhd_log_int("multiplayer: weapon hit refused by the target's collider, damage", (int)k->atp);
            return;
        }
    } else {
        u32 hitter = as_link;
        puppet_slot* puppet = puppet_slot_for(p->player, 0);
        if (puppet && puppet->actor) hitter = puppet->actor;
        if (!hitter) return;
        as_build_ghost();
        g = (u32)as_ghost;
        as_load_at(g, &wanted);
        as_st32(g + AS_OBJ_STTS, hitter + AS_LINK_STTS);
        wwhd_set_at_tg((void*)ccs, (void*)g, (void*)tg, &as_hit_pos);
        if (as_ld32(tg + AS_OBJ_TG_HITOBJ) != g) return; /* filtered like a real continuous hit */
    }
    s->replayed = 1;
    s->replay_clock = 0;
    if (!k->attacker) {
        s->last_strike = *k;
        s->has_last_strike = 1;
    }
    if (!as_strikes_replayed++) wwhd_log_int("multiplayer: first remote hit replayed, damage", (int)k->atp);
}

WWHD_HOOK_RETURN(0x0200D19C, as_after_chk_at_tg, (void* self)) {
    if (!as_strike_count) return;
    u32 count = as_strike_count;
    as_strike_count = 0;
    const u8* play = wwhd_play_get();
    if (!as_enabled || !as_session || !play || (u32)self != (u32)(play + AS_PLAY_CCS)) return;
    for (u32 i = 0; i != count; ++i) as_replay((u32)self, &as_strikes[i]);
}

/* ------------------------------------------------------------------ hit stop */

WWHD_GAME_ORIGINAL(0x025B02E8, s8, as_calc_pause_timer_original, (u32 self));

WWHD_REPLACE(0x025B02E8, s8, as_calc_pause_timer, (u32 self)) {
    if (as_hit_stop) return as_calc_pause_timer_original(self);
    WWHD_GAME_DATA(AS_PAUSE_TIMERS, u16) = 0; /* both s8 timers at once (even address) */
    return 0;
}

/* The area changed or the connection ended: every placed actor is local again; spawned ones are
 * forgotten (their copies go with the stage, ours are not announced again). */
static void as_reset_network(void) {
    for (u32 i = 0; i != AS_MAX; ++i) {
        as_slot* s = &as_slots[i];
        if (!s->used) continue;
        if (s->kind == AS_KIND_SPAWNED) {
            if (s->state == AS_SPAWNING && !s->actor) {
                s->used = 0;
                continue;
            }
            s->state = AS_REMOVED;
            s->deleting = 1;
            s->uid = MP_NO_ID;
            continue;
        }
        s->state = AS_LOCAL;
        s->uid = s->solver = MP_NO_ID;
        s->owner = 0;
        s->ticket = 0xFF;
        s->has_target = s->has_display = s->has_net_health = s->has_sent = 0;
        s->exclusive = s->local_hit = s->deleting = s->replayed = s->broken = 0;
    }
    as_local_uid = MP_NO_ID;
    as_strike_count = 0;
}

static s16 as_turn(s16 from, s16 to, f32 t) {
    s32 delta = (s32)to - (s32)from;
    if (delta > 32767) delta -= 65536;
    if (delta < -32768) delta += 65536;
    return (s16)(from + (s32)((f32)delta * t));
}

static void as_read_spawn(const as_slot* s, const fopAc_ac_c* a, mp_spawn* p) {
    p->proc = s->spawn.proc;
    p->params = a->mParameters;
    p->argument = (mp_u8)a->argument;
    for (u32 i = 0; i != 3; ++i) {
        p->angle[i] = (mp_s16)(((u32)s->prm[AS_PRM_ANGLE + i * 2] << 8) | s->prm[AS_PRM_ANGLE + i * 2 + 1]);
        p->scale[i] = s->prm[AS_PRM_SCALE + i];
    }
    p->room = (mp_u8)a->current.roomNo;
    p->speed_f = a->speedF;
    p->speed[0] = a->speed.x;
    p->speed[1] = a->speed.y;
    p->speed[2] = a->speed.z;
    p->gravity = a->gravity;
    if (s->spawn.proc == AS_PROC_ARROW)
        p->variant = *(const volatile u8*)((const u8*)a + AS_ARROW_TYPE);
    else if (s->spawn.proc == AS_PROC_ROPE) /* its state: in the hand or out (as_rope_visual) */
        p->variant = (u8)*(const volatile s32*)((const u8*)a + ROPE_STATE);
    else
        p->variant = (u8)(((a->actor_status & 0x2000u) ? 1 : 0) | ((a->actor_status & 0x100000u) ? 2 : 0));
}

static void as_send_create(as_slot* s, const fopAc_ac_c* a) {
    mp_actor_values v;
    as_read_values(a, &v);
    mp_writer w = mp_writer_make(as_payload, sizeof(as_payload));
    s->solver = as_next_solver++;
    if (s->kind == AS_KIND_SPAWNED) {
        as_read_spawn(s, a, &v.spawn);
        s->sent_params = a->mParameters;
        s->sent_variant = v.spawn.variant;
        mp_write_actor_create_named(&w, &as_session->factories[s->factory], s->solver, 0, 0, s->name,
                                    MP_SPAWN_FACTORY, &v);
    } else {
        mp_write_actor_create(&w, &as_session->factories[s->factory], s->solver, s->prm, AS_PRM_BYTES, s->name, &v);
    }
    if (!as_send(as_session->client_create, &w)) return;
    s->state = AS_PENDING;
    s->pending_clock = 0;
    s->sent = v.transform;
    s->sent_health = v.health;
    s->sent_tag = v.tag;
    s->sent_status = v.status;
    s->has_sent = 1;
}

/* The owner sends what changed: the transform every AS_UPDATE_PERIOD, the health at once. */
static void as_send_owner(as_slot* s, const fopAc_ac_c* a, double dt) {
    mp_actor_values v;
    as_read_values(a, &v);
    s->send_clock -= dt;
    int moved = 0;
    for (int k = 0; k != 3; ++k)
        moved |= v.transform.pos[k] != s->sent.pos[k] || v.transform.rot[k] != s->sent.rot[k];
    v.has_transform = moved && s->send_clock <= 0;
    v.has_health = as_session->factories[s->factory].health_index != 0xFF &&
                   (v.health != s->sent_health || v.tag != s->sent_tag || v.status != s->sent_status);
    v.has_spawn = 0;
    if (s->kind == AS_KIND_SPAWNED && !s->replica) {
        as_read_spawn(s, a, &v.spawn);
        if (s->spawn.proc == AS_PROC_ARROW) v.has_spawn = a->mParameters != s->sent_params;
        else if (s->spawn.proc == AS_PROC_BOOMERANG || s->spawn.proc == AS_PROC_HOOKSHOT ||
                 s->spawn.proc == AS_PROC_ROPE) /* thrown, back in the hand; the rope's state */
            v.has_spawn = a->mParameters != s->sent_params || v.spawn.variant != s->sent_variant;
        else v.has_spawn = v.spawn.variant != s->sent_variant; /* picked up, put down, thrown */
        if (v.has_spawn) v.has_transform = 1; /* the place it happens at */
    }
    if (!v.has_transform && !v.has_health && !v.has_spawn) return;
    mp_writer w = mp_writer_make(as_payload, sizeof(as_payload));
    mp_write_actor_update(&w, &as_session->factories[s->factory], s->uid, s->ticket, &v);
    if (!as_send(as_session->client_update, &w)) return;
    if (v.has_transform) {
        s->sent = v.transform;
        s->send_clock = AS_UPDATE_PERIOD;
    }
    if (v.has_health) {
        s->sent_health = v.health;
        s->sent_tag = v.tag;
        s->sent_status = v.status;
    }
    if (v.has_spawn) {
        s->sent_params = v.spawn.params;
        s->sent_variant = v.spawn.variant;
    }
    /* what the others will show: no jump when the ownership goes back to them */
    s->display = v.transform;
    s->has_display = 1;
}

/* A copy of another player's spawned actor: requested once, then found in fopAc_Create. */
static void as_step_replica(as_slot* s, double dt) {
    if (s->requested) {
        s->pending_clock += dt;
        if (s->pending_clock >= AS_SPAWN_WAIT) s->used = 0; /* never created (heap, bad parameters) */
        return;
    }
    const mp_spawn* p = &s->spawn;
    as_spawn_pos.x = s->target.pos[0];
    as_spawn_pos.y = s->target.pos[1];
    as_spawn_pos.z = s->target.pos[2];
    as_spawn_angle.x = p->angle[0];
    as_spawn_angle.y = p->angle[1];
    as_spawn_angle.z = p->angle[2];
    as_spawn_scale.x = (f32)p->scale[0] * 0.1f;
    as_spawn_scale.y = (f32)p->scale[1] * 0.1f;
    as_spawn_scale.z = (f32)p->scale[2] * 0.1f;
    as_spawning = 1;
    u32 id = wwhd_fopAcM_create((u32)(s32)(s16)p->proc, p->params, (u32)&as_spawn_pos, (s32)(s8)p->room,
                                (u32)&as_spawn_angle, (u32)&as_spawn_scale, (u32)(s32)(s8)p->argument, 0);
    as_spawning = 0;
    if (id == 0xFFFFFFFFu) {
        s->used = 0;
        return;
    }
    s->request_id = id;
    s->requested = 1;
    s->pending_clock = 0;
}

/* Pots (daTsubo: kotubo, ootubo1, Kmtub, Ptubo... in the server's list). A pot breaks in
 * daTsuboDamage::Act_c::damaged (024CC004, d_a_tsubo_dmg.cpp: this, item action, ground polygon,
 * effects, place): the item of its parameters (fopAcM_createItemFromTable 025D8120), its break
 * effect and sound; its execute then deletes it. A synchronised pot broken in this game (a hit,
 * a throw, a fall) is marked, and its deletion sends CLIENT_ACTOR_SYNC_REMOVE (as_on_deleted); the
 * other games break their copy the same way, effects and sound, but without making the item a
 * second time (the breaker's game made it), then delete it. Before 0.2.77 only dead enemies were
 * removed for the others: a pot broken in one game stayed whole in the others. */
#define AS_PROC_TSUBO 0x1C5 /* fpcNm_TSUBO_e (gc_api/f_op/f_op_actor_mng.h) */
WWHD_GAME_FUNC(0x024CC004, void, as_pot_damaged_call, (void* self, s32 action, void* poly, u32 effects, void* pos));
WWHD_GAME_ORIGINAL(0x025D8120, u32, as_item_from_table_original,
                   (u32 pos, u32 item, u32 bit, u32 room, u32 type, u32 angle, u32 action, u32 scale));
static int as_breaking_copy;

WWHD_HOOK(0x024CC004, as_pot_damaged, (void* self, s32 action, void* poly, u32 effects, void* pos)) {
    (void)action;
    (void)poly;
    (void)effects;
    (void)pos;
    if (as_breaking_copy || !self) return;
    as_slot* s = as_find(self);
    if (s && s->kind == AS_KIND_PLACED && s->state == AS_SYNCED && s->uid != MP_NO_ID) s->broken = 1;
}

/* fopAcM_createItemFromTable: nothing while a copy breaks for another game's break. */
WWHD_REPLACE(0x025D8120, u32, as_item_from_table,
             (u32 pos, u32 item, u32 bit, u32 room, u32 type, u32 angle, u32 action, u32 scale)) {
    if (as_breaking_copy) return 0;
    return as_item_from_table_original(pos, item, bit, room, type, angle, action, scale);
}

static void as_break_pot(as_slot* s, fopAc_ac_c* a) {
    as_breaking_copy = 1;
    as_pot_damaged_call(a, 1, 0, 1, &a->current.pos);
    as_breaking_copy = 0;
    s->deleting = 1;
    wwhd_fopAcM_delete_actor((u32)a);
    static u32 logged;
    if (!logged++) wwhd_log("multiplayer: a pot broken in another game broke here");
}

/* A copy dead at its owner but alive here gets a lethal replayed hit (see the header). */
static void as_step_kill(as_slot* s, const fopAc_ac_c* a, double dt) {
    if (s->kind != AS_KIND_PLACED || s->group != AS_GROUP_ENEMY || a->health <= 0 || !s->has_last_strike) return;
    int dead_there = s->state == AS_REMOVED ||
                     (s->state == AS_SYNCED && s->owner && s->owner != as_local_uid && s->has_net_health &&
                      s->net_health <= 0);
    if (!dead_there) return;
    s->kill_clock -= dt;
    if (s->kill_clock > 0 || as_strike_count == AS_MAX_STRIKES || s->uid == MP_NO_ID) return;
    s->kill_clock = AS_KILL_PERIOD;
    as_pending_strike* p = &as_strikes[as_strike_count++];
    p->player = s->owner;
    p->strike = s->last_strike;
    p->strike.target = s->uid;
    p->strike.attacker = 0;
    p->strike.atp = AS_KILL_DAMAGE;
}

/* The server's mutators changed (SERVER_MUTATORS, or the init): the loaded area follows them. */
static void as_mutators_changed(void) {
    as_mutators_dirty = 1;
}

/* A copy the new mutators do not keep: its owner removes it at the server, every game deletes it. */
static void as_drop_copy(as_copy_entry* c) {
    as_slot* s = as_find((const void*)c->actor);
    if (s && s->state == AS_SYNCED && s->uid != MP_NO_ID && as_local_uid != MP_NO_ID && s->owner == as_local_uid) {
        if (s->exclusive) as_send_uid(as_session->client_exclusive_remove, s->uid);
        as_send_uid(as_session->client_remove, s->uid);
    }
    if (s) s->deleting = 1;
    wwhd_fopAcM_delete_actor(c->actor);
    c->used = 0;
}

/* The loaded area's enemies follow the session's mutators (see the header). */
static void as_apply_mutators(void) {
    as_mutators_dirty = 0;
    u32 multiplier = as_session->enemy_multiplier < 1 ? 1 : as_session->enemy_multiplier;
    int randomized = as_session->enemy_randomizer != 0;
    u32 created = 0, removed = 0;
    for (u32 g = 0; g != AS_MAX_GROUPS; ++g) {
        as_group* e = &as_groups[g];
        if (!e->used) continue;
        if (!as_alive(e->actor, e->pcid)) {
            e->used = 0;
            continue;
        }
        int redo = e->randomized != randomized; /* other variants: all the copies are made again */
        for (u32 c = 0; c != AS_MAX_ENEMY_COPIES; ++c) {
            as_copy_entry* copy = &as_enemy_copies[c];
            if (!copy->used || copy->group != e->id) continue;
            if (!as_alive(copy->actor, copy->pcid)) {
                copy->used = 0;
                continue;
            }
            if (redo || copy->copy >= multiplier) {
                as_drop_copy(copy);
                ++removed;
            }
        }
        u32 made = redo ? 1 : e->made;
        if (made > multiplier) made = multiplier;
        for (u32 n = 0; n != AS_STAGE_NAME; ++n) as_stage_data[n] = (u8)e->name[n];
        for (; made < multiplier; ++made) {
            u32 block = as_make_copy(e->name, e->prm, made, randomized, e->id);
            if (!block) break; /* game heap exhausted */
            as_stage_create_original((u32)as_stage_data, block);
            ++created;
        }
        e->made = (u8)made;
        e->randomized = (u8)randomized;
    }
    wwhd_log_int("multiplayer: mutators applied, enemy multiplier", (int)multiplier);
    wwhd_log_int("multiplayer:   randomizer", randomized);
    wwhd_log_int("multiplayer:   copies made", (int)created);
    wwhd_log_int("multiplayer:   copies deleted", (int)removed);
}

/* Every logic step while in an area with our PlayerPuppet uid known. */
static void as_step(double dt, mp_u32 local_uid, const fopAc_ac_c* link) {
    if (!as_session || !as_enabled) return;
    as_local_uid = local_uid;
    if (as_mutators_dirty) as_apply_mutators();
    as_link = (u32)link;
    as_have_link = link != 0;
    if (link) {
        as_link_pos[0] = link->current.pos.x;
        as_link_pos[1] = link->current.pos.y;
        as_link_pos[2] = link->current.pos.z;
    }
    f32 blend = (f32)(dt / AS_UPDATE_PERIOD);
    if (blend > 1.0f) blend = 1.0f;
    u32 creates = 0;
    for (u32 i = 0; i != AS_MAX; ++i) {
        as_slot* s = &as_slots[i];
        if (!s->used) continue;
        if (s->state == AS_SPAWNING) { /* another player's spawned actor, not created yet */
            as_step_replica(s, dt);
            continue;
        }
        const fopAc_ac_c* a = (const fopAc_ac_c*)s->actor;
        if (*(const volatile u32*)((const u8*)a + AS_PC_ID) != s->pcid) {
            as_free(s); /* deleted without fopAc_Delete (failed create) */
            continue;
        }
        s->group = a->group;
        if (a->health > s->max_health) s->max_health = a->health;
        s->age += dt;
        s->hit_clock += dt;
        s->replay_clock += dt;
        if (s->replayed && s->replay_clock >= AS_REPLAY_WINDOW) s->replayed = 0;
        if (s->local_hit && s->hit_clock >= AS_EXCLUSIVE_HOLD) s->local_hit = 0;

        if (s->state == AS_LOCAL) {
            if (creates != AS_CREATES_PER_STEP && local_uid != MP_NO_ID) {
                as_send_create(s, a);
                ++creates;
            }
            continue;
        }
        if (s->state == AS_PENDING) {
            s->pending_clock += dt;
            if (s->pending_clock >= AS_PENDING_TIMEOUT) s->state = AS_LOCAL;
            continue;
        }
        if (s->state == AS_REMOVED && s->kind == AS_KIND_SPAWNED) {
            if (!s->deleting) { /* picked up or gone at another player: gone here */
                s->deleting = 1;
                wwhd_fopAcM_delete_actor(s->actor);
            }
            continue;
        }
        if (s->replica && !s->spawn_ready) { /* items and cannon bombs get their speeds after creation */
            fopAc_ac_c* w = (fopAc_ac_c*)s->actor;
            if (s->cls != AS_CLS_BALL && s->spawn.proc != AS_PROC_ARROW) {
                w->speedF = s->spawn.speed_f;
                w->speed.x = s->spawn.speed[0];
                w->speed.y = s->spawn.speed[1];
                w->speed.z = s->spawn.speed[2];
                w->gravity = s->spawn.gravity;
            }
            s->spawn_ready = 1;
        }
        as_step_kill(s, a, dt);
        if (s->state == AS_REMOVED && !s->deleting && s->kind == AS_KIND_PLACED &&
            *(const volatile s16*)((const u8*)a + AS_PROC_NAME) == AS_PROC_TSUBO) {
            as_break_pot(s, (fopAc_ac_c*)(u32)a); /* broken in another game */
            continue;
        }
        if (s->state == AS_REMOVED) {
            /* dead at its owner: ours dies of the replayed hits, else it goes after a while */
            s->remove_clock += dt;
            if (!s->deleting && a->health > 0 && s->remove_clock >= AS_REMOVE_WAIT) {
                s->deleting = 1;
                wwhd_fopAcM_delete_actor(s->actor);
            }
            continue;
        }

        if (s->kind == AS_KIND_SPAWNED) {
            /* weapons: the creator holds them from the create on (as_handle_create) */
        } else if (s->local_hit && !s->exclusive && as_session->client_exclusive_add != MP_NO_ID) {
            as_send_uid(as_session->client_exclusive_add, s->uid);
            s->exclusive = 1;
        } else if (s->exclusive && !s->local_hit && as_session->client_exclusive_remove != MP_NO_ID) {
            as_send_uid(as_session->client_exclusive_remove, s->uid);
            s->exclusive = 0;
        }

        if (s->owner == local_uid) {
            as_send_owner(s, a, dt);
        } else if (s->has_target) {
            if (!s->has_display) {
                s->display = s->target;
                s->has_display = 1;
            }
            for (int k = 0; k != 3; ++k) {
                s->display.pos[k] += (s->target.pos[k] - s->display.pos[k]) * blend;
                s->display.rot[k] = as_turn(s->display.rot[k], s->target.rot[k], blend);
            }
        }
    }
}

static void as_log_counts(void) {
    u32 used = 0, synced = 0, owned = 0;
    for (u32 i = 0; i != AS_MAX; ++i) {
        if (!as_slots[i].used) continue;
        ++used;
        if (as_slots[i].state == AS_SYNCED) {
            ++synced;
            if (as_slots[i].owner == as_local_uid) ++owned;
        }
    }
    if (!used) return;
    wwhd_log_int("multiplayer: synchronised actors tracked", (int)used);
    wwhd_log_int("multiplayer:   synchronised", (int)synced);
    wwhd_log_int("multiplayer:   owned here", (int)owned);
}
