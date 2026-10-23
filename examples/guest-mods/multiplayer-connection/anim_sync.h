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

static int sr_anim_locked(const void* self, int anm); /* ship_ride.h: a seated passenger (0.3.96) */

static int anim_is_local(const void* self) {
    return anim_capture && self && (u32)self == puppet_local_player() && !puppet_is(self);
}

/* 0.3.90: speed changes made by Link's procs on the animation of setSingleMoveAnime, without a new
 * call (decomp): procFrontRollCrash (0241BC90, d_a_player_main_05.cpp) starts ANM_ROLLFMIS at
 * rate 0 and sets mFrameCtrlUnder[0] to 0.7 on landing (getting up: the puppet stayed down);
 * procCrawlMove (d_a_player_main_06.cpp) sets its rate from the stick each step, 0 and a frame when
 * stopped (the puppet crawled on). After the local Link's execute, a change of that rate is sent as
 * one more single animation event, its index + ANIM_RATE_FLAG (an older mod drops it: past
 * ANIM_COUNT), rate, start = the frame, end = the loop frame, morf 0. The server relays the single
 * animations as they are (ActorPlayerSync.java SingleMoveAnimPacket). */
#define ANIM_COUNT 0xEA /* (see the replay below) */
#define ANIM_RATE_FLAG 0x100u
#define ANIM_RATE_STEP 0.05f
static int anim_rate_watch;   /* the under animation is the last setSingleMoveAnime's */
static u32 anim_rate_anm;
static f32 anim_rate_last;

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
    if (sr_anim_locked(self, anm0)) return 1; /* seated on a friend's ship: stays seated */
    if (anim_is_local(self)) {
        mp_move_anim* m = &anim_local.move[mp_anim_slot(&anim_local.move_count, &anim_local.dropped)];
        m->blend = blend;
        m->rate = rate;
        m->rate1 = rate1;
        m->anm0 = (mp_u32)anm0;
        m->anm1 = (mp_u32)anm1;
        m->mode = (mp_u32)mode;
        m->morf = morf;
        anim_rate_watch = 0;
    }
    return anim_move_original(self, blend, rate, rate1, anm0, anm1, mode, morf);
}

WWHD_REPLACE(WWHD_ADDR_daPy_lk_c__setSingleMoveAnime, s32, anim_single_hook,
             (void* self, int anm, f32 rate, f32 start, int end, f32 morf)) {
    if (sr_anim_locked(self, anm)) return 1;
    if (anim_is_local(self)) {
        mp_single_anim* s = &anim_local.single[mp_anim_slot(&anim_local.single_count, &anim_local.dropped)];
        s->anm = (mp_u32)anm;
        s->rate = rate;
        s->start = start;
        s->end = (mp_s16)end;
        s->morf = morf;
        s32 r = anim_single_original(self, anm, rate, start, end, morf);
        anim_rate_watch = 1;
        anim_rate_anm = (u32)anm;
        anim_rate_last = ((const daPy_lk_c*)self)->mFrameCtrlUnder[0].mRate;
        return r;
    }
    return anim_single_original(self, anm, rate, start, end, morf);
}

/* mod.c, after the local Link's execute. */
static void anim_rate_check(void* self) {
    if (!anim_rate_watch || !anim_is_local(self) || anim_rate_anm >= ANIM_COUNT) return;
    const J3DFrameCtrl* fc = &((const daPy_lk_c*)self)->mFrameCtrlUnder[0];
    f32 rate = fc->mRate, d = rate - anim_rate_last;
    int changed = (rate == 0.0f) != (anim_rate_last == 0.0f) || (rate < 0.0f) != (anim_rate_last < 0.0f) ||
                  d > ANIM_RATE_STEP || d < -ANIM_RATE_STEP;
    if (!changed) return;
    anim_rate_last = rate;
    mp_single_anim* s = &anim_local.single[mp_anim_slot(&anim_local.single_count, &anim_local.dropped)];
    s->anm = anim_rate_anm | ANIM_RATE_FLAG;
    s->rate = rate;
    s->start = fc->mFrame;
    s->end = fc->mLoop;
    s->morf = 0.0f;
    static int logged;
    if (logged++ < 4) wwhd_log_int("multiplayer: animations: a speed set by Link's code sent, animation", (int)anim_rate_anm);
}

/* A puppet: the speed (and the frame when it stops, or drifted away) of its under animation. */
static void anim_rate_apply(void* puppet, const mp_single_anim* s) {
    J3DFrameCtrl* fc = &((daPy_lk_c*)puppet)->mFrameCtrlUnder[0];
    if (!(s->start >= (f32)fc->mStart - 1.0f && s->start <= (f32)fc->mEnd + 1.0f)) return; /* another animation */
    fc->mRate = s->rate;
    f32 d = fc->mFrame - s->start;
    if (s->rate == 0.0f || d > 3.0f || d < -3.0f) fc->mFrame = s->start;
    if (s->end >= fc->mStart && s->end <= fc->mEnd) fc->mLoop = s->end;
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
/* 0.3.79: Link's voice and sounds for the puppets. A puppet runs none of Link's procs, where his
 * voice (daPy_lk_c::voiceStart 02443880: startLinkVoice at its eyePos, d_a_player_main_08.cpp) and
 * sounds (seStartOnlyReverb 023DC0F4 at its place, seStartSwordCut 024072F4 at its sword's tip,
 * d_a_player_main.cpp) start. The local Link's calls are recorded (entry hooks) and sent; a puppet
 * replays them on itself, at its own place (the sword's at the puppet: its sword tip is not
 * kept). The sounds of the ground (seStartMapInfo: footsteps on the floor's material) are not
 * sent: they read the floor under the Link, which a puppet does not follow. */
static void anim_sound_note(void* self, mp_u8 kind, u32 id) {
    if (!anim_is_local(self) || anim_local.sound_count == MP_MAX_SOUNDS) return;
    anim_local.sound[anim_local.sound_count].kind = kind;
    anim_local.sound[anim_local.sound_count].id = id;
    ++anim_local.sound_count;
    static int logged;
    if (!logged) {
        logged = 1;
        wwhd_log_int("multiplayer: sounds: the local Link's first sound sent, kind", kind);
    }
}
WWHD_HOOK(0x02443880, anim_voice_note, (void* self, u32 id)) { anim_sound_note(self, MP_SOUND_VOICE, id); }
WWHD_HOOK(0x023DC0F4, anim_reverb_note, (void* self, u32 id)) { anim_sound_note(self, MP_SOUND_REVERB, id); }
WWHD_HOOK(0x024072F4, anim_sword_note, (void* self, u32 id)) { anim_sound_note(self, MP_SOUND_SWORD, id); }

static void anim_apply(void* puppet, const mp_anim_events* e) {
    static int sound_logged;
    if (e->sound_count && !sound_logged) {
        sound_logged = 1;
        wwhd_log("multiplayer: sounds: another player's sounds received and played");
    }
    for (mp_u32 i = 0; i != e->sound_count && i != MP_MAX_SOUNDS; ++i) {
        if (e->sound[i].kind == MP_SOUND_VOICE) {
            /* 0.3.82: the voice starts at eyePos (voiceStart 02443880); the puppet's is set from its eye
             * joint after its execute, never before its first one: a voice far away was not heard
             * (the rolls, at current.pos, were). Kept near the head. */
            fopAc_ac_c* a = (fopAc_ac_c*)puppet;
            f32 dx = a->eyePos.x - a->current.pos.x, dy = a->eyePos.y - a->current.pos.y, dz = a->eyePos.z - a->current.pos.z;
            f32 d2 = dx * dx + dy * dy + dz * dz;
            static int logged;
            if (logged++ < 3) wwhd_log_int("multiplayer: sounds: a voice, the puppet's eyes from its feet", (int)d2);
            if (!(d2 < 300.0f * 300.0f)) {
                a->eyePos.x = a->current.pos.x;
                a->eyePos.y = a->current.pos.y + 110.0f;
                a->eyePos.z = a->current.pos.z;
            }
            wwhd_daPy_lk_c__voiceStart(puppet, e->sound[i].id);
        }
        else wwhd_daPy_lk_c__seStartOnlyReverb(puppet, e->sound[i].id);
    }
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
        if ((s->anm & ANIM_RATE_FLAG) && (s->anm & ~ANIM_RATE_FLAG) < ANIM_COUNT) {
            anim_rate_apply(puppet, s); /* 0.3.90 */
            continue;
        }
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
