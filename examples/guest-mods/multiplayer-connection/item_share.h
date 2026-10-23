/* 0.3.56: an item held up for everyone to see, rupees shared, the pigs' anger.
 *
 * Transport: the cutscene packet (event_sync.h, CLIENT_PLAYER_EVENT / SERVER_PLAYER_EVENT, which the
 * server relays to every other client), with names the game's events never have, starting with '#'
 * (event_sync.h ev_received hands them here; no server change):
 *  - "#GET_xx": the sender holds up the item xx (hex). daPy_lk_c::dProcGetItem_init (024216C8,
 *    decomp d_a_player_main_05.cpp; one argument) starts the hold-up (proc DEMO_GET_ITEM 0xAE) for a
 *    chest, a gift or a heart container and keeps the item number in mProcVar2 (+0x691A, -1 for
 *    none; dComIfGp_event_getGtItm for 0x100). The local Link's step sends it once (0.3.58). The
 *    others draw the item's model (0.3.60) or its icon (save_sync.h sv_icon: itemForSlot 0262F880 then
 *    wwhd_hud_game_icon) over that player's name for SHARE_ITEM_STEPS (mod.c draw_markers). A rupee
 *    (d_item_data.h: 0x01 green 1, 0x02 blue 5, 0x03 yellow 10, 0x04 red 20, 0x05 purple 50,
 *    0x06 orange 100, 0x0F silver 200; d_item.cpp item_func_*_rupee) is given to every other
 *    connected player, wherever they are, through the game's rupee counter (PLAY_ITEM_RUPEE_COUNT,
 *    as the teleport's price). Rupees are each player's own in the save sync (save_sync.h
 *    sv_local_ranges): nothing counts twice.
 *  - "#END" (0.3.61): the sender's hold-up ended (its proc left DEMO_GET_ITEM, the hand comes down):
 *    the item over that player goes. Shown SHARE_SHOW_DELAY after "#GET" (the arm up first).
 *  - "#PIG_1" / "#PIG_0": the pigs are angry or calm again. Their anger is one flag for all the
 *    pigs, l_kbStatics.ALL_ANGER (data 101B7FA0, decomp d_a_kb.cpp; d_a_kb_exec.cpp sets it when a
 *    pig's health runs out and every pig not hit then attacks, attack_move clears it). Each change
 *    of the local flag is sent; a received one, same stage and room (Outset), sets it, and is not
 *    sent back. */
#pragma once
#include "wwhd_guest.h"

#define SHARE_ITEM_STEPS 900           /* 30 s at most: ended by "#END" when the hand comes down (0.3.61) */
#define SHARE_SHOW_DELAY 15            /* 0.5 s after the hold-up starts, as the game's item shows (0.3.61) */
#define SHARE_PROC_GET_ITEM 0xAE       /* daPyProc_DEMO_GET_ITEM_e */
#define SHARE_PIG_ANGER 0x101B7FA0     /* l_kbStatics.ALL_ANGER (u8) */

/* 0.3.60: the item's own model over the player, turning, as the game's hold-up shows it: the game's
 * cutscene item daDitem_c (process fpcNm_Demo_Item_e 0x101, decomp d_a_demo_item.cpp), made with
 * fopAcM_create (025D5834) and the parameters of fopAcM_createDemoItem (025D7D40,
 * f_op_actor_mng_3.cpp): the item, bit 0x7F00 (none), its argument flags << 16 (CreateInit keeps
 * them in mArgFlag +0x76D). FLAG_UNK01: its Delete gives no item (it calls execItemGet otherwise);
 * FLAG_UNK08: set_pos puts it at home.pos + mOffsetPos (+0x760, 0 here) instead of over player 0,
 * and set_mtx stands it upright, turning (fopAcM_addAngleY 0x111 each step). It shows only with
 * DRAW_VISIBLE in mDrawFlags (+0x74F, daItemBase_c, d_a_itembase_static.cpp: CreateInit hides it)
 * and deletes itself at its next execute with DRAW_DEAD (chkDead). share_step keeps its home over
 * the player's puppet, shows it, and ends it at "#END" (0.3.61), after SHARE_ITEM_STEPS or when the
 * puppet is gone.
 * Verified in the HD code: set_pos 021236A8 reads +0x76D (& 8) and +0x760; daItemBase_c::show
 * 021842C8 and dead 0218432C set +0x74F bits 1 and 2. */
#define SHARE_PROC_DEMO_ITEM 0x101
#define SHARE_DITEM_FLAGS 0x09u          /* FLAG_UNK01 | FLAG_UNK08 */
#define SHARE_DITEM_DRAW_FLAGS 0x74F
#define SHARE_DITEM_VISIBLE 0x01u
#define SHARE_DITEM_DEAD 0x02u
#define SHARE_DITEM_LIFT 130.0f          /* over the feet (d_a_demo_item.cpp offset_tbl[0].y) */
static u32 share_ditem_proc[MAX_REMOTES];
static int share_ditem_used[MAX_REMOTES];
static cXyz share_ditem_pos;
static csXyz share_ditem_angle;

static u8 share_item[MAX_REMOTES];
static u8 share_wanted[MAX_REMOTES];   /* the item whose model is still to be made (0.3.61) */
static unsigned long long share_show_at[MAX_REMOTES];
static int share_held_sent;            /* "#GET" sent for the hold-up running here */
static s32 share_proc_before = -1;     /* the local Link's proc before dProcGetItem_init */
static unsigned long long share_until[MAX_REMOTES];
static int share_pig_last = -1;        /* the flag as last seen or set here */

#define SHARE_SAVE 0x101F84DC          /* the save (dComIfGs_info() = *this + 0x20) */
#define SHARE_SAVE_ITEMS (0x20 + 0x3C) /* dSv_player_item_c mItems[21] (decomp d_save.h) */
#define SHARE_SLOT_BOW 0xC             /* dInvSlot_BOW_e (d_item.h) */
#define SHARE_SLOT_BOMB 0xD            /* dInvSlot_BOMB_e */
static int share_receiving;            /* an item another player shared is being given here */

/* 0x0B-0x0E bombs, 0x10-0x12 arrows (d_item_data.h): 1 bombs, 2 arrows, 0 else. */
static int share_ammo(u32 item) {
    if (item >= 0x0B && item <= 0x0E) return 1;
    if (item >= 0x10 && item <= 0x12) return 2;
    return 0;
}

static int share_owns_slot(u32 slot) {
    u32 save = WWHD_GAME_DATA(SHARE_SAVE, u32);
    return save && *(const volatile u8*)(save + SHARE_SAVE_ITEMS + slot) != 0xFF;
}

static u32 share_rupees(u32 item) {
    switch (item) {
    case 0x01: return 1;
    case 0x02: return 5;
    case 0x03: return 10;
    case 0x04: return 20;
    case 0x05: return 50;
    case 0x06: return 100;
    case 0x0F: return 200;
    default: return 0;
    }
}

static void share_send(const char* name) {
    const u8* play = wwhd_play_get();
    const fopAc_ac_c* link = dh_link ? (const fopAc_ac_c*)dh_link : 0;
    if (!play || !link || state != READY || local_uid == MP_NO_ID || session.client_player_event == MP_NO_ID) return;
    mp_event e;
    e.sender = local_uid;
    map_stage(play, e.stage);
    e.room = link->current.roomNo;
    e.has_place = 0;
    int n = 0;
    for (; name[n] && n != MP_EVENT_NAME_MAX; ++n) e.name[n] = name[n];
    e.name[n] = 0;
    mp_writer body = mp_writer_make(payload, sizeof(payload));
    mp_write_event(&body, &e);
    send_payload(session.client_player_event, &body);
}

static int share_hex(char c) {
    return c >= '0' && c <= '9' ? c - '0' : c >= 'A' && c <= 'F' ? c - 'A' + 10 : c >= 'a' && c <= 'f' ? c - 'a' + 10 : -1;
}

/* An item held up by the local Link: sent once, when its proc becomes DEMO_GET_ITEM (share_step).
 * 0.3.56 sent from a return hook on dProcGetItem_init, called again while the hold-up runs: a chest's
 * 100 rupees reached the other player twelve times. 0.3.57 kept the calls where the proc was not
 * DEMO_GET_ITEM before: none (log: no send), the game enters it before that call. Now the local
 * Link's step looks at its proc after its execute: the step it becomes 0xAE, mProcVar2 holds the
 * item (dProcGetItem_init sets both). */
static void share_held_up(const daPy_lk_c* link) {
    s32 proc = (s32)link->mCurProc;
    int entered = proc == SHARE_PROC_GET_ITEM && share_proc_before != SHARE_PROC_GET_ITEM;
    int left = proc != SHARE_PROC_GET_ITEM && share_proc_before == SHARE_PROC_GET_ITEM;
    share_proc_before = proc;
    if (left && share_held_sent) { /* the hand comes down: the item goes over the others' puppet */
        share_held_sent = 0;
        share_send("#END");
    }
    if (!entered) return;
    s16 item = link->mProcVar2;
    if (item <= 0 || item > 0xFF) return;
    static const char hex[] = "0123456789ABCDEF";
    char name[8] = {'#', 'G', 'E', 'T', '_', hex[(item >> 4) & 0xF], hex[item & 0xF], 0};
    share_send(name);
    share_held_sent = 1;
    wwhd_log_int("multiplayer: item held up, shown to the others, item", item);
}

static void share_model_end(int k) {
    if (!share_ditem_used[k]) return;
    share_ditem_used[k] = 0;
    u32 actor = 0;
    if (wwhd_fopAcM_SearchByID(share_ditem_proc[k], (u32)&actor) && actor)
        *(volatile u8*)(actor + SHARE_DITEM_DRAW_FLAGS) |= SHARE_DITEM_DEAD; /* it deletes itself */
}

/* The model of the item a player holds up, over that player's puppet. */
static void share_model_make(int k, mp_u32 uid, u32 item) {
    share_model_end(k);
    puppet_slot* p = puppet_slot_for(uid, 0);
    if (!p || !p->actor) return; /* no puppet: the icon instead (mod.c draw_markers) */
    const fopAc_ac_c* puppet = (const fopAc_ac_c*)p->actor;
    share_ditem_pos.x = puppet->current.pos.x;
    share_ditem_pos.y = puppet->current.pos.y + SHARE_DITEM_LIFT;
    share_ditem_pos.z = puppet->current.pos.z;
    share_ditem_angle.x = share_ditem_angle.z = 0;
    share_ditem_angle.y = puppet->shape_angle.y;
    u8 saved = as_context;
    as_context = AS_CTX_NONE; /* this game's own: not sent to the others (actor_sync.h) */
    u32 proc = wwhd_fopAcM_create(SHARE_PROC_DEMO_ITEM, (item & 0xFFu) | 0x7F00u | (SHARE_DITEM_FLAGS << 16),
                                  (u32)&share_ditem_pos, puppet->current.roomNo, (u32)&share_ditem_angle, 0,
                                  0xFFFFFFFFu, 0);
    as_context = saved;
    if (proc == 0xFFFFFFFFu) return;
    share_ditem_proc[k] = proc;
    share_ditem_used[k] = 1;
}

/* Every step: the models follow their puppets, shown, and end with their time. */
static void share_models_step(void) {
    unsigned long long now = wwhd_logic_step();
    for (int k = 0; k != MAX_REMOTES; ++k) {
        if (share_wanted[k] && now >= share_show_at[k]) { /* the arm is up: the item appears */
            u32 item = share_wanted[k];
            share_wanted[k] = 0;
            if (remotes[k].active && now < share_until[k]) share_model_make(k, remotes[k].uid, item);
        }
        if (!share_ditem_used[k]) continue;
        u32 actor = 0;
        if (!wwhd_fopAcM_SearchByID(share_ditem_proc[k], (u32)&actor)) { /* gone */
            share_ditem_used[k] = 0;
            continue;
        }
        if (!actor) continue; /* still being made */
        puppet_slot* p = remotes[k].active ? puppet_slot_for(remotes[k].uid, 0) : 0;
        if (!p || !p->actor || now >= share_until[k]) {
            share_model_end(k);
            continue;
        }
        const fopAc_ac_c* puppet = (const fopAc_ac_c*)p->actor;
        fopAc_ac_c* model = (fopAc_ac_c*)actor;
        model->home.pos.x = puppet->current.pos.x;
        model->home.pos.y = puppet->current.pos.y + SHARE_DITEM_LIFT;
        model->home.pos.z = puppet->current.pos.z;
        *(volatile u8*)(actor + SHARE_DITEM_DRAW_FLAGS) |= SHARE_DITEM_VISIBLE;
    }
}

/* mod.c draw_markers: 1 while the item's model shows over that player (no icon then). */
static int share_model_shown(int remote_index) {
    return remote_index >= 0 && remote_index < MAX_REMOTES && share_ditem_used[remote_index];
}

/* Rupees and ammunition another player got, given here too: rupees through the game's rupee
 * counter (PLAY_ITEM_RUPEE_COUNT); bombs and arrows only to a player who has the bomb bag or the bow,
 * through execItemGet itself (bonus.h bonus_exec_item_get: the elemental arrows' stocks and caps;
 * d_item.cpp item_func_bomb_* / arrow_*), not shared again (share_receiving). */
static void share_give(u32 item) {
    u8* play = wwhd_play_get();
    if (!play || !dh_link) return;
    u32 rupees = share_rupees(item);
    if (rupees) {
        *(volatile s32*)(play + PLAY_ITEM_RUPEE_COUNT) += (s32)rupees;
        wwhd_log_int("multiplayer: rupees shared by another player", (int)rupees);
        return;
    }
    int ammo = share_ammo(item);
    if (!ammo || !share_owns_slot(ammo == 1 ? SHARE_SLOT_BOMB : SHARE_SLOT_BOW)) return;
    share_receiving = 1;
    bonus_exec_item_get(item);
    share_receiving = 0;
    wwhd_log_int("multiplayer: ammunition shared by another player, item", (int)item);
}

/* bonus.h execItemGet: an item got here, not picked up on the ground nor held up (a chest's item is
 * sent by share_held_up): a purchase or a gift. Its rupees or ammunition go to the others ("#GIVE_xx"). */
static void share_item_given(u32 item) {
    if (share_receiving || bonus_ground_pickup || !dh_link) return;
    if (!share_rupees(item) && !share_ammo(item)) return;
    if (((const daPy_lk_c*)dh_link)->mCurProc == SHARE_PROC_GET_ITEM) return;
    static const char hex[] = "0123456789ABCDEF";
    char name[9] = {'#', 'G', 'I', 'V', 'E', '_', hex[(item >> 4) & 0xF], hex[item & 0xF], 0};
    share_send(name);
    wwhd_log_int("multiplayer: item bought or given, shared, item", (int)item);
}

static void sw_received(const mp_event* e); /* switch_sync.h (0.3.66) */
static void lp_received(const mp_event* e); /* last_place.h (0.3.70) */
static void gf_received(const mp_event* e); /* gale_follow.h (0.3.75) */

/* event_sync.h ev_received: a name starting with '#'. */
static void ck_received(const mp_event* e); /* color_key.h */
static void sr_received(const mp_event* e); /* ship_ride.h (0.3.92) */
static void dc_received(const mp_event* e); /* death_count.h (0.3.92) */
static void ls_received(const mp_event* e); /* life_share.h (0.3.92) */
static void dl_key_received(const mp_event* e); /* door_lock.h */

static void share_received(const mp_event* e) {
    if (e->sender == local_uid) return;
    const char* n = e->name;
    if (n[1] == 'S' && n[2] == 'W') { /* a switch of the room */
        sw_received(e);
        return;
    }
    if (n[1] == 'G' && n[2] == 'A' && n[3] == 'L' && n[4] == 'E') { /* a Ballad of Gales (0.3.75) */
        gf_received(e);
        return;
    }
    if (n[1] == 'D' && n[2] == 'K' && !n[3]) { /* a door unlocked with a key (door_lock.h, 0.3.82) */
        dl_key_received(e);
        return;
    }
    if (n[1] == 'H' && n[2] == 'S' && n[3] == '_') { /* the shared life, from the server (0.3.92) */
        ls_received(e);
        return;
    }
    if (n[1] == 'D' && n[2] == 'C' && n[3] == '_') { /* a player's deaths, from the server (0.3.92) */
        dc_received(e);
        return;
    }
    if (n[1] == 'S' && (n[2] == 'B' || n[2] == 'L')) { /* on another player's ship, or off (0.3.92) */
        sr_received(e);
        return;
    }
    if (n[1] == 'K' && (n[2] == 'A' || n[2] == 'T')) { /* a colour key, made or picked up (0.3.81) */
        ck_received(e);
        return;
    }
    if (n[1] == 'L' && n[2] == 'P') { /* this player's last place, from the server (0.3.70) */
        lp_received(e);
        return;
    }
    if (n[1] == 'G' && n[2] == 'E' && n[3] == 'T' && n[4] == '_') {
        int hi = share_hex(n[5]), lo = share_hex(n[6]);
        if (hi < 0 || lo < 0) return;
        u32 item = (u32)(hi << 4 | lo);
        remote_player* r = find_remote(e->sender, 0);
        if (r) {
            int k = (int)(r - remotes);
            share_item[k] = (u8)item;
            share_show_at[k] = wwhd_logic_step() + SHARE_SHOW_DELAY;
            share_until[k] = wwhd_logic_step() + SHARE_ITEM_STEPS;
            share_model_end(k);
            share_wanted[k] = (u8)item;
        }
        share_give(item);
        return;
    }
    if (n[1] == 'E' && n[2] == 'N' && n[3] == 'D' && !n[4]) { /* the hold-up ended */
        remote_player* r = find_remote(e->sender, 0);
        if (r) {
            int k = (int)(r - remotes);
            share_until[k] = wwhd_logic_step(); /* the icon ends */
            share_wanted[k] = 0;
            share_model_end(k);
        }
        return;
    }
    if (n[1] == 'G' && n[2] == 'I' && n[3] == 'V' && n[4] == 'E' && n[5] == '_') { /* bought or given */
        int hi = share_hex(n[6]), lo = share_hex(n[7]);
        if (hi >= 0 && lo >= 0) share_give((u32)(hi << 4 | lo));
        return;
    }
    if (n[1] == 'P' && n[2] == 'I' && n[3] == 'G' && n[4] == '_' && (n[5] == '0' || n[5] == '1')) {
        const u8* play = wwhd_play_get();
        const fopAc_ac_c* link = dh_link ? (const fopAc_ac_c*)dh_link : 0;
        if (!play || !link) return;
        char stage[STAGE_NAME_BYTES + 1];
        map_stage(play, stage);
        if (!map_same_stage(stage, e->stage) || link->current.roomNo != e->room) return;
        int angry = n[5] == '1';
        WWHD_GAME_DATA(SHARE_PIG_ANGER, u8) = (u8)angry;
        share_pig_last = angry; /* not sent back */
        wwhd_log_int("multiplayer: the pigs' anger from another player", angry);
    }
}

/* mod.c draw_markers: the item another player holds up, 0 when none (a rupee has no inventory icon:
 * its amount is drawn, share_rupees). */
static u32 share_item_of(int remote_index) {
    if (remote_index < 0 || remote_index >= MAX_REMOTES) return 0;
    unsigned long long now = wwhd_logic_step();
    if (now >= share_until[remote_index] || now < share_show_at[remote_index]) return 0;
    return share_item[remote_index];
}

/* Every step of the local Link (after its execute): the item it holds up, a change of the pigs'
 * anger. */
static void share_step(void* link_actor) {
    share_models_step();
    if (link_actor && (u32)link_actor == dh_link && !puppet_is(link_actor)) share_held_up((const daPy_lk_c*)link_actor);
    int angry = WWHD_GAME_DATA(SHARE_PIG_ANGER, u8) != 0;
    if (share_pig_last < 0) { /* the first look: nothing to send */
        share_pig_last = angry;
        return;
    }
    if (angry == share_pig_last) return;
    share_pig_last = angry;
    share_send(angry ? "#PIG_1" : "#PIG_0");
    wwhd_log_int("multiplayer: the pigs' anger sent to the room", angry);
}
