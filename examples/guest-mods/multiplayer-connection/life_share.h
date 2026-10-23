/* 0.3.92: the server mutator "shared life": every player has the same life, yellow hearts included.
 *
 * The server keeps the one life (WWClient.java): quarter hearts of the game's meter and the yellow
 * hearts (bonus.h bonus_hearts). Each game watches its own: the save's life
 * (dComIfGs_player_status_a.life) plus the pending life the meter has still to add (play + 0x5B44,
 * f32, mItemLifeCount: damage and healing go there first, death.h, bonus.h), and its yellow hearts.
 * A change made here (a hit, a heart picked up, a fairy, a yellow heart bought or spent) goes to the
 * server as "#HD_<life change>_<yellow change>_<life>_<yellow>_<max life>" (events of the room,
 * item_share.h transport); the server adds it to the shared life (the first one it gets sets it),
 * within 0..max, and sends "#HS_<life>_<yellow>" to every player, the sender too. Each game then
 * gives its Link that life the game's way (the pending life set to the difference, the meter shows
 * it, as death.h's revive does) and its yellow hearts.
 * At 0 every Link dies (the game's death, death.h: no survivor for the ghosts). A Link given life
 * again after that (the game over's continue) sends "#HR_<life>_<yellow>_<max>": the server takes it
 * while the shared life is 0, the others then get it too.
 * Max life: the save synchronisation shares the heart containers (save_sync.h), so the players have
 * the same; each game keeps the life within its own max anyway. */
#pragma once
#include "wwhd_guest.h"
#include "wwhd/save.h"

#define LS_PLAY_LIFE 0x5B44        /* dComIfG_play_c::mItemLifeCount, f32 quarter hearts (bonus.h) */
#define LS_INIT_STEPS 90           /* the first report again every 3 s until the server answers */

static int ls_has_base, ls_known, ls_new;
static s32 ls_base_life, ls_base_yellow;
static s32 ls_target_life, ls_target_yellow, ls_server_life = -1;
static unsigned long long ls_init_step;
static int ls_was_on;

static void ls_send(const char* text) {
    const u8* play = wwhd_play_get();
    const fopAc_ac_c* link = (const fopAc_ac_c*)dh_link;
    if (!play || !link || state != READY || local_uid == MP_NO_ID || session.client_player_event == MP_NO_ID) return;
    mp_event e;
    e.sender = local_uid;
    map_stage(play, e.stage);
    if (!e.stage[0]) return;
    e.room = link->current.roomNo;
    e.has_place = 0;
    int n = 0;
    for (; text[n] && n != MP_EVENT_NAME_MAX; ++n) e.name[n] = text[n];
    e.name[n] = 0;
    mp_writer body = mp_writer_make(payload, sizeof(payload));
    mp_write_event(&body, &e);
    send_payload(session.client_player_event, &body);
}

static u32 ls_put(char* out, u32 n, s32 v) {
    char digits[12];
    int d = 0;
    u32 u = v < 0 ? 0u - (u32)v : (u32)v;
    do { digits[d++] = (char)('0' + u % 10u); u /= 10u; } while (u && d != 11);
    out[n++] = '_';
    if (v < 0) out[n++] = '-';
    while (d) out[n++] = digits[--d];
    return n;
}

/* "#Hx" then the numbers. */
static void ls_report(char kind, const s32* v, u32 count) {
    char text[MP_EVENT_NAME_MAX + 1];
    u32 n = 0;
    text[n++] = '#';
    text[n++] = 'H';
    text[n++] = kind;
    for (u32 i = 0; i != count && n + 13 < sizeof(text); ++i) n = ls_put(text, n, v[i]);
    text[n] = 0;
    ls_send(text);
}

/* A number of "#HS_l_y" at *i (after its '_'), -1 when malformed. */
static s32 ls_number(const char* s, u32* i) {
    if (s[*i] != '_') return -1;
    ++*i;
    s32 v = 0;
    u32 start = *i;
    while (s[*i] >= '0' && s[*i] <= '9' && v < 100000) v = v * 10 + (s[(*i)++] - '0');
    return *i == start ? -1 : v;
}

/* item_share.h share_received: "#HS_<life>_<yellow>" from the server. */
static void ls_received(const mp_event* e) {
    u32 i = 3;
    s32 life = ls_number(e->name, &i), yellow = ls_number(e->name, &i);
    if (life < 0 || yellow < 0 || e->name[i]) return;
    ls_target_life = life;
    ls_target_yellow = yellow > BONUS_HEARTS_MAX ? BONUS_HEARTS_MAX : yellow;
    ls_server_life = life;
    ls_new = 1;
    if (!ls_known) wwhd_log_int("multiplayer: shared life: the server's life (quarter hearts)", life);
    ls_known = 1;
}

/* mod.c, after the local Link's execute. */
static void ls_step(void) {
    const u8* play = wwhd_play_get();
    int on = session.shared_life && state == READY;
    if (!on) {
        if (ls_was_on) wwhd_log("multiplayer: shared life: off");
        ls_was_on = ls_has_base = ls_known = ls_new = 0;
        ls_server_life = -1;
        return;
    }
    if (!ls_was_on) wwhd_log("multiplayer: shared life: on");
    ls_was_on = 1;
    /* between two areas (no player uid: nothing can be sent) and while dead, nothing is counted */
    if (local_uid == MP_NO_ID || !play || !dh_link || dh_ghost || dh_dying_anim || dh_reviving) return;
    bonus_load();
    volatile f32* pending = (volatile f32*)(play + LS_PLAY_LIFE);
    s32 max = (s32)dComIfGs_player_status_a.max_life;
    s32 life = (s32)dComIfGs_player_status_a.life + (s32)(*pending >= 0.0f ? *pending + 0.5f : *pending - 0.5f);
    s32 yellow = (s32)bonus_hearts;
    if (life < 0) life = 0;
    if (!ls_has_base) {
        ls_has_base = 1;
        ls_base_life = life;
        ls_base_yellow = yellow;
    }
    s32 dl = life - ls_base_life, dy = yellow - ls_base_yellow;
    if (!ls_known && wwhd_logic_step() - ls_init_step >= LS_INIT_STEPS) { /* the first: sets it if none */
        ls_init_step = wwhd_logic_step();
        s32 v[5] = {0, 0, life, yellow, max};
        ls_report('D', v, 5);
        ls_base_life = life;
        ls_base_yellow = yellow;
    } else if (dl || dy) {
        if (ls_base_life == 0 && life > 0 && ls_server_life == 0) { /* given life again after the death */
            s32 v[3] = {life, yellow, max};
            ls_report('R', v, 3);
            wwhd_log_int("multiplayer: shared life: back to life, sent", life);
        } else {
            s32 v[5] = {dl, dy, life, yellow, max};
            ls_report('D', v, 5);
        }
        ls_base_life = life;
        ls_base_yellow = yellow;
    }
    if (!ls_new) return;
    ls_new = 0;
    s32 want = ls_target_life > max ? max : ls_target_life;
    s32 saved = (s32)dComIfGs_player_status_a.life;
    *pending = (f32)(want - saved); /* the meter adds it (death.h: the game's way) */
    if ((s32)bonus_hearts != ls_target_yellow) {
        bonus_hearts = (u32)ls_target_yellow;
        bonus_dirty = 1;
    }
    ls_base_life = want;
    ls_base_yellow = ls_target_yellow;
}
