#!/usr/bin/env python3
"""Make a multiplayer skin from a GameCube Link model (Wind Waker Randomizer format), on the player's machine.

A GameCube custom player model is a Link.arc (RARC of J3D models: cl.bdl for the body, hands.bdl,
the items...) that keeps the original skeleton, materials and shape order, with metadata.txt and
colour masks next to it. The HD Link.bfres holds the same 54 models under the same names, converted
from the GameCube ones: cl keeps the 42 joints and the 24 materials, one HD shape per GameCube
material (shape "<bone>__<material>", "ear(2)" becoming "ear_2_"), positions in model space for the
skinned shapes and in their bone's space for the rigid ones, the same UVs, the triangles' winding
reversed (checked on the vanilla bow of a Link.arc: every vertex and triangle matches).

The player's own Link.szs is the template: the shaders, materials, skeleton and every model the
GameCube archive leaves as it was stay the game's. For each converted model, each shape's vertex and
index buffers are replaced by the GameCube shape of the same material (appended, offsets and counts
updated, same vertex layout and skin count), and the textures the model's materials name are
replaced by the GameCube ones (uncompressed RGBA8, linear aligned, like
tools/story_mods/notebook_model.py; the channel selectors of the game's texture are kept). The output
is the skin archive of make_skin.py (skin_<id>.szs, Link.bfres renamed MpSk_<id>.bfres).

A model is converted when its GameCube vertices differ from the HD ones (the HD bind pose and the
GameCube one are the same); the vanilla ones keep the HD model and textures. --models forces a list.

usage: make_gc_skin.py SOURCE --id ID --out DIR --game GAME_DIR [--models cl,hands]
  SOURCE   the skin folder (with Link.arc) or the Link.arc itself
"""
import argparse
import math
import os
import struct
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import j3d  # noqa: E402
import make_skin  # noqa: E402

BODY_MODELS = ("cl", "hands", "katsura")
KEEP_TEXTURES = {"ZBtoonEX"}  # the HD shaders' toon ramp
FMT_RGBA8, SRGB, TILE_LINEAR_ALIGNED = 0x1A, 0x400, 1
ATTR_FORMATS = {0x80D: "2f", 0x811: "3f", 0x813: "4f", 0x10C: "2I", 0x110: "3I", 0x112: "4I"}
SKINNED_EXTENT = 200.0


class GcSkinError(Exception):
    pass


def hd_material_name(gc_name):
    if gc_name.endswith(")") and "(" in gc_name:
        base, _, n = gc_name[:-1].rpartition("(")
        return "%s_%s_" % (base, n)
    return gc_name


class Bfres:
    def __init__(self, data):
        self.b = bytearray(data)
        if self.b[:4] != b"FRES" or self.u32(4) != 0x03040001:
            raise GcSkinError("Link.bfres is not the expected BFRES 3.4.0.1")

    def u32(self, o): return struct.unpack_from(">I", self.b, o)[0]
    def s32(self, o): return struct.unpack_from(">i", self.b, o)[0]
    def u16(self, o): return struct.unpack_from(">H", self.b, o)[0]
    def s16(self, o): return struct.unpack_from(">h", self.b, o)[0]
    def put(self, fmt, o, *v): struct.pack_into(">" + fmt, self.b, o, *v)

    def rel(self, o):
        v = self.s32(o)
        return o + v if v else 0

    def set_rel(self, o, target): self.put("i", o, target - o)
    def cstr(self, o): return self.b[o:self.b.index(b"\0", o)].decode()

    def dict(self, o):
        return [(self.cstr(self.rel(o + 8 + 16 * i + 8)), self.rel(o + 8 + 16 * i + 12))
                for i in range(1, self.s32(o + 4) + 1)] if o else []

    def append(self, data, align):
        while len(self.b) % align:
            self.b.append(0)
        at = len(self.b)
        self.b += data
        return at


def gc_vertex(model, v, uv_attr):
    """(model-space position, model-space normal, uv, [(joint, weight)], rigid joint or None)."""
    infl, model_space = model.influences(v["drw"])
    p = model.arrays[j3d.ATTR_POS][v[j3d.ATTR_POS]]
    n = model.arrays[j3d.ATTR_NRM][v[j3d.ATTR_NRM]] if j3d.ATTR_NRM in v else (0.0, 1.0, 0.0)
    uv = model.arrays[uv_attr][v[uv_attr]] if uv_attr in v else (0.0, 0.0)
    if not model_space:
        w = model.world[infl[0][0]]
        p, n = j3d.apply(w, p), j3d.rotate(w, n)
    else:
        p, n = model.rest_position(infl, p, n)
    return p, n, uv, infl


class Converter:
    def __init__(self, bfres):
        self.f = bfres
        self.notes = []

    def model(self, name):
        models = dict(self.f.dict(self.f.rel(0x20)))
        if name not in models:
            raise GcSkinError("Link.bfres has no model " + name)
        return models[name]

    def skeleton(self, fm):
        f = self.f
        sk = f.rel(fm + 0xC)
        bones = {}
        arr = f.rel(sk + 0x14)
        for i in range(f.u16(sk + 8)):
            bo = arr + 0x40 * i
            bones[f.cstr(f.rel(bo))] = (f.u16(bo + 4), f.s16(bo + 8))
        return bones

    def map_joints(self, name, gc, bones):
        """GameCube joint -> (HD bone index, smooth index): same name, or a name ending with "_" + the HD
        name (exporters prefix them: skeleton_root_cl_LhandB), else the parent's bone (a note)."""
        joint_bone, exact = {}, set()
        root = next(b for b in bones.values() if b[0] == 0)
        for ji, joint in enumerate(gc.joints):
            target = joint.name if joint.name in bones else None
            if target is None:
                for hd in bones:
                    if joint.name.endswith("_" + hd) and (target is None or len(hd) > len(target)):
                        target = hd
            if target is not None:
                joint_bone[ji] = bones[target]
                exact.add(ji)
                continue
            parent = gc.joint_parent.get(ji)
            joint_bone[ji] = joint_bone.get(parent, root) if parent is not None else root
            self.notes.append("%s: joint %s is not in the HD skeleton, it follows its parent" % (name, joint.name))
        bone_joint = {}
        for ji in sorted(exact) + [j for j in range(len(gc.joints)) if j not in exact]:
            bone_joint.setdefault(joint_bone[ji][0], ji)
        return joint_bone, bone_joint

    def material_textures(self, mt):
        f = self.f
        tr = f.rel(mt + 0x28)
        return [f.cstr(f.rel(tr + 8 * k)) for k in range(f.b[mt + 0x10])]

    def is_vanilla(self, name, gc):
        """True when every GameCube vertex is an HD vertex of the model (same bind pose)."""
        f = self.f
        fm = self.model(name)
        hd = set()
        fva = f.rel(fm + 0x10)
        for _sn, sh in f.dict(f.rel(fm + 0x14)):
            fv = fva + 0x20 * f.u16(sh + 0x12)
            layout, stride, data = self.layout(fv)
            off = layout["_p0"][0]
            for i in range(f.u32(fv + 8)):
                hd.add(tuple(round(x, 1) for x in struct.unpack_from(">3f", f.b, data + i * stride + off)))
        total = found = 0
        for shape in gc.shapes:
            for tri in gc.triangles(shape):
                for v in tri:
                    p = gc_vertex(gc, v, j3d.ATTR_TEX0)[0]
                    total += 1
                    found += tuple(round(x, 1) for x in p) in hd
        return total and found >= 0.95 * total

    def layout(self, fv):
        f = self.f
        at, bufs = f.rel(fv + 0x10), f.rel(fv + 0x18)
        if f.b[fv + 5] != 1:
            raise GcSkinError("vertex data in several buffers is not supported")
        layout = {}
        for a in range(f.b[fv + 4]):
            e = at + 12 * a
            layout[f.cstr(f.rel(e))] = (f.u16(e + 6), f.u32(e + 8))
        return layout, f.u16(bufs + 0xC), f.rel(bufs + 0x14)

    def convert_model(self, name, gc):
        """Converts one model; returns (GameCube texture indices, {HD texture name: GameCube index}) for
        replace_textures (the second: a texture the GameCube material renamed, by its place)."""
        f = self.f
        fm = self.model(name)
        bones = self.skeleton(fm)
        joint_bone, bone_joint = self.map_joints(name, gc, bones)
        mats = f.dict(f.rel(fm + 0x18))
        mat_index = {mn: k for k, (mn, _) in enumerate(mats)}
        hd_shapes = []
        for sn, sh in f.dict(f.rel(fm + 0x14)):
            mn, mt = mats[f.u16(sh + 0xE)]
            hd_shapes.append((sn, sh, mn, self.material_textures(mt), f.b[sh + 0x16]))
        by_material, roles = {}, {}
        for shape in gc.shapes:
            gm = gc.materials[gc.shape_material[shape["index"]]]
            target = hd_material_name(gm["name"])
            gc_textures = [gc.textures[t]["name"] for t in gm["textures"]]
            if target in mat_index:
                hd_textures = self.material_textures(mats[mat_index[target]][1])
                for hd_tex, gi in zip(hd_textures, gm["textures"]):
                    if hd_tex != gc.textures[gi]["name"]:
                        roles.setdefault(hd_tex, gi)
            else:
                first = gc_textures[0] if gc_textures else None
                best = None
                for sn, sh, mn, textures, skin in hd_shapes:
                    if first is not None and textures and textures[0] == first and (best is None or skin > best[1]):
                        best = (mn, skin)
                if best is None:
                    self.notes.append("%s: material %s has no HD place, its shape is left out" % (name, gm["name"]))
                    continue
                self.notes.append("%s: material %s is drawn as %s" % (name, gm["name"], best[0]))
                target = best[0]
            by_material.setdefault(target, []).append(shape)
        fva = f.rel(fm + 0x10)
        total_verts = 0
        for sn, sh, mn, _textures, _skin in hd_shapes:
            fv = fva + 0x20 * f.u16(sh + 0x12)
            total_verts += self.convert_shape(name, sn, sh, fv, gc, by_material.get(mn, []), joint_bone, bone_joint, bones)
        f.put("I", fm + 0x28, total_verts)
        textures = set()
        for mat in gc.materials:
            textures.update(mat["textures"])
        return textures, roles

    def convert_shape(self, model_name, sn, sh, fv, gc, shapes, joint_bone, bone_joint, bones):
        f = self.f
        layout, stride, old_data = self.layout(fv)
        for attr in ("_p0", "_n0", "_u0"):
            if attr not in layout:
                raise GcSkinError("%s/%s: no %s attribute" % (model_name, sn, attr))
        skin = f.b[sh + 0x16]
        shape_bone = f.u16(sh + 0x10)
        colour = None
        if "_c0" in layout:
            off, fmt = layout["_c0"]
            colour = struct.unpack_from(">" + ATTR_FORMATS[fmt], f.b, old_data + off)
        rigid_world_inv = None
        if skin == 0 and shapes:
            if shape_bone not in bone_joint:
                raise GcSkinError("%s/%s: bone %d has no GameCube joint" % (model_name, sn, shape_bone))
            rigid_world_inv = j3d.inverse(gc.world[bone_joint[shape_bone]])
        verts, index, tris = [], {}, []
        reduced = moved = 0
        for shape in shapes:
            for tri in gc.triangles(shape):
                ids = []
                for v in tri:
                    key = (shape["index"], v.get(j3d.ATTR_POS), v.get(j3d.ATTR_NRM), v.get(j3d.ATTR_TEX0), v["drw"])
                    if key not in index:
                        p, n, uv, infl = gc_vertex(gc, v, j3d.ATTR_TEX0)
                        if skin == 0:
                            if len(infl) != 1 or joint_bone[infl[0][0]][0] != shape_bone:
                                moved += 1
                            p, n = j3d.apply(rigid_world_inv, p), j3d.rotate(rigid_world_inv, n)
                            weights = []
                        else:
                            weights = []
                            for joint, w in infl:
                                smooth = joint_bone[joint][1]
                                parent = joint
                                while smooth < 0 and gc.joint_parent.get(parent) is not None:
                                    parent = gc.joint_parent[parent]
                                    smooth = joint_bone[parent][1]
                                if smooth < 0:
                                    raise GcSkinError("%s: joint %s has no skinning matrix" % (model_name, gc.joints[joint].name))
                                if parent != joint:
                                    moved += 1
                                weights.append((smooth, w, joint_bone[parent][0]))
                            weights.sort(key=lambda x: -x[1])
                            if len(weights) > skin:
                                reduced += 1
                                weights = weights[:skin]
                            s = sum(w for _, w, _ in weights) or 1.0
                            weights = [(i, w / s, b) for i, w, b in weights]
                        index[key] = len(verts)
                        verts.append((p, n, uv, weights))
                    ids.append(index[key])
                tris.append((ids[0], ids[2], ids[1]))
        if not verts:
            # nothing of the GameCube model for this HD shape: one degenerate triangle, drawn as nothing
            weights = []
            if skin:
                smooth = next((b for b in bones.values() if b[1] >= 0), None)
                if smooth is None:
                    raise GcSkinError("%s: the HD skeleton has no skinning matrix" % model_name)
                weights = [(smooth[1], 1.0, smooth[0])]
            verts.append(((0.0, 0.0, 0.0), (0.0, 1.0, 0.0), (0.0, 0.0), weights))
            tris.append((0, 0, 0))
            self.notes.append("%s/%s: no GameCube shape for it, emptied" % (model_name, sn))

        data = bytearray(stride * len(verts))
        used_bones = set()
        for k, (p, n, uv, weights) in enumerate(verts):
            base = k * stride
            vals = {"_p0": p, "_n0": n, "_u0": uv}
            if colour is not None:
                vals["_c0"] = colour
            if skin:
                pad = weights + [(weights[0][0], 0.0, weights[0][2])] * (skin - len(weights))
                vals["_i0"] = [i for i, _, _ in pad]
                vals["_w0"] = [w for _, w, _ in pad]
                used_bones.update(b for _, w, b in weights)
            for attr, (off, fmt) in layout.items():
                if attr not in vals:
                    raise GcSkinError("%s/%s: attribute %s is not supported" % (model_name, sn, attr))
                code = ATTR_FORMATS.get(fmt)
                if code is None:
                    raise GcSkinError("%s/%s: attribute format 0x%X is not supported" % (model_name, sn, fmt))
                count = int(code[0])
                value = list(vals[attr])[:count]
                value += [1.0 if (attr == "_n0" or attr == "_c0") else 0.0] * (count - len(value))
                if code[1] == "I":
                    value = [int(x) for x in value]
                struct.pack_into(">" + code, data, base + off, *value)
        bufs = f.rel(fv + 0x18)
        at = f.append(bytes(data), 0x1000)
        f.put("I", bufs + 4, len(data))
        f.set_rel(bufs + 0x14, at)
        f.put("I", fv + 8, len(verts))

        mesh = f.rel(sh + 0x24)
        if f.u32(mesh + 4) != 9:
            raise GcSkinError("%s/%s: index format %d is not supported" % (model_name, sn, f.u32(mesh + 4)))
        flat = [i for t in tris for i in t]
        idata = struct.pack(">%dI" % len(flat), *flat)
        ibuf = f.rel(mesh + 0x14)
        at = f.append(idata, 0x1000)
        f.put("I", ibuf + 4, len(idata))
        f.set_rel(ibuf + 0x14, at)
        f.put("I", mesh + 8, len(flat))
        f.put("H", mesh + 0xC, 1)
        f.put("II", f.rel(mesh + 0x10), 0, len(flat))
        f.put("I", mesh + 0x18, 0)

        if skin:
            bones = sorted(used_bones)
            if len(bones) > f.u16(sh + 0x14):
                at = f.append(struct.pack(">%dH" % len(bones), *bones), 4)
                f.set_rel(sh + 0x28, at)
            else:
                f.put("%dH" % len(bones), f.rel(sh + 0x28), *bones)
            f.put("H", sh + 0x14, len(bones))
            centre, extent = (0.0, 0.0, 0.0), (SKINNED_EXTENT,) * 3
        else:
            lo = [min(v[0][k] for v in verts) for k in range(3)]
            hi = [max(v[0][k] for v in verts) for k in range(3)]
            centre = tuple((lo[k] + hi[k]) / 2 for k in range(3))
            extent = tuple((hi[k] - lo[k]) / 2 + 1.0 for k in range(3))
        box = f.rel(sh + 0x34)
        f.put("3f", box, *centre)
        f.put("3f", box + 12, *extent)
        f.put("f", sh + 0x1C, math.sqrt(sum(e * e for e in extent)))
        if reduced:
            self.notes.append("%s/%s: %d vertices kept their %d strongest weights" % (model_name, sn, reduced, skin))
        if moved:
            self.notes.append("%s/%s: %d vertices moved to another bone" % (model_name, sn, moved))
        return len(verts)

    def replace_textures(self, gc, indices, roles):
        """The materials' textures and their animation frames (eyeh.1 -> eyeh.2...), the toon ramp
        excepted; roles: HD textures whose GameCube material names another texture in their place."""
        textures = dict(self.f.dict(self.f.rel(0x24)))
        source = {}
        for i in sorted(indices):
            source.setdefault(gc.textures[i]["name"], i)
        bases = {name.split(".")[0] for name in source}
        for i, t in enumerate(gc.textures):
            if t["name"].split(".")[0] in bases:
                source.setdefault(t["name"], i)
        for name, i in sorted(roles.items()):
            source.setdefault(name, i)
        done = []
        for name, i in sorted(source.items()):
            if name in KEEP_TEXTURES or name not in textures:
                continue
            self.put_texture(textures[name], *gc.texture_rgba(i))
            done.append(name)
        return done

    def put_texture(self, t, w, h, rgba):
        f = self.f
        surf = t + 4
        fmt = f.u32(surf + 0x14)
        sel = list(f.b[t + 0x88:t + 0x8C])
        pitch = (w + 63) // 64 * 64
        data = bytearray(pitch * h * 4)
        for y in range(h):
            for x in range(w):
                want = rgba[y * w + x]
                src = [0, 0, 0, 255]
                filled = [False] * 4
                for c in range(4):
                    k = sel[c]
                    if k < 4 and not filled[k]:
                        src[k] = want[c]
                        filled[k] = True
                o = (y * pitch + x) * 4
                data[o:o + 4] = bytes(src)
        at = f.append(bytes(data), 0x2000)
        f.put("I", surf + 0x04, w)
        f.put("I", surf + 0x08, h)
        f.put("I", surf + 0x10, 1)
        f.put("I", surf + 0x14, FMT_RGBA8 | (fmt & SRGB))
        f.put("I", surf + 0x20, len(data))
        f.put("I", surf + 0x28, 0)
        f.put("I", surf + 0x30, TILE_LINEAR_ALIGNED)
        f.put("I", surf + 0x34, 0)
        f.put("I", surf + 0x38, 0x100)
        f.put("I", surf + 0x3C, pitch)
        for k in range(13):
            f.put("I", surf + 0x40 + 4 * k, 0)
        f.put("I", t + 0x7C, 1)
        for k in range(5):
            f.put("I", t + 0x8C + 4 * k, 0)
        f.set_rel(t + 0xB0, at)
        f.put("i", t + 0xB4, 0)


def find_arc(source):
    if os.path.isfile(source):
        return source
    for root, _, files in os.walk(source):
        for name in files:
            if name.lower() == "link.arc":
                return os.path.join(root, name)
    raise GcSkinError("no Link.arc in " + source)


def build(source, skin_id, out, game, models=None):
    if not make_skin.ID_PATTERN.match(skin_id):
        raise GcSkinError("skin id must be 1 to 12 of a-z 0-9 _: " + skin_id)
    pack_path = os.path.join(game, "content", "Common", "Pack", make_skin.PACK)
    if not os.path.isfile(pack_path):
        raise GcSkinError("game pack not found: " + pack_path)
    with open(pack_path, "rb") as fh:
        files = make_skin.read_archive(make_skin.read_archive(fh.read())["Link.szs"])
    with open(find_arc(source), "rb") as fh:
        arc = j3d.rarc_files(fh.read())
    conv = Converter(Bfres(files["Link.bfres"]))
    gc_models = {path.rsplit("/", 1)[-1][:-4]: path for path in arc if path.endswith(".bdl") or path.endswith(".bmd")}
    if models is None:
        chosen = []
        for name in BODY_MODELS:
            if name in gc_models and not conv.is_vanilla(name, j3d.Model(arc[gc_models[name]])):
                chosen.append(name)
    else:
        chosen = list(models)
    if not chosen:
        raise GcSkinError("the GameCube model changes none of " + ", ".join(BODY_MODELS))
    replaced = []
    for name in chosen:
        if name not in gc_models:
            raise GcSkinError("Link.arc has no model " + name)
        gc = j3d.Model(arc[gc_models[name]])
        indices, roles = conv.convert_model(name, gc)
        replaced += conv.replace_textures(gc, indices, roles)
    f = conv.f
    while len(f.b) % 0x1000:
        f.b.append(0)
    f.put("I", 0xC, len(f.b))
    renamed = {}
    for name, body in files.items():
        if name == "Link.bfres":
            renamed[make_skin.archive_name(skin_id) + ".bfres"] = bytes(f.b)
        else:
            renamed[name] = body
    archive = make_skin.yaz0_stored(make_skin.sarc(renamed))
    if os.path.isdir(out) or not out.lower().endswith(".szs"):
        os.makedirs(out, exist_ok=True)
        out = os.path.join(out, "skin_%s.szs" % skin_id)
    tmp = out + ".tmp"
    with open(tmp, "wb") as fh:
        fh.write(archive)
    os.replace(tmp, out)
    return out, chosen, sorted(set(replaced)), conv.notes


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    ap.add_argument("source")
    ap.add_argument("--id", required=True)
    ap.add_argument("--out", required=True)
    ap.add_argument("--game", required=True, help="the game folder (with content/)")
    ap.add_argument("--models", help="comma-separated models to convert (default: the body models it changes)")
    args = ap.parse_args()
    try:
        path, models, textures, notes = build(args.source, args.id, args.out, args.game,
                                              args.models.split(",") if args.models else None)
    except (GcSkinError, j3d.J3DError, make_skin.SkinError, OSError, KeyError, IndexError, struct.error) as error:
        print("make_gc_skin: %s" % error, file=sys.stderr)
        return 1
    for note in notes:
        print("make_gc_skin: " + note)
    print("make_gc_skin: %s (models %s, %d textures)" % (path, ", ".join(models), len(textures)))
    return 0


if __name__ == "__main__":
    sys.exit(main())
