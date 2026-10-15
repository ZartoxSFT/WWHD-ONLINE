/* Link animation events (ActorPlayerSync event lists), the HD counterpart of the GameCube
 * client's ActorPlayerSync::Initialize trampolines (writeAnimeData, writeSingleMoveAnime, ...).
 *
 * Capture: the seven daPy_lk_c animation calls are replaced; a call on the local player
 * (player 0, never a puppet) is recorded while anim_capture is set, then the game's own code runs
 * unchanged. HD signatures and addresses: wwhd/bindings.h, wwhd/functions.h.
 *
 * Replay on a remote Link puppet (puppet.h, before its animeUpdate): only the four calls whose HD
 * code (decomp/wwhd_src d_a_player_main_01.cpp, _02.cpp) changes nothing but the Link it is
 * called on: setMoveAnime, setSingleMoveAnime, setActAnimeUpper, resetActAnimeUpper. The three
 * equipment calls are sent for the protocol but not replayed: setAnimeEquipItem reads the LOCAL
 * player's ready item (getReadyItem) and clears a status bit of player 0, and the sword/item
 * models they drive need equipment state the puppet does not have yet. */
#pragma once
#include "wwhd_guest.h"
#include "wwhd/functions.h"
#include "wwhd/link.h"
#include "link_access.h"
#include "mp_protocol.h"

static int anim_capture;          /* set by mod.c while the local PlayerPuppet has a uid */
static mp_anim_events anim_local; /* recorded since the last ActorPlayerSync update */

static int anim_is_local(const void* self) {
    return anim_capture && self && (u32)self == puppet_local_player() && !puppet_is(self);
}

WWHD_GAME_ORIGINAL(WWHD_ADDR_daPy_lk_c__setMoveAnime, int, anim_move_original,
                   (void* self, f32 blend, f32 rate, f32 rate1, int anm0, int anm1, int mode, f32 morf));
WWHD_GAME_ORIGINAL(WWHD_ADDR_daPy_lk_c__setSingleMoveAnime, s32, anim_single_original,
                   (void* self, int anm, f32 rate, f32 start, int end, f32 morf));
WWHD_GAME_ORIGINAL(WWHD_ADDR_daPy_lk_c__setActAnimeUpper, s32, anim_upper_original,
                   (void* self, u32 bck, int upper, f32 rate, f32 start, int end, f32 morf));
WWHD_GAME_ORIGINAL(WWHD_ADDR_daPy_lk_c__resetActAnimeUpper, s32, anim_reset_original,
                   (void* self, int upper, f32 morf));
WWHD_GAME_ORIGINAL(WWHD_ADDR_daPy_lk_c__setAnimeEquipSword, void, anim_sword_original, (void* self, s32 b1));
WWHD_GAME_ORIGINAL(WWHD_ADDR_daPy_lk_c__setAnimeEquipItem, void, anim_equip_original, (void* self));
WWHD_GAME_ORIGINAL(WWHD_ADDR_daPy_lk_c__setAnimeUnequip, void, anim_unequip_original, (void* self));

WWHD_REPLACE(WWHD_ADDR_daPy_lk_c__setMoveAnime, int, anim_move_hook,
             (void* self, f32 blend, f32 rate, f32 rate1, int anm0, int anm1, int mode, f32 morf)) {
    if (anim_is_local(self)) {
        mp_move_anim* m = &anim_local.move[mp_anim_slot(&anim_local.move_count, &anim_local.dropped)];
        m->blend = blend;
        m->rate = rate;
        m->rate1 = rate1;
        m->anm0 = (mp_u32)anm0;
        m->anm1 = (mp_u32)anm1;
        m->mode = (mp_u32)mode;
        m->morf = morf;
    }
    return anim_move_original(self, blend, rate, rate1, anm0, anm1, mode, morf);
}

WWHD_REPLACE(WWHD_ADDR_daPy_lk_c__setSingleMoveAnime, s32, anim_single_hook,
             (void* self, int anm, f32 rate, f32 start, int end, f32 morf)) {
    if (anim_is_local(self)) {
        mp_single_anim* s = &anim_local.single[mp_anim_slot(&anim_local.single_count, &anim_local.dropped)];
        s->anm = (mp_u32)anm;
        s->rate = rate;
        s->start = start;
        s->end = (mp_s16)end;
        s->morf = morf;
    }
    return anim_single_original(self, anm, rate, start, end, morf);
}

WWHD_REPLACE(WWHD_ADDR_daPy_lk_c__setActAnimeUpper, s32, anim_upper_hook,
             (void* self, u32 bck, int upper, f32 rate, f32 start, int end, f32 morf)) {
    if (anim_is_local(self)) {
        mp_upper_anim* u = &anim_local.upper[mp_anim_slot(&anim_local.upper_count, &anim_local.dropped)];
        u->bck = (mp_u16)bck;
        u->upper = (mp_u32)upper;
        u->rate = rate;
        u->start = start;
        u->end = (mp_s16)end;
        u->morf = morf;
    }
    return anim_upper_original(self, bck, upper, rate, start, end, morf);
}

WWHD_REPLACE(WWHD_ADDR_daPy_lk_c__resetActAnimeUpper, s32, anim_reset_hook, (void* self, int upper, f32 morf)) {
    if (anim_is_local(self)) {
        mp_reset_upper* z = &anim_local.reset[mp_anim_slot(&anim_local.reset_count, &anim_local.dropped)];
        z->upper = (mp_u32)upper;
        z->morf = morf;
    }
    return anim_reset_original(self, upper, morf);
}

WWHD_REPLACE(WWHD_ADDR_daPy_lk_c__setAnimeEquipSword, void, anim_sword_hook, (void* self, s32 b1)) {
    if (anim_is_local(self))
        anim_local.sword[mp_anim_slot(&anim_local.sword_count, &anim_local.dropped)] = (mp_u32)b1;
    anim_sword_original(self, b1);
}

WWHD_REPLACE(WWHD_ADDR_daPy_lk_c__setAnimeEquipItem, void, anim_equip_hook, (void* self)) {
    if (anim_is_local(self)) ++anim_local.equip_item_count;
    anim_equip_original(self);
}

WWHD_REPLACE(WWHD_ADDR_daPy_lk_c__setAnimeUnequip, void, anim_unequip_hook, (void* self)) {
    if (anim_is_local(self)) ++anim_local.unequip_count;
    anim_unequip_original(self);
}

/* The upper-body slots are daPy_lk_c::mAnmRatioUpper[3] / mFrameCtrlUpper[3]
 * (UPPER_MOVE0..2_e); a value outside them from the network would index past the arrays. */
#define ANIM_UPPER_SLOTS 3
/* daPy_ANM indices select an mAnmDataTable entry (getAnmData). The GameCube enum ends at
 * ANM_TAKTCHISIN = 0xE9 (WindWakerClient/include/d/actor/d_a_player_main.h); the HD table size
 * is not in the sources, the HD code uses the same indices (setBlendMoveAnime), so a remote
 * index past the GameCube range is dropped rather than read past the table. */
#define ANIM_COUNT 0xEA

/* setMoveAnime restarts its frame controls at the phase it reads from the J3DAnmTransform of
 * mAnmRatioUnder[UNDER_MOVE0_e] (decomp d_a_player_main_02.cpp: frame / getFrameMax). In HD the
 * animation heaps return the archive's resource itself (d_a_player_main_01.cpp
 * Player_getArchiveResource), so that object and its frame are shared by every Link and every
 * slot playing the same animation, and the frame it holds is not always this puppet's. Measured
 * in game (anim_diag.h): a remote run had ~10 phase jumps a second of ~8 frames each, one per
 * received batch, while it advanced at the right speed: the "slow, jerky" run. So while a replayed
 * call keeps the same animations (a running player calls it every step with the same ones), the
 * puppet keeps its own frames and only takes the new rates and weights; a call that changes an
 * animation restarts the phase like the game. Pack layout: anmRatioPack_c {vtbl, mpAnmTransform,
 * count, ratios}, 0x10 bytes (decomp include/d/actor/d_a_player_main_02.h); under packs at
 * mAnmRatioUnder, upper packs at mAnmRatioUpper (SDK). */
#define ANIM_PACK_BYTES 0x10
#define ANIM_PACK_TRANSFORM 0x04

typedef struct anim_phase {
    u32 under[2], upper[2]; /* animations of the move slots */
    f32 under_frame[2], upper_frame[2];
} anim_phase;

static u32 anim_pack(const u8* packs, int i) {
    return *(const volatile u32*)(packs + i * ANIM_PACK_BYTES + ANIM_PACK_TRANSFORM);
}

static void anim_phase_save(const daPy_lk_c* l, anim_phase* p) {
    for (int i = 0; i != 2; ++i) {
        p->under[i] = anim_pack(l->mAnmRatioUnder, i);
        p->upper[i] = anim_pack(l->mAnmRatioUpper, i);
        p->under_frame[i] = l->mFrameCtrlUnder[i].mFrame;
        p->upper_frame[i] = l->mFrameCtrlUpper[i].mFrame;
    }
}

static void anim_phase_keep(daPy_lk_c* l, const anim_phase* p) {
    for (int i = 0; i != 2; ++i)
        if (anim_pack(l->mAnmRatioUnder, i) != p->under[i]) return; /* another animation: new phase */
    for (int i = 0; i != 2; ++i) {
        l->mFrameCtrlUnder[i].mFrame = p->under_frame[i];
        if (anim_pack(l->mAnmRatioUpper, i) == p->upper[i]) l->mFrameCtrlUpper[i].mFrame = p->upper_frame[i];
    }
}

/* Replays the recorded calls on a remote Link puppet, in the GameCube client's order
 * (ActorPlayerSync::calc: move, single, upper, reset). */
static void anim_apply(void* puppet, const mp_anim_events* e) {
    for (mp_u32 i = 0; i != e->move_count; ++i) {
        const mp_move_anim* m = &e->move[i];
        if (m->anm0 >= ANIM_COUNT || m->anm1 >= ANIM_COUNT) continue;
        anim_phase phase;
        anim_phase_save((const daPy_lk_c*)puppet, &phase);
        anim_move_original(puppet, m->blend, m->rate, m->rate1, (int)m->anm0, (int)m->anm1, (int)m->mode, m->morf);
        anim_phase_keep((daPy_lk_c*)puppet, &phase);
    }
    for (mp_u32 i = 0; i != e->single_count; ++i) {
        const mp_single_anim* s = &e->single[i];
        if (s->anm >= ANIM_COUNT) continue;
        anim_single_original(puppet, (int)s->anm, s->rate, s->start, s->end, s->morf);
    }
    for (mp_u32 i = 0; i != e->upper_count; ++i) {
        const mp_upper_anim* u = &e->upper[i];
        if (u->upper >= ANIM_UPPER_SLOTS) continue;
        anim_upper_original(puppet, u->bck, (int)u->upper, u->rate, u->start, u->end, u->morf);
    }
    for (mp_u32 i = 0; i != e->reset_count; ++i) {
        if (e->reset[i].upper >= ANIM_UPPER_SLOTS) continue;
        anim_reset_original(puppet, (int)e->reset[i].upper, e->reset[i].morf);
    }
}
