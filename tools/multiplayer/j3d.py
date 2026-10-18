"""GameCube RARC archives and J3D models (BDL/BMD): what make_gc_skin.py needs from a Link.arc.

Read: the joints and their bind pose, the envelopes and draw matrices, the vertex arrays, the
shapes' display lists, the materials' names and textures, and the BTI textures decoded to RGBA.
"""
import math
import struct


class J3DError(Exception):
    pass


def be(fmt, b, o):
    return struct.unpack_from(">" + fmt, b, o)


def yaz0(src):
    size = be("I", src, 4)[0]
    dst = bytearray()
    s = 16
    while len(dst) < size:
        code = src[s]
        s += 1
        for bit in range(8):
            if len(dst) >= size:
                break
            if code & (0x80 >> bit):
                dst.append(src[s])
                s += 1
                continue
            b1, b2 = src[s], src[s + 1]
            s += 2
            dist = ((b1 & 0xF) << 8 | b2) + 1
            n = b1 >> 4
            if n == 0:
                n = src[s] + 0x12
                s += 1
            else:
                n += 2
            for _ in range(n):
                dst.append(dst[-dist])
    return bytes(dst)


def rarc_files(d):
    """{"<folder>/<name>": bytes} of a RARC archive (Yaz0 or not)."""
    if d[:4] == b"Yaz0":
        d = yaz0(d)
    if d[:4] != b"RARC":
        raise J3DError("not a RARC archive")
    data_off = be("I", d, 0xC)[0] + 0x20
    nn, fn_off, _ne, fe_off, _st_size, st_off = be("6I", d, 0x20)
    nodes, ents, strs = fn_off + 0x20, fe_off + 0x20, st_off + 0x20

    def name(o):
        return d[strs + o:d.index(b"\0", strs + o)].decode("ascii", "replace")

    out = {}
    for i in range(nn):
        _typ, no, _h, cnt, first = be("4sIHHI", d, nodes + i * 16)
        folder = name(no)
        for j in range(cnt):
            _idx, _hh, attr, no2, doff, dsz = be("HHHHII", d, ents + (first + j) * 20)
            entry = name(no2)
            if entry not in (".", "..") and (attr >> 8) & 1:
                out[folder + "/" + entry] = d[data_off + doff:data_off + doff + dsz]
    return out


# ---- 3x4 matrices ----

def srt(s, r, t):
    """J3D joint transform: T * Rz * Ry * Rx * S."""
    cx, sx = math.cos(r[0]), math.sin(r[0])
    cy, sy = math.cos(r[1]), math.sin(r[1])
    cz, sz = math.cos(r[2]), math.sin(r[2])
    rot = [[cy * cz, sx * sy * cz - cx * sz, cx * sy * cz + sx * sz],
           [cy * sz, sx * sy * sz + cx * cz, cx * sy * sz - sx * cz],
           [-sy, sx * cy, cx * cy]]
    return [[rot[i][0] * s[0], rot[i][1] * s[1], rot[i][2] * s[2], t[i]] for i in range(3)]


def mul(a, b):
    return [[sum(a[i][k] * b[k][j] for k in range(3)) + (a[i][3] if j == 3 else 0.0) for j in range(4)]
            for i in range(3)]


def apply(m, p):
    return tuple(m[i][0] * p[0] + m[i][1] * p[1] + m[i][2] * p[2] + m[i][3] for i in range(3))


def rotate_raw(m, n):
    return tuple(m[i][0] * n[0] + m[i][1] * n[1] + m[i][2] * n[2] for i in range(3))


def rotate(m, n):
    v = tuple(m[i][0] * n[0] + m[i][1] * n[1] + m[i][2] * n[2] for i in range(3))
    length = math.sqrt(sum(x * x for x in v)) or 1.0
    return tuple(x / length for x in v)


def inverse(m):
    """Inverse of an affine 3x4 matrix."""
    a = [row[:3] for row in m]
    det = (a[0][0] * (a[1][1] * a[2][2] - a[1][2] * a[2][1])
           - a[0][1] * (a[1][0] * a[2][2] - a[1][2] * a[2][0])
           + a[0][2] * (a[1][0] * a[2][1] - a[1][1] * a[2][0]))
    if abs(det) < 1e-12:
        raise J3DError("singular joint matrix")
    inv = [[(a[(j + 1) % 3][(i + 1) % 3] * a[(j + 2) % 3][(i + 2) % 3]
             - a[(j + 1) % 3][(i + 2) % 3] * a[(j + 2) % 3][(i + 1) % 3]) / det for j in range(3)] for i in range(3)]
    t = [m[i][3] for i in range(3)]
    return [inv[i] + [-sum(inv[i][k] * t[k] for k in range(3))] for i in range(3)]


# ---- J3D model ----

COMP = {0: ("B", 1), 1: ("b", 1), 2: ("H", 2), 3: ("h", 2), 4: ("f", 4)}  # u8 s8 u16 s16 f32
ATTR_POS, ATTR_NRM, ATTR_CLR0, ATTR_TEX0 = 9, 0xA, 0xB, 0xD
PRIM_TRIANGLES, PRIM_STRIP, PRIM_FAN = 0x90, 0x98, 0xA0


class Joint:
    def __init__(self, name, scale, rot, trans):
        self.name, self.scale, self.rot, self.trans = name, scale, rot, trans


class Model:
    def __init__(self, d):
        if d[:4] != b"J3D2":
            raise J3DError("not a J3D model")
        self.d = d
        self.sec = {}
        o = 0x20
        for _ in range(be("I", d, 0xC)[0]):
            tag, size = be("4sI", d, o)
            self.sec[tag.decode("ascii", "replace")] = o
            o += size
        for need in ("INF1", "VTX1", "DRW1", "JNT1", "SHP1", "MAT3", "TEX1"):
            if need not in self.sec:
                raise J3DError("no %s section" % need)
        self._jnt1()
        self._evp1()
        self._drw1()
        self._vtx1()
        self._tex1()
        self._mat3()
        self._shp1()
        self._inf1()
        self.world = []
        for i, j in enumerate(self.joints):
            local = srt(j.scale, j.rot, j.trans)
            parent = self.joint_parent.get(i)
            self.world.append(local if parent is None else mul(self.world[parent], local))
        # an envelope's matrix at rest: the joint's rest pose times its inverse bind matrix (identity
        # when the model was bound in its rest pose; not for every custom model)
        self.skin = {j: mul(self.world[j], ib) for j, ib in self.inv_bind.items() if j < len(self.world)}

    def rest_position(self, infl, p, n):
        """A weighted vertex (bind space) at the joints' rest pose, as the game skins it there."""
        pos, nrm = [0.0, 0.0, 0.0], [0.0, 0.0, 0.0]
        for joint, w in infl:
            m = self.skin.get(joint)
            q = apply(m, p) if m else p
            r = rotate_raw(m, n) if m else n
            for k in range(3):
                pos[k] += w * q[k]
                nrm[k] += w * r[k]
        length = math.sqrt(sum(x * x for x in nrm)) or 1.0
        return tuple(pos), tuple(x / length for x in nrm)

    def _names(self, o):
        d = self.d
        out = []
        for i in range(be("H", d, o)[0]):
            s = o + be("H", d, o + 4 + 4 * i + 2)[0]
            out.append(d[s:d.index(b"\0", s)].decode("ascii", "replace"))
        return out

    def _jnt1(self):
        d, o = self.d, self.sec["JNT1"]
        n, _, eo, ro, no = be("HHIII", d, o + 8)
        names = self._names(o + no)
        self.joints = []
        for i in range(n):
            e = o + eo + 0x40 * be("H", d, o + ro + 2 * i)[0]
            rot = tuple(v * math.pi / 32768.0 for v in be("3h", d, e + 0x10))
            self.joints.append(Joint(names[i], be("3f", d, e + 4), rot, be("3f", d, e + 0x18)))

    def _evp1(self):
        self.envelopes, self.inv_bind = [], {}
        o = self.sec.get("EVP1")
        if o is None:
            return
        d = self.d
        n, _, co, io, wo, mo = be("HHIIII", d, o + 8)
        k = 0
        last = -1
        for i in range(n):
            env = []
            for _ in range(d[o + co + i]):
                joint = be("H", d, o + io + 2 * k)[0]
                env.append((joint, be("f", d, o + wo + 4 * k)[0]))
                last = max(last, joint)
                k += 1
            self.envelopes.append(env)
        if mo:
            for j in range(last + 1):
                m = be("12f", d, o + mo + 48 * j)
                self.inv_bind[j] = [list(m[0:4]), list(m[4:8]), list(m[8:12])]

    def _drw1(self):
        d, o = self.d, self.sec["DRW1"]
        n, _, wo, io = be("HHII", d, o + 8)
        self.draw = [(d[o + wo + i], be("H", d, o + io + 2 * i)[0]) for i in range(n)]

    def influences(self, draw_index):
        """[(joint, weight)] of a draw matrix and whether its vertices are in model space."""
        weighted, idx = self.draw[draw_index]
        if weighted:
            return self.envelopes[idx], True
        return [(idx, 1.0)], False

    def _vtx1(self):
        d, o = self.d, self.sec["VTX1"]
        size = be("I", d, o + 4)[0]
        offs = list(be("13I", d, o + 0xC))
        fmts = {}
        p = o + be("I", d, o + 8)[0]
        while True:
            attr, cnt, typ, frac = be("IIIB", d, p)
            if attr == 0xFF:
                break
            fmts[attr] = (cnt, typ, frac)
            p += 16
        order = [ATTR_POS, ATTR_NRM, 0x19, ATTR_CLR0, 0xC] + list(range(ATTR_TEX0, ATTR_TEX0 + 8))
        self.arrays = {}
        for k, attr in enumerate(order):
            if not offs[k] or attr not in fmts:
                continue
            end = min([x for x in offs if x > offs[k]] + [size])
            raw = d[o + offs[k]:o + end]
            cnt, typ, frac = fmts[attr]
            if attr in (ATTR_CLR0, 0xC):
                self.arrays[attr] = _colours(raw, typ)
                continue
            if attr == 0x19:
                continue
            ncomp = {ATTR_POS: 3 if cnt == 1 else 2, ATTR_NRM: 3}.get(attr, 2 if cnt == 1 else 1)
            fmt, cs = COMP[typ]
            vals = struct.unpack_from(">%d%s" % (len(raw) // cs, fmt), raw)
            scale = 1.0 if typ == 4 else 1.0 / (1 << frac)
            self.arrays[attr] = [tuple(v * scale for v in vals[i:i + ncomp])
                                 for i in range(0, len(vals) - ncomp + 1, ncomp)]

    def _tex1(self):
        d, o = self.d, self.sec["TEX1"]
        n, _, ho, no = be("HHII", d, o + 8)
        names = self._names(o + no)
        self.textures = []
        for i in range(n):
            h = o + ho + 0x20 * i
            fmt, _alpha, w, hh = be("BBHH", d, h)
            pal_fmt, pal_count, pal_off = d[h + 9], be("H", d, h + 0xA)[0], be("I", d, h + 0xC)[0]
            self.textures.append({"name": names[i], "header": h, "format": fmt, "w": w, "h": hh,
                                  "pal_fmt": pal_fmt, "pal_count": pal_count, "pal_off": pal_off,
                                  "img_off": be("I", d, h + 0x1C)[0]})

    def _mat3(self):
        d, o = self.d, self.sec["MAT3"]
        n = be("H", d, o + 8)[0]
        offs = be("30I", d, o + 0xC)
        names = self._names(o + offs[2])
        self.materials = []
        for i in range(n):
            e = o + offs[0] + 0x14C * be("H", d, o + offs[1] + 2 * i)[0]
            texs = []
            for k in range(8):
                ti = be("H", d, e + 0x84 + 2 * k)[0]
                if ti != 0xFFFF:
                    texs.append(be("H", d, o + offs[15] + 2 * ti)[0])
            self.materials.append({"name": names[i], "textures": texs})

    def _shp1(self):
        d, o = self.d, self.sec["SHP1"]
        n = be("H", d, o + 8)[0]
        init, remap, _names, desco, mtxtab, dlo, mtxdata, pkto = be("8I", d, o + 0xC)
        self.shapes = []
        for i in range(n):
            e = o + init + 0x28 * be("H", d, o + remap + 2 * i)[0]
            npkt, attro, firstmtx, firstpkt = be("HHHH", d, e + 2)
            descs = []
            p = o + desco + attro
            while True:
                a, t = be("II", d, p)
                if a == 0xFF:
                    break
                descs.append((a, t))
                p += 8
            packets, last = [], []
            for k in range(npkt):
                _use, cnt, first = be("HHI", d, o + mtxdata + 8 * (firstmtx + k))
                cur = list(last) + [0xFFFF] * max(0, cnt - len(last))
                for m in range(cnt):
                    v = be("H", d, o + mtxtab + 2 * (first + m))[0]
                    if v != 0xFFFF:
                        cur[m] = v
                last = cur
                size, off = be("II", d, o + pkto + 8 * (firstpkt + k))
                packets.append((list(cur), _display_list(d[o + dlo + off:o + dlo + off + size], descs)))
            self.shapes.append({"index": i, "descs": descs, "packets": packets})

    def _inf1(self):
        d, o = self.d, self.sec["INF1"]
        p = o + be("I", d, o + 0x14)[0]
        stack, jstack = [], [None]
        last_joint, last_mat = None, None
        self.shape_material, self.joint_parent = {}, {}
        while True:
            t, v = be("HH", d, p)
            p += 4
            if t == 0:
                break
            if t == 1:
                stack.append((last_joint, last_mat))
                jstack.append(last_joint)
            elif t == 2:
                last_joint, last_mat = stack.pop()
                jstack.pop()
            elif t == 0x10:
                self.joint_parent[v] = jstack[-1]
                last_joint = v
            elif t == 0x11:
                last_mat = v
            elif t == 0x12:
                self.shape_material[v] = last_mat

    def triangles(self, shape):
        """The shape's triangles as vertex dicts ({attr: index}, with the draw matrix under "drw")."""
        out = []
        for mtx, prims in shape["packets"]:
            for op, verts in prims:
                vs = []
                for v in verts:
                    v = dict(v)
                    v["drw"] = mtx[v[0] // 3] if 0 in v else mtx[0]
                    vs.append(v)
                if op == PRIM_TRIANGLES:
                    out += [tuple(vs[k:k + 3]) for k in range(0, len(vs) - 2, 3)]
                elif op == PRIM_STRIP:
                    for k in range(len(vs) - 2):
                        out.append((vs[k], vs[k + 1], vs[k + 2]) if k % 2 == 0 else (vs[k + 1], vs[k], vs[k + 2]))
                elif op == PRIM_FAN:
                    out += [(vs[0], vs[k], vs[k + 1]) for k in range(1, len(vs) - 1)]
                else:
                    raise J3DError("primitive 0x%02X not supported" % op)
        return out

    def texture_rgba(self, i):
        """(width, height, [(r, g, b, a)]) of texture i."""
        t = self.textures[i]
        d, h = self.d, t["header"]
        pal = None
        if t["format"] in (8, 9, 10):
            praw = d[h + t["pal_off"]:h + t["pal_off"] + 2 * t["pal_count"]]
            pal = [_palette(be("H", praw, 2 * k)[0], t["pal_fmt"]) for k in range(t["pal_count"])]
        return t["w"], t["h"], decode_texture(d[h + t["img_off"]:], t["format"], t["w"], t["h"], pal)


def _display_list(dl, descs):
    prims = []
    p = 0
    while p < len(dl) and dl[p]:
        op = dl[p]
        cnt = be("H", dl, p + 1)[0]
        p += 3
        verts = []
        for _ in range(cnt):
            v = {}
            for a, t in descs:
                if t == 1:
                    if a > 8:
                        raise J3DError("direct vertex attribute %d not supported" % a)
                    v[a] = dl[p]
                    p += 1
                elif t == 2:
                    v[a] = dl[p]
                    p += 1
                elif t == 3:
                    v[a] = be("H", dl, p)[0]
                    p += 2
            verts.append(v)
        prims.append((op & 0xF8, verts))
    return prims


def _colours(raw, typ):
    out = []
    if typ == 5:
        out = [tuple(raw[i:i + 4]) for i in range(0, len(raw) - 3, 4)]
    elif typ == 2:
        out = [tuple(raw[i:i + 3]) + (255,) for i in range(0, len(raw) - 2, 3)]
    elif typ == 3:
        for i in range(0, len(raw) - 1, 2):
            v = be("H", raw, i)[0]
            out.append(tuple(((v >> s) & 0xF) * 17 for s in (12, 8, 4, 0)))
    elif typ == 0:
        out = [_rgb565(be("H", raw, i)[0]) for i in range(0, len(raw) - 1, 2)]
    else:
        raise J3DError("vertex colour type %d not supported" % typ)
    return out


def _rgb565(v):
    return ((v >> 11) * 255 // 31, ((v >> 5) & 63) * 255 // 63, (v & 31) * 255 // 31, 255)


def _rgb5a3(v):
    if v & 0x8000:
        return (((v >> 10) & 31) * 255 // 31, ((v >> 5) & 31) * 255 // 31, (v & 31) * 255 // 31, 255)
    return (((v >> 8) & 15) * 17, ((v >> 4) & 15) * 17, (v & 15) * 17, ((v >> 12) & 7) * 255 // 7)


def _palette(v, fmt):
    if fmt == 0:
        return (v & 0xFF, v & 0xFF, v & 0xFF, v >> 8)
    return _rgb565(v) if fmt == 1 else _rgb5a3(v)


# block width, block height, bits per texel
BLOCK = {0: (8, 8, 4), 1: (8, 4, 8), 2: (8, 4, 8), 3: (4, 4, 16), 4: (4, 4, 16), 5: (4, 4, 16), 6: (4, 4, 32),
         8: (8, 8, 4), 9: (8, 4, 8), 10: (4, 4, 16), 14: (8, 8, 4)}


def decode_texture(data, fmt, w, h, pal=None):
    if fmt not in BLOCK:
        raise J3DError("texture format %d not supported" % fmt)
    out = [(0, 0, 0, 0)] * (w * h)
    bw, bh, bpp = BLOCK[fmt]
    p = 0
    for by in range(0, h, bh):
        for bx in range(0, w, bw):
            if fmt == 6:
                ar, gb = data[p:p + 32], data[p + 32:p + 64]
                p += 64
                for k in range(16):
                    x, y = bx + k % 4, by + k // 4
                    if x < w and y < h:
                        out[y * w + x] = (ar[2 * k + 1], gb[2 * k], gb[2 * k + 1], ar[2 * k])
                continue
            if fmt == 14:
                for sb in range(4):
                    c0, c1, bits = be("HHI", data, p)
                    p += 8
                    a, b = _rgb565(c0), _rgb565(c1)
                    if c0 > c1:
                        cs = [a, b, tuple((2 * a[i] + b[i]) // 3 for i in range(3)) + (255,),
                              tuple((a[i] + 2 * b[i]) // 3 for i in range(3)) + (255,)]
                    else:
                        cs = [a, b, tuple((a[i] + b[i]) // 2 for i in range(3)) + (255,), (0, 0, 0, 0)]
                    for k in range(16):
                        x, y = bx + (sb % 2) * 4 + k % 4, by + (sb // 2) * 4 + k // 4
                        if x < w and y < h:
                            out[y * w + x] = cs[(bits >> (30 - 2 * k)) & 3]
                continue
            for k in range(bw * bh):
                if fmt in (0, 8):
                    v = (data[p + k // 2] >> (4 if k % 2 == 0 else 0)) & 0xF
                    c = (v * 17, v * 17, v * 17, 255) if fmt == 0 else pal[v]
                elif fmt == 1:
                    c = (data[p + k],) * 3 + (255,)
                elif fmt == 2:
                    v = data[p + k]
                    c = ((v & 15) * 17,) * 3 + ((v >> 4) * 17,)
                elif fmt == 3:
                    c = (data[p + 2 * k + 1],) * 3 + (data[p + 2 * k],)
                elif fmt == 4:
                    c = _rgb565(be("H", data, p + 2 * k)[0])
                elif fmt == 5:
                    c = _rgb5a3(be("H", data, p + 2 * k)[0])
                elif fmt == 9:
                    c = pal[data[p + k]]
                else:
                    c = pal[be("H", data, p + 2 * k)[0] & 0x3FFF]
                x, y = bx + k % bw, by + k // bw
                if x < w and y < h:
                    out[y * w + x] = c
            p += bw * bh * bpp // 8
    return out
