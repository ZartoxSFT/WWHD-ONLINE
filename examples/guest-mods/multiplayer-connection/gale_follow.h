/* 0.3.75: follow a player's Ballad of Gales.
 *
 * The game (decomp, verified): the Ballad of Gales (song 1) at sea makes a cyclone,
 * fpcNm_TORNADO_e 0x1BB (d_a_player_main_07.cpp procTactPlay_init: fopAcM_create(TORNADO, 1, Link's
 * place), its id in the ship's mTactWarpID +0x714); the sea chart gives the destination
 * (mTactWarpPosNum +0x6AC, -1 none); then Link paddles (procShipPaddle_init 023F1E60) and the ship
 * is set to MODE_TACT_WARP_e 14 (mNextMode +0x636): daShip_c::procTactWarp_init (0247DBF8, writes
 * mCurMode +0x635 = 14, checked in the generated code) and procTactWarp (02482438, d_a_ship.cpp:
 * the cyclone lifts the ship, then dStage_changeScene(mTactWarpPosNum + 0xC5)).
 *
 * Here:
 *  - the local ship entering MODE_TACT_WARP with a destination: "#GALE_xx" (xx the destination,
 *    hex) to the others (item_share.h transport);
 *  - received: for GF_OFFER_STEPS a notice "<player> played the Ballad of Gales: hold A to follow"
 *    with a bar; on the player's own ship at sea (stage "sea", riding, out of any event), A held
 *    GF_HOLD_STEPS (1 s) does what the song does: a cyclone at the local Link, its id and the
 *    destination in the ship, Link paddling, the ship in MODE_TACT_WARP. Not sent again for it. */
#pragma once
#include "wwhd_guest.h"
#include "wwhd/bindings.h"

#define GF_PROC_TORNADO 0x1BB
#define GF_SHIP_CUR_MODE 0x635
#define GF_SHIP_NEXT_MODE 0x636
#define GF_SHIP_WARP_POS 0x6AC
#define GF_SHIP_WARP_ID 0x714
#define GF_MODE_TACT_WARP 14
#define GF_OFFER_STEPS 600        /* 20 s */
#define GF_HOLD_STEPS 30          /* A held 1 s */
#define GF_BUTTON_A 0x01u         /* daPy_lk_c::mItemButton (death.h DH_LINK_ITEM_BUTTON) */

static int gf_sent;               /* "#GALE" sent for this warp */
static int gf_following;          /* this warp follows another player's: not sent */
static int gf_offer_pos = -1;
static unsigned long long gf_offer_until;
static char gf_offer_name[MP_NAME_MAX + 1];
static int gf_hold;
static int gf_eligible;
static cXyz gf_pos;
static char gf_text[96];
static u32 gf_text_bytes;

static u32 gf_ship(const u8* play) {
    return play ? *(const volatile u32*)(play + AS_PLAY_SHIP) : 0;
}

/* item_share.h share_received: "#GALE_xx". */
static void gf_received(const mp_event* e) {
    const char* n = e->name;
    int a = share_hex(n[6]), b = share_hex(n[7]);
    if (n[5] != '_' || a < 0 || b < 0) return;
    gf_offer_pos = a << 4 | b;
    gf_offer_until = wwhd_logic_step() + GF_OFFER_STEPS;
    gf_hold = 0;
    remote_player* r = find_remote(e->sender, 0);
    const char* who = r && r->has_look && r->look.name[0] ? r->look.name : "A player";
    for (u32 i = 0; i != MP_NAME_MAX + 1; ++i) gf_offer_name[i] = 0;
    for (u32 i = 0; i != MP_NAME_MAX && who[i]; ++i) gf_offer_name[i] = who[i];
    wwhd_log_int("multiplayer: a player played the Ballad of Gales, destination", gf_offer_pos);
}

static void gf_follow(void* link_actor, u32 ship) {
    const fopAc_ac_c* link = (const fopAc_ac_c*)link_actor;
    gf_pos.x = link->current.pos.x;
    gf_pos.y = link->current.pos.y;
    gf_pos.z = link->current.pos.z;
    u8 saved = as_context;
    as_context = AS_CTX_NONE; /* this game's own cyclone */
    u32 id = wwhd_fopAcM_create(GF_PROC_TORNADO, 1, (u32)&gf_pos, link->current.roomNo, 0, 0, 0xFFFFFFFFu, 0);
    as_context = saved;
    if (id == 0xFFFFFFFFu) {
        wwhd_log("multiplayer: Ballad of Gales: the cyclone was refused");
        return;
    }
    *(volatile u32*)(ship + GF_SHIP_WARP_ID) = id;
    *(volatile s32*)(ship + GF_SHIP_WARP_POS) = gf_offer_pos;
    wwhd_daPy_lk_c__procShipPaddle_init(link_actor);
    *(volatile u8*)(ship + GF_SHIP_NEXT_MODE) = GF_MODE_TACT_WARP;
    gf_following = 1;
    wwhd_log_int("multiplayer: Ballad of Gales: following, destination", gf_offer_pos);
    gf_offer_pos = -1;
}

/* Every step of the local Link (mod.c). */
static void gf_step(void* link_actor) {
    const u8* play = wwhd_play_get();
    if (!play || !link_actor) return;
    u32 ship = gf_ship(play);
    u8 mode = ship ? *(const volatile u8*)(ship + GF_SHIP_CUR_MODE) : 0;
    if (mode != GF_MODE_TACT_WARP) {
        gf_sent = 0;
        gf_following = 0;
    } else if (!gf_sent && !gf_following) { /* the local Link's own Ballad of Gales */
        s32 pos = *(const volatile s32*)(ship + GF_SHIP_WARP_POS);
        if (pos >= 0 && pos < 0x100) {
            static const char hex[] = "0123456789ABCDEF";
            char name[9] = {'#', 'G', 'A', 'L', 'E', '_', hex[(pos >> 4) & 0xF], hex[pos & 0xF], 0};
            share_send(name);
            gf_sent = 1;
            wwhd_log_int("multiplayer: Ballad of Gales sent to the others, destination", (int)pos);
        }
    }
    if (gf_offer_pos < 0) return;
    if (wwhd_logic_step() >= gf_offer_until) {
        gf_offer_pos = -1;
        return;
    }
    char stage[STAGE_NAME_BYTES + 1];
    map_stage(play, stage);
    int at_sea = stage[0] == 's' && stage[1] == 'e' && stage[2] == 'a' && !stage[3];
    int riding = (*(const volatile u32*)(play + PUPPET_PLAY_STATUS0) & PUPPET_STATUS0_RIDE) != 0;
    gf_eligible = at_sea && riding && ship && !play[WWHD_PLAY_EVENT_RUNNING_OFFSET] && !teleport_stage_wait &&
                  mode != GF_MODE_TACT_WARP;
    const daPy_lk_c* link = (const daPy_lk_c*)link_actor;
    if (gf_eligible && (link->mItemButton & GF_BUTTON_A)) ++gf_hold;
    else gf_hold = 0;
    if (gf_hold >= GF_HOLD_STEPS) gf_follow(link_actor, ship);
}

/* mod.c draw_markers: the notice and the hold bar. */
static void gf_draw(u32 list) {
    if (gf_offer_pos < 0 || chart_menu_open()) return;
    map_out o = {gf_text, 0, sizeof(gf_text)};
    map_put(&o, gf_offer_name);
    map_put(&o, gf_eligible ? " played the Ballad of Gales: hold A to follow"
                            : " played the Ballad of Gales: be on your boat at sea to follow");
    gf_text_bytes = o.size;
    name_draw(list, CHART_LAYOUT_W * 0.5f, 160.0f, gf_text, gf_text_bytes, 0x9FE6FFu, 0.55f, 1.0f);
    if (!gf_eligible) return;
    static wwhd_hud_element bar;
    const f32 w = 260.0f, h = 10.0f, x = CHART_LAYOUT_W * 0.5f - w * 0.5f, y = 176.0f;
    bar = (wwhd_hud_element){.kind = WWHD_HUD_RECT, .anchor = WWHD_HUD_TOP_LEFT, .blend = WWHD_HUD_ALPHA,
                             .x = x, .y = y, .w = w, .h = h, .thickness = 1, .u1 = 1, .v1 = 1, .rgba = 0x00000099u};
    wwhd_hud_emit(list, &bar);
    if (gf_hold > 0) {
        bar.w = w * (f32)gf_hold / (f32)GF_HOLD_STEPS;
        bar.rgba = 0x9FE6FFFFu;
        wwhd_hud_emit(list, &bar);
    }
    bar.kind = WWHD_HUD_RECT_OUTLINE;
    bar.w = w;
    bar.rgba = 0xFFFFFFCCu;
    wwhd_hud_emit(list, &bar);
}
