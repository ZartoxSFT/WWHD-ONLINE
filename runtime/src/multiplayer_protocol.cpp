#include "multiplayer_protocol.h"

#include <cstring>

namespace multiplayer {

void Writer::byte(std::uint8_t value) {
    m_data.push_back(value);
}

void Writer::bytes(const std::uint8_t* data, std::size_t size) {
    if (size == 0)
        return;
    m_data.insert(m_data.end(), data, data + size);
}

void Writer::u16(std::uint16_t value) {
    byte(static_cast<std::uint8_t>(value >> 8));
    byte(static_cast<std::uint8_t>(value));
}

void Writer::u32(std::uint32_t value) {
    byte(static_cast<std::uint8_t>(value >> 24));
    byte(static_cast<std::uint8_t>(value >> 16));
    byte(static_cast<std::uint8_t>(value >> 8));
    byte(static_cast<std::uint8_t>(value));
}

void Writer::u64(std::uint64_t value) {
    for (int shift = 56; shift >= 0; shift -= 8)
        byte(static_cast<std::uint8_t>(value >> shift));
}

void Writer::f32(float value) {
    static_assert(sizeof(float) == sizeof(std::uint32_t));
    std::uint32_t bits = 0;
    std::memcpy(&bits, &value, sizeof(bits));
    u32(bits);
}

void Writer::vlq(std::uint32_t value) {
    std::uint8_t encoded[5];
    std::size_t count = 0;
    do {
        encoded[count++] = static_cast<std::uint8_t>(value & 0x7F);
        value >>= 7;
    } while (value != 0);
    while (count != 0) {
        const std::uint8_t digit = encoded[--count];
        byte(static_cast<std::uint8_t>(digit | (count == 0 ? 0x80 : 0)));
    }
}

void Writer::string(std::string_view value) {
    bytes(reinterpret_cast<const std::uint8_t*>(value.data()), value.size());
    byte(0);
}

bool Reader::take(const std::uint8_t*& data, std::size_t size) {
    if (size > m_size - m_position)
        return false;
    data = m_data + m_position;
    m_position += size;
    return true;
}

bool Reader::byte(std::uint8_t& value) {
    const std::uint8_t* data = nullptr;
    if (!take(data, 1))
        return false;
    value = data[0];
    return true;
}

bool Reader::bytes(std::uint8_t* data, std::size_t size) {
    const std::uint8_t* source = nullptr;
    if (!take(source, size))
        return false;
    std::memcpy(data, source, size);
    return true;
}

bool Reader::u16(std::uint16_t& value) {
    std::uint8_t bytes[2] = {};
    if (!this->bytes(bytes, sizeof(bytes)))
        return false;
    value = (static_cast<std::uint16_t>(bytes[0]) << 8) | bytes[1];
    return true;
}

bool Reader::u32(std::uint32_t& value) {
    std::uint8_t bytes[4] = {};
    if (!this->bytes(bytes, sizeof(bytes)))
        return false;
    value = (static_cast<std::uint32_t>(bytes[0]) << 24) |
            (static_cast<std::uint32_t>(bytes[1]) << 16) |
            (static_cast<std::uint32_t>(bytes[2]) << 8) |
            bytes[3];
    return true;
}

bool Reader::u64(std::uint64_t& value) {
    std::uint8_t bytes[8] = {};
    if (!this->bytes(bytes, sizeof(bytes)))
        return false;
    value = 0;
    for (std::uint8_t byte : bytes)
        value = (value << 8) | byte;
    return true;
}

bool Reader::f32(float& value) {
    std::uint32_t bits = 0;
    if (!u32(bits))
        return false;
    std::memcpy(&value, &bits, sizeof(value));
    return true;
}

bool Reader::vlq(std::uint32_t& value) {
    value = 0;
    for (std::size_t i = 0; i != 5; ++i) {
        std::uint8_t next = 0;
        if (!byte(next))
            return false;
        if (value > (std::numeric_limits<std::uint32_t>::max() >> 7))
            return false;
        value = (value << 7) | (next & 0x7F);
        if ((next & 0x80) != 0)
            return true;
    }
    return false;
}

bool Reader::string(std::string_view& value) {
    const std::size_t start = m_position;
    while (m_position != m_size) {
        if (m_data[m_position++] == 0) {
            value = std::string_view(reinterpret_cast<const char*>(m_data + start),
                                     m_position - start - 1);
            return true;
        }
    }
    return false;
}

bool write_frame(Writer& writer, std::uint32_t packet_id,
                 const std::vector<std::uint8_t>& payload) {
    if (payload.size() > std::numeric_limits<std::uint32_t>::max())
        return false;
    writer.vlq(packet_id);
    writer.vlq(static_cast<std::uint32_t>(payload.size()));
    writer.bytes(payload.data(), payload.size());
    return true;
}

bool read_frame(Reader& reader, PacketFrame& frame) {
    std::uint32_t length = 0;
    if (!reader.vlq(frame.id) || !reader.vlq(length) || length > reader.remaining())
        return false;
    frame.payload.resize(length);
    return reader.bytes(frame.payload.data(), length);
}

void write_transform_snapshot(Writer& writer, const TransformSnapshot& snapshot) {
    writer.u32(snapshot.client_id);
    writer.u32(snapshot.logic_step);
    writer.u32(snapshot.stage_id);
    writer.byte(snapshot.room);
    for (float value : snapshot.position)
        writer.f32(value);
    for (std::int16_t value : snapshot.rotation)
        writer.u16(static_cast<std::uint16_t>(value));
}

bool read_transform_snapshot(Reader& reader, TransformSnapshot& snapshot) {
    if (!reader.u32(snapshot.client_id) ||
        !reader.u32(snapshot.logic_step) ||
        !reader.u32(snapshot.stage_id) ||
        !reader.byte(snapshot.room))
        return false;
    for (float& value : snapshot.position)
        if (!reader.f32(value))
            return false;
    for (std::int16_t& value : snapshot.rotation) {
        std::uint16_t encoded = 0;
        if (!reader.u16(encoded))
            return false;
        value = static_cast<std::int16_t>(encoded);
    }
    return true;
}

} // namespace multiplayer
