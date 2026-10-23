/* 0.3.81: the players' colour keys, a second lock on the boss door of a dungeon.
 *
 * Each player has keys of their own colour in each dungeon: 3 that enemies drop (an enemy that dies
 * there, at random) and 3 lying next to an object of the stage (a room entered, at random). The boss
 * door opens (with the boss key, as before) only once every player in that dungeon picked up 3 of
 * their keys. A key never goes away; only its owner can pick it up; everyone sees it in its owner's
 * colour.
 *
 * The keys live on the server (WWClient.java "colour keys", User/ColorKeys.txt), carried by the
 * mod's events (item_share.h transport, mp_event with the key's stage, room and place):
 *  - "#KA_ii_o_<owner>": key ii (hex) of <owner> (the player's name) made, origin o (E an enemy,
 *    A next to an object, D the launcher's Debug page), at the event's place;
 *  - "#KT_ii_<owner>": that key picked up.
 * The server keeps them, relays them and sends them all to a player who joins. Each game makes only
 * its own player's keys (no double drop); a key's id is its owner's next free one.
 *
 * The game (decomp, verified):
 *  - a dungeon: dStage_stagInfo_GetSTType == dStageType_DUNGEON_e (1): (stag info +0xC >> 16) & 7,
 *    the stag info from the stage data's getStagInfo (025C5040; d_a_player_main_01.cpp,
 *    d_stage_3.cpp);
 *  - the boss door: daDoor10_c with dDoor_info_c::getType (0252AA2C: (mParameters >> 8) & 15) 1,
 *    which asks for the boss key (d_a_door10.cpp setEventPrm 02127080), and daDoor12_c of type 3
 *    (d_a_door12.cpp setEventPrm 0212919C). door_lock.h stops their setEventPrm while it is locked
 *    and a player of the dungeon lacks keys (ck_boss_blocked);
 *  - the key's model: the cutscene item daDitem_c (0x101) of the small key (dItemNo_SMALL_KEY_e
 *    0x15, d_item_data.h) with FLAG_UNK01 | FLAG_UNK08 (no item given, at home.pos), shown with
 *    DRAW_VISIBLE (item_share.h, verified there), painted in its owner's colour at its draw (the
 *    elites' recolouring, elite.h el_draw_begin; 0.3.82: the owner's exact colour, ck_exact_rgb);
 *  - an enemy died: deleted (as_delete, 025D4B4C) of group fopAc_ENEMY_e (2) with health <= 0;
 *  - the objects next to which a key lies: actors made from the stage's data (fopAc_Create with the
 *    mod's AS_TAG block, actor_sync.h) of group 0 (fopAc_ACTOR_e), in the room entered.
 * The count shows as the game's small keys do, just above them (0.3.82, "drawing" below). A key
 * picked up plays the sound of an enemy's spoils. */
#pragma once
#include "wwhd_guest.h"

#define CK_MAX 192
#define CK_NEED 3                 /* keys a player needs for the boss door */
#define CK_PER_ORIGIN 3           /* keys made by enemies, keys next to objects */
#define CK_ITEM_SMALL_KEY 0x15u   /* dItemNo_SMALL_KEY_e */
#define CK_PROC_DITEM 0x101
#define CK_DITEM_FLAGS 0x09u      /* FLAG_UNK01 | FLAG_UNK08 */
#define CK_DITEM_DRAW_FLAGS 0x74F
#define CK_DITEM_VISIBLE 0x01u
#define CK_DITEM_DEAD 0x02u
#define CK_LIFT 25.0f             /* over the ground */
#define CK_PICK_RADIUS 90.0f
#define CK_PICK_HEIGHT 140.0f
#define CK_SHOW_RADIUS 4500.0f    /* a key's model made this near the local Link */
#define CK_HIDE_RADIUS 5500.0f
#define CK_MODELS_MAX 16
#define CK_SYNC_STEPS 150         /* 5 s after the connection: the server's keys came */
#define CK_ROOM_STEPS 60          /* 2 s in a room before its draw: its objects made */
#define CK_ROOM_CHANCE 35u        /* percent: a key next to an object of a room entered */
#define CK_DROP_BASE 8u           /* percent for an enemy's death, plus this per death without key */
#define CK_DROP_MAX 50u
#define CK_NOTED_MAX 192
#define CK_OFFSET 70.0f           /* from the object */
#define CK_STAG_TYPE 0x0C
#define CK_STAGE_DUNGEON 1u
#define CK_GROUP_ACTOR 0
#define CK_GROUP_ENEMY 2
#define CK_SHOW_SECONDS 0.3
#define CK_SE_SPOILS_GET 0x87Bu   /* JA_SE_SPOILS_GET: d_a_item.cpp itemGetExecute (Joy Pendant), mDoAud_seStart_id 025E1988 */

/* 0.3.90: a key on the ground (its place is the ground under where it was made, dBgS_ObjGndChk_Func
 * 024F1350: d_bg_s_func.cpp, one cXyz*, the ground's height or -1e9 when none) and, when it was just
 * dropped, a fall as the enemies' loot does: it pops up from where the enemy was, falls and bounces. */
#define CK_GROUND_LIFT 12.0f      /* over the ground once landed (0.3.81-0.3.89: 25, floating) */
#define CK_GROUND_FROM 120.0f     /* the ground looked for from this high */
#define CK_NO_GROUND -100000000.0f
#define CK_FALL_GRAVITY 2.5f      /* per step */
#define CK_FALL_POP 11.0f         /* the first jump up */
#define CK_FALL_BOUNCES 2

typedef struct ck_key {
    u8 used, id, origin, taken;
    s8 room;
    u8 bounces, grounded;
    char owner[MP_NAME_MAX + 1];
    char stage[STAGE_NAME_BYTES + 1];
    f32 pos[3];
    f32 fall, vy;  /* the model's height over pos while it falls (0.3.90) */
    u32 proc;      /* its model's process id, 0 none */
} ck_key;

static ck_key ck_keys[CK_MAX];
static u32 ck_noted[CK_NOTED_MAX]; /* process ids of the stage's objects, newest last */
static u32 ck_noted_next;
static unsigned long long ck_ready_step; /* the step the keys may be made from */
static int ck_was_ready;
static char ck_area[STAGE_NAME_BYTES + 1];
static u8 ck_rolled[64];           /* the rooms of this visit already drawn */
static s8 ck_room = -1;
static u32 ck_room_steps;
static u32 ck_deaths_without;      /* enemies dead since the last key they gave */
static cXyz ck_pos;
static csXyz ck_angle;
static s16 ck_spin;
static double ck_door_show;
static f32 ck_door_at[3];
static u32 ck_door_ready, ck_door_total;
static u32 ck_live;                /* key models made: none, no colour lookup at each actor's draw */
static int ck_was_connected;       /* 0.3.90: the connection, apart from the area's player uid */

static int ck_same(const char* a, const char* b, u32 cap) {
    for (u32 i = 0; i != cap; ++i) {
        if (a[i] != b[i]) return 0;
        if (!a[i]) return 1;
    }
    return 1;
}

static void ck_copy(char* out, const char* in, u32 cap) {
    u32 i = 0;
    for (; i + 1 < cap && in[i]; ++i) out[i] = in[i];
    for (; i != cap; ++i) out[i] = 0;
}

static int ck_active(void) { return state == READY && local_uid != MP_NO_ID && local_look.name[0]; }

/* 0.3.90: the stages of one dungeon. Its miniboss's and boss's rooms are other stages (another
 * name): keys counted by stage name were 0 there, and the keys picked up there did not count in the
 * dungeon. The game keeps a dungeon's small keys and bits by its save table, shared by its stages:
 * dStage_stagInfo_GetSaveTbl = (stag info byte +9 >> 1) & 0x7F (decomp d_stage_3.cpp
 * dStage_stagInfoInit 025C1E44, d_a_warpf.cpp; stage_stag_info_class mProp at +0x09, d_stage.h).
 * Each stage's table is learned when it is loaded and kept in a file of the mod (the keys from the
 * server name only their stage). A table is a dungeon's once a stage of type dungeon had it. */
#define CK_STAG_PROP 0x09
#define CK_TABLES_MAX 96
#define CK_TABLES_FILE "color_key_tables.bin"
#define CK_TABLES_MAGIC 0x434B5431u /* "CKT1" */
#define CK_NO_TABLE 0xFFu

typedef struct ck_table_entry {
    char stage[STAGE_NAME_BYTES + 1];
    u8 table, dungeon;
} ck_table_entry;

static struct {
    u32 magic, count;
    ck_table_entry e[CK_TABLES_MAX];
} ck_tables;
static int ck_tables_loaded;

static int ck_same(const char* a, const char* b, u32 cap);
static void ck_copy(char* out, const char* in, u32 cap);

static void ck_tables_load(void) {
    if (ck_tables_loaded) return;
    ck_tables_loaded = 1;
    if (wwhd_file_read(CK_TABLES_FILE, &ck_tables, sizeof(ck_tables)) != (s32)sizeof(ck_tables) ||
        ck_tables.magic != CK_TABLES_MAGIC || ck_tables.count > CK_TABLES_MAX) {
        ck_tables.magic = CK_TABLES_MAGIC;
        ck_tables.count = 0;
    }
}

static const ck_table_entry* ck_table_find(const char* stage) {
    ck_tables_load();
    for (u32 i = 0; i != ck_tables.count; ++i)
        if (map_same_stage(ck_tables.e[i].stage, stage)) return &ck_tables.e[i];
    return 0;
}

static int ck_table_is_dungeon(u8 table) {
    ck_tables_load();
    for (u32 i = 0; i != ck_tables.count; ++i)
        if (ck_tables.e[i].table == table && ck_tables.e[i].dungeon) return 1;
    return 0;
}

static u32 ck_stag(const u8* play) {
    return play ? wwhd_dStage_stageDt_c_getStagInfo((u32)(play + SV_PLAY_STAGE_DATA)) : 0;
}

/* The loaded stage's table and type, noted (and saved) when new. */
static void ck_table_learn(const u8* play, const char* stage) {
    u32 stag = ck_stag(play);
    if (!stag || !stage[0]) return;
    u8 table = (u8)((*(const volatile u8*)(stag + CK_STAG_PROP) >> 1) & 0x7Fu);
    u8 dungeon = ((*(const volatile u32*)(stag + CK_STAG_TYPE) >> 16) & 7u) == CK_STAGE_DUNGEON;
    ck_table_entry* e = (ck_table_entry*)ck_table_find(stage);
    if (e && e->table == table && e->dungeon == dungeon) return;
    if (!e) {
        if (ck_tables.count == CK_TABLES_MAX) return;
        e = &ck_tables.e[ck_tables.count++];
        ck_copy(e->stage, stage, sizeof(e->stage));
    }
    e->table = table;
    e->dungeon = dungeon;
    wwhd_file_write(CK_TABLES_FILE, &ck_tables, sizeof(ck_tables));
    wwhd_log("multiplayer: colour keys: a stage's save table noted:");
    wwhd_log(stage);
    wwhd_log_int("multiplayer: colour keys:   save table", table);
    wwhd_log_int("multiplayer: colour keys:   of type dungeon", dungeon);
}

/* Two stages of the same dungeon: the same stage, or the same dungeon's save table. */
static int ck_same_dungeon(const char* a, const char* b) {
    if (map_same_stage(a, b)) return 1;
    const ck_table_entry* ta = ck_table_find(a);
    const ck_table_entry* tb = ck_table_find(b);
    return ta && tb && ta->table == tb->table && ck_table_is_dungeon(ta->table);
}

/* The current stage is a dungeon (dStageType_DUNGEON_e), or a stage of one (0.3.90: its save table). */
static int ck_dungeon(const u8* play) {
    u32 stag = ck_stag(play);
    if (!stag) return 0;
    if (((*(const volatile u32*)(stag + CK_STAG_TYPE) >> 16) & 7u) == CK_STAGE_DUNGEON) return 1;
    return ck_table_is_dungeon((u8)((*(const volatile u8*)(stag + CK_STAG_PROP) >> 1) & 0x7Fu));
}

/* 0.3.90: the ground's height under (x, from, z), CK_NO_GROUND when none. */
static cXyz ck_ground_at;
static f32 ck_ground(f32 x, f32 from, f32 z) {
    ck_ground_at.x = x;
    ck_ground_at.y = from;
    ck_ground_at.z = z;
    f32 y = wwhd_dBgS_ObjGndChk_Func((u32)&ck_ground_at);
    return y > CK_NO_GROUND ? y : CK_NO_GROUND;
}

/* A new key's place: on the ground under it, else under the local Link (over the void, a wall). */
static void ck_put_on_ground(f32* pos) {
    f32 y = ck_ground(pos[0], pos[1] + CK_GROUND_FROM, pos[2]);
    const fopAc_ac_c* link = (const fopAc_ac_c*)as_link;
    if (y == CK_NO_GROUND && link) {
        wwhd_log("multiplayer: colour keys: no ground under the key: put under Link");
        pos[0] = link->current.pos.x;
        pos[2] = link->current.pos.z;
        y = ck_ground(pos[0], link->current.pos.y + CK_GROUND_FROM, pos[2]);
        if (y == CK_NO_GROUND) y = link->current.pos.y;
    }
    if (y != CK_NO_GROUND) pos[1] = y;
}

static void ck_fall_start(ck_key* k, f32 from_height) {
    k->fall = from_height > 0.0f ? from_height : 0.0f;
    k->vy = CK_FALL_POP;
    k->bounces = 0;
}

static ck_key* ck_find(const char* owner, u8 id) {
    for (u32 i = 0; i != CK_MAX; ++i)
        if (ck_keys[i].used && ck_keys[i].id == id && ck_same(ck_keys[i].owner, owner, MP_NAME_MAX + 1)) return &ck_keys[i];
    return 0;
}

static u32 ck_count(const char* owner, const char* stage, int origin, int taken_only) {
    u32 n = 0;
    for (u32 i = 0; i != CK_MAX; ++i) {
        const ck_key* k = &ck_keys[i];
        if (!k->used || !ck_same(k->owner, owner, MP_NAME_MAX + 1) || !ck_same_dungeon(k->stage, stage)) continue;
        if (origin && k->origin != origin) continue;
        if (taken_only && !k->taken) continue;
        ++n;
    }
    return n;
}

/* ---------------------------------------------------------------- colour */

static u32 ck_rgb565(u32 c) {
    u32 r = (c >> 11) & 31u, g = (c >> 5) & 63u, b = c & 31u;
    return ((r * 255u / 31u) << 16) | ((g * 255u / 63u) << 8) | (b * 255u / 31u);
}

/* The owner's tunic colour: its look when in this area, else its directory entry. */
static u32 ck_owner_rgb(const char* owner) {
    if (ck_same(owner, local_look.name, MP_NAME_MAX + 1)) return ck_rgb565(local_look.colors[0]);
    for (int i = 0; i != MAX_REMOTES; ++i)
        if (remotes[i].active && remotes[i].has_look && ck_same(remotes[i].look.name, owner, MP_NAME_MAX + 1))
            return ck_rgb565(remotes[i].look.colors[0]);
    if (directory_valid)
        for (mp_u32 i = 0; i != directory.count; ++i)
            if (ck_same(directory.entry[i].name, owner, MP_NAME_MAX + 1)) return ck_rgb565(directory.entry[i].color);
    return ck_rgb565(TUNIC_DEFAULT_PRIMARY);
}

static u8 ck_seed_of(u32 rgb) {
    u32 best = 0, best_d = ~0u;
    for (u32 k = 0; k != EC_PALETTE; ++k) {
        s32 dr = (s32)((ec_palette[k] >> 16) & 0xFF) - (s32)((rgb >> 16) & 0xFF);
        s32 dg = (s32)((ec_palette[k] >> 8) & 0xFF) - (s32)((rgb >> 8) & 0xFF);
        s32 db = (s32)(ec_palette[k] & 0xFF) - (s32)(rgb & 0xFF);
        u32 d = (u32)(dr * dr + dg * dg + db * db);
        if (d < best_d) {
            best_d = d;
            best = k;
        }
    }
    return (u8)best;
}

/* 0.3.82: the owners' exact colours for the elites' recolouring (elite.h el_target): seed 0x80 + i
 * is ck_exact[i]. A colour keeps its slot (el_set_for keeps the recoloured textures by seed). */
#define CK_EXACT_MAX 32
static u32 ck_exact[CK_EXACT_MAX];
static u32 ck_exact_count;

static int ck_exact_rgb(u8 seed, u32* rgb) {
    u32 i = (u32)seed - 0x80u;
    if (seed < 0x80 || i >= ck_exact_count) return 0;
    *rgb = ck_exact[i];
    return 1;
}

static u32 ck_exact_seed(u32 rgb) {
    for (u32 i = 0; i != ck_exact_count; ++i)
        if (ck_exact[i] == rgb) return 0x80u + i;
    if (ck_exact_count == CK_EXACT_MAX) return ck_seed_of(rgb); /* full: the nearest palette colour */
    ck_exact[ck_exact_count] = rgb;
    return 0x80u + ck_exact_count++;
}

/* elite.h el_draw_begin: a key's model, its owner's exact colour + 1 (0: not a key). */
static u32 ck_paint_seed(const void* actor) {
    if (!actor || !ck_live) return 0;
    u32 pcid = *(const volatile u32*)((const u8*)actor + AS_PC_ID);
    for (u32 i = 0; i != CK_MAX; ++i)
        if (ck_keys[i].used && ck_keys[i].proc && ck_keys[i].proc == pcid) return ck_exact_seed(ck_owner_rgb(ck_keys[i].owner)) + 1u;
    return 0;
}

/* ---------------------------------------------------------------- the server */

static void ck_send(const char* name, const ck_key* k) {
    const u8* play = wwhd_play_get();
    if (!play || state != READY || local_uid == MP_NO_ID || session.client_player_event == MP_NO_ID) return;
    mp_event e;
    e.sender = local_uid;
    for (u32 i = 0; i != MP_EVENT_STAGE_MAX + 1; ++i) e.stage[i] = 0;
    for (u32 i = 0; i != STAGE_NAME_BYTES && i != MP_EVENT_STAGE_MAX && k->stage[i]; ++i) e.stage[i] = k->stage[i];
    e.room = k->room;
    e.has_place = 1;
    e.pos[0] = k->pos[0];
    e.pos[1] = k->pos[1];
    e.pos[2] = k->pos[2];
    e.heading = 0;
    int n = 0;
    for (; name[n] && n != MP_EVENT_NAME_MAX; ++n) e.name[n] = name[n];
    e.name[n] = 0;
    mp_writer body = mp_writer_make(payload, sizeof(payload));
    mp_write_event(&body, &e);
    send_payload(session.client_player_event, &body);
}

/* "#Kx_ii_" then the rest; the name's length. */
static u32 ck_name(char* out, char kind, u8 id, char origin, const char* owner) {
    static const char hex[] = "0123456789ABCDEF";
    u32 n = 0;
    out[n++] = '#';
    out[n++] = 'K';
    out[n++] = kind;
    out[n++] = '_';
    out[n++] = hex[(id >> 4) & 0xF];
    out[n++] = hex[id & 0xF];
    out[n++] = '_';
    if (origin) {
        out[n++] = origin;
        out[n++] = '_';
    }
    for (u32 i = 0; i != MP_NAME_MAX && owner[i] && n != MP_EVENT_NAME_MAX; ++i) out[n++] = owner[i];
    out[n] = 0;
    return n;
}

static void ck_model_end(ck_key* k) {
    if (!k->proc) return;
    u32 actor = 0;
    if (wwhd_fopAcM_SearchByID(k->proc, (u32)&actor) && actor)
        *(volatile u8*)(actor + CK_DITEM_DRAW_FLAGS) |= CK_DITEM_DEAD; /* it deletes itself */
    k->proc = 0;
}

/* item_share.h share_received: "#KA_ii_o_owner" (with a place) or "#KT_ii_owner". */
static void ck_received(const mp_event* e) {
    const char* n = e->name;
    int hi = share_hex(n[4]), lo = share_hex(n[5]);
    if (n[3] != '_' || hi < 0 || lo < 0 || n[6] != '_') return;
    u8 id = (u8)(hi << 4 | lo);
    if (n[2] == 'A') {
        char origin = n[7];
        if (n[8] != '_' || !n[9] || !e->has_place) return;
        const char* owner = n + 9;
        ck_key* k = ck_find(owner, id);
        if (!k)
            for (u32 i = 0; i != CK_MAX && !k; ++i)
                if (!ck_keys[i].used) k = &ck_keys[i];
        if (!k) {
            static u32 logged;
            if (!logged++) wwhd_log("multiplayer: colour keys: the table is full");
            return;
        }
        int fresh = !k->used;
        if (!k->used) {
            k->used = 1;
            k->taken = 0;
            k->proc = 0;
            k->fall = k->vy = 0.0f;
            k->bounces = CK_FALL_BOUNCES;
            k->grounded = 0;
        }
        k->id = id;
        k->origin = (u8)origin;
        k->room = e->room;
        ck_copy(k->owner, owner, sizeof(k->owner));
        ck_copy(k->stage, e->stage, sizeof(k->stage));
        for (u32 i = 0; i != 3; ++i) k->pos[i] = e->pos[i];
        /* another player's enemy dropped it just now (not the server's list at the connection) */
        if (fresh && origin == 'E' && e->sender != 0 && ck_ready_step && wwhd_logic_step() >= ck_ready_step)
            ck_fall_start(k, 60.0f);
        return;
    }
    if (n[2] == 'T' && n[7]) {
        ck_key* k = ck_find(n + 7, id);
        if (!k || k->taken) return;
        k->taken = 1;
        ck_model_end(k);
    }
}

/* A new key of the local player, here, sent to the server. */
static void ck_make(char origin, const f32* pos, s8 room) {
    const u8* play = wwhd_play_get();
    if (!play) return;
    u32 next = 0;
    for (u32 i = 0; i != CK_MAX; ++i)
        if (ck_keys[i].used && ck_same(ck_keys[i].owner, local_look.name, MP_NAME_MAX + 1) && ck_keys[i].id >= next)
            next = ck_keys[i].id + 1u;
    if (next > 0xFF) next = 0xFF;
    ck_key* k = 0;
    for (u32 i = 0; i != CK_MAX && !k; ++i)
        if (!ck_keys[i].used) k = &ck_keys[i];
    if (!k) return;
    k->used = 1;
    k->id = (u8)next;
    k->origin = (u8)origin;
    k->taken = 0;
    k->proc = 0;
    k->room = room;
    ck_copy(k->owner, local_look.name, sizeof(k->owner));
    char stage[STAGE_NAME_BYTES + 1];
    map_stage(play, stage);
    ck_copy(k->stage, stage, sizeof(k->stage));
    for (u32 i = 0; i != 3; ++i) k->pos[i] = pos[i];
    /* 0.3.90: on the ground (enemy drops were invisible: in the floor or the air where the enemy
     * was deleted); an enemy's key pops up from where the enemy was and falls like its loot */
    f32 made_y = k->pos[1];
    ck_put_on_ground(k->pos);
    k->grounded = 1;
    k->fall = k->vy = 0.0f;
    k->bounces = CK_FALL_BOUNCES;
    if (origin == 'E') ck_fall_start(k, made_y - k->pos[1] + 40.0f);
    wwhd_log_int("multiplayer: colour keys: made this high over the ground (units)", (int)(made_y - k->pos[1]));
    char name[MP_EVENT_NAME_MAX + 1];
    ck_name(name, 'A', k->id, origin, k->owner);
    ck_send(name, k);
    wwhd_log_int(origin == 'E' ? "multiplayer: colour keys: an enemy dropped a key, id"
                 : origin == 'A' ? "multiplayer: colour keys: a key lies next to an object, id"
                                 : "multiplayer: colour keys: a key from the Debug page, id",
                 (int)k->id);
}

/* ---------------------------------------------------------------- where keys come from */

/* actor_sync.h as_create: an actor made from the stage's data (AS_TAG block). */
static void ck_note_create(void* actor, u32 prm) {
    if (!actor || !prm || ((const u8*)prm)[AS_PRM_TAG] != AS_TAG) return;
    ck_noted[ck_noted_next++ % CK_NOTED_MAX] = *(const volatile u32*)((const u8*)actor + AS_PC_ID);
}

static int ck_can_make(const u8* play) {
    return ck_active() && wwhd_logic_step() >= ck_ready_step && ck_dungeon(play) && as_link;
}

/* actor_sync.h as_delete: an actor deleted, its health before. */
static void ck_actor_deleted(const void* actor, s8 health) {
    if (!actor || ((const fopAc_ac_c*)actor)->group != CK_GROUP_ENEMY || health > 0) return;
    const u8* play = wwhd_play_get();
    if (!ck_can_make(play)) return;
    char stage[STAGE_NAME_BYTES + 1];
    map_stage(play, stage);
    if (ck_count(local_look.name, stage, 'E', 0) >= CK_PER_ORIGIN) return;
    u32 chance = CK_DROP_BASE * (ck_deaths_without + 1u);
    if (chance > CK_DROP_MAX) chance = CK_DROP_MAX;
    if (el_random() % 100u >= chance) {
        ++ck_deaths_without;
        return;
    }
    ck_deaths_without = 0;
    const fopAc_ac_c* a = (const fopAc_ac_c*)actor;
    f32 pos[3] = {a->current.pos.x, a->current.pos.y, a->current.pos.z};
    ck_make('E', pos, a->current.roomNo);
}

/* A room entered: maybe a key next to one of its objects. */
static void ck_room_draw(const u8* play, const fopAc_ac_c* link, s8 room) {
    char stage[STAGE_NAME_BYTES + 1];
    map_stage(play, stage);
    if (ck_count(local_look.name, stage, 'A', 0) >= CK_PER_ORIGIN || el_random() % 100u >= CK_ROOM_CHANCE) return;
    u32 found[CK_NOTED_MAX], n = 0;
    for (u32 i = 0; i != CK_NOTED_MAX; ++i) {
        u32 actor = 0;
        if (!ck_noted[i] || !wwhd_fopAcM_SearchByID(ck_noted[i], (u32)&actor) || !actor) continue;
        const fopAc_ac_c* a = (const fopAc_ac_c*)actor;
        if (a->group != CK_GROUP_ACTOR || a->current.roomNo != room || puppet_is((const void*)actor)) continue;
        f32 dx = a->current.pos.x - link->current.pos.x, dy = a->current.pos.y - link->current.pos.y,
            dz = a->current.pos.z - link->current.pos.z;
        if (dy > 400.0f || dy < -400.0f || dx * dx + dz * dz > 6000.0f * 6000.0f) continue;
        found[n++] = actor;
    }
    if (!n) return;
    const fopAc_ac_c* a = (const fopAc_ac_c*)found[el_random() % n];
    s16 dir = (s16)(el_random() & 0xFFFF);
    f32 pos[3] = {a->current.pos.x + CK_OFFSET * as_sin(dir), a->current.pos.y,
                  a->current.pos.z + CK_OFFSET * as_sin((s16)(dir + 0x4000))};
    ck_make('A', pos, room);
}

/* The launcher's Debug page: a key of the local player in front of Link. */
static void ck_debug_spawn(void) {
    const fopAc_ac_c* link = (const fopAc_ac_c*)as_link;
    if (!link || !ck_active()) {
        wwhd_log("multiplayer: colour keys: no key without a connection");
        return;
    }
    s16 y = link->shape_angle.y;
    f32 pos[3] = {link->current.pos.x + 150.0f * as_sin(y), link->current.pos.y,
                  link->current.pos.z + 150.0f * as_sin((s16)(y + 0x4000))};
    ck_make('D', pos, link->current.roomNo);
}

/* ---------------------------------------------------------------- the boss door */

/* door_lock.h: 1 while a player of this dungeon lacks keys (the door offers nothing). */
static int ck_boss_blocked(const fopAc_ac_c* door) {
    const u8* play = wwhd_play_get();
    const fopAc_ac_c* link = (const fopAc_ac_c*)as_link;
    if (!ck_active() || !play || !link || !ck_dungeon(play)) return 0;
    char stage[STAGE_NAME_BYTES + 1];
    map_stage(play, stage);
    u32 total = 1, ready = ck_count(local_look.name, stage, 0, 1) >= CK_NEED;
    if (directory_valid)
        for (mp_u32 i = 0; i != directory.count; ++i) {
            const mp_dir_entry* e = &directory.entry[i];
            if (e->id == directory.self || !ck_same_dungeon(e->stage, stage)) continue;
            ++total;
            ready += ck_count(e->name, stage, 0, 1) >= CK_NEED;
        }
    f32 door_pos[3] = {door->current.pos.x, door->current.pos.y, door->current.pos.z};
    if (dl_someone_near(door_pos, DL_SHOW_RADIUS)) { /* 0.3.83: whoever waits there, all see it */
        ck_door_show = CK_SHOW_SECONDS;
        ck_door_ready = ready;
        ck_door_total = total;
        ck_door_at[0] = door->current.pos.x;
        ck_door_at[1] = door->current.pos.y;
        ck_door_at[2] = door->current.pos.z;
    }
    if (ready >= total) return 0;
    static u32 logged;
    if (logged++ < 3) wwhd_log("multiplayer: colour keys: the boss door waits for every player's 3 keys");
    return 1;
}

/* ---------------------------------------------------------------- every step */

static void ck_models_step(const u8* play, const fopAc_ac_c* link) {
    char stage[STAGE_NAME_BYTES + 1];
    map_stage(play, stage);
    u32 models = 0;
    ck_spin = (s16)(ck_spin + 0x200);
    for (u32 i = 0; i != CK_MAX; ++i) {
        ck_key* k = &ck_keys[i];
        if (!k->used) continue;
        int here = !k->taken && map_same_stage(k->stage, stage);
        f32 dx = k->pos[0] - link->current.pos.x, dy = k->pos[1] - link->current.pos.y, dz = k->pos[2] - link->current.pos.z;
        f32 d2 = dx * dx + dz * dz;
        if (!here || d2 > CK_HIDE_RADIUS * CK_HIDE_RADIUS) {
            ck_model_end(k);
            continue;
        }
        if (!k->grounded && d2 <= CK_SHOW_RADIUS * CK_SHOW_RADIUS && (wwhd_logic_step() + i) % 15u == 0) {
            /* 0.3.90: a key made before (server's list, older mod): put on the ground here once its
             * room's ground is there, if near its place (not a floor far under it) */
            f32 y = ck_ground(k->pos[0], k->pos[1] + CK_GROUND_FROM, k->pos[2]);
            if (y != CK_NO_GROUND && y > k->pos[1] - 400.0f) {
                k->pos[1] = y;
                k->grounded = 1;
                dy = k->pos[1] - link->current.pos.y;
            }
        }
        if (k->vy != 0.0f || k->fall > 0.0f) { /* falling: gravity, a bounce or two (0.3.90) */
            k->vy -= CK_FALL_GRAVITY;
            k->fall += k->vy;
            if (k->fall <= 0.0f) {
                k->fall = 0.0f;
                if (k->vy < -4.0f && k->bounces < CK_FALL_BOUNCES) {
                    k->vy = -k->vy * 0.35f;
                    ++k->bounces;
                } else {
                    k->vy = 0.0f;
                }
            }
        }
        int mine = ck_same(k->owner, local_look.name, MP_NAME_MAX + 1);
        if (mine && k->fall < 30.0f && d2 <= CK_PICK_RADIUS * CK_PICK_RADIUS && dy < CK_PICK_HEIGHT && dy > -CK_PICK_HEIGHT && !dh_ghost &&
            !dh_dying && !play[WWHD_PLAY_EVENT_RUNNING_OFFSET]) { /* picked up */
            k->taken = 1;
            ck_model_end(k);
            char name[MP_EVENT_NAME_MAX + 1];
            ck_name(name, 'T', k->id, 0, k->owner);
            ck_send(name, k);
            wwhd_seStart_025E1988(CK_SE_SPOILS_GET); /* the sound of an enemy's spoils picked up */
            wwhd_log_int("multiplayer: colour keys: a key picked up, now", (int)ck_count(k->owner, stage, 0, 1));
            continue;
        }
        if (k->proc) {
            u32 actor = 0;
            if (!wwhd_fopAcM_SearchByID(k->proc, (u32)&actor)) {
                k->proc = 0; /* gone (its room unloaded): made again */
                continue;
            }
            ++models;
            if (!actor) continue; /* being made */
            fopAc_ac_c* model = (fopAc_ac_c*)actor;
            model->home.pos.x = k->pos[0];
            model->home.pos.y = k->pos[1] + (k->grounded ? CK_GROUND_LIFT : CK_LIFT) + k->fall;
            model->home.pos.z = k->pos[2];
            model->shape_angle.y = ck_spin;
            model->current.angle.y = ck_spin;
            *(volatile u8*)(actor + CK_DITEM_DRAW_FLAGS) |= CK_DITEM_VISIBLE;
            continue;
        }
        if (d2 > CK_SHOW_RADIUS * CK_SHOW_RADIUS || models >= CK_MODELS_MAX) continue;
        ck_pos.x = k->pos[0];
        ck_pos.y = k->pos[1] + (k->grounded ? CK_GROUND_LIFT : CK_LIFT) + k->fall;
        ck_pos.z = k->pos[2];
        ck_angle.x = ck_angle.z = 0;
        ck_angle.y = ck_spin;
        u8 saved = as_context;
        as_context = AS_CTX_NONE; /* this game's own: not sent to the others (actor_sync.h) */
        u32 proc = wwhd_fopAcM_create(CK_PROC_DITEM, CK_ITEM_SMALL_KEY | 0x7F00u | (CK_DITEM_FLAGS << 16), (u32)&ck_pos,
                                      k->room, (u32)&ck_angle, 0, 0xFFFFFFFFu, 0);
        as_context = saved;
        if (proc == 0xFFFFFFFFu) continue;
        k->proc = proc;
        ++models;
    }
    ck_live = models;
}

/* mod.c: every step of the local Link. */
static void ck_step(void* link_actor, double dt) {
    if (ck_door_show > 0) ck_door_show -= dt;
    const u8* play = wwhd_play_get();
    const fopAc_ac_c* link = (const fopAc_ac_c*)link_actor;
    /* 0.3.90: the keys are forgotten only when the connection ends. Before, this followed ck_active,
     * whose player uid (mod.c local_uid) is cleared at each change of stage (enter_area ->
     * clear_area_state) until the server makes the new area's player: every key was forgotten when
     * leaving the dungeon or entering the miniboss's room, and the server sends them only when the
     * game connects ("the keys are not saved"). */
    int connected = state == READY;
    if (connected && !ck_was_connected) ck_ready_step = wwhd_logic_step() + CK_SYNC_STEPS;
    if (!connected && ck_was_connected) /* disconnected: the server's keys come again at the next connection */
        for (u32 i = 0; i != CK_MAX; ++i) {
            ck_model_end(&ck_keys[i]);
            ck_keys[i].used = 0;
        }
    ck_was_connected = connected;
    int ready = ck_active();
    ck_was_ready = ready;
    if (!play || !link) return;
    char stage[STAGE_NAME_BYTES + 1];
    map_stage(play, stage);
    ck_table_learn(play, stage); /* this stage's dungeon (0.3.90), with or without a connection */
    if (!ready) return;
    if (!ck_same(stage, ck_area, sizeof(ck_area))) { /* another stage: its models are gone */
        ck_copy(ck_area, stage, sizeof(ck_area));
        for (u32 i = 0; i != CK_MAX; ++i) ck_keys[i].proc = 0;
        for (u32 i = 0; i != 64; ++i) ck_rolled[i] = 0;
        for (u32 i = 0; i != CK_NOTED_MAX; ++i) ck_noted[i] = 0;
        ck_room = -1;
    }
    if (play[WWHD_PLAY_EVENT_RUNNING_OFFSET]) return;
    s8 room = link->current.roomNo;
    if (room != ck_room) {
        ck_room = room;
        ck_room_steps = 0;
    } else if (ck_room_steps <= CK_ROOM_STEPS) {
        ++ck_room_steps;
    }
    if (ck_room_steps == CK_ROOM_STEPS && room >= 0 && room < 64 && !ck_rolled[room] && ck_can_make(play)) {
        ck_rolled[room] = 1;
        ck_room_draw(play, link, room);
    }
    ck_models_step(play, link);
}

/* ---------------------------------------------------------------- drawing */

/* 0.3.82: the count as the game shows its small keys, just above them. The game's block is the part
 * L_DungeonKey_00 of Main_00 (read offline in the game's layout: N_All_00 with the pictures
 * P_DungeonKeySh_00 (shadow) and P_DungeonKey_00 (the grey key, 38 x 40) and the text box
 * T_NumKey_00 at (55, 0); the part at (546, -281), scale 0.6, the rupees' part under it).
 *  - The number: our own, in the game's digits. 0.3.82 drew the game's text box a second time with
 *    our text, but the game draws its lists later from the same glyph buffer: both showed the same
 *    number. Now the digits 0-9 are read from the text box's font (TextBox +0xB4, runtime
 *    button_glyphs.cpp) with the game's Font::GetGlyph (font vtable +4, slot +0x8C, (font, record,
 *    code): runtime story_notebook_game.cpp), each glyph's cell cut from its sheet (record: +4 left,
 *    +5 width, +6 advance, +7 height, +8/+0xA the sheet's logical size, +0xC x, +0xE cell y from the
 *    bottom, +0x13 slice, +0x14 the sheet whose GX2Texture is at +0x10 and +0x0E 1 for a coverage
 *    sheet: runtime game_glyph.cpp read_glyph), decoded with wwhd_hud_game_pixels, and drawn by
 *    this mod: two digits as the game writes its count, in the game's yellow, with a dark edge.
 *  - The key: the game's own picture (its texture decoded, wwhd_hud_game_pixels, as bonus.h does for
 *    the hearts), its grey turned to the player's colour, and its shadow, drawn at the game's key
 *    moved up by CK_HUD_LIFT. */
#define CK_HUD_LIFT 36.0f         /* layout units (1 = 1 TV pixel), up (0.3.87: 44 a little high) */
#define CK_TB_FONT 0xB4
#define CK_FONT_GET_GLYPH 0x8C
#define CK_KEY_TEXTURE_MAX (128u * 128u * 4u)
#define CK_SHEET_MAX (1024u * 1024u * 4u)
#define CK_DIGIT_RGB 0xFFD94Au     /* the game's counters' yellow (screenshot), when its text box gives none */
#define CK_DIGIT_EDGE 0x000000u    /* the border part of the glyphs (the material's black colour, not read) */
#define CK_TB_MATERIAL 0xF8       /* the text box's material (DrawSelf draws nothing without it) */
#define CK_DIGIT_TOP 0xFFE65Au     /* the game's counter, sampled on a capture: top, bottom, border */
#define CK_DIGIT_BOTTOM 0xF5B21Eu
#define CK_TB_TOP_COLOR 0xAC       /* lyt::TextBox mTextColors[2] (RGBA8, top then bottom), between the */
#define CK_TB_BOTTOM_COLOR 0xB0    /* text buffer +0xA4 and the font +0xB4 (deduced, logged) */
#define CK_DIGIT_HEIGHT 1.0f       /* the glyph cell over the text box's drawn height (0.3.84: 0.62 was 60 % of the game's) */

static u32 ck_part, ck_numkey, ck_key_pic, ck_key_shadow, ck_part_parts, ck_part_root;
static unsigned long long ck_parts_step;
static int ck_hud_count = -1;     /* the count to show, -1 none (not in a dungeon, offline) */
static unsigned long long ck_digit_loaded_epoch = ~0ull;
static u32 ck_key_image, ck_shadow_image, ck_key_image_rgb;
static unsigned long long ck_key_epoch = ~0ull;
static u32 ck_digit[10], ck_digit_w[10], ck_digit_adv[10], ck_digit_h;
static s32 ck_digit_left[10];
static int ck_digit_colour;        /* an RGBA picture sheet: drawn as it is */
static unsigned long long ck_digit_epoch = ~0ull;
static int ck_digit_failed;
static u8 ck_glyph_record[0x18] __attribute__((aligned(8)));

/* bonus.h bonus_parts_draw: the HUD screen that has the small keys' part. */
static void ck_parts_draw(u32 self) {
    if (!ck_active()) return;
    u32 root = bonus_root_of(self);
    if (!root) return;
    if (!bonus_ptr(ck_part) || !bonus_name_is(ck_part, "L_DungeonKey_00") || bonus_root_of(ck_part_parts) != ck_part_root) {
        u32 part = bonus_find(root, "L_DungeonKey_00", 4);
        if (!part) return;
        u32 text = bonus_find(part, "T_NumKey_00", 3), key = bonus_find(part, "P_DungeonKey_00", 3),
            shadow = bonus_find(part, "P_DungeonKeySh_00", 3);
        if (!text || !key) return;
        ck_part = part;
        ck_numkey = text;
        ck_key_pic = key;
        ck_key_shadow = shadow;
        ck_part_parts = self;
        ck_part_root = root;
        ck_key_epoch = ~0ull; /* the pictures again */
        ck_digit_epoch = ~0ull;
        wwhd_log("multiplayer: colour keys: the HUD's small keys found");
    }
    if (self == ck_part_parts) ck_parts_step = wwhd_logic_step();
}

static void ck_digits_free(void) {
    for (u32 i = 0; i != 10; ++i) {
        if (ck_digit[i]) wwhd_hud_release(ck_digit[i]);
        ck_digit[i] = 0;
    }
}

/* The digits of the game's counter font, as HUD images. */
static void ck_digits_load(void) {
    u32 font = *(const volatile u32*)(ck_numkey + CK_TB_FONT);
    u32 table = bonus_ptr(font) ? *(const volatile u32*)(font + 4) : 0;
    u32 get_glyph = bonus_ptr(table) ? *(const volatile u32*)(table + CK_FONT_GET_GLYPH) : 0;
    if (!get_glyph) {
        wwhd_log("multiplayer: colour keys: the counter's font is not readable");
        ck_digit_failed = 1;
        return;
    }
    u32 sheet_seen = 0, sw = 0, sh = 0;
    u8* sheet_px = 0;
    int format = -1;
    /* 0.3.86: the text's own colours (top, bottom) */
    u32 top = *(const volatile u32*)(ck_numkey + CK_TB_TOP_COLOR), bottom = *(const volatile u32*)(ck_numkey + CK_TB_BOTTOM_COLOR);
    wwhd_log_hex("multiplayer: colour keys: the counter's top colour (RGBA)", top);
    wwhd_log_hex("multiplayer: colour keys: the counter's bottom colour (RGBA)", bottom);
    if (!(top & 0xFFu) || !(bottom & 0xFFu)) top = bottom = 0xFFFFFFFFu;
    /* 0.3.87: these are white; the yellow is the material's (its colours tint white text). Its words
     * are logged to find them; until then a yellow sampled on a capture of the game's counter. */
    u32 material = *(const volatile u32*)(ck_numkey + CK_TB_MATERIAL);
    if (bonus_ptr(material)) {
        static int dumped;
        if (!dumped++)
            for (u32 i = 0; i != 0x60; i += 4)
                wwhd_log_hex("multiplayer: colour keys: the counter's material word", *(const volatile u32*)(material + i));
    }
    /* 0.3.88: the material's black (+0x00) and white (+0x04) colours, RGBA (journal 0.3.87: 0A0200 and
     * FFD817, then its name T_NumKey_00 at +0x18): the glyph's value mixes them, the border's middle
     * values giving the brown edge. */
    u32 black = CK_DIGIT_EDGE, white = CK_DIGIT_TOP;
    if (bonus_ptr(material)) {
        u32 b = *(const volatile u32*)material, w = *(const volatile u32*)(material + 4);
        if ((w & 0xFFu) == 0xFFu) {
            black = b >> 8;
            white = w >> 8;
        }
    }
    wwhd_log_hex("multiplayer: colour keys: the counter's colours, black", black);
    wwhd_log_hex("multiplayer: colour keys: the counter's colours, white", white);
    u32 hist_mid = 0, hist_full = 0;
    for (u32 d = 0; d != 10 && !ck_digit_failed; ++d) {
        for (u32 i = 0; i != sizeof(ck_glyph_record); ++i) ck_glyph_record[i] = 0;
        ((void (*)(u32, u32, u32))get_glyph)(font, (u32)ck_glyph_record, '0' + d);
        const u8* r = ck_glyph_record;
        u32 sheet = (u32)r[0x14] << 24 | (u32)r[0x15] << 16 | (u32)r[0x16] << 8 | r[0x17];
        u32 gw = r[5], gh = r[7], lw = (u32)r[8] << 8 | r[9], lh = (u32)r[10] << 8 | r[11];
        u32 x0 = (u32)r[12] << 8 | r[13], cell_y = (u32)r[14] << 8 | r[15];
        if (!bonus_ptr(sheet) || !gw || !gh || r[0x13]) {
            wwhd_log_int("multiplayer: colour keys: a digit's glyph not readable, slice", r[0x13]);
            ck_digit_failed = 1;
            break;
        }
        if (sheet != sheet_seen) {
            if (sheet_px) wwhd_free(sheet_px);
            sheet_px = 0;
            u32 dims = wwhd_hud_game_pixels(sheet + 0x10, 0, 0);
            sw = dims >> 16;
            sh = dims & 0xFFFFu;
            if (!sw || !sh || sw * sh * 4u > CK_SHEET_MAX || lw != sw || lh != sh) {
                wwhd_log_int("multiplayer: colour keys: the font's sheet not readable, width", (int)sw);
                wwhd_log_int("multiplayer: colour keys:   its logical width", (int)lw);
                ck_digit_failed = 1;
                break;
            }
            sheet_px = (u8*)wwhd_malloc(sw * sh * 4u);
            if (!sheet_px || !wwhd_hud_game_pixels(sheet + 0x10, sheet_px, sw * sh * 4u)) {
                wwhd_log("multiplayer: colour keys: the font's sheet not decoded");
                ck_digit_failed = 1;
                break;
            }
            sheet_seen = sheet;
            ck_digit_colour = *(const volatile u8*)(sheet + 0x0E) == 0;
            format = (int)(*(const volatile u32*)(sheet + 0x10 + 0x14) & 0x3Fu);
            wwhd_log_int("multiplayer: colour keys: the counter's font, sheet format", format);
            wwhd_log_int("multiplayer: colour keys:   picture sheet (1) or coverage (0)", ck_digit_colour);
        }
        if (x0 + gw > sw || cell_y + gh > sh) {
            ck_digit_failed = 1;
            break;
        }
        u8* px = (u8*)wwhd_malloc(gw * gh * 4u);
        if (!px) {
            ck_digit_failed = 1;
            break;
        }
        u32 y0 = sh - cell_y - gh; /* the cell's y counts from the sheet's bottom (game_glyph.cpp) */
        for (u32 y = 0; y != gh; ++y)
            for (u32 x = 0; x != gw; ++x) {
                const u8* s = sheet_px + ((y0 + y) * sw + x0 + x) * 4u;
                u8* o = px + ((gh - 1u - y) * gw + x) * 4u;
                if (ck_digit_colour) {
                    o[0] = s[0];
                    o[1] = s[1];
                    o[2] = s[2];
                    o[3] = s[3];
                } else {
                    /* 0.3.86: a bordered coverage font, as the game draws it: the glyph's value v gives
                     * the opacity (2v) and the mix from the border to the text's colour (2v - 1); the
                     * text's colour goes from its top colour to its bottom one down the cell. */
                    u32 v = format == 1 ? s[0] : format == 7 ? s[1] : s[3];
                    if (d == 0) {
                        if (v >= 192u) ++hist_full;
                        else if (v >= 64u) ++hist_mid;
                    }
                    u32 a = v * 2u > 255u ? 255u : v * 2u;
                    u32 mix = v * v / 255u; /* 0.3.89: the border browner and wider, as on the game's counters */
                    for (u32 c = 0; c != 3; ++c) { /* black -> white by the glyph's value (0.3.88) */
                        u32 sh_c = 16u - 8u * c;
                        u32 lo = (black >> sh_c) & 0xFFu, hi = (white >> sh_c) & 0xFFu;
                        u32 text_c = ((((top & bottom) >> (sh_c + 8u)) & 0xFFu)); /* the text's colour (white here) */
                        o[c] = (u8)((lo * (255u - mix) + hi * mix) / 255u * text_c / 255u);
                    }
                    o[3] = (u8)a;
                }
            }
        ck_digit[d] = wwhd_hud_image(px, gw, gh);
        wwhd_free(px);
        ck_digit_w[d] = gw;
        ck_digit_adv[d] = r[6];
        ck_digit_left[d] = (s8)r[4];
        ck_digit_h = gh;
        if (!ck_digit[d]) ck_digit_failed = 1;
    }
    if (sheet_px) wwhd_free(sheet_px);
    wwhd_log_int("multiplayer: colour keys: the digit 0's border texels (values 64-191)", (int)hist_mid);
    wwhd_log_int("multiplayer: colour keys: the digit 0's fill texels (192-255)", (int)hist_full);
    if (ck_digit_failed) ck_digits_free();
    else wwhd_log_int("multiplayer: colour keys: the game's digits read, height", (int)ck_digit_h);
}

/* The game's key picture, its grey in the player's colour (or as it is: the shadow). */
static u32 ck_key_picture(u32 picture, u32 rgb, int shadow) {
    u32 texture = bonus_texture_of(picture);
    if (!texture) return 0;
    u32 dims = wwhd_hud_game_pixels(texture, 0, 0);
    u32 w = dims >> 16, h = dims & 0xFFFFu;
    if (!w || !h || w * h * 4u > CK_KEY_TEXTURE_MAX) return 0;
    u8* px = (u8*)wwhd_malloc(w * h * 4u);
    if (!px) return 0;
    u32 image = 0;
    if (wwhd_hud_game_pixels(texture, px, w * h * 4u)) {
        if (!shadow) {
            u32 cr = (rgb >> 16) & 0xFF, cg = (rgb >> 8) & 0xFF, cb = rgb & 0xFF;
            for (u32 i = 0; i != w * h * 4u; i += 4) {
                u32 r = px[i], g = px[i + 1], b = px[i + 2];
                u32 hi = r > g ? (r > b ? r : b) : (g > b ? g : b), lo = r < g ? (r < b ? r : b) : (g < b ? g : b);
                if (hi && (hi - lo) * 100u > hi * 25u) continue; /* a coloured texel: kept */
                /* a grey: the player's colour at that grey's brightness (white stays a light tint) */
                u32 t = (r + g + b) / 3u;
                u32 k = t * 320u / 255u; /* mid grey -> the colour itself, light grey -> lighter */
                u32 nr = cr * k / 255u, ng = cg * k / 255u, nb = cb * k / 255u;
                if (k > 255u) {
                    u32 lift = k - 255u;
                    nr = cr + (255u - cr) * lift / 65u;
                    ng = cg + (255u - cg) * lift / 65u;
                    nb = cb + (255u - cb) * lift / 65u;
                }
                px[i] = (u8)(nr > 255u ? 255u : nr);
                px[i + 1] = (u8)(ng > 255u ? 255u : ng);
                px[i + 2] = (u8)(nb > 255u ? 255u : nb);
            }
        }
        image = wwhd_hud_image(px, w, h);
    }
    wwhd_free(px);
    return image;
}

static wwhd_hud_element ck_element;

static void ck_emit(u32 list, u32 image, u32 pane, u32 rgb, f32 alpha) {
    if (!image || !bonus_ptr(pane)) return;
    f32 x, y, w, h;
    bonus_rect(pane, 0, &x, &y, &w, &h);
    y -= CK_HUD_LIFT;
    if (!(x > -4096.0f && x < 4096.0f && y > -4096.0f && y < 4096.0f && w > 0.0f && w < 1024.0f && h > 0.0f && h < 1024.0f))
        return;
    ck_element = (wwhd_hud_element){.kind = WWHD_HUD_IMAGE, .anchor = WWHD_HUD_TOP_LEFT, .blend = WWHD_HUD_ALPHA,
                                    .x = x, .y = y, .w = w, .h = h, .thickness = 1, .u1 = 1, .v1 = 1,
                                    .rgba = (rgb << 8) | (u32)(alpha * 255.0f), .image = image};
    wwhd_hud_emit(list, &ck_element);
}

/* mod.c draw_markers: the key above the game's small keys, the boss door's count. */
static void ck_draw(u32 list) {
    const u8* play = wwhd_play_get();
    ck_hud_count = -1;
    if (!ck_active() || !play) return;
    if (!ck_dungeon(play)) return;
    char stage[STAGE_NAME_BYTES + 1];
    map_stage(play, stage);
    ck_hud_count = (int)ck_count(local_look.name, stage, 0, 1);
    if (play[WWHD_PLAY_EVENT_RUNNING_OFFSET]) return;
    if (ck_door_show > 0) {
        f32 x, y;
        if (dh_project(ck_door_at, DL_COUNT_LIFT + 50.0f, &x, &y)) {
            static char text[48];
            const char* label = "Colour keys ";
            u32 n = 0;
            for (; label[n]; ++n) text[n] = label[n];
            n += hv_number(text + n, ck_door_ready);
            text[n++] = '/';
            n += hv_number(text + n, ck_door_total);
            name_draw(list, x, y, text, n, ck_door_ready >= ck_door_total ? 0x6FE08Au : 0xF9C74Fu, 0.9f, 1.0f);
        }
    }
    if (!bonus_ptr(ck_key_pic) || wwhd_logic_step() - ck_parts_step > 2) return; /* the game's keys not drawn */
    f32 alpha = bonus_alpha(ck_key_pic);
    if (!(alpha > 0.0f) || alpha > 1.0f) return;
    u32 rgb = ck_owner_rgb(local_look.name);
    unsigned long long epoch = wwhd_hud_epoch();
    if (epoch != ck_key_epoch || rgb != ck_key_image_rgb) {
        if (ck_key_image && epoch == ck_key_epoch) wwhd_hud_release(ck_key_image);
        if (ck_shadow_image && epoch == ck_key_epoch) wwhd_hud_release(ck_shadow_image);
        ck_key_epoch = epoch;
        ck_key_image_rgb = rgb;
        ck_key_image = ck_key_picture(ck_key_pic, rgb, 0);
        ck_shadow_image = ck_key_shadow ? ck_key_picture(ck_key_shadow, 0, 1) : 0;
        if (!ck_key_image) wwhd_log("multiplayer: colour keys: the game's key picture could not be read");
    }
    if (epoch != ck_digit_epoch) {
        if (ck_digit_loaded_epoch == epoch) ck_digits_free(); /* handles of this epoch: released */
        for (u32 i = 0; i != 10; ++i) ck_digit[i] = 0;        /* of an older one: already gone */
        ck_digit_loaded_epoch = epoch;
        ck_digit_epoch = epoch;
        ck_digit_failed = 0;
        ck_digits_load();
    }
    ck_emit(list, ck_shadow_image, ck_key_shadow, 0x000000u, bonus_alpha(ck_key_shadow));
    ck_emit(list, ck_key_image, ck_key_pic, 0xFFFFFFu, alpha);
    /* the count: two digits, as the game writes its own, at the game's number moved up */
    f32 tx, ty, tw, th;
    bonus_rect(ck_numkey, 0, &tx, &ty, &tw, &th);
    ty -= CK_HUD_LIFT;
    u32 n = (u32)ck_hud_count > 99u ? 99u : (u32)ck_hud_count;
    u32 digits[2] = {n / 10u, n % 10u};
    if (ck_digit_failed || !ck_digit_h || !ck_digit[digits[0]] || !ck_digit[digits[1]]) {
        static char text[4];
        text[0] = (char)('0' + digits[0]);
        text[1] = (char)('0' + digits[1]);
        name_draw(list, tx + tw * 0.5f, ty + th * 0.75f, text, 2, CK_DIGIT_RGB, th / 40.0f, alpha); /* fallback */
        return;
    }
    f32 k = th * CK_DIGIT_HEIGHT / (f32)ck_digit_h; /* glyph pixels -> TV pixels */
    f32 width = 0.0f;
    for (u32 i = 0; i != 2; ++i) width += (f32)ck_digit_adv[digits[i]] * k;
    f32 pen = tx + (tw - width) * 0.5f, top = ty + (th - (f32)ck_digit_h * k) * 0.5f;
    for (u32 i = 0; i != 2; ++i) {
        u32 d = digits[i];
        f32 gx = pen + (f32)ck_digit_left[d] * k, gw = (f32)ck_digit_w[d] * k, gh = (f32)ck_digit_h * k;
        ck_element = (wwhd_hud_element){.kind = WWHD_HUD_IMAGE, .anchor = WWHD_HUD_TOP_LEFT, .blend = WWHD_HUD_ALPHA,
                                        .x = gx, .y = top, .w = gw, .h = gh, .thickness = 1, .u1 = 1, .v1 = 1,
                                        .rgba = 0xFFFFFF00u | (u32)(alpha * 255.0f), /* the colours are in the image (0.3.86) */
                                        .image = ck_digit[d]};
        wwhd_hud_emit(list, &ck_element);
        pen += (f32)ck_digit_adv[d] * k;
    }
}
