#include "network_state.h"

#include <algorithm>
#include <cmath>

namespace multiplayer {

NetworkState::NetworkState(std::size_t max_remote_players)
    : m_max_remote_players(max_remote_players) {}

void NetworkState::set_local_snapshot(const TransformSnapshot& snapshot) {
    if (snapshot.client_id == m_local_client_id && valid_snapshot(snapshot))
        m_local_snapshot = snapshot;
}

SnapshotResult NetworkState::apply_remote_snapshot(const TransformSnapshot& snapshot) {
    if (!valid_snapshot(snapshot) || snapshot.client_id == 0 ||
        snapshot.client_id == m_local_client_id)
        return SnapshotResult::invalid;

    auto it = m_remote_players.find(snapshot.client_id);
    if (it == m_remote_players.end()) {
        if (m_remote_players.size() >= m_max_remote_players)
            return SnapshotResult::capacity_reached;
        m_remote_players.emplace(snapshot.client_id, RemotePlayer{snapshot});
        return SnapshotResult::accepted;
    }

    if (snapshot.logic_step <= it->second.snapshot.logic_step)
        return SnapshotResult::stale;
    it->second.snapshot = snapshot;
    return SnapshotResult::accepted;
}

bool NetworkState::remove_remote_player(std::uint32_t client_id) {
    return m_remote_players.erase(client_id) != 0;
}

void NetworkState::clear_remote_players() {
    m_remote_players.clear();
}

const TransformSnapshot* NetworkState::remote_snapshot(std::uint32_t client_id) const {
    const auto it = m_remote_players.find(client_id);
    return it == m_remote_players.end() ? nullptr : &it->second.snapshot;
}

std::vector<std::uint32_t> NetworkState::remote_client_ids() const {
    std::vector<std::uint32_t> ids;
    ids.reserve(m_remote_players.size());
    for (const auto& [client_id, player] : m_remote_players)
        ids.push_back(client_id);
    std::sort(ids.begin(), ids.end());
    return ids;
}

bool NetworkState::valid_snapshot(const TransformSnapshot& snapshot) {
    for (float value : snapshot.position)
        if (!std::isfinite(value))
            return false;
    return true;
}

} // namespace multiplayer
