/* 0.3.12: PvP. 0.3.13: a mutator of the server (launcher's Server tab, ActorFilter "#pvp=1" in the
 * init and the last byte of SERVER_MUTATORS: session.pvp): every living player can lock on to and
 * hit the other living players.
 *
 * Lock-on: the attention (d_attention.cpp, verified) goes over every actor (fopAcIt_Executor, select_
 * attention 024EB7E0) and keeps those whose attention_info.flags (+0x39C) meet the player's mask
 * through loc_type_tbl, at the distance entry attention_info.distances[type] (+0x388; type 2 battle)
 * of dist_table (101D3CC0, read in the EU cking.rpx: entry 3, 375 units around, 500 up and down, the
 * enemies' kind), measured from attention_info.position (+0x390). A PvP player's puppet gets
 * fopAc_Attn_LOCKON_BATTLE_e (4), the battle distance 3 and its position 100 units above its feet;
 * else its flags are 0.
 *
 * Hits: the victim's game decides. The attacker's sword is already replayed in every game for the
 * scenery (sword_swing.h: its capsules, Link's l_at_cps_src, owner the attacker's puppet). Read in the
 * EU cking.rpx: l_at_cps_src (101CEDA8) is AT type 2 (the sword), damage 1, AtSPrm 0xB (Set | VsEnemy
 * | VsOther); Link's body l_cyl_src (101CECDC) is TG type 0xFF1DFEFF (it has 2), TgSPrm 5. When the
 * attacker and the local player both play PvP, copies of those capsules with AtSPrm Set | VsPlayer
 * (5: no enemy, pot or switch is hit) are given to the main collision space (dCcS::Set 0200E240,
 * play + 0x26A4, as puppet.h does for the light): the game itself hurts and pushes back the local
 * Link (its damage reaction, invincibility time, shield). */
#pragma once

#define PVP_ATTN_DISTANCES 0x388   /* fopAc_ac_c::attention_info.distances[] */
#define PVP_ATTN_POSITION 0x390
#define PVP_ATTN_FLAGS 0x39C
#define PVP_ATTN_TYPE_BATTLE 2
#define PVP_ATTN_LOCKON_BATTLE 4u
#define PVP_BATTLE_DISTANCE 3      /* dist_table entry 3 */
#define PVP_ATTN_HEIGHT 100.0f

/* A remote player the local one may fight: the server's PvP on, both alive. */
static int pvp_with(mp_u32 uid) {
    if (!session.pvp || dh_ghost || dh_dying || state != READY) return 0;
    const remote_player* r = find_remote(uid, 0);
    return r && r->has_look && !(r->look.death & (MP_DEATH_GHOST | MP_DEATH_DOWN));
}

/* sword_swing.h: the swings of that player may hit the local Link. */
static int pvp_hits_local(mp_u32 uid) { return pvp_with(uid); }

/* After a puppet's execute: lock-on on a PvP player, none on the others. */
static void pvp_puppet_after(void* actor) {
    int slot = puppet_slot_of_actor((u32)actor);
    u32 a = (u32)actor;
    const puppet_slot* s = slot >= 0 ? &puppet_slots[slot] : 0;
    int fight = s && !s->ghost && !(s->uid & (PUPPET_BODY_UID | PUPPET_BOT_UID)) && pvp_with(s->uid);
    if (!fight) {
        *(volatile u32*)(a + PVP_ATTN_FLAGS) = 0;
        return;
    }
    const fopAc_ac_c* p = (const fopAc_ac_c*)actor;
    *(volatile u8*)(a + PVP_ATTN_DISTANCES + PVP_ATTN_TYPE_BATTLE) = PVP_BATTLE_DISTANCE;
    volatile f32* pos = (volatile f32*)(a + PVP_ATTN_POSITION);
    pos[0] = p->current.pos.x;
    pos[1] = p->current.pos.y + PVP_ATTN_HEIGHT;
    pos[2] = p->current.pos.z;
    *(volatile u32*)(a + PVP_ATTN_FLAGS) = PVP_ATTN_LOCKON_BATTLE;
}
