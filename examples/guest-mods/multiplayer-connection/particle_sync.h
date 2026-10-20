/* Particle effects of the local Link, replayed in the other players' games (hammer on the ground,
 * sword sparks and splashes, leaf gusts, the sword's charge and spin...).
 *
 * Every particle effect goes through dPa_control_c::set (025A847C, decomp include/bindings.h
 * dPa_control_set: pa, group, id, pos, angle, scale, alpha, level callback, setup, prm and env
 * colours, 2D scale; twelve arguments, the last four on the stack). It is replaced: an emitter
 * made while the local Link or its ship runs its execute (actor_sync.h, AS_CTX_LOCAL) is noted.
 *
 * One-shot emitters (no callback): right after that execute (ps_flush) their global rotation and
 * translation are read (JPABaseEmitter mGlobalRotation +0x1F0, a 3x4 matrix, mGlobalTranslation
 * +0x22C, decomp include/JSystem/JParticle/JPA_emitter_calc.h): Link's code often puts them on a
 * hand, the sword or the ground after creating them (JPASetRMtxTVecfromMtx, setHammerQuake). The
 * others make them again and give them that rotation; an endless one (mMaxFrame +0x5C at 0) is
 * made a PS_BURST_FRAMES burst.
 *
 * Followed emitters (0.2.41): made with a callback, they are moved every frame by it and stopped
 * by their maker. daPy_mtxFollowEcallBack_c (d_a_player.cpp 023D4528: execute copies a matrix of
 * Link's into the emitter's global rotation and translation; 023D4538 end: mMaxFrame -1, mFlags |
 * StopEmit 0x01, & ~Immortal 0x40, callback cleared) makes the sword's charge (CHARGEPOWER00/01,
 * d_a_player_main_08.cpp); the spin attack's ROUNDATTACK00..02 have Link's cut turn callback
 * (d_a_player_main_04.cpp, with their colours and the room). In 0.2.40 they were sent once, at
 * their creation place before the callback had moved them, and replayed as a fixed burst: in the
 * wrong place, at the wrong time. Now each gets a key: one step after its creation (once its
 * callback has placed it) its record goes with its colours, then its place each step it moves (at
 * least every PS_KEEPALIVE steps), then a stop when it ended (its callback is no longer the one it
 * was made with, or mFlags has StopEmit or EnableDeleteEmitter 0x08, or mMaxFrame < 0:
 * JPA_emitter_calc.cpp doTerminationProcess). The others make it Immortal (it never deletes itself
 * while they move it), move it, and end it the way end() does; one not heard of for
 * PS_FOLLOW_TIMEOUT steps (its maker left) is ended too. The callback itself is not replayed: the
 * spin's particles follow its emitter, not the sword's blur.
 * The hit sparks of a replayed sword hit come from the game's own hit code (dCcS::ProcAtTgHitmark),
 * not from here. */
#pragma once
#include "wwhd_guest.h"
#include "mp_protocol.h"
#include "actor_sync.h"

#define PS_SET 0x025A847C
#define PS_PLAY_PARTICLE 0x5AB0 /* dComIfG_play_c's dPa_control_c* (d_com_inf_game.h PLAY_PARTICLE) */
#define PS_EMITTER_MAX_FRAME 0x5C
#define PS_EMITTER_ROTATION 0x1F0    /* f32[12], a 3x4 matrix */
#define PS_EMITTER_CALLBACK 0x1E4    /* mpEmitterCallBack */
#define PS_EMITTER_TRANSLATION 0x22C /* f32[3] */
#define PS_EMITTER_FLAGS 0x254
#define PS_STOP_EMIT 0x01
#define PS_DELETE_ENABLED 0x08
#define PS_IMMORTAL 0x40
#define PS_BURST_FRAMES 30
#define PS_QUEUE 48
#define PS_PENDING 16
#define PS_FOLLOW_MAX 12
#define PS_KEEPALIVE 10          /* steps between two places of an unmoving followed emitter */
#define PS_FOLLOW_TIMEOUT 45     /* steps without news before a copy is ended */
/* 0.3.10: a followed emitter that ends by itself (its frames run out: a burning Link's flames) may keep
 * its callback and flags in the freed memory, so its maker never saw it end and kept sending its place:
 * the others' copy burned on (a puppet in flames for good). A followed emitter is ended after
 * PS_FOLLOW_MAX_STEPS at its maker (a stop is sent) and a copy after PS_COPY_MAX_STEPS (whatever comes). */
#define PS_FOLLOW_MAX_STEPS 300  /* 10 s */
#define PS_COPY_MAX_STEPS 450    /* 15 s */
#define PS_MOVE_EPSILON 0.5f

static int ps_replaying;
typedef struct ps_pending_emitter {
    u32 emitter;
    mp_particle particle;
} ps_pending_emitter;
static ps_pending_emitter ps_pending[PS_PENDING];
static u32 ps_pending_count;
static u32 ps_sent, ps_replayed, ps_followed;

/* the local Link's followed emitters */
typedef struct ps_follow {
    u32 emitter;
    u32 callback;               /* the callback it was made with */
    unsigned long long born;    /* logic step of its creation */
    int started;                /* its first record went */
    int idle;                   /* steps since its last record */
    float last[12];             /* rotation 3x3, translation */
    mp_particle particle;       /* group, id, scale, alpha, colours, key */
} ps_follow;
static ps_follow ps_follows[PS_FOLLOW_MAX];
static u32 ps_follow_count;
static mp_u8 ps_next_key;
static unsigned long long ps_follow_step_done;

/* 0.3.15: the spin attack's and the hurricane spin's trail (ROUNDATTACK00, 01, 02: group 1, ids 0x25,
 * 0x26, 0x28, made in daPy_lk_c::setCollision while daPyProc_CUT_TURN_e 0x55 or CUT_ROLL_e 0x56,
 * d_a_player_main_04.cpp). Those emitters make no particle by themselves: their callback
 * dPa_cutTurnEcallBack_c (d_particle_callbacks.cpp executeAfter 025A9C3C; layout d_particle_local.h:
 * vtable +0, alpha +4, end flag +5, s16 count +6, positions +8, emitter +0xC) creates one particle at
 * each position Link gives it each frame (count, positions: the sword's trail) and puts the emitter on
 * the newest; end (025A9D64) sets the flag, and the next executeAfter stops the emitter. A copy without
 * that callback showed nothing. A copy of those now gets its own callback object (the vtable the local
 * Link's m331C, +0x6730, holds), fed every step with PS_CUT_POINTS positions from the last place to
 * the new place of the sword's tip of its maker's puppet (its right hand cl_RhandA 12, PS_CUT_REACH
 * further out from the puppet's body); ended by its flag. */
#define PS_CUT_POINTS 5
#define PS_CUT_REACH 70.0f
#define PS_CUTS 6
#define PS_CUT_VTABLE 0x6730       /* daPy_lk_c::m331C: a dPa_cutTurnEcallBack_c */
#define PS_CUT_END_STEPS 10        /* steps an ended object is kept: its emitter's last executeAfter */
typedef struct ps_cut {
    u8 obj[0x10];                  /* the dPa_cutTurnEcallBack_c the game calls */
    f32 pos[PS_CUT_POINTS * 3];
    f32 last[3];
    int has_last;
    int used;
    int ending;                    /* steps left before the slot is free again */
} ps_cut;
static ps_cut ps_cuts[PS_CUTS] __attribute__((aligned(8)));

static int ps_is_cut_turn(const mp_particle* p) {
    return p->group == 1 && (p->id == 0x25 || p->id == 0x26 || p->id == 0x28);
}

/* the other players' followed emitters, here */
typedef struct ps_copy {
    mp_u32 uid;
    mp_u8 key;
    u32 emitter;
    unsigned long long heard;
    unsigned long long made;    /* its creation step */
    int cut;                    /* its ps_cuts slot, -1 for none */
} ps_copy;
static ps_copy ps_copies[PS_FOLLOW_MAX * 2];
static u32 ps_copy_count;

typedef struct ps_queued {
    mp_u32 uid;
    mp_particle particle;
} ps_queued;
static ps_queued ps_queue[PS_QUEUE];
static u32 ps_count;
/* the arguments are passed by guest address */
static cXyz ps_pos, ps_scale;
static csXyz ps_angle;
static u8 ps_prm[4], ps_env[4];

WWHD_GAME_ORIGINAL(PS_SET, u32, ps_set_original,
                   (u32 pa, u32 group, u32 id, u32 pos, u32 angle, u32 scale, u32 alpha, u32 cb, u32 setup,
                    u32 prm, u32 env, u32 scale2d));

static u32 ps_ld32(u32 a) { return *(const volatile u32*)a; }

/* A record of a followed emitter that is no longer there (or whose memory now holds another). */
static void ps_send_stop(const ps_follow* f) {
    if (!f->started) return; /* the others never made it */
    mp_particle* p = &anim_local.particle[mp_particle_slot(&anim_local)];
    *p = f->particle;
    p->flags = MP_PARTICLE_FOLLOW | MP_PARTICLE_STOP;
}

static void ps_follow_remove(u32 i) {
    ps_follows[i] = ps_follows[--ps_follow_count];
}

WWHD_REPLACE(PS_SET, u32, ps_set,
             (u32 pa, u32 group, u32 id, u32 pos, u32 angle, u32 scale, u32 alpha, u32 cb, u32 setup,
              u32 prm, u32 env, u32 scale2d)) {
    u32 emitter = ps_set_original(pa, group, id, pos, angle, scale, alpha, cb, setup, prm, env, scale2d);
    if (ps_replaying || !ps_capturing || !as_enabled || !as_session || as_local_uid == MP_NO_ID) return emitter;
    if (!emitter) return emitter;
    mp_particle p;
    p.group = (mp_u8)group;
    p.id = (mp_u16)id;
    p.flags = MP_PARTICLE_MATRIX;
    p.alpha = (mp_u8)alpha;
    p.key = 0;
    for (int c = 0; c != 3; ++c) {
        p.angle[c] = 0;
        p.scale[c] = 1.0f;
        p.pos[c] = 0.0f;
    }
    for (int c = 0; c != 4; ++c) p.prm[c] = p.env[c] = 0xFF;
    if (scale) {
        const cXyz* s = (const cXyz*)scale;
        p.scale[0] = s->x;
        p.scale[1] = s->y;
        p.scale[2] = s->z;
        p.flags |= MP_PARTICLE_SCALE;
    }
    if (prm && env) {
        for (int c = 0; c != 4; ++c) {
            p.prm[c] = *(const volatile u8*)(prm + (u32)c);
            p.env[c] = *(const volatile u8*)(env + (u32)c);
        }
        p.flags |= MP_PARTICLE_COLOR;
    }
    if (cb) {
        /* the same memory again: the emitter it held is gone */
        for (u32 i = 0; i != ps_follow_count; ++i) {
            if (ps_follows[i].emitter != emitter) continue;
            ps_send_stop(&ps_follows[i]);
            ps_follow_remove(i);
            break;
        }
        if (ps_follow_count == PS_FOLLOW_MAX) return emitter;
        ps_follow* f = &ps_follows[ps_follow_count++];
        f->emitter = emitter;
        f->callback = cb;
        f->born = wwhd_logic_step();
        f->started = 0;
        f->idle = 0;
        if (!++ps_next_key) ps_next_key = 1; /* 0 is never a key */
        p.key = ps_next_key;
        p.flags |= MP_PARTICLE_FOLLOW;
        f->particle = p;
        return emitter;
    }
    if (ps_pending_count == PS_PENDING) return emitter;
    ps_pending[ps_pending_count].emitter = emitter;
    ps_pending[ps_pending_count].particle = p;
    ++ps_pending_count;
    return emitter;
}

static void ps_read_place(u32 emitter, float* m) {
    const volatile f32* r = (const volatile f32*)(emitter + PS_EMITTER_ROTATION);
    const volatile f32* t = (const volatile f32*)(emitter + PS_EMITTER_TRANSLATION);
    for (int row = 0; row != 3; ++row)
        for (int col = 0; col != 3; ++col) m[row * 3 + col] = r[row * 4 + col];
    for (int c = 0; c != 3; ++c) m[9 + c] = t[c];
}

static int ps_follow_ended(const ps_follow* f) {
    u32 flags = ps_ld32(f->emitter + PS_EMITTER_FLAGS);
    return ps_ld32(f->emitter + PS_EMITTER_CALLBACK) != f->callback || (flags & (PS_STOP_EMIT | PS_DELETE_ENABLED)) ||
           *(const volatile s32*)(f->emitter + PS_EMITTER_MAX_FRAME) < 0;
}

/* Once per step: the followed emitters' first record, places and stops. */
static void ps_follow_step(void) {
    unsigned long long step = wwhd_logic_step();
    if (step == ps_follow_step_done) return;
    ps_follow_step_done = step;
    for (u32 i = 0; i < ps_follow_count;) {
        ps_follow* f = &ps_follows[i];
        if (ps_follow_ended(f) || step - f->born > PS_FOLLOW_MAX_STEPS) {
            ps_send_stop(f);
            ps_follow_remove(i);
            continue;
        }
        ++i;
        if (step == f->born) continue; /* its callback places it during this step's particle work */
        float m[12];
        ps_read_place(f->emitter, m);
        int moved = !f->started;
        for (int c = 0; c != 12 && !moved; ++c) {
            float d = m[c] - f->last[c];
            moved = d > PS_MOVE_EPSILON * (c < 9 ? 0.002f : 1.0f) || d < -PS_MOVE_EPSILON * (c < 9 ? 0.002f : 1.0f);
        }
        if (!moved && ++f->idle < PS_KEEPALIVE) continue;
        mp_particle* p = &anim_local.particle[mp_particle_slot(&anim_local)];
        *p = f->particle;
        if (f->started) p->flags &= ~(mp_u8)(MP_PARTICLE_SCALE | MP_PARTICLE_COLOR); /* only the place */
        for (int c = 0; c != 9; ++c) p->matrix[c] = m[c];
        for (int c = 0; c != 3; ++c) p->pos[c] = m[9 + c];
        for (int c = 0; c != 12; ++c) f->last[c] = m[c];
        f->idle = 0;
        if (!f->started && !ps_followed++) wwhd_log_int("multiplayer: first followed particle effect shared, id", (int)p->id);
        f->started = 1;
    }
}

/* After the local Link's (or ship's) execute: the noted one-shot emitters, where that code put them,
 * and the followed ones. */
static void ps_flush(void) {
    for (u32 i = 0; i != ps_pending_count; ++i) {
        ps_pending_emitter* e = &ps_pending[i];
        mp_particle* p = &e->particle;
        float m[12];
        ps_read_place(e->emitter, m);
        for (int c = 0; c != 9; ++c) p->matrix[c] = m[c];
        for (int c = 0; c != 3; ++c) p->pos[c] = m[9 + c];
        anim_local.particle[mp_particle_slot(&anim_local)] = *p;
        if (!ps_sent++) wwhd_log_int("multiplayer: first particle effect shared, id", (int)p->id);
    }
    ps_pending_count = 0;
    ps_follow_step();
}

/* Particles another player's Link made, made again at the next step. */
static void ps_queue_particles(mp_u32 uid, const mp_anim_events* e) {
    for (u32 i = 0; i != e->particle_count && ps_count != PS_QUEUE; ++i) {
        ps_queue[ps_count].uid = uid;
        ps_queue[ps_count].particle = e->particle[i];
        ++ps_count;
    }
}

static void ps_place(u32 em, const mp_particle* p) {
    volatile f32* r = (volatile f32*)(em + PS_EMITTER_ROTATION);
    for (int row = 0; row != 3; ++row) {
        for (int col = 0; col != 3; ++col) r[row * 4 + col] = p->matrix[row * 3 + col];
        r[row * 4 + 3] = 0.0f;
    }
    volatile f32* t = (volatile f32*)(em + PS_EMITTER_TRANSLATION);
    for (int c = 0; c != 3; ++c) t[c] = p->pos[c];
}

/* As daPy_mtxFollowEcallBack_c::end (023D4538): no more particles, deleted once they are gone. */
static void ps_end(u32 em) {
    *(volatile s32*)(em + PS_EMITTER_MAX_FRAME) = -1;
    u32 flags = ps_ld32(em + PS_EMITTER_FLAGS);
    *(volatile u32*)(em + PS_EMITTER_FLAGS) = (flags | PS_STOP_EMIT) & ~(u32)PS_IMMORTAL;
}

static u32 ps_make_cb(u32 pa, const mp_particle* p, u32 cb);
static u32 ps_make(u32 pa, const mp_particle* p) { return ps_make_cb(pa, p, 0); }
static u32 ps_make_cb(u32 pa, const mp_particle* p, u32 cb) {
    ps_pos.x = p->pos[0];
    ps_pos.y = p->pos[1];
    ps_pos.z = p->pos[2];
    ps_angle.x = p->angle[0];
    ps_angle.y = p->angle[1];
    ps_angle.z = p->angle[2];
    ps_scale.x = p->scale[0];
    ps_scale.y = p->scale[1];
    ps_scale.z = p->scale[2];
    for (int c = 0; c != 4; ++c) {
        ps_prm[c] = p->prm[c];
        ps_env[c] = p->env[c];
    }
    int colour = (p->flags & MP_PARTICLE_COLOR) != 0;
    return ps_set_original(pa, p->group, p->id, (u32)&ps_pos, (p->flags & MP_PARTICLE_ANGLE) ? (u32)&ps_angle : 0,
                           (p->flags & MP_PARTICLE_SCALE) ? (u32)&ps_scale : 0, p->alpha, cb, 0xFFFFFFFFu,
                           colour ? (u32)ps_prm : 0, colour ? (u32)ps_env : 0, 0);
}

/* 0.3.13: a copy's emitter is still ours: Immortal (we set it) and without a callback (we gave none).
 * The game can free an emitter whatever its flags (a scene's particles cleared) and give its memory
 * to another effect (the wind, the sky's): the copy's place, written there every update, and its end
 * twisted that effect (big broken textures in the sky). A copy that is no longer ours is forgotten,
 * nothing written. */
static int ps_copy_alive(const ps_copy* c) {
    u32 callback = c->cut >= 0 ? (u32)ps_cuts[c->cut].obj : 0;
    return c->emitter && (ps_ld32(c->emitter + PS_EMITTER_FLAGS) & PS_IMMORTAL) &&
           ps_ld32(c->emitter + PS_EMITTER_CALLBACK) == callback;
}

/* Ends a copy the way its kind ends: a spin's by its callback's end flag. */
static void ps_copy_end(ps_copy* c, int alive) {
    if (c->cut >= 0) {
        ps_cut* k = &ps_cuts[c->cut];
        if (alive) {
            *(volatile u8*)(k->obj + 5) = 1; /* dPa_cutTurnEcallBack_c::end: its next executeAfter stops it */
            *(volatile u32*)(c->emitter + PS_EMITTER_FLAGS) = ps_ld32(c->emitter + PS_EMITTER_FLAGS) & ~(u32)PS_IMMORTAL;
        }
        k->ending = PS_CUT_END_STEPS;
        return;
    }
    if (alive) ps_end(c->emitter);
}

/* The sword's tip of a player's puppet: its right hand, PS_CUT_REACH further out from its body. */
static int ps_cut_tip(mp_u32 uid, f32* out) {
    puppet_slot* s = puppet_slot_for(uid, 0);
    if (!s || !s->actor) return 0;
    const daPy_lk_c* pl = (const daPy_lk_c*)s->actor;
    const fopAc_ac_c* a = (const fopAc_ac_c*)s->actor;
    const f32* rh = pl->mpCLModel ? puppet_anm_mtx(pl->mpCLModel, 12) : 0;
    if (!rh) return 0;
    f32 dx = rh[3] - a->current.pos.x, dz = rh[11] - a->current.pos.z;
    f32 len = as_sqrt(dx * dx + dz * dz);
    if (len < 1.0f) {
        dx = as_sin(a->shape_angle.y);
        dz = as_sin((s16)(a->shape_angle.y + 0x4000));
        len = 1.0f;
    }
    out[0] = rh[3] + dx / len * PS_CUT_REACH;
    out[1] = rh[7];
    out[2] = rh[11] + dz / len * PS_CUT_REACH;
    return 1;
}

/* Every step: the trail of each spin copy, from the last tip to the new one, newest first. */
static void ps_cut_step(void) {
    for (u32 i = 0; i != ps_copy_count; ++i) {
        ps_copy* c = &ps_copies[i];
        if (c->cut < 0) continue;
        ps_cut* k = &ps_cuts[c->cut];
        if (k->ending) continue;
        f32 tip[3];
        if (!ps_cut_tip(c->uid, tip)) continue;
        if (!k->has_last) {
            for (int d = 0; d != 3; ++d) k->last[d] = tip[d];
            k->has_last = 1;
        }
        for (u32 n = 0; n != PS_CUT_POINTS; ++n) {
            f32 t = (f32)n / (f32)(PS_CUT_POINTS - 1);
            for (int d = 0; d != 3; ++d) k->pos[n * 3 + (u32)d] = tip[d] + (k->last[d] - tip[d]) * t;
        }
        for (int d = 0; d != 3; ++d) k->last[d] = tip[d];
        *(volatile u32*)(k->obj + 8) = (u32)k->pos;
        *(volatile s16*)(k->obj + 6) = PS_CUT_POINTS;
    }
    for (u32 j = 0; j != PS_CUTS; ++j)
        if (ps_cuts[j].used && ps_cuts[j].ending && !--ps_cuts[j].ending) ps_cuts[j].used = 0;
}

/* A free spin callback object, set up as dPa_cutTurnEcallBack_c's constructor would; -1 when none
 * (or no local Link to take the vtable from). */
static int ps_cut_take(void) {
    u32 local = puppet_local_player();
    u32 vtable = local ? ps_ld32(local + PS_CUT_VTABLE) : 0;
    if (!vtable) return -1;
    for (int j = 0; j != PS_CUTS; ++j) {
        ps_cut* k = &ps_cuts[j];
        if (k->used) continue;
        k->used = 1;
        k->ending = 0;
        k->has_last = 0;
        *(volatile u32*)(k->obj + 0) = vtable;
        *(volatile u8*)(k->obj + 4) = 0xFF;
        *(volatile u8*)(k->obj + 5) = 0;
        *(volatile s16*)(k->obj + 6) = 0;
        *(volatile u32*)(k->obj + 8) = (u32)k->pos;
        *(volatile u32*)(k->obj + 0xC) = 0;
        return j;
    }
    return -1;
}

static ps_copy* ps_copy_find(mp_u32 uid, mp_u8 key) {
    for (u32 i = 0; i != ps_copy_count; ++i)
        if (ps_copies[i].uid == uid && ps_copies[i].key == key) return &ps_copies[i];
    return 0;
}

static void ps_copy_drop(ps_copy* c) {
    *c = ps_copies[--ps_copy_count];
}

static void ps_follow_replay(u32 pa, mp_u32 uid, const mp_particle* p) {
    ps_copy* c = ps_copy_find(uid, p->key);
    if (c && !ps_copy_alive(c)) { /* freed by the game: not ours any more */
        ps_copy_end(c, 0);
        ps_copy_drop(c);
        c = 0;
        if (p->flags & MP_PARTICLE_STOP) return;
    }
    if (p->flags & MP_PARTICLE_STOP) {
        if (c) {
            ps_copy_end(c, 1);
            ps_copy_drop(c);
        }
        return;
    }
    if (c) {
        if (c->cut < 0) ps_place(c->emitter, p); /* a spin's callback places it */
        c->heard = wwhd_logic_step();
        return;
    }
    if (ps_copy_count == PS_FOLLOW_MAX * 2) return;
    int cut = ps_is_cut_turn(p) ? ps_cut_take() : -1;
    u32 em = ps_make_cb(pa, p, cut >= 0 ? (u32)ps_cuts[cut].obj : 0);
    if (!em) {
        if (cut >= 0) ps_cuts[cut].used = 0;
        return;
    }
    ps_place(em, p);
    *(volatile u32*)(em + PS_EMITTER_FLAGS) = ps_ld32(em + PS_EMITTER_FLAGS) | PS_IMMORTAL;
    c = &ps_copies[ps_copy_count++];
    c->uid = uid;
    c->key = p->key;
    c->cut = cut;
    if (cut >= 0) {
        static u32 cut_logged;
        if (cut_logged++ < 3) wwhd_log_int("multiplayer: spin attack trail of another player, id", (int)p->id);
    }
    c->emitter = em;
    c->heard = wwhd_logic_step();
    c->made = c->heard;
    if (!ps_replayed++) wwhd_log_int("multiplayer: first particle effect of another player, id", (int)p->id);
}

static void ps_step(void) {
    const u8* play = wwhd_play_get();
    u32 pa = play ? *(const volatile u32*)(play + PS_PLAY_PARTICLE) : 0;
    u32 count = ps_count;
    ps_count = 0;
    if (!pa) return;
    ps_replaying = 1;
    for (u32 i = 0; i != count; ++i) {
        const mp_particle* p = &ps_queue[i].particle;
        if (p->flags & MP_PARTICLE_FOLLOW) {
            ps_follow_replay(pa, ps_queue[i].uid, p);
            continue;
        }
        u32 em = ps_make(pa, p);
        if (!em) continue;
        if (p->flags & MP_PARTICLE_MATRIX) ps_place(em, p);
        volatile s32* max = (volatile s32*)(em + PS_EMITTER_MAX_FRAME);
        if (*max == 0) *max = PS_BURST_FRAMES; /* an endless one: its maker stops it, a copy's would not */
        if (!ps_replayed++) wwhd_log_int("multiplayer: first particle effect of another player, id", (int)p->id);
    }
    ps_replaying = 0;
    ps_cut_step();
    /* copies whose maker went silent (left, or its stop was lost) */
    unsigned long long step = wwhd_logic_step();
    for (u32 i = 0; i < ps_copy_count;) {
        if (!ps_copy_alive(&ps_copies[i])) {
            ps_copy_end(&ps_copies[i], 0);
            ps_copy_drop(&ps_copies[i]);
            continue;
        }
        if (step - ps_copies[i].heard > PS_FOLLOW_TIMEOUT || step - ps_copies[i].made > PS_COPY_MAX_STEPS) {
            ps_copy_end(&ps_copies[i], 1);
            ps_copy_drop(&ps_copies[i]);
            continue;
        }
        ++i;
    }
}

/* On leaving an area. The copies still Immortal and without a callback are ours: ended (after a
 * stage change their memory may hold the new stage's emitters, which are not Immortal copies). */
static void ps_clear(void) {
    ps_count = 0;
    ps_pending_count = 0;
    ps_follow_count = 0;
    for (u32 i = 0; i != ps_copy_count; ++i) {
        u32 em = ps_copies[i].emitter;
        if ((ps_ld32(em + PS_EMITTER_FLAGS) & PS_IMMORTAL) && !ps_ld32(em + PS_EMITTER_CALLBACK)) ps_end(em);
        else if (ps_copy_alive(&ps_copies[i])) ps_copy_end(&ps_copies[i], 1);
    }
    ps_copy_count = 0;
    for (u32 j = 0; j != PS_CUTS; ++j)
        if (ps_cuts[j].used && !ps_cuts[j].ending) ps_cuts[j].ending = PS_CUT_END_STEPS;
}
