#pragma once

#include <cstddef>
#include <cstdint>
#include <limits>
#include <string_view>
#include <vector>

namespace multiplayer {

class Writer {
public:
    void byte(std::uint8_t value);
    void bytes(const std::uint8_t* data, std::size_t size);
    void u16(std::uint16_t value);
    void u32(std::uint32_t value);
    void u64(std::uint64_t value);
    void f32(float value);
    void vlq(std::uint32_t value);
    void string(std::string_view value);

    const std::vector<std::uint8_t>& data() const { return m_data; }

private:
    std::vector<std::uint8_t> m_data;
};

class Reader {
public:
    Reader(const std::uint8_t* data, std::size_t size) : m_data(data), m_size(size) {}

    bool byte(std::uint8_t& value);
    bool bytes(std::uint8_t* data, std::size_t size);
    bool u16(std::uint16_t& value);
    bool u32(std::uint32_t& value);
    bool u64(std::uint64_t& value);
    bool f32(float& value);
    bool vlq(std::uint32_t& value);
    bool string(std::string_view& value);
    std::size_t remaining() const { return m_size - m_position; }

private:
    bool take(const std::uint8_t*& data, std::size_t size);

    const std::uint8_t* m_data = nullptr;
    std::size_t m_size = 0;
    std::size_t m_position = 0;
};

struct PacketFrame {
    std::uint32_t id = 0;
    std::vector<std::uint8_t> payload;
};

bool write_frame(Writer& writer, std::uint32_t packet_id,
                 const std::vector<std::uint8_t>& payload);
bool read_frame(Reader& reader, PacketFrame& frame);

struct TransformSnapshot {
    std::uint32_t client_id = 0;
    std::uint32_t logic_step = 0;
    std::uint32_t stage_id = 0;
    std::uint8_t room = 0;
    float position[3] = {};
    std::int16_t rotation[3] = {};
};

void write_transform_snapshot(Writer& writer, const TransformSnapshot& snapshot);
bool read_transform_snapshot(Reader& reader, TransformSnapshot& snapshot);

} // namespace multiplayer
