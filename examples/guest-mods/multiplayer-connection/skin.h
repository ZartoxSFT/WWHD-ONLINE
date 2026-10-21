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

#define SKIN_ARCHIVES 16 /* 0.3.52: 6; the skins' heap (254 MiB) is the real bound */
#define SKIN_ACTORS 20   /* the local Link and every puppet (PUPPET_MAX) */
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

/* "Link" resources that Link shares with another actor's model, always taken from "Link": the
 * bomb model (dRes_INDEX_LINK_BDL_BOMB_e 0x3C, d_a_bomb.h) and its brk (0x51). daBomb_c::createHeap
 * builds the bomb from "Link" 0x3C outside any Link's code, and daBomb_c::draw_norm (020C4398)
 * plays the local Link's brk (+0x44D4) on it; Link binds that brk to its own 0x3C in phase_2
 * (d_a_player_main_01.cpp initBrkAnm). Taken from a skin, it was bound to the skin's bomb and the
 * game's bomb drew unbound: crash in J3DModel::viewCalc (027F55FC) reading null + 0x3C as soon as a
 * bomb showed (an elite's bombs, crash-20261018-*.log). */
/* The same for every model of that loop of Link's creation (d_a_player_main_01.cpp, anmLines: the
 * model, then the animations Link binds to it): 0x3C bomb (brk 0x51), 0x42 (brk 0x55, btk 0x63),
 * 0x37 (btk 0x68), 0x38 (btk 0x65), 0x40 (btk 0x61), 0x41 (btk 0x62). 0x42 is the bomb's water
 * column, GWP00 (dRes_INDEX_LINK_BDL_GWP00_e, d_wpillar.cpp: dWpillar_c built from "Link" 0x42 for
 * a bomb exploding over water): the same crash at 15:49 and 15:53 in dWpillar_c::draw (025D24B0)
 * after an elite's explosion (crash-20261018-1549*.log, -155303.log). */
static int skin_shared_res(u32 index) {
    static const u8 shared[] = {0x3C, 0x51, 0x42, 0x55, 0x63, 0x37, 0x68, 0x38, 0x65, 0x40, 0x61, 0x41, 0x62};
    for (u32 i = 0; i != sizeof(shared); ++i)
        if (index == shared[i]) return 1;
    return 0;
}

WWHD_REPLACE(0x026066C4, u32, skin_get_res, (u32 self, u32 name, u32 index)) {
    if (skin_shared_res(index)) return skin_get_res_original(self, name, index);
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
#define SKIN_SEEN 16
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

/* 0.3.19: the skins of the players met lately that this machine does not have (players.txt
 * "skins_missing=", ids comma-separated): the launcher downloads them from the server (it keeps the
 * skin each player sends), and they load at the next area. Checked every SKIN_MISSING_STEPS. */
#define SKIN_MISSING_STEPS 150
static char skin_missing_text[SKIN_SEEN * (MP_SKIN_MAX + 1) + 1];
static unsigned long long skin_missing_step;
static const char* skin_missing(void) {
    unsigned long long now = wwhd_logic_step();
    if (skin_missing_step && now - skin_missing_step < SKIN_MISSING_STEPS) return skin_missing_text;
    skin_missing_step = now ? now : 1;
    int n = 0;
    for (int i = 0; i != SKIN_SEEN; ++i) {
        if (!skin_seen[i][0] || now - skin_seen_step[i] >= SKIN_SEEN_STEPS || skin_file_present(skin_seen[i])) continue;
        if (n) skin_missing_text[n++] = ',';
        for (int k = 0; skin_seen[i][k]; ++k) skin_missing_text[n++] = skin_seen[i][k];
    }
    skin_missing_text[n] = 0;
    return skin_missing_text;
}

static int skin_wanted_now(const skin_archive* a, unsigned long long now) {
    if (skin_same_id(a->id, skin_local) && skin_local[0]) return 1;
    for (int i = 0; i != SKIN_SEEN; ++i)
        if (skin_seen[i][0] && skin_same_id(skin_seen[i], a->id) && now - skin_seen_step[i] < SKIN_SEEN_STEPS) return 1;
    return 0;
}

/* 0.3.24: a skin loads only when the object heaps keep room for the game. Crashes (dist 14:09-14:54,
 * instance 3 16:52: Omasao/sea, a skin just loaded, "an object archive heap is missing", then
 * ResMgr_loadArchive 02612E64 writing through a failed allocation): a skin (~25 MB, about 30 MB
 * unpacked) lives in ResourceControlHD::mpObjectParentHeap (+0x2040), where the game also cuts its
 * 15 MiB object heap at sea (createObjectHeaps 026079B4) and loads its objects (with the enemy
 * multiplier, many). The heap's free size is its own virtual (vtable at +0xC, entry +0x74: the call
 * selectObjectArchiveHeap 02607B68 makes, decomp d_resorce.cpp ResourceHeapVirtualsHD). A skin is
 * requested only when the free size keeps SKIN_RESERVE for the game (and the sea's 15 MiB while not
 * made yet); else that player shows as Link, tried again at the next area. */
#define SKIN_LOAD_BYTES (32u << 20)    /* what a skin takes once loaded (estimate, logged) */
#define SKIN_RESERVE (12u << 20)       /* kept free for the area's own objects */
#define SKIN_SEA_HEAPS (16u << 20)     /* createObjectHeaps' 15 MiB, not made yet */
#define SKIN_CONTROL 0x101F4F28        /* dComIfG_resControl() */
#define SKIN_PARENT_HEAP 0x2040        /* ResourceControlHD::mpObjectParentHeap */
#define SKIN_HEAP_FREE_SLOT 0x74       /* its vtable's getFreeSize */
static u32 skin_planned;               /* bytes asked for by skins requested in this scene load */

static u32 skin_heap_free(u32 heap) {
    u32 vtable = heap ? *(const volatile u32*)(heap + 0xC) : 0;
    u32 fn = vtable ? *(const volatile u32*)(vtable + SKIN_HEAP_FREE_SLOT) : 0;
    if (!fn) return 0;
    return ((u32(*)(u32))fn)(heap);
}

#define SKIN_HEAP_LARGEST_SLOT 0x7C     /* getMaxAllocatableSize(alignment) (heap_grow.h hg_max_alloc) */
static u32 skin_heap_largest(u32 heap) {
    u32 vtable = heap ? *(const volatile u32*)(heap + 0xC) : 0;
    u32 fn = vtable ? *(const volatile u32*)(vtable + SKIN_HEAP_LARGEST_SLOT) : 0;
    if (!fn) return 0;
    return ((u32(*)(u32, s32))fn)(heap, 4);
}

/* 1 when a skin may load now; logs the numbers. */
#define SKIN_HEAP_NEED (26u << 20)  /* what a skin takes in the skins' heap when its size is not known */
#define SKIN_MOUNT_BYTES (3u << 20) /* its mounting (shader objects, info): ~2 MB measured */
static u32 hg_skin_heap_get(void);  /* heap_grow.h: the skins' own heap, 0 without */
static u32 hg_skin_heap_room(void);
static void hg_log_heaps(void);
static int skin_room(const skin_archive* a) {
    if (hg_skin_heap_get()) { /* 0.3.39: the skins' own heap, its largest free block */
        u32 largest = hg_skin_heap_room();
        wwhd_log_int("multiplayer: skin memory: skins' heap room (KiB)", (int)(largest >> 10));
        /* a skin takes ~23 MB once loaded (22.8 MB unpacked: bfres 14 MB, shaders 8.8 MB); the ones
         * requested in this scene load are not in the heap yet */
        /* 0.3.48: the skin's own size, from the header skin_file_present just read (Yaz0: the
         * unpacked size at +4; SARC: the file size at +8), and the mounting (~2 MB, log 0.3.42) */
        u32 need = SKIN_HEAP_NEED;
        if (skin_probe[0] == 'Y' || skin_probe[0] == 'S') {
            u32 at = skin_probe[0] == 'Y' ? 4u : 8u;
            u32 size = (u32)skin_probe[at] << 24 | (u32)skin_probe[at + 1] << 16 | (u32)skin_probe[at + 2] << 8 | skin_probe[at + 3];
            if (size > (1u << 20) && size < (64u << 20)) need = size + SKIN_MOUNT_BYTES;
        }
        wwhd_log_int("multiplayer:   needed with this skin (KiB)", (int)(need >> 10));
        if (largest >= skin_planned + need) {
            skin_planned += need;
            return 1;
        }
        skin_log("left out: the skins' heap is full (Link is shown, tried again at the next area)", a);
        return 0;
    }
    u32 control = WWHD_GAME_DATA(SKIN_CONTROL, u32);
    u32 heap = control ? *(const volatile u32*)(control + SKIN_PARENT_HEAP) : 0;
    if (!heap) return 1; /* not known: the game's own checks */
    u32 free_bytes = skin_heap_free(heap);
    /* 0.3.40: an archive loads into the largest free block (hd_res_load_task.cpp startLoad), not
     * the total: crash 0.3.39 (instance 3, 21:33) after a second skin, 84 MiB free in pieces */
    u32 largest = skin_heap_largest(heap);
    wwhd_log_int("multiplayer: skin memory: object heap largest free block (KiB)", (int)(largest >> 10));
    if (largest < free_bytes) free_bytes = largest;
    const u8* play = wwhd_play_get();
    const char* stage = play ? (const char*)play + WWHD_PLAY_START_STAGE_NAME_OFFSET : "";
    int sea = stage[0] == 's' && stage[1] == 'e' && stage[2] == 'a' && stage[3] == 0;
    u32 need = skin_planned + SKIN_LOAD_BYTES + SKIN_RESERVE +
               (sea && !*(const volatile u32*)(control + 0x2048) ? SKIN_SEA_HEAPS : 0u);
    wwhd_log_int("multiplayer: skin memory: object heap free (KiB)", (int)(free_bytes >> 10));
    wwhd_log_int("multiplayer:   needed with this skin (KiB)", (int)(need >> 10));
    if (free_bytes >= need) {
        skin_planned += SKIN_LOAD_BYTES;
        return 1;
    }
    skin_log("left out: not enough memory in this area (Link is shown, tried again at the next area)", a);
    return 0;
}

static void skin_add(const char* id) {
    if (!id[0] || skin_find(id) >= 0) return;
    int idle = 0;
    for (int k = 0; k != SKIN_ARCHIVES; ++k) idle |= skin_archives[k].state == SKIN_IDLE;
    if (!idle) {
        wwhd_log("multiplayer: skin not loaded, no free skin slot:");
        wwhd_log(id);
        return;
    }
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
        } else if (!skin_room(a)) {
            a->state = SKIN_FAILED; /* tried again at the next area (skin_scene_load) */
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

/* 0.3.18: the object heaps of the Great Sea. Crashes (dist 14:09, 14:19, 14:29, instance 3 14:54,
 * always at sea after a skin loaded): resorce_hd::ResourceControlHD::selectObjectArchiveHeap
 * (02607B68, decomp d_resorce.cpp; the sea's far islands "LOD..") reads the vtable of each of its 22
 * archive heaps (+0x2078) with no null check. createObjectHeaps (026079B4) makes them, on first need,
 * inside a 15 MiB heap (mpObjectHeap +0x2048) cut from mpObjectParentHeap (+0x2040), where the skins
 * (about 25 MB each) are: with skins in, that heap could not be made, the pointers stayed null.
 * At sea, before the skins load, the object heaps are made first (the skins kept from the last
 * area, unused here, are unloaded before: they load again after, or show Link when no room is
 * left). Other stages keep the game's own order (no separate heaps there). */
#define SKIN_RES_CONTROL 0x101F4F28   /* dComIfG_resControl() */
#define SKIN_OBJECT_HEAP 0x2048        /* ResourceControlHD::mpObjectHeap */
#define SKIN_OBJECT_ARCHIVE_HEAPS 0x2078
WWHD_GAME_FUNC(0x026079B4, void, skin_create_object_heaps, (u32 control));
static __attribute__((unused)) void skin_sea_heaps(void) {
    const u8* play = wwhd_play_get();
    u32 control = WWHD_GAME_DATA(SKIN_RES_CONTROL, u32);
    if (!play || !control) return;
    const char* stage = (const char*)play + WWHD_PLAY_START_STAGE_NAME_OFFSET;
    if (!(stage[0] == 's' && stage[1] == 'e' && stage[2] == 'a' && stage[3] == 0)) return;
    if (*(const volatile u32*)(control + SKIN_OBJECT_HEAP)) return; /* made already */
    for (int i = 0; i != SKIN_ARCHIVES; ++i) {
        skin_archive* a = &skin_archives[i];
        if (a->state != SKIN_LOADED || a->users > 0) continue;
        wwhd_dComIfG_deleteObjectRes((u32)a->arc);
        a->state = SKIN_IDLE;
        skin_log("unloaded to make room for the sea's islands", a);
    }
    skin_create_object_heaps(control);
    wwhd_log(*(const volatile u32*)(control + SKIN_OBJECT_HEAP) ? "multiplayer: the sea's object heaps made before the skins"
                                                               : "multiplayer: the sea's object heaps could not be made");
}

/* The guard of selectObjectArchiveHeap: a heap pointer left null (createObjectHeaps found no room)
 * is never read; then the archive goes to mpObjectHeap as when all 22 are full (null: the parent's). */
WWHD_GAME_ORIGINAL(0x02607B68, u32, skin_archive_heap_original, (u32 control, u32 name));
WWHD_REPLACE(0x02607B68, u32, skin_archive_heap, (u32 control, u32 name)) {
    int missing = !control || !*(const volatile u32*)(control + SKIN_OBJECT_HEAP);
    for (u32 i = 0; !missing && i != 22; ++i) missing = !*(const volatile u32*)(control + SKIN_OBJECT_ARCHIVE_HEAPS + i * 4);
    if (!missing) return skin_archive_heap_original(control, name);
    static u32 logged;
    if (logged++ < 4) wwhd_log("multiplayer: an object archive heap is missing (no room): the archive goes to the object heap");
    u32 heap = control ? *(const volatile u32*)(control + SKIN_OBJECT_HEAP) : 0;
    return heap ? heap : (control ? *(const volatile u32*)(control + 0x2040) : 0);
}

static int skin_scene_load(void) {
    unsigned long long now = wwhd_logic_step();
    skin_read_local(); /* before the first Link: its settings are read in its steps otherwise */
    /* 0.3.36: what the scene load sees, once per load (no skin line at all was logged in a session
     * where the local skin and the others' were installed); a slot in no known state is freed */
    static unsigned long long logged_step;
    if (now - logged_step > 30) {
        logged_step = now;
        hg_log_heaps(); /* heap_grow.h */
        wwhd_log("multiplayer: skins at this scene load, local skin:");
        wwhd_log(skin_local[0] ? skin_local : "(none)");
        for (int i = 0; i != SKIN_ARCHIVES; ++i) {
            skin_archive* a = &skin_archives[i];
            if (a->state > SKIN_FAILED) {
                wwhd_log_int("multiplayer:   skin slot in an unknown state, freed", (int)a->state);
                a->state = SKIN_IDLE;
                a->users = 0;
            }
            if (a->state != SKIN_IDLE) {
                wwhd_log_int("multiplayer:   skin slot state (1 loading, 2 loaded, 3 failed)", (int)a->state);
                wwhd_log(a->id);
            }
        }
    }
    int loading = 0; /* the memory a skin still loading will take is not in the heap's free size yet */
    for (int i = 0; i != SKIN_ARCHIVES; ++i) loading |= skin_archives[i].state == SKIN_REQUESTED;
    if (!loading) skin_planned = 0;
    /* 0.3.20: skin_sea_heaps is no longer called. Made here (phase_5) the sea's object heaps could not
     * be made even with no skin loaded (log "could not be made", 15:57 and 16:16), and the sea's
     * weather then wrote through a failed allocation (setup_cloud_geometry, guest address 0). The
     * game makes them on first need again; selectObjectArchiveHeap keeps its null guard. */
    for (int i = 0; i != SKIN_ARCHIVES; ++i) {
        skin_archive* a = &skin_archives[i];
        /* 0.3.21: a skin that failed for want of its file is tried again once the file is there
         * (fetched by the launcher from the server), even while still wanted */
        if (a->state == SKIN_FAILED && skin_file_present(a->id)) {
            a->state = SKIN_IDLE;
            skin_log("is installed now: loading it", a);
            continue;
        }
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

/* 0.3.74: a skinned Link's casual clothes texture object: its playerInit reads getRes("Link", 0x71)
 * with the skin's archive in place of "Link" (skin_key_for), so it is that archive's 0x71 HD object
 * (+0x20), 0 when the skin has none. */
static u32 skin_casual_object(u32 actor) {
    int i = skin_of(actor);
    if (i < 0) return 0;
    skin_archive* a = &skin_archives[i];
    if (a->state != SKIN_LOADED) return 0;
    u32 control = WWHD_GAME_DATA(0x101F4F28, u32);
    if (!control) return 0;
    a->key[0] = (u32)a->arc;
    a->key[1] = skin_string_vtable();
    u32 res = skin_get_res_original(control, (u32)a->key, 0x71);
    return res ? *(const volatile u32*)(res + 0x20) : 0;
}

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

/* 0.3.48: the shader archives of a skin are Link's. A skin archive held a copy of every shader
 * archive of Link.szs (~20 <model>.sharcfb, 8.8 MB of the 22.8 MB; tools/multiplayer/make_skin.py
 * now leaves out the ones byte for byte the game's). hdResModel_loadFiles (decomp hd_res_model.cpp)
 * reads "<archive>.bfres", then "<model>.sharcfb" for each model, from the same archive through
 * ResMgr_getFile2 (026124F4: an unused first argument, sResourceLoader's object, then the archive
 * resource, the file's SafeString, the size out; generated code: r4-r6 moved on to ResMgr_getFile).
 * Replaced: when a ".sharcfb" is missing from the archive whose "MpSk_..." bfres was just read, it
 * is read from the archive "Link" (ResMgr_getArchive 026123FC: the manager, the name; its first
 * argument is that same manager object, permanent_3d.pack keeps "Link" loaded). */
WWHD_GAME_FUNC(0x026123FC, u32, skin_res_archive, (u32 manager, u32 name));
WWHD_GAME_ORIGINAL(0x026124F4, u32, skin_get_file_original, (u32 manager, u32 res, u32 file, u32 size_out));
static u32 skin_bfres_res;                /* the archive whose MpSk_ bfres was read last */
static u32 skin_shared_key[2];

static int skin_ends_with(const char* text, const char* tail) {
    int n = 0, m = 0;
    while (text[n]) ++n;
    while (tail[m]) ++m;
    if (n < m) return 0;
    for (int i = 0; i != m; ++i)
        if (text[n - m + i] != tail[i]) return 0;
    return 1;
}

WWHD_REPLACE(0x026124F4, u32, skin_get_file, (u32 manager, u32 res, u32 file, u32 size_out)) {
    u32 data = skin_get_file_original(manager, res, file, size_out);
    const char* name = file ? (const char*)*(const volatile u32*)file : 0;
    if (!name) return data;
    if (name[0] == 'M' && name[1] == 'p' && name[2] == 'S' && name[3] == 'k' && name[4] == '_' &&
        skin_ends_with(name, ".bfres")) {
        skin_bfres_res = res;
        return data;
    }
    if (data || res != skin_bfres_res || !skin_ends_with(name, ".sharcfb")) return data;
    skin_shared_key[0] = (u32)skin_link_name;
    skin_shared_key[1] = skin_string_vtable();
    u32 link = skin_res_archive(manager, (u32)skin_shared_key);
    u32 shared = link ? skin_get_file_original(manager, link, file, size_out) : 0;
    static u32 logged;
    if (logged++ < 6) {
        wwhd_log(shared ? "multiplayer: skin shader taken from Link:" : "multiplayer: skin shader missing, also in Link:");
        wwhd_log(name);
    }
    return shared;
}
