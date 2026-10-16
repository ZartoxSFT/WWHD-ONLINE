#!/usr/bin/env python3
"""Minimal stand-in for the historical Java server, for local tests of the multiplayer guest mod.

usage: fake_server.py [--host 0.0.0.0] [--port 10014] [--bot [--mirror] [--ship] [--swing]]
                      [--multiplier N] [--randomizer] [--actor-list FILE]

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
--multiplier sends the enemy multiplier (ActorFilter entry "#enemy_multiplier=N", mp_protocol.h),
--randomizer turns the copies' random variants on ("#enemy_randomizer=1").
While it runs, the line "mutators N 0|1" on its standard input changes them live, like the Java
server's console (ActorFilter.java): SERVER_MUTATORS (VLQ N, u8 randomizer) goes to every client,
the clients connecting later get the new values in their init.
--actor-list is the ActorFilter name list (default: the Java server's
WindWakerServer/windwakerserver/resources/WWServer/actorList.ktml, one quoted name per line).

Placed actors (a CREATE with a game UID: enemies, props) follow the Java server
(MMBServerCommon SyncedActorManager, SyncedActorComponent, SyncedActorClient): one actor per game
UID and area, created by the first client, the others get the existing one; the owner is the
creator, then every 2 s the client nearest to it (its PlayerPuppet position), unless a client
holds it exclusively (CLIENT_ACTOR_EXCLUSIVE_ADD changes the owner at once); a change of owner
raises the ticket and sends a full update to everybody; an update with an old ticket is dropped,
the others are relayed to every client of the area but the owner; CLIENT_ACTOR_SYNC_REMOVE
removes it for everybody; a client leaving the area loses its actors (owner 0 until the next
choice). The factories are those of resources/SyncedActorServer/actorCreators.ktml.
Spawned actors (a CREATE without a game UID whose factory is "Spawned": dropped items, bombs,
arrows) are kept the same way, one per create, and sent to the clients that enter the area later
(SyncedActorManager.addClient).

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
import os
import random
import socket
import struct
import sys
import threading
import time

SERVER_PACKETS = ["STUB", "SERVER_ACTOR_SYNC_CREATE", "SERVER_ACTOR_SYNC_UPDATE",
                  "SERVER_ACTOR_SYNC_REMOVE", "SERVER_MUTATORS"]
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


# resources/SyncedActorServer/actorCreators.ktml (DefaultCreators = ActorBaseSync, ActorHealthSync)
DEFAULT_COMPONENTS = ["ActorBaseSync", "ActorHealthSync"]
CHUCHU = DEFAULT_COMPONENTS + ["ChuChuSync"]
PROPS = DEFAULT_COMPONENTS + ["PropsSync"]
FACTORIES = {
    "Default": DEFAULT_COMPONENTS,
    "PlayerPuppet": ["ActorBaseSync", "ActorPlayerSync"],
    "c_red": CHUCHU, "c_green": CHUCHU, "c_blue": CHUCHU, "c_kiiro": CHUCHU, "c_black": CHUCHU,
    "Bk": DEFAULT_COMPONENTS + ["BokoblinSync"],
    "Spawned": ["ActorBaseSync", "ActorSpawnSync"],
}
for _name in ["kotubo", "ootubo1", "Kmtub", "Ktaru", "Ostool", "Odokuro", "Okioke", "Kmi02", "Ptubo",
              "KkibaB", "Kmi00", "Hbox2S"]:
    FACTORIES[_name] = PROPS
COMPONENT_SIZES = {"ActorBaseSync": 18, "ActorHealthSync": 6, "ChuChuSync": 3, "BokoblinSync": 6,
                   "PropsSync": 5, "ActorSpawnSync": 38}
DEFAULT_ACTOR_LIST = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "..",
                                  "WindWakerServer", "windwakerserver", "resources", "WWServer",
                                  "actorList.ktml")


def read_actor_list(path):
    """actorList.ktml: one quoted name per line."""
    names = []
    with open(path, encoding="utf-8") as f:
        for line in f:
            line = line.strip()
            if len(line) >= 2 and line[0] == line[-1] == '"':
                names.append(line[1:-1])
    return names


def init_blob(filter_names, multiplier, randomizer=False):
    body = vlq(len(SERVER_PACKETS)) + b"".join(cstr(n) for n in SERVER_PACKETS)
    body += vlq(len(CLIENT_PACKETS)) + b"".join(cstr(n) for n in CLIENT_PACKETS)
    body += vlq(2)
    body += cstr("ActorSync") + vlq(len(FACTORIES))
    for name, components in FACTORIES.items():
        body += cstr(name) + vlq(len(components)) + b"".join(cstr(c) for c in components)
    entries = list(filter_names) + ([f"#enemy_multiplier={multiplier}"] if multiplier > 1 else [])
    entries += ["#enemy_randomizer=1"] if randomizer else []
    body += cstr("ActorFilter") + vlq(len(entries)) + b"".join(cstr(n) for n in entries)
    return vlq(len(body)) + body


def read_components(block, components):
    """'VLQ count + (VLQ index, data)*' of fixed-size components -> {index: data}."""
    r = Reader(block)
    out = {}
    for _ in range(r.vlq()):
        index = r.vlq()
        if index >= len(components) or components[index] not in COMPONENT_SIZES:
            raise ValueError("bad component index")
        out[index] = r.bytes(COMPONENT_SIZES[components[index]])
    if r.pos != len(block):
        raise ValueError("trailing bytes")
    return out


class PlacedActor:
    """A placed actor of one area (SyncedActorComponent with a game UID)."""

    def __init__(self, uid, game_uid, name, properties, data):
        self.uid, self.game_uid, self.name, self.properties = uid, game_uid, name, properties
        self.key = game_uid if game_uid is not None else ("spawned", uid)
        self.components = FACTORIES.get(properties, FACTORIES["Default"])
        self.data = data  # component index -> bytes
        self.owner = None  # Client
        self.ticket = 1
        self.next_owner_check = time.monotonic() + random.uniform(0, 2)

    def position(self):
        if 0 not in self.data or self.components[0] != "ActorBaseSync":
            return None
        return struct.unpack(">fff", self.data[0][:12])

    def body(self, indices):
        owner = self.owner.puppet_uid() if self.owner else 0
        indices = list(indices)
        out = bytes([self.ticket]) + vlq(owner or 0) + vlq(len(indices))
        for i in indices:
            out += vlq(i) + self.data.get(i, bytes(COMPONENT_SIZES[self.components[i]]))
        return out

    def create_payload(self, solver=None):
        if self.game_uid is None:
            head = vlq(self.uid) + b"\0"
        else:
            head = vlq(self.uid) + b"\1" + vlq(len(self.game_uid)) + self.game_uid
        head += (b"\1" + vlq(solver)) if solver is not None else b"\0"
        return head + cstr(self.name) + cstr(self.properties) + self.body(range(len(self.components)))

    def full_update(self):
        return vlq(self.uid) + self.body(range(len(self.components)))


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
    def __init__(self, bot=False, mirror=False, ship=False, swing=False, filter_names=(), multiplier=1,
                 randomizer=False):
        self.mirror, self.ship, self.swing = mirror, ship, swing
        self.lock = threading.Lock()
        self.clients = []
        self.next_uid = 1
        self.bot_enabled = bot
        self.bots = {}  # area -> Bot
        self.init = init_blob(filter_names, multiplier, randomizer)
        self.placed = {}  # area -> {game uid: PlacedActor}

    def area_clients(self, area):
        return [c for c in self.clients if c.area == area]

    def change_owner(self, actor, owner, area):
        """SyncedActorComponent.tryChangeOwner, then the full update its next run sends."""
        if actor.owner is owner:
            return
        actor.owner = owner
        actor.ticket = 1 if actor.ticket == 0xFE else actor.ticket + 1
        for client in self.area_clients(area):
            client.send(S["SERVER_ACTOR_SYNC_UPDATE"], actor.full_update())

    def owner_loop(self):
        """SyncedActorComponent.onRun: every 2 s the exclusive holder, else the nearest player."""
        while True:
            time.sleep(0.1)
            now = time.monotonic()
            with self.lock:
                for area, actors in self.placed.items():
                    clients = [c for c in self.area_clients(area) if c.puppet_position() is not None]
                    for actor in actors.values():
                        if now < actor.next_owner_check:
                            continue
                        actor.next_owner_check = now + 2.0
                        best, best_distance = None, None
                        for client in clients:
                            if actor.uid in client.exclusive:
                                best = client
                                break
                            here, there = client.puppet_position(), actor.position()
                            if there is None:
                                continue
                            distance = sum((a - b) ** 2 for a, b in zip(here, there))
                            if best_distance is None or distance < best_distance:
                                best, best_distance = client, distance
                        if best is not None:
                            self.change_owner(actor, best, area)

    def find_placed(self, area, uid):
        for actor in self.placed.get(area, {}).values():
            if actor.uid == uid:
                return actor
        return None

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
        area = client.area
        client.area = None
        client.exclusive.clear()
        if area is None:
            return
        if not self.area_clients(area):
            self.placed.pop(area, None)  # the manager dies with its last client
            return
        for actor in self.placed.get(area, {}).values():
            if actor.owner is client:
                self.change_owner(actor, None, area)


class Client:
    def __init__(self, server, sock, address):
        self.server, self.sock, self.address = server, sock, address
        self.area, self.actors, self.identity = None, {}, None
        self.exclusive = set()  # placed actor uids held with CLIENT_ACTOR_EXCLUSIVE_ADD
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

    def puppet(self):
        for actor in self.actors.values():
            if actor.properties == "PlayerPuppet":
                return actor
        return None

    def puppet_uid(self):
        puppet = self.puppet()
        return puppet.uid if puppet else 0

    def puppet_position(self):
        puppet = self.puppet()
        if puppet is None:
            return None
        block = puppet.last_updates.get(0)
        position = read_base(block) if block else read_base(puppet.components)
        return position[:3] if position else None

    def create_placed(self, game_uid, solver, name, properties, block):
        srv = self.server
        actors = srv.placed.setdefault(self.area, {})
        existing = actors.get(game_uid) if game_uid is not None else None
        if existing is not None:  # SyncedActorClient: the existing actor, without our solver
            self.send(S["SERVER_ACTOR_SYNC_CREATE"], existing.create_payload())
            return
        components = FACTORIES.get(properties, FACTORIES["Default"])
        actor = PlacedActor(srv.next_uid, game_uid, name, properties, read_components(block, components))
        srv.next_uid += 1
        actor.owner = self if self.puppet() else None
        actors[actor.key] = actor
        if len(actors) % 50 == 1:
            self.log(f"area {self.area!r}: {len(actors)} placed actors (last {properties} uid={actor.uid})")
        self.send(S["SERVER_ACTOR_SYNC_CREATE"], actor.create_payload(solver))
        for peer in srv.peers(self):
            peer.send(S["SERVER_ACTOR_SYNC_CREATE"], actor.create_payload())

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
                for actor in srv.placed.get(self.area, {}).values():  # SyncedActorManager.addClient
                    if actor.game_uid is None:
                        self.send(S["SERVER_ACTOR_SYNC_CREATE"], actor.create_payload())
                for peer in srv.peers(self):  # existing players of the area
                    for actor in peer.actors.values():
                        self.send(S["SERVER_ACTOR_SYNC_CREATE"], actor.create_payload())
                        # position and ActorPlayerSync (colours) are separate updates: send both
                        for block in actor.last_updates.values():
                            self.send(S["SERVER_ACTOR_SYNC_UPDATE"], actor.update_payload(block))
            elif packet_id == C["CLIENT_ACTOR_SYNC_CREATE"]:
                game_uid = r.bytes(r.vlq()) if r.byte() else None
                solver = r.vlq()
                name, properties = r.string(), r.string()
                if game_uid is not None or properties == "Spawned":
                    if self.area is not None:
                        self.create_placed(game_uid, solver, name, properties, r.rest())
                    return
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
                ticket = r.byte()
                placed = srv.find_placed(self.area, uid)
                if placed is not None:  # SyncedActorClient.receiveClientActorSyncUpdate
                    if ticket != placed.ticket:
                        return
                    changed = read_components(r.rest(), placed.components)
                    placed.data.update(changed)
                    payload = vlq(uid) + placed.body(sorted(changed))
                    for client in srv.area_clients(self.area):
                        if client is not placed.owner:
                            client.send(S["SERVER_ACTOR_SYNC_UPDATE"], payload)
                    return
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
                uid = r.vlq()
                placed = srv.find_placed(self.area, uid)
                if placed is not None:  # SyncedActorManager.removeActorSync: to every client
                    del srv.placed[self.area][placed.key]
                    self.log(f"remove {placed.properties} uid={uid}")
                    for client in srv.area_clients(self.area):
                        client.send(S["SERVER_ACTOR_SYNC_REMOVE"], vlq(uid))
                    return
                actor = self.actors.pop(uid, None)
                if actor:
                    for peer in srv.peers(self):
                        peer.send(S["SERVER_ACTOR_SYNC_REMOVE"], vlq(actor.uid))
            elif packet_id == C["CLIENT_ACTOR_EXCLUSIVE_ADD"]:
                uid = r.vlq()
                placed = srv.find_placed(self.area, uid)
                if placed is not None and self.puppet():
                    self.log(f"exclusive {placed.properties} uid={uid}")
                    srv.change_owner(placed, self, self.area)
                self.exclusive.add(uid)
            elif packet_id == C["CLIENT_ACTOR_EXCLUSIVE_REMOVE"]:
                self.exclusive.discard(r.vlq())

    def run(self):
        self.log("connected")
        with self.send_lock:
            self.sock.sendall(self.server.init)
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


def console(server, names):
    """'mutators N 0|1' on the standard input: the mutators change live (see the module doc)."""
    for line in sys.stdin:
        words = line.split()
        if len(words) == 3 and words[0] == "mutators" and words[1].isdigit():
            multiplier = max(1, min(15, int(words[1])))
            randomizer = words[2] in ("1", "true")
            payload = vlq(multiplier) + bytes([1 if randomizer else 0])
            with server.lock:
                server.init = init_blob(names, multiplier, randomizer)
                for client in server.clients:
                    client.send(S["SERVER_MUTATORS"], payload)
            print(f"mutators changed: enemy multiplier {multiplier}"
                  f"{', randomizer' if randomizer else ''}", flush=True)
        elif words:
            print("unknown command (mutators N 0|1)", flush=True)


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
    parser.add_argument("--multiplier", type=int, default=1, choices=range(1, 16), metavar="1..15",
                        help="enemy multiplier sent to the clients")
    parser.add_argument("--randomizer", action="store_true",
                        help="the enemy copies take random variants")
    parser.add_argument("--actor-list", default=DEFAULT_ACTOR_LIST,
                        help="ActorFilter names (actorList.ktml)")
    args = parser.parse_args()
    names = read_actor_list(args.actor_list) if os.path.exists(args.actor_list) else []
    print(f"{len(names)} actor names to synchronise, enemy multiplier {args.multiplier}"
          f"{', randomizer' if args.randomizer else ''}", flush=True)
    server = Server(args.bot, args.mirror, args.ship, args.swing, names, args.multiplier, args.randomizer)
    threading.Thread(target=server.owner_loop, daemon=True).start()
    listener = socket.create_server((args.host, args.port))
    print(f"fake multiplayer server on {args.host}:{args.port}{' with bot' if args.bot else ''}",
          flush=True)
    if args.bot:
        threading.Thread(target=server.bot_loop, daemon=True).start()
    threading.Thread(target=console, args=(server, names), daemon=True).start()
    while True:
        sock, address = listener.accept()
        sock.setsockopt(socket.IPPROTO_TCP, socket.TCP_NODELAY, 1)
        client = Client(server, sock, address)
        with server.lock:
            server.clients.append(client)
        threading.Thread(target=client.run, daemon=True).start()


if __name__ == "__main__":
    main()
