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
#include "outfit.h"
#include "anim_sync.h"
#include "tunic.h"
#include "skin.h"

#define PUPPET_MAX 8 /* the remote players and the dead players' bodies (death.h) */
#define PUPPET_BODY_UID 0x40000000u /* a body's slot: this bit and its player's uid (death.h) */
static int puppet_local_ghost;      /* death.h: the local Link is a ghost */
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
    mp_u32 remote_proc;      /* the owner's current proc (ActorPlayerSync curProc) */
    mp_u16 remote_item;      /* the owner's mEquipItem (ActorPlayerSync item) */
    mp_u8 remote_extras;     /* MP_EXTRA_* */
    mp_u16 shown_item;       /* the item the puppet holds now (PUPPET_ITEM_NONE: nothing) */
    char skin[MP_SKIN_MAX + 1]; /* the owner's skin (skin.h), "" for Link */
    int ghost;               /* the owner is dead (death.h): drawn one logic step in two */
    u32 posed;               /* a body (death.h): the actor its death animation was given to */
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

/* Player 0's status words (play + 0x5CD8 / 0x5CDC, dComIfGp mPlayerStatus[0][0..1]: SHIP_RIDE
 * 0x10000 in the first, SAIL 0x400 and 0x100000 in the second): the game's code run for a puppet
 * writes them as for player 0 (procWait_init clears 0x100000 of the second, d_a_player_main_02.cpp;
 * create and delete set and clear others). Kept around every creation phase and the deletion of a
 * puppet: the local Link sailing was frozen in place when another player joined (until an item reset
 * his state). */
#define PUPPET_PLAY_STATUS_WORDS 0x5CD8
typedef struct puppet_status_keep {
    u32 words[2];
    int kept;
} puppet_status_keep;

static void puppet_status_save(puppet_status_keep* k) {
    const u8* play = wwhd_play_get();
    k->kept = play != 0;
    if (!play) return;
    k->words[0] = *(const volatile u32*)(play + PUPPET_PLAY_STATUS_WORDS);
    k->words[1] = *(const volatile u32*)(play + PUPPET_PLAY_STATUS_WORDS + 4);
}

static void puppet_status_restore(const puppet_status_keep* k) {
    u8* play = wwhd_play_get();
    if (!k->kept || !play) return;
    *(volatile u32*)(play + PUPPET_PLAY_STATUS_WORDS) = k->words[0];
    *(volatile u32*)(play + PUPPET_PLAY_STATUS_WORDS + 4) = k->words[1];
}

/* A puppet's skin: the one of the slot that requested it (fopAcM_create gave the slot the id
 * the actor carries), when it is loaded; else Link (it loads at the next scene load). Returns 0. */
static int puppet_skin_create(void* i_this) {
    if (skin_bound((u32)i_this)) return 0;
    u32 id = *(const volatile u32*)((const u8*)i_this + SKIN_PROC_ID);
    const char* skin = "";
    for (int i = 0; i != PUPPET_MAX; ++i)
        if (puppet_slots[i].used && puppet_slots[i].proc == id) skin = puppet_slots[i].skin;
    skin_bind((u32)i_this, skin_want(skin));
    return 0;
}

WWHD_REPLACE(0x0240EE2C, s32, puppet_phase_1, (void* i_this)) {
    if (!puppet_is(i_this)) {
        if (skin_local_create((u32)i_this)) return cPhs_INIT; /* its skin is loading */
        int skin = skin_enter((u32)i_this);
        s32 result = puppet_phase_1_original(i_this);
        skin_leave(skin);
        return result;
    }
    u8* play = wwhd_play_get();
    if (!play) return cPhs_INIT;
    if (puppet_skin_create(i_this)) return cPhs_INIT;
    u8 registry[PUPPET_REGISTRY_BYTES];
    memcpy(registry, play + WWHD_PLAY_PLAYER_OFFSET, sizeof(registry));
    puppet_status_keep status;
    puppet_status_save(&status);
    int skin = skin_enter((u32)i_this);
    s32 result = puppet_phase_1_original(i_this);
    skin_leave(skin);
    puppet_status_restore(&status);
    memcpy(play + WWHD_PLAY_PLAYER_OFFSET, registry, sizeof(registry));
    *(volatile u32*)((u8*)i_this + ACTOR_ATTENTION_FLAGS) = 0;
    return result;
}

WWHD_REPLACE(0x02412884, s32, puppet_phase_2, (void* i_this)) {
    int skin = skin_enter((u32)i_this); /* its model from its skin (skin.h) */
    if (!puppet_is(i_this)) {
        s32 result = puppet_phase_2_original(i_this);
        skin_leave(skin);
        return result;
    }
    u32 shared = puppet_shared_model_data();
    if (!puppet_texture_table(shared, 1)) { /* wait for the local Link */
        skin_leave(skin);
        return cPhs_INIT;
    }
    puppet_status_keep status;
    puppet_status_save(&status);
    ++puppet_init_depth;
    s32 result = puppet_phase_2_original(i_this);
    --puppet_init_depth;
    puppet_status_restore(&status);
    skin_leave(skin);
    puppet_texture_table(shared, 0);
    *(volatile u32*)((u8*)i_this + ACTOR_ATTENTION_FLAGS) = 0;
    return result;
}

WWHD_REPLACE(0x02543534, s32, puppet_set_start_demo, (u32 self, s32 index)) {
    if (puppet_init_depth > 0) return 255; /* "no demo", without touching the event state */
    return puppet_set_start_demo_original(self, index);
}

WWHD_REPLACE(0x02413E68, s32, puppet_phase_3, (void* i_this)) {
    int skin = skin_enter((u32)i_this);
    if (!puppet_is(i_this)) {
        s32 result = puppet_phase_3_original(i_this);
        skin_leave(skin);
        return result;
    }
    puppet_status_keep status;
    puppet_status_save(&status);
    wwhd_daPy_lk_c__procWait_init(i_this);
    puppet_status_restore(&status);
    skin_leave(skin);
    return cPhs_NEXT;
}

WWHD_REPLACE(0x0240EE28, s32, puppet_delete, (void* i_this)) {
    int skin = skin_enter((u32)i_this);
    if (!puppet_is(i_this)) {
        s32 result = puppet_delete_original(i_this);
        skin_leave(skin);
        skin_unbind((u32)i_this); /* its skin may be unloaded once no Link uses it */
        return result;
    }
    u32 shared = ((const daPy_lk_c*)i_this)->mpCLModelData;
    int saved = puppet_texture_table(shared, 1);
    puppet_status_keep status;
    puppet_status_save(&status);
    s32 result = puppet_delete_original(i_this);
    puppet_status_restore(&status);
    if (saved) puppet_texture_table(shared, 0);
    skin_leave(skin);
    skin_unbind((u32)i_this);
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

/* A created puppet drawn as Link (no skin): its model data has the Link texture the puppets without
 * skin share, the tunic source while the local Link wears a skin (mod.c). 0 when none. */
static u32 puppet_unskinned_actor(void) {
    for (int i = 0; i != PUPPET_MAX; ++i) {
        u32 a = puppet_slots[i].actor;
        if (puppet_slots[i].used && a && puppet_is((const void*)a) && skin_of(a) < 0) return a;
    }
    return 0;
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

/* The camera's attention status that daPy_lk_c::draw reads for the Link it draws (decomp
 * d_a_player_main_01.cpp lk_camAttnStatus: play + 0x5B00 + idx * 0x34, idx the Link's
 * mCameraInfoIdx at +0x69BC; daPy_lk_c::checkPlayerNoDraw 02444E58 tests bit 2 of the same word,
 * d_a_player_main_08.cpp). A puppet has the local Link's index: with the camera close to the local
 * Link (bit 0x20: aiming the bow or the hookshot, first person) it drew only its sleeves and an ear,
 * and with bit 2 only its hands. Those bits are off for the puppet's draw, then put back. */
#define PUPPET_CAMERA_INDEX 0x69BC
#define PUPPET_CAMERA_STATUS 0x5B00
#define PUPPET_CAMERA_STRIDE 0x34
#define PUPPET_CAMERA_CLOSE 0x22u /* 0x20 close to Link, 2 first person (no draw) */

static volatile u32* puppet_camera_status(const u8* play, const void* link) {
    s32 idx = *(const volatile s32*)((const u8*)link + PUPPET_CAMERA_INDEX);
    if (idx < 0 || idx > 3) return 0;
    return (volatile u32*)(play + PUPPET_CAMERA_STATUS + (u32)idx * PUPPET_CAMERA_STRIDE);
}

/* A model without model data is not drawn, and logged: daPy_lk_c::draw then mDoExt_modelEntryDL /
 * mDoExt_modelUpdateDL (025E2E5C (model) / 025E2DE0 (model, data), decomp m_Do_ext_heap_model.cpp)
 * read null + 0x3C in J3DModel::viewCalc (crashes 16:19 and 16:22, 0.2.61, crash-20261018-161947.log
 * and -162211.log, a Link's model; cause not found in the logs). The J3DModel's model data is at
 * +0xAC (elite.h EL_MODEL_DATA). The log names the model and the Link drawing it. */
#define DRAW_MODEL_DATA 0xAC
static u32 draw_link;          /* the Link whose daPy_lk_c::draw runs, 0 outside */
static u32 draw_guard_logged;
WWHD_GAME_ORIGINAL(0x025E2E5C, void, draw_entry_dl_original, (void* model));
WWHD_GAME_ORIGINAL(0x025E2DE0, void, draw_update_dl_original, (void* model, void* data));

static int draw_model_ok(const void* model) {
    u32 m = (u32)model;
    if (!m || (m & 3)) return 0;
    u32 data = *(const volatile u32*)(m + DRAW_MODEL_DATA);
    return data && !(data & 3);
}

static void draw_guard_log(const void* model) {
    if (draw_guard_logged++ >= 8) return;
    wwhd_log_hex("multiplayer: a model without model data was not drawn, model", (u32)model);
    wwhd_log_hex("multiplayer:   drawn by Link", draw_link);
    wwhd_log_int("multiplayer:   that Link is a puppet", draw_link ? puppet_is((const void*)draw_link) : 0);
    wwhd_log_int("multiplayer:   its skin (-1: Link)", draw_link ? skin_of(draw_link) : -1);
}

WWHD_REPLACE(0x025E2E5C, void, draw_entry_dl, (void* model)) {
    if (!draw_model_ok(model)) {
        draw_guard_log(model);
        return;
    }
    draw_entry_dl_original(model);
}

WWHD_REPLACE(0x025E2DE0, void, draw_update_dl, (void* model, void* data)) {
    if (!draw_model_ok(model)) {
        draw_guard_log(model);
        return;
    }
    draw_update_dl_original(model, data);
}

WWHD_REPLACE(0x023DBBE4, s32, puppet_draw, (void* i_this)) {
    u32 outer_link = draw_link;
    draw_link = (u32)i_this;
    if (!puppet_is(i_this)) {
        int skin = skin_enter((u32)i_this);
        s32 result = puppet_draw_original(i_this);
        skin_leave(skin);
        draw_link = outer_link;
        return result;
    }
    int slot = puppet_slot_of_actor((u32)i_this);
    u8* play = wwhd_play_get();
    if (slot < 0 || !play) { /* not followed yet, or an orphan about to delete itself: not drawn */
        draw_link = outer_link;
        return 1;
    }
    /* a dead player's ghost (death.h): nearly invisible, a flicker of one logic step in two */
    if (puppet_slots[slot].ghost && (wwhd_logic_step() & 1)) {
        draw_link = outer_link;
        return 1;
    }
    volatile u32* list_p0 = (volatile u32*)(play + PLAY_OPA_LIST_P0);
    volatile u32* list_p1 = (volatile u32*)(play + PLAY_XLU_LIST_P1);
    u32 saved_p0 = *list_p0, saved_p1 = *list_p1;
    puppet_build_packets(slot);
    /* its clothes first (the local Link's other clothes while it wears them), then its colours;
     * a skinned puppet has its own model data and textures: neither applies */
    int skinned = skin_of((u32)i_this) >= 0;
    int outfit_swapped = skinned ? 0 : outfit_puppet_begin((puppet_slots[slot].remote_extras & MP_EXTRA_CASUAL) != 0);
    if (!skinned) tunic_swap_begin(slot, ((const daPy_lk_c*)i_this)->mpCLModelData);
    int skin = skin_enter((u32)i_this);
    *list_p0 = *(const volatile u32*)(play + PLAY_OPA_LIST_ZELDA0);
    *list_p1 = *(const volatile u32*)(play + PLAY_XLU_LIST_ZELDA1);
    puppet_drawing = slot;
    volatile u32* camera = puppet_camera_status(play, i_this);
    u32 camera_saved = camera ? *camera : 0;
    if (camera) *camera = camera_saved & ~PUPPET_CAMERA_CLOSE;
    s32 result = puppet_draw_original(i_this);
    if (camera) *camera = camera_saved;
    puppet_drawing = -1;
    skin_leave(skin);
    *list_p0 = saved_p0;
    *list_p1 = saved_p1;
    tunic_swap_end();
    outfit_puppet_end(outfit_swapped);
    draw_link = outer_link;
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

/* The remote player's puppet ship and whether that player rides it, and that ship's matrix this
 * step (ship_puppet.h). */
static u32 ship_puppet_of(mp_u32 uid, int* riding);
static int ship_puppet_matrix(mp_u32 uid, f32* m);

/* A riding Link stands where daPy_lk_c::setShipRidePos (023E287C, d_a_player_main_02.cpp) puts
 * him: the ship body's base matrix * l_ship_offset (10034F84, {0, 15, -35}), l_ship_offset3
 * (10034F9C, {0, 15, -96}) in proc 0xD7, facing the ship's heading. The puppet takes that place
 * from its puppet ship (the function itself also sets the R button status of player 0 and reads
 * the wind, so it is not called): it sails with its ship instead of following the ActorBaseSync
 * places, which arrive at a different time than the ship's. */
#define PUPPET_SHIP_OFFSET 0x10034F84
#define PUPPET_SHIP_OFFSET3 0x10034F9C
#define PUPPET_PROC_SHIP_OFFSET3 0xD7
static f32 puppet_ship_mtx[12] __attribute__((aligned(16)));

/* The visual tail of execute reads the ship of player 0 (dComIfGp ship actor, play + 0x5B3C) and
 * its SHIP_RIDE status (play + 0x5CD8, 0x10000): daPy_lk_c::jointAfterCB 023D7564 and
 * setShipRideArmAngle 023D7198 put a riding Link's hand on that ship's tiller top (+0x720,
 * d_a_player_main_01.cpp). Around a puppet's tail they are the puppet's own: its player's puppet
 * ship and riding flag (without them a puppet's hand went to the local ship's tiller whenever the
 * local Link rode). */
#define PUPPET_PLAY_SHIP 0x5B3C
#define PUPPET_PLAY_STATUS0 0x5CD8
#define PUPPET_STATUS0_RIDE 0x10000u

/* ---- what the owner holds ----
 * daPy_lk_c::makeItemType (023DF600, d_a_player_main_01.cpp) makes the held item's model from
 * mEquipItem, in that Link's own item heap: telescope 0x20, Tingle Tuner 0x21, Wind Waker 0x22,
 * Picto Boxes 0x23 / 0x26, bows 0x27 / 0x35 / 0x36, Skull Hammer 0x33, empty bottle 0x50. The
 * grappling hook 0x25, boomerang 0x2D, hookshot 0x2F and bombs 0x31 are actors of their own that
 * act for player 0 (fopAcM create in makeItemType): not made on a puppet. The sword in hand is
 * mEquipItem 0x103: its blade is made by setSwordModel(1) (023DE0B4, what the game calls when Link
 * draws it: blade, glow, tip models in the item heap; 0.2.18 set only the number and no blade
 * showed). The Deku Leaf in
 * flight is mEquipItem 0x102 with setParachuteFanModel (0241A204), as procFanGlide_init does; the
 * proc itself is not run (it uses magic and sets player 0's status). deleteEquipItem (023DC7AC,
 * argument 0: no sound) drops a made model, with mEquipItem set to none first: its sword case
 * stops the sword music. The iron boots are mNoResetFlg0 (+0x3B8) 0x2000000 (draw and
 * setItemModel).
 *
 * Item heaps (d_a_player_main.cpp setItemHeap 023DDF64, setAnimeHeap 023DDEA8): a Link makes an
 * item model in one of its two item heaps, after freeAll on it; it changes heap at most once per
 * frame (mResetFlg0 +0x3C0, daPyRFlg0_UNK4000 0x4000), so the heap freed is never the one whose
 * model the last frame entered in the draw lists. The game's execute clears mResetFlg0 every frame
 * (d_a_player_main_04.cpp): the puppet's execute did not, so every model was made in the same heap
 * over the last one's packets (0.2.18: "J3DPacket.cpp mEntryPtr == (0)" halt after a few item
 * changes). The puppet now clears it every execute too. */
#define PUPPET_RESET_FLG0 0x3C0
#define PUPPET_ITEM_NONE 0x100
#define PUPPET_ITEM_SWORD 0x103
#define PUPPET_ITEM_FAN_FLY 0x102
/* Link's Deku Leaf gust (daPyProc_FAN_SWING_e 0x92, d_a_player_main_02.cpp procFanSwing_init):
 * the big fan model comes from setFanModel (023E7660), not from the equipped item (makeItemType
 * makes no model for the Deku Leaf in HD, d_a_player_main_01.cpp). A pseudo item number. */
#define PUPPET_ITEM_FAN_SWING 0x104
#define PUPPET_PROC_FAN_SWING 0x92
#define PUPPET_DEKU_LEAF 0x34
#define PUPPET_NO_RESET_FLG0 0x3B8
#define PUPPET_HEAVY_BOOTS 0x2000000u

/* Where the others' boomerang is caught and their hookshot's chain starts, as execute computes them
 * after jointCB1 (d_a_player_main_04.cpp): mBoomerangCatchPos = joint 0 times
 * l_execute_boomerang_catch (10035B18), mHookshotRootPos = joint 8 (left hand) times
 * l_execute_hookshot_root (10035B24). The puppet's execute did not: a boomerang copy flew to a
 * stale point and spun there, a hookshot copy's chain started from nowhere. */
#define PUPPET_ITEM_HOOKSHOT 0x2F
#define PUPPET_HOOKSHOT_ROOT 0x72F0 /* daPy_lk_c::mHookshotRootPos (SDK link.h) */
#define PUPPET_BOOMERANG_CATCH 0x72FC /* daPy_lk_c::mBoomerangCatchPos */
static void puppet_point(const f32* m, const volatile f32* v, volatile f32* out) {
    out[0] = m[0] * v[0] + m[1] * v[1] + m[2] * v[2] + m[3];
    out[1] = m[4] * v[0] + m[5] * v[1] + m[6] * v[2] + m[7];
    out[2] = m[8] * v[0] + m[9] * v[1] + m[10] * v[2] + m[11];
}

static void puppet_item_points(daPy_lk_c* link) {
    const f32* root = puppet_anm_mtx(link->mpCLModel, 0);
    const f32* hand = puppet_anm_mtx(link->mpCLModel, 8);
    if (root) puppet_point(root, &WWHD_GAME_DATA(0x10035B18, f32), (volatile f32*)((u32)link + PUPPET_BOOMERANG_CATCH));
    if (hand) puppet_point(hand, &WWHD_GAME_DATA(0x10035B24, f32), (volatile f32*)((u32)link + PUPPET_HOOKSHOT_ROOT));
}

static int puppet_item_has_model(mp_u16 item) {
    switch (item) {
    case 0x20: case 0x21: case 0x22: case 0x23: case 0x26: case 0x27: case 0x35: case 0x36: case 0x33: case 0x50:
        return 1;
    default:
        return 0;
    }
}

static void puppet_apply_item(void* link, puppet_slot* s) {
    mp_u16 want = s->remote_item;
    if (s->remote_proc == PUPPET_PROC_FAN_SWING) want = PUPPET_ITEM_FAN_SWING;
    else if (!puppet_item_has_model(want) && want != PUPPET_ITEM_SWORD && want != PUPPET_ITEM_FAN_FLY &&
             want != PUPPET_ITEM_HOOKSHOT) want = PUPPET_ITEM_NONE;
    if (want != s->shown_item) {
        daPy_lk_c* l = (daPy_lk_c*)link;
        if (s->shown_item != PUPPET_ITEM_NONE) {
            l->mEquipItem = PUPPET_ITEM_NONE; /* never the sword case (it stops the sword music) */
            wwhd_daPy_lk_c__deleteEquipItem(link, 0);
        }
        l->mEquipItem = want == PUPPET_ITEM_FAN_SWING ? PUPPET_DEKU_LEAF : want;
        if (want == PUPPET_ITEM_FAN_SWING) wwhd_daPy_lk_c__setFanModel(link);
        else if (puppet_item_has_model(want)) wwhd_daPy_lk_c__makeItemType(link);
        else if (want == PUPPET_ITEM_SWORD) wwhd_daPy_lk_c__setSwordModel(link, 1);
        else if (want == PUPPET_ITEM_FAN_FLY) wwhd_daPy_lk_c__setParachuteFanModel(link, 1.0f);
        /* the hookshot's own model only (makeItemType would also make a hookshot actor working
         * for player 0, d_a_player_main_01.cpp): its chain is the owner's copy (actor_sync.h) */
        else if (want == PUPPET_ITEM_HOOKSHOT) wwhd_daPy_lk_c__setHookshotModel(link);
        s->shown_item = want;
    }
    volatile u32* flags = (volatile u32*)((u8*)link + PUPPET_NO_RESET_FLG0);
    if (s->remote_extras & MP_EXTRA_HEAVY_BOOTS) *flags |= PUPPET_HEAVY_BOOTS;
    else *flags &= ~PUPPET_HEAVY_BOOTS;
    volatile u32* flags1 = (volatile u32*)((u8*)link + OUTFIT_FLG1); /* its hat (draw) */
    if (s->remote_extras & MP_EXTRA_CASUAL) *flags1 |= OUTFIT_CASUAL;
    else *flags1 &= ~OUTFIT_CASUAL;
}

/* setItemModel (in the visual tail) sets Link's footstep, sword and shield sound types, one
 * global audio state (LinkItemApi::setBootsSoundType 025E1E04, setSwordSoundType 025E1E14,
 * setShieldSoundType 025E1E2C, d_a_player_main_04.cpp): a puppet's would replace the local
 * Link's (its iron boots sounding like normal boots), so they do nothing during a puppet's tail. */
static int puppet_in_tail;
WWHD_GAME_ORIGINAL(0x025E1E04, u32, puppet_boots_sound_original, (u32 type));
WWHD_GAME_ORIGINAL(0x025E1E14, u32, puppet_sword_sound_original, (u32 a, u32 b));
WWHD_GAME_ORIGINAL(0x025E1E2C, u32, puppet_shield_sound_original, (u32 a, u32 b));
WWHD_REPLACE(0x025E1E04, u32, puppet_boots_sound, (u32 type)) {
    return puppet_in_tail ? 0 : puppet_boots_sound_original(type);
}
WWHD_REPLACE(0x025E1E14, u32, puppet_sword_sound, (u32 a, u32 b)) {
    return puppet_in_tail ? 0 : puppet_sword_sound_original(a, b);
}
WWHD_REPLACE(0x025E1E2C, u32, puppet_shield_sound, (u32 a, u32 b)) {
    return puppet_in_tail ? 0 : puppet_shield_sound_original(a, b);
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
    *(volatile u32*)((u8*)i_this + PUPPET_RESET_FLG0) = 0; /* as the game's execute, every frame */

    mp_u32 owner = 0xFFFFFFFFu, owner_proc = PUPPET_NO_PROC;
    for (int i = 0; i != PUPPET_MAX; ++i) {
        puppet_slot* s = &puppet_slots[i];
        if (!s->used || s->actor != (u32)i_this) continue;
        owner = s->uid;
        owner_proc = s->remote_proc;
        puppet_apply_item(i_this, s);
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

    u8* play = wwhd_play_get();
    volatile u32* play_ship = play ? (volatile u32*)(play + PUPPET_PLAY_SHIP) : 0;
    volatile u32* play_status = play ? (volatile u32*)(play + PUPPET_PLAY_STATUS0) : 0;
    u32 saved_ship = play_ship ? *play_ship : 0, saved_status = play_status ? *play_status : 0;
    if (play) {
        int riding = 0;
        u32 own = owner != 0xFFFFFFFFu ? ship_puppet_of(owner, &riding) : 0;
        if (riding && own) {
            *play_ship = own;
            *play_status = saved_status | PUPPET_STATUS0_RIDE;
            if (ship_puppet_matrix(owner, puppet_ship_mtx)) {
                const volatile f32* o = owner_proc == PUPPET_PROC_SHIP_OFFSET3 ? &WWHD_GAME_DATA(PUPPET_SHIP_OFFSET3, f32)
                                                                              : &WWHD_GAME_DATA(PUPPET_SHIP_OFFSET, f32);
                const f32* m = puppet_ship_mtx;
                actor->current.pos.x = m[0] * o[0] + m[1] * o[1] + m[2] * o[2] + m[3];
                actor->current.pos.y = m[4] * o[0] + m[5] * o[1] + m[6] * o[2] + m[7];
                actor->current.pos.z = m[8] * o[0] + m[9] * o[1] + m[10] * o[2] + m[11];
                actor->shape_angle.y = ((const fopAc_ac_c*)own)->shape_angle.y;
                actor->current.angle.y = actor->shape_angle.y;
            }
        } else {
            *play_status = saved_status & ~PUPPET_STATUS0_RIDE;
        }
    }
    ++puppet_in_tail;
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
    puppet_item_points((daPy_lk_c*)i_this);
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
    --puppet_in_tail;
    if (play) {
        *play_ship = saved_ship;
        *play_status = saved_status;
    }
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
    free_slot->remote_proc = PUPPET_NO_PROC;
    free_slot->remote_item = PUPPET_ITEM_NONE;
    free_slot->remote_extras = 0;
    free_slot->shown_item = PUPPET_ITEM_NONE;
    free_slot->ghost = 0;
    free_slot->posed = 0;
    for (int k = 0; k != MP_SKIN_MAX + 1; ++k) free_slot->skin[k] = 0;
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

/* Remote tunic colours and proc (from ActorPlayerSync), for that player's puppet. */
static void puppet_set_look(mp_u32 uid, mp_u16 primary, mp_u16 secondary, mp_u32 proc, mp_u16 item, mp_u8 extras,
                            const char* skin) {
    puppet_slot* s = puppet_slot_for(uid, 0);
    if (!s) return;
    skin_note_remote(skin); /* loaded at the next scene load (skin.h) */
    if (!skin_same_id(s->skin, skin)) {
        skin_copy_id(s->skin, skin);
        /* another model already loaded (or back to Link): the puppet is made again from it (one being
         * created deletes itself as an orphan, puppet_is_orphan); else it stays as it is until the
         * next area, where the new skin is loaded */
        int shown = s->actor ? skin_of(s->actor) : SKIN_NONE;
        if (skin_want(skin) != shown) {
            if (s->proc != PUPPET_NO_PROC && s->actor != 0) wwhd_fopAcM_delete_id(s->proc);
            s->actor = 0;
            s->proc = PUPPET_NO_PROC;
            s->retries = 0;
            s->shown_item = PUPPET_ITEM_NONE;
            wwhd_log_int("multiplayer: a player changed skin, puppet made again, uid", (int)uid);
        }
    }
    s->look = tunic_look_make(primary, secondary);
    s->remote_proc = proc;
    s->remote_item = item;
    s->remote_extras = extras;
}

static void puppet_set_ghost(mp_u32 uid, int ghost) {
    puppet_slot* s = puppet_slot_for(uid, 0);
    if (s) s->ghost = ghost;
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
