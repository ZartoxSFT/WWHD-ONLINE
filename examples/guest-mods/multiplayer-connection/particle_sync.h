/* Particle effects of the local Link, replayed in the other players' games (hammer on the ground,
 * sword sparks and splashes, leaf gusts...).
 *
 * Every particle effect goes through dPa_control_c::set (025A847C, decomp include/bindings.h
 * dPa_control_set: pa, group, id, pos, angle, scale, alpha, level callback, setup, prm and env
 * colours, 2D scale; twelve arguments, the last four on the stack). It is replaced: an emitter
 * made while the local Link or its ship runs its execute (actor_sync.h, AS_CTX_LOCAL) is noted, and
 * right after that execute (ps_flush) its global rotation and translation are read (JPABaseEmitter
 * mGlobalRotation +0x1F0, a 3x4 matrix, mGlobalTranslation +0x22C, decomp
 * include/JSystem/JParticle/JPA_emitter_calc.h): Link's code often puts its emitters on a hand, the
 * sword or the ground after creating them (JPASetRMtxTVecfromMtx, setHammerQuake), and the creation
 * position alone was Link's feet (0.2.27: the sword's sparkle under the puppet). The emitter joins
 * the next ActorPlayerSync update (mp_protocol.h mp_particle, with MP_PARTICLE_MATRIX). The
 * others make it again through the same function and give it that rotation; an endless one
 * (mMaxFrame +0x5C at 0, JPA_emitter_calc.cpp doTerminationProcess: only the code that made it
 * stops it) is made a PS_BURST_FRAMES burst. The hit sparks of a replayed sword hit come from the
 * game's own hit code (dCcS::ProcAtTgHitmark), not from here. */
#pragma once
#include "wwhd_guest.h"
#include "mp_protocol.h"
#include "actor_sync.h"

#define PS_SET 0x025A847C
#define PS_PLAY_PARTICLE 0x5AB0 /* dComIfG_play_c's dPa_control_c* (d_com_inf_game.h PLAY_PARTICLE) */
#define PS_EMITTER_MAX_FRAME 0x5C
#define PS_EMITTER_ROTATION 0x1F0    /* f32[12], a 3x4 matrix */
#define PS_EMITTER_TRANSLATION 0x22C /* f32[3] */
#define PS_BURST_FRAMES 30
#define PS_QUEUE 32
#define PS_PENDING 16

static int ps_replaying;
typedef struct ps_pending_emitter {
    u32 emitter;
    mp_particle particle;
} ps_pending_emitter;
static ps_pending_emitter ps_pending[PS_PENDING];
static u32 ps_pending_count;
static u32 ps_sent, ps_replayed;
static mp_particle ps_queue[PS_QUEUE];
static u32 ps_count;
/* the arguments are passed by guest address */
static cXyz ps_pos, ps_scale;
static csXyz ps_angle;

WWHD_GAME_ORIGINAL(PS_SET, u32, ps_set_original,
                   (u32 pa, u32 group, u32 id, u32 pos, u32 angle, u32 scale, u32 alpha, u32 cb, u32 setup,
                    u32 prm, u32 env, u32 scale2d));

WWHD_REPLACE(PS_SET, u32, ps_set,
             (u32 pa, u32 group, u32 id, u32 pos, u32 angle, u32 scale, u32 alpha, u32 cb, u32 setup,
              u32 prm, u32 env, u32 scale2d)) {
    u32 emitter = ps_set_original(pa, group, id, pos, angle, scale, alpha, cb, setup, prm, env, scale2d);
    if (ps_replaying || !ps_capturing || !as_enabled || !as_session || as_local_uid == MP_NO_ID) return emitter;
    if (!emitter || ps_pending_count == PS_PENDING) return emitter;
    ps_pending_emitter* e = &ps_pending[ps_pending_count++];
    mp_particle* p = &e->particle;
    e->emitter = emitter;
    p->group = (mp_u8)group;
    p->id = (mp_u16)id;
    p->flags = MP_PARTICLE_MATRIX;
    p->alpha = (mp_u8)alpha;
    for (int c = 0; c != 3; ++c) {
        p->angle[c] = 0;
        p->scale[c] = 1.0f;
    }
    if (scale) {
        const cXyz* s = (const cXyz*)scale;
        p->scale[0] = s->x;
        p->scale[1] = s->y;
        p->scale[2] = s->z;
        p->flags |= MP_PARTICLE_SCALE;
    }
    return emitter;
}

/* After the local Link's (or ship's) execute: the noted emitters, where that code put them. */
static void ps_flush(void) {
    for (u32 i = 0; i != ps_pending_count; ++i) {
        ps_pending_emitter* e = &ps_pending[i];
        mp_particle* p = &e->particle;
        const volatile f32* r = (const volatile f32*)(e->emitter + PS_EMITTER_ROTATION);
        const volatile f32* t = (const volatile f32*)(e->emitter + PS_EMITTER_TRANSLATION);
        for (int row = 0; row != 3; ++row)
            for (int col = 0; col != 3; ++col) p->matrix[row * 3 + col] = r[row * 4 + col];
        for (int c = 0; c != 3; ++c) p->pos[c] = t[c];
        anim_local.particle[mp_particle_slot(&anim_local)] = *p;
        if (!ps_sent++) wwhd_log_int("multiplayer: first particle effect shared, id", (int)p->id);
    }
    ps_pending_count = 0;
}

/* Particles another player's Link made, made again at the next step. */
static void ps_queue_particles(const mp_anim_events* e) {
    for (u32 i = 0; i != e->particle_count && ps_count != PS_QUEUE; ++i) ps_queue[ps_count++] = e->particle[i];
}

static void ps_step(void) {
    if (!ps_count) return;
    const u8* play = wwhd_play_get();
    u32 pa = play ? *(const volatile u32*)(play + PS_PLAY_PARTICLE) : 0;
    u32 count = ps_count;
    ps_count = 0;
    if (!pa) return;
    ps_replaying = 1;
    for (u32 i = 0; i != count; ++i) {
        const mp_particle* p = &ps_queue[i];
        ps_pos.x = p->pos[0];
        ps_pos.y = p->pos[1];
        ps_pos.z = p->pos[2];
        ps_angle.x = p->angle[0];
        ps_angle.y = p->angle[1];
        ps_angle.z = p->angle[2];
        ps_scale.x = p->scale[0];
        ps_scale.y = p->scale[1];
        ps_scale.z = p->scale[2];
        u32 em = ps_set_original(pa, p->group, p->id, (u32)&ps_pos,
                                 (p->flags & MP_PARTICLE_ANGLE) ? (u32)&ps_angle : 0,
                                 (p->flags & MP_PARTICLE_SCALE) ? (u32)&ps_scale : 0, p->alpha, 0, 0xFFFFFFFFu, 0, 0, 0);
        if (!em) continue;
        if (p->flags & MP_PARTICLE_MATRIX) {
            volatile f32* r = (volatile f32*)(em + PS_EMITTER_ROTATION);
            for (int row = 0; row != 3; ++row) {
                for (int col = 0; col != 3; ++col) r[row * 4 + col] = p->matrix[row * 3 + col];
                r[row * 4 + 3] = 0.0f;
            }
        }
        volatile s32* max = (volatile s32*)(em + PS_EMITTER_MAX_FRAME);
        if (*max == 0) *max = PS_BURST_FRAMES; /* an endless one: its maker stops it, a copy's would not */
        if (!ps_replayed++) wwhd_log_int("multiplayer: first particle effect of another player, id", (int)p->id);
    }
    ps_replaying = 0;
}

static void ps_clear(void) {
    ps_count = 0;
    ps_pending_count = 0;
}
