import json
import os
import struct
import sys

REQUIRED_NODES = [
    "Body", "Door_FL", "Door_FR", "Door_RL", "Door_RR",
    "Wheel_FL", "Wheel_FR", "Wheel_RL", "Wheel_RR",
    "Glass", "InteriorLight", "Headlights", "TailLights", "Seats",
]
REQUIRED_MATERIALS = [
    "Paint", "Glass", "InteriorLight", "Tire", "Rim", "Lamp_Front", "Lamp_Rear", "Trim", "Seat",
]
MAX_TRIANGLES = 30000
MAX_BYTES = 3 * 1024 * 1024

failures = []

def check(cond, msg):
    print(("  OK   " if cond else "  FAIL ") + msg)
    if not cond:
        failures.append(msg)
    return cond

def read_glb(path):
    with open(path, "rb") as f:
        data = f.read()
    if len(data) < 20:
        raise ValueError("파일이 너무 작다")
    magic, version, length = struct.unpack_from("<4sII", data, 0)
    if magic != b"glTF":
        raise ValueError("magic 이 glTF 가 아니다: %r" % magic)
    if version != 2:
        raise ValueError("glTF version %d (2 필요)" % version)
    if length != len(data):
        raise ValueError("헤더 길이 %d ≠ 실제 %d" % (length, len(data)))
    clen, ctype = struct.unpack_from("<II", data, 12)
    if ctype != 0x4E4F534A:
        raise ValueError("첫 청크가 JSON 이 아니다")
    gltf = json.loads(data[20:20 + clen].decode("utf-8"))
    return gltf, len(data)

def mesh_bounds(gltf, mesh_idx):
    mn = [float("inf")] * 3
    mx = [float("-inf")] * 3
    for prim in gltf["meshes"][mesh_idx]["primitives"]:
        acc = gltf["accessors"][prim["attributes"]["POSITION"]]
        for i in range(3):
            mn[i] = min(mn[i], acc["min"][i])
            mx[i] = max(mx[i], acc["max"][i])
    return mn, mx

def mesh_triangles(gltf, mesh_idx):
    tris = 0
    for prim in gltf["meshes"][mesh_idx]["primitives"]:
        if prim.get("mode", 4) != 4:
            continue
        if "indices" in prim:
            tris += gltf["accessors"][prim["indices"]]["count"] // 3
        else:
            tris += gltf["accessors"][prim["attributes"]["POSITION"]]["count"] // 3
    return tris

def main(path):
    print("check_glb:", path)
    try:
        gltf, size = read_glb(path)
    except (OSError, ValueError) as e:
        print("  FAIL", e)
        return 1
    print("  info GLB 헤더·JSON 청크 파싱 완료 (glTF 2.0, %d B)" % size)

    used = gltf.get("extensionsUsed", [])
    required = gltf.get("extensionsRequired", [])
    check("KHR_draco_mesh_compression" not in used + required, "Draco 압축 미사용 (extensionsUsed=%s)" % used)
    check(len(required) == 0, "extensionsRequired 비어 있음 (KHR 확장 필수 아님 → Filament 로드 보장, 현재=%s)" % required)

    nodes = {n.get("name"): n for n in gltf.get("nodes", [])}
    for name in REQUIRED_NODES:
        n = nodes.get(name)
        check(n is not None and "mesh" in n, "노드 %-14s 존재·메시 보유" % name)

    mats = {m.get("name"): m for m in gltf.get("materials", [])}
    for name in REQUIRED_MATERIALS:
        check(name in mats, "재질 %-14s 존재" % name)
    if "Glass" in mats:
        check(mats["Glass"].get("alphaMode") == "BLEND", "Glass 재질 반투명 (alphaMode BLEND)")
    if "InteriorLight" in mats:
        ef = mats["InteriorLight"].get("emissiveFactor", [0, 0, 0])
        check(max(ef) > 0.0, "InteriorLight emissiveFactor=%s" % (ef,))

    for name in ("Lamp_Front", "Lamp_Rear"):
        if name in mats:
            ef = mats[name].get("emissiveFactor", [0, 0, 0])
            check(max(ef) == 0.0, "%s 발광 없음 (emissiveFactor=%s)" % (name, ef))

    mat_names = [m.get("name") for m in gltf.get("materials", [])]
    if "InteriorLight" in mat_names:
        il_idx = mat_names.index("InteriorLight")
        users = []
        for name, n in nodes.items():
            if "mesh" not in n:
                continue
            for prim in gltf["meshes"][n["mesh"]]["primitives"]:
                if prim.get("material") == il_idx:
                    users.append(name)
        check(sorted(set(users)) == ["InteriorLight"],
              "InteriorLight 재질 사용 노드 = %s (InteriorLight 노드만)" % sorted(set(users)))
        il_node = nodes.get("InteriorLight")
        if il_node is not None and "mesh" in il_node:
            prims = gltf["meshes"][il_node["mesh"]]["primitives"]
            check(all(p.get("material") == il_idx for p in prims),
                  "InteriorLight 노드의 모든 프리미티브가 InteriorLight 재질")

    gmn = [float("inf")] * 3
    gmx = [float("-inf")] * 3
    total_tris = 0
    for name, n in nodes.items():
        if "mesh" not in n:
            continue
        rot = n.get("rotation", [0, 0, 0, 1])
        scl = n.get("scale", [1, 1, 1])
        check(all(abs(a - b) < 1e-6 for a, b in zip(rot, [0, 0, 0, 1])) and
              all(abs(s - 1) < 1e-6 for s in scl), "노드 %-14s 회전·스케일 없음 (휴지 자세)" % name)
        t = n.get("translation", [0, 0, 0])
        mn, mx = mesh_bounds(gltf, n["mesh"])
        for i in range(3):
            gmn[i] = min(gmn[i], mn[i] + t[i])
            gmx[i] = max(gmx[i], mx[i] + t[i])
        total_tris += mesh_triangles(gltf, n["mesh"])
    L = gmx[2] - gmn[2]
    W = gmx[0] - gmn[0]
    H = gmx[1] - gmn[1]
    print("  info 전체 경계 min=%s max=%s" % ([round(v, 3) for v in gmn], [round(v, 3) for v in gmx]))
    check(4.55 <= L <= 4.75, "길이 %.3f m (목표 4.65, 전방 +Z)" % L)
    check(1.80 <= W <= 2.10, "폭 %.3f m (차체 1.825 + 사이드미러)" % W)
    check(1.38 <= H <= 1.46, "높이 %.3f m (목표 1.42)" % H)
    check(abs(gmn[1]) < 0.01, "바닥 y = %.4f (≈ 0)" % gmn[1])
    check(abs(gmn[2] + gmx[2]) < 0.05 and abs(gmn[0] + gmx[0]) < 0.05, "차량 중심 ≈ 원점 (x·z 대칭)")
    if "Body" in nodes:
        bmn, bmx = mesh_bounds(gltf, nodes["Body"]["mesh"])
        check(1.78 <= bmx[0] - bmn[0] <= 1.86, "차체 폭 %.3f m (목표 1.825)" % (bmx[0] - bmn[0]))

    wz = {}
    for w in ("Wheel_FL", "Wheel_FR", "Wheel_RL", "Wheel_RR"):
        if w in nodes:
            wz[w] = nodes[w].get("translation", [0, 0, 0])
    if len(wz) == 4:
        wb = wz["Wheel_FL"][2] - wz["Wheel_RL"][2]
        check(abs(wb - 2.72) < 0.02, "휠베이스 %.3f m (목표 2.72, 앞바퀴 +Z)" % wb)
        check(wz["Wheel_FL"][0] > 0 > wz["Wheel_FR"][0], "Wheel_FL +X(좌) / Wheel_FR −X(우)")

    print("  info 도어 피벗 (translation = 앞쪽 힌지 선, 노드 로컬 +Y 축 회전으로 개방)")
    doors = {}
    for name, side in (("Door_FL", 1), ("Door_FR", -1), ("Door_RL", 1), ("Door_RR", -1)):
        n = nodes.get(name)
        if n is None or "mesh" not in n:
            continue
        t = n.get("translation", [0, 0, 0])
        mn, mx = mesh_bounds(gltf, n["mesh"])
        doors[name] = t
        print("       %s translation=(%.3f, %.3f, %.3f)  local z∈[%.3f, %.3f]  열림 각도 부호 %s"
              % (name, t[0], t[1], t[2], mn[2], mx[2], "−(음수)" if side > 0 else "+(양수)"))
        check(0.6 <= side * t[0] <= 1.0, "%s 피벗이 %s 측면 (x=%.3f)" % (name, "좌(+X)" if side > 0 else "우(−X)", t[0]))
        check(0.2 <= t[1] <= 1.2, "%s 피벗 높이 y=%.3f" % (name, t[1]))
        check(mx[2] <= 0.02, "%s 메시 전체가 피벗 뒤쪽 (로컬 max z=%.3f ≤ 0.02 → 피벗=앞 모서리)" % (name, mx[2]))
        check(mn[2] <= -0.6, "%s 도어 길이 %.3f m" % (name, -mn[2]))

        outward = mx[0] if side > 0 else -mn[0]
        inward = -mn[0] if side > 0 else mx[0]
        check(outward <= 0.20 and inward <= 0.40,
              "%s 메시가 힌지 선 근처 측면 판 (바깥 %.3f ≤ 0.20, 안쪽 %.3f ≤ 0.40)" % (name, outward, inward))
    if len(doors) == 4:
        check(doors["Door_FL"][2] > doors["Door_RL"][2] and doors["Door_FR"][2] > doors["Door_RR"][2],
              "앞도어 힌지가 뒷도어 힌지보다 전방(+Z)")
        check(doors["Door_FL"][2] > 0.0, "앞도어 힌지가 차체 앞쪽 절반 (z=%.3f)" % doors["Door_FL"][2])

    check(total_tris <= MAX_TRIANGLES, "삼각형 %d 개 (≤ %d)" % (total_tris, MAX_TRIANGLES))
    check(size < MAX_BYTES, "파일 크기 %.1f KB (< 3 MB)" % (size / 1024.0))

    print("규약: +Y 위, +Z 전방, +X 좌측. 도어 개방 = 노드 로컬 +Y 회전, 좌측 도어 음수(−) / 우측 도어 양수(+).")
    if failures:
        print("FAIL: %d 건" % len(failures))
        return 1
    print("PASS")
    return 0

if __name__ == "__main__":
    if len(sys.argv) != 2:
        print("usage: check_glb.py <car.glb>")
        sys.exit(2)
    if not os.path.isfile(sys.argv[1]):
        print("FAIL 파일 없음:", sys.argv[1])
        sys.exit(1)
    sys.exit(main(sys.argv[1]))
