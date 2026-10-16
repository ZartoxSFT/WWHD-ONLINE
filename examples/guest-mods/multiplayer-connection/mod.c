/* Wind Waker multiplayer connection: historical protocol over the host TCP transport.
 *
 * Reproduces the WindWakerClient flow for the local player only:
 *   init blob -> CLIENT_INIT_PACKET (8-byte identity)
 *   stage change -> CLIENT_PLAYER_LEAVE_MANAGER (after the first area),
 *                   CLIENT_PLAYER_CHANGE_MANAGER(stage), CLIENT_ACTOR_SYNC_CREATE(PlayerPuppet)
 *   SERVER_ACTOR_SYNC_CREATE carrying our solver -> our uid
 *   every 0.1 s when moved -> CLIENT_ACTOR_SYNC_UPDATE(ActorBaseSync)
 *   at most 30 times a second when Link's animation calls, item, proc or colours changed
 *                -> CLIENT_ACTOR_SYNC_UPDATE(ActorPlayerSync with its event lists, anim_sync.h)
 * Remote PlayerPuppets are decoded, smoothed and drawn as HUD markers above their heads and,
 * with the experimental "puppet" option, as a second Link actor (puppet.h) that replays the
 * owner's animations and wears its tunic colours.
 * Launcher settings (launcher_cfg.h) override the manifest options and are re-read every
 * second: colour changes recolour the local tunic (tunic.h), the boat body, and are sent to the
 * server while playing.
 *
 * Engine access:
 *  - SDK-verified fields: fopAc_ac_c::current.pos (0x314), fopAc_ac_c::shape_angle (0x328),
 *    the play start stage name (wwhd_play_get() + WWHD_PLAY_START_STAGE_NAME_OFFSET);
 *  - SDK bindings of the game's own projection: mDoLib_pos2camera (025F1108, world to camera
 *    space with the current view matrix) and mDoLib_project (025F0C48, world to the game's
 *    screen rectangle);
 *  - from the public decomp (decomp/wwhd_src/m_Do/m_Do_lib.cpp): wwhd_play_get() is
 *    dComIfGp_get(), its view pointer is at +0x5FA4 (mDoLibGameView::mpView; pos2camera does
 *    not check it), and the projection rectangle is screenWidth/screenHeight (10058FA4/AC).
 * mp_protocol.h knows nothing about the game. */
#include "wwhd_guest.h"
#include "wwhd/functions.h"
#include "wwhd/bindings.h"
#include "wwhd/actor.h"
#include "wwhd/data.h"
#include "mp_protocol.h"
#include "launcher_cfg.h"
#include "puppet.h" /* also anim_sync.h and tunic.h */
#include "name_tag.h"
#include "boat_paint.h"
#include "ship_material.h"
#include "sea_chart.h"
#include "ship_puppet.h"
#include "puppet_sense.h"

#define MAX_REMOTES 8
#define CFG_PERIOD 1.0      /* launcher.cfg re-read interval, seconds */
#define UPDATE_PERIOD 0.1   /* ActorBaseSync::sUpdateRate */
#define PLAYER_PERIOD (1.0 / 30.0) /* ActorPlayerSync updates, at most one per 30 fps frame */
#define LOG_PERIOD 2.0
#define STAGE_NAME_BYTES 8  /* dStage_startStage_c name, NUL-terminated within 8 bytes */
#define AREA_SETTLE_STEPS 10 /* Link is placed a few steps after the stage name changes */

#define PLAY_VIEW_OFFSET 0x5FA4             /* mDoLibGameView::mpView */
#define ADDR_mDoLib_screenWidth 0x10058FA4  /* m_Do_lib.cpp data screenWidth */
#define ADDR_mDoLib_screenHeight 0x10058FAC /* m_Do_lib.cpp data screenHeight */
#define HUD_WIDTH 1280.0f                   /* wwhd_hud base coordinates (TV) */
#define HUD_HEIGHT 720.0f
#define MARKER_HEIGHT 140.0f                /* name baseline above the remote Link's feet */
/* Name fade with the distance between the two Links, in game units (deduced: 1 unit = 1 cm,
 * Link stands about 140 units tall): opaque up to 10 m, fading out, hidden from 20 m. */
#define NAME_FADE_START 1000.0f
#define NAME_HIDE_DISTANCE 2000.0f

enum { IDLE, WAIT_INIT, READY, FAILED };

typedef struct remote_player {
    int active;
    mp_u32 uid;
    int has_transform;
    mp_transform transform; /* last received */
    int has_display;
    mp_transform display;   /* smoothed towards transform every logic step */
    int has_look;
    mp_player_look look;    /* ActorPlayerSync: tunic colours, map cursor */
} remote_player;

typedef struct marker {
    int visible;
    f32 x, y, scale;
    f32 opacity;          /* distance fade, (0, 1] */
    u32 rgb;              /* primary tunic colour 0xRRGGBB */
    char label[MP_NAME_MAX + 1];
    u32 label_bytes;
} marker;

/* 1 MiB transport payload limit plus its VLQ length prefix. */
static unsigned char incoming[1024 * 1024 + 8];
/* An ActorPlayerSync update with every event list full takes about 1.6 KiB. */
static mp_u8 payload[2048];
static mp_u8 frame[2048 + 16];

static int state;
static mp_session session;
static char area[STAGE_NAME_BYTES + 1];
static mp_u32 next_solver;
static mp_u32 pending_solver = MP_NO_ID;
static mp_u32 local_uid = MP_NO_ID;
static mp_u8 local_ticket = 0xFF; /* SyncedActor::mTicket default */
static mp_transform last_sent;
static double update_clock;
static double log_clock;
static remote_player remotes[MAX_REMOTES];
static int area_settle;
static mp_player_look local_look; /* colours and cursor from the launcher/options */
static mp_player_look sent_look;  /* last ActorPlayerSync sent, with item and proc */
static double player_clock;
static double ship_clock;  /* a moving ship is sent at most every UPDATE_PERIOD */
static marker markers[MAX_REMOTES];
/* Projection arguments are passed to game code by guest address, so they live in mod memory. */
static cXyz project_in;
static cXyz project_out;
static mp_launcher_cfg cfg;
static char cfg_text[MP_CFG_MAX_BYTES];
static double cfg_clock;
static int names_hidden;  /* launcher "hide player names" (hide_names), else the option */

static void fail(const char* message) {
    wwhd_log(message);
    state = FAILED;
    wwhd_net_disconnect();
}

static int send_payload(mp_u32 id, const mp_writer* body) {
    if (id == MP_NO_ID || !body->ok) return 0;
    mp_writer w = mp_writer_make(frame, sizeof(frame));
    mp_write_frame(&w, id, body->data, body->size);
    return w.ok && wwhd_net_send(w.data, w.size);
}

static void read_transform(const fopAc_ac_c* actor, mp_transform* t) {
    t->pos[0] = actor->current.pos.x;
    t->pos[1] = actor->current.pos.y;
    t->pos[2] = actor->current.pos.z;
    t->rot[0] = actor->shape_angle.x;
    t->rot[1] = actor->shape_angle.y;
    t->rot[2] = actor->shape_angle.z;
}

static int same_transform(const mp_transform* a, const mp_transform* b) {
    for (int i = 0; i != 3; ++i)
        if (a->pos[i] != b->pos[i] || a->rot[i] != b->rot[i]) return 0;
    return 1;
}

static remote_player* find_remote(mp_u32 uid, int create) {
    remote_player* free_slot = 0;
    for (int i = 0; i != MAX_REMOTES; ++i) {
        if (remotes[i].active && remotes[i].uid == uid) return &remotes[i];
        if (!remotes[i].active && !free_slot) free_slot = &remotes[i];
    }
    if (!create || !free_slot) return 0;
    free_slot->active = 1;
    free_slot->uid = uid;
    free_slot->has_transform = 0;
    free_slot->has_display = 0;
    free_slot->has_look = 0;
    return free_slot;
}

static void clear_area_state(void) {
    for (int i = 0; i != MAX_REMOTES; ++i) remotes[i].active = markers[i].visible = 0;
    puppet_clear_all();
    ship_puppet_clear_all();
    mp_anim_events_clear(&anim_local);
    pending_solver = MP_NO_ID;
    local_uid = MP_NO_ID;
    local_ticket = 0xFF;
}

static void handle_init(const unsigned char* item, u32 size) {
    mp_reader r = mp_reader_make(item, size);
    mp_u32 length = mp_read_vlq(&r);
    if (!r.ok || length != r.size - r.pos) { fail("multiplayer: invalid init packet framing"); return; }
    if (!mp_parse_init(item + r.pos, length, &session))
        { fail("multiplayer: unsupported server init packet"); return; }
    if (session.client_init == MP_NO_ID || session.client_change_manager == MP_NO_ID ||
        session.client_create == MP_NO_ID || session.client_update == MP_NO_ID ||
        !session.has_actor_sync || session.puppet_base_index == MP_NO_ID)
        { fail("multiplayer: server lacks the PlayerPuppet ActorSync packets"); return; }

    char uid[9] = {0};
    if (cfg.has_uid) {
        for (int i = 0; i != 9; ++i) uid[i] = cfg.uid[i];
    } else if (wwhd_config_string("uid", uid, sizeof(uid)) != 8) {
        fail("multiplayer: uid option must contain exactly 8 bytes");
        return;
    }
    mp_writer body = mp_writer_make(payload, sizeof(payload));
    for (int i = 0; i != 8; ++i) mp_write_u8(&body, (mp_u8)uid[i]);
    if (!send_payload(session.client_init, &body)) { fail("multiplayer: init send failed"); return; }
    wwhd_log("multiplayer: connected, identity sent");
    state = READY;
}

static void handle_frame(const unsigned char* item, u32 size) {
    mp_u32 id, length;
    const mp_u8* body;
    static mp_actor_state actor; /* static: its event lists are too large for the stack */
    if (!mp_split_frame(item, size, &id, &body, &length))
        { fail("multiplayer: malformed frame"); return; }

    if (id == session.server_create) {
        if (!mp_read_actor_create(body, length, &session, &actor))
            { fail("multiplayer: malformed actor create"); return; }
        if (!actor.is_puppet) return; /* enemies/props are not handled yet */
        if (actor.has_solver && actor.solver == pending_solver) {
            local_uid = actor.uid;
            local_ticket = actor.ticket;
            pending_solver = MP_NO_ID;
            wwhd_log_int("multiplayer: local player uid", (int)local_uid);
            return;
        }
        remote_player* remote = find_remote(actor.uid, 1);
        if (!remote) { wwhd_log("multiplayer: remote player table full"); return; }
        if (actor.has_transform) {
            remote->transform = actor.transform;
            remote->has_transform = 1;
        }
        if (actor.has_look) {
            remote->look = actor.look;
            remote->has_look = 1;
            ship_puppet_set(actor.uid, &actor.look.ship);
        }
        wwhd_log_int("multiplayer: remote player joined, uid", (int)actor.uid);
        /* the events of a create are history (the moment the player entered): not replayed */
    } else if (id == session.server_update) {
        mp_u32 uid = mp_read_update_uid(body, length);
        remote_player* remote = find_remote(uid, 0);
        if (uid != local_uid && !remote) return; /* not a PlayerPuppet we track */
        if (!mp_read_actor_update(body, length, &session, &actor))
            { fail("multiplayer: malformed actor update"); return; }
        if (uid == local_uid) {
            local_ticket = actor.ticket;
        } else {
            if (actor.has_transform) {
                remote->transform = actor.transform;
                remote->has_transform = 1;
            }
            if (actor.has_look) {
                remote->look = actor.look;
                remote->has_look = 1;
                puppet_queue_events(uid, &actor.events);
                ship_puppet_set(uid, &actor.look.ship);
            }
        }
    } else if (id == session.server_remove) {
        remote_player* remote = find_remote(mp_read_actor_remove(body, length), 0);
        if (remote) {
            remote->active = 0;
            puppet_remove(remote->uid);
            ship_puppet_remove(remote->uid);
            wwhd_log_int("multiplayer: remote player left, uid", (int)remote->uid);
        }
    }
    /* other server packets (downloads, save updates) are ignored for now */
}

/* ActorPlayerSync as the owner's ActorPlayerSync::calc sees it: launcher colours and cursor,
 * the equipped item (daPy_lk_c::mEquipItem) and the current proc (mCurProc), SDK fields. */
static mp_player_look current_look(const daPy_lk_c* link) {
    mp_player_look look = local_look;
    look.item = link->mEquipItem;
    look.proc = (mp_u32)link->mCurProc;
    look.ship = ship_local_state();
    look.extras = (*(const volatile u32*)((const u8*)link + PUPPET_NO_RESET_FLG0) & PUPPET_HEAVY_BOOTS) ? MP_EXTRA_HEAVY_BOOTS : 0;
    return look;
}

static void enter_area(const char* stage, const mp_transform* t, const daPy_lk_c* link) {
    mp_writer body;
    if (area[0] && session.client_leave_manager != MP_NO_ID) {
        body = mp_writer_make(payload, 0);
        send_payload(session.client_leave_manager, &body);
    }
    for (int i = 0; i != STAGE_NAME_BYTES + 1; ++i) area[i] = stage[i];
    clear_area_state();

    body = mp_writer_make(payload, sizeof(payload));
    mp_write_change_manager(&body, area);
    if (!send_payload(session.client_change_manager, &body))
        { fail("multiplayer: change manager send failed"); return; }

    body = mp_writer_make(payload, sizeof(payload));
    pending_solver = next_solver++;
    sent_look = current_look(link);
    mp_write_player_create(&body, &session, pending_solver, t, &sent_look);
    if (!send_payload(session.client_create, &body))
        { fail("multiplayer: player create send failed"); return; }
    last_sent = *t;
    wwhd_log("multiplayer: entered area");
    wwhd_log(area);
}

static void log_remotes(void) {
    for (int i = 0; i != MAX_REMOTES; ++i) {
        if (!remotes[i].active || !remotes[i].has_transform) continue;
        wwhd_log_int("multiplayer: remote uid", (int)remotes[i].uid);
        wwhd_log_float("multiplayer:   x", remotes[i].transform.pos[0]);
        wwhd_log_float("multiplayer:   y", remotes[i].transform.pos[1]);
        wwhd_log_float("multiplayer:   z", remotes[i].transform.pos[2]);
    }
}

/* ------------------------------------------------------------------ remote markers */

static void hide_markers(void) {
    for (int i = 0; i != MAX_REMOTES; ++i) markers[i].visible = 0;
}

static u32 format_label(char* out, mp_u32 uid) {
    char digits[10];
    u32 count = 0, size = 0;
    do {
        digits[count++] = (char)('0' + uid % 10);
        uid /= 10;
    } while (uid && count != sizeof(digits));
    out[size++] = 'J';
    while (count) out[size++] = digits[--count];
    return size;
}

/* Square root for x in [NAME_FADE_START^2, NAME_HIDE_DISTANCE^2]: the 750 has no fsqrt and the
 * guest has no libm. Newton from the fade range midpoint, error far below a unit. */
static f32 marker_sqrt(f32 x) {
    f32 r = (NAME_FADE_START + NAME_HIDE_DISTANCE) * 0.5f;
    for (int i = 0; i != 6; ++i) r = 0.5f * (r + x / r);
    return r;
}

/* Smooths each remote position towards the last received one (updates come every 0.1 s,
 * like ActorBaseSync's remote interpolation) and projects it with the game's own camera. */
static void update_markers(double dt, const fopAc_ac_c* local) {
    f32 blend = (f32)(dt / UPDATE_PERIOD);
    if (blend > 1.0f) blend = 1.0f;
    const u8* play = wwhd_play_get();
    int view = play && *(const volatile u32*)(play + PLAY_VIEW_OFFSET) != 0;
    f32 screen_w = WWHD_GAME_DATA(ADDR_mDoLib_screenWidth, f32);
    f32 screen_h = WWHD_GAME_DATA(ADDR_mDoLib_screenHeight, f32);

    for (int i = 0; i != MAX_REMOTES; ++i) {
        remote_player* r = &remotes[i];
        marker* m = &markers[i];
        m->visible = 0;
        if (!r->active || !r->has_transform) continue;
        if (!r->has_display) {
            r->display = r->transform;
            r->has_display = 1;
        } else {
            for (int k = 0; k != 3; ++k)
                r->display.pos[k] += (r->transform.pos[k] - r->display.pos[k]) * blend;
            for (int k = 0; k != 3; ++k) r->display.rot[k] = r->transform.rot[k];
        }
        puppet_set_target(r->uid, &r->display);
        if (r->has_look) puppet_set_look(r->uid, (mp_u16)r->look.colors[0], (mp_u16)r->look.colors[1], r->look.proc,
                                         r->look.item, r->look.extras);
        if (!view || screen_w <= 0.0f || screen_h <= 0.0f) continue;

        f32 dx = r->display.pos[0] - local->current.pos.x;
        f32 dy = r->display.pos[1] - local->current.pos.y;
        f32 dz = r->display.pos[2] - local->current.pos.z;
        f32 distance2 = dx * dx + dy * dy + dz * dz;
        if (!(distance2 < NAME_HIDE_DISTANCE * NAME_HIDE_DISTANCE)) continue;
        f32 opacity = 1.0f;
        if (distance2 > NAME_FADE_START * NAME_FADE_START) {
            f32 distance = marker_sqrt(distance2);
            opacity = (NAME_HIDE_DISTANCE - distance) / (NAME_HIDE_DISTANCE - NAME_FADE_START);
        }

        project_in.x = r->display.pos[0];
        project_in.y = r->display.pos[1] + MARKER_HEIGHT;
        project_in.z = r->display.pos[2];
        wwhd_mDoLib_pos2camera((u32)&project_in, (u32)&project_out);
        f32 depth = -project_out.z; /* the GX view looks down -Z */
        if (depth < 20.0f) continue;
        wwhd_mDoLib_project((u32)&project_in, (u32)&project_out);
        f32 x = project_out.x / screen_w * HUD_WIDTH;
        f32 y = project_out.y / screen_h * HUD_HEIGHT;
        if (x < -40.0f || x > HUD_WIDTH + 40.0f || y < -40.0f || y > HUD_HEIGHT + 40.0f) continue;

        /* name height: 0.7 x the 40 px atlas up close, down to 0.25 x far away */
        f32 scale = 400.0f / depth;
        if (scale < 0.25f) scale = 0.25f;
        if (scale > 0.7f) scale = 0.7f;
        m->x = x;
        m->y = y;
        m->scale = scale;
        m->opacity = opacity;
        m->label_bytes = 0;
        if (r->has_look)
            while (m->label_bytes != MP_NAME_MAX && r->look.name[m->label_bytes]) {
                m->label[m->label_bytes] = r->look.name[m->label_bytes];
                ++m->label_bytes;
            }
        if (!m->label_bytes) m->label_bytes = format_label(m->label, r->uid); /* no name sent */
        m->rgb = r->has_look ? mp_rgb565_to_888((mp_u16)r->look.colors[0]) : 0x2FD06Au;
        m->visible = 1;
    }
}

/* ------------------------------------------------------------------ sea chart */

/* The menus pause the game: the local Link's execute, which reads the network every step, stops
 * while the MAP screen is open. The sea chart's update (sea_chart.h) reads it instead, so the
 * remote players' arrows keep moving; it only takes what the server sent (positions, looks). */
static void multiplayer_pump(void) {
    if (state != READY || wwhd_net_status() != 2) return;
    for (;;) {
        int received = wwhd_net_receive(incoming, sizeof(incoming));
        if (received <= 0) break;
        handle_frame(incoming, (u32)received);
        if (state != READY) return;
    }
}

#define CHART_ARROW_SCALE 1.0f    /* remote icon size / the game's Link icon */
#define CHART_FACE_ARROW 0.5f     /* heading arrow beside the icon / icon size */
#define CHART_NAME_SCALE 0.28f    /* name height on the chart (name_tag.h scale) */
#define CHART_DEFAULT_CURSOR 0xFFFF4Du

static u32 chart_arrow_texture;
static unsigned long long chart_arrow_epoch = ~0ull;
static wwhd_hud_element chart_element;

/* Each remote player's Link icon: the game's icon (sea_chart.h) with its green in the player's
 * tunic colours, made again when they, the decoded icon or the HUD epoch change. */
typedef struct chart_face_image {
    u32 image;
    tunic_look look;
    u32 generation;
    unsigned long long epoch;
} chart_face_image;
static chart_face_image chart_faces[MAX_REMOTES];
static u8* chart_face_work;

static u32 chart_face_for(int slot, const remote_player* r, unsigned long long epoch) {
    if (!chart_face_w || !chart_face_rgba) return 0;
    tunic_look look = tunic_look_make(TUNIC_DEFAULT_PRIMARY, TUNIC_DEFAULT_SECONDARY);
    if (r->has_look) look = tunic_look_make((u16)r->look.colors[0], (u16)r->look.colors[1]);
    chart_face_image* f = &chart_faces[slot];
    if (f->image && f->epoch == epoch && f->generation == chart_face_generation && tunic_same_look(&f->look, &look))
        return f->image;
    if (f->image && f->epoch == epoch) wwhd_hud_release(f->image); /* older epochs: already gone */
    f->image = 0;
    u32 bytes = chart_face_w * chart_face_h * 4;
    if (!chart_face_work) chart_face_work = (u8*)wwhd_malloc(CHART_FACE_MAX_BYTES);
    if (!chart_face_work) return 0;
    memcpy(chart_face_work, chart_face_rgba, bytes);
    if (look.custom) {
        tc_palette p = tc_palette_make(tc_565_to_888(look.primary), tc_565_to_888(look.secondary));
        for (u32 i = 0; i < bytes; i += 4) {
            u8* px = chart_face_work + i;
            if (!px[3]) continue;
            tc_u32 out;
            if (tc_recolor_rgb(&p, ((tc_u32)px[0] << 16) | ((tc_u32)px[1] << 8) | px[2], &out)) {
                px[0] = (u8)(out >> 16);
                px[1] = (u8)(out >> 8);
                px[2] = (u8)out;
            }
        }
    }
    f->image = wwhd_hud_image(chart_face_work, chart_face_w, chart_face_h);
    f->look = look;
    f->generation = chart_face_generation;
    f->epoch = epoch;
    return f->image;
}

static int sea_stage(void) {
    return area[0] == 's' && area[1] == 'e' && area[2] == 'a' && area[3] == 0;
}

/* The remote players on the sea chart of the MAP screen, as the GameCube client drew their cursor
 * on the map (ActorPlayerSync mPlayerCursor): the game's Link icon in their tunic colours at their
 * place, an arrow in their cursor colour beside it towards their heading, and their name under it. Only on the Great Sea ("sea"), where their
 * positions are sea coordinates. k: screen units per layout unit (the GamePad: 854 / 1280). */
static void draw_chart_players(u32 list, f32 k) {
    chart_view v;
    if (state != READY || !sea_stage() || !chart_on_screen(&v)) return;
    unsigned long long epoch = wwhd_hud_epoch();
    if (epoch != chart_arrow_epoch) {
        chart_arrow_epoch = epoch;
        chart_arrow_texture = wwhd_hud_texture(WWHD_HUD_PACKAGE, "assets/map_player.png");
        if (!chart_arrow_texture) wwhd_log("multiplayer: sea chart arrow not loaded, dots used");
    }
    f32 size = v.icon * CHART_ARROW_SCALE * k;
    for (int i = 0; i != MAX_REMOTES; ++i) {
        const remote_player* r = &remotes[i];
        /* the last received place and heading: the menu pauses the game, so the smoothed place
         * (update_markers, in the local Link's execute) does not move while the chart is open */
        if (!r->active || !r->has_transform) continue;
        f32 wx = r->transform.pos[0], wz = r->transform.pos[2];
        if (!(wx > -CHART_SEA_HALF && wx < CHART_SEA_HALF && wz > -CHART_SEA_HALF && wz < CHART_SEA_HALF)) continue;
        f32 x = (v.ox + wx * v.kx) * k, y = (v.oy + wz * v.ky) * k;
        if (!chart_finite(x) || !chart_finite(y)) continue;
        u32 rgb = CHART_DEFAULT_CURSOR;
        if (r->has_look) rgb = ((u32)r->look.cursor[0] << 16) | ((u32)r->look.cursor[1] << 8) | r->look.cursor[2];
        u32 alpha = (u32)(v.alpha * 255.0f + 0.5f);
        if (alpha > 255) alpha = 255;
        u32 face = chart_face_for(i, r, epoch);
        if (chart_arrow_texture) {
            /* the arrow points up at rotation 0; heading 0 faces +z (south, down on the chart) and
             * the HUD turns clockwise on the y-down screen: rotation = pi - heading. With the icon
             * it sits beside it, towards the heading (screen direction (sin h, cos h)). */
            f32 heading = (f32)r->transform.rot[1] * (6.28318531f / 65536.0f);
            f32 arrow = face ? size * CHART_FACE_ARROW : size;
            f32 ax = x, ay = y;
            if (face) {
                f32 h = heading, s2 = h * h; /* sin, cos by Taylor on [-pi, pi]: no libm */
                while (h > 3.14159265f) h -= 6.28318531f;
                while (h < -3.14159265f) h += 6.28318531f;
                s2 = h * h;
                f32 sn = h * (1.0f - s2 / 6.0f * (1.0f - s2 / 20.0f * (1.0f - s2 / 42.0f * (1.0f - s2 / 72.0f))));
                f32 cs = 1.0f - s2 / 2.0f * (1.0f - s2 / 12.0f * (1.0f - s2 / 30.0f * (1.0f - s2 / 56.0f * (1.0f - s2 / 90.0f))));
                ax += sn * size * 0.55f;
                ay += cs * size * 0.55f;
            }
            chart_element = (wwhd_hud_element){
                .kind = WWHD_HUD_IMAGE, .anchor = WWHD_HUD_CENTER, .blend = WWHD_HUD_ALPHA,
                .x = ax - arrow * 0.5f, .y = ay - arrow * 0.5f, .w = arrow, .h = arrow, .thickness = 1,
                .rotation = 3.14159265f - heading, .u1 = 1, .v1 = 1,
                .rgba = (rgb << 8) | alpha, .image = chart_arrow_texture};
        } else {
            chart_element = (wwhd_hud_element){
                .kind = WWHD_HUD_CIRCLE, .anchor = WWHD_HUD_CENTER, .x = x, .y = y,
                .size = size * 0.3f, .thickness = 1, .u1 = 1, .v1 = 1, .rgba = (rgb << 8) | alpha};
        }
        wwhd_hud_emit(list, &chart_element);
        if (face) {
            chart_element = (wwhd_hud_element){
                .kind = WWHD_HUD_IMAGE, .anchor = WWHD_HUD_CENTER, .blend = WWHD_HUD_ALPHA,
                .x = x - size * 0.5f, .y = y - size * 0.5f, .w = size, .h = size, .thickness = 1,
                .u1 = 1, .v1 = 1, .rgba = 0xFFFFFF00u | alpha, .image = face};
            wwhd_hud_emit(list, &chart_element);
        }
        if (names_hidden) continue;
        char label[MP_NAME_MAX + 1];
        u32 bytes = 0;
        if (r->has_look)
            while (bytes != MP_NAME_MAX && r->look.name[bytes]) {
                label[bytes] = r->look.name[bytes];
                ++bytes;
            }
        if (!bytes) bytes = format_label(label, r->uid);
        f32 scale = CHART_NAME_SCALE * k * (v.icon / 32.0f);
        if (scale < 0.18f * k) scale = 0.18f * k;
        if (scale > 0.4f * k) scale = 0.4f * k;
        name_draw(list, x, y + size * 0.5f + (f32)NAME_FONT_CAP * scale + 2.0f * k, label, bytes, rgb, scale, v.alpha);
    }
}

/* The GamePad's own list (wwhd_hud_register_drc): the sea chart, when the menus are on the GamePad. */
static void draw_gamepad(u32 list) {
    if (!chart_menus_to_tv()) draw_chart_players(list, CHART_DRC_SCALE);
}

/* Names above the remote players' heads (name_tag.h), baseline at the projected head point. With
 * the Pro Controller the MAP screen is on the TV: its sea chart instead (the game is in its menu). */
static void draw_markers(u32 list) {
    chart_view v;
    if (chart_menus_to_tv() && chart_on_screen(&v)) {
        draw_chart_players(list, 1.0f);
        return;
    }
    if (names_hidden) return;
    for (int i = 0; i != MAX_REMOTES; ++i) {
        const marker* m = &markers[i];
        if (!m->visible) continue;
        name_draw(list, m->x, m->y, m->label, m->label_bytes, m->rgb, m->scale, m->opacity);
    }
}

/* ------------------------------------------------------------------ lifecycle */

static void read_launcher_cfg(void) {
    s32 n = wwhd_file_read("launcher.cfg", cfg_text, sizeof(cfg_text));
    if (n < 0 || !mp_parse_launcher_cfg(cfg_text, (unsigned int)n, &cfg)) {
        mp_launcher_cfg empty = {0};
        cfg = empty;
    }
}

static int option_rgb(const char* id, unsigned int* rgb) {
    char text[16] = {0};
    wwhd_config_string(id, text, sizeof(text));
    unsigned int length = 0;
    while (text[length]) ++length;
    return length && mp_parse_rgb(text, length, rgb);
}

static void refresh_ship_color_setting(void) {
    unsigned int rgb = 0;
    ship_color_custom = cfg.has_ship_color
        ? (rgb = cfg.ship_color, 1)
        : option_rgb("ship_color", &rgb);
    ship_color_rgb = rgb;
    boat_set_paint(ship_color_custom, rgb); /* textured paint (boat_paint.h) */
}

/* Tunic colours sent in ActorPlayerSync: launcher.cfg, else the options, else the original
 * tunic (the GameCube launcher wrote them into Dolphin memory instead). */
static mp_player_look wanted_look(void) {
    mp_player_look look = mp_player_look_default();
    unsigned int primary = 0, secondary = 0;
    int has_primary, has_secondary;
    if (cfg.has_color) { primary = cfg.color; has_primary = 1; }
    else has_primary = option_rgb("color", &primary);
    if (cfg.has_color2) { secondary = cfg.color2; has_secondary = 1; }
    else has_secondary = option_rgb("color2", &secondary);
    if (has_primary)
        mp_player_look_set_colors(&look, mp_rgb888_to_565(primary),
                                  mp_rgb888_to_565(has_secondary ? secondary : primary));
    if (cfg.has_cursor) {
        look.cursor[0] = (mp_u8)(cfg.cursor >> 16);
        look.cursor[1] = (mp_u8)(cfg.cursor >> 8);
        look.cursor[2] = (mp_u8)cfg.cursor;
    }
    if (cfg.has_name) {
        mp_set_name(&look, cfg.name, MP_NAME_MAX);
    } else {
        char name[MP_NAME_MAX + 1] = {0};
        wwhd_config_string("name", name, sizeof(name));
        mp_set_name(&look, name, MP_NAME_MAX);
    }
    return look;
}

static int same_look(const mp_player_look* a, const mp_player_look* b) {
    for (int i = 0; i != 5; ++i) if (a->colors[i] != b->colors[i]) return 0;
    for (int i = 0; i != 4; ++i) if (a->cursor[i] != b->cursor[i]) return 0;
    for (int i = 0; i != MP_NAME_MAX; ++i) if (a->name[i] != b->name[i]) return 0;
    return a->item == b->item && a->proc == b->proc && a->extras == b->extras;
}

static void apply_puppet_setting(void) {
    int wanted = cfg.has_puppet ? cfg.puppet : wwhd_config_bool("puppet", 0);
    if (wanted == puppet_enabled) return;
    puppet_enabled = wanted;
    if (!wanted) {
        puppet_clear_all();
        ship_puppet_clear_all();
    }
    wwhd_log_int("multiplayer: remote Link puppets", wanted);
}

/* Re-reads launcher.cfg every second and keeps the local tunic texture in its colours, with or
 * without a server. Colour changes reach the server through send_player_state. */
static void refresh_launcher_settings(const daPy_lk_c* link, double dt) {
    cfg_clock -= dt;
    if (cfg_clock > 0) return;
    cfg_clock = CFG_PERIOD;
    read_launcher_cfg();
    apply_puppet_setting();
    names_hidden = cfg.has_hide_names ? cfg.hide_names : wwhd_config_bool("hide_names", 0);
    sense_enabled = wwhd_config_bool("actors_see_puppets", 1);
    refresh_ship_color_setting();
    mp_player_look look = wanted_look();
    if (!same_look(&look, &local_look)) {
        local_look = look;
        wwhd_log("multiplayer: launcher colours changed");
    }
    tunic_set_local((u16)local_look.colors[0], (u16)local_look.colors[1]);
    tunic_refresh((u32)link);
}

WWHD_HOOK_RETURN(WWHD_ADDR_daShip_c__execute, after_ship_execute, (void* ship)) {
    apply_ship_color(ship);
    boat_refresh((u32)ship);
}

WWHD_HOOK_RETURN(WWHD_ADDR_dScnKy_env_light_c_setLightTevColorType,
                 after_ship_lighting, (u32 environment, u32 model, u32 tev)) {
    (void)environment;
    (void)tev;
    for (int m = 0; m != SHIP_MODELS; ++m)
        if (model && model == ship_paint[m].model) apply_ship_model_color(&ship_paint[m], model);
}

/* ActorPlayerSync update (ActorSync::calcWriters when the component is invalidated): sent when
 * Link's animation calls were recorded or the colours, item or proc changed. */
static void send_player_state(const daPy_lk_c* link, double dt) {
    player_clock -= dt;
    if (player_clock > 0) return;
    player_clock = PLAYER_PERIOD;
    mp_player_look look = current_look(link);
    ship_clock -= PLAYER_PERIOD;
    /* a ship that only moved waits for its period; a change of its flags goes at once */
    int ship_due = !mp_same_ship(&look.ship, &sent_look.ship) &&
                   (ship_clock <= 0 || look.ship.flags != sent_look.ship.flags);
    if (!ship_due) look.ship = sent_look.ship;
    if (mp_anim_events_empty(&anim_local) && same_look(&look, &sent_look) && !ship_due) return;
    if (ship_due) ship_clock = UPDATE_PERIOD;
    mp_writer body = mp_writer_make(payload, sizeof(payload));
    mp_write_player_update(&body, &session, local_uid, local_ticket, &look, &anim_local);
    int colours = 0;
    for (int i = 0; i != 5; ++i) colours |= look.colors[i] != sent_look.colors[i];
    if (send_payload(session.client_update, &body)) {
        if (colours) wwhd_log("multiplayer: tunic colours sent");
        sent_look = look;
    } else if (!body.ok) {
        wwhd_log("multiplayer: player update too large, events dropped");
    }
    mp_anim_events_clear(&anim_local);
}

static void start(void) {
    char host[256] = {0};
    read_launcher_cfg();
    local_look = wanted_look();
    apply_puppet_setting();
    state = FAILED;
    if (cfg.has_host) {
        for (u32 i = 0; i != sizeof(host); ++i) host[i] = cfg.host[i];
    } else if (!wwhd_config_string("host", host, sizeof(host))) {
        wwhd_log("multiplayer: host option is empty");
        return;
    }
    tunic_remote_enabled = wwhd_config_bool("remote_tunics", 0);
    int port = cfg.has_port ? (int)cfg.port : wwhd_config_int("port", 10014);
    if (port < 1 || port > 65535) { wwhd_log("multiplayer: invalid port"); return; }
    if (!wwhd_net_connect(host, (u32)port)) { wwhd_log("multiplayer: connect rejected"); return; }
    state = WAIT_INIT;
}

static void stop_remote_views(void) {
    hide_markers();
    puppet_clear_all();
    ship_puppet_clear_all();
}

static void multiplayer_connection_step(void* link) {
    if (state == IDLE) {
        start();
        wwhd_hud_register(draw_markers, WWHD_HUD_TV);
        wwhd_hud_register_drc(draw_gamepad);
    }
    double dt = wwhd_logic_dt();
    refresh_launcher_settings((const daPy_lk_c*)link, dt);
    anim_capture = state == READY && local_uid != MP_NO_ID;
    if (!anim_capture) mp_anim_events_clear(&anim_local);
    if (state == FAILED) {
        stop_remote_views();
        return;
    }

    u32 status = wwhd_net_status();
    if (status == 3 || (status == 0 && state == READY)) {
        wwhd_log_int("multiplayer: connection lost, error", wwhd_net_last_error());
        state = FAILED;
        stop_remote_views();
        return;
    }
    if (status != 2) return;

    for (;;) {
        int received = wwhd_net_receive(incoming, sizeof(incoming));
        if (received <= 0) break;
        if (state == WAIT_INIT) handle_init(incoming, (u32)received);
        else handle_frame(incoming, (u32)received);
        if (state == FAILED) return;
    }
    if (state != READY) return;

    mp_transform now;
    read_transform((const fopAc_ac_c*)link, &now);

    char stage[STAGE_NAME_BYTES + 1] = {0};
    const u8* play = wwhd_play_get();
    if (!play) return;
    for (int i = 0; i != STAGE_NAME_BYTES; ++i) stage[i] = (char)play[WWHD_PLAY_START_STAGE_NAME_OFFSET + i];
    if (!stage[0]) return;
    int changed = 0;
    for (int i = 0; i != STAGE_NAME_BYTES + 1; ++i)
        if (stage[i] != area[i]) changed = 1;
    if (changed) {
        /* Wait for Link to be placed in the new stage before announcing its position. */
        hide_markers();
        if (++area_settle < AREA_SETTLE_STEPS) return;
        area_settle = 0;
        enter_area(stage, &now, (const daPy_lk_c*)link);
        return;
    }
    area_settle = 0;

    update_markers(dt, (const fopAc_ac_c*)link);
    puppet_step((const fopAc_ac_c*)link);
    ship_puppet_step();
    update_clock -= dt;
    if (local_uid != MP_NO_ID && update_clock <= 0) {
        update_clock += UPDATE_PERIOD;
        if (update_clock < 0) update_clock = UPDATE_PERIOD;
        if (!same_transform(&now, &last_sent)) {
            mp_writer body = mp_writer_make(payload, sizeof(payload));
            mp_write_transform_update(&body, &session, local_uid, local_ticket, &now);
            if (send_payload(session.client_update, &body)) last_sent = now;
        }
    }
    if (local_uid != MP_NO_ID) send_player_state((const daPy_lk_c*)link, dt);

    log_clock -= dt;
    if (log_clock <= 0) {
        log_clock = LOG_PERIOD;
        log_remotes();
    }
}

WWHD_GAME_ORIGINAL(WWHD_ADDR_daPy_Execute, s32, daPy_Execute_original, (void* i_this));

/* daPy_Execute (0240EBB0): a puppet only runs its visual update; any other Link runs the game's
 * execute, then the multiplayer step (an entry hook before puppets existed). */
WWHD_REPLACE(WWHD_ADDR_daPy_Execute, s32, multiplayer_daPy_Execute, (void* i_this)) {
    if (puppet_is(i_this)) return puppet_execute(i_this);
    s32 result = daPy_Execute_original(i_this);
    multiplayer_connection_step(i_this);
    return result;
}
