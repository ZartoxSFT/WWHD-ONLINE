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

/* the other players' followed emitters, here */
typedef struct ps_copy {
    mp_u32 uid;
    mp_u8 key;
    u32 emitter;
    unsigned long long heard;
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
        if (ps_follow_ended(f)) {
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

static u32 ps_make(u32 pa, const mp_particle* p) {
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
                           (p->flags & MP_PARTICLE_SCALE) ? (u32)&ps_scale : 0, p->alpha, 0, 0xFFFFFFFFu,
                           colour ? (u32)ps_prm : 0, colour ? (u32)ps_env : 0, 0);
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
    if (p->flags & MP_PARTICLE_STOP) {
        if (c) {
            ps_end(c->emitter);
            ps_copy_drop(c);
        }
        return;
    }
    if (c) {
        ps_place(c->emitter, p);
        c->heard = wwhd_logic_step();
        return;
    }
    if (ps_copy_count == PS_FOLLOW_MAX * 2) return;
    u32 em = ps_make(pa, p);
    if (!em) return;
    ps_place(em, p);
    *(volatile u32*)(em + PS_EMITTER_FLAGS) = ps_ld32(em + PS_EMITTER_FLAGS) | PS_IMMORTAL;
    c = &ps_copies[ps_copy_count++];
    c->uid = uid;
    c->key = p->key;
    c->emitter = em;
    c->heard = wwhd_logic_step();
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
    /* copies whose maker went silent (left, or its stop was lost) */
    unsigned long long step = wwhd_logic_step();
    for (u32 i = 0; i < ps_copy_count;) {
        if (step - ps_copies[i].heard > PS_FOLLOW_TIMEOUT) {
            ps_end(ps_copies[i].emitter);
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
    }
    ps_copy_count = 0;
}
