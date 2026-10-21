/* 0.3.62: a path point outside its path no longer stops the game.
 *
 * Crashes of 2026-10-21 (friends, 16:55 to 17:06, five times): "PROGRAM HALT d_path.cpp line 31,
 * 0 <= pnt_index && pnt_index < path->m_num", each time just after the mod's teleport to another
 * player in the Forsaken Fortress (MajyuE, majroom), by players early in the game. Host stack (EU
 * addresses, USA here): fopAc_Create 025D4C70 -> daObjMovebox Mthd_Create 0237A468 ->
 * Act_c::path_init 02377078 -> dPath_GetPnt 025AAEB8 -> assert.
 *
 * Decomp (d_a_obj_movebox.cpp path_init): a push block on a path takes its point from two saved
 * switches (point 0-3: switch 1 adds 1, switch 2 adds 2). The switches come from the shared
 * progression of the other players (save_sync.h), which can name a point the path of this game's
 * layer does not have.
 *
 * dPath_GetPnt (025AAEB8, d_path.cpp; generated code: two arguments, r3 the path, r4 the index;
 * m_num the u16 at +0, the points at +8): an index outside [0, m_num) is brought back to the nearest
 * point (the block stands at the path's end) instead of the assert. A null path or a path without
 * points goes to the original, as before. */
#pragma once
#include "wwhd_guest.h"

#define PG_PATH_NUM 0x00 /* u16 m_num */

WWHD_GAME_ORIGINAL(0x025AAEB8, u32, pg_get_pnt_original, (u32 path, s32 index));

WWHD_REPLACE(0x025AAEB8, u32, pg_get_pnt, (u32 path, s32 index)) {
    if (path) {
        s32 num = (s32)*(const volatile u16*)(path + PG_PATH_NUM);
        if (num > 0 && (index < 0 || index >= num)) {
            static u32 logged;
            if ((logged++ & 31) == 0) wwhd_log_int("multiplayer: path point outside its path, brought back (index)", (int)index);
            index = index < 0 ? 0 : num - 1;
        }
    }
    return pg_get_pnt_original(path, index);
}
