/* Which Link is which: the local player (player 0 of the play registry, SDK
 * WWHD_PLAY_PLAYER_OFFSET) and the remote Link puppets (puppet.h), recognized by their creation
 * parameters and process name (fpcNm_PLAYER_e = 0xA8 in HD, process name s16 at +8). */
#pragma once
#include "wwhd_guest.h"
#include "wwhd/actor.h"
#include "wwhd/data.h"

#define PUPPET_PARAMS 0xFFAA0040u /* start event 0xFF, start mode 0, no restart room (0x40) */
#define PUPPET_PROC_PLAYER 0xA8
#define ACTOR_PROC_NAME 0x08

static int puppet_is(const void* actor) {
    if (!actor) return 0;
    const fopAc_ac_c* a = (const fopAc_ac_c*)actor;
    return a->mParameters == PUPPET_PARAMS &&
           *(const volatile s16*)((const u8*)actor + ACTOR_PROC_NAME) == PUPPET_PROC_PLAYER;
}

static u32 puppet_local_player(void) {
    const u8* play = wwhd_play_get();
    return play ? *(const volatile u32*)(play + WWHD_PLAY_PLAYER_OFFSET) : 0;
}
