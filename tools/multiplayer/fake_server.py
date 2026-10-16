#!/usr/bin/env python3
"""Minimal stand-in for the historical Java server, for local tests of the multiplayer guest mod.

usage: fake_server.py [--host 0.0.0.0] [--port 10014] [--bot [--mirror] [--ship] [--swing]]

--bot adds a scripted PlayerPuppet to every area a client enters, circling the first position
that client reported, and prints the client's ActorBaseSync positions once per second, so one
game instance is enough to see both directions of the synchronization.
--mirror makes the bot copy the client's ActorPlayerSync updates (animation calls, item,
colours): with the "puppet" option the bot's Link replays the player's own animations.
--ship puts the bot on its King of Red Lions (ActorPlayerSync ship block, mp_protocol.h): the ship
sails a 1200-unit circle 2500 units east of that position, sail up, painted blue, the bot's Link
on board; with
the "puppet" option the client shows it as a ship puppet (on a stage that has the local ship).
--swing makes the bot spin-attack every 2 s where it stands (ActorPlayerSync sword swing block,
mp_protocol.h): the grass, flowers and bushes around it are cut in the client's game.

It is NOT the reference server. The real ActorSync server logic lives in
com.kirbymimi.serverCommon, which is missing from this workspace, so this script only
reproduces what the historical client code proves about the wire format:

  - init blob: VLQ(length) + receiver names, sender names (index = packet ID), initializers
    (Client::readInitPacket); here only "ActorSync" (PlayerPuppet = ActorBaseSync,
    ActorPlayerSync, as in resources/SyncedActorServer/actorCreators.ktml) and "ActorFilter";
  - frames: VLQ(id) + VLQ(length) + payload, VLQ terminated by the high bit;
  - client CREATE: byte hasGameUID [VLQ n, bytes], VLQ solver, name, properties, components;
  - server CREATE: VLQ uid, byte hasGameUID, byte hasSolver [VLQ solver], name, properties,
    ticket byte, VLQ owner, components (ActorSync::create / SyncedActor::readUpdate);
  - client UPDATE: VLQ uid, ticket byte, components; server UPDATE adds VLQ owner.

Assumptions of this stand-in (not verified against serverCommon): uids start at 1, the ticket
is always 0, the owner of a PlayerPuppet is its own uid, and actors are relayed only between
clients that announced the same area with CLIENT_PLAYER_CHANGE_MANAGER.
"""
import argparse
import math
import socket
import struct
import threading
import time

SERVER_PACKETS = ["STUB", "SERVER_ACTOR_SYNC_CREATE", "SERVER_ACTOR_SYNC_UPDATE",
                  "SERVER_ACTOR_SYNC_REMOVE"]
CLIENT_PACKETS = ["CLIENT_STUB", "CLIENT_TEST", "CLIENT_INIT_PACKET", "CLIENT_SAVE_UPDATE",
                  "CLIENT_ACTOR_SYNC_CREATE", "CLIENT_ACTOR_SYNC_UPDATE",
                  "CLIENT_ACTOR_SYNC_REMOVE", "CLIENT_PLAYER_CHANGE_MANAGER",
                  "CLIENT_ACTOR_EXCLUSIVE_REMOVE", "CLIENT_ACTOR_EXCLUSIVE_ADD",
                  "CLIENT_PLAYER_LEAVE_MANAGER"]
S = {name: i for i, name in enumerate(SERVER_PACKETS)}
C = {name: i for i, name in enumerate(CLIENT_PACKETS)}


def vlq(value):
    groups = [value & 0x7F]
    value >>= 7
    while value:
        groups.append(value & 0x7F)
        value >>= 7
    groups.reverse()
    groups[-1] |= 0x80
    return bytes(groups)


def cstr(text):
    return text.encode() + b"\0"


class Reader:
    def __init__(self, data):
        self.data, self.pos = data, 0

    def byte(self):
        if self.pos >= len(self.data):
            raise ValueError("truncated")
        self.pos += 1
        return self.data[self.pos - 1]

    def vlq(self):
        value = 0
        for _ in range(5):
            b = self.byte()
            value = (value << 7) | (b & 0x7F)
            if b & 0x80:
                return value
        raise ValueError("VLQ too long")

    def bytes(self, n):
        if self.pos + n > len(self.data):
            raise ValueError("truncated")
        self.pos += n
        return self.data[self.pos - n:self.pos]

    def string(self):
        end = self.data.index(b"\0", self.pos)
        text = self.data[self.pos:end].decode(errors="replace")
        self.pos = end + 1
        return text

    def rest(self):
        return self.data[self.pos:]


def init_blob():
    body = vlq(len(SERVER_PACKETS)) + b"".join(cstr(n) for n in SERVER_PACKETS)
    body += vlq(len(CLIENT_PACKETS)) + b"".join(cstr(n) for n in CLIENT_PACKETS)
    body += vlq(2)
    body += cstr("ActorSync") + vlq(2)
    body += cstr("Default") + vlq(2) + cstr("ActorBaseSync") + cstr("ActorHealthSync")
    body += cstr("PlayerPuppet") + vlq(2) + cstr("ActorBaseSync") + cstr("ActorPlayerSync")
    body += cstr("ActorFilter") + vlq(0)
    return vlq(len(body)) + body


def base_component(x, y, z, angle_y):
    """ActorBaseSync: cXyz as 3 big-endian floats, csXyz as 3 big-endian shorts."""
    return struct.pack(">fffhhh", x, y, z, 0, angle_y, 0)


# The bot wears red (primary 0xF800, secondary 0xFC00 and their darkenRGB565 values) so its
# marker colour shows that ActorPlayerSync colours reach the client.
BOT_PLAYER_COMPONENT = (struct.pack(">H", 0xFFFF) + vlq(0xFFFFFFFF)
                        + struct.pack(">5I", 0xF800, 0xFC00, 0xB800, 0xBB00, 0x8800)
                        + bytes([0xFF, 0x30, 0x30, 0xFF]) + vlq(0) * 8
                        + b"Bot\0")  # WWHD extension: the name after the hit list


def read_base(components):
    """Position from a 'VLQ count + (VLQ index, data)*' block whose index 0 is ActorBaseSync."""
    r = Reader(components)
    if r.vlq() < 1 or r.vlq() != 0:
        return None
    return struct.unpack(">fffhhh", r.bytes(18))


class Actor:
    def __init__(self, uid, name, properties, components):
        self.uid, self.name, self.properties = uid, name, properties
        self.components = components  # VLQ count + (VLQ index, data)*, as last received
        self.last_updates = {}  # component index -> its last single-component update block

    def create_payload(self, solver=None):
        head = vlq(self.uid) + b"\0"
        head += (b"\1" + vlq(solver)) if solver is not None else b"\0"
        return head + cstr(self.name) + cstr(self.properties) + b"\0" + vlq(self.uid) + self.components

    def update_payload(self, components):
        return vlq(self.uid) + b"\0" + vlq(self.uid) + components


SHIP_PRESENT, SHIP_RIDING, SHIP_SAIL, SHIP_COLOR = 1, 2, 4, 8
SHIP_PART_SAIL = 1 << 4  # bits 4-5: daShip_c::mPart (1 sail, 2 cannon, 3 crane)
BOT_SHIP_COLOR = bytes([0x20, 0x60, 0xFF])  # a blue boat, to see its own paint


def ship_block(x, y, z, heading,
               flags=SHIP_PRESENT | SHIP_RIDING | SHIP_SAIL | SHIP_COLOR | SHIP_PART_SAIL):
    """WWHD extension after the name: u8 flags, then x, y, z and the three angles when present,
    then r, g, b with SHIP_COLOR."""
    out = bytes([flags]) + struct.pack(">fffhhh", x, y, z, 0, heading, 0)
    return out + BOT_SHIP_COLOR if flags & SHIP_COLOR else out


SWING_INTERVAL = 2.0
AT_TYPE_SWORD = 2


def swing_block(x, y, z):
    """WWHD extension after the extras: one swing, a spin attack's cylinder as the capsule from
    its centre (Link's feet) to its top, sword attack type, no attack vector."""
    return (bytes([1]) + struct.pack(">I3f", AT_TYPE_SWORD, 0.0, 0.0, 0.0) + bytes([1])
            + struct.pack(">7f", x, y, z, x, y + 80.0, z, 150.0))


def s16(angle):
    """Radians (0 facing +z, growing towards +x) as a game angle."""
    value = int(angle / (2 * math.pi) * 65536) & 0xFFFF
    return value - 0x10000 if value >= 0x8000 else value


class Bot:
    """Scripted PlayerPuppet shown to the clients of one area."""

    def __init__(self, uid, area, centre, ship=False, swing=False):
        self.area, self.centre, self.ship, self.swing = area, centre, ship, swing
        self.last_swing = 0.0
        self.actor = Actor(uid, "PlayerPuppet", "PlayerPuppet", self.components(0.0))

    def place(self, t):
        """On a 150-unit circle around the centre, one turn every ~8 s; with the ship, on board:
        at the ship's place on its own circle."""
        x, y, z = self.centre
        if self.ship:
            return self.ship_pose(t)
        angle = t * 0.8
        heading = int(math.degrees(angle + math.pi) / 360 * 65536) & 0xFFFF
        heading = heading - 0x10000 if heading >= 0x8000 else heading
        return x + 150 * math.cos(angle), y, z + 150 * math.sin(angle), heading

    def pose(self, t):
        """ActorBaseSync at the bot's place."""
        return base_component(*self.place(t))

    def ship_pose(self, t):
        """1200-unit circle 2500 units east of the centre, one turn every ~40 s, bow forward."""
        x, y, z = self.centre
        angle = t * 0.16
        px, pz = x + 2500 + 1200 * math.cos(angle), z + 1200 * math.sin(angle)
        # velocity (-sin, cos): heading atan2(dx, dz)
        heading = s16(math.atan2(-math.sin(angle), math.cos(angle)))
        return px, y, pz, heading

    def player_component(self, t, swing=False):
        out = BOT_PLAYER_COMPONENT
        if self.ship:
            out += ship_block(*self.ship_pose(t))
        if swing:  # the ship block (or none), the extras, then the swing
            x, y, z, _ = self.place(t)
            out += (b"" if self.ship else b"\0") + b"\0" + swing_block(x, y, z)
        return out

    def components(self, t):
        return vlq(2) + vlq(0) + self.pose(t) + vlq(1) + self.player_component(t)

    def update(self, t):
        swing = self.swing and t - self.last_swing >= SWING_INTERVAL
        if swing:
            self.last_swing = t
        if self.ship or swing:
            return vlq(2) + vlq(0) + self.pose(t) + vlq(1) + self.player_component(t, swing)
        return vlq(1) + vlq(0) + self.pose(t)


class Server:
    def __init__(self, bot=False, mirror=False, ship=False, swing=False):
        self.mirror, self.ship, self.swing = mirror, ship, swing
        self.lock = threading.Lock()
        self.clients = []
        self.next_uid = 1
        self.bot_enabled = bot
        self.bots = {}  # area -> Bot

    def bot_for(self, client, actor):
        """Shows the area's bot to a client whose PlayerPuppet was just created."""
        if not self.bot_enabled or actor.properties != "PlayerPuppet":
            return
        bot = self.bots.get(client.area)
        if bot is None:
            position = read_base(actor.components)
            if position is None:
                return
            bot = Bot(self.next_uid, client.area, position[:3], self.ship, self.swing)
            self.next_uid += 1
            self.bots[client.area] = bot
            client.log(f"bot uid={bot.actor.uid} circles {tuple(round(v, 1) for v in position[:3])}")
        client.send(S["SERVER_ACTOR_SYNC_CREATE"], bot.actor.create_payload())

    def bot_loop(self):
        start = time.monotonic()
        while True:
            time.sleep(0.1)
            t = time.monotonic() - start
            with self.lock:
                for bot in self.bots.values():
                    payload = bot.actor.update_payload(bot.update(t))
                    for client in self.clients:
                        if client.area == bot.area:
                            client.send(S["SERVER_ACTOR_SYNC_UPDATE"], payload)

    def peers(self, client):
        return [c for c in self.clients if c is not client and c.area and c.area == client.area]

    def leave(self, client):
        for actor in client.actors.values():
            for peer in self.peers(client):
                peer.send(S["SERVER_ACTOR_SYNC_REMOVE"], vlq(actor.uid))
        client.actors.clear()
        client.area = None


class Client:
    def __init__(self, server, sock, address):
        self.server, self.sock, self.address = server, sock, address
        self.area, self.actors, self.identity = None, {}, None
        self.send_lock = threading.Lock()
        self.last_position_log = 0.0

    def send(self, packet_id, payload):
        with self.send_lock:
            try:
                self.sock.sendall(vlq(packet_id) + vlq(len(payload)) + payload)
            except OSError:
                pass

    def log(self, text):
        print(f"[{self.address[0]}:{self.address[1]}] {text}", flush=True)

    def handle(self, packet_id, payload):
        srv = self.server
        r = Reader(payload)
        with srv.lock:
            if packet_id == C["CLIENT_INIT_PACKET"]:
                self.identity = payload.hex()
                self.log(f"identity {self.identity}")
            elif packet_id == C["CLIENT_PLAYER_LEAVE_MANAGER"]:
                srv.leave(self)
            elif packet_id == C["CLIENT_PLAYER_CHANGE_MANAGER"]:
                srv.leave(self)
                self.area = r.string()
                self.log(f"area {self.area!r}")
                for peer in srv.peers(self):  # existing players of the area
                    for actor in peer.actors.values():
                        self.send(S["SERVER_ACTOR_SYNC_CREATE"], actor.create_payload())
                        # position and ActorPlayerSync (colours) are separate updates: send both
                        for block in actor.last_updates.values():
                            self.send(S["SERVER_ACTOR_SYNC_UPDATE"], actor.update_payload(block))
            elif packet_id == C["CLIENT_ACTOR_SYNC_CREATE"]:
                if r.byte():
                    r.bytes(r.vlq())
                solver = r.vlq()
                name, properties = r.string(), r.string()
                actor = Actor(srv.next_uid, name, properties, r.rest())
                srv.next_uid += 1
                self.actors[actor.uid] = actor
                self.log(f"create {properties} uid={actor.uid} solver={solver}")
                self.send(S["SERVER_ACTOR_SYNC_CREATE"], actor.create_payload(solver))
                for peer in srv.peers(self):
                    peer.send(S["SERVER_ACTOR_SYNC_CREATE"], actor.create_payload())
                srv.bot_for(self, actor)
            elif packet_id == C["CLIENT_ACTOR_SYNC_UPDATE"]:
                uid = r.vlq()
                r.byte()  # ticket
                actor = self.actors.get(uid)
                if actor is None:
                    return
                block = r.rest()
                try:
                    head = Reader(block)
                    if head.vlq() == 1:
                        actor.last_updates[head.vlq()] = block
                except ValueError:
                    return
                now = time.monotonic()
                position = read_base(block)
                if srv.bot_enabled and position and now - self.last_position_log >= 1.0:
                    self.last_position_log = now
                    if position:
                        x, y, z, _, angle_y, _ = position
                        self.log(f"uid={uid} pos=({x:.1f}, {y:.1f}, {z:.1f}) angle_y={angle_y}")
                for peer in srv.peers(self):
                    peer.send(S["SERVER_ACTOR_SYNC_UPDATE"], actor.update_payload(block))
                bot = srv.bots.get(self.area)
                if srv.mirror and bot and position is None and actor.properties == "PlayerPuppet":
                    # ActorPlayerSync (component 1): the bot plays the player's own animations
                    self.send(S["SERVER_ACTOR_SYNC_UPDATE"], bot.actor.update_payload(block))
            elif packet_id == C["CLIENT_ACTOR_SYNC_REMOVE"]:
                actor = self.actors.pop(r.vlq(), None)
                if actor:
                    for peer in srv.peers(self):
                        peer.send(S["SERVER_ACTOR_SYNC_REMOVE"], vlq(actor.uid))

    def run(self):
        self.log("connected")
        with self.send_lock:
            self.sock.sendall(init_blob())
        buffer = b""
        try:
            while True:
                chunk = self.sock.recv(4096)
                if not chunk:
                    break
                buffer += chunk
                while True:
                    r = Reader(buffer)
                    try:
                        packet_id, length = r.vlq(), r.vlq()
                    except ValueError:
                        break
                    if len(buffer) - r.pos < length:
                        break
                    payload = buffer[r.pos:r.pos + length]
                    buffer = buffer[r.pos + length:]
                    self.handle(packet_id, payload)
        except (OSError, ValueError) as error:
            self.log(f"error: {error}")
        finally:
            with self.server.lock:
                self.server.leave(self)
                self.server.clients.remove(self)
            self.sock.close()
            self.log("disconnected")


def main():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--host", default="0.0.0.0")
    parser.add_argument("--port", type=int, default=10014)
    parser.add_argument("--bot", action="store_true", help="add a scripted remote player")
    parser.add_argument("--mirror", action="store_true",
                        help="the bot copies the player's animations, item and colours")
    parser.add_argument("--ship", action="store_true",
                        help="the bot sails its King of Red Lions (ship puppet test)")
    parser.add_argument("--swing", action="store_true",
                        help="the bot spin-attacks every 2 s (sword swing test: grass, bushes)")
    args = parser.parse_args()
    server = Server(args.bot, args.mirror, args.ship, args.swing)
    listener = socket.create_server((args.host, args.port))
    print(f"fake multiplayer server on {args.host}:{args.port}{' with bot' if args.bot else ''}",
          flush=True)
    if args.bot:
        threading.Thread(target=server.bot_loop, daemon=True).start()
    while True:
        sock, address = listener.accept()
        sock.setsockopt(socket.IPPROTO_TCP, socket.TCP_NODELAY, 1)
        client = Client(server, sock, address)
        with server.lock:
            server.clients.append(client)
        threading.Thread(target=client.run, daemon=True).start()


if __name__ == "__main__":
    main()
