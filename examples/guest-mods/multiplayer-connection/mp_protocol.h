/* Historical Wind Waker multiplayer protocol, freestanding C.
 *
 * Byte-compatible with WindWakerClient (mmb++ DataOutputStream, ActorSync, ActorBaseSync,
 * ActorPlayerSync) and the Java server components. No libc, no game headers: it builds for
 * the PowerPC guest and natively for tests (runtime/tools/mp_protocol_test.c).
 *
 * Wire rules (WindWakerClient/src/mmb++/stream/DataOutputStream.cpp):
 *  - scalars big-endian;
 *  - VLQ: 7-bit groups most significant first, the LAST byte carries 0x80;
 *  - strings NUL-terminated;
 *  - frame: VLQ(packet id) + VLQ(payload length) + payload. */
#pragma once

typedef unsigned char mp_u8;
typedef unsigned short mp_u16;
typedef unsigned int mp_u32;
typedef short mp_s16;
typedef char mp_check_u16[sizeof(mp_u16) == 2 ? 1 : -1];
typedef char mp_check_u32[sizeof(mp_u32) == 4 ? 1 : -1];
typedef char mp_check_f32[sizeof(float) == 4 ? 1 : -1];

#define MP_NO_ID 0xFFFFFFFFu
#define MP_MAX_COMPONENTS 8
#define MP_MAX_RESOURCE_DEPTH 32

/* ------------------------------------------------------------------ reader / writer */

typedef struct mp_reader {
    const mp_u8* data;
    mp_u32 size;
    mp_u32 pos;
    int ok; /* sticky: any failed read clears it */
} mp_reader;

static inline mp_reader mp_reader_make(const mp_u8* data, mp_u32 size) {
    mp_reader r;
    r.data = data;
    r.size = size;
    r.pos = 0;
    r.ok = 1;
    return r;
}

static inline mp_u8 mp_read_u8(mp_reader* r) {
    if (!r->ok || r->pos >= r->size) {
        r->ok = 0;
        return 0;
    }
    return r->data[r->pos++];
}

static inline mp_u16 mp_read_u16(mp_reader* r) {
    mp_u16 hi = mp_read_u8(r);
    return (mp_u16)((hi << 8) | mp_read_u8(r));
}

static inline mp_u32 mp_read_u32(mp_reader* r) {
    mp_u32 v = 0;
    for (int i = 0; i != 4; ++i) v = (v << 8) | mp_read_u8(r);
    return v;
}

static inline float mp_read_f32(mp_reader* r) {
    union { mp_u32 u; float f; } v;
    v.u = mp_read_u32(r);
    return v.f;
}

/* At most 5 groups so a value always fits 32 bits (negative s16 values sent through
 * writeIntVLQ are sign-extended to u32 and take 5 bytes). */
static inline mp_u32 mp_read_vlq(mp_reader* r) {
    mp_u32 v = 0;
    for (int i = 0; i != 5; ++i) {
        mp_u8 next = mp_read_u8(r);
        if (!r->ok) return 0;
        if (v > 0x01FFFFFFu) break;
        v = (v << 7) | (next & 0x7Fu);
        if (next & 0x80u) return v;
    }
    r->ok = 0;
    return 0;
}

static inline void mp_skip(mp_reader* r, mp_u32 bytes) {
    if (!r->ok || bytes > r->size - r->pos) {
        r->ok = 0;
        return;
    }
    r->pos += bytes;
}

/* Returns the string start (inside the buffer) and its length without the NUL. */
static inline const char* mp_read_string(mp_reader* r, mp_u32* length) {
    mp_u32 start = r->pos;
    while (r->ok && r->pos < r->size) {
        if (r->data[r->pos++] == 0) {
            *length = r->pos - start - 1;
            return (const char*)(r->data + start);
        }
    }
    r->ok = 0;
    *length = 0;
    return "";
}

static inline int mp_string_equals(const char* s, mp_u32 length, const char* wanted) {
    mp_u32 i = 0;
    for (; i != length; ++i)
        if (wanted[i] == 0 || wanted[i] != s[i]) return 0;
    return wanted[i] == 0;
}

typedef struct mp_writer {
    mp_u8* data;
    mp_u32 capacity;
    mp_u32 size;
    int ok; /* sticky: overflow clears it */
} mp_writer;

static inline mp_writer mp_writer_make(mp_u8* data, mp_u32 capacity) {
    mp_writer w;
    w.data = data;
    w.capacity = capacity;
    w.size = 0;
    w.ok = 1;
    return w;
}

static inline void mp_write_u8(mp_writer* w, mp_u8 v) {
    if (!w->ok || w->size >= w->capacity) {
        w->ok = 0;
        return;
    }
    w->data[w->size++] = v;
}

static inline void mp_write_u16(mp_writer* w, mp_u16 v) {
    mp_write_u8(w, (mp_u8)(v >> 8));
    mp_write_u8(w, (mp_u8)v);
}

static inline void mp_write_u32(mp_writer* w, mp_u32 v) {
    for (int shift = 24; shift >= 0; shift -= 8) mp_write_u8(w, (mp_u8)(v >> shift));
}

static inline void mp_write_f32(mp_writer* w, float f) {
    union { mp_u32 u; float f; } v;
    v.f = f;
    mp_write_u32(w, v.u);
}

/* Same output as DataOutputStream::writeIntVLQ, including 0 -> 0x80. */
static inline void mp_write_vlq(mp_writer* w, mp_u32 v) {
    mp_u8 groups[5];
    int count = 0;
    do {
        groups[count++] = (mp_u8)(v & 0x7Fu);
        v >>= 7;
    } while (v);
    while (count) {
        --count;
        mp_write_u8(w, (mp_u8)(groups[count] | (count ? 0 : 0x80u)));
    }
}

static inline void mp_write_string(mp_writer* w, const char* s) {
    while (*s) mp_write_u8(w, (mp_u8)*s++);
    mp_write_u8(w, 0);
}

/* Wraps payload[0..length) as one frame into w. */
static inline void mp_write_frame(mp_writer* w, mp_u32 id, const mp_u8* payload, mp_u32 length) {
    mp_write_vlq(w, id);
    mp_write_vlq(w, length);
    for (mp_u32 i = 0; i != length; ++i) mp_write_u8(w, payload[i]);
}

/* ------------------------------------------------------------------ initialization */

/* Component kinds the guest can encode/decode. ActorSync::create aborts on an unknown
 * component name, so an unsupported one in PlayerPuppet is an init failure here too. */
enum {
    MP_COMPONENT_UNSUPPORTED = 0,
    MP_COMPONENT_BASE = 1,     /* ActorBaseSync: 3 f32 position, 3 s16 shape angle */
    MP_COMPONENT_PLAYER = 2,   /* ActorPlayerSync */
    MP_COMPONENT_HEALTH = 3,   /* ActorHealthSync: s16 health, u32 actor_status */
    MP_COMPONENT_CHUCHU = 4,   /* ChuChuSync: s16 state, u8 clipping */
    MP_COMPONENT_BOKOBLIN = 5, /* BokoblinSync: 6 state bytes */
    MP_COMPONENT_PROPS = 6,    /* PropsSync: u8 state, u8 thrown, u8 grounded, s16 model angle */
    MP_COMPONENT_SPAWN = 7,    /* ActorSpawnSync (WWHD): how to create the actor, see mp_spawn */
};

/* Synchronised actors other than the players (ActorSync factories and ActorFilter).
 *
 * Every factory of the ActorSync initializer is kept when its name can be an actor name (at most
 * MP_ACTOR_NAME_MAX characters, the stage name table's char[8]; ActorSync::getFactory falls back
 * to "Default" otherwise). A factory is usable when all its components have a fixed size: the
 * guest then writes and reads it whole, even the components it does not apply (ChuChuSync,
 * BokoblinSync, PropsSync carry GameCube field offsets, see their WindWakerClient sources: the
 * guest sends zeros and ignores what it receives, which keeps the stream aligned).
 *
 * ActorFilter (ActorFilter.java, from resources/WWServer/actorList.ktml) lists the actor names to
 * synchronise. Protocol extension (WWHD), backward compatible: an entry
 * MP_MULTIPLIER_ENTRY + decimal number carries the server's enemy multiplier, and
 * MP_RANDOMIZER_ENTRY + 0 or 1 whether the copies take random variants (actor_sync.h). The
 * GameCube client only adds them to its name set, where no actor name matches them. */
#define MP_SAVE_MAP_MAX 256 /* SaveServer map runs (the WWSave of the server: 120) */
#define MP_MAX_FACTORIES 64
#define MP_ACTOR_NAME_MAX 8
#define MP_MAX_FILTER 512
#define MP_GAME_UID_MAX 64
#define MP_MULTIPLIER_ENTRY "#enemy_multiplier="
#define MP_MULTIPLIER_MAX 15
#define MP_RANDOMIZER_ENTRY "#enemy_randomizer="
/* WWHD extensions, the same way (ActorFilter.java, elite.h): "1" when the enemies' health and
 * damage follow the number of players in their area; the percentage (0..100) of the enemies that
 * are elites. */
#define MP_SCALING_ENTRY "#dynamic_scaling="
#define MP_ELITE_ENTRY "#elite_chance="
#define MP_ELITE_CHANCE_MAX 100
/* WWHD 0.3.13, the same way: "1" when the players fight each other (pvp.h, a server mutator). */
#define MP_PVP_ENTRY "#pvp="
/* WWHD 0.3.92, the same way: "1" when the players share one life (life_share.h, a server mutator). */
#define MP_SHARED_LIFE_ENTRY "#shared_life="

typedef struct mp_factory {
    char name[MP_ACTOR_NAME_MAX + 1];
    mp_u8 count;
    mp_u8 usable;       /* every component has a fixed size */
    mp_u8 base_index;   /* first ActorBaseSync, 0xFF when none */
    mp_u8 health_index; /* first ActorHealthSync, 0xFF when none */
    mp_u8 kinds[MP_MAX_COMPONENTS];
} mp_factory;

typedef struct mp_session {
    /* server -> client (index in the first init list) */
    mp_u32 server_create, server_update, server_remove;
    mp_u32 server_mutators; /* WWHD extension (ActorFilter.java): the mutators changed */
    mp_u32 server_player_directory; /* WWHD extension: every player, whatever their area */
    mp_u32 client_player_directory; /* WWHD extension: this player's entry of it */
    mp_u32 server_player_summon;    /* WWHD extension: a creature another player bought for us */
    mp_u32 client_player_summon;    /* WWHD extension: a creature bought for another player */
    mp_u32 server_save_update;      /* SaveServer: values of the shared save another player changed */
    mp_u32 server_save_from;        /* WWHD extension (SaveServer): who changed them */
    mp_u32 server_world;            /* WWHD extension (ActorFilter): the world clock and weather */
    mp_u32 client_save_update;      /* SaveServer: values this player's save changed */
    mp_u32 client_save_merge;       /* WWHD extension (SaveServer): all of them, once connected */
    mp_u32 client_world;            /* WWHD extension (ActorFilter): a sailing player's clock and weather */
    mp_u32 client_player_revive;    /* WWHD extension (WWClient): revive a dead player of the directory */
    mp_u32 server_player_revive;    /* WWHD extension: another player revived us */
    mp_u32 client_player_carry;     /* WWHD extension (WWClient): a player carried, thrown, put down */
    mp_u32 server_player_carry;     /* WWHD extension: the same, from another player */
    mp_u32 client_player_event;     /* WWHD extension (WWClient): a cutscene started here */
    mp_u32 server_player_event;     /* WWHD extension: a cutscene another player started */
    mp_u32 client_player_pay;       /* WWHD extension (WWClient): rupees paid to a player of the directory */
    mp_u32 server_player_pay;       /* WWHD extension: rupees another player paid us */
    /* the SaveServer initializer's map (SaveServer.writeMap): runs of {type, length in bytes} */
    mp_u32 save_map_count;
    mp_u8 save_map_type[MP_SAVE_MAP_MAX];
    mp_u8 save_map_length[MP_SAVE_MAP_MAX];
    /* client -> server (index in the second init list) */
    mp_u32 client_init, client_change_manager, client_leave_manager;
    mp_u32 client_create, client_update, client_remove;
    mp_u32 client_exclusive_add, client_exclusive_remove; /* SyncedActorClient, VLQ uid */
    /* "PlayerPuppet" factory from the ActorSync initializer, in component index order */
    mp_u32 puppet_component_count;
    mp_u8 puppet_components[MP_MAX_COMPONENTS];
    mp_u32 puppet_base_index;
    int has_actor_sync;
    /* the other factories; factories_complete = 0 when one could not be kept (table full) */
    mp_u32 factory_count;
    int factories_complete;
    mp_factory factories[MP_MAX_FACTORIES];
    /* ActorFilter names (at most MP_ACTOR_NAME_MAX characters, not NUL-terminated at 8) */
    mp_u32 filter_count;
    char filter[MP_MAX_FILTER][MP_ACTOR_NAME_MAX];
    mp_u32 enemy_multiplier; /* 1 when the server sends none */
    int enemy_randomizer;    /* 0 when the server sends none */
    int dynamic_scaling;     /* 0 when the server sends none */
    mp_u32 elite_chance;     /* percent, 0 when the server sends none */
    int pvp;                 /* the players fight each other; 0 when the server sends none */
    int shared_life;         /* 0.3.92: one life for all the players; 0 when the server sends none */
} mp_session;

static inline void mp_assign_id(const char* s, mp_u32 n, mp_u32 index, const char* name, mp_u32* id) {
    if (mp_string_equals(s, n, name)) *id = index;
}

static inline mp_u32 mp_component_size(mp_u8 kind) {
    if (kind == MP_COMPONENT_BASE) return 18;
    if (kind == MP_COMPONENT_HEALTH) return 6;
    if (kind == MP_COMPONENT_CHUCHU) return 3;
    if (kind == MP_COMPONENT_BOKOBLIN) return 6;
    if (kind == MP_COMPONENT_PROPS) return 5;
    if (kind == MP_COMPONENT_SPAWN) return 38;
    return 0; /* variable (ActorPlayerSync) or unknown */
}

static inline mp_u8 mp_component_kind(const char* name, mp_u32 n) {
    if (mp_string_equals(name, n, "ActorBaseSync")) return MP_COMPONENT_BASE;
    if (mp_string_equals(name, n, "ActorPlayerSync")) return MP_COMPONENT_PLAYER;
    if (mp_string_equals(name, n, "ActorHealthSync")) return MP_COMPONENT_HEALTH;
    if (mp_string_equals(name, n, "ChuChuSync")) return MP_COMPONENT_CHUCHU;
    if (mp_string_equals(name, n, "BokoblinSync")) return MP_COMPONENT_BOKOBLIN;
    if (mp_string_equals(name, n, "PropsSync")) return MP_COMPONENT_PROPS;
    if (mp_string_equals(name, n, "ActorSpawnSync")) return MP_COMPONENT_SPAWN;
    return MP_COMPONENT_UNSUPPORTED;
}

/* a (up to MP_ACTOR_NAME_MAX characters, NUL-terminated when shorter) equals the C string b */
static inline int mp_name_equals(const char* a, const char* b) {
    for (mp_u32 i = 0; i != MP_ACTOR_NAME_MAX; ++i) {
        if (a[i] != b[i]) return 0;
        if (!a[i]) return 1;
    }
    return b[MP_ACTOR_NAME_MAX] == 0;
}

/* ActorSync::getFactory: the factory named like the actor, else "Default"; MP_NO_ID when none. */
static inline mp_u32 mp_find_factory(const mp_session* s, const char* name) {
    mp_u32 fallback = MP_NO_ID;
    for (mp_u32 i = 0; i != s->factory_count; ++i) {
        if (mp_name_equals(s->factories[i].name, name)) return i;
        if (mp_name_equals(s->factories[i].name, "Default")) fallback = i;
    }
    return fallback;
}

static inline int mp_filter_contains(const mp_session* s, const char* name) {
    for (mp_u32 i = 0; i != s->filter_count; ++i)
        if (mp_name_equals(s->filter[i], name)) return 1;
    return 0;
}

/* ActorSync::readSyncTable: VLQ factories, each = name + VLQ components + names. */
static inline int mp_read_actor_sync(mp_reader* r, mp_session* s) {
    mp_u32 factories = mp_read_vlq(r);
    for (mp_u32 f = 0; r->ok && f != factories; ++f) {
        mp_u32 n;
        const char* factory = mp_read_string(r, &n);
        int puppet = mp_string_equals(factory, n, "PlayerPuppet");
        mp_u32 count = mp_read_vlq(r);
        mp_factory* kept = 0;
        if (puppet) {
            if (count > MP_MAX_COMPONENTS) return 0;
            s->puppet_component_count = count;
            s->puppet_base_index = MP_NO_ID;
        } else if (r->ok && n <= MP_ACTOR_NAME_MAX) {
            if (s->factory_count == MP_MAX_FACTORIES || count > MP_MAX_COMPONENTS) {
                s->factories_complete = 0;
            } else {
                kept = &s->factories[s->factory_count++];
                for (mp_u32 i = 0; i != MP_ACTOR_NAME_MAX + 1; ++i) kept->name[i] = i < n ? factory[i] : 0;
                kept->count = (mp_u8)count;
                kept->usable = 1;
                kept->base_index = kept->health_index = 0xFF;
            }
        }
        for (mp_u32 c = 0; r->ok && c != count; ++c) {
            const char* component = mp_read_string(r, &n);
            if (!r->ok) break;
            mp_u8 kind = mp_component_kind(component, n);
            if (kept) {
                kept->kinds[c] = kind;
                if (!mp_component_size(kind)) kept->usable = 0;
                if (kind == MP_COMPONENT_BASE && kept->base_index == 0xFF) kept->base_index = (mp_u8)c;
                if (kind == MP_COMPONENT_HEALTH && kept->health_index == 0xFF) kept->health_index = (mp_u8)c;
            }
            if (!puppet) continue;
            if (kind != MP_COMPONENT_BASE && kind != MP_COMPONENT_PLAYER) return 0;
            s->puppet_components[c] = kind;
            if (kind == MP_COMPONENT_BASE && s->puppet_base_index == MP_NO_ID)
                s->puppet_base_index = c;
        }
        if (puppet) s->has_actor_sync = 1;
    }
    return r->ok;
}

/* name[0..n) is prefix followed by at least one character; *rest is that remainder. */
static inline int mp_entry_value(const char* name, mp_u32 n, const char* prefix, const char** rest, mp_u32* rest_n) {
    mp_u32 k = 0;
    while (prefix[k]) {
        if (k == n || name[k] != prefix[k]) return 0;
        ++k;
    }
    if (k == n) return 0;
    *rest = name + k;
    *rest_n = n - k;
    return 1;
}

/* Decimal digits; max + 1 when malformed or larger than max. */
static inline mp_u32 mp_entry_number(const char* text, mp_u32 n, mp_u32 max) {
    mp_u32 value = 0;
    for (mp_u32 k = 0; k != n; ++k) {
        if (text[k] < '0' || text[k] > '9' || value > max) return max + 1;
        value = value * 10 + (mp_u32)(text[k] - '0');
    }
    return value > max ? max + 1 : value;
}

/* ActorFilter: VLQ count + names. Names longer than an actor name are not kept (none can match). */
static inline int mp_read_actor_filter(mp_reader* r, mp_session* s) {
    mp_u32 count = mp_read_vlq(r);
    mp_u32 prefix = sizeof(MP_MULTIPLIER_ENTRY) - 1;
    mp_u32 random_prefix = sizeof(MP_RANDOMIZER_ENTRY) - 1;
    for (mp_u32 i = 0; r->ok && i != count; ++i) {
        mp_u32 n;
        const char* name = mp_read_string(r, &n);
        if (!r->ok) break;
        int randomizer = n == random_prefix + 1;
        for (mp_u32 k = 0; randomizer && k != random_prefix; ++k) randomizer = name[k] == MP_RANDOMIZER_ENTRY[k];
        if (randomizer) {
            s->enemy_randomizer = name[random_prefix] == '1';
            continue;
        }
        const char* rest;
        mp_u32 rest_n;
        if (mp_entry_value(name, n, MP_SCALING_ENTRY, &rest, &rest_n)) {
            s->dynamic_scaling = rest_n == 1 && rest[0] == '1';
            continue;
        }
        if (mp_entry_value(name, n, MP_PVP_ENTRY, &rest, &rest_n)) {
            s->pvp = rest_n == 1 && rest[0] == '1';
            continue;
        }
        if (mp_entry_value(name, n, MP_SHARED_LIFE_ENTRY, &rest, &rest_n)) {
            s->shared_life = rest_n == 1 && rest[0] == '1';
            continue;
        }
        if (mp_entry_value(name, n, MP_ELITE_ENTRY, &rest, &rest_n)) {
            mp_u32 chance = mp_entry_number(rest, rest_n, MP_ELITE_CHANCE_MAX);
            s->elite_chance = chance > MP_ELITE_CHANCE_MAX ? 0 : chance;
            continue;
        }
        int setting = n > prefix;
        for (mp_u32 k = 0; setting && k != prefix; ++k) setting = name[k] == MP_MULTIPLIER_ENTRY[k];
        if (setting) {
            mp_u32 value = 0;
            for (mp_u32 k = prefix; k != n && value <= MP_MULTIPLIER_MAX; ++k) {
                if (name[k] < '0' || name[k] > '9') { value = 0; break; }
                value = value * 10 + (mp_u32)(name[k] - '0');
            }
            if (value > MP_MULTIPLIER_MAX) value = MP_MULTIPLIER_MAX;
            s->enemy_multiplier = value ? value : 1;
            continue;
        }
        if (n == 0 || n > MP_ACTOR_NAME_MAX || s->filter_count == MP_MAX_FILTER) continue;
        char* kept = s->filter[s->filter_count++];
        for (mp_u32 k = 0; k != MP_ACTOR_NAME_MAX; ++k) kept[k] = k < n ? name[k] : 0;
    }
    return r->ok;
}

/* ResourceClient::readResource: name, byte hasChildren, then VLQ children or VLQ size. */
static inline int mp_skip_resource(mp_reader* r, int depth) {
    mp_u32 n;
    if (depth > MP_MAX_RESOURCE_DEPTH) return 0;
    mp_read_string(r, &n);
    if (mp_read_u8(r)) {
        mp_u32 children = mp_read_vlq(r);
        for (mp_u32 i = 0; r->ok && i != children; ++i)
            if (!mp_skip_resource(r, depth + 1)) return 0;
    } else {
        mp_read_vlq(r);
    }
    return r->ok;
}

/* Initializers have no length prefix, so every one WWClient::onInitializer knows is
 * parsed to stay aligned; any other name is rejected like the historical client. */
static inline int mp_skip_initializer(mp_reader* r, const char* name, mp_u32 n, mp_session* s) {
    mp_u32 count;
    if (mp_string_equals(name, n, "ActorSync")) return mp_read_actor_sync(r, s);
    if (mp_string_equals(name, n, "ActorFilter")) return mp_read_actor_filter(r, s);
    if (mp_string_equals(name, n, "SaveServer")) {
        /* SaveServer::readMap: VLQ entries of {u8 type, u8 length}; kept when they fit */
        count = mp_read_vlq(r);
        if (count > 0x7FFFFFFFu) return 0;
        s->save_map_count = count <= MP_SAVE_MAP_MAX ? count : 0;
        for (mp_u32 i = 0; r->ok && i != count; ++i) {
            mp_u8 type = mp_read_u8(r), length = mp_read_u8(r);
            if (i < MP_SAVE_MAP_MAX) {
                s->save_map_type[i] = type;
                s->save_map_length[i] = length;
            }
        }
        return r->ok;
    }
    if (mp_string_equals(name, n, "ResourceClient")) {
        mp_read_vlq(r); /* packet size */
        count = mp_read_vlq(r);
        for (mp_u32 i = 0; r->ok && i != count; ++i) {
            mp_read_string(r, &n);
            mp_u32 resources = mp_read_vlq(r);
            for (mp_u32 j = 0; r->ok && j != resources; ++j)
                if (!mp_skip_resource(r, 1)) return 0;
        }
        return r->ok;
    }
    return 0;
}

/* Parses the init payload (without its VLQ length prefix), see Client::readInitPacket. */
static inline int mp_parse_init(const mp_u8* payload, mp_u32 size, mp_session* s) {
    mp_reader r = mp_reader_make(payload, size);
    mp_u32 n, count;
    s->server_create = s->server_update = s->server_remove = s->server_mutators = MP_NO_ID;
    s->server_player_directory = s->client_player_directory = MP_NO_ID;
    s->server_player_summon = s->client_player_summon = MP_NO_ID;
    s->server_save_update = s->server_save_from = s->server_world = MP_NO_ID;
    s->client_save_update = s->client_save_merge = s->client_world = MP_NO_ID;
    s->client_player_revive = s->server_player_revive = MP_NO_ID;
    s->client_player_carry = s->server_player_carry = MP_NO_ID;
    s->client_player_event = s->server_player_event = MP_NO_ID;
    s->client_player_pay = s->server_player_pay = MP_NO_ID;
    s->save_map_count = 0;
    s->client_init = s->client_change_manager = s->client_leave_manager = MP_NO_ID;
    s->client_create = s->client_update = s->client_remove = MP_NO_ID;
    s->client_exclusive_add = s->client_exclusive_remove = MP_NO_ID;
    s->puppet_component_count = 0;
    s->puppet_base_index = MP_NO_ID;
    s->has_actor_sync = 0;
    s->factory_count = 0;
    s->factories_complete = 1;
    s->filter_count = 0;
    s->enemy_multiplier = 1;
    s->enemy_randomizer = 0;
    s->dynamic_scaling = 0;
    s->elite_chance = 0;
    s->pvp = 0;
    s->shared_life = 0;

    count = mp_read_vlq(&r);
    for (mp_u32 i = 0; r.ok && i != count; ++i) {
        const char* name = mp_read_string(&r, &n);
        mp_assign_id(name, n, i, "SERVER_ACTOR_SYNC_CREATE", &s->server_create);
        mp_assign_id(name, n, i, "SERVER_ACTOR_SYNC_UPDATE", &s->server_update);
        mp_assign_id(name, n, i, "SERVER_ACTOR_SYNC_REMOVE", &s->server_remove);
        mp_assign_id(name, n, i, "SERVER_MUTATORS", &s->server_mutators);
        mp_assign_id(name, n, i, "SERVER_PLAYER_DIRECTORY", &s->server_player_directory);
        mp_assign_id(name, n, i, "SERVER_PLAYER_SUMMON", &s->server_player_summon);
        mp_assign_id(name, n, i, "SERVER_SAVE_UPDATE", &s->server_save_update);
        mp_assign_id(name, n, i, "SERVER_SAVE_FROM", &s->server_save_from);
        mp_assign_id(name, n, i, "SERVER_WORLD", &s->server_world);
        mp_assign_id(name, n, i, "SERVER_PLAYER_REVIVE", &s->server_player_revive);
        mp_assign_id(name, n, i, "SERVER_PLAYER_CARRY", &s->server_player_carry);
        mp_assign_id(name, n, i, "SERVER_PLAYER_EVENT", &s->server_player_event);
        mp_assign_id(name, n, i, "SERVER_PLAYER_PAY", &s->server_player_pay);
    }
    count = mp_read_vlq(&r);
    for (mp_u32 i = 0; r.ok && i != count; ++i) {
        const char* name = mp_read_string(&r, &n);
        mp_assign_id(name, n, i, "CLIENT_INIT_PACKET", &s->client_init);
        mp_assign_id(name, n, i, "CLIENT_PLAYER_CHANGE_MANAGER", &s->client_change_manager);
        mp_assign_id(name, n, i, "CLIENT_PLAYER_LEAVE_MANAGER", &s->client_leave_manager);
        mp_assign_id(name, n, i, "CLIENT_ACTOR_SYNC_CREATE", &s->client_create);
        mp_assign_id(name, n, i, "CLIENT_ACTOR_SYNC_UPDATE", &s->client_update);
        mp_assign_id(name, n, i, "CLIENT_ACTOR_SYNC_REMOVE", &s->client_remove);
        mp_assign_id(name, n, i, "CLIENT_PLAYER_DIRECTORY", &s->client_player_directory);
        mp_assign_id(name, n, i, "CLIENT_PLAYER_SUMMON", &s->client_player_summon);
        mp_assign_id(name, n, i, "CLIENT_SAVE_UPDATE", &s->client_save_update);
        mp_assign_id(name, n, i, "CLIENT_SAVE_MERGE", &s->client_save_merge);
        mp_assign_id(name, n, i, "CLIENT_WORLD", &s->client_world);
        mp_assign_id(name, n, i, "CLIENT_PLAYER_REVIVE", &s->client_player_revive);
        mp_assign_id(name, n, i, "CLIENT_PLAYER_CARRY", &s->client_player_carry);
        mp_assign_id(name, n, i, "CLIENT_PLAYER_EVENT", &s->client_player_event);
        mp_assign_id(name, n, i, "CLIENT_PLAYER_PAY", &s->client_player_pay);
        mp_assign_id(name, n, i, "CLIENT_ACTOR_EXCLUSIVE_ADD", &s->client_exclusive_add);
        mp_assign_id(name, n, i, "CLIENT_ACTOR_EXCLUSIVE_REMOVE", &s->client_exclusive_remove);
    }
    count = mp_read_vlq(&r);
    for (mp_u32 i = 0; r.ok && i != count; ++i) {
        const char* name = mp_read_string(&r, &n);
        if (!mp_skip_initializer(&r, name, n, s)) return 0;
    }
    return r.ok && r.pos == r.size;
}

/* ------------------------------------------------------------------ components */

typedef struct mp_transform {
    float pos[3];      /* fopAc_ac_c::current.pos */
    mp_s16 rot[3];     /* fopAc_ac_c::shape_angle */
} mp_transform;

/* ActorBaseSync::write: cXyz as 3 floats, csXyz as 3 shorts. */
static inline void mp_write_base(mp_writer* w, const mp_transform* t) {
    for (int i = 0; i != 3; ++i) mp_write_f32(w, t->pos[i]);
    for (int i = 0; i != 3; ++i) mp_write_u16(w, (mp_u16)t->rot[i]);
}

static inline void mp_read_base(mp_reader* r, mp_transform* t) {
    for (int i = 0; i != 3; ++i) t->pos[i] = mp_read_f32(r);
    for (int i = 0; i != 3; ++i) t->rot[i] = (mp_s16)mp_read_u16(r);
}

/* ActorPlayerSync state without animation events. The defaults are the ActorPlayerSync
 * constructor values: no item/proc, the unmodified tunic colours the historical
 * recolouring replaces, and the default yellow map cursor.
 *
 * Protocol extension (WWHD): the player name follows the hit list as a NUL-terminated string.
 * The historical component never sent it (WindWakerServer ActorPlayerSync.java has an unused
 * playerName field, the GameCube launcher a name mailbox); the Java server reads and writes it
 * since this extension. The GameCube client does not know it. */
#define MP_NAME_MAX 16 /* printable ASCII; longer names are cut on reading */

/* Protocol extension (WWHD, after the name, optional the same way): the player's King of Red
 * Lions. u8 flags; when MP_SHIP_PRESENT, then f32 x, y, z and u16 rotation x, y, z (ActorBaseSync
 * order); when MP_SHIP_COLOR, then u8 r, g, b (its custom paint). Bits 4-5: the part deployed
 * (daShip_c::mPart: 0 none, 1 sail, 2 cannon, 3 crane); when MP_SHIP_DETAIL, then s16 tiller
 * (m0366), sail angle (mSailAngle), cannon pitch (m0394), cannon yaw (m0396), crane angle
 * (m0398 + m039C), rope segments (mRopeCnt), neck pitch (m03A0) and neck turn (m03A2), and f32
 * x, y, z of the crane's hook (mpLinkModel's base matrix). The Java server keeps and forwards it
 * (ActorPlayerSync.java ship*).
 *
 * Then the player's extras (WWHD, optional the same way, after the ship block): u8, bit 7 clear,
 * MP_EXTRA_HEAVY_BOOTS when the iron boots are on (daPy_lk_c::mNoResetFlg0 0x2000000). */
#define MP_EXTRA_HEAVY_BOOTS 0x01
#define MP_EXTRA_CASUAL 0x02      /* casual clothes worn (outfit.h) */
#define MP_EXTRA_LIGHT 0x04       /* the Mirror Shield sends light back (puppet.h puppet_light) */
#define MP_EXTRA_BARREL 0x08      /* hiding under a barrel: no actor sees the puppet (0.3.73) */
#define MP_EXTRAS 0x7F
#define MP_SHIP_PRESENT 0x01 /* the player's stage has its ship */
#define MP_SHIP_RIDING 0x02  /* Link is on it */
#define MP_SHIP_SAIL 0x04    /* its sail is up */
#define MP_SHIP_COLOR 0x08   /* custom paint follows */
#define MP_SHIP_PART_SHIFT 4
#define MP_SHIP_PART_MASK 0x30
#define MP_SHIP_DETAIL 0x40  /* tiller, sail, cannon, crane, rope and hook follow */
#define MP_SHIP_FLAGS 0x7F   /* bit 7 always clear: see mp_read_ship */

typedef struct mp_ship_state {
    mp_u8 flags;
    float pos[3];
    mp_s16 rot[3];
    mp_u8 color[3];  /* with MP_SHIP_COLOR */
    mp_s16 tiller, sail, cannon_pitch, cannon_yaw, crane, rope, neck_pitch, neck_turn; /* with MP_SHIP_DETAIL */
    float hook[3];
} mp_ship_state;

/* Protocol extension (WWHD, after the particles, optional the same way): the player's skin, a
 * NUL-terminated string, empty for Link. A skin is another Link model the player chose
 * (skin.h); its id is 1 to MP_SKIN_MAX of a-z 0-9 _, anything else reads as no skin. The Java
 * server keeps and forwards it (ActorPlayerSync.java skin). */
#define MP_SKIN_MAX 12

/* Protocol extension (WWHD, after the skin, optional the same way): the player's death (death.h).
 * u8 flags, bit 7 clear; with MP_DEATH_GHOST (dead, walking as a ghost until another player of the
 * stage revives it), then VLQ the player's directory id (where a revive is sent: CLIENT_PLAYER_REVIVE),
 * f32 x, y, z and u16 rotation y of its body. The Java server keeps and forwards it
 * (ActorPlayerSync.java death*). MP_DEATH_DOWN (no extra field): dead without a ghost, the game's
 * own death and game over under way; such a player is not a survivor of its stage. The server
 * forwards the flags byte as it is (DEATH_FLAGS mask), older clients only test MP_DEATH_GHOST. */
#define MP_DEATH_GHOST 0x01
#define MP_DEATH_DOWN 0x02
#define MP_DEATH_FLAGS 0x7F

typedef struct mp_player_look {
    mp_u16 item;
    mp_u32 proc;
    mp_u32 colors[5]; /* RGB565 in u32 slots */
    mp_u8 cursor[4];  /* GXColor r, g, b, a */
    char name[MP_NAME_MAX + 1];
    mp_ship_state ship;
    mp_u8 extras; /* MP_EXTRA_* */
    char skin[MP_SKIN_MAX + 1]; /* "" for Link */
    mp_u8 death;                /* MP_DEATH_* */
    mp_u32 death_dir;           /* with MP_DEATH_GHOST: its directory id */
    float corpse[3];            /* with MP_DEATH_GHOST: its body */
    mp_s16 corpse_angle;
} mp_player_look;

static inline mp_player_look mp_player_look_default(void) {
    mp_player_look l;
    l.item = 0xFFFF;
    l.proc = 0xFFFFFFFFu;
    l.colors[0] = 0x5D89;
    l.colors[1] = 0xAF08;
    l.colors[2] = 0x24E7;
    l.colors[3] = 0x8566;
    l.colors[4] = 0x2443;
    l.cursor[0] = 0xFF;
    l.cursor[1] = 0xFF;
    l.cursor[2] = 0x4D;
    l.cursor[3] = 0xFF;
    for (int i = 0; i != MP_NAME_MAX + 1; ++i) l.name[i] = 0;
    l.ship.flags = 0;
    for (int i = 0; i != 3; ++i) {
        l.ship.pos[i] = 0.0f;
        l.ship.rot[i] = 0;
        l.ship.color[i] = 0;
        l.ship.hook[i] = 0.0f;
    }
    l.ship.tiller = l.ship.sail = l.ship.cannon_pitch = l.ship.cannon_yaw = l.ship.crane = l.ship.rope = 0;
    l.ship.neck_pitch = l.ship.neck_turn = 0;
    l.extras = 0;
    for (int i = 0; i != MP_SKIN_MAX + 1; ++i) l.skin[i] = 0;
    l.death = 0;
    l.death_dir = 0;
    l.corpse[0] = l.corpse[1] = l.corpse[2] = 0.0f;
    l.corpse_angle = 0;
    return l;
}

/* A skin id: 1 to MP_SKIN_MAX of a-z 0-9 _. */
static inline int mp_skin_valid(const char* text, mp_u32 length) {
    if (length == 0 || length > MP_SKIN_MAX) return 0;
    for (mp_u32 i = 0; i != length; ++i) {
        char c = text[i];
        if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_')) return 0;
    }
    return 1;
}

/* Copies a valid skin id, else no skin. */
static inline void mp_set_skin(mp_player_look* l, const char* text, mp_u32 length) {
    mp_u32 n = mp_skin_valid(text, length) ? length : 0;
    for (mp_u32 i = 0; i != MP_SKIN_MAX + 1; ++i) l->skin[i] = i < n ? text[i] : 0;
}

/* Copies a name keeping printable ASCII only (others become '?'), at most MP_NAME_MAX. */
static inline void mp_set_name(mp_player_look* l, const char* name, mp_u32 length) {
    mp_u32 n = 0;
    for (; n != length && n != MP_NAME_MAX && name[n]; ++n)
        l->name[n] = name[n] >= 0x20 && name[n] <= 0x7E ? name[n] : '?';
    for (; n != MP_NAME_MAX + 1; ++n) l->name[n] = 0;
}

/* ActorPlayerSync::darkenRGB565: each channel scaled by 3/4. */
static inline mp_u16 mp_darken_rgb565(mp_u16 color) {
    mp_u16 r = (mp_u16)((color >> 11) & 0x1F), g = (mp_u16)((color >> 5) & 0x3F), b = (mp_u16)(color & 0x1F);
    r = (mp_u16)(r * 3 / 4);
    g = (mp_u16)(g * 3 / 4);
    b = (mp_u16)(b * 3 / 4);
    return (mp_u16)((r << 11) | (g << 5) | b);
}

/* The five tunic colours ActorPlayerSync::calc derives from the primary and secondary
 * colours chosen by the player (its owner branch; the constructor darkens the primary twice). */
static inline void mp_player_look_set_colors(mp_player_look* l, mp_u16 primary, mp_u16 secondary) {
    l->colors[0] = primary;
    l->colors[1] = secondary;
    l->colors[2] = mp_darken_rgb565(primary);
    l->colors[3] = mp_darken_rgb565(secondary);
    l->colors[4] = mp_darken_rgb565(mp_darken_rgb565(primary));
}

static inline mp_u16 mp_rgb888_to_565(mp_u32 rgb) {
    return (mp_u16)((((rgb >> 19) & 0x1F) << 11) | (((rgb >> 10) & 0x3F) << 5) | ((rgb >> 3) & 0x1F));
}

static inline mp_u32 mp_rgb565_to_888(mp_u16 c) {
    mp_u32 r = (c >> 11) & 0x1F, g = (c >> 5) & 0x3F, b = c & 0x1F;
    return (((r << 3) | (r >> 2)) << 16) | (((g << 2) | (g >> 4)) << 8) | ((b << 3) | (b >> 2));
}

/* ------------------------------------------------------------------ animation events */

/* Protocol extension (WWHD, after the extras, optional the same way): the owner's sword swings,
 * for the scenery that the game cuts through its mass collision manager (dCcMassS_Mng: grass,
 * flowers, bushes, small trees), which are not actors and so outside the historical hit list.
 * u8 count (bit 7 clear), then per swing: u32 attack type (dCcD_GObjInf::mAtType), f32 x, y, z
 * attack vector (mGObjAt.mVec), u8 capsule count (bit 7 clear), then per capsule f32 start x, y,
 * z, end x, y, z and radius. One swing is one logic step of the attack colliders Link registers
 * with the mass manager (daPy_lk_c::setCollision 024076CC); a spin attack's cylinder travels as
 * the capsule from its centre to its top. Events: sent once, never kept by the server. */
#define MP_MAX_SWINGS 4        /* per message; the newest swing replaces the last slot */
#define MP_SWING_CAPSULES 4    /* mAtCps[0..2], or the spin attack's mAtCyl */

typedef struct mp_swing_capsule {
    float start[3], end[3], radius;
} mp_swing_capsule;

/* WWHD extension after the swings: the hits of the player's sword on synchronised placed actors
 * (actor_sync.h), a byte count (bit 7 clear) then the records. The historical hit list
 * (ActorPlayerSync HitPacket: actor uid, collider offset, capsule index) stays empty: its
 * receivers replayed the hit with their copy of the player's capsule, whose attack values the
 * HD puppet does not have; a record here carries them. */
#define MP_MAX_STRIKES 8       /* per message; the newest replaces the last slot */

typedef struct mp_strike {
    mp_u32 target;      /* the synchronised actor's uid */
    mp_u32 tg_offset;   /* its hit collider (cCcD_Obj), bytes from the actor */
    mp_u32 attacker;    /* 0: the player's Link; else the uid of its spawned actor (bomb, arrow...) */
    mp_u32 at_offset;   /* with an attacker: its attack collider, bytes from it */
    mp_u8 at_index;     /* the player's collider: 0..2 mAtCps, 3 mAtCyl */
    mp_u8 atp;          /* cCcD_ObjAt::mAtp, the damage */
    mp_u8 se, mtrl, hit_mark, spl; /* dCcD_GObjAt */
    mp_u32 at_sprm;     /* cCcD_ObjAt::mSPrm */
    mp_u32 at_type;     /* cCcD_ObjAt::mType */
    mp_u32 gat_sprm;    /* dCcD_GObjAt::mSPrm */
    float vec[3];       /* dCcD_GObjAt::mVec */
    float pos[3];       /* the hit position */
} mp_strike;

/* WWHD extension after the strikes: particle effects the player's Link made (hammer on the
 * ground, sword sparks and splashes...), replayed where they were (particle_sync.h). A byte count
 * (bit 7 clear), then per particle: u8 group, u16 id, u8 flags (1 angle, 2 scale, 4 matrix,
 * 8 follow, 16 stop, 32 colours), f32 x y z, [f32 rotation 3x3, row by row], [s16 angle x y z],
 * [f32 scale x y z], u8 alpha, [u8 key], [u8 prm r g b a, u8 env r g b a].
 * A followed emitter (0.2.41: one its maker moves every frame and stops itself, the sword's charge
 * or spin) has a key of its maker's: the first record with that key makes it, the next ones move it
 * (position and rotation), one with "stop" ends it. */
#define MP_MAX_PARTICLES 16
#define MP_PARTICLE_ANGLE 1
#define MP_PARTICLE_SCALE 2
#define MP_PARTICLE_MATRIX 4
#define MP_PARTICLE_FOLLOW 8
#define MP_PARTICLE_STOP 16
#define MP_PARTICLE_COLOR 32
#define MP_PARTICLE_FLAGS 0x3F

typedef struct mp_particle {
    mp_u8 group;
    mp_u16 id;
    mp_u8 flags;
    float pos[3];
    float matrix[9];   /* the emitter's global rotation, when MP_PARTICLE_MATRIX */
    mp_s16 angle[3];
    float scale[3];
    mp_u8 alpha;
    mp_u8 key;         /* MP_PARTICLE_FOLLOW */
    mp_u8 prm[4];      /* MP_PARTICLE_COLOR: primary and environment colours */
    mp_u8 env[4];
} mp_particle;

typedef struct mp_swing {
    mp_u32 at_type;
    float vec[3];
    mp_u32 count;
    mp_swing_capsule capsule[MP_SWING_CAPSULES];
} mp_swing;

/* The ActorPlayerSync event lists (ActorPlayerSync.h packet structs, same field order as
 * ActorPlayerSync::write). Each list records one daPy_lk_c call of the owner's Link, replayed
 * on the remote Link. Field names follow the HD signatures (wwhd/bindings.h). */
#define MP_MAX_EVENTS 16 /* per list and per message; the newest event replaces the last slot */

typedef struct mp_move_anim {    /* setMoveAnime(f27, f28, f25, r27, r28, r29, i_morf) */
    float blend, rate, rate1;    /* f1 f2 f3 */
    mp_u32 anm0, anm1, mode;     /* animdata1 animdata2 x (VLQ) */
    float morf;                  /* f4 */
} mp_move_anim;

typedef struct mp_single_anim {  /* setSingleMoveAnime(anm, rate, start, end, i_morf) */
    mp_u32 anm;
    float rate, start;
    mp_s16 end;                  /* s16 sent through writeIntVLQ: sign-extended VLQ */
    float morf;
} mp_single_anim;

typedef struct mp_upper_anim {   /* setActAnimeUpper(bckIdx, upperIdx, rate, start, end, i_morf) */
    mp_u16 bck;                  /* u16 in the GameCube packet */
    mp_u32 upper;
    float rate, start;
    mp_s16 end;
    float morf;
} mp_upper_anim;

typedef struct mp_reset_upper {  /* resetActAnimeUpper(upperIdx, i_morf) */
    mp_u32 upper;
    float morf;
} mp_reset_upper;

/* 0.3.79: Link's voice and sounds (anim_sync.h), after the death block, optional the same way:
 * u8 count (bit 7 clear, at most MP_MAX_SOUNDS), each u8 kind (MP_SOUND_*) and u32 the sound id.
 * The server keeps and forwards them like the other event lists (ActorPlayerSync.java sounds). */
#define MP_MAX_SOUNDS 16
#define MP_SOUND_VOICE 0          /* daPy_lk_c::voiceStart */
#define MP_SOUND_REVERB 1         /* daPy_lk_c::seStartOnlyReverb */
#define MP_SOUND_SWORD 2          /* daPy_lk_c::seStartSwordCut (played at the puppet) */
typedef struct mp_sound {
    mp_u8 kind;
    mp_u32 id;
} mp_sound;

typedef struct mp_anim_events {
    mp_u32 move_count, single_count, upper_count, reset_count, sword_count;
    mp_u32 equip_item_count, unequip_count; /* setAnimeEquipItem / setAnimeUnequip: no data */
    mp_u32 hit_count;                       /* decoded and dropped (no hit replay yet) */
    mp_u32 swing_count;                     /* WWHD extension */
    mp_u32 strike_count;                    /* WWHD extension */
    mp_u32 particle_count;                  /* WWHD extension */
    mp_u32 dropped;                         /* events beyond MP_MAX_EVENTS or MP_MAX_SWINGS */
    mp_move_anim move[MP_MAX_EVENTS];
    mp_single_anim single[MP_MAX_EVENTS];
    mp_upper_anim upper[MP_MAX_EVENTS];
    mp_reset_upper reset[MP_MAX_EVENTS];
    mp_u32 sword[MP_MAX_EVENTS];            /* setAnimeEquipSword(b1) */
    mp_swing swing[MP_MAX_SWINGS];
    mp_strike strike[MP_MAX_STRIKES];
    mp_particle particle[MP_MAX_PARTICLES];
    mp_u32 sound_count;                     /* WWHD extension (0.3.79) */
    mp_sound sound[MP_MAX_SOUNDS];
} mp_anim_events;

static inline void mp_anim_events_clear(mp_anim_events* e) {
    e->move_count = e->single_count = e->upper_count = e->reset_count = e->sword_count = 0;
    e->equip_item_count = e->unequip_count = e->hit_count = e->swing_count = e->dropped = 0;
    e->strike_count = e->particle_count = 0;
    e->sound_count = 0;
}

static inline int mp_anim_events_empty(const mp_anim_events* e) {
    return !(e->move_count | e->single_count | e->upper_count | e->reset_count | e->sword_count |
             e->equip_item_count | e->unequip_count | e->swing_count | e->strike_count | e->particle_count |
             e->sound_count);
}

/* Slot for one more particle (the newest survives in the last slot). */
static inline mp_u32 mp_particle_slot(mp_anim_events* e) {
    if (e->particle_count < MP_MAX_PARTICLES) return e->particle_count++;
    ++e->dropped;
    return MP_MAX_PARTICLES - 1;
}

/* Slot for one more strike (the newest survives in the last slot). */
static inline mp_u32 mp_strike_slot(mp_anim_events* e) {
    if (e->strike_count < MP_MAX_STRIKES) return e->strike_count++;
    ++e->dropped;
    return MP_MAX_STRIKES - 1;
}

/* Slot for one more swing (the newest survives in the last slot, like mp_anim_slot). */
static inline mp_u32 mp_swing_slot(mp_anim_events* e) {
    if (e->swing_count < MP_MAX_SWINGS) return e->swing_count++;
    ++e->dropped;
    return MP_MAX_SWINGS - 1;
}

/* Slot for one more event of a list holding *count entries: a full list reuses its last slot,
 * so the most recent call always survives (the remote Link ends in the owner's latest pose). */
static inline mp_u32 mp_anim_slot(mp_u32* count, mp_u32* dropped) {
    if (*count < MP_MAX_EVENTS) return (*count)++;
    ++*dropped;
    return MP_MAX_EVENTS - 1;
}

/* Appends every event of src to dst (both bounded). */
static inline void mp_anim_events_append(mp_anim_events* dst, const mp_anim_events* src) {
    for (mp_u32 i = 0; i != src->move_count; ++i)
        dst->move[mp_anim_slot(&dst->move_count, &dst->dropped)] = src->move[i];
    for (mp_u32 i = 0; i != src->single_count; ++i)
        dst->single[mp_anim_slot(&dst->single_count, &dst->dropped)] = src->single[i];
    for (mp_u32 i = 0; i != src->upper_count; ++i)
        dst->upper[mp_anim_slot(&dst->upper_count, &dst->dropped)] = src->upper[i];
    for (mp_u32 i = 0; i != src->reset_count; ++i)
        dst->reset[mp_anim_slot(&dst->reset_count, &dst->dropped)] = src->reset[i];
    for (mp_u32 i = 0; i != src->sword_count; ++i)
        dst->sword[mp_anim_slot(&dst->sword_count, &dst->dropped)] = src->sword[i];
    for (mp_u32 i = 0; i != src->swing_count; ++i)
        dst->swing[mp_swing_slot(dst)] = src->swing[i];
    for (mp_u32 i = 0; i != src->strike_count; ++i)
        dst->strike[mp_strike_slot(dst)] = src->strike[i];
    for (mp_u32 i = 0; i != src->particle_count; ++i)
        dst->particle[mp_particle_slot(dst)] = src->particle[i];
    for (mp_u32 i = 0; i != src->sound_count && dst->sound_count != MP_MAX_SOUNDS; ++i)
        dst->sound[dst->sound_count++] = src->sound[i];
    dst->equip_item_count += src->equip_item_count;
    dst->unequip_count += src->unequip_count;
    dst->dropped += src->dropped;
}

static inline void mp_write_ship(mp_writer* w, const mp_ship_state* s) {
    mp_write_u8(w, (mp_u8)(s->flags & MP_SHIP_FLAGS));
    if (s->flags & MP_SHIP_PRESENT) {
        for (int i = 0; i != 3; ++i) mp_write_f32(w, s->pos[i]);
        for (int i = 0; i != 3; ++i) mp_write_u16(w, (mp_u16)s->rot[i]);
    }
    if (s->flags & MP_SHIP_COLOR)
        for (int i = 0; i != 3; ++i) mp_write_u8(w, s->color[i]);
    if (s->flags & MP_SHIP_DETAIL) {
        mp_write_u16(w, (mp_u16)s->tiller);
        mp_write_u16(w, (mp_u16)s->sail);
        mp_write_u16(w, (mp_u16)s->cannon_pitch);
        mp_write_u16(w, (mp_u16)s->cannon_yaw);
        mp_write_u16(w, (mp_u16)s->crane);
        mp_write_u16(w, (mp_u16)s->rope);
        mp_write_u16(w, (mp_u16)s->neck_pitch);
        mp_write_u16(w, (mp_u16)s->neck_turn);
        for (int i = 0; i != 3; ++i) mp_write_f32(w, s->hook[i]);
    }
}

/* The ship block when it is there. ActorPlayerSync is not always the last component (a server
 * create may write ActorBaseSync after it): what follows the name may be the next component's
 * index, one VLQ byte with bit 7 set (at most MP_MAX_COMPONENTS). The ship flags (0..7) have bit 7
 * clear, so a byte with bit 7 set, or no byte, means no ship block (a sender before it). */
static inline void mp_read_ship(mp_reader* r, mp_ship_state* s) {
    s->flags = 0;
    for (int i = 0; i != 3; ++i) {
        s->pos[i] = 0.0f;
        s->rot[i] = 0;
        s->color[i] = 0;
        s->hook[i] = 0.0f;
    }
    s->tiller = s->sail = s->cannon_pitch = s->cannon_yaw = s->crane = s->rope = 0;
    s->neck_pitch = s->neck_turn = 0;
    if (!r->ok || r->pos >= r->size || (r->data[r->pos] & 0x80)) return;
    mp_u8 flags = mp_read_u8(r);
    if (flags & MP_SHIP_PRESENT) {
        for (int i = 0; i != 3; ++i) s->pos[i] = mp_read_f32(r);
        for (int i = 0; i != 3; ++i) s->rot[i] = (mp_s16)mp_read_u16(r);
    }
    if (flags & MP_SHIP_COLOR)
        for (int i = 0; i != 3; ++i) s->color[i] = mp_read_u8(r);
    if (flags & MP_SHIP_DETAIL) {
        s->tiller = (mp_s16)mp_read_u16(r);
        s->sail = (mp_s16)mp_read_u16(r);
        s->cannon_pitch = (mp_s16)mp_read_u16(r);
        s->cannon_yaw = (mp_s16)mp_read_u16(r);
        s->crane = (mp_s16)mp_read_u16(r);
        s->rope = (mp_s16)mp_read_u16(r);
        s->neck_pitch = (mp_s16)mp_read_u16(r);
        s->neck_turn = (mp_s16)mp_read_u16(r);
        for (int i = 0; i != 3; ++i) s->hook[i] = mp_read_f32(r);
    }
    if (r->ok) s->flags = flags;
}

static inline void mp_write_swings(mp_writer* w, const mp_anim_events* e) {
    mp_u32 count = e ? e->swing_count : 0;
    if (count > MP_MAX_SWINGS) count = MP_MAX_SWINGS;
    mp_write_u8(w, (mp_u8)count);
    for (mp_u32 i = 0; i != count; ++i) {
        const mp_swing* s = &e->swing[i];
        mp_u32 n = s->count > MP_SWING_CAPSULES ? MP_SWING_CAPSULES : s->count;
        mp_write_u32(w, s->at_type);
        for (int k = 0; k != 3; ++k) mp_write_f32(w, s->vec[k]);
        mp_write_u8(w, (mp_u8)n);
        for (mp_u32 c = 0; c != n; ++c) {
            for (int k = 0; k != 3; ++k) mp_write_f32(w, s->capsule[c].start[k]);
            for (int k = 0; k != 3; ++k) mp_write_f32(w, s->capsule[c].end[k]);
            mp_write_f32(w, s->capsule[c].radius);
        }
    }
}

/* The swing block when it is there (a byte with bit 7 clear, as for the ship block). Counts
 * above the limits are consumed and counted in e->dropped. */
static inline void mp_read_swings(mp_reader* r, mp_anim_events* e) {
    e->swing_count = 0;
    if (!r->ok || r->pos >= r->size || (r->data[r->pos] & 0x80)) return;
    mp_u32 count = mp_read_u8(r);
    for (mp_u32 i = 0; r->ok && i != count; ++i) {
        mp_swing s;
        s.at_type = mp_read_u32(r);
        for (int k = 0; k != 3; ++k) s.vec[k] = mp_read_f32(r);
        mp_u32 n = mp_read_u8(r);
        if (n & 0x80) r->ok = 0;
        s.count = 0;
        for (mp_u32 c = 0; r->ok && c != n; ++c) {
            mp_swing_capsule capsule;
            for (int k = 0; k != 3; ++k) capsule.start[k] = mp_read_f32(r);
            for (int k = 0; k != 3; ++k) capsule.end[k] = mp_read_f32(r);
            capsule.radius = mp_read_f32(r);
            if (s.count < MP_SWING_CAPSULES) s.capsule[s.count++] = capsule;
            else ++e->dropped;
        }
        if (r->ok && s.count) e->swing[mp_swing_slot(e)] = s;
    }
}

static inline void mp_write_strikes(mp_writer* w, const mp_anim_events* e) {
    mp_u32 count = e ? e->strike_count : 0;
    if (count > MP_MAX_STRIKES) count = MP_MAX_STRIKES;
    mp_write_u8(w, (mp_u8)count);
    for (mp_u32 i = 0; i != count; ++i) {
        const mp_strike* k = &e->strike[i];
        mp_write_vlq(w, k->target);
        mp_write_vlq(w, k->tg_offset);
        mp_write_vlq(w, k->attacker);
        mp_write_vlq(w, k->at_offset);
        mp_write_u8(w, k->at_index);
        mp_write_u8(w, k->atp);
        mp_write_u8(w, k->se);
        mp_write_u8(w, k->mtrl);
        mp_write_u8(w, k->hit_mark);
        mp_write_u8(w, k->spl);
        mp_write_u32(w, k->at_sprm);
        mp_write_u32(w, k->at_type);
        mp_write_u32(w, k->gat_sprm);
        for (int c = 0; c != 3; ++c) mp_write_f32(w, k->vec[c]);
        for (int c = 0; c != 3; ++c) mp_write_f32(w, k->pos[c]);
    }
}

/* The strike block when it is there (a byte with bit 7 clear, as for the swings). */
static inline void mp_read_strikes(mp_reader* r, mp_anim_events* e) {
    e->strike_count = 0;
    if (!r->ok || r->pos >= r->size || (r->data[r->pos] & 0x80)) return;
    mp_u32 count = mp_read_u8(r);
    for (mp_u32 i = 0; r->ok && i != count; ++i) {
        mp_strike k;
        k.target = mp_read_vlq(r);
        k.tg_offset = mp_read_vlq(r);
        k.attacker = mp_read_vlq(r);
        k.at_offset = mp_read_vlq(r);
        k.at_index = mp_read_u8(r);
        k.atp = mp_read_u8(r);
        k.se = mp_read_u8(r);
        k.mtrl = mp_read_u8(r);
        k.hit_mark = mp_read_u8(r);
        k.spl = mp_read_u8(r);
        k.at_sprm = mp_read_u32(r);
        k.at_type = mp_read_u32(r);
        k.gat_sprm = mp_read_u32(r);
        for (int c = 0; c != 3; ++c) k.vec[c] = mp_read_f32(r);
        for (int c = 0; c != 3; ++c) k.pos[c] = mp_read_f32(r);
        if (r->ok) e->strike[mp_strike_slot(e)] = k;
    }
}

static inline void mp_write_particles(mp_writer* w, const mp_anim_events* e) {
    mp_u32 count = e ? e->particle_count : 0;
    if (count > MP_MAX_PARTICLES) count = MP_MAX_PARTICLES;
    mp_write_u8(w, (mp_u8)count);
    for (mp_u32 i = 0; i != count; ++i) {
        const mp_particle* p = &e->particle[i];
        mp_write_u8(w, p->group);
        mp_write_u16(w, p->id);
        mp_write_u8(w, (mp_u8)(p->flags & MP_PARTICLE_FLAGS));
        for (int c = 0; c != 3; ++c) mp_write_f32(w, p->pos[c]);
        if (p->flags & MP_PARTICLE_MATRIX)
            for (int c = 0; c != 9; ++c) mp_write_f32(w, p->matrix[c]);
        if (p->flags & MP_PARTICLE_ANGLE)
            for (int c = 0; c != 3; ++c) mp_write_u16(w, (mp_u16)p->angle[c]);
        if (p->flags & MP_PARTICLE_SCALE)
            for (int c = 0; c != 3; ++c) mp_write_f32(w, p->scale[c]);
        mp_write_u8(w, p->alpha);
        if (p->flags & MP_PARTICLE_FOLLOW) mp_write_u8(w, p->key);
        if (p->flags & MP_PARTICLE_COLOR) {
            for (int c = 0; c != 4; ++c) mp_write_u8(w, p->prm[c]);
            for (int c = 0; c != 4; ++c) mp_write_u8(w, p->env[c]);
        }
    }
}

/* The particle block when it is there (a byte with bit 7 clear, as for the strikes). */
static inline void mp_read_particles(mp_reader* r, mp_anim_events* e) {
    e->particle_count = 0;
    if (!r->ok || r->pos >= r->size || (r->data[r->pos] & 0x80)) return;
    mp_u32 count = mp_read_u8(r);
    for (mp_u32 i = 0; r->ok && i != count; ++i) {
        mp_particle p;
        p.group = mp_read_u8(r);
        p.id = mp_read_u16(r);
        p.flags = mp_read_u8(r);
        if (p.flags & ~(mp_u32)MP_PARTICLE_FLAGS) r->ok = 0;
        if ((p.flags & MP_PARTICLE_STOP) && !(p.flags & MP_PARTICLE_FOLLOW)) r->ok = 0; /* stop what? */
        for (int c = 0; c != 3; ++c) p.pos[c] = mp_read_f32(r);
        for (int c = 0; c != 9; ++c) p.matrix[c] = (c % 4) ? 0.0f : 1.0f;
        if (p.flags & MP_PARTICLE_MATRIX)
            for (int c = 0; c != 9; ++c) p.matrix[c] = mp_read_f32(r);
        for (int c = 0; c != 3; ++c) p.angle[c] = 0;
        for (int c = 0; c != 3; ++c) p.scale[c] = 1.0f;
        if (p.flags & MP_PARTICLE_ANGLE)
            for (int c = 0; c != 3; ++c) p.angle[c] = (mp_s16)mp_read_u16(r);
        if (p.flags & MP_PARTICLE_SCALE)
            for (int c = 0; c != 3; ++c) p.scale[c] = mp_read_f32(r);
        p.alpha = mp_read_u8(r);
        p.key = (p.flags & MP_PARTICLE_FOLLOW) ? mp_read_u8(r) : 0;
        for (int c = 0; c != 4; ++c) p.prm[c] = p.env[c] = 0xFF;
        if (p.flags & MP_PARTICLE_COLOR) {
            for (int c = 0; c != 4; ++c) p.prm[c] = mp_read_u8(r);
            for (int c = 0; c != 4; ++c) p.env[c] = mp_read_u8(r);
        }
        if (r->ok) e->particle[mp_particle_slot(e)] = p;
    }
}

static inline int mp_same_ship(const mp_ship_state* a, const mp_ship_state* b) {
    if (a->flags != b->flags) return 0;
    if (a->flags & MP_SHIP_COLOR)
        for (int i = 0; i != 3; ++i)
            if (a->color[i] != b->color[i]) return 0;
    if (a->flags & MP_SHIP_DETAIL) {
        if (a->tiller != b->tiller || a->sail != b->sail || a->cannon_pitch != b->cannon_pitch ||
            a->cannon_yaw != b->cannon_yaw || a->crane != b->crane || a->rope != b->rope ||
            a->neck_pitch != b->neck_pitch || a->neck_turn != b->neck_turn)
            return 0;
        for (int i = 0; i != 3; ++i)
            if (a->hook[i] != b->hook[i]) return 0;
    }
    if (!(a->flags & MP_SHIP_PRESENT)) return 1;
    for (int i = 0; i != 3; ++i)
        if (a->pos[i] != b->pos[i] || a->rot[i] != b->rot[i]) return 0;
    return 1;
}

static inline void mp_write_death(mp_writer* w, const mp_player_look* l) {
    mp_u8 flags = (mp_u8)(l->death & MP_DEATH_FLAGS);
    mp_write_u8(w, flags);
    if (!(flags & MP_DEATH_GHOST)) return;
    mp_write_vlq(w, l->death_dir);
    for (int i = 0; i != 3; ++i) mp_write_f32(w, l->corpse[i]);
    mp_write_u16(w, (mp_u16)l->corpse_angle);
}

/* The death block, the same way as the extensions before it: a byte with bit 7 clear, else none. */
static inline void mp_read_death(mp_reader* r, mp_player_look* l) {
    l->death = 0;
    l->death_dir = 0;
    if (!(r->ok && r->pos < r->size && !(r->data[r->pos] & 0x80))) return;
    l->death = (mp_u8)(mp_read_u8(r) & MP_DEATH_FLAGS);
    if (!(l->death & MP_DEATH_GHOST)) return;
    l->death_dir = mp_read_vlq(r);
    for (int i = 0; i != 3; ++i) l->corpse[i] = mp_read_f32(r);
    l->corpse_angle = (mp_s16)mp_read_u16(r);
}

static inline void mp_write_sounds(mp_writer* w, const mp_anim_events* e) {
    mp_u32 n = e ? e->sound_count : 0;
    if (n > MP_MAX_SOUNDS) n = MP_MAX_SOUNDS;
    mp_write_u8(w, (mp_u8)n);
    for (mp_u32 i = 0; i != n; ++i) {
        mp_write_u8(w, e->sound[i].kind);
        mp_write_u32(w, e->sound[i].id);
    }
}

static inline void mp_read_sounds(mp_reader* r, mp_anim_events* e) {
    if (e) e->sound_count = 0;
    if (!(r->ok && r->pos < r->size && !(r->data[r->pos] & 0x80))) return;
    mp_u32 n = mp_read_u8(r);
    for (mp_u32 i = 0; i != n && r->ok; ++i) {
        mp_u8 kind = mp_read_u8(r);
        mp_u32 id = mp_read_u32(r);
        if (e && e->sound_count != MP_MAX_SOUNDS) {
            e->sound[e->sound_count].kind = kind;
            e->sound[e->sound_count].id = id;
            ++e->sound_count;
        }
    }
}

/* ActorPlayerSync::write; events may be null (all eight lists empty). */
static inline void mp_write_player_events(mp_writer* w, const mp_player_look* l, const mp_anim_events* e) {
    mp_write_u16(w, l->item);
    mp_write_vlq(w, l->proc);
    for (int i = 0; i != 5; ++i) mp_write_u32(w, l->colors[i]);
    for (int i = 0; i != 4; ++i) mp_write_u8(w, l->cursor[i]);
    if (!e) {
        for (int i = 0; i != 8; ++i) mp_write_vlq(w, 0);
        mp_write_string(w, l->name);
        mp_write_ship(w, &l->ship);
        mp_write_u8(w, (mp_u8)(l->extras & MP_EXTRAS));
        mp_write_swings(w, 0);
        mp_write_strikes(w, 0);
        mp_write_particles(w, 0);
        mp_write_string(w, l->skin);
        mp_write_death(w, l);
        mp_write_sounds(w, 0);
        return;
    }
    mp_write_vlq(w, e->move_count);
    for (mp_u32 i = 0; i != e->move_count; ++i) {
        const mp_move_anim* m = &e->move[i];
        mp_write_f32(w, m->blend); mp_write_f32(w, m->rate); mp_write_f32(w, m->rate1);
        mp_write_vlq(w, m->anm0); mp_write_vlq(w, m->anm1); mp_write_vlq(w, m->mode);
        mp_write_f32(w, m->morf);
    }
    mp_write_vlq(w, e->single_count);
    for (mp_u32 i = 0; i != e->single_count; ++i) {
        const mp_single_anim* s = &e->single[i];
        mp_write_vlq(w, s->anm); mp_write_f32(w, s->rate); mp_write_f32(w, s->start);
        mp_write_vlq(w, (mp_u32)(int)s->end); mp_write_f32(w, s->morf);
    }
    mp_write_vlq(w, e->upper_count);
    for (mp_u32 i = 0; i != e->upper_count; ++i) {
        const mp_upper_anim* u = &e->upper[i];
        mp_write_vlq(w, u->bck); mp_write_vlq(w, u->upper);
        mp_write_f32(w, u->rate); mp_write_f32(w, u->start);
        mp_write_vlq(w, (mp_u32)(int)u->end); mp_write_f32(w, u->morf);
    }
    mp_write_vlq(w, e->reset_count);
    for (mp_u32 i = 0; i != e->reset_count; ++i) {
        mp_write_vlq(w, e->reset[i].upper);
        mp_write_f32(w, e->reset[i].morf);
    }
    mp_write_vlq(w, e->sword_count);
    for (mp_u32 i = 0; i != e->sword_count; ++i) mp_write_vlq(w, e->sword[i]);
    mp_write_vlq(w, e->equip_item_count);
    mp_write_vlq(w, e->unequip_count);
    mp_write_vlq(w, 0); /* hits are not sent yet */
    mp_write_string(w, l->name);
    mp_write_ship(w, &l->ship);
    mp_write_u8(w, (mp_u8)(l->extras & MP_EXTRAS));
    mp_write_swings(w, e);
    mp_write_strikes(w, e);
    mp_write_particles(w, e);
    mp_write_string(w, l->skin);
    mp_write_death(w, l);
    mp_write_sounds(w, e);
}

static inline void mp_write_player(mp_writer* w, const mp_player_look* l) {
    mp_write_player_events(w, l, 0);
}

/* ActorPlayerSync::read. Events go to e when it is not null, at most MP_MAX_EVENTS per list
 * (the rest is consumed and counted in e->dropped). */
static inline void mp_read_player_events(mp_reader* r, mp_player_look* l, mp_anim_events* e) {
    mp_u32 count, slot;
    mp_anim_events scratch;
    if (!e) e = &scratch;
    mp_anim_events_clear(e);
    l->item = mp_read_u16(r);
    l->proc = mp_read_vlq(r);
    for (int i = 0; i != 5; ++i) l->colors[i] = mp_read_u32(r);
    for (int i = 0; i != 4; ++i) l->cursor[i] = mp_read_u8(r);
    count = mp_read_vlq(r); /* move anim: f32 f32 f32 vlq vlq vlq f32 */
    for (mp_u32 i = 0; r->ok && i != count; ++i) {
        mp_move_anim m;
        m.blend = mp_read_f32(r); m.rate = mp_read_f32(r); m.rate1 = mp_read_f32(r);
        m.anm0 = mp_read_vlq(r); m.anm1 = mp_read_vlq(r); m.mode = mp_read_vlq(r);
        m.morf = mp_read_f32(r);
        slot = mp_anim_slot(&e->move_count, &e->dropped);
        e->move[slot] = m;
    }
    count = mp_read_vlq(r); /* single move anim: vlq f32 f32 vlq f32 */
    for (mp_u32 i = 0; r->ok && i != count; ++i) {
        mp_single_anim s;
        s.anm = mp_read_vlq(r); s.rate = mp_read_f32(r); s.start = mp_read_f32(r);
        s.end = (mp_s16)mp_read_vlq(r); s.morf = mp_read_f32(r);
        slot = mp_anim_slot(&e->single_count, &e->dropped);
        e->single[slot] = s;
    }
    count = mp_read_vlq(r); /* act anime upper: vlq vlq f32 f32 vlq f32 */
    for (mp_u32 i = 0; r->ok && i != count; ++i) {
        mp_upper_anim u;
        u.bck = (mp_u16)mp_read_vlq(r); u.upper = mp_read_vlq(r);
        u.rate = mp_read_f32(r); u.start = mp_read_f32(r);
        u.end = (mp_s16)mp_read_vlq(r); u.morf = mp_read_f32(r);
        slot = mp_anim_slot(&e->upper_count, &e->dropped);
        e->upper[slot] = u;
    }
    count = mp_read_vlq(r); /* reset act anime upper: vlq f32 */
    for (mp_u32 i = 0; r->ok && i != count; ++i) {
        mp_reset_upper z;
        z.upper = mp_read_vlq(r); z.morf = mp_read_f32(r);
        slot = mp_anim_slot(&e->reset_count, &e->dropped);
        e->reset[slot] = z;
    }
    count = mp_read_vlq(r); /* equip sword: vlq */
    for (mp_u32 i = 0; r->ok && i != count; ++i) {
        mp_u32 b1 = mp_read_vlq(r);
        slot = mp_anim_slot(&e->sword_count, &e->dropped);
        e->sword[slot] = b1;
    }
    e->equip_item_count = mp_read_vlq(r); /* empty records */
    e->unequip_count = mp_read_vlq(r);
    count = mp_read_vlq(r); /* hit: vlq vlq vlq */
    for (mp_u32 i = 0; r->ok && i != count; ++i) { mp_read_vlq(r); mp_read_vlq(r); mp_read_vlq(r); }
    e->hit_count = count;
    /* No name when the packet ends here: a GameCube client or a mod before the extension
     * (ActorPlayerSync is the last component of a PlayerPuppet, like in ActorPlayerSync.java). */
    mp_u32 name_length = 0;
    const char* name = r->ok && r->pos < r->size ? mp_read_string(r, &name_length) : "";
    mp_set_name(l, name, name_length);
    mp_read_ship(r, &l->ship);
    /* the extras, the same way: a byte with bit 7 clear, else none (a sender before them) */
    l->extras = r->ok && r->pos < r->size && !(r->data[r->pos] & 0x80) ? (mp_u8)(mp_read_u8(r) & MP_EXTRAS) : 0;
    mp_read_swings(r, e); /* the swings, the same way (a sender before them: none) */
    mp_read_strikes(r, e); /* then the strikes */
    mp_read_particles(r, e); /* then the particles */
    /* then the skin, the same way (a string: its first byte has bit 7 clear) */
    mp_u32 skin_length = 0;
    const char* skin = r->ok && r->pos < r->size && !(r->data[r->pos] & 0x80) ? mp_read_string(r, &skin_length) : "";
    mp_set_skin(l, skin, skin_length);
    mp_read_death(r, l); /* then the death, the same way */
    mp_read_sounds(r, e); /* then Link's sounds (0.3.79) */
    if (!r->ok) mp_anim_events_clear(e);
}

static inline void mp_read_player(mp_reader* r, mp_player_look* l) {
    mp_read_player_events(r, l, 0);
}

/* ------------------------------------------------------------------ client packets */

/* CLIENT_PLAYER_CHANGE_MANAGER payload: the area string. WWClient::onGameRun formats
 * only the stage name ("%s" with an unused room argument). */
static inline void mp_write_change_manager(mp_writer* w, const char* stage) {
    mp_write_string(w, stage);
}

/* CLIENT_ACTOR_SYNC_CREATE for the local player (ActorSync::getOrCreate): no game UID,
 * solver ID, game and properties name "PlayerPuppet", then a full update of every
 * component of the PlayerPuppet factory. */
static inline void mp_write_player_create(mp_writer* w, const mp_session* s, mp_u32 solver,
                                          const mp_transform* t, const mp_player_look* l) {
    mp_write_u8(w, 0);
    mp_write_vlq(w, solver);
    mp_write_string(w, "PlayerPuppet");
    mp_write_string(w, "PlayerPuppet");
    mp_write_vlq(w, s->puppet_component_count);
    for (mp_u32 i = 0; i != s->puppet_component_count; ++i) {
        mp_write_vlq(w, i);
        if (s->puppet_components[i] == MP_COMPONENT_BASE) mp_write_base(w, t);
        else if (s->puppet_components[i] == MP_COMPONENT_PLAYER) mp_write_player(w, l);
        else w->ok = 0;
    }
}

/* CLIENT_ACTOR_SYNC_UPDATE with only ActorBaseSync invalidated (ActorSync::calcWriters):
 * VLQ uid, ticket byte, VLQ 1, VLQ component index, component data. */
static inline void mp_write_transform_update(mp_writer* w, const mp_session* s, mp_u32 uid,
                                             mp_u8 ticket, const mp_transform* t) {
    if (s->puppet_base_index == MP_NO_ID) {
        w->ok = 0;
        return;
    }
    mp_write_vlq(w, uid);
    mp_write_u8(w, ticket);
    mp_write_vlq(w, 1);
    mp_write_vlq(w, s->puppet_base_index);
    mp_write_base(w, t);
}

/* CLIENT_ACTOR_SYNC_UPDATE with only ActorPlayerSync invalidated (colours, item or proc changed,
 * or animation events recorded); events may be null. */
static inline void mp_write_player_update(mp_writer* w, const mp_session* s, mp_u32 uid, mp_u8 ticket,
                                          const mp_player_look* l, const mp_anim_events* e) {
    mp_u32 index = MP_NO_ID;
    for (mp_u32 i = 0; i != s->puppet_component_count; ++i)
        if (s->puppet_components[i] == MP_COMPONENT_PLAYER) index = i;
    if (index == MP_NO_ID) {
        w->ok = 0;
        return;
    }
    mp_write_vlq(w, uid);
    mp_write_u8(w, ticket);
    mp_write_vlq(w, 1);
    mp_write_vlq(w, index);
    mp_write_player_events(w, l, e);
}

static inline void mp_write_look_update(mp_writer* w, const mp_session* s, mp_u32 uid,
                                        mp_u8 ticket, const mp_player_look* l) {
    mp_write_player_update(w, s, uid, ticket, l, 0);
}

/* ------------------------------------------------------------------ server packets */

/* Decoded PlayerPuppet state. Other actor kinds are not decoded (frames are
 * length-delimited, so the caller simply skips them). */
typedef struct mp_actor_state {
    mp_u32 uid;
    mp_u32 owner;
    mp_u8 ticket;
    int has_solver;
    mp_u32 solver;
    int is_puppet;
    int has_transform;
    mp_transform transform;
    int has_look;
    mp_player_look look;
    mp_anim_events events; /* valid when has_look */
} mp_actor_state;

/* SyncedActor::readUpdate: ticket byte, VLQ owner, VLQ count, (VLQ index, data)*. */
static inline int mp_read_actor_update_body(mp_reader* r, const mp_session* s, mp_actor_state* a) {
    a->has_transform = 0;
    a->has_look = 0;
    mp_anim_events_clear(&a->events);
    a->ticket = mp_read_u8(r);
    a->owner = mp_read_vlq(r);
    mp_u32 count = mp_read_vlq(r);
    for (mp_u32 i = 0; r->ok && i != count; ++i) {
        mp_u32 index = mp_read_vlq(r);
        if (index >= s->puppet_component_count) return 0;
        if (s->puppet_components[index] == MP_COMPONENT_BASE) {
            mp_read_base(r, &a->transform);
            a->has_transform = 1;
        } else if (s->puppet_components[index] == MP_COMPONENT_PLAYER) {
            mp_read_player_events(r, &a->look, &a->events);
            a->has_look = 1;
        } else {
            return 0;
        }
    }
    return r->ok;
}

/* SERVER_ACTOR_SYNC_CREATE (ActorSync::create(DataInputStream)): VLQ uid, byte hasGameUID
 * [VLQ size, bytes], byte hasSolver [VLQ solver], name, properties name, then the update
 * body. Returns 1 with is_puppet = 0 for a well-formed header of another actor kind. */
static inline int mp_read_actor_create(const mp_u8* payload, mp_u32 size, const mp_session* s,
                                       mp_actor_state* a) {
    mp_reader r = mp_reader_make(payload, size);
    mp_u32 n;
    a->has_transform = 0;
    a->has_look = 0;
    mp_anim_events_clear(&a->events);
    a->ticket = 0xFF;
    a->owner = 0;
    a->uid = mp_read_vlq(&r);
    if (mp_read_u8(&r)) mp_skip(&r, mp_read_vlq(&r));
    a->has_solver = mp_read_u8(&r) != 0;
    a->solver = a->has_solver ? mp_read_vlq(&r) : MP_NO_ID;
    mp_read_string(&r, &n);
    const char* properties = mp_read_string(&r, &n);
    if (!r.ok) return 0;
    a->is_puppet = mp_string_equals(properties, n, "PlayerPuppet");
    if (!a->is_puppet) return 1;
    return mp_read_actor_update_body(&r, s, a) && r.pos == r.size;
}

/* SERVER_ACTOR_SYNC_UPDATE (ActorSync::receiveUpdatePacket): VLQ uid + update body. The
 * caller must only decode it for a uid it knows to be a PlayerPuppet. */
static inline mp_u32 mp_read_update_uid(const mp_u8* payload, mp_u32 size) {
    mp_reader r = mp_reader_make(payload, size);
    mp_u32 uid = mp_read_vlq(&r);
    return r.ok ? uid : MP_NO_ID;
}

static inline int mp_read_actor_update(const mp_u8* payload, mp_u32 size, const mp_session* s,
                                       mp_actor_state* a) {
    mp_reader r = mp_reader_make(payload, size);
    a->uid = mp_read_vlq(&r);
    return mp_read_actor_update_body(&r, s, a) && r.pos == r.size;
}

/* SERVER_ACTOR_SYNC_REMOVE: VLQ uid. */
static inline mp_u32 mp_read_actor_remove(const mp_u8* payload, mp_u32 size) {
    return mp_read_update_uid(payload, size);
}

/* ------------------------------------------------------------------ other synchronised actors */

/* ActorSpawnSync (WWHD extension): an actor the game created while playing (an item dropped by an
 * enemy, a bomb, an arrow, a boomerang), as the others create it again: its process name, the
 * fields of its parameter block (fopAcM_prm_class), its speeds after creation and one byte of the
 * actor's own state (variant: an arrow's element, daArrow_c::mArrowType).
 * Wire: u16 proc, u32 params, u8 argument, s16 angle x y z, u8 scale x y z, u8 room,
 * f32 speedF, f32 speed x y z, f32 gravity, u8 variant (38 bytes). The owner also sends it in an
 * update when the actor's parameters change (an arrow is shot when Link sets them to 1). */
typedef struct mp_spawn {
    mp_u16 proc;
    mp_u32 params;
    mp_u8 argument;
    mp_s16 angle[3];
    mp_u8 scale[3];
    mp_u8 room;
    float speed_f;
    float speed[3];
    float gravity;
    mp_u8 variant;
} mp_spawn;

/* What the guest applies of an actor's components: ActorBaseSync, ActorHealthSync, ActorSpawnSync. */
typedef struct mp_actor_values {
    int has_transform;
    mp_transform transform;
    int has_health;
    mp_s16 health;     /* fopAc_ac_c::health, written as a short like ActorHealthSync */
    mp_u8 tag;         /* WWHD: the owner's elite draw (elite.h), 0 none; see mp_health_word */
    mp_u32 status;     /* fopAc_ac_c::actor_status */
    int has_spawn;     /* in a create, or in an update when the owner's parameters changed */
    mp_spawn spawn;
} mp_actor_values;

static inline void mp_write_spawn(mp_writer* w, const mp_spawn* p) {
    mp_write_u16(w, p->proc);
    mp_write_u32(w, p->params);
    mp_write_u8(w, p->argument);
    for (int i = 0; i != 3; ++i) mp_write_u16(w, (mp_u16)p->angle[i]);
    for (int i = 0; i != 3; ++i) mp_write_u8(w, p->scale[i]);
    mp_write_u8(w, p->room);
    mp_write_f32(w, p->speed_f);
    for (int i = 0; i != 3; ++i) mp_write_f32(w, p->speed[i]);
    mp_write_f32(w, p->gravity);
    mp_write_u8(w, p->variant);
}

static inline void mp_read_spawn(mp_reader* r, mp_spawn* p) {
    p->proc = mp_read_u16(r);
    p->params = mp_read_u32(r);
    p->argument = mp_read_u8(r);
    for (int i = 0; i != 3; ++i) p->angle[i] = (mp_s16)mp_read_u16(r);
    for (int i = 0; i != 3; ++i) p->scale[i] = mp_read_u8(r);
    p->room = mp_read_u8(r);
    p->speed_f = mp_read_f32(r);
    for (int i = 0; i != 3; ++i) p->speed[i] = mp_read_f32(r);
    p->gravity = mp_read_f32(r);
    p->variant = mp_read_u8(r);
}

/* ActorHealthSync's health is a short, the game's an s8 (fopAc_ac_c::health): the high byte only
 * repeats the sign (0x00 or 0xFF). WWHD puts the owner's elite draw there (elite.h EL_TAG_*, never
 * 0x00 or 0xFF) with the health in the low byte; the GameCube client keeps the low byte
 * (ActorHealthSync::calc, gameActor->health = mHealth). */
static inline mp_u16 mp_health_word(mp_s16 health, mp_u8 tag) {
    if (!tag || tag == 0xFF) return (mp_u16)health;
    return (mp_u16)(((mp_u16)tag << 8) | (mp_u8)health);
}

static inline void mp_health_split(mp_u16 word, mp_s16* health, mp_u8* tag) {
    mp_u8 high = (mp_u8)(word >> 8);
    if (high == 0x00 || high == 0xFF) {
        *health = (mp_s16)word;
        *tag = 0;
    } else {
        *health = (mp_s16)(signed char)(word & 0xFF);
        *tag = high;
    }
}

static inline void mp_write_component(mp_writer* w, mp_u8 kind, const mp_actor_values* v) {
    if (kind == MP_COMPONENT_BASE) {
        mp_write_base(w, &v->transform);
    } else if (kind == MP_COMPONENT_HEALTH) {
        mp_write_u16(w, mp_health_word(v->health, v->tag));
        mp_write_u32(w, v->status);
    } else if (kind == MP_COMPONENT_SPAWN) {
        mp_write_spawn(w, &v->spawn);
    } else {
        mp_u32 size = mp_component_size(kind);
        if (!size) w->ok = 0;
        for (mp_u32 i = 0; i != size; ++i) mp_write_u8(w, 0); /* not applied by the guest */
    }
}

/* SyncedActor::writeUpdate: VLQ count, then (VLQ index, data) for every component (full) or for
 * the first ActorBaseSync / ActorHealthSync / ActorSpawnSync the values carry. */
static inline int mp_component_wanted(const mp_factory* f, mp_u32 i, int full, const mp_actor_values* v) {
    if (full) return 1;
    if (v->has_transform && i == f->base_index) return 1;
    if (v->has_health && i == f->health_index) return 1;
    if (v->has_spawn && f->kinds[i] == MP_COMPONENT_SPAWN) {
        for (mp_u32 k = 0; k != i; ++k)
            if (f->kinds[k] == MP_COMPONENT_SPAWN) return 0;
        return 1;
    }
    return 0;
}

static inline void mp_write_actor_components(mp_writer* w, const mp_factory* f, int full,
                                             const mp_actor_values* v) {
    if (!f->usable) {
        w->ok = 0;
        return;
    }
    mp_u32 count = 0;
    for (mp_u32 i = 0; i != f->count; ++i)
        if (mp_component_wanted(f, i, full, v)) ++count;
    mp_write_vlq(w, count);
    for (mp_u32 i = 0; i != f->count; ++i) {
        if (!mp_component_wanted(f, i, full, v)) continue;
        mp_write_vlq(w, i);
        mp_write_component(w, f->kinds[i], v);
    }
}

/* CLIENT_ACTOR_SYNC_CREATE (ActorSync::getOrCreate): byte hasGameUID [VLQ size, game UID], VLQ
 * solver, game name, properties name (the factory), full update. A placed actor has a game UID and
 * its name as both names; a spawned one none, and the factory MP_SPAWN_FACTORY. */
#define MP_SPAWN_FACTORY "Spawned"

static inline void mp_write_name8(mp_writer* w, const char* name) {
    char text[MP_ACTOR_NAME_MAX + 1];
    for (mp_u32 i = 0; i != MP_ACTOR_NAME_MAX + 1; ++i) text[i] = 0;
    for (mp_u32 i = 0; i != MP_ACTOR_NAME_MAX && name[i]; ++i) text[i] = name[i];
    mp_write_string(w, text);
}

static inline void mp_write_actor_create_named(mp_writer* w, const mp_factory* f, mp_u32 solver,
                                               const mp_u8* game_uid, mp_u32 game_uid_size, const char* name,
                                               const char* properties, const mp_actor_values* v) {
    mp_write_u8(w, game_uid_size != 0);
    if (game_uid_size) {
        mp_write_vlq(w, game_uid_size);
        for (mp_u32 i = 0; i != game_uid_size; ++i) mp_write_u8(w, game_uid[i]);
    }
    mp_write_vlq(w, solver);
    mp_write_name8(w, name);
    mp_write_name8(w, properties);
    mp_write_actor_components(w, f, 1, v);
}

static inline void mp_write_actor_create(mp_writer* w, const mp_factory* f, mp_u32 solver,
                                         const mp_u8* game_uid, mp_u32 game_uid_size, const char* name,
                                         const mp_actor_values* v) {
    mp_write_actor_create_named(w, f, solver, game_uid, game_uid_size, name, name, v);
}

/* CLIENT_ACTOR_SYNC_UPDATE of a placed actor: VLQ uid, ticket, the components the values carry. */
static inline void mp_write_actor_update(mp_writer* w, const mp_factory* f, mp_u32 uid, mp_u8 ticket,
                                         const mp_actor_values* v) {
    mp_write_vlq(w, uid);
    mp_write_u8(w, ticket);
    mp_write_actor_components(w, f, 0, v);
}

/* CLIENT_ACTOR_SYNC_REMOVE, CLIENT_ACTOR_EXCLUSIVE_ADD and _REMOVE: VLQ uid. */
static inline void mp_write_actor_uid(mp_writer* w, mp_u32 uid) {
    mp_write_vlq(w, uid);
}

typedef struct mp_placed_actor {
    mp_u32 uid;
    mp_u32 owner;      /* the owner player's PlayerPuppet uid, 0 for none */
    mp_u8 ticket;
    int has_solver;
    mp_u32 solver;
    mp_u32 game_uid_size; /* 0: none, or longer than MP_GAME_UID_MAX */
    mp_u8 game_uid[MP_GAME_UID_MAX];
    char name[MP_ACTOR_NAME_MAX + 1]; /* properties name */
    mp_u32 factory;    /* index in the session, MP_NO_ID: no usable factory (body not read) */
    mp_actor_values values;
} mp_placed_actor;

static inline int mp_read_actor_body(mp_reader* r, const mp_factory* f, mp_placed_actor* a) {
    a->values.has_transform = a->values.has_health = a->values.has_spawn = 0;
    a->ticket = mp_read_u8(r);
    a->owner = mp_read_vlq(r);
    mp_u32 count = mp_read_vlq(r);
    for (mp_u32 i = 0; r->ok && i != count; ++i) {
        mp_u32 index = mp_read_vlq(r);
        if (!r->ok || index >= f->count) return 0;
        mp_u8 kind = f->kinds[index];
        if (kind == MP_COMPONENT_BASE) {
            mp_read_base(r, &a->values.transform);
            a->values.has_transform = 1;
        } else if (kind == MP_COMPONENT_HEALTH) {
            mp_health_split(mp_read_u16(r), &a->values.health, &a->values.tag);
            a->values.status = mp_read_u32(r);
            a->values.has_health = 1;
        } else if (kind == MP_COMPONENT_SPAWN) {
            mp_read_spawn(r, &a->values.spawn);
            a->values.has_spawn = 1;
        } else {
            mp_u32 size = mp_component_size(kind);
            if (!size) return 0;
            mp_skip(r, size);
        }
    }
    return r->ok;
}

/* SERVER_ACTOR_SYNC_CREATE of an actor that is not a PlayerPuppet. Returns 0 when malformed. */
static inline int mp_read_placed_create(const mp_u8* payload, mp_u32 size, const mp_session* s,
                                        mp_placed_actor* a) {
    mp_reader r = mp_reader_make(payload, size);
    mp_u32 n;
    a->values.has_transform = a->values.has_health = a->values.has_spawn = 0;
    a->ticket = 0xFF;
    a->owner = 0;
    a->uid = mp_read_vlq(&r);
    a->game_uid_size = 0;
    if (mp_read_u8(&r)) {
        mp_u32 length = mp_read_vlq(&r);
        if (r.ok && length <= MP_GAME_UID_MAX && length <= r.size - r.pos) {
            for (mp_u32 i = 0; i != length; ++i) a->game_uid[i] = r.data[r.pos + i];
            a->game_uid_size = length;
        }
        mp_skip(&r, length);
    }
    a->has_solver = mp_read_u8(&r) != 0;
    a->solver = a->has_solver ? mp_read_vlq(&r) : MP_NO_ID;
    mp_read_string(&r, &n);
    const char* properties = mp_read_string(&r, &n);
    if (!r.ok) return 0;
    for (mp_u32 i = 0; i != MP_ACTOR_NAME_MAX + 1; ++i) a->name[i] = (i < n && n <= MP_ACTOR_NAME_MAX) ? properties[i] : 0;
    a->factory = n <= MP_ACTOR_NAME_MAX ? mp_find_factory(s, a->name) : MP_NO_ID;
    if (a->factory != MP_NO_ID && !s->factories[a->factory].usable) a->factory = MP_NO_ID;
    if (a->factory == MP_NO_ID) return 1;
    return mp_read_actor_body(&r, &s->factories[a->factory], a) && r.pos == r.size;
}

/* SERVER_ACTOR_SYNC_UPDATE of a placed actor whose factory the caller knows. */
static inline int mp_read_placed_update(const mp_u8* payload, mp_u32 size, const mp_factory* f,
                                        mp_placed_actor* a) {
    mp_reader r = mp_reader_make(payload, size);
    a->uid = mp_read_vlq(&r);
    return mp_read_actor_body(&r, f, a) && r.pos == r.size;
}

/* SERVER_MUTATORS (WWHD extension): VLQ enemy multiplier, u8 randomizer, then (0.2.47 servers)
 * u8 dynamic scaling and u8 elite chance (percent), then (0.3.13 servers) u8 pvp, then (0.3.92) u8
 * shared life. The server sends it to every client when its mutators change while it runs. A body
 * without the last ones (an older server) leaves them at 0; 0 when malformed. */
static inline int mp_read_mutators_life(const mp_u8* payload, mp_u32 size, mp_u32* multiplier, int* randomizer,
                                        int* scaling, mp_u32* elite, int* pvp, int* shared_life) {
    mp_reader r = mp_reader_make(payload, size);
    mp_u32 m = mp_read_vlq(&r);
    mp_u8 random = mp_read_u8(&r);
    mp_u8 scale = 0, chance = 0, fight = 0;
    if (r.ok && r.pos != r.size) {
        scale = mp_read_u8(&r);
        chance = mp_read_u8(&r);
    }
    if (r.ok && r.pos != r.size) fight = mp_read_u8(&r);
    mp_u8 life = 0;
    if (r.ok && r.pos != r.size) life = mp_read_u8(&r);
    if (!r.ok || r.pos != r.size) return 0;
    if (m < 1) m = 1;
    if (m > MP_MULTIPLIER_MAX) m = MP_MULTIPLIER_MAX;
    *multiplier = m;
    *randomizer = random != 0;
    *scaling = scale != 0;
    *elite = chance > MP_ELITE_CHANCE_MAX ? MP_ELITE_CHANCE_MAX : chance;
    *pvp = fight != 0;
    *shared_life = life != 0;
    return 1;
}

static inline int mp_read_mutators_ex(const mp_u8* payload, mp_u32 size, mp_u32* multiplier, int* randomizer,
                                      int* scaling, mp_u32* elite, int* pvp) {
    int life;
    return mp_read_mutators_life(payload, size, multiplier, randomizer, scaling, elite, pvp, &life);
}

static inline int mp_read_mutators(const mp_u8* payload, mp_u32 size, mp_u32* multiplier, int* randomizer) {
    int scaling, pvp;
    mp_u32 elite;
    return mp_read_mutators_ex(payload, size, multiplier, randomizer, &scaling, &elite, &pvp);
}

/* ------------------------------------------------------------------ player directory */

/* WWHD extension: where every player is, whatever their area (the actor sync only shows the
 * players of one's own area). For the launcher's world map and its teleport (mod.c map_step).
 *
 * CLIENT_PLAYER_DIRECTORY (about once a second): this player's entry without its id:
 *   string name, u16 colour (RGB565, primary tunic), string stage (dStage_startStage_c name),
 *   s16 start point, s8 start room, s8 start layer (how the player entered that stage: a point
 *   that exists, for a teleport there), s8 current room, f32 x, y, z, s16 heading, u8 flags,
 *   then with MP_DIR_SEA the last place on the Great Sea: f32 x, z. MP_DIR_DEAD (no field): the
 *   player is dead (a ghost, or the game's own death under way: death.h); one cannot teleport to
 *   them. A reader before 0.2.71 refuses a directory holding that flag.
 * SERVER_PLAYER_DIRECTORY (the answer, WWClient.java): VLQ id of the receiver, VLQ count, then
 *   per player VLQ id and its entry. Ids are the server's, stable while the client is connected. */
#define MP_DIR_MAX 24
#define MP_DIR_STAGE 8
#define MP_DIR_SEA 0x01 /* the last place on the Great Sea follows */
#define MP_DIR_DEAD 0x02 /* dead: a ghost or dying */
#define MP_DIR_FLAGS (MP_DIR_SEA | MP_DIR_DEAD)

typedef struct mp_dir_entry {
    mp_u32 id;
    char name[MP_NAME_MAX + 1];
    mp_u16 color;
    char stage[MP_DIR_STAGE + 1];
    mp_s16 point;
    signed char start_room, layer, room;
    float pos[3];
    mp_s16 heading;
    mp_u8 flags;
    float sea[2];
} mp_dir_entry;

typedef struct mp_directory {
    mp_u32 self;
    mp_u32 count;
    mp_dir_entry entry[MP_DIR_MAX];
} mp_directory;

static inline void mp_write_dir_entry(mp_writer* w, const mp_dir_entry* e) {
    mp_write_string(w, e->name);
    mp_write_u16(w, e->color);
    mp_write_string(w, e->stage);
    mp_write_u16(w, (mp_u16)e->point);
    mp_write_u8(w, (mp_u8)e->start_room);
    mp_write_u8(w, (mp_u8)e->layer);
    mp_write_u8(w, (mp_u8)e->room);
    for (int i = 0; i != 3; ++i) mp_write_f32(w, e->pos[i]);
    mp_write_u16(w, (mp_u16)e->heading);
    mp_write_u8(w, (mp_u8)(e->flags & MP_DIR_FLAGS));
    if (e->flags & MP_DIR_SEA) {
        mp_write_f32(w, e->sea[0]);
        mp_write_f32(w, e->sea[1]);
    }
}

static inline void mp_copy_text(char* out, mp_u32 capacity, const char* text, mp_u32 length) {
    mp_u32 n = 0;
    /* at most length characters, up to a NUL, printable ASCII (others become '?') */
    for (; n != length && n + 1 != capacity && text[n]; ++n) out[n] = text[n] >= 0x20 && text[n] <= 0x7E ? text[n] : '?';
    for (; n != capacity; ++n) out[n] = 0;
}

static inline void mp_read_dir_entry(mp_reader* r, mp_dir_entry* e) {
    mp_u32 n;
    const char* text = mp_read_string(r, &n);
    mp_copy_text(e->name, MP_NAME_MAX + 1, text, n);
    e->color = mp_read_u16(r);
    text = mp_read_string(r, &n);
    mp_copy_text(e->stage, MP_DIR_STAGE + 1, text, n);
    e->point = (mp_s16)mp_read_u16(r);
    e->start_room = (signed char)mp_read_u8(r);
    e->layer = (signed char)mp_read_u8(r);
    e->room = (signed char)mp_read_u8(r);
    for (int i = 0; i != 3; ++i) e->pos[i] = mp_read_f32(r);
    e->heading = (mp_s16)mp_read_u16(r);
    e->flags = mp_read_u8(r);
    if (e->flags & ~(mp_u32)MP_DIR_FLAGS) r->ok = 0;
    e->sea[0] = e->sea[1] = 0.0f;
    if (e->flags & MP_DIR_SEA) {
        e->sea[0] = mp_read_f32(r);
        e->sea[1] = mp_read_f32(r);
    }
}

/* SERVER_PLAYER_DIRECTORY; 0 when malformed. Entries past MP_DIR_MAX are read and dropped. */
static inline int mp_read_directory(const mp_u8* payload, mp_u32 size, mp_directory* d) {
    mp_reader r = mp_reader_make(payload, size);
    d->self = mp_read_vlq(&r);
    mp_u32 count = mp_read_vlq(&r);
    d->count = 0;
    for (mp_u32 i = 0; r.ok && i != count; ++i) {
        mp_dir_entry e;
        e.id = mp_read_vlq(&r);
        mp_read_dir_entry(&r, &e);
        if (r.ok && d->count != MP_DIR_MAX) d->entry[d->count++] = e;
    }
    return r.ok && r.pos == r.size;
}

/* Splits one received transport item (VLQ id + VLQ length + payload). */
static inline int mp_split_frame(const mp_u8* frame, mp_u32 size, mp_u32* id,
                                 const mp_u8** payload, mp_u32* length) {
    mp_reader r = mp_reader_make(frame, size);
    *id = mp_read_vlq(&r);
    *length = mp_read_vlq(&r);
    if (!r.ok || *length != r.size - r.pos) return 0;
    *payload = frame + r.pos;
    return 1;
}

/* ------------------------------------------------------------------ shop summons */

/* WWHD extension: the launcher's shop (mod.c shop_*) buys a creature for another player of the
 * directory; that player's game makes it next to its Link (actor_sync.h as_summon).
 * CLIENT_PLAYER_SUMMON: VLQ the target's directory id, u8 creature (the shop's number), string the
 *   buyer's name.
 * SERVER_PLAYER_SUMMON (WWClient.java, to the target only): u8 creature, string the buyer's name. */
#define MP_SUMMON_CREATURES 32

static inline void mp_write_summon_request(mp_writer* w, mp_u32 target, mp_u8 creature, const char* buyer) {
    mp_write_vlq(w, target);
    mp_write_u8(w, creature);
    mp_write_string(w, buyer);
}

/* SERVER_PLAYER_SUMMON; 0 when malformed (buyer: MP_NAME_MAX + 1 bytes, printable). */
static inline int mp_read_summon(const mp_u8* payload, mp_u32 size, mp_u8* creature, char* buyer) {
    mp_reader r = mp_reader_make(payload, size);
    *creature = mp_read_u8(&r);
    mp_u32 n;
    const char* text = mp_read_string(&r, &n);
    if (!r.ok || r.pos != r.size || *creature >= MP_SUMMON_CREATURES) return 0;
    mp_copy_text(buyer, MP_NAME_MAX + 1, text, n);
    return 1;
}

/* ------------------------------------------------------------------ shop payments */

/* WWHD extension (0.3.103): the launcher's shop pays rupees to another player of the directory.
 * CLIENT_PLAYER_PAY: VLQ the target's directory id, VLQ rupees (1 to MP_PAY_MAX), VLQ the payment's
 *   number (the launcher's shop.cfg id: a payment delivered twice is credited once), string the
 *   payer's name.
 * SERVER_PLAYER_PAY (WWClient.java, to the target only): VLQ rupees, VLQ the number, string the
 *   payer's name. The payer's game took the rupees already. */
#define MP_PAY_MAX 5000u /* the biggest wallet */

static inline void mp_write_pay_request(mp_writer* w, mp_u32 target, mp_u32 rupees, mp_u32 number, const char* payer) {
    mp_write_vlq(w, target);
    mp_write_vlq(w, rupees);
    mp_write_vlq(w, number);
    mp_write_string(w, payer);
}

/* SERVER_PLAYER_PAY; 0 when malformed (payer: MP_NAME_MAX + 1 bytes, printable). */
static inline int mp_read_pay(const mp_u8* payload, mp_u32 size, mp_u32* rupees, mp_u32* number, char* payer) {
    mp_reader r = mp_reader_make(payload, size);
    *rupees = mp_read_vlq(&r);
    *number = mp_read_vlq(&r);
    mp_u32 n;
    const char* text = mp_read_string(&r, &n);
    if (!r.ok || r.pos != r.size || *rupees == 0 || *rupees > MP_PAY_MAX) return 0;
    mp_copy_text(payer, MP_NAME_MAX + 1, text, n);
    return 1;
}

/* ------------------------------------------------------------------ revive */

/* WWHD extension: a player who held A by a dead player's body for its time (death.h) revives it.
 * CLIENT_PLAYER_REVIVE: VLQ the dead player's directory id (its death block), string the reviver's
 * name. SERVER_PLAYER_REVIVE (WWClient.java, to that player only): string the reviver's name. */
static inline void mp_write_revive_request(mp_writer* w, mp_u32 target, const char* reviver) {
    mp_write_vlq(w, target);
    mp_write_string(w, reviver);
}

/* SERVER_PLAYER_REVIVE; 0 when malformed (reviver: MP_NAME_MAX + 1 bytes, printable). */
static inline int mp_read_revive(const mp_u8* payload, mp_u32 size, char* reviver) {
    mp_reader r = mp_reader_make(payload, size);
    mp_u32 n;
    const char* text = mp_read_string(&r, &n);
    if (!r.ok || r.pos != r.size) return 0;
    mp_copy_text(reviver, MP_NAME_MAX + 1, text, n);
    return 1;
}

/* ------------------------------------------------------------------ carry */

/* WWHD extension: a player carries another (carry.h). CLIENT_PLAYER_CARRY: VLQ the carried (or
 * carrying) player's ActorPlayerSync uid, VLQ the sender's own uid, u8 action (MP_CARRY_*), u16
 * the sender's Link's facing (shape_angle.y). The server (WWClient.java) sends the same body as
 * SERVER_PLAYER_CARRY to every other client; the one whose player has that uid acts on it, the
 * others note who carries whom. */
#define MP_CARRY_LIFT 0    /* sender lifts the target */
#define MP_CARRY_PUT 1     /* sender puts the target down (or drops it) */
#define MP_CARRY_THROW 2   /* sender throws the target forward, the facing given */
#define MP_CARRY_JUMP 3    /* sender, carried by the target, jumped off */
#define MP_CARRY_ACTIONS 4

typedef struct mp_carry {
    mp_u32 target;
    mp_u32 sender;
    mp_u8 action;
    mp_s16 angle;
} mp_carry;

static inline void mp_write_carry(mp_writer* w, const mp_carry* c) {
    mp_write_vlq(w, c->target);
    mp_write_vlq(w, c->sender);
    mp_write_u8(w, c->action);
    mp_write_u16(w, (mp_u16)c->angle);
}

/* SERVER_PLAYER_CARRY (and CLIENT_PLAYER_CARRY, the same body); 0 when malformed. */
static inline int mp_read_carry(const mp_u8* payload, mp_u32 size, mp_carry* c) {
    mp_reader r = mp_reader_make(payload, size);
    c->target = mp_read_vlq(&r);
    c->sender = mp_read_vlq(&r);
    c->action = mp_read_u8(&r);
    c->angle = (mp_s16)mp_read_u16(&r);
    return r.ok && r.pos == r.size && c->action < MP_CARRY_ACTIONS && c->target != c->sender;
}

/* ------------------------------------------------------------------ cutscenes */

/* WWHD extension: a stage cutscene started in one game plays in the games of the players of the
 * same stage and room (event_sync.h). CLIENT_PLAYER_EVENT: VLQ the sender's ActorPlayerSync uid,
 * string the stage (at most 8), u8 the room (s8), string the event's name (at most 31: dEvDtEvent_c
 * mName), then optionally (0.3.32) the sender's place when it started: f32 x, y, z, s16 heading
 * (a body without it stays valid). The server (WWClient.java) sends the same body as
 * SERVER_PLAYER_EVENT to every other client. */
#define MP_EVENT_STAGE_MAX 8
#define MP_EVENT_NAME_MAX 31

typedef struct mp_event {
    mp_u32 sender;
    char stage[MP_EVENT_STAGE_MAX + 1];
    signed char room;
    char name[MP_EVENT_NAME_MAX + 1];
    int has_place;
    float pos[3];
    mp_s16 heading;
} mp_event;

static inline void mp_write_event(mp_writer* w, const mp_event* e) {
    mp_write_vlq(w, e->sender);
    mp_write_string(w, e->stage);
    mp_write_u8(w, (mp_u8)e->room);
    mp_write_string(w, e->name);
    if (!e->has_place) return;
    for (int i = 0; i != 3; ++i) mp_write_f32(w, e->pos[i]);
    mp_write_u16(w, (mp_u16)e->heading);
}

/* SERVER_PLAYER_EVENT (and CLIENT_PLAYER_EVENT, the same body); 0 when malformed (too long, empty
 * name, a character out of printable ASCII). */
static inline int mp_read_event(const mp_u8* payload, mp_u32 size, mp_event* e) {
    mp_reader r = mp_reader_make(payload, size);
    mp_u32 sn = 0, nn = 0; /* mp_read_string leaves them unset on a cut body */
    e->sender = mp_read_vlq(&r);
    const char* stage = mp_read_string(&r, &sn);
    e->room = (signed char)mp_read_u8(&r);
    const char* name = mp_read_string(&r, &nn);
    e->has_place = r.ok && r.pos != r.size;
    if (e->has_place) {
        for (int i = 0; i != 3; ++i) e->pos[i] = mp_read_f32(&r);
        e->heading = (mp_s16)mp_read_u16(&r);
    }
    if (!r.ok || r.pos != r.size || sn == 0 || sn > MP_EVENT_STAGE_MAX || nn == 0 || nn > MP_EVENT_NAME_MAX) return 0;
    for (mp_u32 i = 0; i != sn; ++i) if (stage[i] < 0x20 || stage[i] > 0x7E) return 0;
    for (mp_u32 i = 0; i != nn; ++i) if (name[i] < 0x20 || name[i] > 0x7E) return 0;
    for (mp_u32 i = 0; i != sn; ++i) e->stage[i] = stage[i];
    e->stage[sn] = 0;
    for (mp_u32 i = 0; i != nn; ++i) e->name[i] = name[i];
    e->name[nn] = 0;
    return 1;
}

/* ------------------------------------------------------------------ save sync */

/* The SaveServer (save/SaveServer.java, the GameCube client's SaveServer.cpp): the shared save is
 * dSv_save_c (0x778 bytes, d_save.h). Its map gives runs of values of one type: type 0 byte,
 * 1 short, 2 int, 3 long, 4 float, 5 Vector3, 6 string (the run is one string of that length; the
 * server also writes runs of length 0 after strings, which hold nothing), 7 bits (bytes of eight
 * flags). An element is one value of a run, sent whole: its byte offset in dSv_save_c, then
 * CLIENT_SAVE_UPDATE / CLIENT_SAVE_MERGE: VLQ count, then per element VLQ offset and its bytes
 *   (a string up to its NUL, the NUL included);
 * SERVER_SAVE_UPDATE: VLQ count, then per element VLQ offset, VLQ length and its bytes;
 * SERVER_SAVE_FROM (WWHD): string, the name of the player who made the next SERVER_SAVE_UPDATE. */
#define MP_SAVE_BYTES 0x778
#define MP_SAVE_STRING 6

typedef struct mp_save_element {
    mp_u16 offset;
    mp_u8 size;   /* bytes (a string: its whole room) */
    mp_u8 type;
} mp_save_element;

static inline mp_u32 mp_save_type_size(mp_u8 type) {
    static const mp_u8 sizes[8] = {1, 2, 4, 8, 4, 12, 0, 1};
    return type < 8 ? sizes[type] : 0;
}

/* The elements of the map in order; the count, 0 when the map is unusable (an unknown type, a run
 * not a whole number of its values, more than max elements, or past MP_SAVE_BYTES). */
static inline mp_u32 mp_save_elements(const mp_session* s, mp_save_element* out, mp_u32 max) {
    mp_u32 n = 0, offset = 0;
    for (mp_u32 i = 0; i != s->save_map_count; ++i) {
        mp_u8 type = s->save_map_type[i];
        mp_u32 length = s->save_map_length[i];
        if (!length) continue;
        if (type == MP_SAVE_STRING) {
            if (n == max || offset + length > MP_SAVE_BYTES) return 0;
            out[n].offset = (mp_u16)offset;
            out[n].size = (mp_u8)length;
            out[n].type = type;
            ++n;
            offset += length;
            continue;
        }
        mp_u32 size = mp_save_type_size(type);
        if (!size || length % size) return 0;
        for (mp_u32 k = 0; k != length / size; ++k) {
            if (n == max || offset + size > MP_SAVE_BYTES) return 0;
            out[n].offset = (mp_u16)offset;
            out[n].size = (mp_u8)size;
            out[n].type = type;
            ++n;
            offset += size;
        }
    }
    return n;
}

/* One element of a CLIENT_SAVE_UPDATE / MERGE body (after its VLQ count): value is the element's
 * bytes in dSv_save_c. */
static inline void mp_write_save_element(mp_writer* w, const mp_save_element* e, const mp_u8* value) {
    mp_write_vlq(w, e->offset);
    if (e->type == MP_SAVE_STRING) {
        mp_u32 n = 0;
        while (n + 1 < e->size && value[n]) ++n;
        for (mp_u32 i = 0; i != n; ++i) mp_write_u8(w, value[i]);
        mp_write_u8(w, 0);
        return;
    }
    for (mp_u32 i = 0; i != e->size; ++i) mp_write_u8(w, value[i]);
}

/* SERVER_SAVE_UPDATE, one entry at a time: the count first, then each entry; 0 at a bad one. */
static inline mp_u32 mp_read_save_count(mp_reader* r) { return mp_read_vlq(r); }
static inline int mp_read_save_entry(mp_reader* r, mp_u32* offset, mp_u32* length, const mp_u8** bytes) {
    *offset = mp_read_vlq(r);
    *length = mp_read_vlq(r);
    if (!r->ok || *length > MP_SAVE_BYTES || *offset > MP_SAVE_BYTES || *offset + *length > MP_SAVE_BYTES ||
        *length > r->size - r->pos) {
        r->ok = 0;
        return 0;
    }
    *bytes = r->data + r->pos;
    r->pos += *length;
    return 1;
}

/* SERVER_SAVE_FROM: the name (MP_NAME_MAX + 1 bytes, printable); 0 when malformed. */
static inline int mp_read_save_from(const mp_u8* payload, mp_u32 size, char* name) {
    mp_reader r = mp_reader_make(payload, size);
    mp_u32 n;
    const char* text = mp_read_string(&r, &n);
    if (!r.ok || r.pos != r.size) return 0;
    mp_copy_text(name, MP_NAME_MAX + 1, text, n);
    return 1;
}

/* ------------------------------------------------------------------ world */

/* SERVER_WORLD (WWHD, ActorFilter.java): u8 flags, f32 time of day (degrees, 0-360), u16 day,
 * u8 weather (0 the game's own, 1 sunny, 2 cloudy, 3 rain, 4 heavy rain, 5 storm). */
#define MP_WORLD_TIME 1
#define MP_WORLD_WEATHER 2
#define MP_WORLD_EXPLICIT 4
#define MP_WORLD_FLAGS 7
#define MP_WEATHER_MAX 5

typedef struct mp_world {
    mp_u8 flags;
    float time;
    mp_u16 day;
    mp_u8 weather;
} mp_world;

static inline int mp_read_world(const mp_u8* payload, mp_u32 size, mp_world* w) {
    mp_reader r = mp_reader_make(payload, size);
    w->flags = mp_read_u8(&r);
    w->time = mp_read_f32(&r);
    w->day = mp_read_u16(&r);
    w->weather = mp_read_u8(&r);
    if (!r.ok || r.pos != r.size || (w->flags & ~MP_WORLD_FLAGS) || w->weather > MP_WEATHER_MAX) return 0;
    if (!(w->time >= 0.0f && w->time < 360.0f)) return 0;
    return 1;
}

/* CLIENT_WORLD (WWHD, ActorFilter.java): the same body, from a player sailing where time passes
 * (world_sync.h): flags MP_WORLD_TIME, and MP_WORLD_WEATHER with the weather its dice made. The
 * server follows one such player at a time and sends the world to the others. */
static inline void mp_write_world(mp_writer* w, const mp_world* world) {
    mp_write_u8(w, world->flags);
    mp_write_f32(w, world->time);
    mp_write_u16(w, world->day);
    mp_write_u8(w, world->weather);
}

