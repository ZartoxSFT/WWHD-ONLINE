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
 * Placed actors of the server's ActorFilter list (enemies, props) are synchronised with the
 * historical ownership model, and the server's enemy multiplier creates the enemy copies
 * (actor_sync.h).
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
#include "actor_sync.h"
#include "particle_sync.h"
#include "sword_swing.h"
#include "dungeon_map.h"

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
    int dead;             /* a ghost (death.h): the skull instead of the name */
} marker;

/* 1 MiB transport payload limit plus its VLQ length prefix. */
static unsigned char incoming[1024 * 1024 + 8];
/* An ActorPlayerSync update with every event list and the sword swings full takes about 2.2 KiB. */
static mp_u8 payload[4096];
static mp_u8 frame[4096 + 16];

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
static int hud_hidden;    /* launcher "hide the HUD" (hide_hud), else the option */

/* hdIngameUi_c::drawHudScreens (02715CA0, this and the draw info; runtime/src/mods/
 * story_lyt_screen.cpp names it): the HD HUD's layouts on the TV (hearts, magic, rupees, the
 * buttons, the mini map). While the HUD is hidden it is not called, except with a Start menu open
 * (with the Pro Controller the item menu is one of those screens) or during an event (the message
 * windows). 0.2.47 skipped dMeter_Draw (02593630), the GameCube meter's draw: it changed nothing
 * on screen. The port's own hook on it (story notebook) stays outside the mods' layer. */
static int chart_menu_open(void);
WWHD_GAME_ORIGINAL(0x02715CA0, void, hud_screens_draw_original, (u32 self, u32 info));
WWHD_REPLACE(0x02715CA0, void, hud_screens_draw, (u32 self, u32 info)) {
    const u8* play = wwhd_play_get();
    if (hud_hidden && play && !play[WWHD_PLAY_EVENT_RUNNING_OFFSET] && !chart_menu_open()) return;
    hud_screens_draw_original(self, info);
}

static void fail(const char* message) {
    wwhd_log(message);
    state = FAILED;
    wwhd_net_disconnect();
}

/* An actor packet that does not decode (frames are length-delimited, so the next one is still
 * read): logged with its uid and size, the first times and then every 100th, and skipped. Before
 * 0.2.30 it closed the connection; the Java server's VLQ of negative values (fixed in
 * ActorPlayerSync.java) made every remote player's animation with an end of -1 do so. */
static u32 dropped_frames;

static void drop_frame(const char* what, const mp_u8* body, mp_u32 length) {
    ++dropped_frames;
    if (dropped_frames > 5 && dropped_frames % 100) return;
    wwhd_log(what);
    wwhd_log_int("multiplayer:   uid", (int)mp_read_update_uid(body, length));
    wwhd_log_int("multiplayer:   bytes", (int)length);
    wwhd_log_int("multiplayer:   skipped so far", (int)dropped_frames);
}

static int send_payload(mp_u32 id, const mp_writer* body) {
    if (id == MP_NO_ID || !body->ok) return 0;
    mp_writer w = mp_writer_make(frame, sizeof(frame));
    mp_write_frame(&w, id, body->data, body->size);
    return w.ok && wwhd_net_send(w.data, w.size);
}

#include "save_sync.h"  /* the shared progression (SaveServer) */
#include "world_sync.h" /* the world's clock and weather */
#include "elite.h"      /* dynamic scaling and elite enemies */
#include "bonus.h"      /* yellow hearts and magic bought in the shop */
static void wd_roster_retry(void) { as_roster_retry(); }

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
    swing_clear_all();
    mp_anim_events_clear(&anim_local);
    pending_solver = MP_NO_ID;
    local_uid = MP_NO_ID;
    local_ticket = 0xFF;
    as_reset_network();
    ps_clear();
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
    as_session = &session;
    as_mutators_changed(); /* the area loaded before the connection takes the server's values */
    wwhd_log_int("multiplayer: actor names to synchronise", (int)session.filter_count);
    wwhd_log_int("multiplayer: server enemy multiplier", (int)session.enemy_multiplier);
    wwhd_log_int("multiplayer: server dynamic scaling", session.dynamic_scaling);
    wwhd_log_int("multiplayer: server elite chance (percent)", (int)session.elite_chance);
    if (!session.factories_complete) wwhd_log("multiplayer: too many ActorSync factories, placed actors stay local");
    state = READY;
    sv_open(&session); /* the SaveServer map came with the init */
}

static void directory_received(const mp_u8* body, mp_u32 length); /* the launcher map section */
static void shop_received(const mp_u8* body, mp_u32 length);      /* the launcher shop section */
static char shop_notice[64];  /* "X sent you a Moblin!" (shop section), drawn by draw_markers */
static u32 shop_notice_bytes;
static int shop_notice_steps;
static mp_directory directory; /* the server's player directory (launcher map section) */
static int directory_valid;
#include "death.h"    /* no game over while another player of the stage lives */

static void handle_frame(const unsigned char* item, u32 size) {
    mp_u32 id, length;
    const mp_u8* body;
    static mp_actor_state actor; /* static: its event lists are too large for the stack */
    if (!mp_split_frame(item, size, &id, &body, &length))
        { fail("multiplayer: malformed frame"); return; }

    if (id == session.server_create) {
        if (!mp_read_actor_create(body, length, &session, &actor))
            { drop_frame("multiplayer: malformed actor create skipped", body, length); return; }
        if (!actor.is_puppet) { /* a placed actor (actor_sync.h) */
            if (!as_handle_create(body, length))
                drop_frame("multiplayer: malformed placed actor create skipped", body, length);
            return;
        }
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
        int placed = as_handle_update(body, length);
        if (placed < 0) { drop_frame("multiplayer: malformed placed actor update skipped", body, length); return; }
        if (placed) return;
        mp_u32 uid = mp_read_update_uid(body, length);
        remote_player* remote = find_remote(uid, 0);
        if (uid != local_uid && !remote) return; /* not a PlayerPuppet we track */
        if (!mp_read_actor_update(body, length, &session, &actor))
            { drop_frame("multiplayer: malformed actor update skipped", body, length); return; }
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
                swing_queue(uid, &actor.events);
                as_queue_strikes(uid, &actor.events); /* sword hits on placed actors */
                ps_queue_particles(uid, &actor.events); /* its Link's particle effects */
                ship_puppet_set(uid, &actor.look.ship);
            }
        }
    } else if (id == session.server_remove) {
        mp_u32 removed = mp_read_actor_remove(body, length);
        if (as_handle_remove(removed)) return;
        remote_player* remote = find_remote(removed, 0);
        if (remote) {
            remote->active = 0;
            puppet_remove(remote->uid);
            ship_puppet_remove(remote->uid);
            swing_remove(remote->uid);
            wwhd_log_int("multiplayer: remote player left, uid", (int)remote->uid);
        }
    } else if (id == session.server_player_directory) {
        directory_received(body, length);
    } else if (id == session.server_player_summon) {
        shop_received(body, length);
    } else if (id == session.server_player_revive) {
        dh_revived(body, length);
    } else if (id == session.server_save_from) {
        sv_from_received(body, length);
    } else if (id == session.server_save_update) {
        sv_update_received(body, length);
    } else if (id == session.server_world) {
        wd_received(body, length);
    } else if (id == session.server_mutators) {
        mp_u32 multiplier, elite;
        int randomizer, scaling;
        if (!mp_read_mutators_ex(body, length, &multiplier, &randomizer, &scaling, &elite)) {
            drop_frame("multiplayer: malformed mutators skipped", body, length);
            return;
        }
        session.enemy_multiplier = multiplier;
        session.enemy_randomizer = randomizer;
        session.dynamic_scaling = scaling; /* elite.h: the actors follow at their next execute */
        session.elite_chance = elite;      /* for the enemies made from now on */
        as_mutators_changed(); /* the loaded area follows at the next step */
        wwhd_log_int("multiplayer: server mutators changed, enemy multiplier", (int)multiplier);
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
    if (outfit_is_casual((u32)link)) look.extras |= MP_EXTRA_CASUAL;
    /* the Mirror Shield sends light back (daPyRFlg0_LIGHT_REFLECT, puppet.h puppet_light) */
    if (*(const volatile u32*)((const u8*)link + PUPPET_RESET_FLG0) & PUPPET_LIGHT_REFLECT) look.extras |= MP_EXTRA_LIGHT;
    /* the skin the local Link wears (bound at its creation, skin.h): a new choice shows at the
     * next area, for everybody at once */
    int skin = skin_of((u32)link);
    if (skin >= 0) {
        const char* id = skin_archives[skin].id;
        mp_u32 n = 0;
        while (n != MP_SKIN_MAX && id[n]) ++n;
        mp_set_skin(&look, id, n);
    }
    dh_fill_look(&look); /* a ghost's body and directory id (death.h) */
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
                                         r->look.item, r->look.extras, r->look.skin);
        if (!view || screen_w <= 0.0f || screen_h <= 0.0f) continue;

        /* the name follows the puppet itself (riding its ship, carried along), else the place */
        f32 at[3] = {r->display.pos[0], r->display.pos[1], r->display.pos[2]};
        puppet_slot* body = puppet_slot_for(r->uid, 0);
        if (body && body->actor) {
            const fopAc_ac_c* pa = (const fopAc_ac_c*)body->actor;
            at[0] = pa->current.pos.x;
            at[1] = pa->current.pos.y;
            at[2] = pa->current.pos.z;
        }
        f32 dx = at[0] - local->current.pos.x;
        f32 dy = at[1] - local->current.pos.y;
        f32 dz = at[2] - local->current.pos.z;
        f32 distance2 = dx * dx + dy * dy + dz * dz;
        if (!(distance2 < NAME_HIDE_DISTANCE * NAME_HIDE_DISTANCE)) continue;
        f32 opacity = 1.0f;
        if (distance2 > NAME_FADE_START * NAME_FADE_START) {
            f32 distance = marker_sqrt(distance2);
            opacity = (NAME_HIDE_DISTANCE - distance) / (NAME_HIDE_DISTANCE - NAME_FADE_START);
        }

        project_in.x = at[0];
        project_in.y = at[1] + MARKER_HEIGHT;
        project_in.z = at[2];
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
        m->dead = r->has_look && (r->look.death & MP_DEATH_GHOST);
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

/* dScnPly_Execute (025B0314, the play scene's execute, decomp d_s_play.cpp) runs every frame, also
 * while the Start menu pauses the actors (its own dMenu_flag / isPause checks are inside it). The
 * local Link's execute, which reads the network, does not run then, and the host transport closes
 * the connection when 256 received frames wait (runtime/src/network_transport.cpp
 * kMaxQueuedFrames): on the ship, with its updates and the sea's actors, a few seconds of menu
 * were enough. The network is read here too, every frame. */
WWHD_HOOK(0x025B0314, multiplayer_scene_pump, (u32 scene)) {
    (void)scene;
    multiplayer_pump();
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
#define CHART_FACE_SLOTS (MP_DIR_MAX + SV_SIDE_NOTICES) /* the chart's players, then the join notices */
static chart_face_image chart_faces[CHART_FACE_SLOTS];
static u8* chart_face_work;

static u32 chart_face_for(int slot, const tunic_look* wanted, unsigned long long epoch) {
    if (!chart_face_w || !chart_face_rgba || slot < 0 || slot >= CHART_FACE_SLOTS) return 0;
    tunic_look look = *wanted;
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

static void chart_arrow_load(unsigned long long epoch);

/* save_sync.h's join notices: the sea chart's Link icon in the player's tunic colour (the icon the
 * chart shows for them), slot k of the left notices; while the game's icon is not decoded yet (it
 * is read from the MAP screen's layout, sea_chart.h), the chart's other player mark, the arrow
 * (assets/map_player.png), in their colour. */
static u32 sv_player_icon(u32 k, u16 color, u32* tint) {
    unsigned long long epoch = wwhd_hud_epoch();
    tunic_look look = tunic_look_make(color, color);
    u32 face = k < SV_SIDE_NOTICES ? chart_face_for(MP_DIR_MAX + (int)k, &look, epoch) : 0;
    if (face) {
        *tint = 0xFFFFFFu;
        return face;
    }
    chart_arrow_load(epoch);
    *tint = mp_rgb565_to_888(color);
    return chart_arrow_texture;
}

static int sea_stage(void) {
    return area[0] == 's' && area[1] == 'e' && area[2] == 'a' && area[3] == 0;
}

/* The other players on the sea chart, as the GameCube client drew their cursor on the map
 * (ActorPlayerSync mPlayerCursor): the game's Link icon in their tunic colours at their place, an
 * arrow beside it towards their heading, and their name under it. With the server's player
 * directory every player of the server: on the Great Sea at their place, elsewhere at their last
 * place at sea, faded; without it the players of one's own area on the Great Sea. The chart is
 * the MAP screen's (sea_chart.h chart_on_screen) or another screen's sea chart grid (the Ballad of
 * Gales' warp map, chart_grid_on_screen). k: screen units per layout unit (the GamePad: 854 / 1280). */
typedef struct chart_point {
    f32 x, z;
    s16 heading;
    tunic_look look;
    u32 rgb;      /* arrow and name colour */
    f32 fade;     /* 1 at sea, less elsewhere */
    char name[MP_NAME_MAX + 1];
    mp_u32 id;
} chart_point;
static chart_point chart_points[MP_DIR_MAX];

/* A remote player of one's own area, as a map point: in the menu the last received place (the
 * menu pauses the game, so the smoothed place, moved in the local Link's execute, stands still);
 * during play the smoothed one. */
static void chart_point_remote(chart_point* c, const remote_player* r, int paused) {
    const mp_transform* place = paused || !r->has_display ? &r->transform : &r->display;
    c->x = place->pos[0];
    c->z = place->pos[2];
    c->heading = place->rot[1];
    c->look = tunic_look_make(TUNIC_DEFAULT_PRIMARY, TUNIC_DEFAULT_SECONDARY);
    c->rgb = CHART_DEFAULT_CURSOR;
    if (r->has_look) {
        c->look = tunic_look_make((u16)r->look.colors[0], (u16)r->look.colors[1]);
        c->rgb = ((u32)r->look.cursor[0] << 16) | ((u32)r->look.cursor[1] << 8) | r->look.cursor[2];
    }
    c->fade = 1.0f;
    for (int k = 0; k != MP_NAME_MAX + 1; ++k) c->name[k] = r->has_look ? r->look.name[k] : 0;
    c->id = r->uid;
}

/* A directory entry of one's own area that is one of the remote players drawn live (same name). */
static int chart_entry_is_live(const mp_dir_entry* e) {
    for (int i = 0; i != MAX_REMOTES; ++i) {
        const remote_player* r = &remotes[i];
        if (!r->active || !r->has_transform || !r->has_look) continue;
        int same = 1;
        for (int k = 0; k != MP_NAME_MAX && same; ++k) {
            if (r->look.name[k] != e->name[k]) same = 0;
            if (!e->name[k]) break;
        }
        if (same) return 1;
    }
    return 0;
}

/* The players of one's own area at their live place (the directory only comes once a second and
 * not while the game is paused), the others from the directory. */
static int chart_collect(int paused) {
    int n = 0;
    if (sea_stage()) {
        for (int i = 0; i != MAX_REMOTES && n != MP_DIR_MAX; ++i) {
            const remote_player* r = &remotes[i];
            if (r->active && r->has_transform) chart_point_remote(&chart_points[n++], r, paused);
        }
    }
    if (!directory_valid) return n;
    for (mp_u32 i = 0; i != directory.count && n != MP_DIR_MAX; ++i) {
        const mp_dir_entry* e = &directory.entry[i];
        if (e->id == directory.self) continue;
        int here = 1;
        for (int k = 0; k != STAGE_NAME_BYTES && here; ++k) {
            if (e->stage[k] != area[k]) here = 0;
            if (!area[k]) break;
        }
        if (here && sea_stage() && chart_entry_is_live(e)) continue;
        int on_sea = e->stage[0] == 's' && e->stage[1] == 'e' && e->stage[2] == 'a' && !e->stage[3];
        if (!on_sea && !(e->flags & MP_DIR_SEA)) continue;
        chart_point* c = &chart_points[n++];
        c->x = on_sea ? e->pos[0] : e->sea[0];
        c->z = on_sea ? e->pos[2] : e->sea[1];
        c->heading = e->heading;
        c->look = tunic_look_make(e->color, e->color);
        c->rgb = mp_rgb565_to_888(e->color);
        c->fade = on_sea ? 1.0f : 0.6f;
        for (int k = 0; k != MP_NAME_MAX + 1; ++k) c->name[k] = e->name[k];
        c->id = e->id;
    }
    return n;
}

static void chart_arrow_load(unsigned long long epoch) {
    if (epoch == chart_arrow_epoch) return;
    chart_arrow_epoch = epoch;
    chart_arrow_texture = wwhd_hud_texture(WWHD_HUD_PACKAGE, "assets/map_player.png");
    if (!chart_arrow_texture) wwhd_log("multiplayer: sea chart arrow not loaded, dots used");
}

/* One player on a map at screen place (x, y): icon, heading arrow, name. size: the icon's size;
 * icon_scale: the name's size follows it; base_alpha: the map's fade. */
static void chart_draw_point(u32 list, f32 k, f32 x, f32 y, f32 size, f32 icon_scale, f32 base_alpha,
                             const chart_point* c, int slot, unsigned long long epoch) {
    if (!chart_finite(x) || !chart_finite(y)) return;
    u32 rgb = c->rgb;
    u32 alpha = (u32)(base_alpha * c->fade * 255.0f + 0.5f);
    if (alpha > 255) alpha = 255;
    u32 face = chart_face_for(slot, &c->look, epoch);
    if (chart_arrow_texture) {
        /* the arrow points up at rotation 0; heading 0 faces +z (south, down on the chart) and
         * the HUD turns clockwise on the y-down screen: rotation = pi - heading. With the icon
         * it sits beside it, towards the heading (screen direction (sin h, cos h)). */
        f32 heading = (f32)c->heading * (6.28318531f / 65536.0f);
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
    if (names_hidden) return;
    char label[MP_NAME_MAX + 1];
    u32 bytes = 0;
    while (bytes != MP_NAME_MAX && c->name[bytes]) {
        label[bytes] = c->name[bytes];
        ++bytes;
    }
    if (!bytes) bytes = format_label(label, c->id);
    f32 scale = CHART_NAME_SCALE * k * (icon_scale / 32.0f);
    if (scale < 0.18f * k) scale = 0.18f * k;
    if (scale > 0.4f * k) scale = 0.4f * k;
    name_draw(list, x, y + size * 0.5f + (f32)NAME_FONT_CAP * scale + 2.0f * k, label, bytes, rgb, scale,
              base_alpha * c->fade);
}

static void draw_chart_players(u32 list, f32 k, int in_menu) {
    chart_view v;
    if (state != READY) return;
    if (!chart_on_screen(&v, in_menu) && !chart_grid_on_screen(&v, in_menu)) return;
    int count = chart_collect(chart_menu_open());
    if (!count) return;
    unsigned long long epoch = wwhd_hud_epoch();
    chart_arrow_load(epoch);
    f32 size = v.icon * CHART_ARROW_SCALE * k;
    for (int i = 0; i != count; ++i) {
        const chart_point* c = &chart_points[i];
        if (!(c->x > -CHART_SEA_HALF && c->x < CHART_SEA_HALF && c->z > -CHART_SEA_HALF && c->z < CHART_SEA_HALF)) continue;
        chart_draw_point(list, k, (v.ox + c->x * v.kx) * k, (v.oy + c->z * v.ky) * k, size, v.icon, v.alpha, c, i, epoch);
    }
}

/* The other players of the same dungeon on its map (dungeon_map.h): placed as the game places Link's
 * icon, on the floor shown only. Computed while the map updates (game calls), drawn from here. */
static chart_point dmap_points[MAX_REMOTES];
static f32 dmap_xy[MAX_REMOTES][2];
static int dmap_count;

static void dmap_players_update(void) {
    int n = 0, paused = chart_menu_open();
    if (state == READY) {
        for (int i = 0; i != MAX_REMOTES; ++i) {
            const remote_player* r = &remotes[i];
            if (!r->active || !r->has_transform) continue;
            chart_point* c = &dmap_points[n];
            chart_point_remote(c, r, paused);
            const mp_transform* place = paused || !r->has_display ? &r->transform : &r->display;
            if (!dmap_on_floor(place->pos[1])) continue;
            if (!dmap_place(c->x, c->z, &dmap_xy[n][0], &dmap_xy[n][1])) continue;
            ++n;
        }
    }
    dmap_count = n;
}

static void draw_dungeon_players(u32 list, f32 k) {
    if (state != READY || !dmap_shown()) return;
    unsigned long long epoch = wwhd_hud_epoch();
    chart_arrow_load(epoch);
    f32 size = dmap_v.icon * CHART_ARROW_SCALE * k;
    for (int i = 0; i != dmap_count; ++i)
        chart_draw_point(list, k, dmap_xy[i][0] * k, dmap_xy[i][1] * k, size, dmap_v.icon, 1.0f, &dmap_points[i], i, epoch);
}

/* The GamePad's own list (wwhd_hud_register_drc): the sea chart, when the menus are on the GamePad. */
static void draw_gamepad(u32 list) {
    if (chart_menus_to_tv()) return;
    draw_chart_players(list, CHART_DRC_SCALE, 0); /* also during play */
    draw_dungeon_players(list, CHART_DRC_SCALE);
}

/* Names above the remote players' heads (name_tag.h), baseline at the projected head point. With
 * the Pro Controller the MAP screen is on the TV: its sea chart instead (the game is in its menu). */
static wwhd_hud_element marker_element;
static void hv_draw(u32 list); /* heavy.h, included below */
static void pb_draw(u32 list); /* push_bot.h, included below */

static void draw_markers(u32 list) {
    chart_view v;
    sv_draw(list); /* what the other players got, bottom left */
    bonus_draw(list); /* the yellow hearts and magic, next to the game's (bonus.h) */
    if (!chart_menu_open()) el_draw_crowns(list); /* the elites' crowns, not over the Start menu */
    if (chart_menus_to_tv() && chart_menu_open() && dmap_shown()) draw_dungeon_players(list, 1.0f);
    if (chart_menus_to_tv() && (chart_on_screen(&v, 1) || chart_grid_on_screen(&v, 1))) {
        draw_chart_players(list, 1.0f, 1);
        return;
    }
    /* the names float over the game picture: not over the Start menu (sea_chart.h chart_menu_open:
     * items, collection, map and bag, on the TV or the GamePad) */
    dh_draw(list); /* the dead players' bodies, the revive prompt (death.h) */
    hv_draw(list); /* the players lifting or pushing a heavy thing together (heavy.h) */
    pb_draw(list); /* the Debug page's bot (push_bot.h) */
    if (shop_notice_steps > 0 && !chart_menu_open()) /* a creature another player sent */
        name_draw(list, CHART_LAYOUT_W * 0.5f, 120.0f, shop_notice, shop_notice_bytes, 0xF9C74Fu, 0.6f,
                  shop_notice_steps < 30 ? (f32)shop_notice_steps / 30.0f : 1.0f);
    /* not during a cutscene or a dialogue (both are events: play's event running flag) */
    const u8* event_play = wwhd_play_get();
    if (names_hidden || chart_menu_open() || !event_play || event_play[WWHD_PLAY_EVENT_RUNNING_OFFSET]) return;
    for (int i = 0; i != MAX_REMOTES; ++i) {
        const marker* m = &markers[i];
        if (!m->visible) continue;
        u32 skull = m->dead ? sv_dead_icon() : 0;
        if (skull) { /* a ghost: the skull (50 x 44), bottom at the name's baseline */
            f32 h = 44.0f * m->scale * 1.3f, w = 50.0f * m->scale * 1.3f;
            marker_element = (wwhd_hud_element){.kind = WWHD_HUD_IMAGE, .anchor = WWHD_HUD_TOP_LEFT, .blend = WWHD_HUD_ALPHA,
                                                .x = m->x - w * 0.5f, .y = m->y - h, .w = w, .h = h, .thickness = 1,
                                                .u1 = 1, .v1 = 1, .rgba = 0xFFFFFF00u | (u32)(m->opacity * 255.0f),
                                                .image = skull};
            wwhd_hud_emit(list, &marker_element);
            continue;
        }
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
    if (!skin_same_id(a->skin, b->skin)) return 0;
    if (a->death != b->death || a->death_dir != b->death_dir) return 0;
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

/* The local player's skin: launcher.cfg, else the option (skin.h). */
static void skin_choose_local(void) {
    if (cfg.has_skin) {
        skin_copy_id(skin_local, cfg.skin);
    } else {
        char skin[MP_SKIN_MAX + 1] = {0};
        wwhd_config_string("skin", skin, sizeof(skin));
        mp_u32 n = 0;
        while (n != MP_SKIN_MAX && skin[n]) ++n;
        skin_copy_id(skin_local, mp_skin_valid(skin, n) ? skin : "");
    }
}

/* At a scene load (skin.h skin_scene_load): launcher.cfg read again for the skin. */
static void skin_read_local(void) {
    read_launcher_cfg();
    skin_choose_local();
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
    int hud = cfg.has_hide_hud ? cfg.hide_hud : wwhd_config_bool("hide_hud", 0);
    if (hud != hud_hidden) wwhd_log(hud ? "multiplayer: HUD hidden" : "multiplayer: HUD shown");
    hud_hidden = hud;
    sense_enabled = wwhd_config_bool("actors_see_puppets", 1);
    as_enabled = wwhd_config_bool("sync_actors", 1);
    as_hit_stop = wwhd_config_bool("hit_stop", 0);
    if (cfg.has_outfit) {
        outfit_wanted = cfg.outfit;
    } else {
        char outfit[16] = {0};
        wwhd_config_string("outfit", outfit, sizeof(outfit));
        outfit_wanted = outfit_parse(outfit, OUTFIT_AUTO);
    }
    refresh_ship_color_setting();
    mp_player_look look = wanted_look();
    if (!same_look(&look, &local_look)) {
        local_look = look;
        wwhd_log("multiplayer: launcher colours changed");
    }
    skin_choose_local();
    /* A skinned local Link keeps the skin's own textures; the Link texture the puppets without skin
     * share is then taken from one of them and kept in its original colours, each puppet getting
     * its own colours (tunic.h remote copies). Before 0.2.62 it was left as it was: holding the
     * local colours written before the skin was put on, every such puppet wore them. */
    if (skin_of((u32)link) < 0) {
        tunic_set_local((u16)local_look.colors[0], (u16)local_look.colors[1]);
        tunic_refresh((u32)link);
    } else {
        tunic_set_local(TUNIC_DEFAULT_PRIMARY, TUNIC_DEFAULT_SECONDARY);
        u32 source = puppet_unskinned_actor();
        if (source) tunic_refresh(source);
    }
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
    /* on by default since 0.2.62: the same draw-time texture swap as the elites' colours works in
     * game, and the per-player colours were seen working (0.2.29) */
    tunic_remote_enabled = wwhd_config_bool("remote_tunics", 1);
    int port = cfg.has_port ? (int)cfg.port : wwhd_config_int("port", 10014);
    if (port < 1 || port > 65535) { wwhd_log("multiplayer: invalid port"); return; }
    if (!wwhd_net_connect(host, (u32)port)) { wwhd_log("multiplayer: connect rejected"); return; }
    state = WAIT_INIT;
}

/* ------------------------------------------------------------------ launcher map and teleport */

/* The launcher's World Map (LauncherUI createMapPage) shows the players on the Great Sea and lets
 * the player teleport to one of them for TELEPORT_COST rupees. Files in this mod's Data folder,
 * next to launcher.cfg (wwhd_file_*, flat names):
 *  - players.txt, written every MAP_PERIOD: "stage=", "rupees=", "step=", "directory=" (1: the
 *    server sends the player directory), "me=" then one "player=" per other player, each
 *    "id|x|z|heading|RRGGBB|name|stage|sea x|sea z|sea known|dead" (x, z in that stage; the last place
 *    on the Great Sea for the chart; heading the s16 shape angle; the primary tunic colour; '|' and
 *    control characters of names as '?'). With the directory every player of the server is listed
 *    and the ids are the directory's; without it, the remote players of the area by actor uid. Then
 *    "teleport=id|result" for the last request (ok, loading, arrived, no_target, rupees, riding,
 *    busy, offline, dead);
 *  - teleport.cfg, written by the launcher: "id=<number>" and "target=<uid>"; a new id is one
 *    request. The id found at the first read is not acted on (a request older than the game).
 * The teleport is the game's own respawn (decomp, verified): Link restarts at the save's restart
 * place (dSv_restart_c at *(101F84DC) + 0x1148, d_a_npc_os_b.h; setRoom 025B9810: position,
 * angle, room, d_save_4.cpp) when a stage is entered with start point -1 (d_stage_2.cpp player
 * init: the restart position and angle, parameters = the restart room param at + 0x116C). The game
 * does it so after a fall into the void (dStage_restartRoom 025C3D68, d_stage_4.cpp:
 * dComIfGp_setNextStage(current stage, -1, restart room, layer -1, 0, mode, 0, 0), then the param;
 * daPy_lk_c::startRestartRoom d_a_player_main_03.cpp: mode 5, event info 0xC9, param =
 * setParamData(room, 0, 0xC9, 0) = room | 0xC9 << 24, d_a_player_main.cpp). The teleport sets the
 * restart place to the target's last place, angle and room, and enters the target's stage that way
 * (its layer when it is another stage): the room is loaded and Link stands on its ground. 0.2.37-
 * 0.2.57 moved Link by hand above the target, in a room often not loaded: he fell and the game put
 * him back at his restart place. The restart place stays the target's (a later fall returns there).
 *
 * Rupees (decomp, verified): the save's dSv_player_status_a_c::mRupee is at *(101F84DC) + 0x20 +
 * 4 (d/d_save.h); a payment is added to dComIfG_play_c::mItemRupeeCount, play + 0x12A0 + 0x48A8
 * from dComIfGp_get(), which the rupee meter applies with its count-down and clears
 * (d_meter_02.cpp), as shops do (dComIfGp_setItemRupeeCount).
 * Not while Link rides his ship (play + 0x5CD8 SHIP_RIDE, puppet.h) or during an event. */
#define MAP_PERIOD 0.5
#define TELEPORT_COST 35
#define PLAY_ITEM_RUPEE_COUNT (0x12A0 + 0x48A8)
#define SAVE_POINTER 0x101F84DC
#define SAVE_RUPEE (0x20 + 0x04)
#define SAVE_RESTART 0x1148        /* dSv_restart_c */
#define SAVE_RESTART_PARAM 0x116C  /* dSv_restart_c::mRestartParam */
#define TELEPORT_MODE 5            /* the void respawn's mode and event info (startRestartRoom) */
#define TELEPORT_EVENT_INFO 0xC9u

/* The Great Sea's squares the player has unlocked (the fishman's chart, decomp d_save_3.cpp
 * dSv_player_map_c::isSaveArriveGrid: bit 0 of mFmapBits[square], save + 0x20 + 0xC4 + 0x40,
 * d_save.h; the fishman tests it for its room - 1, d_a_npc_so_mode.cpp; three squares at the start,
 * d_save_3.cpp init). Square = row * 7 + column, from the north-west (sea rooms 1..49). players.txt
 * "chart=" lists them (49 of 0/1) for the launcher's map; a teleport to a player on a locked square
 * is refused ("locked"). */
#define SAVE_FMAP_BITS (0x20 + 0xC4 + 0x40)
#define SEA_SQUARES 49
#define SEA_SQUARE_SIZE 100000.0f
#define SEA_HALF_SIZE 350000.0f

static int sea_square_unlocked(u32 square) {
    u32 save = WWHD_GAME_DATA(SAVE_POINTER, u32);
    if (!save || square >= SEA_SQUARES) return 1;
    return (*(const volatile u8*)(save + SAVE_FMAP_BITS + square) & 1u) != 0;
}

/* The square of a place on the Great Sea, -1 outside. */
static int sea_square_of(f32 x, f32 z) {
    if (!(x > -SEA_HALF_SIZE && x < SEA_HALF_SIZE && z > -SEA_HALF_SIZE && z < SEA_HALF_SIZE)) return -1;
    int column = (int)((x + SEA_HALF_SIZE) / SEA_SQUARE_SIZE), row = (int)((z + SEA_HALF_SIZE) / SEA_SQUARE_SIZE);
    if (column > 6) column = 6;
    if (row > 6) row = 6;
    return row * 7 + column;
}

/* dSv_restart_c::setRoom (025B9810: this, const cXyz* position, angle, room) */
WWHD_GAME_FUNC(0x025B9810, void, teleport_set_restart, (u32 restart, u32 position, u32 angle, u32 room));

static double map_clock;
static char map_text[4096];
static char teleport_text[256];
static int teleport_read_once;
static u32 teleport_last_id;
static u32 teleport_result_id;
static const char* teleport_result = "";
static cXyz teleport_pos; /* passed to the game by address: in mod memory */

typedef struct map_out {
    char* data;
    u32 size, capacity;
} map_out;

static void map_put(map_out* o, const char* s) {
    while (*s && o->size + 1 < o->capacity) o->data[o->size++] = *s++;
}

static void map_put_name(map_out* o, const char* s, u32 n) {
    for (u32 i = 0; i != n && s[i] && o->size + 1 < o->capacity; ++i)
        o->data[o->size++] = (s[i] < 0x20 || s[i] > 0x7E || s[i] == '|') ? '?' : s[i];
}

/* 32-bit only: 64-bit arithmetic calls compiler helpers (__fixsfdi, __udivdi3) that are no host
 * services, and a module importing one does not load ("module ABI does not match"). */
static void map_put_digits(map_out* o, u32 u, int negative) {
    char digits[12];
    int n = 0;
    do { digits[n++] = (char)('0' + u % 10); u /= 10; } while (u);
    if (negative) digits[n++] = '-';
    while (n && o->size + 1 < o->capacity) o->data[o->size++] = digits[--n];
}

static void map_put_int(map_out* o, s32 v) { map_put_digits(o, v < 0 ? 0u - (u32)v : (u32)v, v < 0); }
static void map_put_uint(map_out* o, u32 v) { map_put_digits(o, v, 0); }

static void map_put_rgb(map_out* o, mp_u32 rgb565) {
    static const char hex[] = "0123456789ABCDEF";
    mp_u32 rgb = mp_rgb565_to_888((mp_u16)rgb565);
    for (int shift = 20; shift >= 0; shift -= 4)
        if (o->size + 1 < o->capacity) o->data[o->size++] = hex[(rgb >> shift) & 0xF];
}

static u32 map_rupees(void) {
    u32 save = WWHD_GAME_DATA(SAVE_POINTER, u32);
    return save ? *(const volatile u16*)(save + SAVE_RUPEE) : 0;
}

static int map_on_sea(const u8* play) {
    const u8* stage = play + WWHD_PLAY_START_STAGE_NAME_OFFSET;
    return stage[0] == 's' && stage[1] == 'e' && stage[2] == 'a' && stage[3] == 0;
}

/* "key=number" in teleport.cfg; 0 when missing or malformed. */
static u32 map_cfg_number(const char* text, u32 size, const char* key) {
    u32 klen = 0;
    while (key[klen]) ++klen;
    for (u32 pos = 0; pos < size;) {
        u32 end = pos;
        while (end < size && text[end] != '\n') ++end;
        if (end - pos > klen && text[pos + klen] == '=') {
            u32 k = 0;
            while (k != klen && text[pos + k] == key[k]) ++k;
            if (k == klen) {
                u32 value = 0;
                for (u32 i = pos + klen + 1; i < end && text[i] >= '0' && text[i] <= '9'; ++i)
                    value = value * 10 + (u32)(text[i] - '0');
                return value;
            }
        }
        pos = end + 1;
    }
    return 0;
}

/* ---- the player directory (mp_protocol.h): every player, whatever their area ----
 * Sent every DIR_PERIOD while connected (a server without it ignores nothing: the packet name is
 * not in its init list, nothing is sent); the answer replaces the list. Without it (an older
 * server) the map shows the remote players of the area, as before. */
#define DIR_PERIOD 1.0
#define TELEPORT_STAGE_TIMEOUT (30 * 60) /* steps to reach the target's stage */
#define TELEPORT_SETTLE 20               /* steps in the stage, out of events, before "ok" */
#define TELEPORT_RELOAD_STEPS 45         /* the reload's fade and load take at least that */
#define PLAY_START_POINT (WWHD_PLAY_START_STAGE_NAME_OFFSET + 0x8)  /* dStage_startStage_c::mPoint */
#define PLAY_START_ROOM (WWHD_PLAY_START_STAGE_NAME_OFFSET + 0xA)   /* mRoomNo */
#define PLAY_START_LAYER (WWHD_PLAY_START_STAGE_NAME_OFFSET + 0xB)  /* mLayer */

static double dir_clock;
static f32 local_sea[2];
static int local_sea_known;
/* a teleport to another stage: the stage asked for, then the hop to the target */
static mp_u32 teleport_target;
static char teleport_stage[STAGE_NAME_BYTES + 1];
static unsigned long long teleport_stage_since;
static int teleport_stage_wait;
static int teleport_settle;

static void map_stage(const u8* play, char* out) {
    for (int i = 0; i != STAGE_NAME_BYTES; ++i) out[i] = (char)play[WWHD_PLAY_START_STAGE_NAME_OFFSET + i];
    out[STAGE_NAME_BYTES] = 0;
}

static int map_same_stage(const char* a, const char* b);

/* elite.h: the players in the local player's area, itself included: the directory's players in
 * the same stage and, on the Great Sea (one stage, a room per square), in the same room or within
 * 5000 units; without the directory the remote players of the area. At most 8. */
static int el_players_here(void) {
    const u8* play = wwhd_play_get();
    const fopAc_ac_c* link = (const fopAc_ac_c*)as_link;
    if (!play || !link) return 1;
    int n = 1;
    if (directory_valid) {
        char stage[STAGE_NAME_BYTES + 1];
        map_stage(play, stage);
        int sea = stage[0] == 's' && stage[1] == 'e' && stage[2] == 'a' && !stage[3];
        for (mp_u32 i = 0; i != directory.count; ++i) {
            const mp_dir_entry* e = &directory.entry[i];
            if (e->id == directory.self || !map_same_stage(e->stage, stage)) continue;
            if (sea && e->room != link->current.roomNo) {
                f32 dx = e->pos[0] - link->current.pos.x, dz = e->pos[2] - link->current.pos.z;
                if (dx * dx + dz * dz > 5000.0f * 5000.0f) continue;
            }
            ++n;
        }
    } else {
        for (int i = 0; i != MAX_REMOTES; ++i) n += remotes[i].active != 0;
    }
    return n > 8 ? 8 : n;
}

static int map_same_stage(const char* a, const char* b) {
    for (int i = 0; i != STAGE_NAME_BYTES; ++i) {
        if (a[i] != b[i]) return 0;
        if (!a[i]) return 1;
    }
    return 1;
}

static void directory_send(const fopAc_ac_c* link, const u8* play) {
    if (state != READY || session.client_player_directory == MP_NO_ID) return;
    mp_dir_entry e;
    mp_copy_text(e.name, sizeof(e.name), local_look.name, MP_NAME_MAX);
    e.color = (mp_u16)local_look.colors[0];
    map_stage(play, e.stage);
    e.point = *(const volatile s16*)(play + PLAY_START_POINT);
    e.start_room = *(const volatile s8*)(play + PLAY_START_ROOM);
    e.layer = *(const volatile s8*)(play + PLAY_START_LAYER);
    e.room = link->current.roomNo;
    e.pos[0] = link->current.pos.x;
    e.pos[1] = link->current.pos.y;
    e.pos[2] = link->current.pos.z;
    e.heading = link->shape_angle.y;
    e.flags = local_sea_known ? MP_DIR_SEA : 0;
    if (dh_local_dead()) e.flags |= MP_DIR_DEAD;
    e.sea[0] = local_sea[0];
    e.sea[1] = local_sea[1];
    mp_writer body = mp_writer_make(payload, sizeof(payload));
    mp_write_dir_entry(&body, &e);
    send_payload(session.client_player_directory, &body);
}

static void directory_received(const mp_u8* body, mp_u32 length) {
    static mp_directory incoming_directory;
    if (!mp_read_directory(body, length, &incoming_directory)) {
        drop_frame("multiplayer: malformed player directory skipped", body, length);
        return;
    }
    /* a player who joined: an id the last directory did not have (not at the first one) */
    if (directory_valid) {
        for (mp_u32 i = 0; i != incoming_directory.count; ++i) {
            const mp_dir_entry* e = &incoming_directory.entry[i];
            if (e->id == incoming_directory.self) continue;
            int known = 0;
            for (mp_u32 j = 0; j != directory.count && !known; ++j) known = directory.entry[j].id == e->id;
            if (known) {
                /* a player who died: alive in the last directory, dead in this one */
                for (mp_u32 j = 0; j != directory.count; ++j)
                    if (directory.entry[j].id == e->id && !(directory.entry[j].flags & MP_DIR_DEAD) &&
                        (e->flags & MP_DIR_DEAD)) {
                        sv_notice_died(e->name);
                        wwhd_log("multiplayer: a player died:");
                        wwhd_log(e->name[0] ? e->name : "(no name)");
                    }
                continue;
            }
            sv_notice_joined(e->name, e->color);
            wwhd_log("multiplayer: a player joined the game:");
            wwhd_log(e->name[0] ? e->name : "(no name)");
        }
    }
    /* a player who left: an id of the last directory missing from this one */
    if (directory_valid) {
        for (mp_u32 j = 0; j != directory.count; ++j) {
            const mp_dir_entry* old = &directory.entry[j];
            if (old->id == directory.self) continue;
            int still = 0;
            for (mp_u32 i = 0; i != incoming_directory.count && !still; ++i) still = incoming_directory.entry[i].id == old->id;
            if (still) continue;
            sv_notice_left(old->name, old->color);
            wwhd_log("multiplayer: a player left the game:");
            wwhd_log(old->name[0] ? old->name : "(no name)");
        }
    }
    directory = incoming_directory;
    if (!directory_valid) wwhd_log_int("multiplayer: player directory received, players", (int)directory.count);
    directory_valid = 1;
}

static const mp_dir_entry* directory_find(mp_u32 id) {
    if (!directory_valid) return 0;
    for (mp_u32 i = 0; i != directory.count; ++i)
        if (directory.entry[i].id == id) return &directory.entry[i];
    return 0;
}

/* Enters `stage` (here or another) as the game's respawn does, Link at pos, facing angle, in room. */
static int teleport_go(const char* stage, const f32* pos, s16 angle, s8 room, s8 layer, int same_stage) {
    u32 save = WWHD_GAME_DATA(SAVE_POINTER, u32);
    if (!save || room < 0) return 0;
    for (int i = 0; i != STAGE_NAME_BYTES + 1; ++i) teleport_stage[i] = stage[i];
    teleport_stage[STAGE_NAME_BYTES] = 0;
    teleport_pos.x = pos[0];
    teleport_pos.y = pos[1];
    teleport_pos.z = pos[2];
    teleport_set_restart(save + SAVE_RESTART, (u32)&teleport_pos, (u32)(s32)angle, (u32)(s32)room);
    if (!wwhd_dComIfGp_setNextStage((u32)teleport_stage, 0xFFFFFFFFu, (u32)(s32)room,
                                    same_stage ? 0xFFFFFFFFu : (u32)(s32)layer, 0.0f, TELEPORT_MODE, 0, 0))
        return 0;
    *(volatile u32*)(save + SAVE_RESTART_PARAM) = ((u32)(s32)room & 0x3Fu) | (TELEPORT_EVENT_INFO << 24);
    teleport_stage_wait = 1;
    teleport_settle = 0;
    teleport_stage_since = wwhd_logic_step();
    return 1;
}

static int teleport_pay(u8* play) {
    s32 pending = *(const volatile s32*)(play + PLAY_ITEM_RUPEE_COUNT);
    if ((s32)map_rupees() + pending < TELEPORT_COST) return 0;
    *(volatile s32*)(play + PLAY_ITEM_RUPEE_COUNT) = pending - TELEPORT_COST;
    return 1;
}

/* target: a directory id (the server has the directory), else a remote player of the area by uid
 * (its room unknown: Link's own, in the same stage). The stage is entered as the game's respawn
 * does (teleport_go), then teleport_stage_step reports the arrival. */
static const char* teleport_try(fopAc_ac_c* link, u8* play, mp_u32 target) {
    if (state != READY) return "offline";
    if (play[WWHD_PLAY_EVENT_RUNNING_OFFSET] || teleport_stage_wait) return "busy";
    if (*(const volatile u32*)(play + PUPPET_PLAY_STATUS0) & PUPPET_STATUS0_RIDE) return "riding";
    char here[STAGE_NAME_BYTES + 1];
    map_stage(play, here);
    const mp_dir_entry* e = directory_find(target);
    if (e) {
        if (e->id == directory.self || !e->stage[0] || e->room < 0) return "no_target";
        if (e->flags & MP_DIR_DEAD) return "dead";
        /* on the Great Sea, or in an island's stage with its last place at sea: that square */
        int at_sea = e->stage[0] == 's' && e->stage[1] == 'e' && e->stage[2] == 'a' && !e->stage[3];
        int square = at_sea ? sea_square_of(e->pos[0], e->pos[2])
                            : (e->flags & MP_DIR_SEA) ? sea_square_of(e->sea[0], e->sea[1]) : -1;
        if (square >= 0 && !sea_square_unlocked((u32)square)) return "locked";
        if (!teleport_pay(play)) return "rupees";
        if (!teleport_go(e->stage, e->pos, e->heading, e->room, e->layer, map_same_stage(e->stage, here))) {
            *(volatile s32*)(play + PLAY_ITEM_RUPEE_COUNT) += TELEPORT_COST; /* refused: given back */
            return "failed";
        }
        teleport_target = target;
        wwhd_log("multiplayer: teleport: to the player's place in");
        wwhd_log(teleport_stage);
        return "loading";
    }
    const remote_player* r = 0;
    for (int i = 0; i != MAX_REMOTES; ++i)
        if (remotes[i].active && remotes[i].has_transform && remotes[i].uid == target) r = &remotes[i];
    if (!r) return "no_target";
    if (r->has_look && (r->look.death & (MP_DEATH_GHOST | MP_DEATH_DOWN))) return "dead";
    if (here[0] == 's' && here[1] == 'e' && here[2] == 'a' && !here[3]) {
        int square = sea_square_of(r->transform.pos[0], r->transform.pos[2]);
        if (square >= 0 && !sea_square_unlocked((u32)square)) return "locked";
    }
    if (!teleport_pay(play)) return "rupees";
    if (!teleport_go(here, r->transform.pos, r->transform.rot[1], link->current.roomNo, 0, 1)) {
        *(volatile s32*)(play + PLAY_ITEM_RUPEE_COUNT) += TELEPORT_COST;
        return "failed";
    }
    teleport_target = 0;
    wwhd_log_int("multiplayer: teleport to the player, uid", (int)target);
    return "loading";
}

/* After teleport_go: Link stands in the stage again (the reload done: at least TELEPORT_SETTLE
 * steps after the request, out of any event) -> "ok". */
static void teleport_stage_step(const u8* play) {
    if (!teleport_stage_wait) return;
    char here[STAGE_NAME_BYTES + 1];
    map_stage(play, here);
    if (wwhd_logic_step() - teleport_stage_since > TELEPORT_STAGE_TIMEOUT) {
        teleport_stage_wait = 0;
        teleport_result = "no_target";
        wwhd_log("multiplayer: teleport: the player's stage was not reached");
        return;
    }
    if (!map_same_stage(here, teleport_stage) || play[WWHD_PLAY_EVENT_RUNNING_OFFSET] ||
        wwhd_logic_step() - teleport_stage_since < TELEPORT_RELOAD_STEPS) {
        teleport_settle = 0;
        return;
    }
    if (++teleport_settle < TELEPORT_SETTLE) return;
    teleport_stage_wait = 0;
    teleport_result = "ok";
    wwhd_log("multiplayer: teleport: arrived");
}

/* ------------------------------------------------------------------ the launcher's shop
 *
 * The launcher's 5th tab (LauncherUI createShopPage) buys a creature for oneself or for another
 * player of the directory: shop.cfg (id, item, target: a directory id, 0 for oneself), answered in
 * players.txt ("shop=id|result"), which also lists the spoils ("spoils=" eight counts).
 * The price is rupees and spoils, paid the way the game's shops pay, through the counters the HUD
 * applies (play + 0x5B48 mItemRupeeCount, play + 0x5B70 + 2i mItemBeastNumCounts[i], s16: decomp
 * d_a_npc_ds1_event.cpp, d_a_npc_bms1_talk.cpp). The spoils counts are dSv_player_c
 * mBagItemRecord.mBeastNum[i] (save + 0x20 + 0x9C + i, d_save.h), fixed indices (d_a_npc_bs1.cpp,
 * d_a_npc_bm1_b.h, d_a_npc_bms1_talk.cpp): 0 Skull Necklace, 1 Boko Baba Seed, 2 Golden Feather,
 * 3 Knight's Crest, 4 Red, 5 Green, 6 Blue Chu Jelly, 7 Joy Pendant.
 * The creatures: stage names of the name table (dStage_searchName, read in the binary: Bk 0xBD,
 * mo2 0xBC, Tn 0xBF, c_red/c_green/c_blue 0xCE, bbaba 0xD6, Bb 0xB5) with parameters and angles
 * of the game's own stages (ACTR entries read in the game's stage files: a common one each, no path
 * nor switch; the ChuChu colour is its parameters' byte 1, d_a_cc.cpp). For oneself the creature
 * is made next to Link (actor_sync.h as_summon); for another player the request goes through the
 * server (CLIENT_PLAYER_SUMMON) and that player's game makes it next to its own Link
 * (SERVER_PLAYER_SUMMON, shop_received). The launcher's catalogue (LauncherUI SHOP_ITEMS) shows
 * the same numbers, prices and icons: keep both in step.
 * Items from SHOP_UPGRADES on are the "Upgrades & Utilities", for oneself only (the target is not
 * read): yellow heart and yellow magic (bonus.h), 75 rupees each; the long hookshot (5 Blue Chu
 * Jelly, 5 Knight's Crests, 1000 rupees) and the golden pig (5 Golden Feathers, 100 rupees) are
 * shown by the launcher but not made yet ("soon", nothing paid). players.txt "bonus=" gives the
 * yellow quarter hearts, the yellow magic, and 1 when each is at its maximum.
 * Utilities made in front of Link, staying there (summoned actors, synchronised like the creatures):
 *  - a bomb flower, 5 bombs and 20 rupees: "BFlower" (process 0x128, the game's name table read in
 *    the EU binary), parameters 0xFF0F, the most common in the game's stages (ACTR, local
 *    analysis): daBFlower_c::CreateHeap (d_a_bflower.cpp) reads its state in bits 4-7 (0: grown)
 *    and its switch in bits 8-15 (0xFF: none). The bombs are paid as the game takes them, through
 *    dComIfG_play_c::mItemBombNumCount (play + 0x12A0 + 0x48CC, s16) next to the save's
 *    mItemRecord2.mBombNum (save + 0x20 + 0x6A, u8, d_save.h); players.txt "bombs=" gives them;
 *  - a beam, 30 rupees: the light falling from above that the Mirror Shield sends back
 *    (daTagLight, d_a_tag_light.cpp, process 0x1D5, name "LTag1"), parameters 0x00FF03FD as in the
 *    game's stages: type 1 the visible cone (PRM_TYPE bits 0-1), no switch (bits 2-9 and 16-23
 *    0xFF), any time (bits 24-31 0); its size is the block's scale (tenths, the stages use
 *    20 x 100 x 20): 20 x 255 x 20 here, the tallest, upright on the ground in front of Link. */
#define SHOP_SPOILS 8
#define PLAY_ITEM_BEAST_COUNTS 0x5B70
#define SAVE_BEAST_NUM (0x20 + 0x9C)
#define SHOP_DISTANCE 250.0f
#define SHOP_QUEUE 4
#define SHOP_NOTICE_STEPS 120

typedef struct shop_creature {
    const char* name;  /* stage name */
    u32 params;
    s16 angle_x, angle_z;
    u8 spoil, spoils;  /* spoil index, count */
    u16 rupees;
    const char* label;
    u8 randomized;     /* given the randomizer's variants (actor_sync.h as_variants) */
} shop_creature;

static const shop_creature shop_creatures[] = {
    {"c_red", 0xFFFF01FFu, 0, 0xFF, 4, 1, 5, "a Red ChuChu", 0},
    {"c_green", 0xFFFF00FFu, 0, 0xFF, 5, 1, 5, "a Green ChuChu", 0},
    {"c_blue", 0xFFFF02FFu, 0, 0xFF, 6, 1, 10, "a Blue ChuChu", 0},
    {"Bk", 0xFFFFFF00u, 0x1F, 0xFF, 7, 1, 10, "a Bokoblin", 1},
    {"mo2", 0xFFFFFF00u, 0x1F, 0xFF, 0, 1, 15, "a Moblin", 1},
    {"Tn", 0xFFFFFF00u, 0x1F, 0xFF, 3, 1, 20, "a Darknut", 1},
    {"bbaba", 0xFFFFFFFFu, 0, 0, 1, 1, 10, "a Boko Baba", 0},
    {"Bb", 0xFFFF0F04u, 0x1F, 0xFF, 2, 1, 5, "a Kargaroc", 0},
};
#define SHOP_CREATURES (sizeof(shop_creatures) / sizeof(shop_creatures[0]))
#define SHOP_UPGRADES 8u
enum { SHOP_YELLOW_HEART = SHOP_UPGRADES, SHOP_YELLOW_MAGIC, SHOP_LONG_HOOKSHOT, SHOP_GOLDEN_PIG, SHOP_BOMB_FLOWER,
       SHOP_BEAM, SHOP_ITEMS_END };

#define SHOP_BOMB_FLOWER_RUPEES 20
#define SHOP_BOMB_FLOWER_BOMBS 5
#define SHOP_BEAM_RUPEES 30
#define SAVE_BOMBS (0x20 + 0x6A)
#define PLAY_ITEM_BOMB_COUNT (0x12A0 + 0x48CC)
static const u8 shop_beam_scale[3] = {20, 255, 20};
#define SHOP_UPGRADE_RUPEES 75

static char shop_text[256];
static int shop_read_once;
static u32 shop_last_id, shop_result_id;
static const char* shop_result = "";
typedef struct shop_gift {
    u8 creature;
    char buyer[MP_NAME_MAX + 1];
} shop_gift;
static shop_gift shop_gifts[SHOP_QUEUE];
static u32 shop_gift_count;

/* Spoils of index i one has, with what the HUD has not taken yet. */
static s32 shop_spoils(const u8* play, u32 i) {
    u32 save = WWHD_GAME_DATA(SAVE_POINTER, u32);
    if (!save || i >= SHOP_SPOILS) return 0;
    s32 have = *(const volatile u8*)(save + SAVE_BEAST_NUM + i);
    return have + *(const volatile s16*)(play + PLAY_ITEM_BEAST_COUNTS + 2 * i);
}

static int shop_affordable(const u8* play, const shop_creature* c) {
    s32 rupees = (s32)map_rupees() + *(const volatile s32*)(play + PLAY_ITEM_RUPEE_COUNT);
    return rupees >= (s32)c->rupees && shop_spoils(play, c->spoil) >= (s32)c->spoils;
}

static void shop_pay(u8* play, const shop_creature* c) {
    volatile s32* rupees = (volatile s32*)(play + PLAY_ITEM_RUPEE_COUNT);
    volatile s16* spoils = (volatile s16*)(play + PLAY_ITEM_BEAST_COUNTS + 2 * c->spoil);
    *rupees = *rupees - (s32)c->rupees;
    *spoils = (s16)(*spoils - (s16)c->spoils);
}

/* The creature next to the local Link, in front of him, facing him. */
static int shop_summon_here(const fopAc_ac_c* link, u32 item) {
    const shop_creature* c = &shop_creatures[item];
    f32 sn, cs;
    dmap_sincos((f32)link->shape_angle.y * (360.0f / 65536.0f), &sn, &cs);
    f32 pos[3] = {link->current.pos.x + sn * SHOP_DISTANCE, link->current.pos.y, link->current.pos.z + cs * SHOP_DISTANCE};
    return as_summon(c->name, c->params, c->angle_x, c->angle_z, pos, (s16)(link->shape_angle.y + 0x8000),
                     (s8)link->current.roomNo, c->randomized);
}

/* Bombs one has, with what the HUD has not taken yet. */
static s32 shop_bombs(const u8* play) {
    u32 save = WWHD_GAME_DATA(SAVE_POINTER, u32);
    if (!save) return 0;
    return (s32)*(const volatile u8*)(save + SAVE_BOMBS) + *(const volatile s16*)(play + PLAY_ITEM_BOMB_COUNT);
}

/* A utility made in front of the local Link, facing him; paid once made. */
static const char* shop_try_utility(const fopAc_ac_c* link, u8* play, u32 item) {
    s32 rupees = (s32)map_rupees() + *(const volatile s32*)(play + PLAY_ITEM_RUPEE_COUNT);
    s32 cost = item == SHOP_BEAM ? SHOP_BEAM_RUPEES : SHOP_BOMB_FLOWER_RUPEES;
    s32 bombs = item == SHOP_BOMB_FLOWER ? SHOP_BOMB_FLOWER_BOMBS : 0;
    if (rupees < cost || shop_bombs(play) < bombs) return "price";
    f32 sn, cs;
    dmap_sincos((f32)link->shape_angle.y * (360.0f / 65536.0f), &sn, &cs);
    f32 pos[3] = {link->current.pos.x + sn * SHOP_DISTANCE, link->current.pos.y, link->current.pos.z + cs * SHOP_DISTANCE};
    s16 yaw = (s16)(link->shape_angle.y + 0x8000);
    int made = item == SHOP_BEAM
                   ? as_summon_scaled("LTag1", 0x00FF03FDu, 0, 0, pos, 0, (s8)link->current.roomNo, 0, shop_beam_scale)
                   : as_summon("BFlower", 0x0000FF0Fu, 0, 0, pos, yaw, (s8)link->current.roomNo, 0);
    if (!made) return "failed";
    volatile s32* pay = (volatile s32*)(play + PLAY_ITEM_RUPEE_COUNT);
    *pay = *pay - cost;
    volatile s16* pay_bombs = (volatile s16*)(play + PLAY_ITEM_BOMB_COUNT);
    *pay_bombs = (s16)(*pay_bombs - bombs);
    return "ok";
}

/* An upgrade for oneself: checked, then paid once given. */
static const char* shop_try_upgrade(const fopAc_ac_c* link, u8* play, u32 item) {
    if (item == SHOP_LONG_HOOKSHOT || item == SHOP_GOLDEN_PIG) return "soon";
    if (item == SHOP_BOMB_FLOWER || item == SHOP_BEAM) return shop_try_utility(link, play, item);
    if (item != SHOP_YELLOW_HEART && item != SHOP_YELLOW_MAGIC) return "unknown";
    s32 rupees = (s32)map_rupees() + *(const volatile s32*)(play + PLAY_ITEM_RUPEE_COUNT);
    if (rupees < SHOP_UPGRADE_RUPEES) return "price";
    const char* result = item == SHOP_YELLOW_HEART ? bonus_buy_heart() : bonus_buy_magic();
    if (result[0] != 'o' || result[1] != 'k' || result[2]) return result;
    volatile s32* pay = (volatile s32*)(play + PLAY_ITEM_RUPEE_COUNT);
    *pay = *pay - SHOP_UPGRADE_RUPEES;
    return "ok";
}

static const char* shop_try(const fopAc_ac_c* link, u8* play, u32 item, u32 target) {
    if (play[WWHD_PLAY_EVENT_RUNNING_OFFSET]) return "busy";
    if (item >= SHOP_UPGRADES) return shop_try_upgrade(link, play, item);
    if (item >= SHOP_CREATURES) return "unknown";
    const shop_creature* c = &shop_creatures[item];
    if (!shop_affordable(play, c)) return "price";
    if (!target || (directory_valid && target == directory.self)) {
        if (!shop_summon_here(link, item)) return "failed";
        shop_pay(play, c);
        return "ok";
    }
    if (state != READY) return "offline";
    if (session.client_player_summon == MP_NO_ID) return "server";
    const mp_dir_entry* e = directory_find(target);
    if (!e) return "no_target";
    mp_writer body = mp_writer_make(payload, sizeof(payload));
    mp_write_summon_request(&body, target, (mp_u8)item, local_look.name);
    if (!send_payload(session.client_player_summon, &body)) return "failed";
    shop_pay(play, c);
    wwhd_log("multiplayer: shop: creature sent to");
    wwhd_log(e->name);
    return "sent";
}

/* CLIENT_WORLD (world_sync.h wd_report); a server without it ignores the sailor's clock. */
static int wd_send_world(const mp_world* w) {
    if (state != READY || session.client_world == MP_NO_ID) return 0;
    mp_writer body = mp_writer_make(payload, sizeof(payload));
    mp_write_world(&body, w);
    return send_payload(session.client_world, &body);
}

/* SERVER_PLAYER_SUMMON: made at the next steps, when the game allows it. */
static void shop_received(const mp_u8* body, mp_u32 length) {
    shop_gift g;
    if (!mp_read_summon(body, length, &g.creature, g.buyer)) {
        drop_frame("multiplayer: malformed summon skipped", body, length);
        return;
    }
    if (g.creature >= SHOP_CREATURES || shop_gift_count == SHOP_QUEUE) return;
    shop_gifts[shop_gift_count++] = g;
}

static void shop_notice_set(const char* buyer, const char* what) {
    map_out o = {shop_notice, 0, sizeof(shop_notice)};
    map_put_name(&o, buyer[0] ? buyer : "A player", MP_NAME_MAX);
    map_put(&o, " sent you ");
    map_put(&o, what);
    map_put(&o, "!");
    shop_notice_bytes = o.size;
    shop_notice_steps = SHOP_NOTICE_STEPS;
}

/* Every step of the local Link: the creatures others bought for this player. */
static void shop_step(const fopAc_ac_c* link, const u8* play) {
    if (shop_notice_steps > 0) --shop_notice_steps;
    if (!shop_gift_count || play[WWHD_PLAY_EVENT_RUNNING_OFFSET]) return;
    shop_gift g = shop_gifts[0];
    for (u32 i = 1; i != shop_gift_count; ++i) shop_gifts[i - 1] = shop_gifts[i];
    --shop_gift_count;
    if (!shop_summon_here(link, g.creature)) return;
    shop_notice_set(g.buyer, shop_creatures[g.creature].label);
}

/* A players.txt line: uid|x|z|heading|RRGGBB|name|stage|sea x|sea z|sea known */
static void map_put_entry(map_out* o, const char* key, const mp_dir_entry* e) {
    map_put(o, key);
    map_put_uint(o, e->id);
    map_put(o, "|");
    map_put_int(o, (s32)e->pos[0]);
    map_put(o, "|");
    map_put_int(o, (s32)e->pos[2]);
    map_put(o, "|");
    map_put_int(o, e->heading);
    map_put(o, "|");
    map_put_rgb(o, e->color);
    map_put(o, "|");
    map_put_name(o, e->name, MP_NAME_MAX);
    map_put(o, "|");
    map_put_name(o, e->stage, STAGE_NAME_BYTES);
    map_put(o, "|");
    map_put_int(o, (s32)e->sea[0]);
    map_put(o, "|");
    map_put_int(o, (s32)e->sea[1]);
    map_put(o, (e->flags & MP_DIR_SEA) ? "|1" : "|0");
    map_put(o, (e->flags & MP_DIR_DEAD) ? "|1\n" : "|0\n"); /* dead (launcher: the skull, no teleport) */
}

static void connect_read(void);            /* the launcher's "Connect to the server", below */

/* ------------------------------------------------------------------ the launcher's Debug page
 *
 * debug.cfg, written by the launcher's Debug page: "id=<number>" (a new id is one request; the id
 * found at the first read is not acted on) and any of "life=" (quarter hearts), "max_life="
 * (quarter hearts, 4-80) and "rupees=". Each value is reached the game's own way, through what the
 * meter adds at its next execute (d_meter_01.cpp, d_meter_02.cpp), so the HUD follows (a life
 * written into the save alone leaves the meter's hearts, play.mItemNowLife, behind: death.h):
 * dComIfG_play_c::mItemMaxLifeCount (play + 0x12A0 + 0x48BE, s16: the meter then fills the hearts),
 * mItemLifeCount (+0x48A4, f32 quarter hearts, capped by the meter to the full hearts),
 * mItemRupeeCount (+0x48A8, s32, as the teleport pays). The meter clears the pending life when it
 * applies a new maximum: a life asked with a maximum is given at a later step. "bot=" (1 pushing,
 * 2 standing by, 0 removed): a crate and a bot in front of the local Link (push_bot.h). players.txt reports
 * "life=", "max_life=" (and "rupees=", already there). */
#define DEBUG_PLAY_MAX_LIFE (0x12A0 + 0x48BE)
#define DEBUG_PLAY_LIFE (0x12A0 + 0x48A4)
#define DEBUG_LIFE_MAX 80
static char debug_text[256];
static int debug_read_once;
static u32 debug_last_id;
static int debug_life_wanted = -1;   /* a life to give once the new maximum is applied */
static void pb_request(u32 mode);    /* "bot=": the push bot (push_bot.h, included below) */

/* "key=number" in debug.cfg: 1 and the number when the key is there. */
static int debug_cfg_value(const char* text, u32 size, const char* key, u32* value) {
    u32 klen = 0;
    while (key[klen]) ++klen;
    for (u32 pos = 0; pos < size;) {
        u32 end = pos;
        while (end < size && text[end] != '\n') ++end;
        if (end - pos > klen && text[pos + klen] == '=') {
            u32 k = 0;
            while (k != klen && text[pos + k] == key[k]) ++k;
            if (k == klen && pos + klen + 1 < end && text[pos + klen + 1] >= '0' && text[pos + klen + 1] <= '9') {
                u32 v = 0;
                for (u32 i = pos + klen + 1; i < end && text[i] >= '0' && text[i] <= '9' && v < 100000u; ++i)
                    v = v * 10 + (u32)(text[i] - '0');
                *value = v;
                return 1;
            }
        }
        pos = end + 1;
    }
    return 0;
}

static void debug_give_life(u8* play, u32 life) {
    u32 max = dComIfGs_player_status_a.max_life;
    if (life > max) life = max;
    *(volatile f32*)(play + DEBUG_PLAY_LIFE) = (f32)((s32)life - (s32)dComIfGs_player_status_a.life);
    wwhd_log_int("multiplayer: debug: life (quarter hearts)", (int)life);
}

static void debug_step(u8* play) {
    if (debug_life_wanted >= 0 && *(const volatile s16*)(play + DEBUG_PLAY_MAX_LIFE) == 0) {
        debug_give_life(play, (u32)debug_life_wanted); /* the new maximum is in */
        debug_life_wanted = -1;
    }
    s32 got = wwhd_file_read("debug.cfg", debug_text, sizeof(debug_text));
    if (got < 0) {
        debug_read_once = 1;
        return;
    }
    u32 id = map_cfg_number(debug_text, (u32)got, "id");
    if (!debug_read_once) {
        debug_read_once = 1;
        debug_last_id = id; /* older than this game: not acted on */
        return;
    }
    if (!id || id == debug_last_id) return;
    debug_last_id = id;
    if (play[WWHD_PLAY_EVENT_RUNNING_OFFSET]) wwhd_log("multiplayer: debug: applied during an event");
    u32 v;
    int max_changed = 0;
    if (debug_cfg_value(debug_text, (u32)got, "max_life", &v)) {
        if (v < 4) v = 4;
        if (v > DEBUG_LIFE_MAX) v = DEBUG_LIFE_MAX;
        s32 delta = (s32)v - (s32)dComIfGs_player_status_a.max_life;
        if (delta) {
            *(volatile s16*)(play + DEBUG_PLAY_MAX_LIFE) = (s16)delta;
            max_changed = 1;
            wwhd_log_int("multiplayer: debug: max life (quarter hearts)", (int)v);
        }
    }
    if (debug_cfg_value(debug_text, (u32)got, "life", &v)) {
        if (v > DEBUG_LIFE_MAX) v = DEBUG_LIFE_MAX;
        if (max_changed) debug_life_wanted = (int)v;
        else debug_give_life(play, v);
    }
    if (debug_cfg_value(debug_text, (u32)got, "rupees", &v)) {
        if (v > 9999) v = 9999;
        *(volatile s32*)(play + PLAY_ITEM_RUPEE_COUNT) = (s32)v - (s32)map_rupees();
        wwhd_log_int("multiplayer: debug: rupees", (int)v);
    }
    if (debug_cfg_value(debug_text, (u32)got, "bot", &v)) pb_request(v);
}
static const char* connection_state(void);

static void map_step(void* link_actor, double dt) {
    fopAc_ac_c* link = (fopAc_ac_c*)link_actor;
    u8* play = wwhd_play_get();
    if (!play) return;
    if (map_on_sea(play)) {
        local_sea[0] = link->current.pos.x;
        local_sea[1] = link->current.pos.z;
        local_sea_known = 1;
    }
    teleport_stage_step(play);
    shop_step(link, play);
    bonus_tick();
    dir_clock -= dt;
    if (dir_clock <= 0) {
        dir_clock = DIR_PERIOD;
        directory_send(link, play);
    }
    map_clock -= dt;
    if (map_clock > 0) return;
    map_clock = MAP_PERIOD;

    s32 got = wwhd_file_read("teleport.cfg", teleport_text, sizeof(teleport_text));
    if (got >= 0) {
        u32 id = map_cfg_number(teleport_text, (u32)got, "id");
        if (!teleport_read_once) {
            teleport_last_id = id; /* older than this game: not acted on */
        } else if (id && id != teleport_last_id) {
            teleport_last_id = id;
            teleport_result_id = id;
            teleport_result = teleport_try(link, play, map_cfg_number(teleport_text, (u32)got, "target"));
        }
    }
    teleport_read_once = 1;

    got = wwhd_file_read("shop.cfg", shop_text, sizeof(shop_text));
    if (got >= 0) {
        u32 id = map_cfg_number(shop_text, (u32)got, "id");
        if (!shop_read_once) {
            shop_last_id = id; /* older than this game: not acted on */
        } else if (id && id != shop_last_id) {
            shop_last_id = id;
            shop_result_id = id;
            shop_result = shop_try(link, play, map_cfg_number(shop_text, (u32)got, "item"),
                                   map_cfg_number(shop_text, (u32)got, "target"));
        }
    }
    shop_read_once = 1;
    debug_step(play);       /* the launcher's Debug page */
    connect_read(); /* the launcher's "Connect to the server" */

    map_out o = {map_text, 0, sizeof(map_text)};
    mp_dir_entry me;
    me.id = directory_valid ? directory.self : (local_uid == MP_NO_ID ? 0 : local_uid);
    mp_copy_text(me.name, sizeof(me.name), local_look.name, MP_NAME_MAX);
    me.color = (mp_u16)local_look.colors[0];
    map_stage(play, me.stage);
    me.pos[0] = link->current.pos.x;
    me.pos[1] = link->current.pos.y;
    me.pos[2] = link->current.pos.z;
    me.heading = link->shape_angle.y;
    me.flags = local_sea_known ? MP_DIR_SEA : 0;
    if (dh_local_dead()) me.flags |= MP_DIR_DEAD;
    me.sea[0] = local_sea[0];
    me.sea[1] = local_sea[1];
    map_put(&o, "stage=");
    map_put_name(&o, me.stage, STAGE_NAME_BYTES);
    map_put(&o, "\nrupees=");
    map_put_uint(&o, map_rupees());
    map_put(&o, "\nbombs=");
    map_put_uint(&o, (u32)(shop_bombs(play) > 0 ? shop_bombs(play) : 0));
    map_put(&o, "\nlife=");
    map_put_uint(&o, dComIfGs_player_status_a.life);
    map_put(&o, "\nmax_life=");
    map_put_uint(&o, dComIfGs_player_status_a.max_life);
    map_put(&o, "\nstep=");
    map_put_uint(&o, (u32)wwhd_logic_step());
    map_put(&o, directory_valid ? "\ndirectory=1\n" : "\ndirectory=0\n");
    map_put(&o, "connection=");
    map_put(&o, connection_state());
    map_put(&o, "\nchart=");
    for (u32 i = 0; i != SEA_SQUARES; ++i) map_put(&o, sea_square_unlocked(i) ? "1" : "0");
    map_put(&o, "\n");
    map_put_entry(&o, "me=", &me);
    if (state == READY && directory_valid) {
        for (mp_u32 i = 0; i != directory.count; ++i)
            if (directory.entry[i].id != directory.self) map_put_entry(&o, "player=", &directory.entry[i]);
    } else if (state == READY) {
        for (int i = 0; i != MAX_REMOTES; ++i) {
            const remote_player* r = &remotes[i];
            if (!r->active || !r->has_transform) continue;
            mp_player_look look = r->has_look ? r->look : mp_player_look_default();
            mp_dir_entry e;
            e.id = r->uid;
            mp_copy_text(e.name, sizeof(e.name), look.name, MP_NAME_MAX);
            e.color = (mp_u16)look.colors[0];
            for (int k = 0; k != STAGE_NAME_BYTES + 1; ++k) e.stage[k] = me.stage[k];
            e.pos[0] = r->transform.pos[0];
            e.pos[1] = r->transform.pos[1];
            e.pos[2] = r->transform.pos[2];
            e.heading = r->transform.rot[1];
            e.flags = (look.death & (MP_DEATH_GHOST | MP_DEATH_DOWN)) ? MP_DIR_DEAD : 0;
            e.sea[0] = e.sea[1] = 0.0f;
            map_put_entry(&o, "player=", &e);
        }
    }
    if (teleport_result_id) {
        map_put(&o, "teleport=");
        map_put_uint(&o, teleport_result_id);
        map_put(&o, "|");
        map_put(&o, teleport_result);
        map_put(&o, "\n");
    }
    map_put(&o, "spoils=");
    for (u32 i = 0; i != SHOP_SPOILS; ++i) {
        s32 n = shop_spoils(play, i);
        map_put_int(&o, n < 0 ? 0 : n);
        map_put(&o, i + 1 == SHOP_SPOILS ? "\n" : ",");
    }
    map_put(&o, "bonus=");
    map_put_uint(&o, bonus_hearts);
    map_put(&o, ",");
    map_put_uint(&o, bonus_magic);
    map_put(&o, bonus_heart_full() ? ",1" : ",0");
    map_put(&o, bonus_magic_full() ? ",1\n" : ",0\n");
    if (shop_result_id) {
        map_put(&o, "shop=");
        map_put_uint(&o, shop_result_id);
        map_put(&o, "|");
        map_put(&o, shop_result);
        map_put(&o, "\n");
    }
    wwhd_file_write("players.txt", map_text, o.size);
}

static void stop_remote_views(void) {
    sv_close();
    el_reset();
    as_session = 0;
    as_reset_network();
    hide_markers();
    puppet_clear_all();
    ship_puppet_clear_all();
    swing_clear_all();
}

/* The launcher's "Connect to the server" (Play Online): connect.cfg "id=" (a new id is one
 * request, the one found at the first read is not acted on), read with the map files. The current
 * connection, if any, is closed, everything the session made is cleared as when it is lost, then
 * start() connects again with launcher.cfg's host and port, read anew. players.txt "connection=":
 * connecting, connected or offline. */
static char connect_text[128];
static int connect_read_once, connect_requested, hud_registered;
static u32 connect_last_id;

static const char* connection_state(void) {
    if (state == READY) return "connected";
    if (state == IDLE || state == WAIT_INIT) return "connecting";
    return "offline";
}

static void connect_read(void) {
    s32 got = wwhd_file_read("connect.cfg", connect_text, sizeof(connect_text));
    if (got >= 0) {
        u32 id = map_cfg_number(connect_text, (u32)got, "id");
        if (connect_read_once && id && id != connect_last_id) connect_requested = 1;
        connect_last_id = id;
    }
    connect_read_once = 1;
}

static void reconnect(void) {
    if (state != IDLE && state != FAILED) wwhd_net_disconnect();
    stop_remote_views();
    clear_area_state();
    for (int i = 0; i != STAGE_NAME_BYTES + 1; ++i) area[i] = 0; /* entered again once connected */
    area_settle = 0;
    directory_valid = 0;
    teleport_stage_wait = 0;
    memset(&sent_look, 0, sizeof(sent_look));
    state = IDLE;
    wwhd_log("multiplayer: connecting again (launcher)");
}

static void multiplayer_connection_step(void* link) {
    if (connect_requested) {
        connect_requested = 0;
        reconnect();
    }
    if (state == IDLE) {
        start();
        if (!hud_registered) {
            hud_registered = 1;
            wwhd_hud_register(draw_markers, WWHD_HUD_TV);
            wwhd_hud_register_drc(draw_gamepad);
        }
    }
    double dt = wwhd_logic_dt();
    refresh_launcher_settings((const daPy_lk_c*)link, dt);
    map_step(link, dt); /* the launcher's map and its teleport requests */
    outfit_local_step((u32)link); /* the chosen clothes, with or without a server */
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
    sv_step(wwhd_play_get());
    wd_step(wwhd_play_get());
    el_update_crowns(wwhd_play_get());

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
    swing_step(link);
    if (anim_capture) swing_capture(link, &anim_local);
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
    if (local_uid != MP_NO_ID) as_step(dt, local_uid, (const fopAc_ac_c*)link);
    ps_step();

    log_clock -= dt;
    if (log_clock <= 0) {
        log_clock = LOG_PERIOD;
        log_remotes();
        as_log_counts();
    }
}

#include "push_bot.h" /* the Debug page's bot at a crate */
#include "heavy.h" /* heavy things take several players */

WWHD_GAME_ORIGINAL(WWHD_ADDR_daPy_Execute, s32, daPy_Execute_original, (void* i_this));

/* daPy_Execute (0240EBB0): a puppet only runs its visual update; any other Link runs the game's
 * execute, then the multiplayer step (an entry hook before puppets existed). */
WWHD_REPLACE(WWHD_ADDR_daPy_Execute, s32, multiplayer_daPy_Execute, (void* i_this)) {
    int skin = skin_enter((u32)i_this); /* its models from its skin (skin.h) */
    if (puppet_is(i_this)) {
        s32 result = puppet_execute(i_this);
        skin_leave(skin);
        return result;
    }
    bonus_note_link(i_this); /* the yellow hearts and magic follow this Link (bonus.h) */
    dh_note_link(i_this);    /* its death and revive (death.h) */
    el_link_step(i_this); /* an electric elite's shock, given in Link's own execute (elite.h) */
    s32 result = daPy_Execute_original(i_this);
    skin_leave(skin);
    multiplayer_connection_step(i_this);
    dh_step(i_this, wwhd_logic_dt());
    hv_step(i_this, wwhd_logic_dt());
    pb_step(i_this);
    return result;
}
