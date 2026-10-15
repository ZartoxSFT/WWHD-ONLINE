/* Read-only probe for the first HD multiplayer integration tranche.
 * The actor_place layout and offsets come from the generated public SDK. */
#include "wwhd_guest.h"
#include "wwhd/bindings.h"
#include "wwhd/actor.h"

static u32 samples;

WWHD_HOOK(WWHD_ADDR_daPy_Execute, probe_link_transform, (void* link)) {
    int every = wwhd_config_int("every", 30);
    if (every < 1) every = 1;
    if (++samples % (u32)every) return;

    const fopAc_ac_c* actor = (const fopAc_ac_c*)link;
    wwhd_log_float("link-transform-probe: x", actor->current.pos.x);
    wwhd_log_float("link-transform-probe: y", actor->current.pos.y);
    wwhd_log_float("link-transform-probe: z", actor->current.pos.z);
    wwhd_log_int("link-transform-probe: angle-y", actor->current.angle.y);
    wwhd_log_int("link-transform-probe: room", actor->current.roomNo);
}
