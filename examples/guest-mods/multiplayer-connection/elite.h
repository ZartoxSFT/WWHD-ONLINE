/* Dynamic scaling and elite enemies (the server's mutators "#dynamic_scaling=" and
 * "#elite_chance=", ActorFilter.java, set from the launcher's Server tab).
 *
 * Which enemies: the placed enemies (actor_sync.h, block tagged AS_TAG: the stage's own and the
 * multiplier's copies) whose name is in el_regular (the regular enemies of as_enemies; no boss,
 * pig or ship). Each game draws at random when the enemy is made (each time the room loads):
 * elite or not, its faculty (el_ability_odds) and its colour (0..19, elite_color.h ec_palette). The enemy's
 * owner sends its draw with its health (mp_protocol.h mp_health_word: the high byte of
 * ActorHealthSync's short, EL_TAG_*), and the other games take it (el_adopt, before the copy's
 * execute), so every player sees the same elites with the same colour.
 *
 * Dynamic scaling, with n the players in the local player's area (mod.c el_players_here: the
 * server's player directory, same stage and, on the Great Sea, the same room or 5000 units):
 *  - health: the enemy's own health (fopAc_ac_c::health, s8 at 0x3A1, read at its first execute
 *    once positive) times 1 + 0.5 (n - 1), followed as n changes (health and max_health 0x3A0
 *    rescaled), at most 127;
 *  - damage to the local Link: dCcS::CalcTgPlusDmg (02518670, d_cc_s.cpp: the target's
 *    cCcD_Stts::m_dmg (+0x16) takes the attacker's mAtp) is replaced; when the target status's
 *    actor (cCcD_Stts::mp_actor +0xC) is the local Link and the attacker's is an enemy (group 2),
 *    the damage taken is times 1 + 0.25 (n - 1).
 * Elites (elite chance percent of the regular enemies), on top of the scaling:
 *  - health x 3, damage x 1.5;
 *  - faster attacks: while an elite runs its execute, J3DFrameCtrl::update (027F2FC4) and
 *    J3DFrameCtrl::checkPass (027F2BF8, the frame window [frame, frame + rate) the actors test
 *    their attack frames with, J3DAnimation.cpp) run with mRate (+0) x 1.5, so every animation
 *    of the elite and its frame events go 1.5 times as fast;
 *  - one faculty: none, bombs, summons or electricity (el_faculty). Bombs: while the local Link is
 *    within 1400 units, every 4 to 8 seconds a lit bomb
 *    (daBomb_c state 1: fuse, l_attrState 0x1C; parameters state | 0x80000000, d_a_bomb_static.cpp
 *    daBomb_prm_make) falls near the local Link; made with fopAcM_create (025D5834) in the LOCAL
 *    context, so the other players see it fall on this player's puppet (el_make_bomb);
 *    Summons (rare, by the game that runs it, at most 3): a Keese or a Miniblin (el_summon).
 *    Electricity: sparks around it (el_sparks) and, the local Link within 300 units, a shock every
 *    5 to 9 s (el_link_step);
 *  - loot when it dies: 3 random spoils among those the player has ever picked up (blue rupees
 *    while none) and 15 % a purple rupee (el_loot);
 *  - a big explosion when it dies (deleted with health <= 0, or 3 s after its health fell to 0):
 *    seven bombs that explode at once (state 0, l_attrState 0xA1; the centre one state 8, mAtp 2);
 *  - 25 % bigger, in its models only: after each J3DModel::calc (027F4D5C) run during the elite's
 *    execute or draw, the world matrices it wrote (model +0x2C -> block, +0x10 matrices, +0x2C u16
 *    count, runtime interp.cpp) are scaled by 1.25 around the model's root joint. calc rebuilds them
 *    from scratch each time, so nothing adds up. (0.2.40-0.2.53 multiplied fopAc_ac_c::scale again
 *    whenever the actor had written it: the actors that grow their scale step by step - a Bokoblin
 *    out of a pot, the skulls - grew without end, and those whose model ignores scale never grew.)
 *    The actor's collision and logic are not changed;
 *  - a crown above it (assets/crown.png, drawn by this mod, projected with the game camera), shown
 *    up to 10 m (1000 units) from the local Link, fading from 7 m;
 *  - another colour: elite_color.h finds the primary colour of the model's textures and turns it
 *    to the elite's palette colour. The HD renderer draws a model at mDoExt_drawModel (025E2BF4,
 *    m_Do_ext_heap_model.cpp: every mDoExt_model* helper ends there and it draws at once), so while
 *    an elite runs fopAc_Draw, each of its models drawn there has the texture headers of its
 *    J3DModelData (+0xAC of the J3DModel; table at +0x30, 0x24-byte headers, HD texture object at
 *    +0x20) pointed to recoloured copies, then put back when that draw returns, with the table's
 *    dirty mask as tunic.h does for the remote players' tunics. The copies live in one arena where
 *    each image keeps the low 16 address bits of the original (tiling swizzle, tunic.h).
 *    Many enemies take their colour from their materials, not their textures (the ChuChus: TEV
 *    registers animated by brk, d_a_cc.cpp). An HD material (0x39C bytes, J3DModelData +0x10, count
 *    +0xC, d_kankyo_6.h) keeps the colours the shader reads as float slots behind its flags word
 *    (+0xA0): 027F9F0C(flags, slot) gives a slot, bit 1 << slot marks it to upload; TEV colours 0-2
 *    are slots 4-6 and konst colours 0-3 slots 7-10 (d_a_bgn_draw.cpp, d_a_swhit0.cpp,
 *    d_a_ship.cpp, d_a_obj_ajav.cpp). The animations write into the model's own copy after that
 *    copy is made, so the colours are painted there, once the animations ran (el_paint_instance,
 *    hook of 027F53CC during the elite's draw: 0.2.48-0.2.49 painted the shared slots, which the
 *    ChuChus' body animation overwrote: only their eyes changed). */
#pragma once
#include "wwhd_guest.h"
#include "elite_color.h"
#include "tunic.h"

#define EL_MAX 128
#define EL_HEALTH_MAX 127
#define EL_HEALTH_STEP 0.5f     /* per other player in the area */
#define EL_DAMAGE_STEP 0.25f
#define EL_ELITE_HEALTH 3.0f
#define EL_ELITE_DAMAGE 1.5f
#define EL_ELITE_SPEED 1.5f
#define EL_BOMB_RANGE 1400.0f
#define EL_BOMB_MIN 4.0
#define EL_BOMB_SPREAD 4.0
#define EL_DEATH_WAIT 3.0
#define EL_BLAST_RADIUS 140.0f
#define EL_PROC_BOMB 0x126       /* fpcNm_BOMB_e */
#define EL_BOMB_LIT 1u           /* daBomb_c state 1 */
#define EL_BOMB_BLAST 0u         /* state 0: explodes at once */
#define EL_BOMB_BIG_BLAST 8u     /* state 8: explodes at once, mAtp 2 */
#define EL_BOMB_VERSION 0x80000000u
#define EL_GROUP_ENEMY 2
#define EL_STTS_ACTOR 0x0C       /* cCcD_Stts::mp_actor */
#define EL_STTS_DMG 0x16         /* cCcD_Stts::m_dmg */
#define EL_MODEL_DATA 0xAC       /* J3DModel -> J3DModelData (HD) */
#define EL_CROWN_HEIGHT 70.0f
/* An elite's faculty (one each, drawn with it; percent of the elites in EL_ABILITY_ODDS) */
enum { EL_ABILITY_NONE = 0, EL_ABILITY_BOMBS = 1, EL_ABILITY_SUMMON = 2, EL_ABILITY_ELECTRIC = 3 };
static const u8 el_ability_odds[4] = {20, 30, 25, 25};
/* The draw the owner sends with the health (mp_health_word, never 0x00 or 0xFF): 0x01 not an
 * elite; 0x80 | faculty << 5 | colour (0..19) an elite. */
#define EL_TAG_REGULAR 0x01u
#define EL_TAG_ELITE 0x80u
#define EL_SUMMON_RANGE 1200.0f
#define EL_SUMMON_MIN 12.0
#define EL_SUMMON_SPREAD 10.0
#define EL_SUMMON_MAX 3          /* creatures per summoner */
#define EL_SHOCK_RANGE 300.0f
#define EL_SHOCK_MIN 5.0
#define EL_SHOCK_SPREAD 4.0
#define EL_SHOCK_DAMAGE 2.0f     /* quarter hearts, before the damage factor */
#define EL_SPARK_PERIOD 0.35     /* seconds between two bursts of sparks */
#define EL_SPARK_ID 0x3ED        /* ID_AK_JN_CCTHUNDER00: the Yellow ChuChu's sparks (d_a_cc_exec.cpp) */
#define EL_SPARK_FRAMES 10       /* the burst's emitter lives that long (JPA mMaxFrame) */
#define EL_LOOT 3
#define EL_PURPLE_RUPEE 0x05     /* dItemNo_PURPLE_RUPEE_e */
#define EL_PURPLE_PERCENT 15u
/* the monsters' spoils (d_item_data.h): Joy Pendant, Skull Necklace, Boko Baba Seed, Golden
 * Feather, Knight's Crest, Red and Green Chu Jelly. Not the Blue Chu Jelly (0x4B): daItem keeps its
 * bit number whole and sets it as a switch when picked up (d_a_item.cpp: itemOnSwitch for it,
 * the bit & 0x7F for the others), so -1 became switch 255 and the game halted (d_save.cpp
 * "0 <= i_no && i_no < 128", 0.2.50). */
#define EL_SPOILS 7
static const u8 el_spoils[EL_SPOILS] = {0x1F, 0x45, 0x46, 0x47, 0x48, 0x49, 0x4A};
/* Their bag index (dBeastIdx_*, d_item_4.h): the bit dSv_player_get_bag_item_c::onBeast sets the
 * first time the spoil is picked up (d_item.cpp, d_item_2.cpp item_func_*). Only those the player
 * has ever had drop; with none, a blue rupee. */
static const u8 el_spoil_beast[EL_SPOILS] = {7, 0, 1, 2, 3, 4, 5};
#define EL_BLUE_RUPEE 0x02       /* dItemNo_BLUE_RUPEE_e */
#define EL_SAVE_POINTER 0x101F84DC
#define EL_SAVE_GET_BAG (0x20 + 0x90) /* dSv_player_c::mGetBagItem (d_save.h) */
/* dSv_player_get_bag_item_c::isBeast (025B7690, d_save_2.cpp: this, index) */
WWHD_GAME_FUNC(0x025B7690, s32, el_is_beast, (u32 self, u32 index));
#define EL_CROWN_FADE 700.0f     /* units (about 7 m) */
#define EL_CROWN_HIDE 1000.0f    /* about 10 m, as asked */
#define EL_ELITE_SIZE 1.25f

/* the regular enemies of as_enemies (actor_sync.h): no boss, pig or ship */
static const char* const el_regular[] = {
    "Bk", "Bb", "mo2", "Tn", "gmos", "c_red", "c_green", "c_blue", "c_black", "c_kiiro", "Puti",
    "wiz_r", "bbaba", "amos", "amos2", "bable", "bable_r", "pow", "Sss", "keeth", "Fkeeth",
    "Fmaster", "Fmastr1", "Fmastr2", "magtail", "Oq", "Oqw", "p_hat", "sea_hat", "nezumi",
    "Rdead1", "Rdead2", "Stal",
};

typedef struct el_actor {
    u32 actor, pcid;
    u32 hash;
    u8 used, elite, based, exploded;
    u8 ability, seed;     /* EL_ABILITY_*; the elite's colour, 0..EC_PALETTE - 1 */
    u8 owned;             /* this game runs it (not a copy following another game) */
    u8 summoned;          /* a creature an elite summoned: never an elite itself */
    u8 summons;           /* creatures it summoned */
    double ability_clock; /* seconds to its next bomb, summon or shock */
    double spark_clock;
    u8 crown_visible;
    s8 base_health;
    f32 applied;          /* the health factor in its health now */
    double bomb_clock;
    double dead_clock;    /* seconds since its health fell to 0 or less */
    f32 last_pos[3];
    s8 last_room;
    f32 crown_x, crown_y, crown_scale, crown_alpha;
} el_actor;

static el_actor el_actors[EL_MAX];
static u32 el_count;
static int el_fast;               /* an elite runs its execute */
static int el_growing;            /* an elite runs its execute or draw: its models are bigger */
static el_actor* el_drawing;      /* an elite runs fopAc_Draw */
static u32 el_random_state = 0x2545F491u;
static int el_players_cached = 1;
static unsigned long long el_players_step;

static int el_players_here(void); /* mod.c: the players of the local player's area, 1 alone */

static u32 el_random(void) {
    el_random_state = el_random_state * 1664525u + 1013904223u;
    return el_random_state >> 8;
}
static f32 el_random01(void) { return (f32)(el_random() & 0xFFFF) / 65535.0f; }

static int el_name_regular(const char* name) {
    for (u32 e = 0; e != sizeof(el_regular) / sizeof(el_regular[0]); ++e)
        if (as_name_is(name, el_regular[e])) return 1;
    return 0;
}

static int el_scaling(void) { return as_session && as_session->dynamic_scaling; }

static int el_players(void) {
    unsigned long long step = wwhd_logic_step();
    if (step != el_players_step) { /* counted once a step */
        el_players_step = step;
        el_players_cached = el_players_here();
        if (el_players_cached < 1) el_players_cached = 1;
    }
    return el_players_cached;
}

static f32 el_health_factor(const el_actor* e) {
    f32 f = el_scaling() ? 1.0f + EL_HEALTH_STEP * (f32)(el_players() - 1) : 1.0f;
    return e->elite ? f * EL_ELITE_HEALTH : f;
}

static f32 el_damage_factor(const el_actor* e) {
    f32 f = el_scaling() ? 1.0f + EL_DAMAGE_STEP * (f32)(el_players() - 1) : 1.0f;
    return e && e->elite ? f * EL_ELITE_DAMAGE : f;
}

static el_actor* el_find(const void* actor) {
    if (!el_count || !actor) return 0;
    for (u32 i = 0; i != EL_MAX; ++i) {
        el_actor* e = &el_actors[i];
        if (!e->used || e->actor != (u32)actor) continue;
        if (*(const volatile u32*)((const u8*)actor + AS_PC_ID) == e->pcid) return e;
        e->used = 0; /* another process at that address now */
        --el_count;
        return 0;
    }
    return 0;
}

/* fopAc_Create's first call for a tagged placed actor (as_create): a regular enemy is noted, and
 * chosen as an elite from its block. */
static void el_note_create(void* actor, u32 prm) {
    if (!as_session || (!as_session->dynamic_scaling && !as_session->elite_chance)) return;
    const u8* block = (const u8*)prm;
    if (block[AS_PRM_TAG] != AS_TAG) return;
    s16 proc = *(const volatile s16*)((const u8*)actor + AS_PROC_NAME);
    u32 text = wwhd_dStage_getName2((u32)(s32)proc, (u32)(s32)(s8)block[AS_PRM_ARGUMENT]);
    if (!text) return;
    char name[MP_ACTOR_NAME_MAX + 1];
    for (u32 i = 0; i != MP_ACTOR_NAME_MAX + 1; ++i) name[i] = 0;
    for (u32 i = 0; i != MP_ACTOR_NAME_MAX && ((const char*)text)[i]; ++i) name[i] = ((const char*)text)[i];
    if (!el_name_regular(name)) return;
    if (el_find(actor)) return;
    el_actor* e = 0;
    for (u32 i = 0; i != EL_MAX && !e; ++i)
        if (!el_actors[i].used) e = &el_actors[i];
    if (!e) return;
    el_actor blank = {0};
    *e = blank;
    u32 h = 2166136261u;
    for (u32 i = 0; i != AS_PRM_BYTES; ++i) h = (h ^ block[i]) * 16777619u;
    e->used = 1;
    e->actor = (u32)actor;
    e->pcid = *(const volatile u32*)((const u8*)actor + AS_PC_ID);
    e->hash = h;
    /* this game's own draw, a new one at each load; the owner's replaces it (el_adopt) */
    el_random_state ^= h ^ (u32)wwhd_logic_step() * 2654435761u;
    e->summoned = block[AS_PRM_COPY] == AS_SUMMON;
    e->elite = !e->summoned && el_random() % 100u < as_session->elite_chance;
    u32 roll = el_random() % 100u, sum = 0;
    e->ability = EL_ABILITY_NONE;
    for (u8 k = 0; k != 4; ++k) {
        sum += el_ability_odds[k];
        if (roll < sum) {
            e->ability = k;
            break;
        }
    }
    e->seed = (u8)(el_random() % EC_PALETTE);
    e->applied = 1.0f;
    e->owned = 1;
    e->ability_clock = 3.0 + (double)el_random01() * 4.0;
    ++el_count;
    if (e->elite) {
        static u32 logged;
        if (logged++ < 8) {
            wwhd_log("multiplayer: elite enemy:");
            wwhd_log(name);
        }
    }
}

static void el_round_health(fopAc_ac_c* a, f32 ratio);

/* What the owner sends with the health (actor_sync.h as_read_values): 0 for no draw. */
static u8 el_tag_of(const void* actor) {
    const el_actor* e = el_find(actor);
    if (!e) return 0;
    if (!e->elite) return (u8)EL_TAG_REGULAR;
    return (u8)(EL_TAG_ELITE | ((u32)e->ability << 5) | e->seed);
}

/* The owner's draw on this game's copy (actor_sync.h as_before_execute, before the owner's health
 * is applied): the health is rescaled at once to the new factor. */
static void el_adopt(void* actor, u8 tag) {
    el_actor* e = el_find(actor);
    if (!e || (tag != EL_TAG_REGULAR && !(tag & EL_TAG_ELITE))) return;
    u8 elite = (tag & EL_TAG_ELITE) != 0;
    u8 ability = elite ? (u8)((tag >> 5) & 3u) : e->ability;
    u8 seed = elite ? (u8)((tag & 0x1Fu) % EC_PALETTE) : e->seed;
    if (elite == e->elite && ability == e->ability && seed == e->seed) return;
    fopAc_ac_c* a = (fopAc_ac_c*)actor;
    if (elite != e->elite) {
        e->elite = elite;
        if (e->based && a->health > 0) {
            f32 want = el_health_factor(e);
            el_round_health(a, want / e->applied);
            e->applied = want;
        }
    }
    e->ability = ability;
    e->seed = seed;
}

static void el_round_health(fopAc_ac_c* a, f32 ratio) {
    s32 h = (s32)((f32)a->health * ratio + 0.5f);
    if (h < 1) h = 1;
    if (h > EL_HEALTH_MAX) h = EL_HEALTH_MAX;
    a->health = (s8)h;
    if (a->max_health > 0) {
        s32 m = (s32)((f32)a->max_health * ratio + 0.5f);
        if (m < 1) m = 1;
        if (m > EL_HEALTH_MAX) m = EL_HEALTH_MAX;
        a->max_health = (s8)m;
    }
}

/* ---------------------------------------------------------------- bombs */

static cXyz el_bomb_pos, el_bomb_scale;
static csXyz el_bomb_angle;

/* shared: the lit bomb falling on the local Link is made in the LOCAL context, as his own bombs
 * (actor_sync.h as_register_runtime: a weapon of the local Link is announced; the others make a
 * copy run with this player's puppet as player 0, which falls on that puppet and whose blast hurts
 * the players near it). Before 0.2.59 it was untagged: only the player it fell on saw it. The death
 * explosion's bombs stay untagged: every game makes them. */
/* The player's camera exists: a bomb's explosion starts with dCamera_c::ForceLockOff on it
 * (daBomb_c::procExplode_init, d_a_bomb.cpp: dComIfGp_getCamera(dComIfGp_getPlayerCameraID(0))).
 * During a stage change (a teleport, 15:59 crash-20261018-155932.log) the dead elites deleted with
 * the stage exploded and their bombs read a null camera. Camera id: s8 at mPlayerInfo[0] + 4 (play +
 * WWHD_PLAY_PLAYER_OFFSET 0x5B2C, data.h); camera: dComIfG_camera_info_class (0x34 bytes) at play +
 * 0x12A0 + 0x4858 (d_com_inf_game.h), its camera_class* first. */
#define EL_PLAY_CAMERAS (0x12A0 + 0x4858)
#define EL_CAMERA_INFO_BYTES 0x34
static int el_camera_ready(void) {
    const u8* play = wwhd_play_get();
    if (!play) return 0;
    s8 id = *(const volatile s8*)(play + WWHD_PLAY_PLAYER_OFFSET + 4);
    if (id < 0 || id > 3) return 0;
    return *(const volatile u32*)(play + EL_PLAY_CAMERAS + (u32)id * EL_CAMERA_INFO_BYTES) != 0;
}

static void el_make_bomb(u32 bomb_state, f32 x, f32 y, f32 z, s8 room, int shared) {
    if (!el_camera_ready()) return;
    el_bomb_pos.x = x;
    el_bomb_pos.y = y;
    el_bomb_pos.z = z;
    el_bomb_angle.x = el_bomb_angle.y = el_bomb_angle.z = 0;
    el_bomb_scale.x = el_bomb_scale.y = el_bomb_scale.z = 1.0f;
    u8 saved = as_context;
    as_context = shared && as_session ? AS_CTX_LOCAL : AS_CTX_NONE;
    as_spawning = !shared;
    wwhd_fopAcM_create(EL_PROC_BOMB, bomb_state | EL_BOMB_VERSION, (u32)&el_bomb_pos, (s32)room, (u32)&el_bomb_angle,
                       (u32)&el_bomb_scale, 0xFFFFFFFFu, 0);
    as_spawning = 0;
    as_context = saved;
}

/* fopAcM_createItem (025D8870, f_op_actor_mng.h: pos, item, bit, room, type, angle, action, scale;
 * action 4: thrown out, as the death puff's drops, d_a_disappear.cpp) */
WWHD_GAME_FUNC(0x025D8870, u32, el_create_item, (u32 pos, s32 item, s32 bit, s32 room, s32 type, u32 angle,
                                                 s32 action, u32 scale));
static cXyz el_item_pos;

/* The spoils this game's player has ever picked up; returns how many. */
static u32 el_unlocked_spoils(u8* out) {
    u32 save = WWHD_GAME_DATA(EL_SAVE_POINTER, u32), n = 0;
    if (!save) return 0;
    for (u32 i = 0; i != EL_SPOILS; ++i)
        if (el_is_beast(save + EL_SAVE_GET_BAG, el_spoil_beast[i])) out[n++] = el_spoils[i];
    return n;
}

/* Three random spoils among those unlocked (else blue rupees) and sometimes a purple rupee, by the
 * game that runs the elite: made in its OWNED context they are announced like an enemy's drops
 * (actor_sync.h), so every player gets the same ones (the unlocked ones of the owner's save); an
 * elite not synchronised drops in each game. */
static void el_loot(el_actor* e) {
    if (!e->owned) return;
    u8 unlocked[EL_SPOILS];
    u32 unlocked_count = el_unlocked_spoils(unlocked);
    u8 saved = as_context;
    as_context = as_session ? AS_CTX_OWNED : AS_CTX_NONE;
    for (u32 i = 0; i != EL_LOOT + 1; ++i) {
        s32 item;
        if (i == EL_LOOT) {
            if (el_random() % 100u >= EL_PURPLE_PERCENT) break;
            item = EL_PURPLE_RUPEE;
        } else {
            item = unlocked_count ? unlocked[el_random() % unlocked_count] : EL_BLUE_RUPEE;
        }
        el_item_pos.x = e->last_pos[0] + (el_random01() - 0.5f) * 80.0f;
        el_item_pos.y = e->last_pos[1] + 20.0f;
        el_item_pos.z = e->last_pos[2] + (el_random01() - 0.5f) * 80.0f;
        el_create_item((u32)&el_item_pos, item, -1, (s32)e->last_room, 0, 0, 4, 0);
    }
    as_context = saved;
}

static void el_explode(el_actor* e) {
    if (e->exploded) return;
    e->exploded = 1;
    f32 x = e->last_pos[0], y = e->last_pos[1] + 30.0f, z = e->last_pos[2];
    el_make_bomb(EL_BOMB_BIG_BLAST, x, y, z, e->last_room, 0);
    static const f32 ring_x[6] = {1.0f, 0.5f, -0.5f, -1.0f, -0.5f, 0.5f};
    static const f32 ring_z[6] = {0.0f, 0.866f, 0.866f, 0.0f, -0.866f, -0.866f};
    for (int i = 0; i != 6; ++i)
        el_make_bomb(EL_BOMB_BLAST, x + EL_BLAST_RADIUS * ring_x[i], y, z + EL_BLAST_RADIUS * ring_z[i], e->last_room, 0);
    el_loot(e);
    wwhd_log("multiplayer: an elite enemy exploded");
}

/* ---------------------------------------------------------------- faculties */

static f32 el_cos(f32 angle, f32* sine) {
    /* cos and sin by their series on [-pi, pi] (no libm in the guest) */
    f32 t = angle > 3.1415927f ? angle - 6.2831853f : angle, t2 = t * t;
    *sine = t - t * t2 / 6.0f + t * t2 * t2 / 120.0f - t * t2 * t2 * t2 / 5040.0f;
    return 1.0f - t2 / 2.0f + t2 * t2 / 24.0f - t2 * t2 * t2 / 720.0f;
}

/* The electric elites' sparks: a short burst of the Yellow ChuChu's effect on it now and then
 * (dPa_control_c::set 025A847C through particle_sync.h's original, so it is not sent; group 0 as
 * dComIfGp_particle_set; the emitter is told to stop after EL_SPARK_FRAMES and never kept). */
static cXyz el_spark_pos;
static void el_sparks(el_actor* e, const fopAc_ac_c* a, double dt) {
    e->spark_clock -= dt;
    if (e->spark_clock > 0) return;
    e->spark_clock = EL_SPARK_PERIOD;
    const u8* play = wwhd_play_get();
    u32 pa = play ? *(const volatile u32*)(play + PS_PLAY_PARTICLE) : 0;
    if (!pa) return;
    el_spark_pos.x = a->current.pos.x + (el_random01() - 0.5f) * 60.0f;
    el_spark_pos.y = a->current.pos.y + 40.0f + el_random01() * 60.0f;
    el_spark_pos.z = a->current.pos.z + (el_random01() - 0.5f) * 60.0f;
    u32 em = ps_set_original(pa, 0, EL_SPARK_ID, (u32)&el_spark_pos, 0, 0, 0xFF, 0, 0xFFFFFFFFu, 0, 0, 0);
    if (em) *(volatile s32*)(em + PS_EMITTER_MAX_FRAME) = EL_SPARK_FRAMES;
}

/* A shock for the local Link, given in its execute (mod.c el_link_step). */
static int el_shock_pending;
static f32 el_shock_amount;

/* daPy_lk_c (d_a_player_main_03.cpp, signatures in wwhd/bindings.h): setDamagePointWait 023F9D1C
 * (this, f32: when Link is not already in his damage wait, the damage, a 30-frame wait, his hurt
 * voice and sound) and setDamageElecEmitter 023F9340 (this: the sparks on Link). What the game's
 * damage check does when Link touches an electrified floor (attribute 0x16, outside a rope or the
 * water): no proc change, no event. */
WWHD_GAME_FUNC(0x023F9D1C, void, el_set_damage_point_wait, (u32 self, f32 amount));
WWHD_GAME_FUNC(0x023F9340, void, el_set_elec_emitter, (u32 self));

/* The shock, like an electric barrier: Link keeps control. 0.2.50-0.2.55 used the paralysis
 * (procElecDamage_init 023F95F4), which orders a compulsory event and changes Link's proc: soft
 * locks in some modes even with the guards, and a fall on his back with a source position. Called
 * for the local Link only (mod.c, never a puppet; as_link is only set while the actors sync). */
static void el_link_step(void* link) {
    if (!link || !el_shock_pending) return;
    el_shock_pending = 0;
    const u8* play = wwhd_play_get();
    if (!play || play[WWHD_PLAY_EVENT_RUNNING_OFFSET]) return;
    el_set_damage_point_wait((u32)link, -el_shock_amount);
    el_set_elec_emitter((u32)link);
    wwhd_log("multiplayer: an electric elite shocked Link");
}

/* A Keese or a Miniblin next to the summoner, made by the game that runs it as the launcher's shop
 * makes its creatures (actor_sync.h as_summon: synchronised, the others get it). Parameters of the
 * game's own stages (ACTR read in M_NewD2 and majroom): the Keese 0xFFFF0200, angle z 0xFF; the
 * Miniblin without its respawn (bits 5-7 = 7) nor its off-screen wait (bit 4) nor switches
 * (d_a_pt.cpp create): 0xFFFF0FEF. */
static void el_summon(el_actor* e, const fopAc_ac_c* a) {
    int bat = el_random() % 2u == 0;
    f32 sine, cosine = el_cos(el_random01() * 6.2831853f, &sine);
    f32 pos[3] = {a->current.pos.x + 150.0f * cosine, a->current.pos.y + (bat ? 150.0f : 20.0f),
                  a->current.pos.z + 150.0f * sine};
    if (as_summon(bat ? "keeth" : "Puti", bat ? 0xFFFF0200u : 0xFFFF0FEFu, 0, bat ? 0xFF : 0, pos, a->shape_angle.y,
                  a->current.roomNo, 0))
        ++e->summons;
}

static void el_faculty(el_actor* e, const fopAc_ac_c* a, double dt) {
    if (e->ability == EL_ABILITY_NONE) return;
    const fopAc_ac_c* link = (const fopAc_ac_c*)as_link;
    const u8* play = wwhd_play_get();
    if (!link || !play) return;
    f32 dx = link->current.pos.x - a->current.pos.x, dz = link->current.pos.z - a->current.pos.z;
    f32 dy = link->current.pos.y - a->current.pos.y;
    f32 d2 = dx * dx + dz * dz + dy * dy;
    /* the sparks only within 10 m (1000 units), like the crown and the names */
    if (e->ability == EL_ABILITY_ELECTRIC && d2 < EL_CROWN_HIDE * EL_CROWN_HIDE) el_sparks(e, a, dt);
    if (play[WWHD_PLAY_EVENT_RUNNING_OFFSET]) return;
    f32 range = e->ability == EL_ABILITY_BOMBS ? EL_BOMB_RANGE
              : e->ability == EL_ABILITY_SUMMON ? EL_SUMMON_RANGE : EL_SHOCK_RANGE;
    if (d2 > range * range) return;
    e->ability_clock -= dt;
    if (e->ability_clock > 0) return;
    if (e->ability == EL_ABILITY_BOMBS) {
        e->ability_clock = EL_BOMB_MIN + (double)el_random01() * EL_BOMB_SPREAD;
        /* near the local Link, a random place 60 to 180 units around, from above */
        f32 r = 60.0f + el_random01() * 120.0f, sine, cosine = el_cos(el_random01() * 6.2831853f, &sine);
        el_make_bomb(EL_BOMB_LIT, link->current.pos.x + r * cosine, link->current.pos.y + 220.0f,
                     link->current.pos.z + r * sine, link->current.roomNo, 1);
    } else if (e->ability == EL_ABILITY_SUMMON) {
        e->ability_clock = EL_SUMMON_MIN + (double)el_random01() * EL_SUMMON_SPREAD;
        if (e->owned && e->summons < EL_SUMMON_MAX) el_summon(e, a); /* rare: a few per fight */
    } else {
        e->ability_clock = EL_SHOCK_MIN + (double)el_random01() * EL_SHOCK_SPREAD;
        el_shock_pending = 1;
        el_shock_amount = EL_SHOCK_DAMAGE * el_damage_factor(e);
    }
}

/* ---------------------------------------------------------------- the actor's execute */

static el_actor* el_execute_begin(void* actor) {
    el_actor* e = el_find(actor);
    el_fast = e && e->elite;
    el_growing = el_fast;
    return e;
}

static void el_execute_end(el_actor* e) {
    el_fast = 0;
    el_growing = 0;
    if (!e) return;
    fopAc_ac_c* a = (fopAc_ac_c*)e->actor;
    e->last_pos[0] = a->current.pos.x;
    e->last_pos[1] = a->current.pos.y;
    e->last_pos[2] = a->current.pos.z;
    e->last_room = a->current.roomNo;
    double dt = wwhd_logic_dt();
    if (!e->based) {
        if (a->health <= 0) return; /* its health comes later in its creation */
        e->based = 1;
        e->base_health = a->health;
        e->applied = 1.0f;
    }
    if (a->health > 0) {
        f32 want = el_health_factor(e);
        if (want != e->applied) {
            el_round_health(a, want / e->applied);
            e->applied = want;
        }
    }
    if (!e->elite) return;
    if (a->health <= 0) {
        e->dead_clock += dt;
        if (e->dead_clock >= EL_DEATH_WAIT) el_explode(e);
        return;
    }
    e->dead_clock = 0;
    as_slot* slot = as_find((const void*)e->actor);
    e->owned = !slot || !as_follows_owner(slot);
    el_faculty(e, a, dt);
}

/* fopAc_Delete: the entry, found before the deletion (el_find), and the health it had then. */
static el_actor* el_delete_begin(const void* actor) { return el_find(actor); }

/* The deletion was done: an elite deleted dead explodes; the entry goes. */
static void el_delete_end(el_actor* e, s8 health) {
    if (!e || !e->used) return;
    if (e->elite && e->based && health <= 0) el_explode(e);
    e->used = 0;
    --el_count;
}

/* ---------------------------------------------------------------- damage to the local Link */

WWHD_GAME_ORIGINAL(0x02518670, void, el_plus_dmg_original, (u32 self, u32 at, u32 tg, u32 at_stts, u32 tg_stts));

WWHD_REPLACE(0x02518670, void, el_plus_dmg, (u32 self, u32 at, u32 tg, u32 at_stts, u32 tg_stts)) {
    if (!as_session || !as_link || !tg_stts || !at_stts ||
        *(const volatile u32*)(tg_stts + EL_STTS_ACTOR) != as_link) {
        el_plus_dmg_original(self, at, tg, at_stts, tg_stts);
        return;
    }
    u32 attacker = *(const volatile u32*)(at_stts + EL_STTS_ACTOR);
    volatile u8* dmg = (volatile u8*)(tg_stts + EL_STTS_DMG);
    u8 before = *dmg;
    el_plus_dmg_original(self, at, tg, at_stts, tg_stts);
    u8 after = *dmg;
    if (after <= before || !attacker || ((const fopAc_ac_c*)attacker)->group != EL_GROUP_ENEMY) return;
    f32 f = el_damage_factor(el_find((const void*)attacker));
    if (f <= 1.0f) return;
    u32 scaled = (u32)((f32)after * f + 0.5f);
    if (scaled > 255u) scaled = 255u;
    if (scaled > after) *dmg = (u8)scaled;
}

/* ---------------------------------------------------------------- faster animations */

WWHD_GAME_ORIGINAL(0x027F2FC4, void, el_frame_update_original, (u32 self));
WWHD_GAME_ORIGINAL(0x027F2BF8, s32, el_frame_pass_original, (u32 self, f32 pass_frame));

WWHD_REPLACE(0x027F2FC4, void, el_frame_update, (u32 self)) {
    if (!el_fast || !self) {
        el_frame_update_original(self);
        return;
    }
    volatile f32* rate = (volatile f32*)self;
    f32 r = *rate;
    *rate = r * EL_ELITE_SPEED;
    el_frame_update_original(self);
    *rate = r;
}

WWHD_REPLACE(0x027F2BF8, s32, el_frame_pass, (u32 self, f32 pass_frame)) {
    if (!el_fast || !self) return el_frame_pass_original(self, pass_frame);
    volatile f32* rate = (volatile f32*)self;
    f32 r = *rate;
    *rate = r * EL_ELITE_SPEED;
    s32 result = el_frame_pass_original(self, pass_frame);
    *rate = r;
    return result;
}

/* ---------------------------------------------------------------- recoloured textures */

#define EL_ARENA_SPAN 0x10000u       /* TUNIC_ADDRESS_SPAN */
#define EL_ARENA_RUNS 512
#define EL_SETS 24
#define EL_SET_IDLE 90       /* logic steps (3 s) unused before a set may be replaced */
#define EL_FAILS 8
#define EL_FAIL_STEPS 600    /* a set that did not fit is not tried again for 20 s */
#define EL_SET_TEXTURES 24
#define EL_PLANS 24
#define EL_TABLE_MAX 64

typedef struct el_run {
    u32 start, end;
} el_run;

static u8* el_arena_raw;
static u32 el_arena_base, el_arena_size;
static el_run el_runs[EL_ARENA_RUNS];
static u32 el_run_count;
static int el_arena_failed;

static int el_arena_open(void) {
    if (el_arena_raw) return 1;
    if (el_arena_failed) return 0;
    static const u32 sizes[] = {3u << 20, 2u << 20, 1u << 20};
    for (u32 i = 0; i != 3 && !el_arena_raw; ++i) {
        el_arena_raw = (u8*)wwhd_malloc(sizes[i] + EL_ARENA_SPAN);
        if (el_arena_raw) el_arena_size = sizes[i];
    }
    if (!el_arena_raw) {
        el_arena_failed = 1;
        wwhd_log("multiplayer: no mod heap for the elites' colours, they keep their own");
        return 0;
    }
    el_arena_base = ((u32)el_arena_raw + EL_ARENA_SPAN - 1) & ~(EL_ARENA_SPAN - 1);
    wwhd_log_int("multiplayer: elite colour arena (bytes)", (int)el_arena_size);
    return 1;
}

/* A free place of size bytes whose low 16 bits are like's; 0 when the arena is full. */
static u32 el_arena_alloc(u32 size, u32 like) {
    if (!el_arena_open() || !size || el_run_count == EL_ARENA_RUNS) return 0;
    for (u32 p = el_arena_base + (like & (EL_ARENA_SPAN - 1)); p + size <= el_arena_base + el_arena_size;
         p += EL_ARENA_SPAN) {
        u32 at = 0, clash = 0;
        while (at != el_run_count && el_runs[at].end <= p) ++at;
        for (u32 k = at; k != el_run_count && el_runs[k].start < p + size; ++k) clash = 1;
        if (clash) continue;
        for (u32 k = el_run_count; k != at; --k) el_runs[k] = el_runs[k - 1];
        el_runs[at].start = p;
        el_runs[at].end = p + size;
        ++el_run_count;
        return p;
    }
    return 0;
}

static void el_arena_free(u32 p) {
    for (u32 k = 0; k != el_run_count; ++k)
        if (el_runs[k].start == p) {
            for (; k + 1 < el_run_count; ++k) el_runs[k] = el_runs[k + 1];
            --el_run_count;
            return;
        }
}

typedef struct el_clone {
    u32 original;        /* the game's HD texture object */
    u8* gx2;             /* its copy, pointing to the recoloured image and mip */
    u32 image, mip;
} el_clone;

typedef struct el_set {
    u8 used;
    u8 seed;             /* the elite's hue, 0..11 */
    u32 model_data;
    u32 count;
    el_clone clone[EL_SET_TEXTURES];
    ec_plan plan;        /* with the elite's hue: the materials are recoloured at each draw */
    unsigned long long last_step;
} el_set;

typedef struct el_plan_entry {
    u32 model_data;
    ec_plan plan;
} el_plan_entry;

static el_set el_sets[EL_SETS];
static el_plan_entry el_plans[EL_PLANS];
static u32 el_plan_next;

static void el_set_free(el_set* s) {
    for (u32 i = 0; i != s->count; ++i) {
        el_clone* c = &s->clone[i];
        if (c->gx2) wwhd_free(c->gx2);
        if (c->image) el_arena_free(c->image);
        if (c->mip) el_arena_free(c->mip);
    }
    s->used = 0;
    s->count = 0;
}

static u32 el_table(u32 model_data, u32* count, u32* res) {
    if (!model_data || (model_data & 3)) return 0;
    u32 table = tunic_ld32(model_data + MODEL_DATA_TEXTURE_TABLE);
    if (!table || (table & 1)) return 0;
    *count = *(const volatile u16*)table;
    *res = tunic_ld32(table + 4);
    if (!*res || !*count) return 0;
    if (*count > EL_TABLE_MAX) *count = EL_TABLE_MAX;
    return table;
}

#define EL_MAT_MAX 48

/* A colour of 3 floats at slot as 0xRRGGBB (components above 1 scaled down by *scale); 0 when not
 * a colour. */
static int el_slot_rgb(u32 slot, tc_u32* rgb, f32* scale) {
    if (!slot || (slot & 3)) return 0;
    f32 r = *(volatile f32*)slot, g = *(volatile f32*)(slot + 4), b = *(volatile f32*)(slot + 8);
    if (!(r >= 0.0f && r <= 8.0f && g >= 0.0f && g <= 8.0f && b >= 0.0f && b <= 8.0f)) return 0;
    f32 m = r > g ? (r > b ? r : b) : (g > b ? g : b);
    f32 k = m > 1.0f ? m : 1.0f;
    *scale = k;
    *rgb = ((tc_u32)(r / k * 255.0f + 0.5f) << 16) | ((tc_u32)(g / k * 255.0f + 0.5f) << 8) |
           (tc_u32)(b / k * 255.0f + 0.5f);
    return 1;
}

/* The primary colour of a model's textures, cached by model data (the materials' colours are an
 * instance's own: el_paint_instance). */
static ec_plan el_plan_of(u32 model_data) {
    for (u32 i = 0; i != EL_PLANS; ++i)
        if (el_plans[i].model_data == model_data) return el_plans[i].plan;
    static ec_hist h;
    ec_hist_clear(&h);
    u32 count, res;
    if (el_table(model_data, &count, &res)) {
        for (u32 j = 0; j != count; ++j) {
            u32 gx2 = tunic_ld32(res + j * TEX_HEADER_BYTES + TEX_HEADER_HD);
            tunic_texture t;
            if (!gx2 || (gx2 & 3) || !tunic_read_surface(gx2, &t)) continue;
            ec_hist_surface(&h, t.format, (const tc_u8*)t.image, t.image_size);
        }
    }
    el_plan_entry* e = &el_plans[el_plan_next++ % EL_PLANS];
    e->model_data = model_data;
    e->plan = ec_dominant(&h);
    return e->plan;
}

/* Copies one texture recoloured; 0 (nothing kept) when none of it is of the primary colour. */
static int el_clone_texture(el_clone* c, u32 gx2, const ec_plan* plan) {
    tunic_texture t;
    if (!tunic_read_surface(gx2, &t)) return 0;
    u32 image = el_arena_alloc(t.image_size, t.image);
    if (!image) return -1;
    memcpy((void*)image, (const void*)t.image, t.image_size);
    if (ec_recolor_surface(plan, t.format, (tc_u8*)image, t.image_size) <= 0) {
        el_arena_free(image);
        return 0;
    }
    u32 mip = 0;
    if (t.mip_size) {
        mip = el_arena_alloc(t.mip_size, t.mip);
        if (!mip) {
            el_arena_free(image);
            return -1;
        }
        memcpy((void*)mip, (const void*)t.mip, t.mip_size);
        ec_recolor_surface(plan, t.format, (tc_u8*)mip, t.mip_size);
    }
    u8* copy = (u8*)wwhd_malloc(HD_TEXTURE_BYTES);
    if (!copy) {
        el_arena_free(image);
        if (mip) el_arena_free(mip);
        return -1;
    }
    memcpy(copy, (const void*)gx2, HD_TEXTURE_BYTES);
    tunic_st32((u32)copy + GX2_SURFACE_IMAGE, image);
    tunic_st32((u32)copy + GX2_SURFACE_MIP, mip);
    c->original = gx2;
    c->gx2 = copy;
    c->image = image;
    c->mip = mip;
    return 1;
}

static int el_build_set(el_set* s, u32 model_data, u8 seed) {
    s->used = 1;
    s->seed = seed;
    s->model_data = model_data;
    s->count = 0;
    ec_plan plan = el_plan_of(model_data);
    s->plan = plan;
    if (plan.mode == EC_NONE) return 1; /* nothing to recolour: an empty set */
    ec_set_target(&plan, seed);
    s->plan = plan;
    u32 count, res;
    if (!el_table(model_data, &count, &res)) return 1;
    for (u32 j = 0; j != count && s->count != EL_SET_TEXTURES; ++j) {
        u32 gx2 = tunic_ld32(res + j * TEX_HEADER_BYTES + TEX_HEADER_HD);
        if (!gx2 || (gx2 & 3)) continue;
        int known = 0;
        for (u32 k = 0; k != s->count && !known; ++k) known = s->clone[k].original == gx2;
        if (known) continue;
        int made = el_clone_texture(&s->clone[s->count], gx2, &plan);
        if (made < 0) return 0; /* the arena is full */
        if (made) ++s->count;
    }
    return 1;
}

typedef struct el_fail {
    u32 model_data;
    u8 seed;
    unsigned long long step;
} el_fail;

static el_fail el_fails[EL_FAILS];
static u32 el_fail_next;
static unsigned long long el_build_step = ~0ull;

/* The longest unused set, if unused for EL_SET_IDLE steps (never one drawn lately). */
static el_set* el_idle_set(unsigned long long step, const el_set* keep) {
    el_set* oldest = 0;
    for (u32 i = 0; i != EL_SETS; ++i) {
        el_set* o = &el_sets[i];
        if (o == keep || !o->used || step - o->last_step < EL_SET_IDLE) continue;
        if (!oldest || o->last_step < oldest->last_step) oldest = o;
    }
    return oldest;
}

/* The recoloured copies of a model for an elite's hue, made the first time. A build copies and
 * recolours whole textures: at most one a logic step, a set only replaces one unused for 3 s, and
 * one that did not fit waits 20 s. More elites in sight than the sets hold keep their own colours
 * (0.2.40-0.2.53 replaced the set of the step before at every step: with several elites side by
 * side - Keese, Magtails, Boko Babas - the sets were rebuilt over and over, down to 3 fps). */
static el_set* el_set_for(u32 model_data, u8 seed) {
    unsigned long long step = wwhd_logic_step();
    el_set* s = 0;
    for (u32 i = 0; i != EL_SETS; ++i) {
        el_set* o = &el_sets[i];
        if (o->used && o->model_data == model_data && o->seed == seed) {
            o->last_step = step;
            return o;
        }
        if (!o->used && !s) s = o;
    }
    if (el_build_step == step) return 0;
    for (u32 i = 0; i != EL_FAILS; ++i)
        if (el_fails[i].model_data == model_data && el_fails[i].seed == seed && step - el_fails[i].step < EL_FAIL_STEPS)
            return 0;
    if (!s) {
        s = el_idle_set(step, 0);
        if (!s) return 0;
        el_set_free(s);
    }
    el_build_step = step;
    for (int attempt = 0; attempt != 2; ++attempt) {
        if (el_build_set(s, model_data, seed)) {
            s->last_step = step;
            return s;
        }
        el_set_free(s);
        el_set* victim = el_idle_set(step, s); /* arena full: an unused set goes, then once more */
        if (!victim) break;
        el_set_free(victim);
    }
    el_fail* f = &el_fails[el_fail_next++ % EL_FAILS];
    f->model_data = model_data;
    f->seed = seed;
    f->step = step;
    static int full_logged;
    if (!full_logged++) wwhd_log("multiplayer: elite colour arena full, an elite keeps its colour");
    return 0;
}

#define EL_SWAPS 32
static u32 el_swap_table;
static u32 el_swap_count;
static u32 el_swap_index[EL_SWAPS];
static u32 el_swap_original[EL_SWAPS];

/* ---- the materials' colours of the instance (the colours the shader reads, animation included)
 * The HD J3DModel draw (027F55FC, generated code) copies each material's parameter block into the
 * model's own per-material entries (J3DModel +0x34, 0x3C bytes each: +0 the shader resource,
 * +0x04 flags, +0x0C a dirty bit set, +0x28 the uniform data; 027F9FB8 copies parameter k found by
 * name in the resource's dictionary, +0x38 relative, with 027DF9B0, at the uniform data + the
 * descriptor's offset (+2) of the table +0x34 relative, 0x14 bytes each; d_a_obj_ftree.cpp reads a
 * tev_color the same way), then runs the material animations (brk: the ChuChus' body colour,
 * tev_color1 of SC_dbady, read in the player's own Cc.bfres) into that data, then 027F53CC. At
 * 027F53CC of an elite's draw the colours are final: every coloured one is painted in the
 * elite's colour (ec_paint_rgb) and the entry marked to upload, as 027F9FB8 marks it. The values
 * belong to that model only and are not put back; a value still equal to what was written last
 * time keeps its original (never painted twice). */
#define EL_COLOR_NAMES 11
static const char* const el_color_names[EL_COLOR_NAMES] = {
    "amb_color0", "amb_color1", "mat_color0", "mat_color1", "tev_color0", "tev_color1", "tev_color2",
    "konst0", "konst1", "konst2", "konst3",
};
#define EL_ENTRY_BYTES 0x3C
#define EL_INSTANCES 32
#define EL_INSTANCE_COLORS 96

WWHD_GAME_FUNC(0x027DF9B0, s32, el_find_param, (u32 dictionary, u32 name));

typedef struct el_color_slot {
    u32 address;
    f32 original[3], written[3];
} el_color_slot;

/* Where a colour parameter of the model is: its material entry, the resource's table, its index
 * and the second one marked, its offset in the entry's uniform data (+0x28, read at each draw). */
typedef struct el_spot {
    u32 entry, table;
    u16 index, second, offset;
} el_spot;

typedef struct el_instance {
    u32 model;
    u32 base, materials, resource; /* what the spots were found with (resource: material 0's) */
    u32 spots;
    el_spot spot[EL_INSTANCE_COLORS];
    u32 count;
    el_color_slot slot[EL_INSTANCE_COLORS];
    unsigned long long step;
} el_instance;

static el_instance el_instances[EL_INSTANCES];
static u32 el_draw_model;

static el_instance* el_instance_for(u32 model) {
    el_instance* oldest = &el_instances[0];
    for (u32 i = 0; i != EL_INSTANCES; ++i) {
        if (el_instances[i].model == model) return &el_instances[i];
        if (el_instances[i].step < oldest->step) oldest = &el_instances[i];
    }
    oldest->model = model;
    oldest->base = 0;
    oldest->materials = 0;
    oldest->spots = 0;
    oldest->count = 0;
    return oldest;
}

/* The model's colour parameters, looked up by name once per model (11 names in every material's
 * dictionary at every draw were most of an elite's cost). */
static void el_find_spots(el_instance* in, u32 base, u32 count) {
    in->base = base;
    in->materials = count;
    in->resource = count ? *(const volatile u32*)base : 0;
    in->spots = 0;
    in->count = 0;
    for (u32 i = 0; i != count; ++i) {
        u32 entry = base + i * EL_ENTRY_BYTES;
        u32 resource = *(const volatile u32*)entry;
        if (!resource || (resource & 3)) continue;
        s32 names = (s32)*(const volatile u32*)(resource + 0x38), colors = (s32)*(const volatile u32*)(resource + 0x34);
        if (!names || !colors) continue;
        u32 dictionary = resource + 0x38 + (u32)names, table = resource + 0x34 + (u32)colors;
        for (u32 k = 0; k != EL_COLOR_NAMES && in->spots != EL_INSTANCE_COLORS; ++k) {
            s32 index = el_find_param(dictionary, (u32)el_color_names[k]);
            if (index < 0) continue;
            el_spot* p = &in->spot[in->spots++];
            p->entry = entry;
            p->table = table;
            p->index = (u16)index;
            p->second = *(const volatile u16*)(table + (u32)index * 0x14 + 0x0C);
            p->offset = *(const volatile u16*)(table + (u32)index * 0x14 + 2);
        }
    }
}

static void el_mark_param(u32 entry, u32 table, u32 index) {
    u32 bits = *(const volatile u32*)(entry + 0x0C);
    if (!bits || (s32)*(const volatile u32*)(table + index * 0x14 + 4) < 0) return;
    *(volatile u16*)(entry + 4) = (u16)(*(const volatile u16*)(entry + 4) | 4u);
    *(volatile u32*)(bits + (index >> 5) * 4) |= 1u << (index & 31u);
}

typedef struct el_found {
    u32 address, entry, table, index, second;
    f32 original[3];
} el_found;

/* mDoExt_drawModel of an elite: the model's colours, once its animations ran. */
static void el_paint_instance(u32 model, u32 seed) {
    u32 count = *(const volatile u16*)(model + 0x2A), base = *(const volatile u32*)(model + 0x34);
    if (!base || (base & 3)) return;
    if (count > EL_MAT_MAX) count = EL_MAT_MAX;
    el_instance* in = el_instance_for(model);
    in->step = wwhd_logic_step();
    if (in->base != base || in->materials != count || (count && in->resource != *(const volatile u32*)base))
        el_find_spots(in, base, count);
    static el_found found[EL_INSTANCE_COLORS];
    u32 n = 0;
    static ec_hist h;
    ec_hist_clear(&h);
    for (u32 i = 0; i != in->spots; ++i) {
        const el_spot* p = &in->spot[i];
        u32 data = *(const volatile u32*)(p->entry + 0x28);
        if (!data) continue;
        u32 address = data + p->offset;
        if (address & 3) continue;
        el_found* f = &found[n];
        volatile f32* c = (volatile f32*)address;
        for (u32 j = 0; j != 3; ++j) f->original[j] = c[j];
        for (u32 s = 0; s != in->count; ++s) { /* our own colour of last time: its original */
            const el_color_slot* o = &in->slot[s];
            if (o->address == address && o->written[0] == f->original[0] && o->written[1] == f->original[1] &&
                o->written[2] == f->original[2]) {
                for (u32 j = 0; j != 3; ++j) f->original[j] = o->original[j];
                break;
            }
        }
        f->address = address;
        f->entry = p->entry;
        f->table = p->table;
        f->index = p->index;
        f->second = p->second;
        tc_u32 rgb;
        f32 scale;
        if (el_slot_rgb((u32)f->original, &rgb, &scale)) {
            tc_hsb px = tc_rgb_to_hsb(rgb);
            if (px.s >= EC_SATURATION_MIN && px.b >= EC_BRIGHTNESS_MIN) ec_hist_rgb(&h, rgb);
        }
        ++n;
    }
    ec_plan plan = ec_dominant(&h);
    if (plan.mode != EC_HUE) return; /* no coloured material: the textures carry its colour */
    ec_set_target(&plan, seed);
    in->count = 0;
    for (u32 i = 0; i != n; ++i) {
        el_found* f = &found[i];
        tc_u32 rgb, out;
        f32 scale;
        if (!el_slot_rgb((u32)f->original, &rgb, &scale) || !ec_paint_rgb(&plan, rgb, &out)) continue;
        volatile f32* c = (volatile f32*)f->address;
        c[0] = (f32)((out >> 16) & 0xFF) / 255.0f * scale;
        c[1] = (f32)((out >> 8) & 0xFF) / 255.0f * scale;
        c[2] = (f32)(out & 0xFF) / 255.0f * scale;
        el_mark_param(f->entry, f->table, f->index);
        el_mark_param(f->entry, f->table, f->second);
        el_color_slot* o = &in->slot[in->count++];
        o->address = f->address;
        for (u32 j = 0; j != 3; ++j) {
            o->original[j] = f->original[j];
            o->written[j] = c[j];
        }
    }
}

/* 027F53CC (the HD model draw's step after the material animations, generated code), during an
 * elite's mDoExt_drawModel. */
WWHD_HOOK(0x027F53CC, el_model_animated, (u32 model, u32 a1, u32 a2, u32 a3)) {
    (void)a1;
    (void)a2;
    (void)a3;
    if (!el_drawing || !model || model != el_draw_model) return;
    el_paint_instance(model, el_drawing->seed);
}

/* mDoExt_drawModel (025E2BF4, model and draw data), before it draws: an elite's colours. */
WWHD_HOOK(0x025E2BF4, el_draw_model_begin, (u32 model, u32 supplied)) {
    (void)supplied;
    ghost_model_drawn(model, supplied); /* a ghost Link's model: translucent (puppet.h) */
    el_swap_table = 0;
    el_swap_count = 0;
    el_draw_model = el_drawing ? model : 0;
    if (!el_drawing || !model) return;
    u32 model_data = tunic_ld32(model + EL_MODEL_DATA);
    el_set* s = el_set_for(model_data, el_drawing->seed);
    if (!s) return;
    u32 count, res;
    u32 table = el_table(model_data, &count, &res);
    if (!table || !s->count) return;
    for (u32 j = 0; j != count && el_swap_count != EL_SWAPS; ++j) {
        u32 header = res + j * TEX_HEADER_BYTES;
        u32 gx2 = tunic_ld32(header + TEX_HEADER_HD);
        for (u32 k = 0; k != s->count; ++k) {
            if (s->clone[k].original != gx2) continue;
            el_swap_index[el_swap_count] = j;
            el_swap_original[el_swap_count++] = gx2;
            tunic_st32(header + TEX_HEADER_HD, (u32)s->clone[k].gx2);
            tunic_mark_dirty(table, j);
            break;
        }
    }
    if (el_swap_count) el_swap_table = table;
}

WWHD_HOOK_RETURN(0x025E2BF4, el_draw_model_end, (u32 model, u32 supplied)) {
    (void)model;
    (void)supplied;
    el_draw_model = 0;
    if (!el_swap_table) return;
    u32 res = tunic_ld32(el_swap_table + 4);
    for (u32 k = 0; k != el_swap_count; ++k) {
        tunic_st32(res + el_swap_index[k] * TEX_HEADER_BYTES + TEX_HEADER_HD, el_swap_original[k]);
        tunic_mark_dirty(el_swap_table, el_swap_index[k]);
    }
    el_swap_table = 0;
    el_swap_count = 0;
}

static int bonus_yellow_item(const void* actor); /* bonus.h: a yellow heart or magic on the ground */
static u32 bonus_elemental_of(const void* actor); /* bonus.h: an elemental arrow pack, its type */
static void bonus_elemental_fx(const void* actor); /* bonus.h: its element's particles */
#define EL_YELLOW_SEED 2 /* ec_palette's gold */
static const u8 el_elemental_seeds[4] = {0, 0 /* fire: crimson */, 7 /* ice: azure */, 16 /* light: white */};
static el_actor el_yellow_paint; /* the colours of the elites' recolouring, nothing else of an elite */

static el_actor* el_draw_begin(void* actor) {
    el_actor* e = el_find(actor);
    el_drawing = e && e->elite ? e : 0;
    el_growing = el_drawing != 0;
    u32 elemental = el_drawing ? 0 : bonus_elemental_of(actor);
    if (elemental) bonus_elemental_fx(actor);
    if (!el_drawing && (elemental || bonus_yellow_item(actor))) {
        el_yellow_paint.elite = 1;
        el_yellow_paint.seed = elemental ? el_elemental_seeds[elemental & 3] : EL_YELLOW_SEED;
        el_drawing = &el_yellow_paint; /* recoloured in gold (el_draw_model_begin), not bigger */
        el_growing = 0;
    }
    return el_drawing;
}

static void el_draw_end(void) {
    el_drawing = 0;
    el_growing = 0;
}

/* ---------------------------------------------------------------- bigger models */

#define EL_MDL_JOINTS 0x2C   /* J3DModel -> joint matrix block (runtime interp.cpp kMdlJoints) */
#define EL_JNT_MTX 0x10      /* block -> world matrices, 3x4 row-major */
#define EL_JNT_NUM 0x2C      /* block -> u16 count */
#define EL_JNT_MAX 256

/* J3DModel::calc (027F4D5C) of a model of an elite: its world matrices, just rebuilt, scaled by
 * EL_ELITE_SIZE around its root joint (matrix 0's translation, the model's base). */
WWHD_HOOK_RETURN(0x027F4D5C, el_model_calc, (u32 model)) {
    if (!el_growing || !model || (model & 3)) return;
    u32 block = *(const volatile u32*)(model + EL_MDL_JOINTS);
    if (!block || (block & 3)) return;
    u32 mtx = *(const volatile u32*)(block + EL_JNT_MTX);
    u32 n = *(const volatile u16*)(block + EL_JNT_NUM);
    if (!mtx || (mtx & 3) || !n || n > EL_JNT_MAX) return;
    volatile f32* m = (volatile f32*)mtx;
    f32 px = m[3], py = m[7], pz = m[11];
    if (!(px == px && py == py && pz == pz)) return; /* NaN */
    const f32 k = EL_ELITE_SIZE;
    for (u32 j = 0; j != n; ++j, m += 12) {
        for (u32 c = 0; c != 3; ++c) {
            m[c] *= k;
            m[4 + c] *= k;
            m[8 + c] *= k;
        }
        m[3] = px + (m[3] - px) * k;
        m[7] = py + (m[7] - py) * k;
        m[11] = pz + (m[11] - pz) * k;
    }
}

/* ---------------------------------------------------------------- crowns */

static u32 el_crown_texture;
static unsigned long long el_crown_epoch = ~0ull;
static cXyz el_project_in, el_project_out;

/* The local Link's step (mod.c): each elite's crown on the TV picture. */
static void el_update_crowns(const u8* play) {
    f32 screen_w = WWHD_GAME_DATA(ADDR_mDoLib_screenWidth, f32);
    f32 screen_h = WWHD_GAME_DATA(ADDR_mDoLib_screenHeight, f32);
    int view = play && *(const volatile u32*)(play + PLAY_VIEW_OFFSET) != 0;
    for (u32 i = 0; i != EL_MAX; ++i) {
        el_actor* e = &el_actors[i];
        e->crown_visible = 0;
        if (!e->used || !e->elite || !view || screen_w <= 0.0f || screen_h <= 0.0f) continue;
        const fopAc_ac_c* a = (const fopAc_ac_c*)e->actor;
        if (*(const volatile u32*)((const u8*)a + AS_PC_ID) != e->pcid || a->health <= 0 || e->exploded) continue;
        const fopAc_ac_c* link = (const fopAc_ac_c*)as_link;
        if (!link) continue;
        f32 lx = a->current.pos.x - link->current.pos.x, ly = a->current.pos.y - link->current.pos.y,
            lz = a->current.pos.z - link->current.pos.z;
        f32 d2 = lx * lx + ly * ly + lz * lz;
        if (!(d2 < EL_CROWN_HIDE * EL_CROWN_HIDE)) continue; /* like the names: hidden far away */
        f32 alpha = 1.0f;
        if (d2 > EL_CROWN_FADE * EL_CROWN_FADE) {
            /* (HIDE - d) / (HIDE - FADE) with d from d2 by a few Newton steps (no libm) */
            f32 d = (EL_CROWN_FADE + EL_CROWN_HIDE) * 0.5f;
            for (int k = 0; k != 5; ++k) d = 0.5f * (d + d2 / d);
            alpha = (EL_CROWN_HIDE - d) / (EL_CROWN_HIDE - EL_CROWN_FADE);
            if (alpha < 0.0f) alpha = 0.0f;
        }
        /* eyePos when the actor keeps it near itself; some never set it (0, 0, 0) */
        f32 ex = a->eyePos.x - a->current.pos.x, ey = a->eyePos.y - a->current.pos.y, ez = a->eyePos.z - a->current.pos.z;
        if (ex * ex + ey * ey + ez * ez < 600.0f * 600.0f && (a->eyePos.x != 0.0f || a->eyePos.y != 0.0f || a->eyePos.z != 0.0f)) {
            el_project_in.x = a->eyePos.x;
            el_project_in.y = a->eyePos.y + EL_CROWN_HEIGHT;
            el_project_in.z = a->eyePos.z;
        } else {
            el_project_in.x = a->current.pos.x;
            el_project_in.y = a->current.pos.y + EL_CROWN_HEIGHT + 120.0f;
            el_project_in.z = a->current.pos.z;
        }
        wwhd_mDoLib_pos2camera((u32)&el_project_in, (u32)&el_project_out);
        f32 depth = -el_project_out.z;
        if (depth < 30.0f || depth > 6000.0f) continue;
        wwhd_mDoLib_project((u32)&el_project_in, (u32)&el_project_out);
        f32 x = el_project_out.x / screen_w * HUD_WIDTH, y = el_project_out.y / screen_h * HUD_HEIGHT;
        if (x < -40.0f || x > HUD_WIDTH + 40.0f || y < -40.0f || y > HUD_HEIGHT + 40.0f) continue;
        f32 scale = 600.0f / depth;
        if (scale < 0.35f) scale = 0.35f;
        if (scale > 1.2f) scale = 1.2f;
        e->crown_x = x;
        e->crown_y = y;
        e->crown_scale = scale;
        e->crown_alpha = alpha;
        e->crown_visible = 1;
    }
}

static wwhd_hud_element el_crown_element;

static void el_draw_crowns(u32 list) {
    unsigned long long epoch = wwhd_hud_epoch();
    if (epoch != el_crown_epoch) {
        el_crown_epoch = epoch;
        el_crown_texture = wwhd_hud_texture(WWHD_HUD_PACKAGE, "assets/crown.png");
        if (!el_crown_texture) wwhd_log("multiplayer: assets/crown.png not loaded, elites without crown");
    }
    if (!el_crown_texture) return;
    for (u32 i = 0; i != EL_MAX; ++i) {
        const el_actor* e = &el_actors[i];
        if (!e->used || !e->crown_visible) continue;
        f32 w = 64.0f * e->crown_scale, h = 48.0f * e->crown_scale;
        el_crown_element = (wwhd_hud_element){.kind = WWHD_HUD_IMAGE, .anchor = WWHD_HUD_TOP_LEFT, .blend = WWHD_HUD_ALPHA,
                                              .x = e->crown_x - w * 0.5f, .y = e->crown_y - h, .w = w, .h = h,
                                              .thickness = 1, .u1 = 1, .v1 = 1,
                                              .rgba = 0xFFFFFF00u | (u32)(e->crown_alpha * 255.0f),
                                              .image = el_crown_texture};
        wwhd_hud_emit(list, &el_crown_element);
    }
}

/* The session ended: nothing is an elite any more (the actors stay as they are). */
static void el_reset(void) {
    for (u32 i = 0; i != EL_MAX; ++i) el_actors[i].used = 0;
    el_count = 0;
    el_fast = 0;
    el_drawing = 0;
}
