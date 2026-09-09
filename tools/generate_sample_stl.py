#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""生成演示用二进制 STL（无第三方依赖）。

用法：python tools/generate_sample_stl.py

输出到 assets/models/：
  - cube_20mm.stl        20×20×20 mm 立方体（单轮廓）
  - box_with_hole.stl    40×40×10 mm 方块带 20×20 mm 方孔（多轮廓，展示内外壁）
  - hollow_cylinder.stl  空心圆柱（V2.0 验收：每层 1 外环 + 1 内环）
  - plate_3holes.stl     方板带 3 个方孔（V2.0 验收：1 外环 + 3 内环）
  - sphere_r30_131k.stl  高密度 UV 球 ≈13 万面（V2.0 性能基线模型）

法线由叉积 (b-a)×(c-a) 计算。所有模型经有符号体积自校验，保证绕序朝外。
"""

import math
import os
import struct

OUT_DIR = os.path.join(os.path.dirname(__file__), "..", "assets", "models")


def quad(tris, a, b, c, d):
    """把四边形 (a,b,c,d) 拆成两个三角形，顺序保证法线朝外。"""
    tris.append((a, b, c))
    tris.append((a, c, d))


def write_binary_stl(path, verts, tris):
    """verts: [(x,y,z), ...]; tris: [(i,j,k), ...]（逆时针朝外）。"""
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, "wb") as f:
        f.write(b"\0" * 80)  # header
        f.write(struct.pack("<I", len(tris)))
        for i, j, k in tris:
            a, b, c = verts[i], verts[j], verts[k]
            u = (b[0] - a[0], b[1] - a[1], b[2] - a[2])
            v = (c[0] - a[0], c[1] - a[1], c[2] - a[2])
            n = (
                u[1] * v[2] - u[2] * v[1],
                u[2] * v[0] - u[0] * v[2],
                u[0] * v[1] - u[1] * v[0],
            )
            ln = math.sqrt(n[0] ** 2 + n[1] ** 2 + n[2] ** 2) or 1.0
            n = (n[0] / ln, n[1] / ln, n[2] / ln)
            f.write(struct.pack("<3f", *n))
            f.write(struct.pack("<3f", *a))
            f.write(struct.pack("<3f", *b))
            f.write(struct.pack("<3f", *c))
            f.write(struct.pack("<H", 0))  # attribute byte count


def make_cube(half):
    """中心在原点、半边长 half 的立方体。"""
    h = half
    verts = [
        (-h, -h, -h), (h, -h, -h), (h, h, -h), (-h, h, -h),  # 底 z=-h
        (-h, -h, h), (h, -h, h), (h, h, h), (-h, h, h),      # 顶 z=+h
    ]
    tris = []
    quad(tris, 4, 5, 6, 7)  # +z
    quad(tris, 1, 0, 3, 2)  # -z
    quad(tris, 0, 1, 5, 4)  # -y
    quad(tris, 2, 3, 7, 6)  # +y
    quad(tris, 3, 0, 4, 7)  # -x
    quad(tris, 1, 2, 6, 5)  # +x
    return verts, tris


def make_box_with_hole(outer, inner, h):
    """外方 outer×outer、内方孔 inner×inner、高 h 的方环（中心在 xy 原点，z∈[0,h]）。"""
    o, i = outer / 2.0, inner / 2.0
    verts = [
        # 底 z=0
        (-o, -o, 0), (o, -o, 0), (o, o, 0), (-o, o, 0),   # 外 0..3
        (-i, -i, 0), (i, -i, 0), (i, i, 0), (-i, i, 0),   # 内 4..7
        # 顶 z=h
        (-o, -o, h), (o, -o, h), (o, o, h), (-o, o, h),   # 外 8..11
        (-i, -i, h), (i, -i, h), (i, i, h), (-i, i, h),   # 内 12..15
    ]
    tris = []
    # 外壁
    quad(tris, 1, 2, 10, 9)   # x=+o, +x
    quad(tris, 3, 0, 8, 11)   # x=-o, -x
    quad(tris, 2, 3, 11, 10)  # y=+o, +y
    quad(tris, 0, 1, 9, 8)    # y=-o, -y
    # 内壁（法线朝孔中心）
    quad(tris, 6, 5, 13, 14)  # x=+i, -x
    quad(tris, 4, 7, 15, 12)  # x=-i, +x
    quad(tris, 7, 6, 14, 15)  # y=+i, -y
    quad(tris, 5, 4, 12, 13)  # y=-i, +y
    # 顶面（+z）
    quad(tris, 8, 9, 13, 12)
    quad(tris, 9, 10, 14, 13)
    quad(tris, 10, 11, 15, 14)
    quad(tris, 11, 8, 12, 15)
    # 底面（-z）
    quad(tris, 4, 5, 1, 0)
    quad(tris, 5, 6, 2, 1)
    quad(tris, 6, 7, 3, 2)
    quad(tris, 7, 4, 0, 3)
    return verts, tris


def signed_volume(verts, tris):
    """网格有符号体积：闭合且绕序朝外时为正。"""
    vol = 0.0
    for i, j, k in tris:
        a, b, c = verts[i], verts[j], verts[k]
        vol += (a[0] * (b[1] * c[2] - b[2] * c[1])
                - a[1] * (b[0] * c[2] - b[2] * c[0])
                + a[2] * (b[0] * c[1] - b[1] * c[0]))
    return vol / 6.0


def fix_winding(verts, tris, name):
    """有符号体积为负则整体翻转绕序（保证法向朝外），返回修正后的 tris。"""
    vol = signed_volume(verts, tris)
    if vol < 0:
        print(f"  [warn] {name}: signed volume {vol:.3f} < 0, 翻转全部三角形绕序")
        tris = [(i, k, j) for i, j, k in tris]
        vol = -vol
    print(f"  [check] {name}: signed volume = {vol:.3f} mm^3")
    return tris


def make_uv_sphere(radius, n_seg, n_ring):
    """中心在原点的 UV 球。n_seg 经向分段 × n_ring 纬向分段。

    三角形数 = 2*n_seg*(n_ring-1)。256×256 ≈ 13 万面，用于性能基线。
    """
    verts = [(0.0, 0.0, -radius)]  # 南极 0
    for ring in range(1, n_ring):
        theta = math.pi * ring / n_ring          # 0(南极) → pi(北极)
        z = -radius * math.cos(theta)
        r_xy = radius * math.sin(theta)
        for seg in range(n_seg):
            phi = 2.0 * math.pi * seg / n_seg
            verts.append((r_xy * math.cos(phi), r_xy * math.sin(phi), z))
    verts.append((0.0, 0.0, radius))             # 北极（最后）
    north = len(verts) - 1

    def v(ring, seg):
        """中间环（1..n_ring-1）上的顶点索引。"""
        return 1 + (ring - 1) * n_seg + (seg % n_seg)

    tris = []
    # 南极帽
    for seg in range(n_seg):
        tris.append((0, v(1, seg + 1), v(1, seg)))
    # 中间环带
    for ring in range(1, n_ring - 1):
        for seg in range(n_seg):
            quad(tris, v(ring, seg), v(ring, seg + 1),
                 v(ring + 1, seg + 1), v(ring + 1, seg))
    # 北极帽
    for seg in range(n_seg):
        tris.append((v(n_ring - 1, seg), v(n_ring - 1, seg + 1), north))
    return verts, tris


def make_hollow_cylinder(r_out, r_in, h, n):
    """轴为 z 轴的空心圆柱，z∈[0,h]，n 分段。切片轮廓 = 1 外环 + 1 内环。"""
    verts = []
    for z in (0.0, h):
        for r in (r_out, r_in):
            for seg in range(n):
                phi = 2.0 * math.pi * seg / n
                verts.append((r * math.cos(phi), r * math.sin(phi), z))

    # 索引：层(0底/1顶) * 2n + 圈(0外/1内) * n + seg
    def idx(zi, ri_, seg):
        return zi * 2 * n + ri_ * n + (seg % n)

    tris = []
    for seg in range(n):
        s1 = seg + 1
        # 外壁（法向朝外）
        quad(tris, idx(0, 0, seg), idx(0, 0, s1), idx(1, 0, s1), idx(1, 0, seg))
        # 内壁（法向朝孔内 = 外壁绕序取反）
        quad(tris, idx(0, 1, s1), idx(0, 1, seg), idx(1, 1, seg), idx(1, 1, s1))
        # 顶环面（+z）：外圈 CCW，内圈 CW
        quad(tris, idx(1, 0, seg), idx(1, 0, s1), idx(1, 1, s1), idx(1, 1, seg))
        # 底环面（-z）：绕序与顶面相反
        quad(tris, idx(0, 0, s1), idx(0, 0, seg), idx(0, 1, seg), idx(0, 1, s1))
    return verts, tris


def make_multi_hole_plate(outer, hole_half, h, holes_cx):
    """外方 outer×outer、高 h 的板，沿 x 轴排列若干方孔（半宽 hole_half，y∈[-hole_half,hole_half]）。

    holes_cx 必须递增且不重叠。切片轮廓 = 1 外环 + N 内环。
    顶/底面用矩形条带剖分（孔沿 x 排列，带状区域可直接枚举矩形）。
    """
    o, i = outer / 2.0, hole_half
    tris = []

    def rect(tris_, x0, y0, x1, y1, z, up):
        """z 平面上轴对齐矩形拆两个三角形；up=True 法向 +z，否则 -z。"""
        a, b = (x0, y0, z), (x1, y0, z)
        c, d = (x1, y1, z), (x0, y1, z)
        if up:
            quad(tris_, a, b, c, d)
        else:
            quad(tris_, a, d, c, b)

    # 侧面直接用独立坐标逐面构造（STL 本无拓扑）。
    # 外壁 4 面（绕序模式与 make_box_with_hole 外壁一致，法向朝外）
    walls = [
        ((o, -o, 0.0), (o, o, 0.0), (o, o, h), (o, -o, h)),      # x=+o 朝 +x
        ((-o, o, 0.0), (-o, -o, 0.0), (-o, -o, h), (-o, o, h)),  # x=-o 朝 -x
        ((o, o, 0.0), (-o, o, 0.0), (-o, o, h), (o, o, h)),      # y=+o 朝 +y
        ((-o, -o, 0.0), (o, -o, 0.0), (o, -o, h), (-o, -o, h)),  # y=-o 朝 -y
    ]
    for a, b, c, d in walls:
        quad(tris, a, b, c, d)
    # 每个孔的内壁 4 面（法向朝孔内）
    for cx in holes_cx:
        x0, x1 = cx - i, cx + i
        inner = [
            ((x0, -i, 0.0), (x0, i, 0.0), (x0, i, h), (x0, -i, h)),  # x=x0 朝 +x(孔内)
            ((x1, i, 0.0), (x1, -i, 0.0), (x1, -i, h), (x1, i, h)),  # x=x1 朝 -x(孔内)
            ((x0, i, 0.0), (x1, i, 0.0), (x1, i, h), (x0, i, h)),    # y=+i 朝 -y(孔内)
            ((x1, -i, 0.0), (x0, -i, 0.0), (x0, -i, h), (x1, -i, h)),# y=-i 朝 +y(孔内)
        ]
        for a, b, c, d in inner:
            quad(tris, a, b, c, d)
    # 顶/底面条带剖分
    xs = [-o]
    for cx in holes_cx:
        xs += [cx - i, cx + i]
    xs.append(o)
    for z, up in ((h, True), (0.0, False)):
        rect(tris, -o, i, o, o, z, up)        # y∈[i, o] 整条
        rect(tris, -o, -o, o, -i, z, up)      # y∈[-o, -i] 整条
        for k in range(0, len(xs) - 1, 2):  # y∈[-i, i] 孔间分段（偶数段为实体，奇数段为孔）
            rect(tris, xs[k], -i, xs[k + 1], i, z, up)
    # 该函数用独立顶点坐标构造（无索引），转索引形式返回
    verts = []
    idx_tris = []
    for a, b, c in tris:
        base = len(verts)
        verts += [a, b, c]
        idx_tris.append((base, base + 1, base + 2))
    return verts, idx_tris


def main():
    out = os.path.abspath(OUT_DIR)
    os.makedirs(out, exist_ok=True)

    verts, tris = make_cube(10.0)
    write_binary_stl(os.path.join(out, "cube_20mm.stl"), verts,
                     fix_winding(verts, tris, "cube_20mm"))
    print(f"[OK] cube_20mm.stl        ({len(tris)} triangles)")

    verts, tris = make_box_with_hole(40.0, 20.0, 10.0)
    write_binary_stl(os.path.join(out, "box_with_hole.stl"), verts,
                     fix_winding(verts, tris, "box_with_hole"))
    print(f"[OK] box_with_hole.stl    ({len(tris)} triangles)")

    # V2.0 验收模型
    verts, tris = make_hollow_cylinder(20.0, 12.0, 30.0, 96)
    write_binary_stl(os.path.join(out, "hollow_cylinder.stl"), verts,
                     fix_winding(verts, tris, "hollow_cylinder"))
    print(f"[OK] hollow_cylinder.stl  ({len(tris)} triangles)")

    verts, tris = make_multi_hole_plate(60.0, 5.0, 8.0, [-20.0, 0.0, 20.0])
    write_binary_stl(os.path.join(out, "plate_3holes.stl"), verts,
                     fix_winding(verts, tris, "plate_3holes"))
    print(f"[OK] plate_3holes.stl     ({len(tris)} triangles)")

    # 性能基线模型：256×256 UV 球 ≈ 13 万面
    verts, tris = make_uv_sphere(30.0, 256, 256)
    write_binary_stl(os.path.join(out, "sphere_r30_131k.stl"), verts,
                     fix_winding(verts, tris, "sphere_r30_131k"))
    print(f"[OK] sphere_r30_131k.stl  ({len(tris)} triangles, {len(verts)} verts)")

    print(f"\n输出目录: {out}")


if __name__ == "__main__":
    main()
