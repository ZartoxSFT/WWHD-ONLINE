/* The sea chart of the MAP screen (hdSeaChartMenu_c: the GamePad, or the TV with the Pro
 * Controller): where its chart area is on the screen, so mod.c can draw the remote players on it,
 * and the green of the game's Link icon in the local tunic colours.
 *
 * Everything here is what the port's own sea chart code reads (runtime/src/mods/story_chart.cpp,
 * story_notebook_game.cpp, story_notebook.cpp, story_bag.cpp; canonical USA addresses):
 *  - updateProgressIcons(menu) 02678B74 runs every update of the menu; while the screen is busy
 *    (opening, zooming, closing) the menu calls hideMenuPanes(menu) 02678B00 instead;
 *    destroy(menu, flags) 02677B90.
 *  - The chart area is the menu's main pane (+0x70): 7 x 7 squares of 100000 world units, square D4's
 *    centre at the pane's position, one square = its size / 7, x east, z south (layout y up). The
 *    game places Link's marker the same way.
 *  - Library panes: parent +0x0C, children list +0x14 (sentinel; a child's link at +0), translate
 *    +0x1C, scale +0x34, size +0x3C, flags +0x44 (bit 0 visible), global alpha +0x46, name +0x80.
 *  - The menu's layout: menu +0x44 -> +4 -> +0xC is the root pane "L_Map_00"; the game's Link icon is
 *    its picture pane "P_MapLinkFace_00", whose material (+0xF4) has its texture at +0x28: a GX2
 *    texture (GX2Surface at +0, 64 x 64).
 *  - A pane's place is its translation in its parent's space scaled by the parent, the screen
 *    layout's origin in the middle, y up; the GamePad's layouts are authored for 1280 x 720 and
 *    shown on its 854 x 480 picture (story_notebook_bag.h place_icon, story_notebook.cpp).
 *  - hdInputManager (101F5088) +0x1D0: 0 when the menus go to the TV (Pro Controller).
 *  - The Start menu is open while SCENE-ingame_ui (101F8344)'s state machine is in one of the menu
 *    states 1049EC84 / 1049ECB0 / 1049ECDC (menu entry, menu and subwindow: items, collection, map,
 *    bag): its current state object (+0x18) has the descriptor at +8; with none, the pending
 *    descriptor is at +0x14 (docs/gamepad-display.md, runtime/src/gfx/gamepad_ui_state.cpp). The
 *    GamePad keeps its sea chart up during play: there the players are drawn whenever the chart
 *    updates (0.2.36; before, only while the menu was open). Deduced, not verified in game: the
 *    chart does not update while the GamePad shows another screen (items), or its panes are hidden.
 *  - The remote players wear the game's Link icon itself, decoded at run time from its texture
 *    (wwhd_hud_game_pixels: nothing of the game is in the package) before our recolouring, then
 *    recoloured per player like the tunic (mod.c).
 * The recolouring of the icon is deduced from the tunic's (tunic.h): its pixels are rewritten in place
 * from a pristine copy and the renderer re-uploads written texture memory. */
#pragma once
#include "wwhd_guest.h"
#include "tunic.h"

#define CHART_MAIN_PANE 0x70
#define CHART_LAYOUT 0x44
#define CHART_LAYOUT_LYT 0x04
#define CHART_LYT_ROOT 0x0C
#define PANE_PARENT 0x0C
#define PANE_CHILDREN 0x14
#define PANE_TRANSLATE 0x1C
#define PANE_SCALE 0x34
#define PANE_SIZE 0x3C
#define PANE_FLAGS 0x44
#define PANE_GLOBAL_ALPHA 0x46
#define PANE_NAME 0x80
#define PANE_VISIBLE 0x01
#define PICTURE_MATERIAL 0xF4
#define MATERIAL_TEXTURE 0x28
#define CHART_SQUARE 100000.0f
#define CHART_SQUARES 7.0f
#define CHART_SEA_HALF 350000.0f
#define CHART_LAYOUT_W 1280.0f
#define CHART_LAYOUT_H 720.0f
#define CHART_DRC_SCALE (854.0f / 1280.0f)
#define CHART_SEEN_STEPS 3          /* logic steps the menu may skip and still be on screen */
#define CHART_ICON_CHECK 30         /* menu updates between two checks of the icon texture */
#define INPUT_MANAGER 0x101F5088
#define INPUT_MODE 0x1D0
#define INGAME_UI 0x101F8344
#define UI_STATE_CURRENT 0x18
#define UI_STATE_PENDING 0x14
#define UI_STATE_DESCRIPTOR 0x08
#define CHART_FACE_MAX_BYTES (128u * 128u * 4u)

static u32 chart_menu;              /* the menu since it last updated, until destroyed */
static unsigned long long chart_step;
static int chart_busy;              /* hideMenuPanes ran: opening, zooming or closing */

/* the game's Link icon texture, recoloured in the local tunic colours */
static u32 chart_icon_menu;         /* menu whose icon was looked up */
static u32 chart_face;              /* P_MapLinkFace_00 */
static tunic_texture chart_icon;
static int chart_icon_ready;
static tunic_look chart_icon_applied;
static int chart_icon_clock;
/* the game's Link icon as RGBA8, decoded before our recolouring (remote players' icons) */
static u8* chart_face_rgba;
static u32 chart_face_w, chart_face_h;
static u32 chart_face_generation;   /* changes with each new decode */

static int chart_finite(f32 v) { return v == v && v < 1.0e6f && v > -1.0e6f; }

static int chart_name_is(u32 pane, const char* name) {
    const volatile char* n = (const volatile char*)(pane + PANE_NAME);
    for (u32 i = 0; i != 24; ++i) {
        if (n[i] != name[i]) return 0;
        if (!name[i]) return 1;
    }
    return 0;
}

/* A pane of the layout by name, depth first (story_notebook_game.cpp find_pane). */
static u32 chart_find_pane(u32 pane, const char* name, int depth) {
    if (!pane || (pane & 3) || depth > 24) return 0;
    if (chart_name_is(pane, name)) return pane;
    u32 sentinel = pane + PANE_CHILDREN, child = tunic_ld32(sentinel);
    for (u32 guard = 0; child && child != sentinel && guard < 256; ++guard, child = tunic_ld32(child)) {
        u32 found = chart_find_pane(child, name, depth + 1);
        if (found) return found;
    }
    return 0;
}

/* Where a pane's own space is on the screen layout: its origin (ox, oy) and scale (kx, ky), from
 * the chain of its ancestors and itself; 0 when a pane of the chain is hidden or being built. */
typedef struct chart_space {
    f32 ox, oy, kx, ky;
} chart_space;

static int chart_pane_space(u32 pane, chart_space* out) {
    u32 chain[32];
    u32 n = 0;
    for (u32 p = pane; p && n != 32; p = tunic_ld32(p + PANE_PARENT)) {
        if (!(*(const volatile u8*)(p + PANE_FLAGS) & PANE_VISIBLE)) return 0;
        chain[n++] = p;
    }
    if (!n) return 0;
    f32 x = 0.0f, y = 0.0f, sx = 1.0f, sy = 1.0f;
    while (n--) { /* from the root down */
        u32 p = chain[n];
        x += *(const volatile f32*)(p + PANE_TRANSLATE) * sx;
        y += *(const volatile f32*)(p + PANE_TRANSLATE + 4) * sy;
        sx *= *(const volatile f32*)(p + PANE_SCALE);
        sy *= *(const volatile f32*)(p + PANE_SCALE + 4);
    }
    if (!(sx > 0.01f && sy > 0.01f && sx < 100.0f && sy < 100.0f) || !chart_finite(x) || !chart_finite(y)) return 0;
    out->ox = x;
    out->oy = y;
    out->kx = sx;
    out->ky = sy;
    return 1;
}

/* The chart on the screen: a world place (x, z) maps to (ox + x * kx, oy + z * ky) in screen layout
 * units (1280 x 720, y down); icon: the game's Link icon size there; alpha: the chart's fade. */
typedef struct chart_view {
    f32 ox, oy, kx, ky;
    f32 icon;
    f32 alpha;
} chart_view;

static int chart_menus_to_tv(void) {
    u32 input = WWHD_GAME_DATA(INPUT_MANAGER, u32);
    return input && !(input & 3) && tunic_ld32(input + INPUT_MODE) == 0;
}

/* The Start menu (SCENE-ingame_ui in a menu state) is open. */
static int chart_menu_open(void) {
    u32 scene = WWHD_GAME_DATA(INGAME_UI, u32);
    if (!scene || (scene & 3)) return 0;
    u32 current = tunic_ld32(scene + UI_STATE_CURRENT);
    u32 descriptor = current && !(current & 3) ? tunic_ld32(current + UI_STATE_DESCRIPTOR) : tunic_ld32(scene + UI_STATE_PENDING);
    return descriptor && (descriptor == (u32)&WWHD_GAME_DATA(0x1049EC84, u8) ||
                          descriptor == (u32)&WWHD_GAME_DATA(0x1049ECB0, u8) ||
                          descriptor == (u32)&WWHD_GAME_DATA(0x1049ECDC, u8));
}

/* in_menu: only while the Start menu is open (the TV's chart). The GamePad's chart is the same
 * screen and stays up during play: there it counts as on screen whenever the game updated it this
 * step and its panes are shown (in_menu 0). */
/* ---- any screen with the sea chart's grid ----
 * hdSeaChartGrid_c::refreshChartMarks (026834A4, every update of a chart grid: runtime/src/mods/
 * story_chart.cpp) gives the grid; its chart area is the pane at +0xC8 (the 7 x 7 squares, square
 * D4's centre at its position, a square = its size / 7, y up: updateChartMarkLayout). The MAP
 * screen has one; the Ballad of Gales' warp map (layout WarpMap_00, with Map_00, MapAreaNow_00 and
 * BtnMapBalloon_00 like the MAP screen) is drawn on whatever grid updates while the MAP screen is
 * not on screen (deduced from its layout names, not verified in game: the first grid of each kind
 * is logged). A return hook: story_chart.cpp replaces nothing there either. */
#define GRID_CHART_AREA 0xC8
static u32 chart_grid;
static unsigned long long chart_grid_step;
static u32 chart_grid_logged[4];

WWHD_HOOK_RETURN(0x026834A4, chart_grid_refresh, (u32 grid)) {
    chart_grid = grid;
    chart_grid_step = wwhd_logic_step();
    for (int i = 0; i != 4; ++i) {
        if (chart_grid_logged[i] == grid) break;
        if (!chart_grid_logged[i]) {
            chart_grid_logged[i] = grid;
            wwhd_log_hex("multiplayer: sea chart grid", grid);
            u32 area = tunic_ld32(grid + GRID_CHART_AREA);
            wwhd_log_int("multiplayer: sea chart grid   is the MAP screen's", chart_menu && area && tunic_ld32(chart_menu + CHART_MAIN_PANE) == area);
            wwhd_log_int("multiplayer: sea chart grid   Start menu open", chart_menu_open());
            if (area && !(area & 3)) wwhd_log((const char*)(area + PANE_NAME));
            break;
        }
    }
}

/* in_menu (the TV): the MAP screen's own grid (its chart area is the menu's main pane) only while
 * the Start menu is open; with the Pro Controller the GamePad's screens keep updating during play
 * (0.2.38 drew them on the TV then). Another grid (the warp map) whenever it updates. */
static int chart_grid_is_menu(u32 area) {
    return chart_menu && tunic_ld32(chart_menu + CHART_MAIN_PANE) == area;
}

static int chart_grid_on_screen(chart_view* v, int in_menu) {
    if (!chart_grid || wwhd_logic_step() - chart_grid_step > CHART_SEEN_STEPS) return 0;
    u32 area = tunic_ld32(chart_grid + GRID_CHART_AREA);
    if (!area || (area & 3)) return 0;
    if (in_menu && !chart_menu_open() && chart_grid_is_menu(area)) return 0;
    chart_space s;
    if (!chart_pane_space(area, &s)) return 0;
    f32 square_x = *(const volatile f32*)(area + PANE_SIZE) / CHART_SQUARES;
    f32 square_y = *(const volatile f32*)(area + PANE_SIZE + 4) / CHART_SQUARES;
    if (!(square_x > 0.0f && square_y > 0.0f) || !chart_finite(square_x) || !chart_finite(square_y)) return 0;
    v->ox = CHART_LAYOUT_W * 0.5f + s.ox;
    v->oy = CHART_LAYOUT_H * 0.5f - s.oy;
    v->kx = square_x / CHART_SQUARE * s.kx;
    v->ky = square_y / CHART_SQUARE * s.ky;
    v->alpha = 1.0f;
    v->icon = square_x * s.kx * 0.35f;
    return 1;
}

static int chart_on_screen(chart_view* v, int in_menu) {
    if (!chart_menu || chart_busy || (in_menu && !chart_menu_open())) return 0;
    if (wwhd_logic_step() - chart_step > CHART_SEEN_STEPS) return 0;
    u32 main = tunic_ld32(chart_menu + CHART_MAIN_PANE);
    if (!main || (main & 3)) return 0;
    chart_space s;
    if (!chart_pane_space(main, &s)) return 0;
    u32 alpha = *(const volatile u8*)(main + PANE_GLOBAL_ALPHA);
    if (!alpha) return 0;
    f32 square_x = *(const volatile f32*)(main + PANE_SIZE) / CHART_SQUARES;
    f32 square_y = *(const volatile f32*)(main + PANE_SIZE + 4) / CHART_SQUARES;
    if (!(square_x > 0.0f && square_y > 0.0f) || !chart_finite(square_x) || !chart_finite(square_y)) return 0;
    /* square D4's centre is the main pane's origin; its own scale is in s */
    v->ox = CHART_LAYOUT_W * 0.5f + s.ox;
    v->oy = CHART_LAYOUT_H * 0.5f - s.oy;
    v->kx = square_x / CHART_SQUARE * s.kx;
    v->ky = square_y / CHART_SQUARE * s.ky; /* z south = layout down = screen down */
    v->alpha = (f32)alpha / 255.0f;
    v->icon = 0.0f;
    chart_space f;
    if (chart_face && chart_pane_space(chart_face, &f)) {
        f32 w = *(const volatile f32*)(chart_face + PANE_SIZE) * f.kx;
        if (w > 4.0f && w < 200.0f) v->icon = w;
    }
    if (!(v->icon > 0.0f)) v->icon = square_x * s.kx * 0.35f;
    return 1;
}

/* ---- the game's Link icon in the local tunic colours ---- */

/* The icon's pixels as the game has them (before chart_icon_write), for the remote players. */
static void chart_face_decode(u32 texture) {
    u32 size = wwhd_hud_game_pixels(texture, 0, 0);
    u32 w = size >> 16, h = size & 0xFFFF;
    if (!w || !h || w * h * 4 > CHART_FACE_MAX_BYTES) {
        wwhd_log_hex("multiplayer: sea chart Link icon not decoded, size", size);
        return;
    }
    if (!chart_face_rgba) chart_face_rgba = (u8*)wwhd_malloc(CHART_FACE_MAX_BYTES);
    if (!chart_face_rgba || wwhd_hud_game_pixels(texture, chart_face_rgba, CHART_FACE_MAX_BYTES) != size) {
        chart_face_w = chart_face_h = 0;
        return;
    }
    chart_face_w = w;
    chart_face_h = h;
    ++chart_face_generation;
}

/* The panes beside the icon, logged once (the game's direction arrow is one of them). */
static void chart_log_icon_group(void) {
    u32 parent = tunic_ld32(chart_face + PANE_PARENT);
    if (!parent || (parent & 3)) return;
    wwhd_log("multiplayer: sea chart Link icon group:");
    wwhd_log((const char*)(parent + PANE_NAME));
    u32 sentinel = parent + PANE_CHILDREN, child = tunic_ld32(sentinel);
    for (u32 guard = 0; child && child != sentinel && guard < 32; ++guard, child = tunic_ld32(child))
        wwhd_log((const char*)(child + PANE_NAME));
}

static void chart_icon_write(const tunic_look* look) {
    tunic_texture* t = &chart_icon;
    memcpy((void*)t->image, t->pristine, t->image_size);
    if (t->mip_size) memcpy((void*)t->mip, t->pristine + t->image_size, t->mip_size);
    if (look->custom) {
        tc_palette p = tc_palette_make(tc_565_to_888(look->primary), tc_565_to_888(look->secondary));
        tc_recolor_surface(&p, t->format, (u8*)t->image, t->image_size);
        if (t->mip_size) tc_recolor_surface(&p, t->format, (u8*)t->mip, t->mip_size);
    }
    t->live_hash = tunic_hash(t->image, t->image_size);
    chart_icon_applied = *look;
}

/* The icon's texture of this menu: copied once (pristine), recoloured when the local colours change
 * or the game reloaded it. */
static void chart_icon_refresh(u32 menu) {
    if (menu != chart_icon_menu) {
        chart_icon_menu = menu;
        chart_icon_clock = 0;
        u32 layout = tunic_ld32(menu + CHART_LAYOUT);
        u32 lyt = layout && !(layout & 3) ? tunic_ld32(layout + CHART_LAYOUT_LYT) : 0;
        u32 root = lyt && !(lyt & 3) ? tunic_ld32(lyt + CHART_LYT_ROOT) : 0;
        chart_face = root && !(root & 3) && chart_name_is(root, "L_Map_00") ? chart_find_pane(root, "P_MapLinkFace_00", 0) : 0;
        if (!chart_face) wwhd_log("multiplayer: sea chart Link icon (P_MapLinkFace_00) not found");
        else chart_log_icon_group();
    }
    if (!chart_face || --chart_icon_clock > 0) return;
    chart_icon_clock = CHART_ICON_CHECK;
    u32 material = tunic_ld32(chart_face + PICTURE_MATERIAL);
    u32 texture = material && !(material & 3) ? tunic_ld32(material + MATERIAL_TEXTURE) : 0;
    if (!texture || (texture & 3)) return;
    tunic_texture now;
    /* tunic_read_surface reads an HD texture object (surface at +4); this one starts with it */
    int supported = tunic_read_surface(texture - HD_TEXTURE_SURFACE, &now);
    if (!chart_icon_ready || !tunic_same_surface(&now, &chart_icon)) {
        if (chart_icon_ready) wwhd_free(chart_icon.pristine);
        chart_icon_ready = 0;
        static u32 logged;
        if (logged != texture) {
            logged = texture;
            wwhd_log_hex("multiplayer: sea chart Link icon texture", texture);
            wwhd_log_hex("multiplayer: sea chart   format", now.format);
            wwhd_log_int("multiplayer: sea chart   width", (int)tunic_ld32(texture + 4));
            wwhd_log_int("multiplayer: sea chart   recolourable format", supported);
        }
        if (!supported || now.image_size + now.mip_size > (256u << 10)) return;
        now.pristine = (u8*)wwhd_malloc(now.image_size + now.mip_size);
        if (!now.pristine) return;
        memcpy(now.pristine, (const void*)now.image, now.image_size);
        if (now.mip_size) memcpy(now.pristine + now.image_size, (const void*)now.mip, now.mip_size);
        chart_face_decode(texture); /* still the game's colours */
        chart_icon = now;
        chart_icon_ready = 1;
        chart_icon_write(&tunic_local_wanted);
        return;
    }
    if (tunic_hash(chart_icon.image, chart_icon.image_size) != chart_icon.live_hash) { /* reloaded */
        chart_face_decode(texture);
        memcpy(chart_icon.pristine, (const void*)chart_icon.image, chart_icon.image_size);
        if (chart_icon.mip_size) memcpy(chart_icon.pristine + chart_icon.image_size, (const void*)chart_icon.mip, chart_icon.mip_size);
        chart_icon_write(&tunic_local_wanted);
        return;
    }
    if (!tunic_same_look(&chart_icon_applied, &tunic_local_wanted)) chart_icon_write(&tunic_local_wanted);
}

/* ---- the menu's updates ---- */

static void multiplayer_pump(void); /* mod.c: reads the network while the game is paused */

WWHD_HOOK(0x02678B74, chart_progress_icons, (u32 menu)) {
    chart_menu = menu;
    chart_step = wwhd_logic_step();
    chart_busy = 0;
    chart_icon_refresh(menu);
    if (chart_menu_open()) multiplayer_pump();
}

WWHD_HOOK(0x02678B00, chart_hide_panes, (u32 menu)) {
    chart_menu = menu;
    chart_step = wwhd_logic_step();
    chart_busy = 1;
}

WWHD_HOOK(0x02677B90, chart_destroy, (u32 menu, s32 flags)) {
    (void)flags;
    if (menu == chart_menu) chart_menu = 0;
    if (menu == chart_icon_menu) {
        chart_icon_menu = 0;
        chart_face = 0;
    }
}
