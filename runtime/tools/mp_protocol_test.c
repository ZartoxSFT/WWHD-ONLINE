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

static const mp_transform kTransform = {{1.0f, 2.0f, -1.0f}, {0x1234, -1, 0}};

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
    };
    expect_bytes(&payload, look_update, sizeof(look_update));

    /* the name follows the hit list, NUL-terminated */
    mp_set_name(&red, "Tetra", 5);
    payload = mp_writer_make(buffer, sizeof(buffer));
    mp_write_look_update(&payload, &s, 300, 5, &red);
    assert(payload.ok && payload.size == sizeof(look_update) + 5);
    assert(memcmp(payload.data + sizeof(look_update) - 3, "Tetra", 6) == 0);
    assert(payload.data[payload.size - 2] == 0x00 && payload.data[payload.size - 1] == 0x00); /* no ship, no extras */

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

    /* malformed values are dropped one by one, the rest still applies */
    static const char bad[] =
        "uid=short\nport=70000\ncolor=12345G\ncolor2=#ABCDEF\npuppet=yes\nhost=\nport=abc\n"
        "name=SeventeenLetters!\nhide_names=yes\n";
    assert(mp_parse_launcher_cfg(bad, sizeof(bad) - 1, &cfg));
    assert(!cfg.has_uid && !cfg.has_port && !cfg.has_color && !cfg.has_puppet && !cfg.has_host);
    assert(!cfg.has_name && !cfg.has_hide_names);
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
    mp_write_player_events(&w, &look, &e);
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
    /* every truncation fails, except the ones that drop only the ship (a mod before the ship
     * extension) or the name and the ship (a GameCube client or an older mod ends the component
     * after the hit list) */
    for (mp_u32 cut = 0; cut != w.size - 3; ++cut) assert(!mp_read_actor_update(w.data, cut, &s, &a));
    assert(mp_read_actor_update(w.data, w.size - 3, &s, &a) && a.has_look && a.look.name[0] == 0);
    assert(a.events.move_count == 1 && a.look.ship.flags == 0);
    assert(mp_read_actor_update(w.data, w.size - 2, &s, &a) && a.has_look && a.look.ship.flags == 0);
    assert(mp_read_actor_update(w.data, w.size - 1, &s, &a) && a.has_look && a.look.extras == 0);

    /* the extras: iron boots */
    {
        mp_player_look booted = look;
        booted.extras = MP_EXTRA_HEAVY_BOOTS;
        mp_writer bw = mp_writer_make(buffer, sizeof(buffer));
        mp_write_vlq(&bw, 77); mp_write_u8(&bw, 3); mp_write_vlq(&bw, 77);
        mp_write_vlq(&bw, 1); mp_write_vlq(&bw, 1);
        mp_write_player_events(&bw, &booted, 0);
        assert(bw.ok && bw.data[bw.size - 1] == MP_EXTRA_HEAVY_BOOTS);
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
        mp_write_player_events(&sw, &sailing, 0);
        static const mp_u8 ship_bytes[] = {0x2F, 0x44, 0xBB, 0x90, 0x00, 0xC0, 0x00, 0x00, 0x00,
                                           0xC8, 0x43, 0x50, 0x00, 0x00, 0x0C, 0xC0, 0x00, 0xFF, 0xFD,
                                           0x12, 0x34, 0x56};
        assert(sw.ok && sw.size > sizeof(ship_bytes) + 1 && sw.data[sw.size - 1] == 0x00); /* then no extras */
        assert(memcmp(sw.data + sw.size - 1 - sizeof(ship_bytes), ship_bytes, sizeof(ship_bytes)) == 0);
        assert(mp_read_actor_update(sw.data, sw.size, &s, &a) && a.has_look);
        assert(mp_same_ship(&a.look.ship, &sailing.ship));
        for (mp_u32 cut = sw.size - 1 - sizeof(ship_bytes) + 1; cut != sw.size - 1; ++cut)
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
    update = mp_writer_make(buffer, 2048);
    mp_write_player_update(&update, &s, 0xFFFFFFFFu, 5, &look, &full);
    assert(update.ok && update.size + 16 <= 2048 + 16);

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
    test_tunic_colors();
    puts("mp_protocol_test: ok");
    return 0;
}
