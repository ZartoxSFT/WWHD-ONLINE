#include "multiplayer_protocol.h"

#include <cassert>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <limits>
#include <vector>

using multiplayer::Reader;
using multiplayer::TransformSnapshot;
using multiplayer::Writer;

static void test_vlq_boundaries() {
    const std::uint32_t values[] = {0, 1, 0x7F, 0x80, 0x3FFF, 0x4000,
                                    0x1FFFFF, 0x200000, 0x0FFFFFFF,
                                    std::numeric_limits<std::uint32_t>::max()};
    for (std::uint32_t expected : values) {
        Writer writer;
        writer.vlq(expected);
        Reader reader(writer.data().data(), writer.data().size());
        std::uint32_t actual = 0;
        assert(reader.vlq(actual));
        assert(actual == expected);
        assert(reader.remaining() == 0);
    }
}

static void test_big_endian_scalars() {
    Writer writer;
    writer.u16(0x1234);
    writer.u32(0x12345678);
    writer.u64(0x0123456789ABCDEF);
    writer.f32(1.0f);
    const std::vector<std::uint8_t> expected = {
        0x12, 0x34, 0x12, 0x34, 0x56, 0x78,
        0x01, 0x23, 0x45, 0x67, 0x89, 0xAB, 0xCD, 0xEF,
        0x3F, 0x80, 0x00, 0x00};
    assert(writer.data() == expected);
}

static void test_frame_compatibility() {
    Writer payload;
    payload.vlq(0x1234);
    payload.string("Wind");

    Writer wire;
    assert(multiplayer::write_frame(wire, 0x80, payload.data()));
    const std::vector<std::uint8_t> expected = {
        0x01, 0x80, 0x87, 0x24, 0xB4, 'W', 'i', 'n', 'd', 0};
    assert(wire.data() == expected);

    Reader reader(wire.data().data(), wire.data().size());
    multiplayer::PacketFrame frame;
    assert(multiplayer::read_frame(reader, frame));
    assert(frame.id == 0x80);
    assert(frame.payload == payload.data());
    assert(reader.remaining() == 0);

    Writer empty_wire;
    assert(multiplayer::write_frame(empty_wire, 3, {}));
    Reader empty_reader(empty_wire.data().data(), empty_wire.data().size());
    assert(multiplayer::read_frame(empty_reader, frame));
    assert(frame.id == 3);
    assert(frame.payload.empty());
}

static void test_snapshot_round_trip() {
    TransformSnapshot expected;
    expected.client_id = 7;
    expected.logic_step = 123456;
    expected.stage_id = 0xBC;
    expected.room = 4;
    expected.position[0] = -10.5f;
    expected.position[1] = 20.25f;
    expected.position[2] = 0.0f;
    expected.rotation[0] = std::numeric_limits<std::int16_t>::min();
    expected.rotation[1] = 1234;
    expected.rotation[2] = -2345;

    Writer writer;
    multiplayer::write_transform_snapshot(writer, expected);
    Reader reader(writer.data().data(), writer.data().size());
    TransformSnapshot actual;
    assert(multiplayer::read_transform_snapshot(reader, actual));
    assert(actual.client_id == expected.client_id);
    assert(actual.logic_step == expected.logic_step);
    assert(actual.stage_id == expected.stage_id);
    assert(actual.room == expected.room);
    for (int i = 0; i != 3; ++i)
        assert(actual.position[i] == expected.position[i]);
    for (int i = 0; i != 3; ++i)
        assert(actual.rotation[i] == expected.rotation[i]);
    assert(reader.remaining() == 0);
}

static void test_rejects_truncated_input() {
    const std::uint8_t unterminated[] = {0x01};
    Reader vlq_reader(unterminated, sizeof(unterminated));
    std::uint32_t value = 0;
    assert(!vlq_reader.vlq(value));

    const std::uint8_t truncated_frame[] = {0x80, 0x82, 0x01};
    Reader frame_reader(truncated_frame, sizeof(truncated_frame));
    multiplayer::PacketFrame frame;
    assert(!multiplayer::read_frame(frame_reader, frame));
}

int main() {
    test_vlq_boundaries();
    test_big_endian_scalars();
    test_frame_compatibility();
    test_snapshot_round_trip();
    test_rejects_truncated_input();
    std::cout << "multiplayer protocol tests passed\n";
}
