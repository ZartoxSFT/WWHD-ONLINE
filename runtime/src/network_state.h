#pragma once

#include "multiplayer_protocol.h"

#include <cstddef>
#include <cstdint>
#include <unordered_map>
#include <vector>

namespace multiplayer {

enum class SnapshotResult {
    accepted,
    stale,
    invalid,
    capacity_reached,
};

class NetworkState {
public:
    explicit NetworkState(std::size_t max_remote_players = 16);

    void set_local_client_id(std::uint32_t client_id) { m_local_client_id = client_id; }
    std::uint32_t local_client_id() const { return m_local_client_id; }

    void set_local_snapshot(const TransformSnapshot& snapshot);
    const TransformSnapshot& local_snapshot() const { return m_local_snapshot; }

    SnapshotResult apply_remote_snapshot(const TransformSnapshot& snapshot);
    bool remove_remote_player(std::uint32_t client_id);
    void clear_remote_players();

    const TransformSnapshot* remote_snapshot(std::uint32_t client_id) const;
    std::vector<std::uint32_t> remote_client_ids() const;
    std::size_t remote_player_count() const { return m_remote_players.size(); }

private:
    struct RemotePlayer {
        TransformSnapshot snapshot;
    };

    static bool valid_snapshot(const TransformSnapshot& snapshot);

    std::uint32_t m_local_client_id = 0;
    TransformSnapshot m_local_snapshot;
    std::size_t m_max_remote_players;
    std::unordered_map<std::uint32_t, RemotePlayer> m_remote_players;
};

} // namespace multiplayer
