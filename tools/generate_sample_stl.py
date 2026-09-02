#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""生成演示用二进制 STL（无第三方依赖）。

用法：python tools/generate_sample_stl.py

输出到 assets/models/：
  - cube_20mm.stl        20×20×20 mm 立方体（单轮廓）
  - box_with_hole.stl    40×40×10 mm 方块带 20×20 mm 方孔（多轮廓，展示内外壁）

法线由叉积 (b-a)×(c-a) 计算，顶点顺序保证朝外（右手定则）。
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
    quad(tris, 4, 5, 13, 12)  # y=-i, +y
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


def main():
    out = os.path.abspath(OUT_DIR)
    os.makedirs(out, exist_ok=True)

    verts, tris = make_cube(10.0)
    write_binary_stl(os.path.join(out, "cube_20mm.stl"), verts, tris)
    print(f"[OK] cube_20mm.stl        ({len(tris)} triangles)")

    verts, tris = make_box_with_hole(40.0, 20.0, 10.0)
    write_binary_stl(os.path.join(out, "box_with_hole.stl"), verts, tris)
    print(f"[OK] box_with_hole.stl    ({len(tris)} triangles)")

    print(f"\n输出目录: {out}")


if __name__ == "__main__":
    main()
