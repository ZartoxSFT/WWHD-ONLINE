#include "../src/network_state.h"

#include <cassert>

using multiplayer::NetworkState;
using multiplayer::SnapshotResult;
using multiplayer::TransformSnapshot;

static TransformSnapshot snapshot(std::uint32_t client_id, std::uint32_t logic_step) {
    TransformSnapshot value;
    value.client_id = client_id;
    value.logic_step = logic_step;
    value.stage_id = 1;
    value.position[0] = static_cast<float>(logic_step);
    value.room = 2;
    return value;
}

int main() {
    NetworkState state(1);
    state.set_local_client_id(7);
    state.set_local_snapshot(snapshot(7, 1));
    assert(state.local_snapshot().logic_step == 1);

    assert(state.apply_remote_snapshot(snapshot(8, 2)) == SnapshotResult::accepted);
    assert(state.apply_remote_snapshot(snapshot(8, 2)) == SnapshotResult::stale);
    assert(state.apply_remote_snapshot(snapshot(8, 3)) == SnapshotResult::accepted);
    assert(state.apply_remote_snapshot(snapshot(9, 1)) == SnapshotResult::capacity_reached);
    assert(state.remote_snapshot(8) != nullptr);
    assert(state.remove_remote_player(8));
    assert(!state.remove_remote_player(8));
    assert(state.remote_player_count() == 0);
}
