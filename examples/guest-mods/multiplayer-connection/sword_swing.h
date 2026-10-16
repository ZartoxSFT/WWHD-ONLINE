/* Sword swings of the remote players on the local scenery (grass, flowers, bushes, small trees).
 *
 * That scenery is not made of actors: dGrass/dFlower/dWood/dTree data ask the mass collision
 * manager (dCcMassS_Mng::Chk 025170D8, play + 0x4EF8) whether an attack collider registered this
 * step touches them, then read its attack type (dCcD_GObjInf::mAtType), attack vector
 * (mGObjAt.mVec) and owner actor (mStts->mp_actor). Link registers his sword there in
 * daPy_lk_c::setCollision (024076CC): mAtCps[0..2], or mAtCyl in a spin attack, priority 1,
 * while daPyFlg0_CUT_AT_FLG is set (decomp d_a_player_main_04.cpp, d_cc_mass_s.cpp).
 *
 * Owner: after the local Link's execute, the colliders it registered are copied into a swing
 * (mp_protocol.h). Others: each received swing is replayed for one logic step by capsules of our
 * own (game constructor 024445B0 and dCcD_Cps::Set 025164C0 with Link's l_at_cps_src), placed
 * where the owner's were and registered with the mass manager like Link's. They are never given
 * to the main collision manager (dCcS): enemies, pots and the local Link are not hit.
 *
 * Their owner actor is the player's Link puppet when there is one, else the local Link: the
 * scenery only reads its name and position, when the attack vector is zero. Layouts and offsets
 * come from the decomp headers (d_cc_d.h, c_m3d_g_cps.h, c_m3d_g_cyl.h, d_a_player_main.h). */

#define SWING_LINK_NO_RESET_FLG0 0x3B8 /* daPy_lk_c::mNoResetFlg0 */
#define SWING_CUT_AT_FLG 0x40u         /* daPyFlg0_CUT_AT_FLG */
#define SWING_LINK_STTS 0x7620         /* daPy_lk_c::mStts */
#define SWING_LINK_AT_CYL 0x78BC       /* daPy_lk_c::mAtCyl (dCcD_Cyl) */
#define SWING_LINK_AT_CPS 0x7B1C       /* daPy_lk_c::mAtCps[3] (dCcD_Cps) */
#define SWING_CPS_BYTES 0x138          /* sizeof(dCcD_Cps) */
#define SWING_AT_SPRM 0x00             /* cCcD_ObjAt::mSPrm: bit 0, the collider is set */
#define SWING_TG_SPRM 0x18             /* cCcD_ObjTg::mSPrm */
#define SWING_CO_SPRM 0x2C             /* cCcD_ObjCo::mSPrm */
#define SWING_SPRM_SET 1u
#define SWING_AT_TYPE 0x10             /* dCcD_GObjInf::mAtType */
#define SWING_STTS 0x44                /* dCcD_GObjInf::mStts */
#define SWING_AT_VEC 0x7C              /* dCcD_GObjInf::mGObjAt (0x50) + dCcD_GObjAt::mVec (0x2C) */
#define SWING_SHAPE 0x118              /* dCcD_Cps::mCps / dCcD_Cyl::mCyl */
#define SWING_CPS_START 0x00           /* cM3dGCps::mStart, mEnd, mRadius */
#define SWING_CPS_END 0x0C
#define SWING_CPS_RADIUS 0x1C
#define SWING_CYL_RADIUS 0x0C          /* cM3dGCyl::mCenter (bottom), mRadius, mHeight */
#define SWING_CYL_HEIGHT 0x10
#define SWING_AT_CPS_SRC 0x101CEDA8    /* l_at_cps_src (canonical USA data) */
#define SWING_LINK_FAN_WIND 0x7EC4     /* daPy_lk_c::mFanWindCps (d_a_player_main.h), the Deku Leaf's gust */
#define SWING_PLAY_MASS 0x4EF8         /* play + 0x4EF8: dCcMassS_Mng */
#define SWING_PRIORITY 1               /* the priority of Link's own sword */
#define SWING_PLAYERS 8
#define SWING_QUEUE 8                  /* swings waiting per player; the oldest is dropped */

static u32 swing_ld32(u32 address) { return *(const volatile u32*)address; }
static f32 swing_ldf(u32 address) { return *(const volatile f32*)address; }
static void swing_st32(u32 address, u32 value) { *(volatile u32*)address = value; }
static void swing_stf(u32 address, f32 value) { *(volatile f32*)address = value; }

/* ------------------------------------------------------------------ owner */

static int swing_sent_logged;

/* Adds the attack colliders the local Link registered with the mass manager this step. */
/* The Deku Leaf's gust (its own attack type and vector), when Link set it this step. */
static void swing_capture_fan(u32 l, mp_anim_events* out) {
    u32 cps = l + SWING_LINK_FAN_WIND;
    if (!(swing_ld32(cps + SWING_AT_SPRM) & SWING_SPRM_SET)) return;
    mp_swing s;
    s.count = 1;
    u32 shape = cps + SWING_SHAPE;
    for (u32 k = 0; k != 3; ++k) {
        s.capsule[0].start[k] = swing_ldf(shape + SWING_CPS_START + k * 4);
        s.capsule[0].end[k] = swing_ldf(shape + SWING_CPS_END + k * 4);
        s.vec[k] = swing_ldf(cps + SWING_AT_VEC + k * 4);
    }
    s.capsule[0].radius = swing_ldf(shape + SWING_CPS_RADIUS);
    s.at_type = swing_ld32(cps + SWING_AT_TYPE);
    out->swing[mp_swing_slot(out)] = s;
}

static void swing_capture(const void* link, mp_anim_events* out) {
    u32 l = (u32)link;
    if (l) swing_capture_fan(l, out);
    if (!l || !(swing_ld32(l + SWING_LINK_NO_RESET_FLG0) & SWING_CUT_AT_FLG)) return;
    mp_swing s;
    s.count = 0;
    s.at_type = 0;
    u32 typed = 0; /* the collider giving the attack type and vector: the first capsule, else the cylinder */
    for (u32 i = 0; i != 3; ++i) {
        u32 cps = l + SWING_LINK_AT_CPS + i * SWING_CPS_BYTES;
        if (!(swing_ld32(cps + SWING_AT_SPRM) & SWING_SPRM_SET)) continue;
        mp_swing_capsule* c = &s.capsule[s.count++];
        u32 shape = cps + SWING_SHAPE;
        for (u32 k = 0; k != 3; ++k) {
            c->start[k] = swing_ldf(shape + SWING_CPS_START + k * 4);
            c->end[k] = swing_ldf(shape + SWING_CPS_END + k * 4);
        }
        c->radius = swing_ldf(shape + SWING_CPS_RADIUS);
        if (!typed) typed = cps;
    }
    u32 cyl = l + SWING_LINK_AT_CYL;
    if (swing_ld32(cyl + SWING_AT_SPRM) & SWING_SPRM_SET) {
        mp_swing_capsule* c = &s.capsule[s.count++];
        u32 shape = cyl + SWING_SHAPE;
        for (u32 k = 0; k != 3; ++k) c->start[k] = c->end[k] = swing_ldf(shape + k * 4);
        c->end[1] += swing_ldf(shape + SWING_CYL_HEIGHT);
        c->radius = swing_ldf(shape + SWING_CYL_RADIUS);
        if (!typed) typed = cyl;
    }
    if (!s.count) return;
    s.at_type = swing_ld32(typed + SWING_AT_TYPE);
    for (u32 k = 0; k != 3; ++k) s.vec[k] = swing_ldf(typed + SWING_AT_VEC + k * 4);
    out->swing[mp_swing_slot(out)] = s;
    if (!swing_sent_logged) {
        swing_sent_logged = 1;
        wwhd_log_int("multiplayer: first sword swing sent, attack type", (int)s.at_type);
    }
}

/* ------------------------------------------------------------------ others */

typedef struct swing_player {
    int used;
    mp_u32 uid;
    int logged;
    u32 first, count;           /* ring of received swings */
    mp_swing queue[SWING_QUEUE];
} swing_player;

static swing_player swing_players[SWING_PLAYERS];
static u8 swing_ghosts[SWING_PLAYERS][MP_SWING_CAPSULES][SWING_CPS_BYTES] __attribute__((aligned(32)));
static int swing_ghosts_built[SWING_PLAYERS];
/* SetStartEnd takes guest addresses: the points live in mod memory */
static cXyz swing_start;
static cXyz swing_end;

static swing_player* swing_player_for(mp_u32 uid, int create) {
    swing_player* free_slot = 0;
    for (int i = 0; i != SWING_PLAYERS; ++i) {
        if (swing_players[i].used && swing_players[i].uid == uid) return &swing_players[i];
        if (!swing_players[i].used && !free_slot) free_slot = &swing_players[i];
    }
    if (!create || !free_slot) return 0;
    free_slot->used = 1;
    free_slot->uid = uid;
    free_slot->logged = 0;
    free_slot->first = free_slot->count = 0;
    return free_slot;
}

static void swing_queue(mp_u32 uid, const mp_anim_events* e) {
    if (!e->swing_count) return;
    swing_player* p = swing_player_for(uid, 1);
    if (!p) return;
    for (u32 i = 0; i != e->swing_count; ++i) {
        if (p->count == SWING_QUEUE) { /* drop the oldest */
            p->first = (p->first + 1) % SWING_QUEUE;
            --p->count;
        }
        p->queue[(p->first + p->count++) % SWING_QUEUE] = e->swing[i];
    }
}

static void swing_remove(mp_u32 uid) {
    swing_player* p = swing_player_for(uid, 0);
    if (p) p->used = 0;
}

static void swing_clear_all(void) {
    for (int i = 0; i != SWING_PLAYERS; ++i) swing_players[i].used = 0;
}

/* Our capsules, built once with the game's constructor and Link's sword source data; they only
 * attack (their Tg and Co are cleared, so nothing pushes or hits them). */
static void swing_build(int slot) {
    if (swing_ghosts_built[slot]) return;
    for (int c = 0; c != MP_SWING_CAPSULES; ++c) {
        u32 cps = (u32)swing_ghosts[slot][c];
        wwhd_dCcD_Cps_ct_l(cps);
        wwhd_dCcD_Cps_Set((void*)cps, (u32)&WWHD_GAME_DATA(SWING_AT_CPS_SRC, u8));
        swing_st32(cps + SWING_TG_SPRM, swing_ld32(cps + SWING_TG_SPRM) & ~SWING_SPRM_SET);
        swing_st32(cps + SWING_CO_SPRM, swing_ld32(cps + SWING_CO_SPRM) & ~SWING_SPRM_SET);
    }
    swing_ghosts_built[slot] = 1;
}

/* One received swing per player and logic step, registered like the owner's sword was. */
static void swing_step(const void* local_link) {
    u8* play = wwhd_play_get();
    if (!play || !local_link) return;
    for (int i = 0; i != SWING_PLAYERS; ++i) {
        swing_player* p = &swing_players[i];
        if (!p->used || !p->count) continue;
        mp_swing s = p->queue[p->first];
        p->first = (p->first + 1) % SWING_QUEUE;
        --p->count;
        swing_build(i);
        u32 owner = (u32)local_link;
        puppet_slot* puppet = puppet_slot_for(p->uid, 0);
        if (puppet && puppet->actor) owner = puppet->actor;
        for (u32 c = 0; c != s.count && c != MP_SWING_CAPSULES; ++c) {
            u32 cps = (u32)swing_ghosts[i][c];
            swing_start.x = s.capsule[c].start[0];
            swing_start.y = s.capsule[c].start[1];
            swing_start.z = s.capsule[c].start[2];
            swing_end.x = s.capsule[c].end[0];
            swing_end.y = s.capsule[c].end[1];
            swing_end.z = s.capsule[c].end[2];
            wwhd_SetStartEnd_cXyz(cps + SWING_SHAPE, (u32)&swing_start, (u32)&swing_end);
            swing_stf(cps + SWING_SHAPE + SWING_CPS_RADIUS, s.capsule[c].radius);
            swing_st32(cps + SWING_AT_TYPE, s.at_type);
            for (u32 k = 0; k != 3; ++k) swing_stf(cps + SWING_AT_VEC + k * 4, s.vec[k]);
            swing_st32(cps + SWING_STTS, owner + SWING_LINK_STTS);
            swing_st32(cps + SWING_AT_SPRM, swing_ld32(cps + SWING_AT_SPRM) | SWING_SPRM_SET);
            wwhd_mass_set(play + SWING_PLAY_MASS, (void*)cps, SWING_PRIORITY);
        }
        if (!p->logged) {
            p->logged = 1;
            wwhd_log_int("multiplayer: sword swings replayed on the scenery for uid", (int)p->uid);
        }
    }
}
