/* The game's actors take the remote Link puppets for the player (option "actors_see_puppets").
 *
 * Actors find the player through player 0 of the play registry (play + 0x5B2C,
 * dComIfGp_getPlayer(0), mostly inlined: it cannot be hooked as a function). While an enemy
 * (fopAc_ac_c::group 2, fopAc_ENEMY_e) or a resident (4, fopAc_NPC_e) runs its execute
 * (fopAc_Execute 025D475C, the process method of every actor, decomp f_op_actor.cpp), player 0 is
 * the nearest Link to it, the local one or a puppet (puppet.h), and gets its value back after. So an
 * enemy near a puppet notices it, turns to it and comes for it, a resident looks at it, exactly as
 * with the player. Only what the actor does in its own execute sees the puppet. For a synchronised
 * actor (actor_sync.h) whose owner is another player, its execute runs here too, between the
 * network values.
 *
 * Known effects, documented: attacks on a puppet hurt nobody (its colliders are not registered,
 * the puppet does not run the player's logic); an actor that keeps the player's pointer keeps the
 * puppet's (it goes when the puppet is deleted); an actor near a puppet compares hits with the
 * puppet, not the local Link, during that execute; an event an actor starts for the player starts
 * on the local game. The player's group (1), objects (0) and the environment (3) are untouched:
 * Link's own items, the ship, doors, chests and the camera keep following the local Link. */
#pragma once
#include "wwhd_guest.h"
#include "wwhd/bindings.h"
#include "wwhd/actor.h"
#include "wwhd/data.h"
#include "link_access.h"

#define SENSE_GROUP_ENEMY 2
#define SENSE_GROUP_NPC 4

static int sense_enabled = 1;
static u32 sense_swaps; /* executes that saw a puppet, logged once */

WWHD_GAME_ORIGINAL(0x025D475C, s32, sense_execute_original, (void* actor));

static f32 sense_distance2(const fopAc_ac_c* a, const fopAc_ac_c* b) {
    f32 dx = a->current.pos.x - b->current.pos.x;
    f32 dy = a->current.pos.y - b->current.pos.y;
    f32 dz = a->current.pos.z - b->current.pos.z;
    return dx * dx + dy * dy + dz * dz;
}

/* The puppet nearer to the actor than the local Link, 0 when the local Link is the nearest. The
 * dead are nobody's prey (death.h): bodies and ghosts are left out, and a local ghost is as far as
 * can be, so the actors go for a living player when there is one. */
static u32 sense_nearest_puppet(const fopAc_ac_c* actor, u32 local) {
    if (!puppet_enabled || puppet_disabled_by_error) return 0;
    f32 best = puppet_local_ghost ? 3.0e38f : sense_distance2(actor, (const fopAc_ac_c*)local);
    u32 found = 0;
    for (int i = 0; i != PUPPET_MAX; ++i) {
        u32 p = puppet_slots[i].used ? puppet_slots[i].actor : 0;
        if (!p || !puppet_is((const void*)p)) continue;
        if (puppet_slots[i].ghost || (puppet_slots[i].uid & PUPPET_BODY_UID)) continue;
        const daPy_lk_c* l = (const daPy_lk_c*)p;
        if (!l->mpCLModel) continue; /* not built yet */
        f32 d = sense_distance2(actor, (const fopAc_ac_c*)p);
        if (d < best) {
            best = d;
            found = p;
        }
    }
    return found;
}

/* Runs an actor's execute (fopAc_Execute, replaced in actor_sync.h, which calls this). */
static s32 sense_execute(void* actor) {
    if (!sense_enabled || !actor) return sense_execute_original(actor);
    u8 group = ((const fopAc_ac_c*)actor)->group;
    if (group != SENSE_GROUP_ENEMY && group != SENSE_GROUP_NPC) return sense_execute_original(actor);
    u8* play = wwhd_play_get();
    if (!play) return sense_execute_original(actor);
    volatile u32* player0 = (volatile u32*)(play + WWHD_PLAY_PLAYER_OFFSET);
    u32 local = *player0;
    if (!local || puppet_is((const void*)local)) return sense_execute_original(actor);
    u32 puppet = sense_nearest_puppet((const fopAc_ac_c*)actor, local);
    if (!puppet) return sense_execute_original(actor);
    *player0 = puppet;
    s32 result = sense_execute_original(actor);
    if (*player0 == puppet) *player0 = local; /* else the game changed it meanwhile: its value */
    if (!sense_swaps++) wwhd_log("multiplayer: an actor took a remote Link puppet for the player");
    return result;
}
