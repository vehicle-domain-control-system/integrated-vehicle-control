import math
import os
import sys

import bmesh
import bpy
from mathutils import Vector
from mathutils.bvhtree import BVHTree

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(HERE, "..", ".."))
OUT = os.path.join(ROOT, "app", "src", "main", "assets", "models", "car.glb")

LENGTH = 4.65
HALF_L = LENGTH / 2.0
HALF_W = 1.825 / 2.0
WHEELBASE = 2.72
FRONT_AXLE = HALF_L - 0.90
REAR_AXLE = FRONT_AXLE - WHEELBASE
TIRE_R = 0.317
TIRE_W = 0.225
TRACK_HALF = 0.79
ARCH_R = 0.395

FD = (0.98, -0.12)
RD = (-0.12, -0.86)
DOOR_GAP = 0.004

WIN_FRONT = (0.95, -0.08)
WIN_REAR = (-0.16, -0.82)
WIN_QUARTER = (-0.92, -1.28)
WINDSHIELD = (0.90, 0.07)
REAR_GLASS = (-0.82, -1.66)

def lerp(a, b, t):
    return a + (b - a) * t

def clamp(x, lo, hi):
    return max(lo, min(hi, x))

def interp(points, s):
    if s >= points[0][0]:
        return points[0][1]
    if s <= points[-1][0]:
        return points[-1][1]
    for (s0, v0), (s1, v1) in zip(points, points[1:]):
        if s0 >= s >= s1:
            t = (s0 - s) / (s0 - s1) if s0 != s1 else 0.0

            t = t * t * (3 - 2 * t) * 0.5 + t * 0.5
            return lerp(v0, v1, t)
    return points[-1][1]

HALF_WIDTH = [
    (2.325, 0.62), (2.31, 0.72), (2.28, 0.80), (2.22, 0.855), (2.12, 0.89),
    (1.95, 0.906), (1.2, 0.9125), (-1.2, 0.9125), (-1.95, 0.905), (-2.15, 0.888),
    (-2.25, 0.86), (-2.30, 0.81), (-2.325, 0.74),
]
BOTTOM = [
    (2.325, 0.40), (2.31, 0.30), (2.27, 0.24), (2.20, 0.205), (2.0, 0.18), (1.8, 0.17),
    (-1.8, 0.17), (-2.0, 0.20), (-2.2, 0.27), (-2.30, 0.33), (-2.325, 0.42),
]

TOP = [
    (2.325, 0.56), (2.315, 0.66), (2.28, 0.725), (2.20, 0.765), (2.0, 0.80),
    (1.5, 0.855), (0.92, 0.92), (0.07, 1.375), (-0.15, 1.415), (-0.45, 1.42),
    (-0.80, 1.37), (-1.20, 1.25), (-1.72, 1.07), (-1.95, 1.05), (-2.15, 1.045),
    (-2.27, 1.045), (-2.325, 0.96),
]

BELT = [
    (2.325, 0.54), (2.315, 0.63), (2.28, 0.695), (2.20, 0.735), (2.0, 0.765),
    (1.5, 0.81), (0.92, 0.895), (0.0, 0.94), (-0.86, 0.975), (-1.50, 1.005),
    (-2.10, 1.005), (-2.27, 1.0), (-2.325, 0.92),
]

CHAR = [
    (2.325, 0.50), (2.22, 0.66), (1.75, 0.75), (1.10, 0.70), (-0.92, 0.44),
    (-1.05, 0.50), (-1.75, 0.83), (-2.325, 0.80),
]

def arch_bottom(s):
    z = interp(BOTTOM, s)
    for a in (FRONT_AXLE, REAR_AXLE):
        d = s - a
        if abs(d) <= ARCH_R:
            z = max(z, TIRE_R + math.sqrt(max(0.0, ARCH_R * ARCH_R - d * d)) * 0.98 + 0.02)
    return z

RIDGE_OUT = 0.032
WIDTH_SCALE = (HALF_W - RIDGE_OUT) / HALF_W

def body_half_width(s):
    return interp(HALF_WIDTH, s) * WIDTH_SCALE

def ring_profile(s):
    w = body_half_width(s)
    zb = arch_bottom(s)
    top = interp(TOP, s)
    belt = interp(BELT, s)
    zc = interp(CHAR, s)
    H = max(0.0, top - belt - 0.05)
    f = clamp(H / 0.40, 0.0, 1.0)
    lat_belt = w - 0.055
    wr = lerp(lat_belt - 0.01, w - 0.30, f)

    rows = [None] * 13
    rows[0] = [0.0, zb]
    rows[1] = [max(0.0, w - 0.14), zb]
    rows[2] = [w - 0.035, zb + 0.045]
    rf = clamp((1.95 - abs(s)) / 0.30, 0.0, 1.0)
    rows[3] = [w - 0.065 * rf - 0.03 * (1 - rf), zb + 0.15]
    rows[4] = [w - 0.010 * rf - 0.02 * (1 - rf), zc - 0.035]
    rows[5] = [w + RIDGE_OUT * rf - 0.01 * (1 - rf), zc]
    rows[6] = [w + 0.006 * rf - 0.012 * (1 - rf), belt - 0.12]

    for k in range(1, 7):
        z = rows[k][1]
        z = min(z, belt - (7 - k) * 0.012)
        z = max(z, rows[k - 1][1] + (0.0 if k == 1 else 0.004))
        rows[k][1] = z
    rows[7] = [lat_belt, belt]
    rows[8] = [lerp(lat_belt, wr, 0.07) - 0.006, belt + 0.07 * H]
    rows[9] = [lerp(lat_belt, wr, 0.93) - 0.006, belt + 0.93 * H]
    rows[10] = [wr, belt + H]
    rows[11] = [wr * 0.52, belt + H + (top - belt - H) * 0.80]
    rows[12] = [0.0, top]
    return [tuple(r) for r in rows]

RIDGE_ROW = 5
N_ROWS = 13

def roof_inner_z(s, x):
    h = ring_profile(s)
    ax = abs(x)
    for a, b in ((12, 11), (11, 10)):
        la, za = h[a]
        lb, zb = h[b]
        if la <= ax <= lb:
            t = (ax - la) / (lb - la) if lb != la else 0.0
            return lerp(za, zb, t)
    return h[10][1]

def stations():
    pts = set()
    base = [2.325, 2.315, 2.30, 2.28, 2.25, 2.20, 2.12, 2.0, 1.92,
            0.98, 0.95, 0.90, 0.80, 0.65, 0.50, 0.35, 0.20, 0.07, -0.08, -0.12, -0.16,
            -0.30, -0.45, -0.60, -0.80, -0.82, -0.86,
            -1.95, -2.10, -2.20, -2.25, -2.28, -2.30, -2.315, -2.325]
    pts.update(base)
    for a in (FRONT_AXLE, REAR_AXLE):
        pts.add(round(a + ARCH_R + 0.012, 4))
        pts.add(round(a - ARCH_R - 0.012, 4))
        n = 14
        for i in range(n + 1):
            ang = math.pi * i / n
            pts.add(round(a + ARCH_R * math.cos(ang), 4))
    for e in (WIN_QUARTER + REAR_GLASS + WINDSHIELD + (-1.15, -1.35, 1.6, 1.25)):
        pts.add(e)
    return sorted(pts, reverse=True)

class FaceBin:
    def __init__(self):
        self.parts = {}

    def add(self, part, coords, mat, sharp_keys=None):
        self.parts.setdefault(part, []).append((coords, mat))

def key(v):
    return (round(v[0], 5), round(v[1], 5), round(v[2], 5))

def newell(coords):
    nx = ny = nz = 0.0
    n = len(coords)
    for i in range(n):
        x0, y0, z0 = coords[i]
        x1, y1, z1 = coords[(i + 1) % n]
        nx += (y0 - y1) * (z0 + z1)
        ny += (z0 - z1) * (x0 + x1)
        nz += (x0 - x1) * (y0 + y1)
    return Vector((nx, ny, nz))

def oriented(coords, outward):
    if newell(coords).dot(Vector(outward)) < 0:
        return list(reversed(coords))
    return list(coords)

def in_range(s, rng):
    hi, lo = max(rng), min(rng)
    return lo <= s <= hi

def classify(band, s, side):
    L = "L" if side > 0 else "R"
    part = "Body"
    mat = "Paint"
    if band in (0,):
        mat = "Trim"

    if 3 <= band <= 9:
        if in_range(s, FD):
            part = "Door_F" + L
        elif in_range(s, RD):
            part = "Door_R" + L

    if band == 8:
        if in_range(s, WIN_FRONT) or in_range(s, WIN_REAR):
            mat = "Glass"
        elif in_range(s, WIN_QUARTER):
            mat = "Glass"
            part = "Glass"
        elif -0.16 <= s <= -0.08:
            mat = "Trim"
        elif -1.28 >= s >= -1.50 or -0.86 >= s >= -0.92:
            mat = "Trim"
    if band in (7, 9) and (-0.16 <= s <= -0.08):
        mat = "Trim"

    if band in (10, 11):
        if in_range(s, WINDSHIELD) or in_range(s, REAR_GLASS):
            mat = "Glass"
            part = "Glass"
    return part, mat

def build_loft(fb):
    st = stations()
    rings = []
    for s in st:
        half = ring_profile(s)
        rings.append((s, half))
    ridge_keys = set()
    for i in range(len(rings) - 1):
        s0, h0 = rings[i]
        s1, h1 = rings[i + 1]
        sm = 0.5 * (s0 + s1)
        for side in (1, -1):
            for b in range(N_ROWS - 1):
                p = [
                    (side * h0[b][0], -s0, h0[b][1]),
                    (side * h1[b][0], -s1, h1[b][1]),
                    (side * h1[b + 1][0], -s1, h1[b + 1][1]),
                    (side * h0[b + 1][0], -s0, h0[b + 1][1]),
                ]
                part, mat = classify(b, sm, side)
                c = sum((Vector(q) for q in p), Vector()) / 4.0
                fb.add(part, oriented(p, c - Vector((0, c.y, 0.62))), mat)
            ridge_keys.add(key((side * h0[RIDGE_ROW][0], -s0, h0[RIDGE_ROW][1])))
            ridge_keys.add(key((side * h1[RIDGE_ROW][0], -s1, h1[RIDGE_ROW][1])))

    for idx, sgn in ((0, 1), (len(rings) - 1, -1)):
        s, h = rings[idx]
        loop = [(hh[0], -s, hh[1]) for hh in h] + [(-hh[0], -s, hh[1]) for hh in reversed(h[1:-1])]
        fb.add("Body", oriented(loop, (0, -sgn, 0)), "Paint")
    return ridge_keys

MAT_ORDER = ["Paint", "Glass", "InteriorLight", "Tire", "Rim", "Lamp_Front", "Lamp_Rear", "Trim", "Seat"]

def faces_to_object(name, faces, mats, origin=(0, 0, 0), ridge_keys=None,
                    smooth_angle=0.6, all_smooth=True):
    bm = bmesh.new()
    vmap = {}
    used = []
    for coords, mat in faces:
        vs = []
        for c in coords:
            k = key(c)
            v = vmap.get(k)
            if v is None:
                v = bm.verts.new(Vector(k))
                vmap[k] = v
            if v not in vs:
                vs.append(v)
        if len(vs) < 3:
            continue

        try:
            f = bm.faces.new(vs)
        except ValueError:
            continue
        if mat not in used:
            used.append(mat)
        f.material_index = used.index(mat)
        f.smooth = all_smooth
    bm.normal_update()
    for e in bm.edges:
        if len(e.link_faces) == 2:
            try:
                ang = e.calc_face_angle()
            except ValueError:
                ang = 0
            if ang > smooth_angle:
                e.smooth = False
            if e.link_faces[0].material_index != e.link_faces[1].material_index:
                e.smooth = False
        if ridge_keys is not None:
            if key(e.verts[0].co) in ridge_keys and key(e.verts[1].co) in ridge_keys:
                e.smooth = False
    bmesh.ops.translate(bm, verts=bm.verts, vec=-Vector(origin))
    me = bpy.data.meshes.new(name)
    bm.to_mesh(me)
    bm.free()
    for m in used:
        me.materials.append(mats[m])
    ob = bpy.data.objects.new(name, me)
    ob.location = origin
    bpy.context.scene.collection.objects.link(ob)
    return ob

def box_faces(center, size, mat, rot_x=0.0):
    cx, cy, cz = center
    sx, sy, sz = (v / 2 for v in size)
    cr, sr = math.cos(rot_x), math.sin(rot_x)

    def P(x, y, z):
        y2 = y * cr - z * sr
        z2 = y * sr + z * cr
        return (cx + x, cy + y2, cz + z2)

    v = [P(-sx, -sy, -sz), P(sx, -sy, -sz), P(sx, sy, -sz), P(-sx, sy, -sz),
         P(-sx, -sy, sz), P(sx, -sy, sz), P(sx, sy, sz), P(-sx, sy, sz)]
    idx = [(0, 3, 2, 1), (4, 5, 6, 7), (0, 1, 5, 4), (1, 2, 6, 5), (2, 3, 7, 6), (3, 0, 4, 7)]
    return [([v[i] for i in q], mat) for q in idx]

def rounded_box_faces(center, size, mat, rot_x=0.0, bevel=0.025):
    cx, cy, cz = center
    sx, sy, sz = (v / 2 for v in size)
    b = min(bevel, sx * 0.4, sz * 0.4)
    prof = [(-sx + b, -sz), (sx - b, -sz), (sx, -sz + b), (sx, sz - b),
            (sx - b, sz), (-sx + b, sz), (-sx, sz - b), (-sx, -sz + b)]
    cr, sr = math.cos(rot_x), math.sin(rot_x)

    def P(x, y, z):
        y2 = y * cr - z * sr
        z2 = y * sr + z * cr
        return (cx + x, cy + y2, cz + z2)

    front = [P(x, -sy, z) for x, z in prof]
    back = [P(x, sy, z) for x, z in prof]
    out = [(front, mat), (list(reversed(back)), mat)]
    n = len(prof)
    for i in range(n):
        j = (i + 1) % n
        out.append(([front[i], back[i], back[j], front[j]], mat))
    return out

def make_materials():
    def mat(name, color, metal=0.0, rough=0.5, alpha=1.0, emit=None, emit_strength=1.0):
        m = bpy.data.materials.new(name)
        m.use_nodes = True
        bsdf = m.node_tree.nodes.get("Principled BSDF")
        bsdf.inputs["Base Color"].default_value = (*color, 1.0)
        bsdf.inputs["Metallic"].default_value = metal
        bsdf.inputs["Roughness"].default_value = rough
        m.diffuse_color = (*color, alpha)
        m.metallic = metal
        m.roughness = rough
        if alpha < 1.0:
            bsdf.inputs["Alpha"].default_value = alpha
            try:
                m.surface_render_method = "BLENDED"
            except AttributeError:
                pass
            try:
                m.blend_method = "BLEND"
            except (AttributeError, TypeError):
                pass
        if emit is not None:
            bsdf.inputs["Emission Color"].default_value = (*emit, 1.0)
            bsdf.inputs["Emission Strength"].default_value = emit_strength
        m.use_backface_culling = False
        return m

    return {
        "Paint": mat("Paint", (0.56, 0.58, 0.61), metal=0.45, rough=0.35),
        "Glass": mat("Glass", (0.02, 0.03, 0.04), metal=0.0, rough=0.05, alpha=0.72),
        "InteriorLight": mat("InteriorLight", (1.0, 1.0, 1.0), rough=0.6, emit=(1.0, 1.0, 1.0)),
        "Tire": mat("Tire", (0.035, 0.035, 0.04), rough=0.9),
        "Rim": mat("Rim", (0.72, 0.74, 0.77), metal=0.9, rough=0.25),

        "Lamp_Front": mat("Lamp_Front", (0.55, 0.58, 0.62), rough=0.15),
        "Lamp_Rear": mat("Lamp_Rear", (0.28, 0.02, 0.03), rough=0.2),
        "Trim": mat("Trim", (0.025, 0.025, 0.028), rough=0.55),
        "Seat": mat("Seat", (0.10, 0.10, 0.11), rough=0.8),
    }

def build_bvh(faces):
    verts, polys, vmap = [], [], {}
    for coords, _ in faces:
        idx = []
        for c in coords:
            k = key(c)
            if k not in vmap:
                vmap[k] = len(verts)
                verts.append(Vector(k))
            if vmap[k] not in idx:
                idx.append(vmap[k])
        if len(idx) >= 3:
            polys.append(idx)
    return BVHTree.FromPolygons(verts, polys)

def project(bvh, x, z, from_front, offset=0.006, min_facing=None):
    if from_front:
        o, d = Vector((x, -5.0, z)), Vector((0, 1, 0))
    else:
        o, d = Vector((x, 5.0, z)), Vector((0, -1, 0))
    loc, nrm, _, _ = bvh.ray_cast(o, d)
    if loc is None:
        return None
    if nrm.dot(d) > 0:
        nrm = -nrm

    if min_facing is not None and -nrm.dot(d) < min_facing:
        return None
    return loc + nrm * offset

def decal_strip(bvh, xs, zbot, ztop, from_front, mat, rows=2, offset=0.006, min_facing=None):
    grid = []
    for x in xs:
        col = []
        for r in range(rows + 1):
            z = lerp(zbot(x), ztop(x), r / rows)
            p = project(bvh, x, z, from_front, offset, min_facing)
            col.append(p)
        grid.append(col)
    out = []
    for i in range(len(xs) - 1):
        for r in range(rows):
            q = [grid[i][r], grid[i + 1][r], grid[i + 1][r + 1], grid[i][r + 1]]
            if any(p is None for p in q):
                continue

            if max(p.y for p in q) - min(p.y for p in q) > 0.12:
                continue
            out.append((oriented([tuple(p) for p in q], (0, -1 if from_front else 1, 0)), mat))
    return out

def linspace(a, b, n):
    return [a + (b - a) * i / (n - 1) for i in range(n)]

def wheel_faces(side):
    seg = 32
    faces = []
    hw = TIRE_W / 2

    prof = [(0.215, -hw + 0.012), (0.27, -hw), (0.302, -hw + 0.008), (TIRE_R, -hw + 0.035),
            (TIRE_R, hw - 0.035), (0.302, hw - 0.008), (0.27, hw), (0.215, hw - 0.012)]

    def P(r, ax, a):
        return (side * ax, r * math.cos(a), r * math.sin(a))

    for i in range(seg):
        a0 = 2 * math.pi * i / seg
        a1 = 2 * math.pi * (i + 1) / seg
        for j in range(len(prof) - 1):
            r0, x0 = prof[j]
            r1, x1 = prof[j + 1]
            faces.append(([P(r0, x0, a0), P(r0, x0, a1), P(r1, x1, a1), P(r1, x1, a0)], "Tire"))

    lip_x = hw - 0.018
    face_x = hw - 0.035
    deep_x = hw - 0.11
    for i in range(seg):
        a0 = 2 * math.pi * i / seg
        a1 = 2 * math.pi * (i + 1) / seg
        faces.append(([P(0.215, lip_x, a0), P(0.215, lip_x, a1), P(0.19, face_x, a1), P(0.19, face_x, a0)], "Rim"))
        mid = (i + 0.5) / seg * 5.0
        spoke = (mid % 1.0) < 0.42
        if spoke:
            faces.append(([P(0.19, face_x, a0), P(0.19, face_x, a1), P(0.065, face_x + 0.01, a1),
                           P(0.065, face_x + 0.01, a0)], "Rim"))
        else:
            faces.append(([P(0.19, deep_x, a0), P(0.19, deep_x, a1), P(0.065, deep_x, a1),
                           P(0.065, deep_x, a0)], "Trim"))

        faces.append(([P(0.065, face_x + 0.01, a0), P(0.065, face_x + 0.01, a1), P(0.0, face_x + 0.02, a1),
                       P(0.0, face_x + 0.02, a0)], "Rim"))

    for i in range(seg):
        a0 = 2 * math.pi * i / seg
        mid_prev = ((i - 0.5) / seg * 5.0) % 1.0 < 0.42
        mid_cur = ((i + 0.5) / seg * 5.0) % 1.0 < 0.42
        if mid_prev != mid_cur:
            faces.append(([P(0.19, face_x, a0), P(0.065, face_x + 0.01, a0), P(0.065, deep_x, a0),
                           P(0.19, deep_x, a0)], "Rim"))

    for i in range(seg):
        a0 = 2 * math.pi * i / seg
        a1 = 2 * math.pi * (i + 1) / seg
        faces.append(([P(0.215, -hw + 0.012, a0), P(0.0, -hw + 0.03, a0), P(0.215, -hw + 0.012, a1)], "Trim"))
    if side < 0:
        faces = [(list(reversed(c)), m) for c, m in faces]
    return faces

def add_solidify(ob, thickness, trim_mat):
    vg = ob.vertex_groups.new(name="rim")
    vg_index = vg.index
    mod = ob.modifiers.new("solid", "SOLIDIFY")
    mod.thickness = thickness
    mod.offset = -1.0
    mod.use_even_offset = False
    mod.rim_vertex_group = vg.name
    bpy.context.view_layer.objects.active = ob
    for o in bpy.context.selected_objects:
        o.select_set(False)
    ob.select_set(True)
    bpy.ops.object.modifier_apply(modifier=mod.name)
    me = ob.data
    if trim_mat.name not in [m.name for m in me.materials]:
        me.materials.append(trim_mat)
    ti = [m.name for m in me.materials].index(trim_mat.name)
    rim = set()
    for v in me.vertices:
        for g in v.groups:
            if g.group == vg_index and g.weight > 0.5:
                rim.add(v.index)
    n_rim = 0
    for p in me.polygons:
        if all(i in rim for i in p.vertices) and abs(p.normal.y) > 0.6:
            p.material_index = ti
            n_rim += 1
    ob.vertex_groups.remove(ob.vertex_groups["rim"])
    return n_rim

def main():
    bpy.ops.wm.read_factory_settings(use_empty=True)
    mats = make_materials()
    fb = FaceBin()
    ridge = build_loft(fb)

    all_skin = []
    for faces in fb.parts.values():
        all_skin.extend(faces)
    bvh = build_bvh(all_skin)

    body_extra = []

    body_extra += decal_strip(
        bvh, linspace(-0.56, 0.56, 21),
        lambda x: 0.335 + 0.02 * (abs(x) / 0.56) ** 2,
        lambda x: 0.615 - 0.04 * (abs(x) / 0.56) ** 2,
        True, "Trim", rows=3, offset=0.004)

    body_extra += decal_strip(
        bvh, linspace(-0.70, 0.70, 15), lambda x: 0.36, lambda x: 0.46 - 0.03 * (abs(x) / 0.7) ** 2,
        False, "Trim", rows=1, offset=0.004)

    for a in (FRONT_AXLE, REAR_AXLE):
        for side in (1, -1):
            xin = side * (HALF_W - 0.33)
            n = 12
            pts_o, pts_t = [], []
            for i in range(n + 1):
                ang = math.pi * i / n
                ss = a + (ARCH_R + 0.01) * math.cos(ang)
                zz = TIRE_R + (ARCH_R + 0.01) * math.sin(ang)
                pts_o.append((xin, -ss, zz))
                pts_t.append((side * (body_half_width(ss) - 0.03), -ss, zz))
            for i in range(n):
                body_extra.append(([pts_o[i], pts_o[i + 1], pts_t[i + 1], pts_t[i]], "Trim"))
                body_extra.append(([pts_o[i], (xin, pts_o[i][1], TIRE_R - 0.05),
                                    (xin, pts_o[i + 1][1], TIRE_R - 0.05), pts_o[i + 1]], "Trim"))

    for side in (1, -1):
        hs = [ring_profile(s) for s in (-0.08, -0.16)]
        for r in range(3, 10):
            q = [(side * (hs[0][r][0] - 0.035), 0.08, hs[0][r][1]),
                 (side * (hs[1][r][0] - 0.035), 0.16, hs[1][r][1]),
                 (side * (hs[1][r + 1][0] - 0.035), 0.16, hs[1][r + 1][1]),
                 (side * (hs[0][r + 1][0] - 0.035), 0.08, hs[0][r + 1][1])]
            body_extra.append((oriented(q, (side, 0, 0)), "Trim"))

    body_extra += box_faces((0, -(-0.35), 0.30), (1.55, 2.10, 0.02), "Trim")

    body_extra += rounded_box_faces((0, -0.72, 0.80), (1.60, 0.40, 0.14), "Trim", rot_x=0.15, bevel=0.03)
    body_extra += box_faces((0, -0.15, 0.42), (0.22, 0.80, 0.18), "Trim")

    sw = []
    c = Vector((0.37, -0.50, 0.86))
    nseg = 16
    for i in range(nseg):
        a0 = 2 * math.pi * i / nseg
        a1 = 2 * math.pi * (i + 1) / nseg

        def SWP(r, a, dy):
            return (c.x + r * math.cos(a), c.y + dy + r * math.sin(a) * 0.35, c.z + r * math.sin(a) * 0.94)
        sw.append(([SWP(0.19, a0, 0), SWP(0.19, a1, 0), SWP(0.16, a1, 0.01), SWP(0.16, a0, 0.01)], "Trim"))
    body_extra += sw

    head = []
    for sgn in (1, -1):
        xs = linspace(sgn * 0.40, sgn * 0.84, 10)
        head += decal_strip(
            bvh, xs,
            lambda x: 0.630 + 0.035 * ((abs(x) - 0.40) / 0.44),
            lambda x: 0.672 + 0.035 * ((abs(x) - 0.40) / 0.44) - 0.02 * ((abs(x) - 0.40) / 0.44) ** 4,
            True, "Lamp_Front", rows=1, offset=0.007)

    tail = []
    tail += decal_strip(bvh, linspace(-0.62, 0.62, 21), lambda x: 0.885, lambda x: 0.915,
                        False, "Lamp_Rear", rows=1, offset=0.007, min_facing=0.35)
    for sgn in (1, -1):
        tail += decal_strip(bvh, linspace(sgn * 0.62, sgn * 0.82, 6),
                            lambda x: 0.845, lambda x: 0.945 - 0.03 * ((abs(x) - 0.62) / 0.20) ** 2,
                            False, "Lamp_Rear", rows=2, offset=0.007, min_facing=0.35)

    objs = {}
    objs["Body"] = faces_to_object("Body", fb.parts["Body"] + body_extra, mats, ridge_keys=ridge)
    objs["Glass"] = faces_to_object("Glass", fb.parts["Glass"], mats)

    door_specs = {
        "Door_FL": (FD, 1), "Door_FR": (FD, -1), "Door_RL": (RD, 1), "Door_RR": (RD, -1),
    }
    for name, (rng, side) in door_specs.items():
        faces = []
        s_front, s_rear = rng
        for coords, m in fb.parts[name]:
            nc = []
            for (x, y, z) in coords:
                s = -y
                if abs(s - s_front) < 1e-6:
                    s -= DOOR_GAP
                elif abs(s - s_rear) < 1e-6:
                    s += DOOR_GAP
                nc.append((x, -s, z))
            faces.append((nc, m))

        if name in ("Door_FL", "Door_FR"):
            mx = side * (body_half_width(0.82) + 0.035)
            faces += rounded_box_faces((mx, -0.84, 0.985), (0.15, 0.08, 0.11), "Paint", bevel=0.03)
            faces += box_faces((side * (body_half_width(0.82) - 0.03), -0.86, 0.955), (0.05, 0.05, 0.05), "Trim")

        hs = s_front - 0.70 * (s_front - s_rear)
        hz = interp(BELT, hs) - 0.08
        p = project_side(bvh, side, hs, hz)
        if p is not None:
            faces += box_faces((p[0], -hs, hz), (0.02, 0.17, 0.025), "Trim")
        sh = s_front - DOOR_GAP
        hinge_x = side * (body_half_width(sh) - 0.03)
        hinge = (hinge_x, -sh, 0.62)

        ob = faces_to_object(name, faces, mats, origin=hinge)
        n_rim = add_solidify(ob, 0.025, mats["Trim"])
        print(f"[build_car] {name}: 끝단 림 {n_rim} 면 → Trim")
        objs[name] = ob

    objs["Headlights"] = faces_to_object("Headlights", head, mats)
    objs["TailLights"] = faces_to_object("TailLights", tail, mats)

    seats = []
    for sx in (0.37, -0.37):
        seats += rounded_box_faces((sx, 0.20, 0.42), (0.50, 0.52, 0.12), "Seat", bevel=0.03)
        seats += rounded_box_faces((sx, 0.48, 0.74), (0.50, 0.12, 0.62), "Seat", rot_x=-0.22, bevel=0.04)
        seats += rounded_box_faces((sx, 0.58, 1.12), (0.26, 0.10, 0.17), "Seat", rot_x=-0.22, bevel=0.03)
    seats += rounded_box_faces((0, 0.98, 0.44), (1.14, 0.50, 0.13), "Seat", bevel=0.04)
    seats += rounded_box_faces((0, 1.24, 0.74), (1.14, 0.12, 0.56), "Seat", rot_x=-0.30, bevel=0.05)
    for sx in (0.42, -0.42):
        seats += rounded_box_faces((sx, 1.33, 1.07), (0.24, 0.09, 0.14), "Seat", rot_x=-0.30, bevel=0.03)
    objs["Seats"] = faces_to_object("Seats", seats, mats, all_smooth=False)

    il = []
    xs = linspace(-0.30, 0.30, 5)
    ss = [s for s in stations() if -0.62 <= s <= -0.07]
    for i in range(len(ss) - 1):
        for j in range(len(xs) - 1):
            q = [(xs[j], ss[i]), (xs[j + 1], ss[i]), (xs[j + 1], ss[i + 1]), (xs[j], ss[i + 1])]
            il.append(([(x, -s, roof_inner_z(s, x) - 0.015) for x, s in q], "InteriorLight"))
    objs["InteriorLight"] = faces_to_object("InteriorLight", il, mats, all_smooth=False)

    for name, a, side in (("Wheel_FL", FRONT_AXLE, 1), ("Wheel_FR", FRONT_AXLE, -1),
                          ("Wheel_RL", REAR_AXLE, 1), ("Wheel_RR", REAR_AXLE, -1)):
        center = (side * TRACK_HALF, -a, TIRE_R)
        wf = [([(p[0] + center[0], p[1] + center[1], p[2] + center[2]) for p in c], m) for c, m in wheel_faces(side)]
        objs[name] = faces_to_object(name, wf, mats, origin=center, smooth_angle=0.5)

    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    bpy.ops.export_scene.gltf(
        filepath=OUT,
        export_format="GLB",
        export_yup=True,
        export_apply=True,
        export_draco_mesh_compression_enable=False,
        export_cameras=False,
        export_lights=False,
        export_materials="EXPORT",
        export_normals=True,
        export_texcoords=False,
    )
    tris = 0
    for ob in bpy.context.scene.objects:
        if ob.type == "MESH":
            ob.data.calc_loop_triangles()
            tris += len(ob.data.loop_triangles)
    print(f"[build_car] wrote {OUT}  triangles={tris}  size={os.path.getsize(OUT)} B")

def project_side(bvh, side, s, z):
    o = Vector((side * 3.0, -s, z))
    d = Vector((-side, 0, 0))
    loc, nrm, _, _ = bvh.ray_cast(o, d)
    if loc is None:
        return None
    return loc + Vector((side * 0.006, 0, 0))

if __name__ == "__main__":
    try:
        main()
    except Exception:
        import traceback
        traceback.print_exc()
        sys.exit(1)
