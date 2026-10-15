#include "network_transport.h"

#include "multiplayer_protocol.h"

#include <algorithm>
#include <climits>
#include <cstring>
#include <deque>
#include <cerrno>
#include <mutex>
#include <thread>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
using Socket = SOCKET;
constexpr Socket kInvalidSocket = INVALID_SOCKET;
static int socket_error() { return WSAGetLastError(); }
#else
#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <unistd.h>
using Socket = int;
constexpr Socket kInvalidSocket = -1;
static int socket_error() { return errno; }
#endif

namespace multiplayer {
namespace {

constexpr std::size_t kMaxFrameBytes = 1024 * 1024;
constexpr std::size_t kMaxQueuedFrames = 256;

void close_socket(Socket socket) {
    if (socket == kInvalidSocket)
        return;
#ifdef _WIN32
    closesocket(socket);
#else
    close(socket);
#endif
}

void shutdown_socket(Socket socket) {
    if (socket == kInvalidSocket)
        return;
#ifdef _WIN32
    shutdown(socket, SD_BOTH);
#else
    shutdown(socket, SHUT_RDWR);
#endif
}

bool send_all(Socket socket, const std::uint8_t* data, std::size_t size) {
    while (size != 0) {
        const int sent = send(socket, reinterpret_cast<const char*>(data),
                              static_cast<int>(std::min(size, std::size_t(INT_MAX))), 0);
        if (sent <= 0)
            return false;
        data += sent;
        size -= static_cast<std::size_t>(sent);
    }
    return true;
}

} // namespace

struct NetworkTransport::State {
    mutable std::mutex mutex;
    std::thread worker;
    Socket socket = kInvalidSocket;
    TransportStatus status = TransportStatus::disconnected;
    int error = 0;
    bool stop = false;
    std::deque<std::vector<std::uint8_t>> outgoing;
    std::deque<std::vector<std::uint8_t>> incoming;
    std::vector<std::uint8_t> receive_buffer;
    bool initialization_pending = true;
};

NetworkTransport::NetworkTransport() : m_state(new State) {}

NetworkTransport::~NetworkTransport() {
    disconnect();
    delete m_state;
}

bool NetworkTransport::connect(const std::string& host, std::uint16_t port) {
    disconnect();
    if (host.empty() || port == 0)
        return false;

#ifdef _WIN32
    static std::once_flag winsock_once;
    static int winsock_error = 0;
    std::call_once(winsock_once, [] {
        WSADATA data = {};
        winsock_error = WSAStartup(MAKEWORD(2, 2), &data);
    });
    if (winsock_error != 0)
        return false;
#endif

    {
        std::lock_guard lock(m_state->mutex);
        m_state->status = TransportStatus::connecting;
        m_state->error = 0;
        m_state->stop = false;
        m_state->receive_buffer.clear();
        m_state->initialization_pending = true;
    }

    m_state->worker = std::thread([this, host, port] {
        addrinfo hints = {};
        hints.ai_socktype = SOCK_STREAM;
        hints.ai_family = AF_UNSPEC;
        addrinfo* addresses = nullptr;
        const std::string service = std::to_string(port);
        const int lookup = getaddrinfo(host.c_str(), service.c_str(), &hints, &addresses);
        if (lookup != 0) {
            std::lock_guard lock(m_state->mutex);
            m_state->error = lookup;
            m_state->status = TransportStatus::error;
            return;
        }

        Socket connected = kInvalidSocket;
        int error = 0;
        for (addrinfo* address = addresses; address != nullptr; address = address->ai_next) {
            connected = static_cast<Socket>(socket(address->ai_family, address->ai_socktype,
                                                   address->ai_protocol));
            if (connected == kInvalidSocket) {
                error = socket_error();
                continue;
            }
            if (::connect(connected, address->ai_addr, static_cast<int>(address->ai_addrlen)) == 0)
                break;
            error = socket_error();
            close_socket(connected);
            connected = kInvalidSocket;
        }
        freeaddrinfo(addresses);
        if (connected != kInvalidSocket) {
            // Small frames sent every step: without TCP_NODELAY Nagle's algorithm holds them until
            // the previous one is acknowledged (delayed ACK, ~200 ms on Windows), so the other
            // players received positions and animations in bursts. The Java server
            // (mmb net/Server.java) and tools/multiplayer/fake_server.py set it on their side.
            int on = 1;
            setsockopt(connected, IPPROTO_TCP, TCP_NODELAY, reinterpret_cast<const char*>(&on), sizeof(on));
        }

        {
            std::lock_guard lock(m_state->mutex);
            if (m_state->stop) {
                close_socket(connected);
                return;
            }
            if (connected == kInvalidSocket) {
                m_state->error = error;
                m_state->status = TransportStatus::error;
                return;
            }
            m_state->socket = connected;
            m_state->status = TransportStatus::connected;
        }
        const Socket socket = connected;

        std::uint8_t buffer[4096];
        for (;;) {
            std::deque<std::vector<std::uint8_t>> outgoing;
            {
                std::lock_guard lock(m_state->mutex);
                if (m_state->stop)
                    break;
                outgoing.swap(m_state->outgoing);
            }
            for (const auto& packet : outgoing) {
                if (!send_all(socket, packet.data(), packet.size())) {
                    std::lock_guard lock(m_state->mutex);
                    m_state->error = socket_error();
                    m_state->status = TransportStatus::error;
                    m_state->stop = true;
                    break;
                }
            }
            {
                std::lock_guard lock(m_state->mutex);
                if (m_state->stop)
                    break;
            }

            fd_set read_set;
            FD_ZERO(&read_set);
            FD_SET(socket, &read_set);
            timeval timeout = {};
            timeout.tv_sec = 0;
            timeout.tv_usec = 100000;
            const int ready = select(
#ifdef _WIN32
                0,
#else
                socket + 1,
#endif
                &read_set, nullptr, nullptr, &timeout);
            if (ready == 0)
                continue;
            if (ready < 0) {
                std::lock_guard lock(m_state->mutex);
                m_state->error = socket_error();
                m_state->status = TransportStatus::error;
                break;
            }

            const int received = recv(socket, reinterpret_cast<char*>(buffer),
                                      sizeof(buffer), 0);
            if (received <= 0) {
                std::lock_guard lock(m_state->mutex);
                if (!m_state->stop) {
                    m_state->error = received == 0 ? 0 : socket_error();
                    m_state->status = received == 0 ? TransportStatus::disconnected
                                                    : TransportStatus::error;
                }
                break;
            }

            std::lock_guard lock(m_state->mutex);
            m_state->receive_buffer.insert(m_state->receive_buffer.end(), buffer,
                                           buffer + received);
            while (!m_state->receive_buffer.empty()) {
                if (m_state->initialization_pending) {
                    Reader reader(m_state->receive_buffer.data(),
                                  m_state->receive_buffer.size());
                    std::uint32_t length = 0;
                    if (!reader.vlq(length)) {
                        if (m_state->receive_buffer.size() >= 5) {
                            m_state->error = 0;
                            m_state->status = TransportStatus::error;
                            m_state->stop = true;
                        }
                        break;
                    }
                    const std::size_t prefix =
                        m_state->receive_buffer.size() - reader.remaining();
                    if (length > kMaxFrameBytes) {
                        m_state->error = 0;
                        m_state->status = TransportStatus::error;
                        m_state->stop = true;
                        break;
                    }
                    if (reader.remaining() < length)
                        break;
                    const std::size_t consumed = prefix + length;
                    if (m_state->incoming.size() >= kMaxQueuedFrames) {
                        m_state->error = 0;
                        m_state->status = TransportStatus::error;
                        m_state->stop = true;
                        break;
                    }
                    m_state->incoming.emplace_back(
                        m_state->receive_buffer.begin(),
                        m_state->receive_buffer.begin() + consumed);
                    m_state->receive_buffer.erase(
                        m_state->receive_buffer.begin(),
                        m_state->receive_buffer.begin() + consumed);
                    m_state->initialization_pending = false;
                    continue;
                }
                {
                    // Reject an oversized announced length before buffering its payload.
                    Reader header(m_state->receive_buffer.data(), m_state->receive_buffer.size());
                    std::uint32_t id = 0;
                    std::uint32_t length = 0;
                    if (header.vlq(id) && header.vlq(length) && length > kMaxFrameBytes) {
                        m_state->error = 0;
                        m_state->status = TransportStatus::error;
                        m_state->stop = true;
                        break;
                    }
                }
                Reader reader(m_state->receive_buffer.data(), m_state->receive_buffer.size());
                PacketFrame frame;
                if (!read_frame(reader, frame))
                    break;
                const std::size_t consumed =
                    m_state->receive_buffer.size() - reader.remaining();
                if (consumed > kMaxFrameBytes) {
                    m_state->error = 0;
                    m_state->status = TransportStatus::error;
                    m_state->stop = true;
                    break;
                }
                if (m_state->incoming.size() >= kMaxQueuedFrames) {
                    m_state->error = 0;
                    m_state->status = TransportStatus::error;
                    m_state->stop = true;
                    break;
                }
                m_state->incoming.emplace_back(
                    m_state->receive_buffer.begin(),
                    m_state->receive_buffer.begin() + consumed);
                m_state->receive_buffer.erase(
                    m_state->receive_buffer.begin(),
                    m_state->receive_buffer.begin() + consumed);
            }
            if (m_state->receive_buffer.size() > kMaxFrameBytes) {
                m_state->error = 0;
                m_state->status = TransportStatus::error;
                m_state->stop = true;
                break;
            }
        }

        close_socket(socket);
        std::lock_guard lock(m_state->mutex);
        m_state->socket = kInvalidSocket;
        if (m_state->status == TransportStatus::connected)
            m_state->status = TransportStatus::disconnected;
    });
    return true;
}

void NetworkTransport::disconnect() {
    {
        std::lock_guard lock(m_state->mutex);
        m_state->stop = true;
        // The worker owns the descriptor and closes it; shutdown only wakes it up.
        shutdown_socket(m_state->socket);
        m_state->socket = kInvalidSocket;
        m_state->status = TransportStatus::disconnected;
    }
    if (m_state->worker.joinable())
        m_state->worker.join();
    std::lock_guard lock(m_state->mutex);
    m_state->outgoing.clear();
    m_state->incoming.clear();
    m_state->receive_buffer.clear();
}

TransportStatus NetworkTransport::status() const {
    std::lock_guard lock(m_state->mutex);
    return m_state->status;
}

int NetworkTransport::last_error() const {
    std::lock_guard lock(m_state->mutex);
    return m_state->error;
}

bool NetworkTransport::send(const std::uint8_t* data, std::size_t size) {
    if (!data || size == 0 || size > kMaxFrameBytes)
        return false;
    std::lock_guard lock(m_state->mutex);
    if (m_state->status != TransportStatus::connected || m_state->stop ||
        m_state->outgoing.size() >= kMaxQueuedFrames)
        return false;
    m_state->outgoing.emplace_back(data, data + size);
    return true;
}

std::size_t NetworkTransport::receive(std::uint8_t* destination, std::size_t capacity) {
    if (!destination || capacity == 0)
        return 0;
    std::lock_guard lock(m_state->mutex);
    if (m_state->incoming.empty())
        return 0;
    const auto& packet = m_state->incoming.front();
    if (packet.size() > capacity)
        return 0;
    std::memcpy(destination, packet.data(), packet.size());
    const std::size_t result = packet.size();
    m_state->incoming.pop_front();
    return result;
}

NetworkTransport& transport() {
    static NetworkTransport instance;
    return instance;
}

} // namespace multiplayer
