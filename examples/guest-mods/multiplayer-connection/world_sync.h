/* The world's clock and weather from the server (SERVER_WORLD, ActorFilter.java "world", set from the
 * launcher's Server tab).
 *
 * Time (decomp d_kankyo_4.cpp setDaytime 0255B300): the environment takes the time of day from the
 * save every frame (dSv_player_status_b_c::mTime, f32 degrees, dSv_save_c + 0x24; mDate, u16,
 * + 0x28) and writes it back advanced by mTimeAdv (0.02 degree a frame) when time passes there
 * (frameTimePass 101EBFD8 not 0); in a stage where it does not, the clock stops at 165 degrees by
 * day and 345 by night. Day is 90 to 270 degrees (dKy_daynight_check 02556D14: 6 h to 18 h).
 * A time the launcher just set (MP_WORLD_EXPLICIT) is written at once, wherever the player is; the
 * server's regular reminders only where time passes and when this clock is more than an hour off.
 *
 * Day and night actors: many actors decide in their creation whether they exist by
 * dKy_daynight_check (the people of Windfall, d_a_npc_*.cpp): the game makes them only when a stage
 * or room loads. When the launcher moves the clock from day to night (or back), the actors already
 * there stay and the stage's actors whose creation was refused are asked again (actor_sync.h
 * as_roster_retry): the actors of both times, an OR, until the next load.
 *
 * Weather (d_kankyo_4.cpp dKy_Execute 0255D128, on the sea only, once the Wind Waker is held): the
 * game draws its weather with dice (dScnKy_env_light_c at 10475A68: mDiceWeatherMode +0x1093,
 * 0 sunny .. 5 heavy thunder; mDiceWeatherState +0x1094, 2 executing; mDiceWeatherTime +0xFC4, when
 * the next roll comes) and then makes it (rain, thunder, colours). A weather the server chose is
 * that mode, kept executing with its next roll always ahead; the game's own again (weather 0) resets
 * the dice. Elsewhere the stage's weather stays.
 * The dice only run where time passes and once the Wind Waker is held (dKy_Execute:
 * frameTimePass 101EBFD8 != 0 && isItem(save + 0x71 = dSv_player_get_item_c, 2, 0)); otherwise the
 * game clears the rain every frame, so a forced weather did nothing on an island whose clock stops
 * (0.2.45). While a weather is forced, wd_ky_execute makes both true for the time of dKy_Execute
 * only and puts them back: the game's own weather code then makes the server's weather. */
#pragma once
#include "wwhd_guest.h"
#include "mp_protocol.h"

#define WD_SAVE_TIME (0x20 + 0x24)
#define WD_SAVE_DATE (0x20 + 0x28) /* not written: see wd_step */
#define WD_TIME_PASS 0x101EBFD8
#define WD_ENV 0x10475A68
#define WD_DICE_TIME 0xFC4
#define WD_DICE_CHANGE 0xFC0
#define WD_DICE_MODE 0x1093
#define WD_DICE_STATE 0x1094
#define WD_DICE_EXEC 2
#define WD_DRIFT 15.0f      /* degrees (an hour) before a reminder resets the clock */

static mp_world wd_world;
static int wd_have, wd_apply_time, wd_weather_forced;

static int wd_is_day(f32 t) { return t >= 90.0f && t < 270.0f; }

static void wd_received(const mp_u8* body, mp_u32 length) {
    mp_world w;
    if (!mp_read_world(body, length, &w)) {
        wwhd_log("multiplayer: malformed world packet skipped");
        return;
    }
    wd_world = w;
    wd_have = 1;
    if (w.flags & MP_WORLD_TIME) wd_apply_time = (w.flags & MP_WORLD_EXPLICIT) ? 2 : 1;
    if (w.flags & MP_WORLD_EXPLICIT) wwhd_log_float("multiplayer: world time from the server (degrees)", (double)w.time);
}

static void wd_roster_retry(void); /* actor_sync.h as_roster_retry, by mod.c */

static f32 wd_abs(f32 v) { return v < 0 ? -v : v; }

#define WD_GET_ITEM 0x71   /* dSv_player_get_item_c from the save pointer (d_kankyo_4.h FrameSave::mItems) */
#define WD_WIND_WAKER 2    /* isItem(2, 0): byte 2, bit 0 */

WWHD_GAME_ORIGINAL(0x0255D128, s32, wd_ky_execute_original, (void));

/* dKy_Execute (no argument, BOOL): the dice run while the server forces a weather. */
WWHD_REPLACE(0x0255D128, s32, wd_ky_execute, (void)) {
    u32 save = WWHD_GAME_DATA(0x101F84DC, u32);
    if (!wd_weather_forced || !save) return wd_ky_execute_original();
    volatile u8* pass = &WWHD_GAME_DATA(WD_TIME_PASS, u8);
    volatile u8* wind = (volatile u8*)(save + WD_GET_ITEM + WD_WIND_WAKER);
    u8 was_pass = *pass, was_wind = *wind;
    *pass = 1;
    *wind = (u8)(was_wind | 1);
    s32 result = wd_ky_execute_original();
    *pass = was_pass;
    *wind = was_wind;
    return result;
}

/* Every step of the local Link. */
static void wd_step(const u8* play) {
    u32 save = WWHD_GAME_DATA(0x101F84DC, u32);
    if (!wd_have || !save || !play) return;
    volatile f32* time = (volatile f32*)(save + WD_SAVE_TIME);
    if (wd_apply_time && !play[WWHD_PLAY_EVENT_RUNNING_OFFSET]) {
        f32 now = *time, want = wd_world.time;
        f32 off = wd_abs(now - want);
        if (off > 180.0f) off = 360.0f - off;
        int passes = WWHD_GAME_DATA(WD_TIME_PASS, u8) != 0;
        if (wd_apply_time == 2 || (passes && off > WD_DRIFT)) {
            int flipped = wd_is_day(now) != wd_is_day(want);
            *time = want; /* mDate stays the player's: the moon's phase (the Ghost Ship) is theirs */
            if (flipped) wd_roster_retry();
            if (wd_apply_time == 2) wwhd_log(flipped ? "multiplayer: world time set, day and night actors together"
                                                    : "multiplayer: world time set");
        }
        wd_apply_time = 0;
    }
    u8* env = (u8*)&WWHD_GAME_DATA(WD_ENV, u8);
    const u8* stage = play + WWHD_PLAY_START_STAGE_NAME_OFFSET;
    int sea = stage[0] == 's' && stage[1] == 'e' && stage[2] == 'a' && stage[3] == 0;
    static const u8 modes[MP_WEATHER_MAX + 1] = {0, 0, 1, 2, 3, 5};
    if ((wd_world.flags & MP_WORLD_WEATHER) && wd_world.weather && sea) {
        env[WD_DICE_MODE] = modes[wd_world.weather];
        env[WD_DICE_STATE] = WD_DICE_EXEC;
        f32 ahead = *time + 90.0f; /* the next roll never comes */
        if (ahead >= 360.0f) ahead -= 360.0f;
        *(volatile f32*)(env + WD_DICE_TIME) = ahead;
        wd_weather_forced = 1;
    } else if (wd_weather_forced) {
        env[WD_DICE_STATE] = 0; /* DICE_STATE_RESET_e: the game rolls again */
        wd_weather_forced = 0;
    }
}
