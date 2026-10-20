/* The launcher's Debug page "bots with a skin" (0.3.51): Link puppets wearing a skin of the Data
 * folder, in front of the local Link, to test many skins at once alone, connected or not.
 *
 * debug.cfg "skinbot=<id>" (a new request id each time, mod.c debug_step): one more bot wearing the
 * skin <id>; "skinbots=0": every such bot removed. At most SB_MAX (puppet.h has PUPPET_MAX slots,
 * shared with the other players and the push bot).
 *
 * Each bot is a puppet (puppet.h, needs the "puppet" option) in its own slot SB_UID(k): the bit
 * PUPPET_BOT_UID marks a bot for the rest of the mod (carry.h, pvp.h leave them out; puppet_clear_all
 * keeps them). Its skin is the slot's (puppet_set_look, as for a player's), noted as met
 * (skin_note_remote): a skin not loaded yet loads at the next scene load (skin.h skin_scene_load),
 * the bot showing Link until then. The bots stand on an arc SB_DISTANCE in front of the local Link,
 * facing it, placed again at each area change. */
#pragma once
#include "wwhd_guest.h"

#define SB_MAX 6
#define SB_UID(k) (PUPPET_BOT_UID | (0x100u + (u32)(k)))
#define SB_DISTANCE 220.0f
#define SB_SPREAD 0x1400          /* angle between two bots (about 28 degrees) */
#define SB_SEEN_STEPS 600         /* their skins noted as met again this often */

static int sb_used[SB_MAX];
static char sb_skin[SB_MAX][MP_SKIN_MAX + 1];
static mp_transform sb_place_of[SB_MAX];
static char sb_stage[STAGE_NAME_BYTES + 1];
static char sb_wanted[MP_SKIN_MAX + 1];  /* a request of debug.cfg, done at the next step of Link */
static int sb_clear_wanted;
static unsigned long long sb_seen_step;

static void sb_request(const char* id) {
    skin_copy_id(sb_wanted, id);
}

static void sb_request_clear(void) { sb_clear_wanted = 1; }

static int sb_count(void) {
    int n = 0;
    for (int k = 0; k != SB_MAX; ++k) n += sb_used[k];
    return n;
}

static void sb_place_all(const fopAc_ac_c* link) {
    int n = sb_count(), i = 0;
    for (int k = 0; k != SB_MAX; ++k) {
        if (!sb_used[k]) continue;
        s16 a = (s16)(link->shape_angle.y + (s16)((2 * i - (n - 1)) * SB_SPREAD / 2));
        f32 sn = as_sin(a), cs = as_sin((s16)(a + 0x4000));
        sb_place_of[k].pos[0] = link->current.pos.x + sn * SB_DISTANCE;
        sb_place_of[k].pos[1] = link->current.pos.y;
        sb_place_of[k].pos[2] = link->current.pos.z + cs * SB_DISTANCE;
        sb_place_of[k].rot[0] = sb_place_of[k].rot[2] = 0;
        sb_place_of[k].rot[1] = (s16)(a + 0x8000); /* facing the local Link */
        ++i;
    }
}

/* Every step of the local Link (after its execute). */
static void sb_step(void* link_actor) {
    const fopAc_ac_c* link = (const fopAc_ac_c*)link_actor;
    const u8* play = wwhd_play_get();
    if (!link || !play) return;
    if (sb_clear_wanted) {
        sb_clear_wanted = 0;
        for (int k = 0; k != SB_MAX; ++k)
            if (sb_used[k]) {
                puppet_remove(SB_UID(k));
                sb_used[k] = 0;
            }
        wwhd_log("multiplayer: debug: bots with a skin removed");
    }
    char stage[STAGE_NAME_BYTES + 1];
    map_stage(play, stage);
    if (sb_wanted[0]) {
        int k = 0;
        while (k != SB_MAX && sb_used[k]) ++k;
        if (k == SB_MAX) {
            wwhd_log_int("multiplayer: debug: no more bots with a skin, at most", SB_MAX);
        } else {
            sb_used[k] = 1;
            skin_copy_id(sb_skin[k], sb_wanted);
            skin_note_remote(sb_skin[k]);
            sb_place_all(link);
            for (int i = 0; i != STAGE_NAME_BYTES + 1; ++i) sb_stage[i] = stage[i];
            wwhd_log("multiplayer: debug: a bot with the skin (shown from the next area if not loaded yet):");
            wwhd_log(sb_skin[k]);
        }
        sb_wanted[0] = 0;
    }
    if (!sb_count()) return;
    if (!map_same_stage(stage, sb_stage)) { /* another area: in front of the local Link again */
        sb_place_all(link);
        for (int i = 0; i != STAGE_NAME_BYTES + 1; ++i) sb_stage[i] = stage[i];
    }
    unsigned long long now = wwhd_logic_step();
    int note = now - sb_seen_step >= SB_SEEN_STEPS;
    if (note) sb_seen_step = now;
    if (!puppet_enabled || puppet_disabled_by_error) return;
    for (int k = 0; k != SB_MAX; ++k) {
        if (!sb_used[k]) continue;
        if (note) skin_note_remote(sb_skin[k]); /* kept wanted at the scene loads */
        puppet_set_target(SB_UID(k), &sb_place_of[k]);
        puppet_slot* s = puppet_slot_for(SB_UID(k), 0);
        if (s && !skin_same_id(s->skin, sb_skin[k]))
            puppet_set_look(SB_UID(k), TUNIC_DEFAULT_PRIMARY, TUNIC_DEFAULT_SECONDARY, PUPPET_NO_PROC, PUPPET_ITEM_NONE, 0,
                            sb_skin[k]);
    }
    if (state != READY && pb_mode == PB_OFF) puppet_step(link); /* connected, mod.c steps the puppets */
}
