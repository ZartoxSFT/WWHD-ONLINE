/* Remote Link puppets (experimental, option "puppet", off by default).
 *
 * Like the GameCube client (ActorSyncConfig.cpp ACTORSYNC_CREATE_ACTOR, Hooks.cpp), a remote
 * player is a second real Link actor (daPy_lk_c, process name fpcNm_PLAYER_e = 0xA8 in HD) that
 * never runs the player's logic. Every change to the game is limited to actors carrying
 * PUPPET_PARAMS; any other Link takes the game's own code unchanged.
 *
 * What a second Link would break, from decomp/wwhd_src, and how it is prevented:
 *  - phase_1 (0240EE2C) registers the new Link as player 0 and link player
 *    (play + 0x5B2C, PlayerRegistry {mpPlayer[2], mpLinkPlayer}): the 12 registry bytes are
 *    saved before and restored after the original phase; attention flags are cleared so the
 *    puppet cannot be targeted (GameCube DisablePlayerAttention).
 *  - playerInit (in phase_2, 02412884) calls dEvent_exception_c::setStartDemo (02543534), which
 *    changes the global event state even for "no demo" (index 255): it is answered 255 without
 *    side effects while a puppet initializes (GameCube skipSetDemo, nine call sites).
 *  - playerInit also rewrites the texture table of the SHARED Link model data
 *    (dComIfG_getObjectRes("Link") in initModel; "linktexS3TC" for the casual clothes), and
 *    playerDelete may swap it back: the shared table entries are saved around the puppet's
 *    phase_2 and deletion, so the local Link keeps its clothes.
 *  - makeBgWait (phase_3, 02413E68) writes the restart room into the save, may attach Link to
 *    the ship or start music: the puppet only runs procWait_init (wait animation).
 *  - daPy_lk_c::execute writes global player state (A/R/Do button status, attention list,
 *    actor pointers) even for a Link that is not player 0: the puppet runs only the visual
 *    tail of execute (animeUpdate, setWorldMatrix, setWaistAngle, setHatAngle, setAnimeRatioHD,
 *    checkOriginalHatAnimation, J3DModel::calc, jointCB1, decOldFrameMorfCounter, hair/mask
 *    models, setItemModel, setAttentionPos), as the GameCube skipPuppetUpdate did.
 * Known remaining effects (documented, not crashes): procWait_init clears a draw-status bit of
 * player 0 once per puppet creation; playerDelete clears player 0's boomerang-wait status and
 * the metronome; the eye/mouth texture pattern pointers of the shared model data follow the
 * last Link drawn (possible face animation glitches between the two Links).
 *
 * Layouts: fopAc_ac_c and daPy_lk_c fields from the SDK (wwhd/actor.h, wwhd/link.h); the
 * process name at +8 (s16), J3DModel anm matrix block (+0x2C: flags u16 at +4, matrices at
 * +0x10, 0x30 per joint) and base TR matrix (+0xC8), ModelData texture table (+0x30;
 * TextureTable mNum u16 at 0, mpRes at 4, 0x24 bytes per header), J3DModel::calc (027F4D5C)
 * from decomp/wwhd_src (d_a_player_main_04.cpp, d_a_player_main_04b.h, JSystem/J3DGraph.h). */
#pragma once
#include "wwhd_guest.h"
#include "wwhd/bindings.h"
#include "wwhd/actor.h"
#include "wwhd/link.h"
#include "wwhd/data.h"
#include "mp_protocol.h"
#include "link_access.h"
#include "anim_sync.h"
#include "tunic.h"

#define PUPPET_MAX 4
#define PUPPET_RETRIES 3
#define PUPPET_CREATE_TIMEOUT 300 /* logic steps */
#define PUPPET_REGISTRY_BYTES 12
#define PUPPET_TEX_HEADERS 64
#define PUPPET_TEX_HEADER_BYTES 0x24
#define PUPPET_NO_PROC 0xFFFFFFFFu
#define ACTOR_ATTENTION_FLAGS (0x388 + 0x14)
#define MODEL_ANM_BLOCK 0x2C
#define MODEL_BASE_TR 0xC8
#define MODEL_DATA_TEXTURE 0x30
#define CL_JNT_HEAD 15
#define CL_JNT_EYE 19
/* A puppet is never failed or deleted before daPy_lk_c is constructed (phase_2): deleting it
 * would run playerDelete on uninitialized members. Like the game itself when the camera is
 * missing, the phases wait (cPhs_INIT) instead. */
#define cPhs_INIT 0
#define cPhs_NEXT 2

/* daPy_lk_c::draw enters four function-local static J3D packets into the opaque list
 * (d_a_player_main_01.cpp lk_staticPacketEntry: l_onCupOffAupPacket2, l_offCupOnAupPacket2,
 * l_onCupOffAupPacket1, l_offCupOnAupPacket1; they switch the Z compare to draw the eyes over
 * the hair). A second Link entering the same packet trips J3DDrawBuffer.cpp:243
 * "p_pkt->getEntryPtr() == 0" (observed in game; the GameCube client's playerEyeFix). Each
 * puppet gets its own packets, built like the statics (J3DPacket constructor 027F1278, then the
 * draw vtable at +0xC), and J3DDrawBuffer::entryImm (027F0E04) swaps them in while a puppet
 * draws. Static addresses are canonical (USA) data, translated through WWHD_GAME_DATA. */
#define PUPPET_PACKETS 4
#define PUPPET_PACKET_BYTES 0x98 /* spacing of the statics: the packet fits in it */
#define PACKET_VTABLE 0x0C
WWHD_GAME_FUNC(0x027F1278, void, puppet_j3d_packet_ct, (u32 packet));
WWHD_GAME_ORIGINAL(0x027F0E04, u32, puppet_entry_imm_original, (u32 buffer, u32 packet, u32 index));
WWHD_GAME_ORIGINAL(0x023DBBE4, s32, puppet_draw_original, (void* i_this));

/* Eyes through the other Link. The draw lists sit at play + 0x5D30 (dDlst_list_c; its mpView
 * at +0x274 is the PLAY_VIEW_OFFSET 0x5FA4 of mod.c): daPy_lk_c::draw enters the face and hair
 * (link_root_joint->entryIn) and two eye/eyebrow passes WITHOUT depth test into mpOpaListP0
 * (+0x24, "PLAYER_1"), between packets that switch the colour/alpha channel masks
 * (m_Do_ext_more_anm.cpp: draw modes 7 and 8, the GameCube onCupOffAup/offCupOnAup), so the eyes
 * show only over a Link head; the depth-tested eye pass goes to mpXluListP1 (+0x2C). A puppet
 * head in the same lists made the local Link's eyes show through it, and the eyes cannot be
 * drawn without the depth-less passes (in game: no eyes at all with the depth-tested pass only).
 * Tetra/Zelda uses the same trick in her own pair of lists (d_a_npc_zl1_draw.cpp setListP0 /
 * setListP1: play + 0x5D64 mpOpaListZelda0, 1 bucket "Zelda_1", and play + 0x5D6C
 * mpXluListZelda1 "Zelda_3"), so that her head does not interact with Link's. While a puppet
 * draws, Link's two list pointers point to Zelda's: the puppet draws its eyes exactly like the
 * game, in lists the local Link does not use. Puppets share that pair between them, and with
 * Tetra when she is in the scene. */
#define PLAY_OPA_LIST_P0 0x5D54
#define PLAY_XLU_LIST_P1 0x5D5C
#define PLAY_OPA_LIST_ZELDA0 0x5D64
#define PLAY_XLU_LIST_ZELDA1 0x5D6C

WWHD_GAME_FUNC(0x027F4D5C, void, puppet_j3d_model_calc, (u32 model));
WWHD_GAME_ORIGINAL(0x0240EE2C, s32, puppet_phase_1_original, (void* i_this));
WWHD_GAME_ORIGINAL(0x02412884, s32, puppet_phase_2_original, (void* i_this));
WWHD_GAME_ORIGINAL(0x02413E68, s32, puppet_phase_3_original, (void* i_this));
WWHD_GAME_ORIGINAL(0x0240EE28, s32, puppet_delete_original, (void* i_this));
WWHD_GAME_ORIGINAL(0x02543534, s32, puppet_set_start_demo_original, (u32 self, s32 index));

typedef struct puppet_slot {
    int used;
    mp_u32 uid;
    u32 proc;          /* fopAcM_create process id */
    u32 actor;         /* guest address once created */
    int retries;
    int waiting;       /* steps since the request */
    int has_target;
    mp_transform target;
    tunic_look look;         /* remote tunic colours (ActorPlayerSync colors[0..1]) */
    mp_anim_events pending;  /* animation calls to replay at the next execute */
} puppet_slot;

static int puppet_enabled;
static int puppet_disabled_by_error;
static int puppet_init_depth;
static puppet_slot puppet_slots[PUPPET_MAX];
static u8 puppet_tex_backup[PUPPET_TEX_HEADERS * PUPPET_TEX_HEADER_BYTES];
static u32 puppet_actor_out;
static u8 puppet_packets[PUPPET_MAX][PUPPET_PACKETS][PUPPET_PACKET_BYTES] __attribute__((aligned(32)));
static int puppet_packets_built[PUPPET_MAX];
static int puppet_drawing = -1; /* slot whose Link is in daPy_Draw, -1 otherwise */
static cXyz puppet_create_pos;
static csXyz puppet_create_angle;

static void puppet_log(const char* message) { wwhd_log(message); }

/* ------------------------------------------------------------------ shared texture table */

/* Copies the shared Link texture headers out (save = 1) or back (save = 0). The headers of
 * the model data a Link points to; 0 when the table is missing or larger than the backup. */
static int puppet_texture_table(u32 model_data, int save) {
    if (!model_data) return 0;
    u32 table = *(const volatile u32*)(model_data + MODEL_DATA_TEXTURE);
    if (!table) return 0;
    u32 count = *(const volatile u16*)table;
    u32 headers = *(const volatile u32*)(table + 4);
    if (!headers || count == 0 || count > PUPPET_TEX_HEADERS) return 0;
    if (save) memcpy(puppet_tex_backup, (const void*)headers, count * PUPPET_TEX_HEADER_BYTES);
    else memcpy((void*)headers, puppet_tex_backup, count * PUPPET_TEX_HEADER_BYTES);
    return 1;
}

static u32 puppet_shared_model_data(void) {
    u32 local = puppet_local_player();
    return local ? ((const daPy_lk_c*)local)->mpCLModelData : 0;
}

/* ------------------------------------------------------------------ creation phases */

WWHD_REPLACE(0x0240EE2C, s32, puppet_phase_1, (void* i_this)) {
    if (!puppet_is(i_this)) return puppet_phase_1_original(i_this);
    u8* play = wwhd_play_get();
    if (!play) return cPhs_INIT;
    u8 registry[PUPPET_REGISTRY_BYTES];
    memcpy(registry, play + WWHD_PLAY_PLAYER_OFFSET, sizeof(registry));
    s32 result = puppet_phase_1_original(i_this);
    memcpy(play + WWHD_PLAY_PLAYER_OFFSET, registry, sizeof(registry));
    *(volatile u32*)((u8*)i_this + ACTOR_ATTENTION_FLAGS) = 0;
    return result;
}

WWHD_REPLACE(0x02412884, s32, puppet_phase_2, (void* i_this)) {
    if (!puppet_is(i_this)) return puppet_phase_2_original(i_this);
    u32 shared = puppet_shared_model_data();
    if (!puppet_texture_table(shared, 1)) return cPhs_INIT; /* wait for the local Link */
    ++puppet_init_depth;
    s32 result = puppet_phase_2_original(i_this);
    --puppet_init_depth;
    puppet_texture_table(shared, 0);
    *(volatile u32*)((u8*)i_this + ACTOR_ATTENTION_FLAGS) = 0;
    return result;
}

WWHD_REPLACE(0x02543534, s32, puppet_set_start_demo, (u32 self, s32 index)) {
    if (puppet_init_depth > 0) return 255; /* "no demo", without touching the event state */
    return puppet_set_start_demo_original(self, index);
}

WWHD_REPLACE(0x02413E68, s32, puppet_phase_3, (void* i_this)) {
    if (!puppet_is(i_this)) return puppet_phase_3_original(i_this);
    wwhd_daPy_lk_c__procWait_init(i_this);
    return cPhs_NEXT;
}

WWHD_REPLACE(0x0240EE28, s32, puppet_delete, (void* i_this)) {
    if (!puppet_is(i_this)) return puppet_delete_original(i_this);
    u32 shared = ((const daPy_lk_c*)i_this)->mpCLModelData;
    int saved = puppet_texture_table(shared, 1);
    s32 result = puppet_delete_original(i_this);
    if (saved) puppet_texture_table(shared, 0);
    for (int i = 0; i != PUPPET_MAX; ++i)
        if (puppet_slots[i].actor == (u32)i_this) {
            puppet_slots[i].actor = 0;
            puppet_slots[i].proc = PUPPET_NO_PROC;
        }
    return result;
}

/* ------------------------------------------------------------------ drawing */

static u32 puppet_static_packet(int k) {
    switch (k) { /* WWHD_GAME_DATA needs a constant address */
    case 0: return (u32)&WWHD_GAME_DATA(0x1046CE20, u8);
    case 1: return (u32)&WWHD_GAME_DATA(0x1046CEB8, u8);
    case 2: return (u32)&WWHD_GAME_DATA(0x1046CF50, u8);
    default: return (u32)&WWHD_GAME_DATA(0x1046CFE8, u8);
    }
}

/* Same draw vtables as the statics: l_onCupOffAupPacket* use 10058D40, l_offCupOnAupPacket*
 * 10058D10 (lk_staticPacketEntry arguments). */
static void puppet_build_packets(int slot) {
    if (puppet_packets_built[slot]) return;
    for (int k = 0; k != PUPPET_PACKETS; ++k) {
        u32 packet = (u32)puppet_packets[slot][k];
        memset((void*)packet, 0, PUPPET_PACKET_BYTES);
        puppet_j3d_packet_ct(packet);
        *(volatile u32*)(packet + PACKET_VTABLE) = (k & 1) ? (u32)&WWHD_GAME_DATA(0x10058D10, u8)
                                                           : (u32)&WWHD_GAME_DATA(0x10058D40, u8);
    }
    puppet_packets_built[slot] = 1;
}

static int puppet_slot_of_actor(u32 actor) {
    for (int i = 0; i != PUPPET_MAX; ++i)
        if (puppet_slots[i].used && puppet_slots[i].actor == actor) return i;
    return -1;
}

WWHD_REPLACE(0x027F0E04, u32, puppet_entry_imm, (u32 buffer, u32 packet, u32 index)) {
    if (puppet_drawing >= 0) {
        for (int k = 0; k != PUPPET_PACKETS; ++k)
            if (packet == puppet_static_packet(k)) {
                packet = (u32)puppet_packets[puppet_drawing][k];
                break;
            }
    }
    return puppet_entry_imm_original(buffer, packet, index);
}

WWHD_REPLACE(0x023DBBE4, s32, puppet_draw, (void* i_this)) {
    if (!puppet_is(i_this)) return puppet_draw_original(i_this);
    int slot = puppet_slot_of_actor((u32)i_this);
    if (slot < 0) return 1; /* not followed yet, or an orphan about to delete itself: not drawn */
    u8* play = wwhd_play_get();
    if (!play) return 1;
    volatile u32* list_p0 = (volatile u32*)(play + PLAY_OPA_LIST_P0);
    volatile u32* list_p1 = (volatile u32*)(play + PLAY_XLU_LIST_P1);
    u32 saved_p0 = *list_p0, saved_p1 = *list_p1;
    puppet_build_packets(slot);
    tunic_swap_begin(slot, ((const daPy_lk_c*)i_this)->mpCLModelData);
    *list_p0 = *(const volatile u32*)(play + PLAY_OPA_LIST_ZELDA0);
    *list_p1 = *(const volatile u32*)(play + PLAY_XLU_LIST_ZELDA1);
    puppet_drawing = slot;
    s32 result = puppet_draw_original(i_this);
    puppet_drawing = -1;
    *list_p0 = saved_p0;
    *list_p1 = saved_p1;
    tunic_swap_end();
    return result;
}

/* ------------------------------------------------------------------ per-step update */

static const f32* puppet_anm_mtx(u32 model, u32 joint) {
    u32 block = *(const volatile u32*)(model + MODEL_ANM_BLOCK);
    if (!block) return 0;
    *(volatile u16*)(block + 4) |= 0x10; /* as lk_getAnmMtx: marks the matrices read */
    u32 matrices = *(const volatile u32*)(block + 0x10);
    return matrices ? (const f32*)(matrices + joint * 0x30) : 0;
}

static void puppet_follow_head(u32 model, const f32* head) {
    if (!model || !head) return;
    memcpy((void*)(model + MODEL_BASE_TR), head, 12 * sizeof(f32));
    puppet_j3d_model_calc(model);
}

/* A created puppet no slot owns (its player left while it was being created, or puppets were
 * turned off): it deletes itself, unless a creation is still pending and may be this one. */
static int puppet_is_orphan(u32 actor) {
    if (!puppet_enabled || puppet_disabled_by_error) return 1;
    for (int i = 0; i != PUPPET_MAX; ++i) {
        const puppet_slot* s = &puppet_slots[i];
        if (!s->used) continue;
        if (s->actor == actor) return 0;
        if (s->proc != PUPPET_NO_PROC && s->actor == 0) return 0;
    }
    return 1;
}

/* Replaces daPy_lk_c::execute for a puppet: network pose, then the visual tail only. */
static s32 puppet_execute(void* i_this) {
    fopAc_ac_c* actor = (fopAc_ac_c*)i_this;
    const daPy_lk_c* link = (const daPy_lk_c*)i_this;
    *(volatile u32*)((u8*)i_this + ACTOR_ATTENTION_FLAGS) = 0;
    if (puppet_is_orphan((u32)i_this)) {
        wwhd_fopAcM_delete_actor((u32)i_this); /* fully created here: a normal deletion */
        return 1;
    }
    if (!link->mpCLModel || !link->mpCLModelData) return 1;

    for (int i = 0; i != PUPPET_MAX; ++i) {
        puppet_slot* s = &puppet_slots[i];
        if (!s->used || s->actor != (u32)i_this) continue;
        /* the owner's animation calls, before animeUpdate advances the frames */
        anim_apply(i_this, &s->pending);
        mp_anim_events_clear(&s->pending);
        if (!s->has_target) continue;
        actor->old.pos = actor->current.pos;
        actor->current.pos.x = s->target.pos[0];
        actor->current.pos.y = s->target.pos[1];
        actor->current.pos.z = s->target.pos[2];
        actor->shape_angle.x = s->target.rot[0];
        actor->shape_angle.y = s->target.rot[1];
        actor->shape_angle.z = s->target.rot[2];
        actor->current.angle.y = s->target.rot[1];
    }

    wwhd_daPy_lk_c__animeUpdate(i_this);
    wwhd_daPy_lk_c__setWorldMatrix(i_this);
    wwhd_daPy_lk_c__setWaistAngle(i_this);
    wwhd_daPy_lk_c__setHatAngle(i_this);
    wwhd_daPy_lk_c__setAnimeRatioHD(i_this);
    wwhd_daPy_lk_c__checkOriginalHatAnimation(i_this);
    puppet_j3d_model_calc(link->mpCLModel);
    wwhd_daPy_lk_c__jointCB1(i_this);
    /* As execute does after jointCB1 (d_a_player_main_04.cpp): without it the blend towards
     * the pose of the previous frame, started by every setMoveAnime morph, never ends, and the
     * puppet's run looked like slow motion. */
    if (link->m_old_fdata) wwhd_ext_decOldCounter((void*)link->m_old_fdata);
    const f32* head = puppet_anm_mtx(link->mpCLModel, CL_JNT_HEAD);
    puppet_follow_head(link->mpKatsuraModel, head);
    puppet_follow_head(link->mpYamuModel, head);
    wwhd_daPy_lk_c__setItemModel(i_this);
    const f32* eye = puppet_anm_mtx(link->mpCLModel, CL_JNT_EYE);
    if (eye) {
        actor->eyePos.x = eye[3];
        actor->eyePos.y = eye[7];
        actor->eyePos.z = eye[11];
    }
    wwhd_daPy_lk_c__setAttentionPos(i_this);
    *(volatile u32*)((u8*)i_this + ACTOR_ATTENTION_FLAGS) = 0;
    return 1;
}

/* ------------------------------------------------------------------ lifecycle */

static puppet_slot* puppet_slot_for(mp_u32 uid, int create) {
    puppet_slot* free_slot = 0;
    for (int i = 0; i != PUPPET_MAX; ++i) {
        if (puppet_slots[i].used && puppet_slots[i].uid == uid) return &puppet_slots[i];
        if (!puppet_slots[i].used && !free_slot) free_slot = &puppet_slots[i];
    }
    if (!create || !free_slot) return 0;
    free_slot->used = 1;
    free_slot->uid = uid;
    free_slot->proc = PUPPET_NO_PROC;
    free_slot->actor = 0;
    free_slot->retries = 0;
    free_slot->waiting = 0;
    free_slot->has_target = 0;
    free_slot->look = tunic_look_make(TUNIC_DEFAULT_PRIMARY, TUNIC_DEFAULT_SECONDARY);
    mp_anim_events_clear(&free_slot->pending);
    return free_slot;
}

/* Only a puppet found fully created is deleted here (delete_id is safe for an id that is gone);
 * one still being created is left to delete itself as an orphan once constructed. */
static void puppet_release(puppet_slot* s) {
    if (s->proc != PUPPET_NO_PROC && s->actor != 0) wwhd_fopAcM_delete_id(s->proc);
    s->used = 0;
    s->actor = 0;
    s->proc = PUPPET_NO_PROC;
}

static void puppet_remove(mp_u32 uid) {
    puppet_slot* s = puppet_slot_for(uid, 0);
    if (s) puppet_release(s);
}

static void puppet_clear_all(void) {
    for (int i = 0; i != PUPPET_MAX; ++i)
        if (puppet_slots[i].used) puppet_release(&puppet_slots[i]);
}

static void puppet_set_target(mp_u32 uid, const mp_transform* t) {
    if (!puppet_enabled || puppet_disabled_by_error) return;
    puppet_slot* s = puppet_slot_for(uid, 1);
    if (!s) return;
    s->target = *t;
    s->has_target = 1;
}

/* Remote tunic colours (from ActorPlayerSync), drawn on that player's puppet. */
static void puppet_set_look(mp_u32 uid, mp_u16 primary, mp_u16 secondary) {
    puppet_slot* s = puppet_slot_for(uid, 0);
    if (s) s->look = tunic_look_make(primary, secondary);
}

/* Animation calls received for a player; kept only while its puppet exists (a puppet created
 * later starts from its wait animation instead of replaying a stale burst). */
static void puppet_queue_events(mp_u32 uid, const mp_anim_events* e) {
    puppet_slot* s = puppet_slot_for(uid, 0);
    if (s && s->actor) mp_anim_events_append(&s->pending, e);
}

static void puppet_disable(const char* reason) {
    puppet_log(reason);
    puppet_disabled_by_error = 1;
    puppet_clear_all();
}

/* Creates missing puppets and follows pending requests. Called from the local Link's step,
 * outside events, in the local Link's room. */
static void puppet_step(const fopAc_ac_c* local) {
    if (!puppet_enabled || puppet_disabled_by_error || !local) return;
    const u8* play = wwhd_play_get();
    if (!play || play[WWHD_PLAY_EVENT_RUNNING_OFFSET]) return;
    if (!puppet_shared_model_data()) return;

    for (int i = 0; i != PUPPET_MAX; ++i) {
        puppet_slot* s = &puppet_slots[i];
        if (!s->used || !s->has_target) continue;

        if (s->proc != PUPPET_NO_PROC) {
            puppet_actor_out = 0;
            s32 found = wwhd_fopAcM_SearchByID(s->proc, (u32)&puppet_actor_out);
            if (found && puppet_actor_out) {
                if (!puppet_is((const void*)puppet_actor_out)) {
                    puppet_disable("multiplayer: puppet id resolved to another actor, puppets disabled");
                    return;
                }
                s->actor = puppet_actor_out;
                s->retries = 0;
                s->waiting = 0;
                tunic_prepare_slot(i, &s->look);
                continue;
            }
            if (found) { /* still creating: never deleted before it is constructed */
                if (++s->waiting == PUPPET_CREATE_TIMEOUT)
                    puppet_log("multiplayer: puppet creation is taking long (waiting for the game)");
                continue;
            }
            /* gone (room unloaded or the game deleted it): forget it and create it again */
            s->actor = 0;
            s->proc = PUPPET_NO_PROC;
            if (++s->retries > PUPPET_RETRIES) {
                puppet_log("multiplayer: puppet creation failed repeatedly for one player");
                s->has_target = 0; /* keep the marker only */
                continue;
            }
        }

        puppet_create_pos.x = s->target.pos[0];
        puppet_create_pos.y = s->target.pos[1];
        puppet_create_pos.z = s->target.pos[2];
        puppet_create_angle.x = 0;
        puppet_create_angle.y = s->target.rot[1];
        puppet_create_angle.z = 0;
        s->waiting = 0;
        s->proc = wwhd_fopAcM_create(PUPPET_PROC_PLAYER, PUPPET_PARAMS, (u32)&puppet_create_pos,
                                     local->current.roomNo, (u32)&puppet_create_angle, 0,
                                     0xFFFFFFFFu, 0);
        if (s->proc == PUPPET_NO_PROC) puppet_log("multiplayer: puppet creation request refused");
        else wwhd_log_int("multiplayer: puppet requested for uid", (int)s->uid);
    }
}
