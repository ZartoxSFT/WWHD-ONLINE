#!/usr/bin/env python3
"""Make a multiplayer skin (another Link model) from a Link model mod, on the player's machine.

A Link model mod for WWHD replaces Link.bfres inside Link.szs, itself inside
Common/Pack/permanent_3d.pack (checked on the Miku pack: the rest of the pack is the game's own
bytes). Installed as a content mod it would replace Link for everybody on that machine, the
remote players' puppets included. The multiplayer mod loads it instead as an archive of its own,
next to "Link", and gives it only to the Links of the players who chose it (skin.h):

  - the archive is Object/MpSk_<id>.szs for the game; the runtime reads it from
    <user>/ModManager/Data/multiplayer-connection/skin_<id>.szs (runtime/src/mods/mp_skins.cpp);
  - a model archive's bfres is named after the archive (hd_res_model.cpp, "%s.bfres"), so
    Link.bfres becomes MpSk_<id>.bfres; the shader archives (<model>.sharcfb) keep their names;
  - the game finds the archive's resources (the indices Link's code asks for) through the table
    of "Link", which the mod hands to it for these archives (LoadTableResources, skin.h).

The output is a Yaz0 SARC without compression (like tools/story_mods/notebook_model.py). Nothing
of the game is stored in the repository: the source is the player's own mod or game copy.

usage: make_skin.py SOURCE --id ID --out DIR [--game GAME_DIR]
  SOURCE   a model mod folder (Cemu graphic pack, SDCafiine, content/ tree), its
           permanent_3d.pack, its Link.szs, or a Link.bfres (then --game gives the shaders)
  --id     the skin's name for the other players: 1 to 12 of a-z 0-9 _
  --out    the folder to write skin_<id>.szs into (the mod's Data folder), or a .szs file
"""
import argparse
import os
import re
import struct
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.dirname(HERE))
import shaderprep  # noqa: E402  (Yaz0 and SARC readers)

ID_PATTERN = re.compile(r"^[a-z0-9_]{1,12}$")
ARCHIVE_PREFIX = "MpSk_"
PACK = "permanent_3d.pack"


class SkinError(Exception):
    pass


def archive_name(skin_id):
    return ARCHIVE_PREFIX + skin_id


def read_archive(data):
    if data[:4] == b"Yaz0":
        data = shaderprep.yaz0(data)
    if data[:4] != b"SARC":
        raise SkinError("not a SARC archive")
    return dict(shaderprep.sarc(data))


def find_source(path):
    """The file to read: a pack, a Link.szs or a Link.bfres, searched in a folder."""
    if os.path.isfile(path):
        return path
    if not os.path.isdir(path):
        raise SkinError("source not found: " + path)
    found = {}
    for root, _, files in os.walk(path):
        for name in files:
            key = name.lower()
            if key in (PACK, "link.szs", "link.bfres") and key not in found:
                found[key] = os.path.join(root, name)
    for key in (PACK, "link.szs", "link.bfres"):
        if key in found:
            return found[key]
    raise SkinError("no %s, Link.szs or Link.bfres in %s" % (PACK, path))


def link_files(source, game=None):
    """The files of the model mod's Link.szs (Link.bfres and the shader archives)."""
    name = os.path.basename(source).lower()
    with open(source, "rb") as fh:
        data = fh.read()
    if name.endswith(".pack"):
        pack = read_archive(data)
        if "Link.szs" not in pack:
            raise SkinError("no Link.szs in " + source)
        files = read_archive(pack["Link.szs"])
    elif name.endswith(".szs"):
        files = read_archive(data)
    elif name.endswith(".bfres"):
        if not game:
            raise SkinError("a Link.bfres alone needs --game (the shaders of the game's Link.szs)")
        pack_path = os.path.join(game, "content", "Common", "Pack", PACK)
        if not os.path.isfile(pack_path):
            raise SkinError("game pack not found: " + pack_path)
        with open(pack_path, "rb") as fh:
            files = read_archive(read_archive(fh.read())["Link.szs"])
        files["Link.bfres"] = data
    else:
        raise SkinError("unknown source type: " + source)
    if "Link.bfres" not in files:
        raise SkinError("no Link.bfres in the source's Link.szs")
    if files["Link.bfres"][:4] != b"FRES":
        raise SkinError("Link.bfres is not a BFRES file")
    if not any(n.endswith(".sharcfb") for n in files):
        raise SkinError("no shader archive (.sharcfb) next to Link.bfres")
    return files


# ---- output: Yaz0 SARC (the writer of tools/story_mods/notebook_model.py) ----

def sarc(files, align=0x2000):
    def name_hash(name):
        h = 0
        for c in name.encode():
            h = (h * 0x65 + c) & 0xFFFFFFFF
        return h
    entries = sorted(files.items(), key=lambda kv: name_hash(kv[0]))
    names, offs = b"", []
    for name, _ in entries:
        offs.append(len(names))
        names += name.encode() + b"\0"
        names += b"\0" * (-len(names) % 4)
    sfat = struct.pack(">4sHHI", b"SFAT", 0xC, len(entries), 0x65)
    data, spans = bytearray(), []
    for name, body in entries:
        data += b"\0" * (-len(data) % align)
        spans.append((len(data), len(data) + len(body)))
        data += body
    for (name, _), noff, (s, e) in zip(entries, offs, spans):
        sfat += struct.pack(">IIII", name_hash(name), 0x01000000 | (noff // 4), s, e)
    sfnt = struct.pack(">4sHH", b"SFNT", 8, 0) + names
    head_len = 0x14 + len(sfat) + len(sfnt)
    doff = (head_len + align - 1) // align * align
    total = doff + len(data)
    head = struct.pack(">4sHHIIHH", b"SARC", 0x14, 0xFEFF, total, doff, 0x0100, 0)
    return head + sfat + sfnt + b"\0" * (doff - head_len) + bytes(data)


def yaz0_stored(data):
    """Yaz0 without compression (every group of eight bytes stored as literals): valid for any reader."""
    out = bytearray(b"Yaz0" + struct.pack(">I", len(data)) + b"\0" * 8)
    for i in range(0, len(data), 8):
        out.append(0xFF)
        out += data[i:i + 8]
    return bytes(out)


def build(source, skin_id, out, game=None):
    if not ID_PATTERN.match(skin_id):
        raise SkinError("skin id must be 1 to 12 of a-z 0-9 _: " + skin_id)
    files = link_files(find_source(source), game)
    renamed = {}
    for name, body in files.items():
        renamed[archive_name(skin_id) + ".bfres" if name == "Link.bfres" else name] = body
    archive = yaz0_stored(sarc(renamed))
    if os.path.isdir(out) or not out.lower().endswith(".szs"):
        os.makedirs(out, exist_ok=True)
        out = os.path.join(out, "skin_%s.szs" % skin_id)
    tmp = out + ".tmp"
    with open(tmp, "wb") as fh:
        fh.write(archive)
    os.replace(tmp, out)
    return out, sorted(renamed)


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    ap.add_argument("source")
    ap.add_argument("--id", required=True)
    ap.add_argument("--out", required=True)
    ap.add_argument("--game")
    args = ap.parse_args()
    try:
        path, names = build(args.source, args.id, args.out, args.game)
    except (SkinError, OSError, KeyError, struct.error) as error:
        print("make_skin: %s" % error, file=sys.stderr)
        return 1
    print("make_skin: %s (%d files, archive %s)" % (path, len(names), archive_name(args.id)))
    return 0


if __name__ == "__main__":
    sys.exit(main())
