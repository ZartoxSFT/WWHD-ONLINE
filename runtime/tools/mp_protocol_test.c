/* Native tests for examples/guest-mods/multiplayer-connection/mp_protocol.h, the guest-side
 * codec of the historical multiplayer protocol. Expected client bytes are written by hand
 * from WindWakerClient (DataOutputStream, ActorSync, ActorBaseSync, ActorPlayerSync), not
 * produced by the codec under test. */
#undef NDEBUG
#include "../../examples/guest-mods/multiplayer-connection/mp_protocol.h"
#include "../../examples/guest-mods/multiplayer-connection/launcher_cfg.h"
#include "../../examples/guest-mods/multiplayer-connection/tunic_color.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

static mp_u8 buffer[4096];
static mp_u8 frame_buffer[4096];

static void expect_bytes(const mp_writer* w, const mp_u8* expected, mp_u32 size) {
    assert(w->ok);
    if (w->size != size || memcmp(w->data, expected, size) != 0) {
        fprintf(stderr, "got %u bytes:", w->size);
        for (mp_u32 i = 0; i != w->size; ++i) fprintf(stderr, " %02X", w->data[i]);
        fprintf(stderr, "\n");
        assert(0);
    }
}

static void test_vlq(void) {
    static const struct { mp_u32 value; mp_u8 bytes[5]; mp_u32 size; } cases[] = {
        {0, {0x80}, 1},
        {127, {0xFF}, 1},
        {128, {0x01, 0x80}, 2},
        {300, {0x02, 0xAC}, 2},
        {0xFFFFFFFFu, {0x0F, 0x7F, 0x7F, 0x7F, 0xFF}, 5},
    };
    for (unsigned i = 0; i != sizeof(cases) / sizeof(cases[0]); ++i) {
        mp_writer w = mp_writer_make(buffer, sizeof(buffer));
        mp_write_vlq(&w, cases[i].value);
        expect_bytes(&w, cases[i].bytes, cases[i].size);
        mp_reader r = mp_reader_make(cases[i].bytes, cases[i].size);
        assert(mp_read_vlq(&r) == cases[i].value && r.ok && r.pos == cases[i].size);
    }
    /* unterminated and over-long sequences */
    const mp_u8 open[] = {0x01, 0x02};
    mp_reader r = mp_reader_make(open, sizeof(open));
    mp_read_vlq(&r);
    assert(!r.ok);
    const mp_u8 too_long[] = {0x7F, 0x7F, 0x7F, 0x7F, 0x7F, 0xFF};
    r = mp_reader_make(too_long, sizeof(too_long));
    mp_read_vlq(&r);
    assert(!r.ok);
    /* writer overflow is sticky */
    mp_writer w = mp_writer_make(buffer, 1);
    mp_write_vlq(&w, 300);
    assert(!w.ok);
}

/* Init payload as Client::readInitPacket reads it, with all four initializers. */
static mp_u32 build_init(mp_u8* out, mp_u32 capacity, const char* puppet_second_component,
                         const char* extra_initializer) {
    mp_writer w = mp_writer_make(out, capacity);
    mp_write_vlq(&w, 4);
    mp_write_string(&w, "STUB");
    mp_write_string(&w, "SERVER_ACTOR_SYNC_UPDATE");
    mp_write_string(&w, "SERVER_ACTOR_SYNC_CREATE");
    mp_write_string(&w, "SERVER_ACTOR_SYNC_REMOVE");
    mp_write_vlq(&w, 7);
    mp_write_string(&w, "CLIENT_STUB");
    mp_write_string(&w, "CLIENT_TEST");
    mp_write_string(&w, "CLIENT_INIT_PACKET");
    mp_write_string(&w, "CLIENT_ACTOR_SYNC_CREATE");
    mp_write_string(&w, "CLIENT_ACTOR_SYNC_UPDATE");
    mp_write_string(&w, "CLIENT_PLAYER_CHANGE_MANAGER");
    mp_write_string(&w, "CLIENT_PLAYER_LEAVE_MANAGER");
    mp_write_vlq(&w, extra_initializer ? 5 : 4);
    mp_write_string(&w, "ResourceClient");
    mp_write_vlq(&w, 1024);
    mp_write_vlq(&w, 1);
    mp_write_string(&w, "folder");
    mp_write_vlq(&w, 2);
    mp_write_string(&w, "dir");      /* nested directory */
    mp_write_u8(&w, 1);
    mp_write_vlq(&w, 1);
    mp_write_string(&w, "file.bin");
    mp_write_u8(&w, 0);
    mp_write_vlq(&w, 99);
    mp_write_string(&w, "top.bin");
    mp_write_u8(&w, 0);
    mp_write_vlq(&w, 5);
    mp_write_string(&w, "ActorSync");
    mp_write_vlq(&w, 2);
    mp_write_string(&w, "Default");
    mp_write_vlq(&w, 2);
    mp_write_string(&w, "ActorBaseSync");
    mp_write_string(&w, "ActorHealthSync");
    mp_write_string(&w, "PlayerPuppet");
    mp_write_vlq(&w, 2);
    mp_write_string(&w, "ActorBaseSync");
    mp_write_string(&w, puppet_second_component);
    mp_write_string(&w, "ActorFilter");
    mp_write_vlq(&w, 2);
    mp_write_string(&w, "Bk");
    mp_write_string(&w, "kotubo");
    mp_write_string(&w, "SaveServer");
    mp_write_vlq(&w, 3);
    for (int i = 0; i != 6; ++i) mp_write_u8(&w, (mp_u8)i);
    if (extra_initializer) mp_write_string(&w, extra_initializer);
    assert(w.ok);
    return w.size;
}

static mp_session parsed_session(void) {
    mp_u8 init[1024];
    mp_session s;
    mp_u32 size = build_init(init, sizeof(init), "ActorPlayerSync", 0);
    assert(mp_parse_init(init, size, &s));
    return s;
}

static void test_init(void) {
    mp_session s = parsed_session();
    assert(s.server_update == 1 && s.server_create == 2 && s.server_remove == 3);
    assert(s.client_init == 2 && s.client_create == 3 && s.client_update == 4);
    assert(s.client_change_manager == 5 && s.client_leave_manager == 6);
    assert(s.client_remove == MP_NO_ID);
    assert(s.has_actor_sync && s.puppet_component_count == 2 && s.puppet_base_index == 0);
    assert(s.puppet_components[0] == MP_COMPONENT_BASE);
    assert(s.puppet_components[1] == MP_COMPONENT_PLAYER);

    mp_u8 init[1024];
    mp_u32 size = build_init(init, sizeof(init), "ActorPlayerSync", 0);
    /* every truncation fails, trailing bytes fail */
    for (mp_u32 cut = 0; cut != size; ++cut) assert(!mp_parse_init(init, cut, &s));
    init[size] = 0;
    assert(!mp_parse_init(init, size + 1, &s));
    /* unknown initializer and unsupported PlayerPuppet component are rejected */
    size = build_init(init, sizeof(init), "ActorPlayerSync", "Mystery");
    assert(!mp_parse_init(init, size, &s));
    size = build_init(init, sizeof(init), "ActorHealthSync", 0);
    assert(!mp_parse_init(init, size, &s));
}

/* The alive byte of the death block (the last one a player component ends with): checked, then
 * dropped, so the bytes before it are tested as a sender before the death block wrote them. */
static void drop_alive(mp_writer* w) {
    assert(w->ok && w->size && w->data[w->size - 1] == 0x00);
    --w->size;
}

static const mp_transform kTransform = {{1.0f, 2.0f, -1.0f}, {0x1234, -1, 0}};

#define BASE_BYTES 0x3F, 0x80, 0x00, 0x00, 0x40, 0x00, 0x00, 0x00, 0xBF, 0x80, 0x00, 0x00, \
                   0x12, 0x34, 0xFF, 0xFF, 0x00, 0x00 /* kTransform as ActorBaseSync::write */

/* Factories, filter and enemy multiplier; placed actors (ActorSync::getOrCreate with a game UID,
 * ActorSync::create, SyncedActor::readUpdate) with ActorHealthSync and a GameCube-only component. */
static mp_u32 build_actor_init(mp_u8* out, mp_u32 capacity) {
    mp_writer w = mp_writer_make(out, capacity);
    mp_write_vlq(&w, 4);
    mp_write_string(&w, "SERVER_ACTOR_SYNC_CREATE");
    mp_write_string(&w, "SERVER_ACTOR_SYNC_UPDATE");
    mp_write_string(&w, "SERVER_ACTOR_SYNC_REMOVE");
    mp_write_string(&w, "SERVER_MUTATORS");
    mp_write_vlq(&w, 6);
    mp_write_string(&w, "CLIENT_INIT_PACKET");
    mp_write_string(&w, "CLIENT_ACTOR_SYNC_CREATE");
    mp_write_string(&w, "CLIENT_ACTOR_SYNC_UPDATE");
    mp_write_string(&w, "CLIENT_ACTOR_SYNC_REMOVE");
    mp_write_string(&w, "CLIENT_ACTOR_EXCLUSIVE_ADD");
    mp_write_string(&w, "CLIENT_ACTOR_EXCLUSIVE_REMOVE");
    mp_write_vlq(&w, 2);
    mp_write_string(&w, "ActorSync");
    mp_write_vlq(&w, 6);
    mp_write_string(&w, "DefaultCreators"); /* a KTML base entry: no actor has this name */
    mp_write_vlq(&w, 1);
    mp_write_string(&w, "ActorBaseSync");
    mp_write_string(&w, "Default");
    mp_write_vlq(&w, 2);
    mp_write_string(&w, "ActorBaseSync");
    mp_write_string(&w, "ActorHealthSync");
    mp_write_string(&w, "Bk");
    mp_write_vlq(&w, 3);
    mp_write_string(&w, "ActorBaseSync");
    mp_write_string(&w, "ActorHealthSync");
    mp_write_string(&w, "BokoblinSync");
    mp_write_string(&w, "Spawned"); /* actors created while playing (WWHD) */
    mp_write_vlq(&w, 2);
    mp_write_string(&w, "ActorBaseSync");
    mp_write_string(&w, "ActorSpawnSync");
    mp_write_string(&w, "Pl"); /* a variable component: not usable for a placed actor */
    mp_write_vlq(&w, 1);
    mp_write_string(&w, "ActorPlayerSync");
    mp_write_string(&w, "PlayerPuppet");
    mp_write_vlq(&w, 2);
    mp_write_string(&w, "ActorBaseSync");
    mp_write_string(&w, "ActorPlayerSync");
    mp_write_string(&w, "ActorFilter");
    mp_write_vlq(&w, 9);
    mp_write_string(&w, "Bk");
    mp_write_string(&w, "#enemy_multiplier=4");
    mp_write_string(&w, "#enemy_randomizer=1");
    mp_write_string(&w, "#dynamic_scaling=1");
    mp_write_string(&w, "#elite_chance=12");
    mp_write_string(&w, "#elite_chance=x"); /* malformed: 0, then the next one wins again below */
    mp_write_string(&w, "Fmastr1");
    mp_write_string(&w, "NotAnActorName");
    mp_write_string(&w, "kotubo");
    assert(w.ok);
    return w.size;
}

static void test_placed_actors(void) {
    static mp_u8 init[1024];
    static mp_session s;
    mp_u32 size = build_actor_init(init, sizeof(init));
    assert(mp_parse_init(init, size, &s));
    assert(s.client_remove == 3 && s.client_exclusive_add == 4 && s.client_exclusive_remove == 5);
    assert(s.server_mutators == 3);
    {
        static const mp_u8 mutators[] = {0x86, 0x01}, too_many[] = {0x01, 0xC0, 0x00}, cut[] = {0x86};
        mp_u32 m = 0;
        int random = 0;
        assert(mp_read_mutators(mutators, sizeof(mutators), &m, &random) && m == 6 && random == 1);
        assert(mp_read_mutators(too_many, sizeof(too_many), &m, &random) && m == MP_MULTIPLIER_MAX && !random);
        assert(!mp_read_mutators(cut, sizeof(cut), &m, &random));
        /* 0.2.47: scaling and elite chance after the randomizer; an older body leaves them 0 */
        static const mp_u8 ext[] = {0x83, 0x00, 0x01, 0x0A}, big[] = {0x81, 0x01, 0x00, 0xFF},
                           half[] = {0x81, 0x01, 0x01};
        int scaling = 7;
        mp_u32 elite = 7;
        assert(mp_read_mutators_ex(mutators, sizeof(mutators), &m, &random, &scaling, &elite) && m == 6 &&
               random == 1 && scaling == 0 && elite == 0);
        assert(mp_read_mutators_ex(ext, sizeof(ext), &m, &random, &scaling, &elite) && m == 3 && !random &&
               scaling == 1 && elite == 10);
        assert(mp_read_mutators_ex(big, sizeof(big), &m, &random, &scaling, &elite) && elite == MP_ELITE_CHANCE_MAX &&
               scaling == 0);
        assert(!mp_read_mutators_ex(half, sizeof(half), &m, &random, &scaling, &elite));
        assert(mp_read_mutators(ext, sizeof(ext), &m, &random) && m == 3);
    }
    assert(s.factories_complete && s.factory_count == 4); /* Default, Bk, Spawned, Pl */
    assert(s.enemy_multiplier == 4 && s.enemy_randomizer == 1 && s.filter_count == 3);
    assert(s.dynamic_scaling == 1 && s.elite_chance == 0); /* the malformed last entry reset it */
    assert(!mp_filter_contains(&s, "#dynamic") && !mp_filter_contains(&s, "#elite_c"));
    assert(mp_filter_contains(&s, "Bk") && mp_filter_contains(&s, "Fmastr1") && mp_filter_contains(&s, "kotubo"));
    assert(!mp_filter_contains(&s, "B") && !mp_filter_contains(&s, "Bkk") && !mp_filter_contains(&s, "#enemy_m"));
    mp_u32 bk = mp_find_factory(&s, "Bk"), def = mp_find_factory(&s, "Default"), pl = mp_find_factory(&s, "Pl");
    assert(bk != MP_NO_ID && def != MP_NO_ID && pl != MP_NO_ID && mp_find_factory(&s, "kotubo") == def);
    assert(s.factories[bk].usable && s.factories[bk].count == 3 && s.factories[bk].base_index == 0);
    assert(s.factories[bk].health_index == 1 && s.factories[bk].kinds[2] == MP_COMPONENT_BOKOBLIN);
    assert(!s.factories[pl].usable);
    for (mp_u32 cut = 0; cut != size; ++cut) assert(!mp_parse_init(init, cut, &s));

    /* the multiplier entry: out of range is clamped, garbage gives 1 */
    {
        static const char* const entries[] = {"#enemy_multiplier=99", "#enemy_multiplier=x2", "#enemy_multiplier=0"};
        static const mp_u32 wanted[] = {MP_MULTIPLIER_MAX, 1, 1};
        for (int i = 0; i != 3; ++i) {
            mp_u8 filter[64];
            mp_writer fw = mp_writer_make(filter, sizeof(filter));
            mp_write_vlq(&fw, 1);
            mp_write_string(&fw, entries[i]);
            mp_reader r = mp_reader_make(fw.data, fw.size);
            s.enemy_multiplier = 7;
            s.filter_count = 0;
            assert(mp_read_actor_filter(&r, &s) && r.pos == r.size && s.enemy_multiplier == wanted[i]);
            assert(s.filter_count == 0);
        }
        size = build_actor_init(init, sizeof(init));
        assert(mp_parse_init(init, size, &s));
    }

    static const mp_u8 uid[] = {0xAA, 0xBB, 0xCC};
    mp_actor_values v = {.has_transform = 1, .transform = kTransform, .has_health = 1, .health = 5, .status = 0x10};
    mp_writer w = mp_writer_make(buffer, sizeof(buffer));
    mp_write_actor_create(&w, &s.factories[bk], 5, uid, sizeof(uid), "Bk", &v);
    static const mp_u8 create[] = {
        0x01, 0x83, 0xAA, 0xBB, 0xCC, 0x85, 'B', 'k', 0, 'B', 'k', 0,
        0x83,                                      /* three components */
        0x80, BASE_BYTES,
        0x81, 0x00, 0x05, 0x00, 0x00, 0x00, 0x10,  /* ActorHealthSync */
        0x82, 0, 0, 0, 0, 0, 0,                    /* BokoblinSync, not applied */
    };
    expect_bytes(&w, create, sizeof(create));

    /* an 8-character name is written whole */
    w = mp_writer_make(buffer, sizeof(buffer));
    mp_write_actor_create(&w, &s.factories[def], 0, uid, 1, "pflwrx77", &v);
    assert(w.ok && memcmp(w.data + 4, "pflwrx77\0pflwrx77\0", 18) == 0);

    w = mp_writer_make(buffer, sizeof(buffer));
    mp_actor_values hurt = {.has_health = 1, .health = -1, .status = 0};
    mp_write_actor_update(&w, &s.factories[bk], 7, 3, &hurt);
    static const mp_u8 update[] = {0x87, 0x03, 0x81, 0x81, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00};
    expect_bytes(&w, update, sizeof(update));

    /* WWHD: the owner's elite draw in the health short's high byte (mp_health_word) */
    w = mp_writer_make(buffer, sizeof(buffer));
    mp_actor_values tagged = {.has_health = 1, .health = 12, .tag = 0x4D, .status = 0};
    mp_write_actor_update(&w, &s.factories[bk], 7, 3, &tagged);
    static const mp_u8 tagged_update[] = {0x87, 0x03, 0x81, 0x81, 0x4D, 0x0C, 0x00, 0x00, 0x00, 0x00};
    expect_bytes(&w, tagged_update, sizeof(tagged_update));
    {
        mp_s16 h;
        mp_u8 t;
        mp_health_split(0x4D0C, &h, &t);
        assert(h == 12 && t == 0x4D);
        mp_health_split(0x40FE, &h, &t); /* dying, tagged: -2 */
        assert(h == -2 && t == 0x40);
        mp_health_split(0xFFFF, &h, &t); /* an untagged -1 (older clients, the GameCube one) */
        assert(h == -1 && t == 0);
        mp_health_split(0x0007, &h, &t);
        assert(h == 7 && t == 0);
        assert(mp_health_word(-1, 0) == 0xFFFF && mp_health_word(-3, 0x41) == 0x41FD);
        assert(mp_health_word(5, 0xFF) == 0x0005); /* never a tag that reads as a sign */
    }

    w = mp_writer_make(buffer, sizeof(buffer));
    mp_write_actor_create(&w, &s.factories[pl], 0, uid, 1, "Pl", &v);
    assert(!w.ok); /* never written with a variable component */

    w = mp_writer_make(buffer, sizeof(buffer));
    mp_write_actor_uid(&w, 300);
    static const mp_u8 exclusive[] = {0x02, 0xAC};
    expect_bytes(&w, exclusive, sizeof(exclusive));

    /* SERVER_ACTOR_SYNC_CREATE as SyncedActorComponent.makeCreatePacket writes it */
    static const mp_u8 server_create[] = {
        0x89, 0x01, 0x83, 0xAA, 0xBB, 0xCC, 0x00, 'B', 'k', 0, 'B', 'k', 0,
        0x02, 0x84, 0x82,                          /* ticket 2, owner 4, two components */
        0x80, BASE_BYTES,
        0x82, 1, 2, 3, 4, 5, 6,
    };
    static mp_placed_actor a;
    assert(mp_read_placed_create(server_create, sizeof(server_create), &s, &a));
    assert(a.uid == 9 && a.ticket == 2 && a.owner == 4 && !a.has_solver && a.factory == bk);
    assert(a.game_uid_size == 3 && memcmp(a.game_uid, uid, 3) == 0 && mp_name_equals(a.name, "Bk"));
    assert(a.values.has_transform && !a.values.has_health && a.values.transform.rot[1] == -1);
    assert(a.values.transform.pos[2] == -1.0f);
    for (mp_u32 cut = 0; cut != sizeof(server_create); ++cut)
        assert(!mp_read_placed_create(server_create, cut, &s, &a));

    /* another name takes the Default factory; a name with an unusable factory is skipped */
    static const mp_u8 server_create_default[] = {
        0x8A, 0x00, 0x01, 0x85, 'k', 'o', 't', 'u', 'b', 'o', 0, 'k', 'o', 't', 'u', 'b', 'o', 0,
        0x01, 0x80, 0x81, 0x81, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01,
    };
    assert(mp_read_placed_create(server_create_default, sizeof(server_create_default), &s, &a));
    assert(a.uid == 10 && a.has_solver && a.solver == 5 && a.game_uid_size == 0 && a.factory == def);
    assert(a.values.has_health && a.values.health == 0 && a.values.status == 1 && a.owner == 0);
    static const mp_u8 server_create_pl[] = {0x8B, 0x00, 0x00, 'P', 'l', 0, 'P', 'l', 0, 0x01, 0x80, 0x81, 0x80, 0xFF};
    assert(mp_read_placed_create(server_create_pl, sizeof(server_create_pl), &s, &a) && a.factory == MP_NO_ID);

    /* SERVER_ACTOR_SYNC_UPDATE: an index past the factory is rejected */
    static const mp_u8 server_update[] = {0x89, 0x05, 0x86, 0x81, 0x81, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00};
    assert(mp_read_placed_update(server_update, sizeof(server_update), &s.factories[bk], &a));
    assert(a.uid == 9 && a.ticket == 5 && a.owner == 6 && a.values.has_health && a.values.health == 2);
    assert(!a.values.has_transform);
    static const mp_u8 server_update_bad[] = {0x89, 0x05, 0x86, 0x81, 0x83, 0, 0, 0, 0, 0, 0};
    assert(!mp_read_placed_update(server_update_bad, sizeof(server_update_bad), &s.factories[bk], &a));

    /* a spawned actor: no game UID, factory "Spawned", ActorSpawnSync after ActorBaseSync */
    mp_u32 sp = mp_find_factory(&s, MP_SPAWN_FACTORY);
    assert(sp != MP_NO_ID && s.factories[sp].usable && s.factories[sp].kinds[1] == MP_COMPONENT_SPAWN);
    assert(s.factories[sp].health_index == 0xFF);
    mp_actor_values item = {.has_transform = 1, .transform = kTransform,
                            .spawn = {.proc = 0xFF, .params = 0x28FF0101, .argument = 0xFF,
                                      .angle = {0, -2, 0xFF}, .scale = {10, 10, 10}, .room = 3,
                                      .speed_f = 2.0f, .speed = {0.0f, 1.0f, 0.0f}, .gravity = -4.0f,
                                      .variant = 2}};
    w = mp_writer_make(buffer, sizeof(buffer));
    mp_write_actor_create_named(&w, &s.factories[sp], 0x100001, 0, 0, "item", MP_SPAWN_FACTORY, &item);
    static const mp_u8 spawn_create[] = {
        0x00, 0x40, 0x00, 0x81,                          /* no game UID, solver 0x100001 */
        'i', 't', 'e', 'm', 0, 'S', 'p', 'a', 'w', 'n', 'e', 'd', 0,
        0x82, 0x80, BASE_BYTES,
        0x81,                                            /* ActorSpawnSync */
        0x00, 0xFF, 0x28, 0xFF, 0x01, 0x01, 0xFF,       /* proc, params, argument */
        0x00, 0x00, 0xFF, 0xFE, 0x00, 0xFF,             /* angle */
        0x0A, 0x0A, 0x0A, 0x03,                         /* scale, room */
        0x40, 0x00, 0x00, 0x00,                         /* speedF 2 */
        0x00, 0x00, 0x00, 0x00, 0x3F, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0xC0, 0x80, 0x00, 0x00,                         /* gravity -4 */
        0x02,                                           /* variant (an arrow's element) */
    };
    expect_bytes(&w, spawn_create, sizeof(spawn_create));
    /* the server's create of it to another client: uid, no game UID, no solver */
    mp_u8 relay[128];
    mp_writer rw = mp_writer_make(relay, sizeof(relay));
    mp_write_vlq(&rw, 12); mp_write_u8(&rw, 0); mp_write_u8(&rw, 0);
    mp_write_string(&rw, "item"); mp_write_string(&rw, MP_SPAWN_FACTORY);
    mp_write_u8(&rw, 1); mp_write_vlq(&rw, 4);
    for (mp_u32 i = 17; i != sizeof(spawn_create); ++i) mp_write_u8(&rw, spawn_create[i]);
    assert(rw.ok && mp_read_placed_create(rw.data, rw.size, &s, &a));
    assert(a.uid == 12 && a.game_uid_size == 0 && !a.has_solver && a.factory == sp && a.owner == 4);
    assert(a.values.has_transform && a.values.has_spawn && a.values.spawn.proc == 0xFF);
    assert(a.values.spawn.params == 0x28FF0101 && a.values.spawn.angle[1] == -2 && a.values.spawn.room == 3);
    assert(a.values.spawn.speed[1] == 1.0f && a.values.spawn.gravity == -4.0f && a.values.spawn.scale[2] == 10);
    assert(a.values.spawn.variant == 2);
    for (mp_u32 cut = 0; cut != rw.size; ++cut) assert(!mp_read_placed_create(rw.data, cut, &s, &a));
    /* an owner's update of it: the position only */
    mp_actor_values moved = {.has_transform = 1, .transform = kTransform};
    w = mp_writer_make(buffer, sizeof(buffer));
    mp_write_actor_update(&w, &s.factories[sp], 12, 2, &moved);
    assert(w.ok && w.size == 1 + 1 + 1 + 1 + 18 && w.data[2] == 0x81 && w.data[3] == 0x80);
    /* an arrow shot: its parameters changed, the spawn component goes with the position */
    mp_actor_values shot = item;
    shot.has_spawn = 1;
    shot.spawn.params = 1;
    w = mp_writer_make(buffer, sizeof(buffer));
    mp_write_actor_update(&w, &s.factories[sp], 12, 2, &shot);
    assert(w.ok && w.size == 1 + 1 + 1 + (1 + 18) + (1 + 38) && w.data[2] == 0x82 && w.data[22] == 0x81);
    mp_u8 relay2[128];
    mp_writer uw = mp_writer_make(relay2, sizeof(relay2));
    mp_write_vlq(&uw, 12); mp_write_u8(&uw, 2); mp_write_vlq(&uw, 4);
    for (mp_u32 i = 2; i != w.size; ++i) mp_write_u8(&uw, w.data[i]);
    assert(uw.ok && mp_read_placed_update(uw.data, uw.size, &s.factories[sp], &a));
    assert(a.values.has_transform && a.values.has_spawn && a.values.spawn.params == 1 && a.owner == 4);
}

static void test_client_packets(void) {
    mp_session s = parsed_session();
    mp_player_look look = mp_player_look_default();

    /* CLIENT_PLAYER_CHANGE_MANAGER "sea" framed with ID 5 */
    mp_writer payload = mp_writer_make(buffer, sizeof(buffer));
    mp_write_change_manager(&payload, "sea");
    mp_writer frame = mp_writer_make(frame_buffer, sizeof(frame_buffer));
    mp_write_frame(&frame, s.client_change_manager, payload.data, payload.size);
    static const mp_u8 change[] = {0x85, 0x84, 's', 'e', 'a', 0x00};
    expect_bytes(&frame, change, sizeof(change));

    /* CLIENT_ACTOR_SYNC_CREATE for the local PlayerPuppet, solver 0 */
    payload = mp_writer_make(buffer, sizeof(buffer));
    mp_write_player_create(&payload, &s, 0, &kTransform, &look);
    static const mp_u8 create[] = {
        0x00,                                     /* no game UID */
        0x80,                                     /* solver 0 */
        'P', 'l', 'a', 'y', 'e', 'r', 'P', 'u', 'p', 'p', 'e', 't', 0x00,
        'P', 'l', 'a', 'y', 'e', 'r', 'P', 'u', 'p', 'p', 'e', 't', 0x00,
        0x82,                                     /* 2 components */
        0x80,                                     /* [0] ActorBaseSync */
        0x3F, 0x80, 0x00, 0x00, 0x40, 0x00, 0x00, 0x00, 0xBF, 0x80, 0x00, 0x00,
        0x12, 0x34, 0xFF, 0xFF, 0x00, 0x00,
        0x81,                                     /* [1] ActorPlayerSync */
        0xFF, 0xFF,                               /* item */
        0x0F, 0x7F, 0x7F, 0x7F, 0xFF,             /* proc 0xFFFFFFFF */
        0x00, 0x00, 0x5D, 0x89, 0x00, 0x00, 0xAF, 0x08, 0x00, 0x00, 0x24, 0xE7,
        0x00, 0x00, 0x85, 0x66, 0x00, 0x00, 0x24, 0x43,
        0xFF, 0xFF, 0x4D, 0xFF,                   /* cursor */
        0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, /* 8 empty event lists */
        0x00,                                     /* WWHD extension: empty name */
        0x00,                                     /* WWHD extension: no ship */
        0x00,                                     /* WWHD extension: no extras */
        0x00,                                     /* WWHD extension: no sword swings */
        0x00,                                     /* WWHD extension: no strikes */
        0x00,                                     /* WWHD extension: no particles */
        0x00,                                     /* WWHD extension: no skin */
        0x00,                                     /* WWHD extension: alive */
    };
    expect_bytes(&payload, create, sizeof(create));

    /* CLIENT_ACTOR_SYNC_UPDATE uid 300, ticket 5, ActorBaseSync only */
    payload = mp_writer_make(buffer, sizeof(buffer));
    mp_write_transform_update(&payload, &s, 300, 5, &kTransform);
    static const mp_u8 update[] = {
        0x02, 0xAC, 0x05, 0x81, 0x80,
        0x3F, 0x80, 0x00, 0x00, 0x40, 0x00, 0x00, 0x00, 0xBF, 0x80, 0x00, 0x00,
        0x12, 0x34, 0xFF, 0xFF, 0x00, 0x00,
    };
    expect_bytes(&payload, update, sizeof(update));

    /* CLIENT_ACTOR_SYNC_UPDATE uid 300, ticket 5, ActorPlayerSync only (red tunic) */
    mp_player_look red = mp_player_look_default();
    mp_player_look_set_colors(&red, 0xF800, 0xF800);
    payload = mp_writer_make(buffer, sizeof(buffer));
    mp_write_look_update(&payload, &s, 300, 5, &red);
    static const mp_u8 look_update[] = {
        0x02, 0xAC, 0x05, 0x81, 0x81,             /* uid, ticket, 1 component, index 1 */
        0xFF, 0xFF, 0x0F, 0x7F, 0x7F, 0x7F, 0xFF,
        0x00, 0x00, 0xF8, 0x00, 0x00, 0x00, 0xF8, 0x00, 0x00, 0x00, 0xB8, 0x00,
        0x00, 0x00, 0xB8, 0x00, 0x00, 0x00, 0x88, 0x00,
        0xFF, 0xFF, 0x4D, 0xFF,
        0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
        0x00,                                     /* empty name */
        0x00,                                     /* no ship */
        0x00,                                     /* no extras */
        0x00,                                     /* no sword swings */
        0x00,                                     /* no strikes */
        0x00,                                     /* no particles */
        0x00,                                     /* no skin */
        0x00,                                     /* alive */
    };
    expect_bytes(&payload, look_update, sizeof(look_update));

    /* the name follows the hit list, NUL-terminated */
    mp_set_name(&red, "Tetra", 5);
    payload = mp_writer_make(buffer, sizeof(buffer));
    mp_write_look_update(&payload, &s, 300, 5, &red);
    assert(payload.ok && payload.size == sizeof(look_update) + 5);
    assert(memcmp(payload.data + sizeof(look_update) - 8, "Tetra", 6) == 0);
    assert(payload.data[payload.size - 7] == 0x00 && payload.data[payload.size - 6] == 0x00 &&
           payload.data[payload.size - 5] == 0x00 && payload.data[payload.size - 4] == 0x00 &&
           payload.data[payload.size - 3] == 0x00 && payload.data[payload.size - 2] == 0x00 &&
           payload.data[payload.size - 1] == 0x00);
    /* no ship, no extras, no swings, no strikes, no particles, no skin, alive */

    /* without an ActorBaseSync in the factory an update cannot be encoded */
    mp_session no_base = s;
    no_base.puppet_base_index = MP_NO_ID;
    payload = mp_writer_make(buffer, sizeof(buffer));
    mp_write_transform_update(&payload, &no_base, 300, 5, &kTransform);
    assert(!payload.ok);
}

/* SERVER_ACTOR_SYNC_CREATE of a remote PlayerPuppet with a non-empty player component. */
static mp_u32 build_server_create_ship(mp_u8* out, mp_u32 capacity, const char* properties, int ship) {
    mp_writer w = mp_writer_make(out, capacity);
    mp_write_vlq(&w, 42);            /* uid */
    mp_write_u8(&w, 1);              /* has game UID */
    mp_write_vlq(&w, 3);
    mp_write_u8(&w, 0xAA); mp_write_u8(&w, 0xBB); mp_write_u8(&w, 0xCC);
    mp_write_u8(&w, 1);              /* has solver */
    mp_write_vlq(&w, 7);
    mp_write_string(&w, "PlayerPuppet");
    mp_write_string(&w, properties);
    mp_write_u8(&w, 9);              /* ticket */
    mp_write_vlq(&w, 42);            /* owner */
    mp_write_vlq(&w, 2);
    mp_write_vlq(&w, 1);             /* ActorPlayerSync first */
    mp_write_u16(&w, 0x0101);
    mp_write_vlq(&w, 12);
    for (int i = 0; i != 5; ++i) mp_write_u32(&w, 0x1000u + (mp_u32)i);
    mp_write_u8(&w, 1); mp_write_u8(&w, 2); mp_write_u8(&w, 3); mp_write_u8(&w, 4);
    mp_write_vlq(&w, 1);             /* move anim */
    mp_write_f32(&w, 1); mp_write_f32(&w, 2); mp_write_f32(&w, 3);
    mp_write_vlq(&w, 1); mp_write_vlq(&w, 2); mp_write_vlq(&w, 3); mp_write_f32(&w, 4);
    mp_write_vlq(&w, 1);             /* single move anim, negative s16 as sign-extended VLQ */
    mp_write_vlq(&w, 5); mp_write_f32(&w, 1); mp_write_f32(&w, 2);
    mp_write_vlq(&w, (mp_u32)(int)(mp_s16)-2); mp_write_f32(&w, 3);
    mp_write_vlq(&w, 1);             /* act anime upper */
    mp_write_vlq(&w, 1); mp_write_vlq(&w, 2); mp_write_f32(&w, 1); mp_write_f32(&w, 2);
    mp_write_vlq(&w, 3); mp_write_f32(&w, 4);
    mp_write_vlq(&w, 1);             /* reset upper */
    mp_write_vlq(&w, 1); mp_write_f32(&w, 1);
    mp_write_vlq(&w, 1);             /* equip sword */
    mp_write_vlq(&w, 1);
    mp_write_vlq(&w, 2);             /* equip item (empty records) */
    mp_write_vlq(&w, 1);             /* unequip (empty records) */
    mp_write_vlq(&w, 1);             /* hit */
    mp_write_vlq(&w, 1); mp_write_vlq(&w, 2); mp_write_vlq(&w, 3);
    mp_write_string(&w, "Medli	of the Rito, the sage"); /* name: tab replaced, cut at 16 */
    if (ship) {                      /* ship block, then the next component's index */
        mp_ship_state st = {.flags = MP_SHIP_PRESENT | MP_SHIP_SAIL, .pos = {10.0f, 20.0f, 30.0f}, .rot = {1, -2, 3}};
        mp_write_ship(&w, &st);
    }
    mp_write_vlq(&w, 0);             /* ActorBaseSync */
    mp_write_base(&w, &kTransform);
    assert(w.ok);
    return w.size;
}

static mp_u32 build_server_create(mp_u8* out, mp_u32 capacity, const char* properties) {
    return build_server_create_ship(out, capacity, properties, 0);
}

static void test_server_packets(void) {
    mp_session s = parsed_session();
    mp_u8 packet[1024];
    mp_actor_state a;
    mp_u32 size = build_server_create(packet, sizeof(packet), "PlayerPuppet");
    assert(mp_read_actor_create(packet, size, &s, &a));
    assert(a.uid == 42 && a.has_solver && a.solver == 7 && a.is_puppet);
    assert(a.ticket == 9 && a.owner == 42);
    assert(a.has_transform && a.has_look);
    for (int i = 0; i != 3; ++i) /* field by field: the struct has padding */
        assert(a.transform.pos[i] == kTransform.pos[i] && a.transform.rot[i] == kTransform.rot[i]);
    assert(a.look.item == 0x0101 && a.look.proc == 12 && a.look.colors[4] == 0x1004);
    assert(a.look.cursor[3] == 4);
    assert(strcmp(a.look.name, "Medli?of the Rit") == 0);
    assert(a.look.ship.flags == 0); /* the next component's index (0x80) is not a ship block */
    {
        mp_u8 with_ship[1024];
        mp_actor_state b;
        mp_u32 n = build_server_create_ship(with_ship, sizeof(with_ship), "PlayerPuppet", 1);
        assert(mp_read_actor_create(with_ship, n, &s, &b) && b.has_transform && b.has_look);
        assert(b.look.ship.flags == (MP_SHIP_PRESENT | MP_SHIP_SAIL) && b.look.ship.pos[2] == 30.0f && b.look.ship.rot[1] == -2);
        assert(b.transform.pos[0] == kTransform.pos[0]);
    }
    for (mp_u32 cut = 0; cut != size; ++cut) assert(!mp_read_actor_create(packet, cut, &s, &a));

    /* other actor kinds: header accepted, body left to the caller */
    size = build_server_create(packet, sizeof(packet), "Bk");
    assert(mp_read_actor_create(packet, size, &s, &a) && !a.is_puppet && a.uid == 42);

    /* SERVER_ACTOR_SYNC_UPDATE: uid + ticket + owner + ActorBaseSync */
    mp_writer w = mp_writer_make(packet, sizeof(packet));
    mp_write_vlq(&w, 300);
    mp_write_u8(&w, 6);
    mp_write_vlq(&w, 300);
    mp_write_vlq(&w, 1);
    mp_write_vlq(&w, 0);
    mp_write_base(&w, &kTransform);
    assert(mp_read_update_uid(w.data, w.size) == 300);
    a.has_transform = a.has_look = 1; /* stale flags from the create above must be reset */
    assert(mp_read_actor_update(w.data, w.size, &s, &a));
    assert(a.uid == 300 && a.ticket == 6 && a.has_transform && !a.has_look);
    assert(a.transform.rot[1] == -1);

    /* component index outside the PlayerPuppet table */
    w = mp_writer_make(packet, sizeof(packet));
    mp_write_vlq(&w, 300); mp_write_u8(&w, 6); mp_write_vlq(&w, 300);
    mp_write_vlq(&w, 1); mp_write_vlq(&w, 2);
    assert(!mp_read_actor_update(w.data, w.size, &s, &a));

    /* SERVER_ACTOR_SYNC_REMOVE */
    const mp_u8 remove[] = {0x02, 0xAC};
    assert(mp_read_actor_remove(remove, sizeof(remove)) == 300);
    assert(mp_read_actor_remove(remove, 1) == MP_NO_ID);
}

static void test_colors(void) {
    /* darkenRGB565 by hand: 0x5D89 = r 11, g 44, b 9 -> r 8, g 33, b 6 = 0x4426 */
    assert(mp_darken_rgb565(0x5D89) == 0x4426);
    assert(mp_darken_rgb565(0xFFFF) == ((23 << 11) | (47 << 5) | 23));
    assert(mp_darken_rgb565(0) == 0);
    mp_player_look l = mp_player_look_default();
    mp_player_look_set_colors(&l, 0xF800, 0x001F);
    assert(l.colors[0] == 0xF800 && l.colors[1] == 0x001F);
    assert(l.colors[2] == (23 << 11) && l.colors[3] == 23);
    assert(l.colors[4] == (17 << 11));
    assert(mp_rgb888_to_565(0xFF0000) == 0xF800 && mp_rgb888_to_565(0x00FF00) == 0x07E0);
    assert(mp_rgb888_to_565(0x0000FF) == 0x001F && mp_rgb888_to_565(0xFFFFFF) == 0xFFFF);
    assert(mp_rgb565_to_888(0xF800) == 0xFF0000 && mp_rgb565_to_888(0xFFFF) == 0xFFFFFF);
    assert(mp_rgb565_to_888(0) == 0);
}

static void test_launcher_cfg(void) {
    static const char text[] =
        "# written by the launcher\r\n"
        "uid=player02\r\n"
        "  host = 192.168.1.20  \n"
        "port=10014\n"
        "color=#3060ff\n"
        "color2=A5D73E\n"
        "cursor=FFFF4D\n"
        "puppet=1\n"
        "name = Captain Tetra \n"
        "hide_names=1\n"
        "hide_hud=1\n"
        "outfit=casual\n"
        "skin=miku_2\n"
        "unknown=whatever\n"
        "no equals sign\n"
        "=empty key\n";
    mp_launcher_cfg cfg;
    assert(mp_parse_launcher_cfg(text, sizeof(text) - 1, &cfg));
    assert(cfg.has_uid && memcmp(cfg.uid, "player02", 9) == 0);
    assert(cfg.has_host && strcmp(cfg.host, "192.168.1.20") == 0);
    assert(cfg.has_port && cfg.port == 10014);
    assert(cfg.has_color && cfg.color == 0x3060FF);
    assert(cfg.has_color2 && cfg.color2 == 0xA5D73E);
    assert(cfg.has_cursor && cfg.cursor == 0xFFFF4D);
    assert(cfg.has_puppet && cfg.puppet == 1);
    assert(cfg.has_name && strcmp(cfg.name, "Captain Tetra") == 0);
    assert(cfg.has_hide_names && cfg.hide_names == 1);
    assert(cfg.has_hide_hud && cfg.hide_hud == 1);
    assert(cfg.has_outfit && cfg.outfit == 1);
    assert(mp_parse_launcher_cfg("outfit=hero", 11, &cfg) && cfg.has_outfit && cfg.outfit == 0);
    assert(mp_parse_launcher_cfg("outfit=auto\n", 12, &cfg) && cfg.has_outfit && cfg.outfit == -1);
    assert(mp_parse_launcher_cfg(text, sizeof(text) - 1, &cfg) && cfg.has_skin && strcmp(cfg.skin, "miku_2") == 0);
    assert(mp_parse_launcher_cfg("skin=\n", 6, &cfg) && cfg.has_skin && cfg.skin[0] == 0); /* Link */
    assert(mp_parse_launcher_cfg("skin=Miku\n", 10, &cfg) && !cfg.has_skin);
    assert(mp_parse_launcher_cfg("skin=abcdefghijklm", 18, &cfg) && !cfg.has_skin);
    assert(mp_parse_launcher_cfg("skin=../x", 9, &cfg) && !cfg.has_skin);

    /* malformed values are dropped one by one, the rest still applies */
    static const char bad[] =
        "uid=short\nport=70000\ncolor=12345G\ncolor2=#ABCDEF\npuppet=yes\nhost=\nport=abc\n"
        "name=SeventeenLetters!\nhide_names=yes\noutfit=pyjama\nhide_hud=2\n";
    assert(mp_parse_launcher_cfg(bad, sizeof(bad) - 1, &cfg));
    assert(!cfg.has_uid && !cfg.has_port && !cfg.has_color && !cfg.has_puppet && !cfg.has_host);
    assert(!cfg.has_name && !cfg.has_hide_names && !cfg.has_outfit && !cfg.has_hide_hud);
    assert(cfg.has_color2 && cfg.color2 == 0xABCDEF);

    /* a last line without newline, an empty file, an oversized file */
    assert(mp_parse_launcher_cfg("color=000000", 12, &cfg) && cfg.has_color && cfg.color == 0);
    assert(mp_parse_launcher_cfg("", 0, &cfg) && !cfg.has_color);
    assert(!mp_parse_launcher_cfg(text, MP_CFG_MAX_BYTES + 1, &cfg));

    /* every truncation of the valid file parses without reading past the end */
    for (unsigned n = 0; n != sizeof(text) - 1; ++n) assert(mp_parse_launcher_cfg(text, n, &cfg));
}

/* ActorPlayerSync::write field by field (ActorPlayerSync.cpp), to compare with the codec. */
static void write_player_by_hand(mp_writer* w, const mp_player_look* l) {
    mp_write_u16(w, l->item);
    mp_write_vlq(w, l->proc);
    for (int i = 0; i != 5; ++i) mp_write_u32(w, l->colors[i]);
    for (int i = 0; i != 4; ++i) mp_write_u8(w, l->cursor[i]);
    mp_write_vlq(w, 1);                                   /* MoveAnimPacket */
    mp_write_f32(w, 0.25f); mp_write_f32(w, 1.0f); mp_write_f32(w, 1.5f);
    mp_write_vlq(w, 1); mp_write_vlq(w, 2); mp_write_vlq(w, 5); mp_write_f32(w, 3.0f);
    mp_write_vlq(w, 1);                                   /* SingleMoveAnimPacket, end -1 */
    mp_write_vlq(w, 0x95); mp_write_f32(w, 1.0f); mp_write_f32(w, 0.0f);
    mp_write_vlq(w, 0xFFFFFFFFu); mp_write_f32(w, -1.0f);
    mp_write_vlq(w, 1);                                   /* ActAnimeUpperPacket */
    mp_write_vlq(w, 0x1234); mp_write_vlq(w, 2); mp_write_f32(w, 1.0f); mp_write_f32(w, 2.0f);
    mp_write_vlq(w, 30); mp_write_f32(w, 4.0f);
    mp_write_vlq(w, 1);                                   /* ResetActAnimeUpperPacket */
    mp_write_vlq(w, 2); mp_write_f32(w, 2.4f);
    mp_write_vlq(w, 1);                                   /* AnimeEquipSwordPacket */
    mp_write_vlq(w, 1);
    mp_write_vlq(w, 2);                                   /* AnimeEquipItemPacket x2 */
    mp_write_vlq(w, 1);                                   /* AnimeUnequipPacket */
    mp_write_vlq(w, 0);                                   /* HitPacket */
    mp_write_string(w, l->name);                          /* WWHD extension: name */
    mp_write_u8(w, 0);                                    /* WWHD extension: no ship */
    mp_write_u8(w, 0);                                    /* WWHD extension: no extras */
    mp_write_u8(w, 0);                                    /* WWHD extension: no sword swings */
    mp_write_u8(w, 0);                                    /* WWHD extension: no strikes */
    mp_write_u8(w, 0);                                    /* WWHD extension: no particles */
    mp_write_u8(w, 0);                                    /* WWHD extension: no skin */
    mp_write_u8(w, 0);                                    /* WWHD extension: alive */
}

static void sample_events(mp_anim_events* e) {
    mp_anim_events_clear(e);
    mp_move_anim m = {0.25f, 1.0f, 1.5f, 1, 2, 5, 3.0f};
    e->move[e->move_count++] = m;
    mp_single_anim s = {0x95, 1.0f, 0.0f, -1, -1.0f};
    e->single[e->single_count++] = s;
    mp_upper_anim u = {0x1234, 2, 1.0f, 2.0f, 30, 4.0f};
    e->upper[e->upper_count++] = u;
    mp_reset_upper z = {2, 2.4f};
    e->reset[e->reset_count++] = z;
    e->sword[e->sword_count++] = 1;
    e->equip_item_count = 2;
    e->unequip_count = 1;
}

static void test_player_events(void) {
    mp_session s = parsed_session();
    mp_player_look look = mp_player_look_default();
    look.item = 0x0103;
    look.proc = 0x30;
    mp_anim_events e;
    sample_events(&e);
    assert(!mp_anim_events_empty(&e));

    /* the codec writes exactly the historical field order */
    mp_writer codec = mp_writer_make(buffer, sizeof(buffer));
    mp_write_player_events(&codec, &look, &e);
    mp_writer hand = mp_writer_make(frame_buffer, sizeof(frame_buffer));
    write_player_by_hand(&hand, &look);
    expect_bytes(&codec, hand.data, hand.size);

    /* round trip through an update, as the server relays it (ticket, owner added) */
    mp_writer w = mp_writer_make(buffer, sizeof(buffer));
    mp_write_vlq(&w, 77); mp_write_u8(&w, 3); mp_write_vlq(&w, 77);
    mp_write_vlq(&w, 1); mp_write_vlq(&w, 1);
    mp_write_player_events(&w, &look, &e); drop_alive(&w);
    static mp_actor_state a;
    assert(mp_read_actor_update(w.data, w.size, &s, &a));
    assert(a.has_look && a.look.item == 0x0103 && a.look.proc == 0x30);
    assert(a.events.move_count == 1 && a.events.move[0].anm1 == 2 && a.events.move[0].mode == 5);
    assert(a.events.move[0].blend == 0.25f && a.events.move[0].morf == 3.0f);
    assert(a.events.single_count == 1 && a.events.single[0].anm == 0x95 && a.events.single[0].end == -1);
    assert(a.events.upper_count == 1 && a.events.upper[0].bck == 0x1234 && a.events.upper[0].end == 30);
    /* (float): with x87 excess precision (FLT_EVAL_METHOD 2) the literal 2.4f is evaluated as
     * a long double, unequal to the float read back */
    assert(a.events.reset_count == 1 && a.events.reset[0].upper == 2 && a.events.reset[0].morf == (float)2.4f);
    assert(a.events.sword_count == 1 && a.events.sword[0] == 1);
    assert(a.events.equip_item_count == 2 && a.events.unequip_count == 1 && a.events.dropped == 0);
    /* every truncation fails, except the ones that end the component after one of the optional
     * WWHD blocks (an older mod), or after the hit list (a GameCube client) */
    for (mp_u32 cut = 0; cut != w.size - 7; ++cut) assert(!mp_read_actor_update(w.data, cut, &s, &a));
    assert(mp_read_actor_update(w.data, w.size - 7, &s, &a) && a.has_look && a.look.name[0] == 0);
    assert(a.events.move_count == 1 && a.look.ship.flags == 0);
    assert(mp_read_actor_update(w.data, w.size - 6, &s, &a) && a.has_look && a.look.ship.flags == 0);
    assert(mp_read_actor_update(w.data, w.size - 5, &s, &a) && a.has_look && a.look.extras == 0);
    assert(mp_read_actor_update(w.data, w.size - 4, &s, &a) && a.has_look && a.events.swing_count == 0);
    assert(mp_read_actor_update(w.data, w.size - 3, &s, &a) && a.has_look && a.events.strike_count == 0);
    assert(mp_read_actor_update(w.data, w.size - 2, &s, &a) && a.has_look && a.events.particle_count == 0);
    assert(mp_read_actor_update(w.data, w.size - 1, &s, &a) && a.has_look && a.look.skin[0] == 0);

    /* the extras: iron boots */
    {
        mp_player_look booted = look;
        booted.extras = MP_EXTRA_HEAVY_BOOTS;
        mp_writer bw = mp_writer_make(buffer, sizeof(buffer));
        mp_write_vlq(&bw, 77); mp_write_u8(&bw, 3); mp_write_vlq(&bw, 77);
        mp_write_vlq(&bw, 1); mp_write_vlq(&bw, 1);
        mp_write_player_events(&bw, &booted, 0); drop_alive(&bw);
        assert(bw.ok && bw.data[bw.size - 5] == MP_EXTRA_HEAVY_BOOTS && bw.data[bw.size - 4] == 0x00 &&
               bw.data[bw.size - 3] == 0x00 && bw.data[bw.size - 2] == 0x00 && bw.data[bw.size - 1] == 0x00);
        assert(mp_read_actor_update(bw.data, bw.size, &s, &a) && a.look.extras == MP_EXTRA_HEAVY_BOOTS);
    }

    /* the ship block: flags, then place and angles when present */
    {
        mp_player_look sailing = look;
        sailing.ship.flags = (mp_u8)(MP_SHIP_PRESENT | MP_SHIP_RIDING | MP_SHIP_SAIL | MP_SHIP_COLOR |
                                     (2 << MP_SHIP_PART_SHIFT)); /* cannon deployed */
        sailing.ship.color[0] = 0x12; sailing.ship.color[1] = 0x34; sailing.ship.color[2] = 0x56;
        sailing.ship.pos[0] = 1500.5f; sailing.ship.pos[1] = -2.0f; sailing.ship.pos[2] = -200000.0f;
        sailing.ship.rot[0] = 12; sailing.ship.rot[1] = -16384; sailing.ship.rot[2] = -3;
        mp_writer sw = mp_writer_make(buffer, sizeof(buffer));
        mp_write_vlq(&sw, 77); mp_write_u8(&sw, 3); mp_write_vlq(&sw, 77);
        mp_write_vlq(&sw, 1); mp_write_vlq(&sw, 1);
        mp_write_player_events(&sw, &sailing, 0); drop_alive(&sw);
        static const mp_u8 ship_bytes[] = {0x2F, 0x44, 0xBB, 0x90, 0x00, 0xC0, 0x00, 0x00, 0x00,
                                           0xC8, 0x43, 0x50, 0x00, 0x00, 0x0C, 0xC0, 0x00, 0xFF, 0xFD,
                                           0x12, 0x34, 0x56};
        assert(sw.ok && sw.size > sizeof(ship_bytes) + 5 && sw.data[sw.size - 5] == 0x00 && sw.data[sw.size - 4] == 0x00 &&
               sw.data[sw.size - 3] == 0x00 && sw.data[sw.size - 2] == 0x00 &&
               sw.data[sw.size - 1] == 0x00); /* no extras, swings, strikes, particles, skin */
        assert(memcmp(sw.data + sw.size - 5 - sizeof(ship_bytes), ship_bytes, sizeof(ship_bytes)) == 0);
        assert(mp_read_actor_update(sw.data, sw.size, &s, &a) && a.has_look);
        assert(mp_same_ship(&a.look.ship, &sailing.ship));
        for (mp_u32 cut = sw.size - 5 - sizeof(ship_bytes) + 1; cut != sw.size - 5; ++cut)
            assert(!mp_read_actor_update(sw.data, cut, &s, &a)); /* a cut ship block is malformed */
        mp_ship_state moved = sailing.ship;
        moved.pos[2] += 1.0f;
        assert(!mp_same_ship(&moved, &sailing.ship));
        mp_ship_state recoloured = sailing.ship;
        recoloured.color[2] = 0x57;
        assert(!mp_same_ship(&recoloured, &sailing.ship));
        assert(mp_read_actor_update(sw.data, sw.size, &s, &a));
        assert(((a.look.ship.flags & MP_SHIP_PART_MASK) >> MP_SHIP_PART_SHIFT) == 2 && a.look.ship.color[1] == 0x34);
        mp_ship_state none = {.flags = 0, .pos = {1.0f, 2.0f, 3.0f}, .rot = {1, 2, 3}}, none2 = {.flags = 0};
        assert(mp_same_ship(&none, &none2)); /* without a ship the place does not matter */
        /* a colour without a ship: flags then the three bytes */
        mp_ship_state painted = {.flags = MP_SHIP_COLOR, .color = {1, 2, 3}};
        mp_u8 painted_buffer[16];
        mp_writer pw = mp_writer_make(painted_buffer, sizeof(painted_buffer));
        mp_write_ship(&pw, &painted);
        static const mp_u8 painted_bytes[] = {0x08, 0x01, 0x02, 0x03};
        expect_bytes(&pw, painted_bytes, sizeof(painted_bytes));
        /* the detail: six s16 (tiller, sail, cannon pitch, yaw, crane, rope) and the hook */
        mp_ship_state detail = {.flags = MP_SHIP_DETAIL, .tiller = -2, .sail = 0x1000, .cannon_pitch = 3,
                                .cannon_yaw = -4, .crane = 0x2000, .rope = 40, .neck_pitch = 0x100,
                                .neck_turn = -0x100, .hook = {1.0f, -2.0f, 0.5f}};
        mp_u8 detail_buffer[32];
        mp_writer dw = mp_writer_make(detail_buffer, sizeof(detail_buffer));
        mp_write_ship(&dw, &detail);
        static const mp_u8 detail_bytes[] = {0x40, 0xFF, 0xFE, 0x10, 0x00, 0x00, 0x03, 0xFF, 0xFC, 0x20, 0x00,
                                             0x00, 0x28, 0x01, 0x00, 0xFF, 0x00, /* neck pitch 0x100, turn -0x100 */
                                             0x3F, 0x80, 0x00, 0x00, 0xC0, 0x00, 0x00, 0x00,
                                             0x3F, 0x00, 0x00, 0x00};
        expect_bytes(&dw, detail_bytes, sizeof(detail_bytes));
        mp_reader dr = {detail_buffer, dw.size, 0, 1};
        mp_ship_state back;
        mp_read_ship(&dr, &back);
        assert(dr.ok && dr.pos == dw.size && mp_same_ship(&back, &detail) && back.rope == 40 && back.hook[1] == -2.0f);
        mp_ship_state turned = detail;
        turned.cannon_yaw = 5;
        assert(!mp_same_ship(&turned, &detail));
        mp_reader cut = {detail_buffer, dw.size - 1, 0, 1};
        mp_read_ship(&cut, &back);
        assert(!cut.ok && back.flags == 0); /* a cut detail is malformed */
    }

    /* the client update: uid, ticket, one component (ActorPlayerSync, index 1) */
    mp_writer update = mp_writer_make(buffer, sizeof(buffer));
    mp_write_player_update(&update, &s, 300, 5, &look, &e);
    static const mp_u8 header[] = {0x02, 0xAC, 0x05, 0x81, 0x81};
    assert(update.ok && update.size == sizeof(header) + hand.size);
    assert(memcmp(update.data, header, sizeof(header)) == 0);
    assert(memcmp(update.data + sizeof(header), hand.data, hand.size) == 0);

    /* bounded lists: the newest call survives in the last slot */
    mp_anim_events many;
    mp_anim_events_clear(&many);
    for (mp_u32 i = 0; i != MP_MAX_EVENTS + 4; ++i) {
        mp_move_anim m = {0, 0, 0, i, i, 0, 0};
        many.move[mp_anim_slot(&many.move_count, &many.dropped)] = m;
    }
    assert(many.move_count == MP_MAX_EVENTS && many.dropped == 4);
    assert(many.move[MP_MAX_EVENTS - 1].anm0 == MP_MAX_EVENTS + 3 && many.move[0].anm0 == 0);
    mp_anim_events_append(&many, &e);
    assert(many.move[MP_MAX_EVENTS - 1].anm0 == 1 && many.dropped == 5);
    assert(many.single_count == 1 && many.equip_item_count == 2);

    /* a full message fits the guest's 2 KiB payload buffer */
    mp_anim_events full;
    mp_anim_events_clear(&full);
    for (mp_u32 i = 0; i != MP_MAX_EVENTS; ++i) {
        full.move[full.move_count++] = e.move[0];
        full.single[full.single_count++] = e.single[0];
        full.upper[full.upper_count++] = e.upper[0];
        full.reset[full.reset_count++] = e.reset[0];
        full.sword[full.sword_count++] = 0xFFFFFFFFu;
    }
    full.move[0].anm0 = full.move[0].anm1 = full.move[0].mode = 0xFFFFFFFFu;
    for (mp_u32 i = 0; i != MP_MAX_SWINGS; ++i) {
        mp_swing* sw = &full.swing[full.swing_count++];
        sw->at_type = 0xFFFFFFFFu;
        sw->vec[0] = sw->vec[1] = sw->vec[2] = 1.0f;
        sw->count = MP_SWING_CAPSULES;
        for (mp_u32 c = 0; c != MP_SWING_CAPSULES; ++c) {
            mp_swing_capsule k = {{1, 2, 3}, {4, 5, 6}, 7};
            sw->capsule[c] = k;
        }
    }
    for (mp_u32 i = 0; i != MP_MAX_STRIKES; ++i) {
        mp_strike* k = &full.strike[full.strike_count++];
        k->target = k->tg_offset = 0xFFFFFFFFu;
        k->at_sprm = k->at_type = k->gat_sprm = 0xFFFFFFFFu;
    }
    update = mp_writer_make(buffer, 4096);
    mp_write_player_update(&update, &s, 0xFFFFFFFFu, 5, &look, &full);
    assert(update.ok && update.size + 16 <= 4096 + 16);

    /* more events than a list keeps on reading: consumed, counted, the rest still decodes */
    mp_writer w2 = mp_writer_make(frame_buffer, sizeof(frame_buffer));
    mp_write_vlq(&w2, 77); mp_write_u8(&w2, 3); mp_write_vlq(&w2, 77);
    mp_write_vlq(&w2, 1); mp_write_vlq(&w2, 1);
    mp_write_u16(&w2, look.item); mp_write_vlq(&w2, look.proc);
    for (int i = 0; i != 5; ++i) mp_write_u32(&w2, look.colors[i]);
    for (int i = 0; i != 4; ++i) mp_write_u8(&w2, look.cursor[i]);
    mp_write_vlq(&w2, MP_MAX_EVENTS + 1);
    for (mp_u32 i = 0; i != MP_MAX_EVENTS + 1; ++i) {
        mp_write_f32(&w2, 0); mp_write_f32(&w2, 0); mp_write_f32(&w2, 0);
        mp_write_vlq(&w2, i); mp_write_vlq(&w2, 0); mp_write_vlq(&w2, 0); mp_write_f32(&w2, 0);
    }
    for (int i = 0; i != 7; ++i) mp_write_vlq(&w2, 0);
    mp_write_string(&w2, "");
    assert(mp_read_actor_update(w2.data, w2.size, &s, &a));
    assert(a.events.move_count == MP_MAX_EVENTS && a.events.dropped == 1);
    assert(a.events.move[MP_MAX_EVENTS - 1].anm0 == MP_MAX_EVENTS);
}

/* The sword swing block (WWHD extension after the extras). */
static void test_swings(void) {
    mp_session s = parsed_session();
    mp_player_look look = mp_player_look_default();
    mp_anim_events e;
    mp_anim_events_clear(&e);
    mp_swing one = {.at_type = 2, .vec = {0.0f, 0.0f, 1.0f}, .count = 1,
                    .capsule = {{{1.0f, 2.0f, 3.0f}, {4.0f, 5.0f, 6.0f}, 20.0f}}};
    e.swing[mp_swing_slot(&e)] = one;
    assert(!mp_anim_events_empty(&e) && e.swing_count == 1);

    /* bytes by hand: count, attack type, vector, capsule count, start, end, radius */
    mp_writer w = mp_writer_make(buffer, sizeof(buffer));
    mp_write_swings(&w, &e);
    static const mp_u8 swing_bytes[] = {
        0x01,                                                 /* one swing */
        0x00, 0x00, 0x00, 0x02,                               /* AT_TYPE_SWORD */
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x3F, 0x80, 0x00, 0x00,
        0x01,                                                 /* one capsule */
        0x3F, 0x80, 0x00, 0x00, 0x40, 0x00, 0x00, 0x00, 0x40, 0x40, 0x00, 0x00,
        0x40, 0x80, 0x00, 0x00, 0x40, 0xA0, 0x00, 0x00, 0x40, 0xC0, 0x00, 0x00,
        0x41, 0xA0, 0x00, 0x00,                               /* radius 20 */
    };
    expect_bytes(&w, swing_bytes, sizeof(swing_bytes));
    w = mp_writer_make(buffer, sizeof(buffer));
    mp_write_swings(&w, 0);
    static const mp_u8 no_swing[] = {0x00};
    expect_bytes(&w, no_swing, sizeof(no_swing));

    /* round trip through an update: three blade capsules, then a spin attack's cylinder */
    mp_swing blade = {.at_type = 0x10000, .vec = {-1.5f, 0.25f, 8.0f}, .count = 3};
    for (mp_u32 c = 0; c != 3; ++c) {
        mp_swing_capsule k = {{(float)c, -100.0f, 2.5f}, {10.0f, (float)c, -3.0f}, 15.0f + (float)c};
        blade.capsule[c] = k;
    }
    mp_swing spin = {.at_type = 2, .vec = {0, 0, 0}, .count = 1,
                     .capsule = {{{5.0f, 6.0f, 7.0f}, {5.0f, 86.0f, 7.0f}, 120.0f}}};
    e.swing[mp_swing_slot(&e)] = blade;
    e.swing[mp_swing_slot(&e)] = spin;
    w = mp_writer_make(buffer, sizeof(buffer));
    mp_write_vlq(&w, 77); mp_write_u8(&w, 3); mp_write_vlq(&w, 77);
    mp_write_vlq(&w, 1); mp_write_vlq(&w, 1);
    mp_write_player_events(&w, &look, &e); drop_alive(&w);
    static mp_actor_state a;
    assert(mp_read_actor_update(w.data, w.size, &s, &a) && a.has_look);
    assert(a.events.swing_count == 3 && a.events.dropped == 0 && !mp_anim_events_empty(&a.events));
    assert(a.events.swing[0].at_type == 2 && a.events.swing[0].capsule[0].radius == 20.0f);
    const mp_swing* b = &a.events.swing[1];
    assert(b->at_type == 0x10000 && b->count == 3 && b->vec[0] == -1.5f && b->vec[2] == 8.0f);
    for (mp_u32 c = 0; c != 3; ++c)
        assert(b->capsule[c].start[0] == (float)c && b->capsule[c].start[1] == -100.0f &&
               b->capsule[c].end[1] == (float)c && b->capsule[c].end[2] == -3.0f &&
               b->capsule[c].radius == 15.0f + (float)c);
    assert(a.events.swing[2].count == 1 && a.events.swing[2].capsule[0].end[1] == 86.0f);
    /* a cut swing block is malformed; the block can only be dropped whole */
    mp_u32 block = 1 + 3 * (4 + 12 + 1) + 5 * 28; /* then the empty strike, particle and skin blocks */
    for (mp_u32 cut = w.size - 3 - block + 1; cut != w.size - 3; ++cut)
        assert(!mp_read_actor_update(w.data, cut, &s, &a));
    assert(mp_read_actor_update(w.data, w.size - 3 - block, &s, &a) && a.events.swing_count == 0);
    assert(mp_read_actor_update(w.data, w.size - 3, &s, &a) && a.events.swing_count == 3);

    /* more swings than a message keeps: the newest survives in the last slot */
    mp_anim_events many;
    mp_anim_events_clear(&many);
    for (mp_u32 i = 0; i != MP_MAX_SWINGS + 2; ++i) {
        mp_swing x = one;
        x.at_type = i;
        many.swing[mp_swing_slot(&many)] = x;
    }
    assert(many.swing_count == MP_MAX_SWINGS && many.dropped == 2);
    assert(many.swing[0].at_type == 0 && many.swing[MP_MAX_SWINGS - 1].at_type == MP_MAX_SWINGS + 1);
    mp_anim_events joined;
    mp_anim_events_clear(&joined);
    mp_anim_events_append(&joined, &e);
    assert(joined.swing_count == 3 && joined.swing[1].at_type == 0x10000);

    /* from another sender: more swings and capsules than kept, consumed and counted */
    w = mp_writer_make(buffer, sizeof(buffer));
    mp_write_vlq(&w, 77); mp_write_u8(&w, 3); mp_write_vlq(&w, 77);
    mp_write_vlq(&w, 1); mp_write_vlq(&w, 1);
    mp_write_player_events(&w, &look, 0); drop_alive(&w);
    w.size -= 4; /* replace the empty swing, strike, particle and skin blocks */
    mp_write_u8(&w, MP_MAX_SWINGS + 1);
    for (mp_u32 i = 0; i != MP_MAX_SWINGS + 1; ++i) {
        mp_write_u32(&w, i); mp_write_f32(&w, 0); mp_write_f32(&w, 0); mp_write_f32(&w, 0);
        mp_write_u8(&w, MP_SWING_CAPSULES + 1);
        for (mp_u32 c = 0; c != MP_SWING_CAPSULES + 1; ++c)
            for (int k = 0; k != 7; ++k) mp_write_f32(&w, (float)c);
    }
    assert(w.ok && mp_read_actor_update(w.data, w.size, &s, &a));
    assert(a.events.swing_count == MP_MAX_SWINGS && a.events.dropped == (MP_MAX_SWINGS + 1) + 1);
    assert(a.events.swing[MP_MAX_SWINGS - 1].at_type == MP_MAX_SWINGS);
    assert(a.events.swing[0].count == MP_SWING_CAPSULES && a.events.swing[0].capsule[3].radius == 3.0f);
    /* a capsule count with bit 7 set is malformed */
    w = mp_writer_make(buffer, sizeof(buffer));
    mp_write_vlq(&w, 77); mp_write_u8(&w, 3); mp_write_vlq(&w, 77);
    mp_write_vlq(&w, 1); mp_write_vlq(&w, 1);
    mp_write_player_events(&w, &look, 0); drop_alive(&w);
    w.size -= 4;
    mp_write_u8(&w, 1); mp_write_u32(&w, 2);
    mp_write_f32(&w, 0); mp_write_f32(&w, 0); mp_write_f32(&w, 0);
    mp_write_u8(&w, 0x81);
    assert(!mp_read_actor_update(w.data, w.size, &s, &a));

    /* in a server create, the next component's index (bit 7 set) after the extras is no swing
     * block; a swing block before it is read, then the index */
    mp_u8 packet[1024];
    mp_writer c = mp_writer_make(packet, sizeof(packet));
    mp_write_vlq(&c, 42); mp_write_u8(&c, 0); mp_write_u8(&c, 0);
    mp_write_string(&c, "PlayerPuppet"); mp_write_string(&c, "PlayerPuppet");
    mp_write_u8(&c, 9); mp_write_vlq(&c, 42); mp_write_vlq(&c, 2);
    mp_write_vlq(&c, 1);
    mp_anim_events only;
    mp_anim_events_clear(&only);
    only.swing[mp_swing_slot(&only)] = one;
    mp_write_player_events(&c, &look, &only); drop_alive(&c);
    mp_write_vlq(&c, 0);
    mp_write_base(&c, &kTransform);
    assert(c.ok && mp_read_actor_create(packet, c.size, &s, &a) && a.has_transform && a.has_look);
    assert(a.events.swing_count == 1 && a.events.swing[0].capsule[0].end[2] == 6.0f);
    c.size = 0;
    mp_write_vlq(&c, 42); mp_write_u8(&c, 0); mp_write_u8(&c, 0);
    mp_write_string(&c, "PlayerPuppet"); mp_write_string(&c, "PlayerPuppet");
    mp_write_u8(&c, 9); mp_write_vlq(&c, 42); mp_write_vlq(&c, 2);
    mp_write_vlq(&c, 1);
    mp_write_player_events(&c, &look, 0); drop_alive(&c);
    c.size -= 4; /* a sender before the swings: the extras, then the next index */
    mp_write_vlq(&c, 0);
    mp_write_base(&c, &kTransform);
    assert(c.ok && mp_read_actor_create(packet, c.size, &s, &a) && a.has_transform && a.events.swing_count == 0);
}

/* The player directory (WWHD extension): a client's entry, the server's list. */
static void test_directory(void) {
    mp_dir_entry a = {.id = 0, .color = 0xF800, .point = 2, .start_room = 44, .layer = -1, .room = 44,
                      .pos = {1.5f, -2.0f, 300000.0f}, .heading = -16384, .flags = MP_DIR_SEA, .sea = {10.0f, -20.0f}};
    mp_copy_text(a.name, sizeof(a.name), "Tetra", 5);
    mp_copy_text(a.stage, sizeof(a.stage), "sea", 3);
    char shortened[MP_NAME_MAX + 1];
    mp_copy_text(shortened, sizeof(shortened), "Tet\0ra", 6); /* stops at a NUL */
    assert(strcmp(shortened, "Tet") == 0 && shortened[MP_NAME_MAX] == 0);
    mp_dir_entry b = a;
    b.flags = 0;
    mp_copy_text(b.name, sizeof(b.name), "Medli", 5);
    mp_copy_text(b.stage, sizeof(b.stage), "M_Dra09", 7);
    b.point = 0; b.start_room = 0; b.layer = 3; b.room = 9;

    mp_writer w = mp_writer_make(buffer, sizeof(buffer));
    mp_write_dir_entry(&w, &a);
    static const mp_u8 entry_bytes[] = {
        'T', 'e', 't', 'r', 'a', 0, 0xF8, 0x00, 's', 'e', 'a', 0, 0x00, 0x02, 44, 0xFF, 44,
        0x3F, 0xC0, 0x00, 0x00, 0xC0, 0x00, 0x00, 0x00, 0x48, 0x92, 0x7C, 0x00, 0xC0, 0x00, 0x01,
        0x41, 0x20, 0x00, 0x00, 0xC1, 0xA0, 0x00, 0x00};
    expect_bytes(&w, entry_bytes, sizeof(entry_bytes));

    /* the server's answer: receiver id 7, two players */
    w = mp_writer_make(buffer, sizeof(buffer));
    mp_write_vlq(&w, 7);
    mp_write_vlq(&w, 2);
    mp_write_vlq(&w, 7);
    mp_write_dir_entry(&w, &a);
    mp_write_vlq(&w, 300);
    mp_write_dir_entry(&w, &b);
    static mp_directory d;
    assert(mp_read_directory(w.data, w.size, &d));
    assert(d.self == 7 && d.count == 2 && d.entry[0].id == 7 && d.entry[1].id == 300);
    assert(strcmp(d.entry[0].name, "Tetra") == 0 && strcmp(d.entry[0].stage, "sea") == 0);
    assert(d.entry[0].point == 2 && d.entry[0].start_room == 44 && d.entry[0].layer == -1);
    assert(d.entry[0].pos[2] == 300000.0f && d.entry[0].heading == -16384 && d.entry[0].sea[1] == -20.0f);
    assert(strcmp(d.entry[1].stage, "M_Dra09") == 0 && d.entry[1].flags == 0 && d.entry[1].sea[0] == 0.0f);
    assert(d.entry[1].room == 9 && d.entry[1].layer == 3);
    /* any cut, trailing bytes or an unknown flag is refused */
    for (mp_u32 cut = 0; cut != w.size; ++cut) assert(!mp_read_directory(w.data, cut, &d));
    mp_u8 longer[512];
    memcpy(longer, w.data, w.size);
    longer[w.size] = 0;
    assert(!mp_read_directory(longer, w.size + 1, &d));
    w = mp_writer_make(buffer, sizeof(buffer));
    mp_write_vlq(&w, 1); mp_write_vlq(&w, 1); mp_write_vlq(&w, 1);
    mp_dir_entry bad = b;
    mp_write_dir_entry(&w, &bad);
    w.data[w.size - 1] = 0x04; /* the flags byte (no sea place follows) */
    assert(!mp_read_directory(w.data, w.size, &d));
    /* dead (MP_DIR_DEAD): no field, kept with the sea place */
    w.data[w.size - 1] = MP_DIR_DEAD;
    assert(mp_read_directory(w.data, w.size, &d) && d.count == 1 && d.entry[0].flags == MP_DIR_DEAD);
    mp_dir_entry dead = a;
    dead.flags = MP_DIR_SEA | MP_DIR_DEAD;
    w = mp_writer_make(buffer, sizeof(buffer));
    mp_write_vlq(&w, 1); mp_write_vlq(&w, 1); mp_write_vlq(&w, 1);
    mp_write_dir_entry(&w, &dead);
    assert(w.data[w.size - 9] == 0x03 && mp_read_directory(w.data, w.size, &d));
    assert(d.entry[0].flags == (MP_DIR_SEA | MP_DIR_DEAD) && d.entry[0].sea[0] == 10.0f);
    /* more players than kept: read through, the first MP_DIR_MAX kept */
    w = mp_writer_make(buffer, sizeof(buffer));
    mp_write_vlq(&w, 1); mp_write_vlq(&w, MP_DIR_MAX + 2);
    for (mp_u32 i = 0; i != MP_DIR_MAX + 2; ++i) { mp_write_vlq(&w, i); mp_write_dir_entry(&w, &b); }
    assert(w.ok && mp_read_directory(w.data, w.size, &d) && d.count == MP_DIR_MAX && d.entry[MP_DIR_MAX - 1].id == MP_DIR_MAX - 1);
    /* the names map by name from the init list */
    mp_session s = parsed_session();
    assert(s.server_player_directory == MP_NO_ID && s.client_player_directory == MP_NO_ID);
}

/* The save sync (SaveServer): the map of the server's WWSave (captured from the Java server, 120
 * runs), its elements, the update codecs and the world packet. */
static const mp_u8 k_save_map[] = {1, 8, 0, 16, 3, 8, 4, 8, 1, 8, 6, 8, 6, 0, 0, 25, 7, 21, 1, 2, 0, 40, 2, 4, 0, 48, 2, 64, 0, 84, 1, 4, 6, 17, 6, 0, 6, 17, 6, 0, 6, 17, 6, 0, 6, 17, 6, 0, 0, 12, 5, 12, 1, 2, 0, 2, 1, 8, 0, 104, 1, 8, 0, 104, 1, 8, 0, 104, 1, 8, 0, 108, 2, 4, 7, 16, 2, 4, 7, 8, 0, 4, 2, 4, 7, 16, 2, 4, 7, 8, 0, 4, 2, 4, 7, 16, 2, 4, 7, 8, 0, 4, 2, 4, 7, 16, 2, 4, 7, 8, 0, 4, 2, 4, 7, 16, 2, 4, 7, 8, 0, 4, 2, 4, 7, 16, 2, 4, 7, 8, 0, 4, 2, 4, 7, 16, 2, 4, 7, 8, 0, 4, 2, 4, 7, 16, 2, 4, 7, 8, 0, 4, 2, 4, 7, 16, 2, 4, 7, 8, 0, 4, 2, 4, 7, 16, 2, 4, 7, 8, 0, 4, 2, 4, 7, 16, 2, 4, 7, 8, 0, 4, 2, 4, 7, 16, 2, 4, 7, 8, 0, 4, 2, 4, 7, 16, 2, 4, 7, 8, 0, 4, 2, 4, 7, 16, 2, 4, 7, 8, 0, 4, 2, 4, 7, 16, 2, 4, 7, 8, 0, 4, 2, 4, 7, 16, 2, 4, 7, 8, 0, 4, 1, 100, 7, 255, 7, 1, 0, 84};

static void test_save_sync(void) {
    mp_session s;
    memset(&s, 0, sizeof(s));
    /* the initializer as SaveServer.writeClientInit writes it */
    mp_u8 init[600];
    mp_writer w = mp_writer_make(init, sizeof(init));
    mp_write_vlq(&w, (mp_u32)(sizeof(k_save_map) / 2));
    for (mp_u32 i = 0; i != sizeof(k_save_map); ++i) mp_write_u8(&w, k_save_map[i]);
    mp_reader r = mp_reader_make(init, w.size);
    assert(mp_skip_initializer(&r, "SaveServer", 10, &s) && r.pos == w.size && s.save_map_count == 120);
    static mp_save_element e[2048];
    mp_u32 n = mp_save_elements(&s, e, 2048);
    assert(n);
    /* the whole dSv_save_c, in order, without gaps */
    mp_u32 at = 0;
    for (mp_u32 i = 0; i != n; ++i) { assert(e[i].offset == at); at += e[i].size; }
    assert(at == MP_SAVE_BYTES);
    /* landmarks of d_save.h: maxLife a short at 0, the return place's name (string, 8) at 0x30,
     * the items (bytes) at 0x3C, the item flags (bits) at 0x51, the player name at 0x15C,
     * the first stage memory's tbox (int) at 0x380, the events (bits) at 0x624 */
    mp_u32 i = 0;
    while (e[i].offset != 0x30) ++i;
    assert(e[i].type == MP_SAVE_STRING && e[i].size == 8 && e[i + 1].offset == 0x38);
    while (e[i].offset != 0x3C) ++i;
    assert(e[i].type == 0 && e[i].size == 1);
    while (e[i].offset != 0x51) ++i;
    assert(e[i].type == 7);
    while (e[i].offset != 0x15C) ++i;
    assert(e[i].type == MP_SAVE_STRING && e[i].size == 17);
    while (e[i].offset != 0x380) ++i;
    assert(e[i].type == 2 && e[i].size == 4);
    while (e[i].offset != 0x624) ++i;
    assert(e[i].type == 7 && e[n - 1].offset == MP_SAVE_BYTES - 1);
    assert(e[0].type == 1 && e[0].size == 2);
    /* a map too long for the buffer, or with an unknown type, is refused */
    assert(mp_save_elements(&s, e, 10) == 0);
    mp_session bad = s;
    bad.save_map_type[0] = 9;
    assert(mp_save_elements(&bad, e, 2048) == 0);
    bad = s;
    bad.save_map_length[0] = 7; /* not whole shorts */
    assert(mp_save_elements(&bad, e, 2048) == 0);

    /* CLIENT_SAVE_UPDATE: maxLife 12 (a short), a string "sea" in its 8 bytes, VLQ offsets */
    mp_u8 save[MP_SAVE_BYTES];
    memset(save, 0, sizeof(save));
    save[0] = 0x00; save[1] = 0x0C;
    memcpy(save + 0x30, "sea", 4);
    n = mp_save_elements(&s, e, 2048);
    mp_u32 name_index = 0;
    while (e[name_index].offset != 0x30) ++name_index;
    w = mp_writer_make(buffer, sizeof(buffer));
    mp_write_vlq(&w, 2);
    mp_write_save_element(&w, &e[0], save + e[0].offset);
    mp_write_save_element(&w, &e[name_index], save + 0x30);
    static const mp_u8 update[] = {0x82, 0x80, 0x00, 0x0C, 0xB0, 's', 'e', 'a', 0x00};
    expect_bytes(&w, update, sizeof(update));
    /* a string without its NUL in its room is cut to room - 1 */
    memcpy(save + 0x30, "ABCDEFGH", 8);
    w = mp_writer_make(buffer, sizeof(buffer));
    mp_write_save_element(&w, &e[name_index], save + 0x30);
    assert(w.size == 1 + 8 && w.data[8] == 0 && w.data[7] == 'G');

    /* SERVER_SAVE_UPDATE: offset 0x624 (VLQ 0x8C 0x24... in the protocol's order), length 1 */
    w = mp_writer_make(buffer, sizeof(buffer));
    mp_write_vlq(&w, 2);
    mp_write_vlq(&w, 0x624); mp_write_vlq(&w, 1); mp_write_u8(&w, 0x40);
    mp_write_vlq(&w, 0); mp_write_vlq(&w, 2); mp_write_u8(&w, 0x00); mp_write_u8(&w, 0x10);
    r = mp_reader_make(w.data, w.size);
    assert(mp_read_save_count(&r) == 2);
    mp_u32 off, len;
    const mp_u8* bytes;
    assert(mp_read_save_entry(&r, &off, &len, &bytes) && off == 0x624 && len == 1 && bytes[0] == 0x40);
    assert(mp_read_save_entry(&r, &off, &len, &bytes) && off == 0 && len == 2 && bytes[1] == 0x10);
    assert(r.ok && r.pos == r.size);
    /* past the save, or past the packet, is refused */
    w = mp_writer_make(buffer, sizeof(buffer));
    mp_write_vlq(&w, 1); mp_write_vlq(&w, MP_SAVE_BYTES - 1); mp_write_vlq(&w, 2); mp_write_u8(&w, 1); mp_write_u8(&w, 2);
    r = mp_reader_make(w.data, w.size);
    mp_read_save_count(&r);
    assert(!mp_read_save_entry(&r, &off, &len, &bytes) && !r.ok);
    w = mp_writer_make(buffer, sizeof(buffer));
    mp_write_vlq(&w, 1); mp_write_vlq(&w, 4); mp_write_vlq(&w, 3); mp_write_u8(&w, 1);
    r = mp_reader_make(w.data, w.size);
    mp_read_save_count(&r);
    assert(!mp_read_save_entry(&r, &off, &len, &bytes));

    /* SERVER_SAVE_FROM */
    static const mp_u8 from[] = {'M', 'e', 'd', 'l', 'i', 0};
    char who[MP_NAME_MAX + 1];
    assert(mp_read_save_from(from, sizeof(from), who) && strcmp(who, "Medli") == 0);
    assert(!mp_read_save_from(from, sizeof(from) - 1, who));

    /* SERVER_WORLD: time and weather, explicit; 90 degrees (6 h), day 3, rain */
    static const mp_u8 world[] = {0x07, 0x42, 0xB4, 0x00, 0x00, 0x00, 0x03, 0x03};
    mp_world wd;
    assert(mp_read_world(world, sizeof(world), &wd) && wd.flags == 7 && wd.time == 90.0f && wd.day == 3 && wd.weather == 3);
    for (mp_u32 cut = 0; cut != sizeof(world); ++cut) assert(!mp_read_world(world, cut, &wd));
    mp_u8 storm[sizeof(world)];
    memcpy(storm, world, sizeof(world));
    storm[7] = 6;
    assert(!mp_read_world(storm, sizeof(storm), &wd)); /* no weather 6 */
    memcpy(storm, world, sizeof(world));
    storm[1] = 0x43; storm[2] = 0xB4; /* 360 degrees */
    assert(!mp_read_world(storm, sizeof(storm), &wd));

    /* CLIENT_WORLD: the same body (WWClient.receiveWorld), here a sailor's clock and heavy rain */
    mp_u8 report[16];
    mp_writer rw = mp_writer_make(report, sizeof(report));
    mp_world sailor = {MP_WORLD_TIME | MP_WORLD_WEATHER, 90.0f, 3, 4};
    mp_write_world(&rw, &sailor);
    static const mp_u8 report_bytes[] = {0x03, 0x42, 0xB4, 0x00, 0x00, 0x00, 0x03, 0x04};
    assert(rw.ok && rw.size == sizeof(report_bytes) && memcmp(report, report_bytes, sizeof(report_bytes)) == 0);
    assert(mp_read_world(report, rw.size, &wd) && wd.flags == 3 && wd.time == 90.0f && wd.weather == 4);

    /* the names map by name from the init list; an init without them leaves them unset */
    mp_session p = parsed_session();
    assert(p.server_save_update == MP_NO_ID && p.client_save_merge == MP_NO_ID && p.server_world == MP_NO_ID);
    assert(p.client_world == MP_NO_ID);
}

/* The launcher's shop: a creature bought for another player (CLIENT_PLAYER_SUMMON) and the one
 * received (SERVER_PLAYER_SUMMON, WWClient.receiveSummon). */
/* The death block after the skin (death.h) and the revive packets (WWClient.receiveRevive). */
static void test_death(void) {
    mp_u8 buffer[256];
    mp_player_look l = mp_player_look_default(), got;
    l.death = MP_DEATH_GHOST;
    l.death_dir = 300;
    l.corpse[0] = 1.0f;
    l.corpse[1] = -2.0f;
    l.corpse[2] = 3.5f;
    l.corpse_angle = -0x4000;
    mp_writer w = mp_writer_make(buffer, sizeof(buffer));
    mp_write_player(&w, &l);
    static const mp_u8 tail[] = {0x01, 0x02, 0xAC, 0x3F, 0x80, 0x00, 0x00, 0xC0, 0x00, 0x00, 0x00,
                                 0x40, 0x60, 0x00, 0x00, 0xC0, 0x00};
    assert(w.ok && w.size > sizeof(tail) && memcmp(w.data + w.size - sizeof(tail), tail, sizeof(tail)) == 0);
    mp_reader r = mp_reader_make(buffer, w.size);
    mp_read_player(&r, &got);
    assert(r.ok && r.pos == r.size && got.death == MP_DEATH_GHOST && got.death_dir == 300);
    assert(got.corpse[0] == 1.0f && got.corpse[1] == -2.0f && got.corpse[2] == 3.5f && got.corpse_angle == -0x4000);
    /* a sender before the death block: alive */
    r = mp_reader_make(buffer, w.size - sizeof(tail));
    mp_read_player(&r, &got);
    assert(r.ok && r.pos == r.size && got.death == 0);
    /* a truncated body fails */
    r = mp_reader_make(buffer, w.size - 1);
    mp_read_player(&r, &got);
    assert(!r.ok);
    /* alive: one byte */
    l.death = 0;
    w = mp_writer_make(buffer, sizeof(buffer));
    mp_write_player(&w, &l);
    assert(w.ok && buffer[w.size - 1] == 0x00 && buffer[w.size - 2] == 0x00); /* no skin, alive */
    /* down (the game's death, no ghost): the flags byte alone, read back as it is */
    l.death = MP_DEATH_DOWN;
    w = mp_writer_make(buffer, sizeof(buffer));
    mp_write_player(&w, &l);
    assert(w.ok && buffer[w.size - 1] == 0x02 && buffer[w.size - 2] == 0x00);
    r = mp_reader_make(buffer, w.size);
    mp_read_player(&r, &got);
    assert(r.ok && r.pos == r.size && got.death == MP_DEATH_DOWN && got.death_dir == 0);

    w = mp_writer_make(buffer, sizeof(buffer));
    mp_write_revive_request(&w, 7, "Medli");
    static const mp_u8 request[] = {0x87, 'M', 'e', 'd', 'l', 'i', 0x00};
    expect_bytes(&w, request, sizeof(request));
    static const mp_u8 revive[] = {'A', 'r', 'y', 'l', 'l', 0x00};
    char who[MP_NAME_MAX + 1];
    assert(mp_read_revive(revive, sizeof(revive), who) && strcmp(who, "Aryll") == 0);
    assert(!mp_read_revive(revive, sizeof(revive) - 1, who));
    mp_u8 longer[sizeof(revive) + 1];
    memcpy(longer, revive, sizeof(revive));
    longer[sizeof(revive)] = 1;
    assert(!mp_read_revive(longer, sizeof(longer), who));
    mp_session p = parsed_session();
    assert(p.client_player_revive == MP_NO_ID && p.server_player_revive == MP_NO_ID);
}

static void test_summon(void) {
    mp_writer w = mp_writer_make(buffer, sizeof(buffer));
    mp_write_summon_request(&w, 300, 4, "Tetra");
    static const mp_u8 request[] = {0x02, 0xAC, 0x04, /* VLQ 300: the protocol's order */ 'T', 'e', 't', 'r', 'a', 0};
    expect_bytes(&w, request, sizeof(request));
    static const mp_u8 answer[] = {0x05, 'M', 'e', 'd', 'l', 'i', 0};
    mp_u8 creature = 0xFF;
    char buyer[MP_NAME_MAX + 1];
    assert(mp_read_summon(answer, sizeof(answer), &creature, buyer) && creature == 5 && strcmp(buyer, "Medli") == 0);
    for (mp_u32 cut = 0; cut != sizeof(answer); ++cut) assert(!mp_read_summon(answer, cut, &creature, buyer));
    mp_u8 longer[sizeof(answer) + 1];
    memcpy(longer, answer, sizeof(answer));
    longer[sizeof(answer)] = 0;
    assert(!mp_read_summon(longer, sizeof(longer), &creature, buyer)); /* trailing byte */
    mp_u8 unknown[] = {MP_SUMMON_CREATURES, 'A', 0};
    assert(!mp_read_summon(unknown, sizeof(unknown), &creature, buyer)); /* no such creature */
    mp_session s = parsed_session(); /* an init without them: not available */
    assert(s.server_player_summon == MP_NO_ID && s.client_player_summon == MP_NO_ID);
}

/* The skin (WWHD extension after the particles): a string, kept only when it is a valid id. */
static void test_skin(void) {
    mp_session s = parsed_session();
    mp_player_look look = mp_player_look_default();
    assert(look.skin[0] == 0);
    mp_set_skin(&look, "miku_2", 6);
    assert(strcmp(look.skin, "miku_2") == 0);
    mp_writer w = mp_writer_make(buffer, sizeof(buffer));
    mp_write_vlq(&w, 77); mp_write_u8(&w, 3); mp_write_vlq(&w, 77);
    mp_write_vlq(&w, 1); mp_write_vlq(&w, 1);
    mp_write_player_events(&w, &look, 0); drop_alive(&w);
    assert(w.ok && memcmp(w.data + w.size - 7, "miku_2", 7) == 0 && w.data[w.size - 8] == 0x00); /* after the particles */
    static mp_actor_state a;
    assert(mp_read_actor_update(w.data, w.size, &s, &a) && a.has_look && strcmp(a.look.skin, "miku_2") == 0);
    for (mp_u32 cut = w.size - 7 + 1; cut != w.size; ++cut)
        assert(!mp_read_actor_update(w.data, cut, &s, &a)); /* a cut string is malformed */
    assert(mp_read_actor_update(w.data, w.size - 7, &s, &a) && a.look.skin[0] == 0); /* an older sender */
    /* ids the mod cannot use read as no skin, the packet stays valid */
    static const char* const bad[] = {"Miku", "a-b", "abcdefghijklm", "../x"};
    for (int i = 0; i != 4; ++i) {
        w = mp_writer_make(buffer, sizeof(buffer));
        mp_write_vlq(&w, 77); mp_write_u8(&w, 3); mp_write_vlq(&w, 77);
        mp_write_vlq(&w, 1); mp_write_vlq(&w, 1);
        mp_write_player_events(&w, &look, 0); drop_alive(&w);
        w.size -= 7;
        mp_write_string(&w, bad[i]);
        assert(mp_read_actor_update(w.data, w.size, &s, &a) && a.has_look && a.look.skin[0] == 0);
    }
    mp_player_look other = look;
    mp_set_skin(&other, "Miku", 4);
    assert(other.skin[0] == 0);
    /* in a server create, the next component's index (bit 7 set) after the particles is no skin */
    mp_u8 packet[1024];
    mp_writer c = mp_writer_make(packet, sizeof(packet));
    mp_write_vlq(&c, 42); mp_write_u8(&c, 0); mp_write_u8(&c, 0);
    mp_write_string(&c, "PlayerPuppet"); mp_write_string(&c, "PlayerPuppet");
    mp_write_u8(&c, 9); mp_write_vlq(&c, 42); mp_write_vlq(&c, 2);
    mp_write_vlq(&c, 1);
    mp_write_player_events(&c, &look, 0); drop_alive(&c);
    c.size -= 7; /* a sender before the skin */
    mp_write_vlq(&c, 0);
    mp_write_base(&c, &kTransform);
    assert(c.ok && mp_read_actor_create(packet, c.size, &s, &a) && a.has_transform && a.look.skin[0] == 0);
    c.size = 0;
    mp_write_vlq(&c, 42); mp_write_u8(&c, 0); mp_write_u8(&c, 0);
    mp_write_string(&c, "PlayerPuppet"); mp_write_string(&c, "PlayerPuppet");
    mp_write_u8(&c, 9); mp_write_vlq(&c, 42); mp_write_vlq(&c, 2);
    mp_write_vlq(&c, 1);
    mp_write_player_events(&c, &look, 0); drop_alive(&c);
    mp_write_vlq(&c, 0);
    mp_write_base(&c, &kTransform);
    assert(c.ok && mp_read_actor_create(packet, c.size, &s, &a) && a.has_transform && strcmp(a.look.skin, "miku_2") == 0);
}

/* The strike block (WWHD extension after the swings): sword hits on placed actors. */
static void test_strikes(void) {
    mp_session s = parsed_session();
    mp_player_look look = mp_player_look_default();
    mp_anim_events e;
    mp_anim_events_clear(&e);
    mp_strike hit = {.target = 300, .tg_offset = 0x4A0, .at_index = 3, .atp = 2, .se = 1, .mtrl = 0,
                     .hit_mark = 3, .spl = 0, .at_sprm = 0x1D, .at_type = 2, .gat_sprm = 0x20,
                     .vec = {0.0f, 1.0f, -1.0f}, .pos = {10.0f, 20.0f, 30.0f}};
    e.strike[mp_strike_slot(&e)] = hit;
    assert(!mp_anim_events_empty(&e) && e.strike_count == 1);
    mp_writer w = mp_writer_make(buffer, sizeof(buffer));
    mp_write_strikes(&w, &e);
    static const mp_u8 strike_bytes[] = {
        0x01,                         /* one strike */
        0x02, 0xAC,                   /* target uid 300 */
        0x09, 0xA0,                   /* collider offset 0x4A0 */
        0x80, 0x80,                   /* attacker: the Link, no collider offset */
        0x03, 0x02, 0x01, 0x00, 0x03, 0x00, /* mAtCyl, atp, se, mtrl, hit mark, spl */
        0x00, 0x00, 0x00, 0x1D,       /* at SPrm */
        0x00, 0x00, 0x00, 0x02,       /* at type */
        0x00, 0x00, 0x00, 0x20,       /* GObjAt SPrm */
        0x00, 0x00, 0x00, 0x00, 0x3F, 0x80, 0x00, 0x00, 0xBF, 0x80, 0x00, 0x00,
        0x41, 0x20, 0x00, 0x00, 0x41, 0xA0, 0x00, 0x00, 0x41, 0xF0, 0x00, 0x00,
    };
    expect_bytes(&w, strike_bytes, sizeof(strike_bytes));
    w = mp_writer_make(buffer, sizeof(buffer));
    mp_write_strikes(&w, 0);
    static const mp_u8 none[] = {0x00};
    expect_bytes(&w, none, sizeof(none));

    /* round trip through an update; a cut block is malformed, a missing one is none */
    w = mp_writer_make(buffer, sizeof(buffer));
    mp_write_vlq(&w, 77); mp_write_u8(&w, 3); mp_write_vlq(&w, 77);
    mp_write_vlq(&w, 1); mp_write_vlq(&w, 1);
    mp_write_player_events(&w, &look, &e); drop_alive(&w);
    static mp_actor_state a;
    assert(mp_read_actor_update(w.data, w.size, &s, &a) && a.has_look && a.events.strike_count == 1);
    const mp_strike* k = &a.events.strike[0];
    assert(k->target == 300 && k->tg_offset == 0x4A0 && k->at_index == 3 && k->atp == 2 && k->hit_mark == 3);
    assert(k->at_sprm == 0x1D && k->at_type == 2 && k->gat_sprm == 0x20 && k->vec[2] == -1.0f && k->pos[1] == 20.0f);
    assert(k->attacker == 0 && k->at_offset == 0);
    /* a bomb's explosion: its uid and collider offset */
    {
        mp_anim_events b;
        mp_anim_events_clear(&b);
        mp_strike bomb = hit;
        bomb.attacker = 500;
        bomb.at_offset = 0x6A8;
        b.strike[mp_strike_slot(&b)] = bomb;
        mp_writer bw = mp_writer_make(frame_buffer, sizeof(frame_buffer));
        mp_write_strikes(&bw, &b);
        assert(bw.ok && bw.data[5] == 0x03 && bw.data[6] == 0xF4 && bw.data[7] == 0x0D && bw.data[8] == 0xA8);
        mp_reader br = mp_reader_make(bw.data, bw.size);
        mp_anim_events back;
        mp_anim_events_clear(&back);
        mp_read_strikes(&br, &back);
        assert(br.ok && br.pos == bw.size && back.strike_count == 1 && back.strike[0].attacker == 500);
        assert(back.strike[0].at_offset == 0x6A8 && back.strike[0].tg_offset == 0x4A0);
    }
    for (mp_u32 cut = w.size - 2 - sizeof(strike_bytes) + 1; cut != w.size - 2; ++cut)
        assert(!mp_read_actor_update(w.data, cut, &s, &a));
    assert(mp_read_actor_update(w.data, w.size - 2 - sizeof(strike_bytes), &s, &a) && a.events.strike_count == 0);

    /* the particle block: the hammer's shock wave, with an angle and a scale, and a plain spark */
    mp_anim_events pe;
    mp_anim_events_clear(&pe);
    mp_particle wave = {.group = 1, .id = 0x8125, .flags = MP_PARTICLE_ANGLE | MP_PARTICLE_SCALE,
                        .pos = {1.0f, 2.0f, 3.0f}, .angle = {0, -1, 2}, .scale = {1.0f, 0.5f, 1.0f}, .alpha = 0xFF};
    mp_particle spark = {.group = 0, .id = 0x0010, .flags = 0, .pos = {0, 0, 0}, .alpha = 0x80};
    pe.particle[mp_particle_slot(&pe)] = wave;
    pe.particle[mp_particle_slot(&pe)] = spark;
    assert(!mp_anim_events_empty(&pe));
    w = mp_writer_make(buffer, sizeof(buffer));
    mp_write_particles(&w, &pe);
    static const mp_u8 particle_bytes[] = {
        0x02,
        0x01, 0x81, 0x25, 0x03, 0x3F, 0x80, 0x00, 0x00, 0x40, 0x00, 0x00, 0x00, 0x40, 0x40, 0x00, 0x00,
        0x00, 0x00, 0xFF, 0xFF, 0x00, 0x02, 0x3F, 0x80, 0x00, 0x00, 0x3F, 0x00, 0x00, 0x00, 0x3F, 0x80,
        0x00, 0x00, 0xFF,
        0x00, 0x00, 0x10, 0x00, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0x80,
    };
    expect_bytes(&w, particle_bytes, sizeof(particle_bytes));
    w = mp_writer_make(buffer, sizeof(buffer));
    mp_write_vlq(&w, 77); mp_write_u8(&w, 3); mp_write_vlq(&w, 77);
    mp_write_vlq(&w, 1); mp_write_vlq(&w, 1);
    mp_write_player_events(&w, &look, &pe); drop_alive(&w);
    assert(mp_read_actor_update(w.data, w.size, &s, &a) && a.events.particle_count == 2);
    assert(a.events.particle[0].id == 0x8125 && a.events.particle[0].angle[1] == -1);
    assert(a.events.particle[0].scale[1] == 0.5f && a.events.particle[1].alpha == 0x80);
    assert(a.events.particle[1].scale[0] == 1.0f && a.events.particle[1].flags == 0);
    for (mp_u32 cut = w.size - 1 - sizeof(particle_bytes) + 1; cut != w.size - 1; ++cut)
        assert(!mp_read_actor_update(w.data, cut, &s, &a));
    assert(mp_read_actor_update(w.data, w.size - 1, &s, &a) && a.events.particle_count == 2);
    /* with the emitter's rotation: 9 floats after the position */
    mp_anim_events pm;
    mp_anim_events_clear(&pm);
    mp_particle turned = spark;
    turned.flags = MP_PARTICLE_MATRIX;
    for (int c = 0; c != 9; ++c) turned.matrix[c] = (float)c;
    pm.particle[mp_particle_slot(&pm)] = turned;
    w = mp_writer_make(buffer, sizeof(buffer));
    mp_write_particles(&w, &pm);
    assert(w.ok && w.size == 1 + 1 + 2 + 1 + 12 + 36 + 1 && w.data[4] == MP_PARTICLE_MATRIX);
    mp_reader pr = mp_reader_make(w.data, w.size);
    mp_anim_events back;
    mp_anim_events_clear(&back);
    mp_read_particles(&pr, &back);
    assert(pr.ok && pr.pos == w.size && back.particle_count == 1 && back.particle[0].matrix[8] == 8.0f);
    mp_reader plain = mp_reader_make(particle_bytes, sizeof(particle_bytes));
    mp_anim_events_clear(&back);
    mp_read_particles(&plain, &back);
    assert(plain.ok && back.particle[1].matrix[0] == 1.0f && back.particle[1].matrix[1] == 0.0f &&
           back.particle[1].matrix[4] == 1.0f); /* identity without the flag */
    mp_u8 bad[] = {0x01, 0x00, 0x00, 0x10, 0x40};
    mp_reader br = mp_reader_make(bad, sizeof(bad));
    mp_read_particles(&br, &back);
    assert(!br.ok); /* an unknown flag is malformed */

    /* a followed emitter (0.2.41): key after the alpha, then the colours; a stop needs the key */
    mp_particle charge = {.group = 1, .id = 0x0219, .flags = MP_PARTICLE_FOLLOW | MP_PARTICLE_COLOR,
                          .pos = {1, 2, 3}, .alpha = 0xFF, .key = 7,
                          .prm = {0xDC, 0xB4, 0x50, 0xFF}, .env = {0x78, 0x78, 0x00, 0xFF}};
    mp_anim_events pf;
    mp_anim_events_clear(&pf);
    pf.particle[mp_particle_slot(&pf)] = charge;
    mp_particle stop = charge;
    stop.flags = MP_PARTICLE_FOLLOW | MP_PARTICLE_STOP;
    pf.particle[mp_particle_slot(&pf)] = stop;
    w = mp_writer_make(buffer, sizeof(buffer));
    mp_write_particles(&w, &pf);
    static const mp_u8 follow_bytes[] = {
        0x02,
        0x01, 0x02, 0x19, 0x28, 0x3F, 0x80, 0x00, 0x00, 0x40, 0x00, 0x00, 0x00, 0x40, 0x40, 0x00, 0x00,
        0xFF, 0x07, 0xDC, 0xB4, 0x50, 0xFF, 0x78, 0x78, 0x00, 0xFF,
        0x01, 0x02, 0x19, 0x18, 0x3F, 0x80, 0x00, 0x00, 0x40, 0x00, 0x00, 0x00, 0x40, 0x40, 0x00, 0x00,
        0xFF, 0x07,
    };
    expect_bytes(&w, follow_bytes, sizeof(follow_bytes));
    mp_reader fr = mp_reader_make(w.data, w.size);
    mp_read_particles(&fr, &back);
    assert(fr.ok && fr.pos == w.size && back.particle_count == 2);
    assert(back.particle[0].key == 7 && back.particle[0].prm[0] == 0xDC && back.particle[0].env[2] == 0x00);
    assert(back.particle[1].flags == (MP_PARTICLE_FOLLOW | MP_PARTICLE_STOP) && back.particle[1].key == 7);
    assert(back.particle[1].prm[0] == 0xFF); /* no colours: white */
    for (mp_u32 cut = 1; cut != w.size; ++cut) {
        mp_reader tr = mp_reader_make(w.data, cut);
        mp_read_particles(&tr, &back);
        assert(!tr.ok);
    }
    mp_u8 lone_stop[] = {0x01, 0x00, 0x00, 0x10, 0x10, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0xFF};
    mp_reader sr = mp_reader_make(lone_stop, sizeof(lone_stop));
    mp_read_particles(&sr, &back);
    assert(!sr.ok); /* a stop without a key */

    /* bounded: the newest survives in the last slot; append keeps them */
    mp_anim_events many;
    mp_anim_events_clear(&many);
    for (mp_u32 i = 0; i != MP_MAX_STRIKES + 3; ++i) {
        mp_strike x = hit;
        x.target = i;
        many.strike[mp_strike_slot(&many)] = x;
    }
    assert(many.strike_count == MP_MAX_STRIKES && many.dropped == 3);
    assert(many.strike[MP_MAX_STRIKES - 1].target == MP_MAX_STRIKES + 2);
    mp_anim_events joined;
    mp_anim_events_clear(&joined);
    mp_anim_events_append(&joined, &e);
    assert(joined.strike_count == 1 && joined.strike[0].tg_offset == 0x4A0);
}

static int near_byte(unsigned a, unsigned b) { return a > b ? a - b <= 1 : b - a <= 1; }
static int near_rgb(tc_u32 a, tc_u32 b) {
    return near_byte(a >> 16, b >> 16) && near_byte((a >> 8) & 0xFF, (b >> 8) & 0xFF) && near_byte(a & 0xFF, b & 0xFF);
}
static int near_float(float a, float b) { return a > b ? a - b < 0.01f : b - a < 0.01f; }

static void test_tunic_colors(void) {
    /* HSB of the launcher references, by hand: #0DBD2F = (13, 189, 47): d = 176,
     * h = 120 + 60 * 34/176 = 131.59, s = 176/189, b = 189/255 */
    tc_hsb g = tc_rgb_to_hsb(TC_LINK_GREEN);
    assert(near_float(g.h, 131.59f) && near_float(g.s, 0.9312f) && near_float(g.b, 0.7412f));
    tc_hsb l = tc_rgb_to_hsb(TC_LINK_SUNNY_LIME); /* (165, 215, 62): h = 120 - 60 * 103/153 */
    assert(near_float(l.h, 79.61f) && near_float(l.s, 0.7116f) && near_float(l.b, 0.8431f));
    assert(tc_hsb_to_rgb(0.0f, 1.0f, 1.0f) == 0xFF0000 && tc_hsb_to_rgb(240.0f, 1.0f, 1.0f) == 0x0000FF);
    assert(near_rgb(tc_hsb_to_rgb(g.h, g.s, g.b), TC_LINK_GREEN));

    tc_u32 out = 0;
    tc_palette same = tc_palette_make(TC_LINK_GREEN, TC_LINK_SUNNY_LIME);
    assert(tc_recolor_rgb(&same, TC_LINK_GREEN, &out) && near_rgb(out, TC_LINK_GREEN));
    assert(tc_recolor_rgb(&same, TC_LINK_SUNNY_LIME, &out) && near_rgb(out, TC_LINK_SUNNY_LIME));

    tc_palette red_blue = tc_palette_make(0xFF0000, 0x0000FF);
    assert(tc_recolor_rgb(&red_blue, TC_LINK_GREEN, &out) && out == 0xFF0000);
    assert(tc_recolor_rgb(&red_blue, TC_LINK_SUNNY_LIME, &out) && out == 0x0000FF);
    /* a darker tunic shade keeps its ratios: (7, 95, 24) has b = 0.3725 = 0.5026 x 0.7412 and
     * s = 88/95 = 0.9947 x 0.9312, so red at 0.5026 x 255 = 128 with g = b = 128 x 0.0053 = 1 */
    assert(tc_recolor_rgb(&red_blue, 0x075F18, &out) && near_rgb(out, 0x800000));
    /* not tunic: skin, hair, grey, near black, a blue */
    out = 0x123456;
    assert(!tc_recolor_rgb(&red_blue, 0xF0C090, &out) && !tc_recolor_rgb(&red_blue, 0xE8D040, &out));
    assert(!tc_recolor_rgb(&red_blue, 0x808080, &out) && !tc_recolor_rgb(&red_blue, 0x041404, &out));

    /* boat rule. #E63946 = (230, 57, 70): d = 173, h = 360 + 60 * (57 - 70)/173 = 355.49,
     * s = 173/230 = 0.7522, b = 230/255 = 0.9020 */
    tc_hsb red = tc_rgb_to_hsb(TC_BOAT_RED);
    assert(near_float(red.h, 355.49f) && near_float(red.s, 0.7522f) && near_float(red.b, 0.9020f));
    tc_palette boat_same = tc_palette_make_boat(TC_BOAT_RED);
    assert(tc_recolor_rgb(&boat_same, TC_BOAT_RED, &out) && near_rgb(out, TC_BOAT_RED));
    tc_palette boat_blue = tc_palette_make_boat(0x0000FF);
    assert(tc_recolor_rgb(&boat_blue, TC_BOAT_RED, &out) && out == 0x0000FF); /* the reference */
    /* orange (255, 106, 0): h = 60 * 106/255 = 24.94, s = b = 1: ratios clamp to 1 */
    assert(tc_recolor_rgb(&boat_blue, 0xFF6A00, &out) && out == 0x0000FF);
    assert(tc_recolor_rgb(&boat_blue, 0xFF0040, &out)); /* h = 344.9: the band wraps */
    /* a darker orange (200, 120, 40): h = 60 * 80/160 = 30, s = 0.8: paint */
    assert(tc_recolor_rgb(&boat_blue, 0xC87828, &out));
    /* not paint: yellow (h 48.9), pale brown (139, 118, 102): h 25.9 but s = 37/139 = 0.27,
     * near black (b 0.063), grey, the tunic green */
    out = 0x123456;
    assert(!tc_recolor_rgb(&boat_blue, 0xFFD000, &out) && !tc_recolor_rgb(&boat_blue, 0x8B7666, &out));
    assert(!tc_recolor_rgb(&boat_blue, 0x100404, &out) && !tc_recolor_rgb(&boat_blue, 0x808080, &out));
    assert(!tc_recolor_rgb(&boat_blue, TC_LINK_GREEN, &out) && out == 0x123456);
    assert(!tc_recolor_rgb(&red_blue, TC_BOAT_RED, &out)); /* and the tunic rule ignores the boat */
    assert(!tc_recolor_rgb(&red_blue, 0x2040C0, &out) && out == 0x123456);

    /* the head's lighting ramp (toonEX) goes grey at the same brightness, never to the paint */
    tc_palette boat_grey = tc_palette_make_boat_neutral();
    assert(tc_recolor_rgb(&boat_grey, 0xFF0000, &out) && out == 0xFFFFFF);
    assert(tc_recolor_rgb(&boat_grey, 0x800000, &out) && out == 0x808080);
    assert(tc_recolor_rgb(&boat_grey, 0xC87828, &out) && out == 0xC8C8C8);
    assert(!tc_recolor_rgb(&boat_grey, TC_LINK_GREEN, &out));
    int in = 0;
    assert(tc_ramp_endpoint(&boat_blue, 0xFF0000, &in) && in);         /* full red: ramp */
    assert(!tc_ramp_endpoint(&boat_blue, TC_BOAT_RED, &in) && in);     /* painted texture */
    assert(!tc_ramp_endpoint(&boat_blue, 0x808080, &in) && !in);       /* not paint */

    /* RGB565 rounding: 0x0DBD2F -> r 2, g 47, b 6 */
    assert(tc_888_to_565(TC_LINK_GREEN) == 0x15E6 && tc_888_to_565(0xFFFFFF) == 0xFFFF);
    assert(tc_565_to_888(0xF800) == 0xFF0000);

    /* 0x15E6 decodes to (16, 190, 49): s = 0.9158, b = 0.7451, so the red has s = 0.9158/0.9312
     * = 0.9834 and b = 1 (clamped): (255, 4, 4) = 0xF820 in RGB565.
     * BC1, three-colour block (c0 <= c1) whose recoloured endpoints would invert:
     * green 0x15E6 <= grey 0x8410 -> red 0xF820 > 0x8410: swapped, indices 0 <-> 1 only */
    tc_u8 b3[8] = {0xE6, 0x15, 0x10, 0x84, 0xE4, 0xE4, 0xE4, 0xE4};
    tc_recolor_color_block(&red_blue, b3, 1);
    static const tc_u8 b3_expected[8] = {0x10, 0x84, 0x20, 0xF8, 0xE1, 0xE1, 0xE1, 0xE1};
    assert(memcmp(b3, b3_expected, 8) == 0);
    /* BC1, four-colour block (c0 > c1): grey 0x8410 > green -> red above grey: swapped, idx ^ 1 */
    tc_u8 b4[8] = {0x10, 0x84, 0xE6, 0x15, 0xE4, 0xE4, 0xE4, 0xE4};
    tc_recolor_color_block(&red_blue, b4, 1);
    static const tc_u8 b4_expected[8] = {0x20, 0xF8, 0x10, 0x84, 0xB1, 0xB1, 0xB1, 0xB1};
    assert(memcmp(b4, b4_expected, 8) == 0);
    /* four-colour block whose endpoints become equal (black tunic): never the transparent index */
    tc_palette black = tc_palette_make(0x000000, 0x000000);
    tc_u8 be[8] = {0xE6, 0x15, 0xE5, 0x15, 0xFF, 0xFF, 0xFF, 0xFF};
    tc_recolor_color_block(&black, be, 1);
    static const tc_u8 be_expected[8] = {0, 0, 0, 0, 0, 0, 0, 0};
    assert(memcmp(be, be_expected, 8) == 0);
    /* nothing green: untouched */
    tc_u8 skin[8] = {0x10, 0x84, 0x00, 0x00, 0x12, 0x34, 0x56, 0x78};
    tc_u8 skin_copy[8];
    memcpy(skin_copy, skin, 8);
    tc_recolor_color_block(&red_blue, skin, 1);
    assert(memcmp(skin, skin_copy, 8) == 0);

    /* BC3: alpha half untouched, colour half in four-colour mode without reordering */
    tc_u8 bc3[16] = {1, 2, 3, 4, 5, 6, 7, 8, 0xE6, 0x15, 0x10, 0x84, 0xE4, 0xE4, 0xE4, 0xE4};
    assert(tc_recolor_surface(&red_blue, TC_FMT_BC3 | 0x400u, bc3, sizeof(bc3)));
    static const tc_u8 bc3_expected[16] = {1, 2, 3, 4, 5, 6, 7, 8, 0x20, 0xF8, 0x10, 0x84, 0xE4, 0xE4, 0xE4, 0xE4};
    assert(memcmp(bc3, bc3_expected, 16) == 0);

    /* 8_8_8_8: R G B A bytes, alpha kept */
    tc_u8 px[8] = {0x0D, 0xBD, 0x2F, 0x80, 0xF0, 0xC0, 0x90, 0xFF};
    assert(tc_recolor_surface(&red_blue, TC_FMT_8_8_8_8, px, sizeof(px)));
    static const tc_u8 px_expected[8] = {0xFF, 0x00, 0x00, 0x80, 0xF0, 0xC0, 0x90, 0xFF};
    assert(memcmp(px, px_expected, 8) == 0);

    assert(!tc_format_supported(0x08) && !tc_recolor_surface(&red_blue, 0x08, px, sizeof(px)));
    assert(tc_format_supported(TC_FMT_BC1) && tc_format_supported(TC_FMT_BC1 | 0x400u));
}

static void test_split_frame(void) {
    const mp_u8 frame[] = {0x81, 0x83, 1, 2, 3};
    mp_u32 id, length;
    const mp_u8* payload;
    assert(mp_split_frame(frame, sizeof(frame), &id, &payload, &length));
    assert(id == 1 && length == 3 && payload == frame + 2);
    assert(!mp_split_frame(frame, sizeof(frame) - 1, &id, &payload, &length));
}

int main(void) {
    test_vlq();
    test_init();
    test_client_packets();
    test_server_packets();
    test_split_frame();
    test_colors();
    test_launcher_cfg();
    test_player_events();
    test_swings();
    test_strikes();
    test_skin();
    test_directory();
    test_summon();
    test_death();
    test_save_sync();
    test_tunic_colors();
    test_placed_actors();
    puts("mp_protocol_test: ok");
    return 0;
}
