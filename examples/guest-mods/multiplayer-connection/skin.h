/* Skins: another Link model for each player who chose one (launcher.cfg "skin", ActorPlayerSync
 * skin, mp_protocol.h MP_SKIN_MAX).
 *
 * A Link model mod replaces Link.bfres in the "Link" archive (Link.szs of permanent_3d.pack,
 * checked on the Miku pack: nothing else differs). Installed as a content mod it changes every
 * Link of that machine, puppets included. Here each skin is an archive of its own, loaded next to
 * "Link" and used only by the Links of the players who chose it:
 *  - the archive: tools/multiplayer/make_skin.py writes skin_<id>.szs into this mod's Data folder
 *    (the one of launcher.cfg), with Link.bfres renamed MpSk_<id>.bfres (a model archive's bfres is
 *    "<archive>.bfres", decomp hd_res_model.cpp hdResModel_loadFiles) and the shader archives kept;
 *    the runtime gives that file to the game as Object/MpSk_<id>.szs (runtime/src/mods/mp_skins.cpp);
 *  - loading: dComIfG_setObjectRes / syncObjectRes / deleteObjectRes (025203xx, decomp
 *    d_com_inf_game.cpp: a char* name; sync 0 loaded, > 0 loading, < 0 failed), the way actors
 *    load their archives (Object/<name>.szs, decomp hd_res_mgr.cpp ResMgr_requestObject); only
 *    while the play scene loads (skin_scene_load, from phase_5): a skin chosen or met during play
 *    shows from the next area on;
 *  - its resource table: ResourceArchiveInfoHD::loadResourceTable (02604550) finds the indices of
 *    an archive by the hash of its name in the game's table (decomp d_resorce.cpp), which has no
 *    "MpSk_" row: for these archives it is given "Link", so the skin answers the indices Link's
 *    code asks for;
 *  - which Link uses it: dRes_control_c::getRes / getIDRes (026066C4 / 026067F4, every
 *    dComIfG_getObjectRes of daPy_lk_c, decomp d_a_player_main_01.cpp) are answered from the skin
 *    instead of "Link" while the code of a Link bound to a skin runs: its creation phases, execute,
 *    draw and delete (puppet.h, mod.c). A Link is bound when its creation starts (phase_1): the
 *    local Link to the skin of launcher.cfg, a puppet to its player's (the slot whose
 *    fopAcM_create id is the actor's base_process_class::mBsPcId +0x04, decomp f_pc_base.h).
 *
 * Name strings given to the game are plain SafeStrings {char* top, vtable} with the vtable of the
 * resource control's own (resorce_hd::SAFE_STRING_VTABLE 100E1994, decomp d_resorce.h), whose
 * assureTermination does nothing (0260A824).
 *
 * Not verified in game (to test): that the parent object heap holds an archive of ~23 MB (setArchive
 * puts a non-"sea"/"gy" object in mpObjectParentHeap), that nothing else of the game looks "Link"
 * up by name for a Link's model. A skin the player does not have, or one that fails to load, shows
 * Link instead (logged once). */
#pragma once
#include "wwhd_guest.h"
#include "wwhd/bindings.h"
#include "mp_protocol.h"

#define SKIN_ARCHIVES 6
#define SKIN_ACTORS 8
#define SKIN_ARC_BYTES (5 + MP_SKIN_MAX + 1)
#define SKIN_LOAD_TIMEOUT 1800   /* logic steps (30 s): then the skin counts as failed */
#define SKIN_PROC_ID 0x04        /* base_process_class::mBsPcId */
#define SKIN_NONE (-1)

enum { SKIN_IDLE = 0, SKIN_REQUESTED, SKIN_LOADED, SKIN_FAILED };

typedef struct skin_archive {
    int state;
    char id[MP_SKIN_MAX + 1];
    char arc[SKIN_ARC_BYTES]; /* "MpSk_<id>" */
    int users;                /* Links built from it, alive */
    unsigned long long wanted_step, requested_step;
    u32 key[2];               /* SafeString of arc, for getRes */
} skin_archive;

typedef struct skin_actor {
    u32 actor;
    int archive; /* SKIN_NONE: Link */
} skin_actor;

static skin_archive skin_archives[SKIN_ARCHIVES];
static skin_actor skin_actors[SKIN_ACTORS];
static int skin_current = SKIN_NONE; /* the skin of the Link whose code runs */
static char skin_local[MP_SKIN_MAX + 1]; /* launcher.cfg "skin" */
static const char skin_link_name[] = "Link";
static u32 skin_link_key[2];
static u8 skin_probe[16];

WWHD_GAME_ORIGINAL(0x026066C4, u32, skin_get_res_original, (u32 self, u32 name, u32 index));
WWHD_GAME_ORIGINAL(0x026067F4, u32, skin_get_id_res_original, (u32 self, u32 name, u32 index));
WWHD_GAME_ORIGINAL(0x02604550, void, skin_load_table_original, (u32 info, u32 name, u32 heap));

static u32 skin_string_vtable(void) { return (u32)&WWHD_GAME_DATA(0x100E1994, u8); }

static int skin_same_id(const char* a, const char* b) {
    for (int i = 0; i != MP_SKIN_MAX + 1; ++i) {
        if (a[i] != b[i]) return 0;
        if (!a[i]) return 1;
    }
    return 1;
}

static void skin_copy_id(char* out, const char* in) {
    int i = 0;
    for (; i != MP_SKIN_MAX && in[i]; ++i) out[i] = in[i];
    for (; i != MP_SKIN_MAX + 1; ++i) out[i] = 0;
}

/* The characters of a SafeString the game passes (a null top reads as none). */
static const char* skin_chars(u32 name) {
    if (!name) return 0;
    return (const char*)*(const volatile u32*)name;
}

static int skin_is_link(u32 name) {
    const char* s = skin_chars(name);
    return s && s[0] == 'L' && s[1] == 'i' && s[2] == 'n' && s[3] == 'k' && s[4] == 0;
}

static int skin_is_archive(u32 name) {
    const char* s = skin_chars(name);
    return s && s[0] == 'M' && s[1] == 'p' && s[2] == 'S' && s[3] == 'k' && s[4] == '_';
}

/* ------------------------------------------------------------------ game hooks */

static u32 skin_key_for(u32 name) {
    if (skin_current < 0 || !skin_is_link(name)) return name;
    skin_archive* a = &skin_archives[skin_current];
    if (a->state != SKIN_LOADED) return name;
    a->key[0] = (u32)a->arc;
    a->key[1] = skin_string_vtable();
    return (u32)a->key;
}

WWHD_REPLACE(0x026066C4, u32, skin_get_res, (u32 self, u32 name, u32 index)) {
    return skin_get_res_original(self, skin_key_for(name), index);
}

WWHD_REPLACE(0x026067F4, u32, skin_get_id_res, (u32 self, u32 name, u32 index)) {
    return skin_get_id_res_original(self, skin_key_for(name), index);
}

WWHD_REPLACE(0x02604550, void, skin_load_table, (u32 info, u32 name, u32 heap)) {
    if (skin_is_archive(name)) {
        skin_link_key[0] = (u32)skin_link_name;
        skin_link_key[1] = skin_string_vtable();
        name = (u32)skin_link_key;
    }
    skin_load_table_original(info, name, heap);
}

/* ------------------------------------------------------------------ archives */

static int skin_find(const char* id) {
    for (int i = 0; i != SKIN_ARCHIVES; ++i)
        if (skin_archives[i].state != SKIN_IDLE && skin_same_id(skin_archives[i].id, id)) return i;
    return -1;
}

/* skin_<id>.szs in the Data folder, starting like an archive. */
static int skin_file_present(const char* id) {
    char file[5 + MP_SKIN_MAX + 5];
    int n = 0;
    const char* head = "skin_";
    while (*head) file[n++] = *head++;
    for (int i = 0; id[i]; ++i) file[n++] = id[i];
    file[n++] = '.'; file[n++] = 's'; file[n++] = 'z'; file[n++] = 's'; file[n] = 0;
    s32 got = wwhd_file_read(file, skin_probe, sizeof(skin_probe));
    if (got < 4) return 0;
    return (skin_probe[0] == 'Y' && skin_probe[1] == 'a' && skin_probe[2] == 'z' && skin_probe[3] == '0') ||
           (skin_probe[0] == 'S' && skin_probe[1] == 'A' && skin_probe[2] == 'R' && skin_probe[3] == 'C');
}

static void skin_log(const char* what, const skin_archive* a) {
    char line[80];
    int n = 0;
    for (const char* s = "multiplayer: skin "; *s; ++s) line[n++] = *s;
    for (int i = 0; a->id[i]; ++i) line[n++] = a->id[i];
    line[n++] = ' ';
    for (; *what && n != (int)sizeof(line) - 1; ++what) line[n++] = *what;
    line[n] = 0;
    wwhd_log(line);
}

/* The loaded archive of a skin, else SKIN_NONE (Link). Never loads anything: a skin is loaded
 * only while the play scene loads (skin_scene_load), so a skin not loaded yet shows Link until
 * the next area. */
static int skin_want(const char* id) {
    if (!id || !id[0]) return SKIN_NONE;
    int i = skin_find(id);
    if (i < 0) return SKIN_NONE;
    skin_archives[i].wanted_step = wwhd_logic_step();
    return skin_archives[i].state == SKIN_LOADED ? i : SKIN_NONE;
}

/* The skins of the other players met lately, loaded at the next scene load. */
#define SKIN_SEEN 8
#define SKIN_SEEN_STEPS (30 * 60 * 10) /* forgotten after 10 minutes unseen */
static char skin_seen[SKIN_SEEN][MP_SKIN_MAX + 1];
static unsigned long long skin_seen_step[SKIN_SEEN];

static void skin_note_remote(const char* id) {
    if (!id || !id[0]) return;
    unsigned long long now = wwhd_logic_step();
    int slot = -1, oldest = 0;
    for (int i = 0; i != SKIN_SEEN; ++i) {
        if (skin_seen[i][0] && skin_same_id(skin_seen[i], id)) { slot = i; break; }
        if (skin_seen_step[i] < skin_seen_step[oldest]) oldest = i;
    }
    if (slot < 0) {
        slot = oldest;
        skin_copy_id(skin_seen[slot], id);
    }
    skin_seen_step[slot] = now;
}

static int skin_wanted_now(const skin_archive* a, unsigned long long now) {
    if (skin_same_id(a->id, skin_local) && skin_local[0]) return 1;
    for (int i = 0; i != SKIN_SEEN; ++i)
        if (skin_seen[i][0] && skin_same_id(skin_seen[i], a->id) && now - skin_seen_step[i] < SKIN_SEEN_STEPS) return 1;
    return 0;
}

static void skin_add(const char* id) {
    if (!id[0] || skin_find(id) >= 0) return;
    for (int k = 0; k != SKIN_ARCHIVES; ++k) {
        skin_archive* a = &skin_archives[k];
        if (a->state != SKIN_IDLE) continue;
        skin_copy_id(a->id, id);
        int n = 0;
        for (const char* p = "MpSk_"; *p; ++p) a->arc[n++] = *p;
        for (int j = 0; a->id[j]; ++j) a->arc[n++] = a->id[j];
        while (n != SKIN_ARC_BYTES) a->arc[n++] = 0;
        a->users = 0;
        a->requested_step = wwhd_logic_step();
        if (!skin_file_present(a->id)) {
            a->state = SKIN_FAILED;
            skin_log("is not installed (skin_<id>.szs in the mod's Data folder): Link is shown", a);
        } else if (wwhd_dComIfG_setObjectRes((u32)a->arc, 0, 0) != 1) {
            a->state = SKIN_FAILED;
            skin_log("load request refused: Link is shown", a);
        } else {
            a->state = SKIN_REQUESTED;
            skin_log("loading", a);
        }
        return;
    }
}

/* Called from the play scene's phase_5 (skin_scene_phase), once the game's own preloaded
 * resources are in and before the stage creates its actors: no Link exists and nothing of the
 * stage runs or draws (crashes of 0.2.32-0.2.34: an archive of ~23 MB loaded during play, while
 * the local Link waited in its phase_1 unregistered as player 0 and the other actors read it, or
 * while Links drew). Unloads the skins nobody wants, requests the wanted ones (the local player's
 * and those of the players met lately), and returns 1 while one still loads. */
static void skin_read_local(void); /* skin_local from launcher.cfg or the option (mod.c) */

static int skin_scene_load(void) {
    unsigned long long now = wwhd_logic_step();
    skin_read_local(); /* before the first Link: its settings are read in its steps otherwise */
    for (int i = 0; i != SKIN_ARCHIVES; ++i) {
        skin_archive* a = &skin_archives[i];
        if (a->state == SKIN_IDLE || a->state == SKIN_REQUESTED || a->users > 0 || skin_wanted_now(a, now)) continue;
        if (a->state == SKIN_LOADED) {
            wwhd_dComIfG_deleteObjectRes((u32)a->arc);
            skin_log("unloaded", a);
        }
        a->state = SKIN_IDLE; /* a failed one is tried again when wanted (installed since) */
    }
    skin_add(skin_local);
    for (int i = 0; i != SKIN_SEEN; ++i)
        if (skin_seen[i][0] && now - skin_seen_step[i] < SKIN_SEEN_STEPS) skin_add(skin_seen[i]);
    int waiting = 0;
    for (int i = 0; i != SKIN_ARCHIVES; ++i) {
        skin_archive* a = &skin_archives[i];
        if (a->state != SKIN_REQUESTED) continue;
        s32 r = wwhd_dComIfG_syncObjectRes((u32)a->arc);
        if (r == 0) {
            a->state = SKIN_LOADED;
            skin_log("loaded", a);
        } else if (r < 0) {
            a->state = SKIN_FAILED;
            skin_log("failed to load: Link is shown", a);
        } else if (now - a->requested_step > SKIN_LOAD_TIMEOUT) {
            a->state = SKIN_FAILED; /* left requested: never deleted while the game may load it */
            skin_log("takes too long to load: Link is shown", a);
        } else {
            waiting = 1;
        }
    }
    return waiting;
}

/* dScnPly phase_5 (025B2F90, decomp d_s_play_2.cpp): loads the scene's preloaded resources and
 * returns cPhs_INIT (0) until they are in; the stage and its actors are created by the next phases
 * (phase_compleate). The skins load after the game's own resources, in the same wait. */
WWHD_GAME_ORIGINAL(0x025B2F90, s32, skin_scene_phase_original, (u32 i_this));
WWHD_REPLACE(0x025B2F90, s32, skin_scene_phase, (u32 i_this)) {
    s32 result = skin_scene_phase_original(i_this);
    if (result == 0) return result;
    return skin_scene_load() ? 0 : result;
}

/* ------------------------------------------------------------------ Links */

static skin_actor* skin_binding(u32 actor) {
    for (int i = 0; i != SKIN_ACTORS; ++i)
        if (skin_actors[i].actor == actor) return &skin_actors[i];
    return 0;
}

static int skin_of(u32 actor) {
    skin_actor* b = actor ? skin_binding(actor) : 0;
    return b ? b->archive : SKIN_NONE;
}

static void skin_bind(u32 actor, int archive) {
    skin_actor* b = skin_binding(actor);
    if (!b) b = skin_binding(0);
    if (!b) return; /* more Links than expected: this one is Link */
    b->actor = actor;
    b->archive = archive;
    if (archive >= 0) ++skin_archives[archive].users;
}

static void skin_unbind(u32 actor) {
    skin_actor* b = skin_binding(actor);
    if (!b) return;
    if (b->archive >= 0 && skin_archives[b->archive].users > 0) {
        --skin_archives[b->archive].users;
        skin_archives[b->archive].wanted_step = wwhd_logic_step();
    }
    b->actor = 0;
    b->archive = SKIN_NONE;
}

static int skin_bound(u32 actor) { return skin_binding(actor) != 0; }

/* The skin's code window: what getRes answers for "Link" while a Link runs. */
static int skin_enter(u32 actor) {
    int previous = skin_current;
    skin_current = skin_of(actor);
    return previous;
}

static void skin_leave(int previous) { skin_current = previous; }

/* The local Link's creation: the skin of launcher.cfg when it is loaded (by the scene load before
 * this Link), else Link. Never waits: a Link waiting in its phase_1 is not registered as player 0
 * yet, and the other actors read player 0 (0.2.32 crash, null actor + 0x314). Returns 0. */
static int skin_local_create(u32 actor) {
    if (!skin_bound(actor)) skin_bind(actor, skin_want(skin_local));
    return 0;
}
