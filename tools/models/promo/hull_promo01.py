# Promo hull 01 (author's reference D:/tmp/___6.png, 2026-11-26): a long low slab-hull capital ship, ~180 m, shell only.
# Procedural, seeded: terraced lofted hull layers, a broken superstructure spine, armour plates on the sides / bottom,
# deck greebles, masts, blue light bays and the stern's glow arc. Bow +X, up +Z, metres.
# Run: blender -b --factory-startup -P hull_promo01.py -- <out_dir> [render]
import bpy, bmesh, math, random, sys, os
from mathutils import Vector

argv = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
OUT = argv[0] if argv else "D:/__elite/promo/hull01"
RENDER = len(argv) > 1 and argv[1] == "render"
os.makedirs(OUT, exist_ok=True)

L = 180.0
HALF = L / 2
rng = random.Random(6)

bpy.ops.wm.read_factory_settings(use_empty=True)


# ---------------------------------------------------------------- materials
def material(name, color, metal, rough, emit=None, strength=0.0):
    m = bpy.data.materials.new(name)
    try:
        m.use_nodes = True
    except Exception:
        pass
    b = m.node_tree.nodes.get("Principled BSDF")
    b.inputs["Base Color"].default_value = (*color, 1.0)
    b.inputs["Metallic"].default_value = metal
    b.inputs["Roughness"].default_value = rough
    if emit:
        b.inputs["Emission Color"].default_value = (*emit, 1.0)
        b.inputs["Emission Strength"].default_value = strength
    return m


M_HULL = material("hull", (0.055, 0.06, 0.07), 0.75, 0.42)
M_PLATE_A = material("plate_light", (0.085, 0.09, 0.1), 0.8, 0.35)
M_PLATE_B = material("plate_dark", (0.03, 0.033, 0.04), 0.7, 0.5)
M_DETAIL = material("detail", (0.06, 0.062, 0.068), 0.6, 0.55)
M_GLOW = material("glow_blue", (0.1, 0.25, 0.4), 0.0, 0.3, (0.25, 0.65, 1.0), 40.0)
M_WINDOW = material("window", (0.2, 0.2, 0.2), 0.0, 0.3, (0.75, 0.88, 1.0), 2.5)


# ---------------------------------------------------------------- mesh builder
class MB:
    def __init__(self):
        self.v, self.f = [], []

    def box(self, c, s):
        cx, cy, cz = c
        hx, hy, hz = s[0] / 2, s[1] / 2, s[2] / 2
        b = len(self.v)
        for dx, dy, dz in ((-1, -1, -1), (1, -1, -1), (1, 1, -1), (-1, 1, -1),
                           (-1, -1, 1), (1, -1, 1), (1, 1, 1), (-1, 1, 1)):
            self.v.append((cx + dx * hx, cy + dy * hy, cz + dz * hz))
        for q in ((0, 3, 2, 1), (4, 5, 6, 7), (0, 1, 5, 4), (1, 2, 6, 5), (2, 3, 7, 6), (3, 0, 4, 7)):
            self.f.append(tuple(b + i for i in q))

    def loft(self, rings):
        b = len(self.v)
        n = len(rings[0])
        for r in rings:
            self.v.extend(r)
        for i in range(len(rings) - 1):
            for j in range(n):
                a0 = b + i * n + j
                a1 = b + i * n + (j + 1) % n
                self.f.append((a0, a1, a1 + n, a0 + n))
        self.f.append(tuple(b + j for j in reversed(range(n))))
        self.f.append(tuple(b + (len(rings) - 1) * n + j for j in range(n)))

    def cylinder(self, base, r, h, seg=10):
        rings = []
        for z in (base[2], base[2] + h):
            rings.append([(base[0] + r * math.cos(2 * math.pi * k / seg), base[1] + r * math.sin(2 * math.pi * k / seg), z)
                          for k in range(seg)])
        self.loft(rings)

    def build(self, name, mat, parent, bevel=0.0, smooth_angle=None):
        me = bpy.data.meshes.new(name)
        me.from_pydata(self.v, [], self.f)
        bm = bmesh.new()
        bm.from_mesh(me)
        bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
        bm.to_mesh(me)
        bm.free()
        me.materials.append(mat)
        ob = bpy.data.objects.new(name, me)
        bpy.context.collection.objects.link(ob)
        ob.parent = parent
        if bevel > 0:
            md = ob.modifiers.new("bevel", "BEVEL")
            md.width = bevel
            md.segments = 1
            md.limit_method = "ANGLE"
        if smooth_angle is not None:
            bpy.context.view_layer.objects.active = ob
            ob.select_set(True)
            try:
                bpy.ops.object.shade_auto_smooth(angle=math.radians(smooth_angle))
            except Exception:
                pass
            ob.select_set(False)
        return ob


def superellipse_ring(x, w, top, bot, n, seg=96):
    zc, hz = (top + bot) / 2, (top - bot) / 2
    pts = []
    for k in range(seg):
        th = 2 * math.pi * k / seg
        c, s = math.cos(th), math.sin(th)
        y = w * math.copysign(abs(c) ** (2 / n), c)
        z = zc + hz * math.copysign(abs(s) ** (2 / n), s)
        pts.append((x, y, z))
    return pts


def half_y(w, top, bot, n, z):
    """the half width of a superellipse section at height z"""
    zc, hz = (top + bot) / 2, (top - bot) / 2
    u = min(1.0, abs((z - zc) / hz))
    return w * (1 - u ** n) ** (1 / n)


# ---------------------------------------------------------------- hull layers
def lower_profile(x):
    t = x / HALF
    w, top, bot = 17.0, 2.0, -7.0
    if t > 0.55:   # bow: a narrowing wedge, the keel rising
        s = (t - 0.55) / 0.45
        w *= (1 - 0.3 * s ** 1.5) * math.sqrt(max(0.04, 1 - 0.96 * s ** 5))   # tapering, rounded at the tip
        bot += 5.5 * s ** 2
        top -= 0.8 * s ** 2
    if t < -0.86:   # stern: rounded in plan
        s = min(1.0, (-0.86 - t) / 0.14)
        w *= math.sqrt(max(0.05, 1 - 0.8 * s * s))
        bot += 1.0 * s
    return w, top, bot


def mid_profile(x):
    w, top, bot = lower_profile(x)
    return w * 0.88, 6.2, 1.0


def stations(x0, x1, step):
    n = max(2, int(round((x1 - x0) / step)) + 1)
    return [x0 + (x1 - x0) * i / (n - 1) for i in range(n)]


root = bpy.data.objects.new("hull_promo01", None)
bpy.context.collection.objects.link(root)

hull = MB()
hull.loft([superellipse_ring(x, *lower_profile(x), 5.0) for x in stations(-HALF, HALF, 1.5)])
hull.loft([superellipse_ring(x, *mid_profile(x), 7.0) for x in stations(-HALF + 6, HALF - 22, 1.5)])
# the rim ledge between the layers
ledge_x = stations(-HALF + 4, HALF - 14, 1.5)
hull.loft([superellipse_ring(x, lower_profile(x)[0] + 0.7, 2.4, 1.4, 10.0, 48) for x in ledge_x])

# superstructure: a broken spine of chamfered blocks, the bridge tower in the stern third
spine = MB()
spine_blocks = []
x = -HALF + 12
while x < HALF - 34:
    seg_len = rng.uniform(10, 28)
    x1 = min(x + seg_len, HALF - 34)
    w = rng.uniform(6.5, 11.0)
    top = rng.uniform(8.5, 11.5)
    if -55 < x < -25:
        top, w = 15.5, 9.0
    c = min(2.0, (x1 - x) * 0.2)
    xs = [x, x + c, x1 - c, x1]
    sc = [0.8, 1.0, 1.0, 0.8]
    dz = [1.2, 0.0, 0.0, 1.2]
    spine.loft([superellipse_ring(xi, w * k, top - d, 5.5, 14.0, 64) for xi, k, d in zip(xs, sc, dz)])
    spine_blocks.append((x + c, x1 - c, w, top))
    if x1 - x > 14 and rng.random() < 0.6:   # a second tier
        tw, tl = w * rng.uniform(0.45, 0.7), (x1 - x) * rng.uniform(0.3, 0.6)
        tx = rng.uniform(x + c + tl / 2, x1 - c - tl / 2)
        th = rng.uniform(1.5, 3.5)
        spine.loft([superellipse_ring(xi, tw * k, top + th - d * 0.5, top - 0.5, 14.0, 48)
                    for xi, k, d in zip((tx - tl / 2, tx - tl / 2 + 0.8, tx + tl / 2 - 0.8, tx + tl / 2), sc, dz)])
        spine_blocks.append((tx - tl / 2 + 0.8, tx + tl / 2 - 0.8, tw, top + th))
    # stepped side pods along the block
    if rng.random() < 0.7:
        for side in (-1, 1):
            pw = rng.uniform(2.0, 4.0)
            spine.box(((x + x1) / 2, side * (w + pw / 2 - 0.5), 6.0 + 1.2), (x1 - x - 2 * c, pw, 2.4))
    x = x1 + rng.uniform(1.5, 4.0)

# ---------------------------------------------------------------- plates
plates = {M_PLATE_A: MB(), M_PLATE_B: MB(), M_HULL: MB()}


def pick():
    r = rng.random()
    return M_PLATE_A if r < 0.45 else (M_PLATE_B if r < 0.85 else M_HULL)


def side_band(prof, n, x0, x1, z0, z1, rows, inset=0.0):
    hgt = (z1 - z0) / rows
    for r in range(rows):
        zc = z0 + hgt * (r + 0.5)
        x = x0
        while x < x1:
            ln = min(rng.uniform(3, 14), x1 - x)
            if ln < 1.0:
                break
            xc = x + ln / 2
            prot = rng.uniform(0.15, 0.6)
            ph = hgt * rng.uniform(0.75, 0.97)
            wa = min(half_y(*prof(x + 0.2), n, zc), half_y(*prof(x + ln - 0.2), n, zc))
            for side in (-1, 1):
                plates[pick()].box((xc, side * (wa + prot / 2 - 0.3 - inset), zc), (ln - 0.35, prot + 0.6, ph))
            x += ln + rng.uniform(0.2, 0.8)


side_band(lower_profile, 5.0, -HALF + 10, HALF - 16, -4.6, 1.2, 3)
side_band(mid_profile, 7.0, -HALF + 10, HALF - 26, 1.8, 5.8, 2)

# bottom plates
for i in range(260):
    x = rng.uniform(-HALF + 10, HALF - 32)
    w, top, bot = lower_profile(x)
    y = rng.uniform(-0.7, 0.7) * w
    sx, sy = rng.uniform(4, 16), rng.uniform(2, 7)
    plates[pick()].box((x, y, bot + 0.1), (sx, sy, rng.uniform(0.3, 0.9)))

# deck greebles on the mid deck, weighted small
greeb = MB()
for i in range(2400):
    x = rng.uniform(-HALF + 8, HALF - 24)
    w, top, bot = mid_profile(x)
    y = rng.uniform(-0.85, 0.85) * w
    sx = rng.uniform(0.8, 1.0) * (rng.random() ** 2 * 9 + 0.8)
    sy = rng.random() ** 2 * 5 + 0.6
    sz = rng.random() ** 3 * 3 + 0.25
    if rng.random() < 0.5:
        sx, sy = sy, sx
    (greeb if rng.random() < 0.6 else plates[pick()]).box((x, y, top - 0.2 + sz / 2), (sx, sy, sz))
# greebles on the spine tops and sides (each block's own top)
for (bx0, bx1, bw, btop) in spine_blocks:
    area = (bx1 - bx0) * bw * 2
    for i in range(int(area / 6)):
        x = rng.uniform(bx0 + 0.5, bx1 - 0.5)
        y = rng.uniform(-0.8, 0.8) * bw
        sz = rng.random() ** 3 * 1.8 + 0.15
        sx, sy = rng.random() ** 2 * 4 + 0.4, rng.random() ** 2 * 2.5 + 0.4
        (greeb if rng.random() < 0.6 else plates[pick()]).box((x, y, btop - 0.25 + sz / 2), (sx, sy, sz))
    for side in (-1, 1):   # side panelling of the block
        x = bx0
        while x < bx1 - 1:
            ln = min(rng.uniform(2, 7), bx1 - x)
            ph = rng.uniform(0.8, (btop - 6.5) * 0.8)
            plates[pick()].box((x + ln / 2, side * (bw - 0.1), 6.3 + ph / 2 + rng.uniform(0, 1)),
                               (ln - 0.3, rng.uniform(0.3, 0.7), ph))
            x += ln + rng.uniform(0.1, 0.6)
# bow deck (the lower hull's top ahead of the mid deck)
for i in range(500):
    x = rng.uniform(HALF - 26, HALF - 6)
    w, top, bot = lower_profile(x)
    y = rng.uniform(-0.75, 0.75) * w
    sz = rng.random() ** 3 * 1.6 + 0.2
    greeb.box((x, y, top - 0.2 + sz / 2), (rng.random() ** 2 * 4 + 0.5, rng.random() ** 2 * 3 + 0.5, sz))
# long conduits along the deck edges
for side in (-1, 1):
    for k, off in enumerate((0.78, 0.68)):
        x0 = -HALF + 12
        while x0 < HALF - 30:
            ln = rng.uniform(15, 40)
            x1 = min(x0 + ln, HALF - 30)
            w = mid_profile((x0 + x1) / 2)[0]
            greeb.box(((x0 + x1) / 2, side * w * off, 6.2 + 0.4), (x1 - x0, 0.7, 0.8))
            x0 = x1 + rng.uniform(2, 6)

# masts and dishes on the forward spine
masts = MB()
for i in range(9):
    x = rng.uniform(10, 50)
    y = rng.uniform(-3, 3)
    h = rng.uniform(4, 13)
    masts.cylinder((x, y, 9.0), rng.uniform(0.12, 0.3), h, 8)
    if rng.random() < 0.6:
        masts.box((x, y, 9.0 + h * rng.uniform(0.5, 0.9)), (0.15, rng.uniform(1.5, 4), 0.15))
for i in range(4):
    x = rng.uniform(-45, 40)
    side = rng.choice((-1, 1))
    masts.cylinder((x, side * rng.uniform(2, 4), 10.0), rng.uniform(1.0, 2.0), 0.4, 18)
    masts.cylinder((x, side * rng.uniform(2, 4), 9.0), 0.3, 1.2, 8)

# ---------------------------------------------------------------- lights
glow = MB()
windows = MB()
# light bays on the sides: recessed glowing panels
for side in (-1, 1):
    for i in range(9):
        x = rng.uniform(-HALF + 14, HALF - 34)
        zc = rng.choice((-1.8, 3.6))
        prof, n = (lower_profile, 5.0) if zc < 1 else (mid_profile, 7.0)
        wa = half_y(*prof(x), n, zc)
        ln = rng.uniform(1.5, 4.5)
        glow.box((x, side * (wa + 0.05), zc), (ln, 0.3, rng.uniform(0.5, 1.1)))
# window rows: sparse dots
for side in (-1, 1):
    for row_z, prof, n in ((4.6, mid_profile, 7.0), (2.9, mid_profile, 7.0), (-0.6, lower_profile, 5.0)):
        x = -HALF + 12
        while x < HALF - 30:
            if rng.random() < 0.18:
                for k in range(rng.randint(2, 7)):
                    xx = x + k * 1.3
                    if xx > HALF - 30:
                        break
                    wa = half_y(*prof(xx), n, row_z)
                    windows.box((xx, side * (wa + 0.42), row_z), (0.4, 0.2, 0.22))
                x += 20
            else:
                x += rng.uniform(4, 12)
# the stern's glow arc: a ribbon along the rounded stern below the waterline of the plates
arc = []
for k in range(41):
    a = -math.pi / 2 + math.pi * k / 40
    arc.append(a)
sx0 = -HALF * 0.86
rx = HALF - abs(sx0)
for z0 in (-5.6,):
    pts = []
    for a in arc:
        x = sx0 - rx * math.cos(a) * 1.0
        w = lower_profile(x)[0]
        y = w * math.sin(a)
        pts.append((x, y))
    for i in range(len(pts) - 1):
        (x0, y0), (x1, y1) = pts[i], pts[i + 1]
        dx, dy = x1 - x0, y1 - y0
        ln = math.hypot(dx, dy)
        nx, ny = -dy / ln, dx / ln   # outward (the arc runs clockwise around the stern)
        if nx > 0:
            nx, ny = -nx, -ny
        cx, cy = (x0 + x1) / 2 + nx * 0.6, (y0 + y1) / 2 + ny * 0.6
        b = len(glow.v)
        for (px, py) in ((x0 + nx * 0.4, y0 + ny * 0.4), (x1 + nx * 0.4, y1 + ny * 0.4)):
            pass
        # a thin box rotated along the segment
        ang = math.atan2(dy, dx)
        hx, hy, hz = ln / 2 + 0.05, 0.3, 0.7
        ca, sa = math.cos(ang), math.sin(ang)
        for ddx, ddy, ddz in ((-1, -1, -1), (1, -1, -1), (1, 1, -1), (-1, 1, -1),
                              (-1, -1, 1), (1, -1, 1), (1, 1, 1), (-1, 1, 1)):
            lx, ly = ddx * hx, ddy * hy
            glow.v.append((cx + lx * ca - ly * sa, cy + lx * sa + ly * ca, z0 + ddz * hz))
        for q in ((0, 3, 2, 1), (4, 5, 6, 7), (0, 1, 5, 4), (1, 2, 6, 5), (2, 3, 7, 6), (3, 0, 4, 7)):
            glow.f.append(tuple(b + i for i in q))
# stern nozzles: glowing discs on the stern face
for y in (-4.5, 4.5):
    glow.box((-HALF - 0.05, y, -2.8), (0.3, 3.5, 0.35))

# ---------------------------------------------------------------- objects
hull.build("hull", M_HULL, root, smooth_angle=40)
spine.build("superstructure", M_HULL, root, smooth_angle=40)
for m, mb in plates.items():
    mb.build("plates_" + m.name, m, root, bevel=0.06)
greeb.build("greebles", M_DETAIL, root, bevel=0.04)
masts.build("masts", M_DETAIL, root, smooth_angle=40)
glow.build("lights_blue", M_GLOW, root)
windows.build("windows", M_WINDOW, root)

polys = sum(len(o.data.polygons) for o in bpy.data.objects if o.type == "MESH")
dims = [max(v.co[i] for o in bpy.data.objects if o.type == "MESH" for v in o.data.vertices) -
        min(v.co[i] for o in bpy.data.objects if o.type == "MESH" for v in o.data.vertices) for i in range(3)]
print(f"[hull_promo01] faces before bevel {polys}, size {dims[0]:.1f} x {dims[1]:.1f} x {dims[2]:.1f} m")

# ---------------------------------------------------------------- export
bpy.ops.wm.save_as_mainfile(filepath=os.path.join(OUT, "hull_promo01.blend"))
bpy.ops.export_scene.gltf(filepath=os.path.join(OUT, "hull_promo01.glb"), export_format="GLB", export_apply=True)
bpy.ops.wm.obj_export(filepath=os.path.join(OUT, "hull_promo01.obj"), apply_modifiers=True, export_materials=True,
                      forward_axis="NEGATIVE_Z", up_axis="Y")

if not RENDER:
    sys.exit(0)

# ---------------------------------------------------------------- preview renders
sc = bpy.context.scene
sc.render.engine = "CYCLES"
sc.cycles.samples = 48
sc.cycles.use_denoising = True
try:
    prefs = bpy.context.preferences.addons["cycles"].preferences
    for dev in ("OPTIX", "CUDA", "HIP", "ONEAPI"):
        try:
            prefs.compute_device_type = dev
            prefs.get_devices()
            if any(d.type == dev for d in prefs.devices):
                for d in prefs.devices:
                    d.use = d.type == dev
                sc.cycles.device = "GPU"
                print("[hull_promo01] render on", dev)
                break
        except Exception:
            continue
except Exception:
    pass
sc.render.resolution_x, sc.render.resolution_y = 1280, 720
sc.view_settings.view_transform = "AgX"
world = bpy.data.worlds.new("space")
sc.world = world
try:
    world.use_nodes = True
except Exception:
    pass
world.node_tree.nodes["Background"].inputs["Color"].default_value = (0.003, 0.005, 0.01, 1)

sun = bpy.data.objects.new("sun", bpy.data.lights.new("sun", "SUN"))
sun.data.energy = 4.0
sun.data.color = (0.85, 0.92, 1.0)
sun.rotation_euler = (math.radians(55), math.radians(-25), math.radians(140))
bpy.context.collection.objects.link(sun)
fill = bpy.data.objects.new("fill", bpy.data.lights.new("fill", "SUN"))
fill.data.energy = 0.35
fill.data.color = (0.4, 0.55, 0.9)
fill.rotation_euler = (math.radians(120), 0, math.radians(-40))
bpy.context.collection.objects.link(fill)

cam = bpy.data.objects.new("cam", bpy.data.cameras.new("cam"))
bpy.context.collection.objects.link(cam)
sc.camera = cam


def shoot(name, loc, target, lens):
    cam.location = Vector(loc)
    d = Vector(target) - cam.location
    cam.rotation_euler = d.to_track_quat("-Z", "Y").to_euler()
    cam.data.lens = lens
    cam.data.clip_end = 5000
    sc.render.filepath = os.path.join(OUT, f"preview_{name}.png")
    bpy.ops.render.render(write_still=True)


shoot("ref", (-175, -120, 115), (-5, 0, 0), 38)
shoot("side", (0, -260, 10), (0, 0, 2), 40)
shoot("top", (30, -60, 230), (0, 0, 0), 40)
shoot("bow", (170, -90, 40), (20, 0, 2), 40)
