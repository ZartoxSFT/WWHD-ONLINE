#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace multiplayer {

enum class TransportStatus : std::uint32_t {
    disconnected = 0,
    connecting = 1,
    connected = 2,
    error = 3,
};

class NetworkTransport {
public:
    NetworkTransport();
    ~NetworkTransport();

    NetworkTransport(const NetworkTransport&) = delete;
    NetworkTransport& operator=(const NetworkTransport&) = delete;

    bool connect(const std::string& host, std::uint16_t port);
    void disconnect();
    TransportStatus status() const;
    int last_error() const;

    bool send(const std::uint8_t* data, std::size_t size);
    std::size_t receive(std::uint8_t* destination, std::size_t capacity);

private:
    struct State;
    State* m_state;
};

NetworkTransport& transport();

} // namespace multiplayer
