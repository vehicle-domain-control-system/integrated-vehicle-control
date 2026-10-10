import math
import os

import bpy
from mathutils import Vector

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(HERE, "..", ".."))
GLB = os.path.join(ROOT, "app", "src", "main", "assets", "models", "car.glb")
OUT_DIR = os.path.join(ROOT, ".harness", "previews")

def setup_scene():
    bpy.ops.wm.read_factory_settings(use_empty=True)
    bpy.ops.import_scene.gltf(filepath=GLB)
    sc = bpy.context.scene
    for eng in ("BLENDER_EEVEE_NEXT", "BLENDER_EEVEE"):
        try:
            sc.render.engine = eng
            break
        except TypeError:
            continue
    sc.render.resolution_x = 800
    sc.render.resolution_y = 450
    sc.render.film_transparent = False
    try:
        sc.eevee.taa_render_samples = 16
    except AttributeError:
        pass
    sc.view_settings.view_transform = "Standard"
    world = bpy.data.worlds.new("w")
    world.use_nodes = True
    bg = world.node_tree.nodes["Background"]
    bg.inputs[0].default_value = (0.62, 0.66, 0.72, 1.0)
    bg.inputs[1].default_value = 0.9
    sc.world = world

    bpy.ops.mesh.primitive_plane_add(size=40, location=(0, 0, 0))
    floor = bpy.context.active_object
    fm = bpy.data.materials.new("floor")
    fm.use_nodes = True
    fm.node_tree.nodes["Principled BSDF"].inputs["Base Color"].default_value = (0.33, 0.34, 0.36, 1)
    floor.data.materials.append(fm)

    sun = bpy.data.lights.new("sun", "SUN")
    sun.energy = 3.0
    so = bpy.data.objects.new("sun", sun)
    so.rotation_euler = (math.radians(50), math.radians(10), math.radians(35))
    sc.collection.objects.link(so)

    cam = bpy.data.cameras.new("cam")
    cam.lens = 50
    co = bpy.data.objects.new("cam", cam)
    sc.collection.objects.link(co)
    sc.camera = co
    tgt = bpy.data.objects.new("tgt", None)
    sc.collection.objects.link(tgt)
    con = co.constraints.new("TRACK_TO")
    con.target = tgt
    con.track_axis = "TRACK_NEGATIVE_Z"
    con.up_axis = "UP_Y"
    return sc, co, tgt

def shot(sc, co, tgt, name, loc, look=(0, 0, 0.62), lens=50):
    co.location = loc
    tgt.location = look
    co.data.lens = lens
    sc.render.filepath = os.path.join(OUT_DIR, name + ".png")
    bpy.ops.render.render(write_still=True)
    print("[render]", sc.render.filepath)

def main():
    os.makedirs(OUT_DIR, exist_ok=True)
    sc, co, tgt = setup_scene()

    shot(sc, co, tgt, "front34", (5.2, -6.6, 2.1))
    shot(sc, co, tgt, "side", (11.0, 0.0, 1.0), lens=60)
    shot(sc, co, tgt, "rear34", (-5.2, 6.6, 2.3))
    shot(sc, co, tgt, "front", (0.0, -9.0, 1.2), lens=60)
    shot(sc, co, tgt, "top", (0.01, 0.0, 13.0), look=(0, 0, 0), lens=45)
    shot(sc, co, tgt, "front_low", (0.0, -7.5, 0.30), look=(0, 0, 0.45), lens=60)
    shot(sc, co, tgt, "bpillar_close", (3.2, 0.35, 1.15), look=(0.88, 0.12, 0.85), lens=50)

    ang = math.radians(60)
    for n, sgn in (("Door_FL", -1), ("Door_RL", -1), ("Door_FR", 1), ("Door_RR", 1)):
        ob = bpy.data.objects[n]
        ob.rotation_mode = "XYZ"
        ob.rotation_euler = (ob.rotation_euler[0], ob.rotation_euler[1], ob.rotation_euler[2] + sgn * ang)
    shot(sc, co, tgt, "doors_open", (6.0, -5.0, 3.4))
    shot(sc, co, tgt, "doors_open_top", (0.01, 0.0, 13.0), look=(0, 0, 0), lens=45)

if __name__ == "__main__":
    main()
