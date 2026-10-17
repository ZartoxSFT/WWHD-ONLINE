/* The dungeon map of the GamePad (HdPadDungeon, layout DungeonMap_00; on the TV with the Pro
 * Controller): where another player of the same dungeon goes on it, computed the way the game places
 * Link's own icon (mod.c draw_dungeon_players).
 *
 * Verified (decomp d/hd_screen_dungeon_map.cpp, include/d/hd_gamepad_map.h; the rest read in the EU
 * cking.rpx and mapped to canonical USA addresses with tools/recomp/builds/eu.json):
 *  - hdDungeonUpdateMarkers (0265799C) runs with the screen object while its markers are active
 *    (not busy, mMarkersActive); it places the boss and Link icons. Its map is at +0x920.
 *  - 02655870 (US): Link's place on the map = his world (x, z) (play + 0x5B34 -> +0x314 / +0x31C)
 *    minus the screen's origin (+0x2FC, +0x300), kept at +0x310 / +0x314.
 *  - 026568E8: Link's icon object is at screen + 0xBC; that place is copied to the icon's +0x58 /
 *    +0x5C (its struct at +0x50: the pane at +0, x +8, y +0xC). The floor shown is screen + 0x3D0:
 *    026569E0 shows another icon only when map_GetFloorNo(its y) equals it.
 *  - 0268531C (the icons' "update place", vtable slot 0x114): the place at +0x58 goes through the
 *    slots 0x124, 0x12C, 0x134, 0x13C and the result is the pane's place +0x68 / +0x6C. The icon
 *    classes of these screens (vtables 100F0AC0, 100F0F00, 100F3BE8, 100F96C8 in the EU binary)
 *    use there 02685430: (x, y) * s, y negated, s = *(f32*)(map + 0x4B4); 02685478: turned by the
 *    map's angle (map + 0x4AC, degrees, about the floor cell's centre; sin/cos table); 026854D8:
 *    times the floor cell's (icon + 0x94) f32 at +0x2C; 02685534: plus the cell's place.
 *    The result is an affine map of the place: another player's pane place is Link's (+0x68)
 *    plus that linear part applied to (their place - Link's place).
 * Deduced, not verified: that Link's icon in the dungeon map is one of those classes (two other
 * classes scale differently); that the pane's translation is +0x68 (02685CD4 copies the pane's
 * translation there when it binds it). Each new screen logs those values once and, when Link moves,
 * how well the prediction matches his own icon, for the next fix. */
#pragma once
#include "wwhd_guest.h"
#include "sea_chart.h"

#define DMAP_LINK_ICON 0xBC
#define DMAP_ORIGIN 0x2FC        /* f32 x, then z at +0x300 */
#define DMAP_FLOOR_SHOWN 0x3D0   /* u8 */
#define DMAP_PLAY_STAGE 0x5150   /* map_GetFloorNo's stage argument (unused by it) */
#define ICON_PANE 0x50
#define ICON_PLACE 0x58          /* f32 x, y: place on the map */
#define ICON_SCREEN 0x68         /* f32 x, y: the pane's place */
#define ICON_MAP 0x90
#define ICON_CELL 0x94
#define MAP_ANGLE 0x4AC
#define MAP_SCALE 0x4B4          /* pointer to an f32 */
#define CELL_SCALE 0x2C
#define DMAP_CHECK_MOVE 200.0f   /* map units Link moves between two checks */

typedef struct dmap_view {
    f32 origin_x, origin_z;      /* world -> map place */
    f32 place_x, place_y;        /* Link's map place */
    f32 pane_x, pane_y;          /* Link's pane place */
    f32 jxx, jxy, jyx, jyy;      /* map place delta -> pane place delta */
    chart_space parent;          /* the pane's parent on the screen */
    f32 icon;                    /* Link's icon size on the screen layout */
    u8 floor;
} dmap_view;

static u32 dmap_screen;
static unsigned long long dmap_step;
static u32 dmap_seen;            /* the screen whose values were logged */
static dmap_view dmap_v;
static int dmap_ok;
static int dmap_have_check;
static f32 dmap_check[4];        /* place x, y; pane x, y at the last check */
static int dmap_checks_logged;

static void dmap_players_update(void); /* mod.c: the other players' places, while the map updates */

static int dmap_ptr(u32 p) { return p && !(p & 3); }

/* sin, cos of an angle in degrees (no libm): Taylor on [-pi, pi]. */
static void dmap_sincos(f32 degrees, f32* sn, f32* cs) {
    f32 h = degrees * (3.14159265f / 180.0f);
    while (h > 3.14159265f) h -= 6.28318531f;
    while (h < -3.14159265f) h += 6.28318531f;
    f32 s2 = h * h;
    *sn = h * (1.0f - s2 / 6.0f * (1.0f - s2 / 20.0f * (1.0f - s2 / 42.0f * (1.0f - s2 / 72.0f))));
    *cs = 1.0f - s2 / 2.0f * (1.0f - s2 / 12.0f * (1.0f - s2 / 30.0f * (1.0f - s2 / 56.0f * (1.0f - s2 / 90.0f))));
}

/* Why the last read failed (logged once per screen): 0 none. */
static int dmap_fail;
static u32 dmap_fail_screen;

static int dmap_read_values(u32 screen, dmap_view* v) {
    u32 icon = tunic_ld32(screen + DMAP_LINK_ICON);
    if (!dmap_ptr(icon)) return 1;
    u32 pane = tunic_ld32(icon + ICON_PANE), map = tunic_ld32(icon + ICON_MAP), cell = tunic_ld32(icon + ICON_CELL);
    if (!dmap_ptr(pane)) return 2;
    if (!dmap_ptr(map)) return 3;
    if (!dmap_ptr(cell)) return 4;
    u32 scale_at = tunic_ld32(map + MAP_SCALE);
    if (!dmap_ptr(scale_at)) return 5;
    f32 s = *(const volatile f32*)scale_at;
    f32 c = *(const volatile f32*)(cell + CELL_SCALE);
    f32 angle = *(const volatile f32*)(map + MAP_ANGLE);
    if (!chart_finite(s) || !chart_finite(c) || !chart_finite(angle) || s == 0.0f || c == 0.0f) return 6;
    /* Link's icon is the root pane of its own layout (02685D30 binds the layout's root when the
     * icon names no pane): no parent, its translation is then on the screen layout itself
     * (origin in the middle, y up). A named pane inside a layout goes through its parents. */
    u32 parent = tunic_ld32(pane + PANE_PARENT);
    if (!parent) {
        if (!(*(const volatile u8*)(pane + PANE_FLAGS) & PANE_VISIBLE)) return 7;
        v->parent.ox = v->parent.oy = 0.0f;
        v->parent.kx = v->parent.ky = 1.0f;
    } else if (!dmap_ptr(parent) || !chart_pane_space(parent, &v->parent)) {
        return 8;
    }
    f32 sn, cs;
    dmap_sincos(angle, &sn, &cs);
    /* (dx, dy) -> (dx s, -dy s) -> turned -> times c */
    v->jxx = c * s * cs;
    v->jxy = c * s * sn;   /* times -dy: -(-sn) */
    v->jyx = c * s * sn;
    v->jyy = -c * s * cs;
    v->origin_x = *(const volatile f32*)(screen + DMAP_ORIGIN);
    v->origin_z = *(const volatile f32*)(screen + DMAP_ORIGIN + 4);
    v->place_x = *(const volatile f32*)(icon + ICON_PLACE);
    v->place_y = *(const volatile f32*)(icon + ICON_PLACE + 4);
    v->pane_x = *(const volatile f32*)(icon + ICON_SCREEN);
    v->pane_y = *(const volatile f32*)(icon + ICON_SCREEN + 4);
    v->floor = *(const volatile u8*)(screen + DMAP_FLOOR_SHOWN);
    if (!chart_finite(v->origin_x) || !chart_finite(v->origin_z) || !chart_finite(v->place_x) ||
        !chart_finite(v->place_y) || !chart_finite(v->pane_x) || !chart_finite(v->pane_y)) return 9;
    v->icon = *(const volatile f32*)(pane + PANE_SIZE) * v->parent.kx * *(const volatile f32*)(pane + PANE_SCALE);
    if (!(v->icon > 8.0f && v->icon < 160.0f)) v->icon = 40.0f;
    if (dmap_seen != screen) {
        dmap_seen = screen;
        dmap_have_check = 0;
        dmap_checks_logged = 0;
        wwhd_log_hex("multiplayer: dungeon map screen", screen);
        wwhd_log_float("multiplayer: dungeon map   map scale", (double)s);
        wwhd_log_float("multiplayer: dungeon map   cell scale", (double)c);
        wwhd_log_float("multiplayer: dungeon map   angle", (double)angle);
        wwhd_log_float("multiplayer: dungeon map   Link place x", (double)v->place_x);
        wwhd_log_float("multiplayer: dungeon map   Link place y", (double)v->place_y);
        wwhd_log_float("multiplayer: dungeon map   Link pane x", (double)v->pane_x);
        wwhd_log_float("multiplayer: dungeon map   pane translate x", (double)*(const volatile f32*)(pane + PANE_TRANSLATE));
        wwhd_log_float("multiplayer: dungeon map   Link pane y", (double)v->pane_y);
        wwhd_log_float("multiplayer: dungeon map   pane translate y", (double)*(const volatile f32*)(pane + PANE_TRANSLATE + 4));
        wwhd_log_int("multiplayer: dungeon map   floor shown", v->floor);
        wwhd_log_int("multiplayer: dungeon map   icon is a layout root", !parent);
    }
    return 0;
}

static int dmap_read(u32 screen, dmap_view* v) {
    int fail = dmap_read_values(screen, v);
    if (fail && (fail != dmap_fail || screen != dmap_fail_screen)) {
        dmap_fail = fail;
        dmap_fail_screen = screen;
        wwhd_log_hex("multiplayer: dungeon map not read, screen", screen);
        wwhd_log_int("multiplayer: dungeon map   reason (1 icon, 2 pane, 3 map, 4 cell, 5 scale, 6 values, 7 hidden, 8 parents, 9 places)", fail);
    }
    return !fail;
}

/* Link's own icon as a test of the prediction: when he has moved, the change of his pane place
 * against the linear part times the change of his map place (logged a few times per screen). */
static void dmap_self_check(const dmap_view* v) {
    if (!dmap_have_check) {
        dmap_check[0] = v->place_x; dmap_check[1] = v->place_y; dmap_check[2] = v->pane_x; dmap_check[3] = v->pane_y;
        dmap_have_check = 1;
        return;
    }
    f32 dx = v->place_x - dmap_check[0], dy = v->place_y - dmap_check[1];
    if (dx * dx + dy * dy < DMAP_CHECK_MOVE * DMAP_CHECK_MOVE) return;
    f32 px = v->jxx * dx + v->jxy * dy, py = v->jyx * dx + v->jyy * dy;
    f32 ax = v->pane_x - dmap_check[2], ay = v->pane_y - dmap_check[3];
    dmap_check[0] = v->place_x; dmap_check[1] = v->place_y; dmap_check[2] = v->pane_x; dmap_check[3] = v->pane_y;
    if (dmap_checks_logged >= 3) return;
    ++dmap_checks_logged;
    wwhd_log_float("multiplayer: dungeon map check: Link's pane moved x", (double)ax);
    wwhd_log_float("multiplayer: dungeon map check:   predicted x", (double)px);
    wwhd_log_float("multiplayer: dungeon map check:   moved y", (double)ay);
    wwhd_log_float("multiplayer: dungeon map check:   predicted y", (double)py);
}

WWHD_HOOK_RETURN(0x0265799C, dmap_markers, (u32 screen)) {
    dmap_ok = dmap_read(screen, &dmap_v);
    if (!dmap_ok) return;
    dmap_screen = screen;
    dmap_step = wwhd_logic_step();
    dmap_self_check(&dmap_v);
    dmap_players_update();
}

/* The map is up (it updated this step or just before). */
static int dmap_shown(void) {
    return dmap_ok && dmap_screen && wwhd_logic_step() - dmap_step <= CHART_SEEN_STEPS;
}

/* A world place on the floor shown? (the game's own test for its icons) */
static int dmap_on_floor(f32 y) {
    u8* play = wwhd_play_get();
    return play && wwhd_map_GetFloorNo((void*)(play + DMAP_PLAY_STAGE), y) == dmap_v.floor;
}

/* A world place (x, z) on the screen layout (1280 x 720, y down). */
static int dmap_place(f32 wx, f32 wz, f32* sx, f32* sy) {
    const dmap_view* v = &dmap_v;
    f32 dx = (wx - v->origin_x) - v->place_x, dy = (wz - v->origin_z) - v->place_y;
    f32 tx = v->pane_x + v->jxx * dx + v->jxy * dy;
    f32 ty = v->pane_y + v->jyx * dx + v->jyy * dy;
    *sx = CHART_LAYOUT_W * 0.5f + v->parent.ox + v->parent.kx * tx;
    *sy = CHART_LAYOUT_H * 0.5f - (v->parent.oy + v->parent.ky * ty);
    return chart_finite(*sx) && chart_finite(*sy);
}
