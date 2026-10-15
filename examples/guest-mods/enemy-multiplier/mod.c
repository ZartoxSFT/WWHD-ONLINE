/* Enemy multiplier, ported from the GameCube multiplayer client (WindWakerClient/src/Hooks.cpp,
 * enemyMultiplier, enemy names from WindWakerClient/data/enemies.h).
 *
 * The GameCube version replaced the dStage_actorCreate call of the stage actor loop and created
 * every listed enemy `EnemyMultiplier` times. Here dStage_actorCreate itself is replaced:
 *  - dStage_actorCreate (025C1154) takes ownership of the parameter block: it frees it for an
 *    unknown name or hands it to fpcSCtRq_Request (decomp/wwhd_src/d/d_stage.cpp), so each
 *    extra copy gets its own block from fopAcM_CreateAppend (025D5600, the allocator the stage
 *    loaders use, decomp/wwhd_src/d/d_stage_3.cpp);
 *  - stage_actor_data_class: char name[8] at 0 (decomp/wwhd_src/include/d/d_stage.h);
 *  - fopAcM_prm_class is 0x24 bytes, base.parameters at 0, base.position (cXyz) at 4
 *    (decomp/wwhd_src/include/d/actor/d_a_esa.h).
 * Copies keep the original parameters and setID (killing one clears the spawn like the
 * original). `spread` moves copies onto a small ring so they do not start inside each other;
 * spread 0 reproduces the GameCube behaviour. The GameCube parameter randomizer is optional
 * and off by default: random parameter words select arbitrary actor variants. */
#include "wwhd_guest.h"
#include "wwhd/bindings.h"

#define ADDR_dStage_actorCreate 0x025C1154
#define PRM_BYTES 0x24
#define PRM_PARAMETERS 0x00
#define PRM_POSITION 0x04

WWHD_GAME_ORIGINAL(ADDR_dStage_actorCreate, void, actor_create_original, (u32 data, u32 prm));

static const char* const enemies[] = {
    "Bk", "Bb", "mo2", "Tn", "gmos", "c_red", "c_green", "c_blue", "c_black", "c_kiiro",
    "Pig", "Puti", "wiz_r", "bbaba", "amos", "amos2", "bable", "bable_r", "pow", "Sss",
    "keeth", "Fkeeth", "Fmaster", "Fmastr1", "Fmastr2", "Gy", "magtail", "bwds", "kuro_s",
    "kuro_t", "Oq", "Oqw", "p_hat", "sea_hat", "nezumi", "Rdead1", "Rdead2", "Stal", "Oship",
    "Fganon", "Daiocta", "Btd", "Bst", "Bkm", "Bdk", "Dk", "big_pow", "Bwd", "Bgn", "Gnd",
};

/* cos/sin of k * 45 degrees, k = 0..7: copies are placed on the ring in this order. */
static const f32 ring_x[8] = {1.0f, 0.7071f, 0.0f, -0.7071f, -1.0f, -0.7071f, 0.0f, 0.7071f};
static const f32 ring_z[8] = {0.0f, 0.7071f, 1.0f, 0.7071f, 0.0f, -0.7071f, -1.0f, -0.7071f};

static int loaded;
static int multiplier;
static f32 spread;
static int randomize;
static u32 seed;
static u32 created_copies;

static int is_enemy(const char* name) {
    for (u32 e = 0; e != sizeof(enemies) / sizeof(enemies[0]); ++e) {
        const char* wanted = enemies[e];
        u32 i = 0;
        while (i != 8 && wanted[i] && name[i] == wanted[i]) ++i;
        if (i == 8 ? wanted[i] == 0 : (wanted[i] == 0 && name[i] == 0)) return 1;
    }
    return 0;
}

static void load_options(void) {
    loaded = 1;
    multiplier = wwhd_config_int("multiplier", 4);
    if (multiplier < 1) multiplier = 1;
    if (multiplier > 9) multiplier = 9;
    spread = (f32)wwhd_config_float("spread", 60.0);
    if (spread < 0.0f) spread = 0.0f;
    randomize = wwhd_config_bool("randomize", 0);
    wwhd_log_int("enemy-multiplier: multiplier", multiplier);
}

WWHD_REPLACE(ADDR_dStage_actorCreate, void, enemy_multiplier_create, (u32 data, u32 prm)) {
    if (!loaded) load_options();
    if (data == 0 || prm == 0 || multiplier == 1 || !is_enemy((const char*)data)) {
        actor_create_original(data, prm);
        return;
    }
    /* Copies first: the original call consumes `prm`. */
    for (int i = 1; i < multiplier; ++i) {
        u32 copy = wwhd_fopAcM_CreateAppend();
        if (copy == 0) break; /* game heap exhausted: keep what was created */
        memcpy((void*)copy, (const void*)prm, PRM_BYTES);
        f32* position = (f32*)(copy + PRM_POSITION);
        position[0] += spread * ring_x[(i - 1) & 7];
        position[2] += spread * ring_z[(i - 1) & 7];
        if (randomize) {
            seed = seed * 0x41C64E6Du + 12345u; /* same generator as the GameCube client */
            *(u32*)(copy + PRM_PARAMETERS) = 0xFFFF0000u | (seed & 0xFFFFu);
        }
        actor_create_original(data, copy);
        ++created_copies;
    }
    actor_create_original(data, prm);
    if ((created_copies & 63) == 1) wwhd_log_int("enemy-multiplier: extra enemies created", (int)created_copies);
}
