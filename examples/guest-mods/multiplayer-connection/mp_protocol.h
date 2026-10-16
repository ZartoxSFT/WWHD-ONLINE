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
    MP_COMPONENT_BASE = 1,   /* ActorBaseSync */
    MP_COMPONENT_PLAYER = 2, /* ActorPlayerSync */
};

typedef struct mp_session {
    /* server -> client (index in the first init list) */
    mp_u32 server_create, server_update, server_remove;
    /* client -> server (index in the second init list) */
    mp_u32 client_init, client_change_manager, client_leave_manager;
    mp_u32 client_create, client_update, client_remove;
    /* "PlayerPuppet" factory from the ActorSync initializer, in component index order */
    mp_u32 puppet_component_count;
    mp_u8 puppet_components[MP_MAX_COMPONENTS];
    mp_u32 puppet_base_index;
    int has_actor_sync;
} mp_session;

static inline void mp_assign_id(const char* s, mp_u32 n, mp_u32 index, const char* name, mp_u32* id) {
    if (mp_string_equals(s, n, name)) *id = index;
}

/* ActorSync::readSyncTable: VLQ factories, each = name + VLQ components + names. */
static inline int mp_read_actor_sync(mp_reader* r, mp_session* s) {
    mp_u32 factories = mp_read_vlq(r);
    for (mp_u32 f = 0; r->ok && f != factories; ++f) {
        mp_u32 n;
        const char* factory = mp_read_string(r, &n);
        int puppet = mp_string_equals(factory, n, "PlayerPuppet");
        mp_u32 count = mp_read_vlq(r);
        if (puppet) {
            if (count > MP_MAX_COMPONENTS) return 0;
            s->puppet_component_count = count;
            s->puppet_base_index = MP_NO_ID;
        }
        for (mp_u32 c = 0; r->ok && c != count; ++c) {
            const char* component = mp_read_string(r, &n);
            if (!puppet) continue;
            mp_u8 kind = MP_COMPONENT_UNSUPPORTED;
            if (mp_string_equals(component, n, "ActorBaseSync")) kind = MP_COMPONENT_BASE;
            else if (mp_string_equals(component, n, "ActorPlayerSync")) kind = MP_COMPONENT_PLAYER;
            else return 0;
            s->puppet_components[c] = kind;
            if (kind == MP_COMPONENT_BASE && s->puppet_base_index == MP_NO_ID)
                s->puppet_base_index = c;
        }
        if (puppet) s->has_actor_sync = 1;
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
    if (mp_string_equals(name, n, "ActorFilter")) {
        count = mp_read_vlq(r);
        for (mp_u32 i = 0; r->ok && i != count; ++i) mp_read_string(r, &n);
        return r->ok;
    }
    if (mp_string_equals(name, n, "SaveServer")) {
        /* SaveServer::readMap: VLQ entries of {u8 type, u8 length} */
        count = mp_read_vlq(r);
        if (count > 0x7FFFFFFFu) return 0;
        mp_skip(r, count * 2);
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
    s->server_create = s->server_update = s->server_remove = MP_NO_ID;
    s->client_init = s->client_change_manager = s->client_leave_manager = MP_NO_ID;
    s->client_create = s->client_update = s->client_remove = MP_NO_ID;
    s->puppet_component_count = 0;
    s->puppet_base_index = MP_NO_ID;
    s->has_actor_sync = 0;

    count = mp_read_vlq(&r);
    for (mp_u32 i = 0; r.ok && i != count; ++i) {
        const char* name = mp_read_string(&r, &n);
        mp_assign_id(name, n, i, "SERVER_ACTOR_SYNC_CREATE", &s->server_create);
        mp_assign_id(name, n, i, "SERVER_ACTOR_SYNC_UPDATE", &s->server_update);
        mp_assign_id(name, n, i, "SERVER_ACTOR_SYNC_REMOVE", &s->server_remove);
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

typedef struct mp_player_look {
    mp_u16 item;
    mp_u32 proc;
    mp_u32 colors[5]; /* RGB565 in u32 slots */
    mp_u8 cursor[4];  /* GXColor r, g, b, a */
    char name[MP_NAME_MAX + 1];
    mp_ship_state ship;
    mp_u8 extras; /* MP_EXTRA_* */
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
    return l;
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

typedef struct mp_anim_events {
    mp_u32 move_count, single_count, upper_count, reset_count, sword_count;
    mp_u32 equip_item_count, unequip_count; /* setAnimeEquipItem / setAnimeUnequip: no data */
    mp_u32 hit_count;                       /* decoded and dropped (no hit replay yet) */
    mp_u32 swing_count;                     /* WWHD extension */
    mp_u32 dropped;                         /* events beyond MP_MAX_EVENTS or MP_MAX_SWINGS */
    mp_move_anim move[MP_MAX_EVENTS];
    mp_single_anim single[MP_MAX_EVENTS];
    mp_upper_anim upper[MP_MAX_EVENTS];
    mp_reset_upper reset[MP_MAX_EVENTS];
    mp_u32 sword[MP_MAX_EVENTS];            /* setAnimeEquipSword(b1) */
    mp_swing swing[MP_MAX_SWINGS];
} mp_anim_events;

static inline void mp_anim_events_clear(mp_anim_events* e) {
    e->move_count = e->single_count = e->upper_count = e->reset_count = e->sword_count = 0;
    e->equip_item_count = e->unequip_count = e->hit_count = e->swing_count = e->dropped = 0;
}

static inline int mp_anim_events_empty(const mp_anim_events* e) {
    return !(e->move_count | e->single_count | e->upper_count | e->reset_count | e->sword_count |
             e->equip_item_count | e->unequip_count | e->swing_count);
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
