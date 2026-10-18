#!/usr/bin/env python3
"""make_gc_skin.py and j3d.py checks that need no game files (the Among Us Link.arc when present)."""
import os
import struct
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import j3d  # noqa: E402
import make_gc_skin  # noqa: E402


def test_material_names():
    assert make_gc_skin.hd_material_name("ear(2)") == "ear_2_"
    assert make_gc_skin.hd_material_name("ear(12)") == "ear_12_"
    assert make_gc_skin.hd_material_name("face") == "face"


def test_matrices():
    m = j3d.srt((1.0, 2.0, 1.0), (0.3, -1.1, 2.0), (5.0, -3.0, 7.0))
    p = (1.5, -2.0, 4.0)
    q = j3d.apply(j3d.inverse(m), j3d.apply(m, p))
    assert all(abs(a - b) < 1e-9 for a, b in zip(p, q))
    rz = j3d.srt((1, 1, 1), (0, 0, 3.141592653589793 / 2), (0, 0, 0))
    assert all(abs(a - b) < 1e-9 for a, b in zip(j3d.apply(rz, (1, 0, 0)), (0, 1, 0)))


def test_joint_names():
    class Fake:
        joints = [type("J", (), {"name": n})() for n in ("skeleton_root_world_root", "skeleton_root_cl_LhandB", "extra")]
        joint_parent = {1: 0, 2: 1}
    conv = make_gc_skin.Converter.__new__(make_gc_skin.Converter)
    conv.notes = []
    bones = {"world_root": (0, -1), "cl_LhandB": (1, 0), "root": (2, -1)}
    joint_bone, bone_joint = conv.map_joints("hands", Fake(), bones)
    assert joint_bone[0] == (0, -1) and joint_bone[1] == (1, 0)  # the longest HD name the joint ends with
    assert joint_bone[2] == (1, 0) and len(conv.notes) == 1      # unknown: its parent's bone
    assert bone_joint[1] == 1                                    # the named joint, not the follower


def test_rest_position():
    class Fake:
        skin = {0: j3d.srt((1, 1, 1), (0, 0, 0), (0, 10, 0))}
    p, n = j3d.Model.rest_position(Fake(), [(0, 0.5), (1, 0.5)], (1.0, 0.0, 0.0), (0.0, 1.0, 0.0))
    assert p == (1.0, 5.0, 0.0) and abs(n[1] - 1.0) < 1e-12


def test_cmpr():
    # one 8x8 CMPR block: four DXT1 sub-blocks, colour 0 red, colour 1 blue, every texel index 1
    sub = struct.pack(">HHI", 0xF800, 0x001F, 0x55555555)
    px = j3d.decode_texture(sub * 4, 14, 8, 8)
    assert px[0] == (0, 0, 255, 255) and px[63] == (0, 0, 255, 255)


def test_ia8_and_rgb5a3():
    px = j3d.decode_texture(bytes([0x80, 0x40] * 16), 3, 4, 4)
    assert px[0] == (0x40, 0x40, 0x40, 0x80)
    assert j3d._rgb5a3(0xFFFF) == (255, 255, 255, 255)
    assert j3d._rgb5a3(0x7000)[3] == 255


def test_among_us():
    arc_path = os.path.join(os.path.dirname(HERE), "skin", "AmongUs", "Link.arc")
    if not os.path.isfile(arc_path):
        print("skip: no tools/skin/AmongUs/Link.arc")
        return
    with open(arc_path, "rb") as fh:
        arc = j3d.rarc_files(fh.read())
    m = j3d.Model(arc["bdl/cl.bdl"])
    assert len(m.joints) == 42 and m.joints[15].name == "head_jnt"
    assert len(m.materials) == 24 and len(m.shapes) == 24
    names = {make_gc_skin.hd_material_name(m.materials[m.shape_material[s["index"]]]["name"]) for s in m.shapes}
    assert "ear_2_" in names and "face" in names and "sleeve" in names
    tris = sum(len(m.triangles(s)) for s in m.shapes)
    assert tris > 500
    w, h, px = m.texture_rgba(next(i for i, t in enumerate(m.textures) if t["name"] == "linktexS3TC"))
    assert (w, h) == (160, 96) and len(px) == w * h
    # the world matrix of link_root is its translation
    assert abs(m.world[0][1][3] - 45.0) < 1e-6


if __name__ == "__main__":
    for name, fn in sorted(globals().items()):
        if name.startswith("test_"):
            fn()
            print("ok", name)
