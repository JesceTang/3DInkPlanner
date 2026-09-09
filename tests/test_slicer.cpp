// Slicer 单元测试（无第三方框架，失败返回非零）。
#include "slicing/Slicer.h"

#include <cmath>
#include <iostream>
#include <string>
#include <vector>

namespace {

int g_failures = 0;

void check(bool cond, const std::string &msg) {
    if (cond) {
        std::cout << "[PASS] " << msg << "\n";
    } else {
        std::cerr << "[FAIL] " << msg << "\n";
        ++g_failures;
    }
}

bool nearD(double a, double b, double eps = 1e-9) {
    return std::fabs(a - b) < eps;
}

geometry::Triangle tri(float x0, float y0, float z0,
                       float x1, float y1, float z1,
                       float x2, float y2, float z2) {
    geometry::Triangle t;
    t.v0 = geometry::Vec3f(x0, y0, z0);
    t.v1 = geometry::Vec3f(x1, y1, z1);
    t.v2 = geometry::Vec3f(x2, y2, z2);
    return t;
}

bool samePoint(const geometry::Point2D &a, const geometry::Point2D &b) {
    return nearD(a.x, b.x) && nearD(a.y, b.y);
}

bool containsPoint(const std::vector<geometry::Point2D> &pts,
                   const geometry::Point2D &p) {
    for (const auto &q : pts) {
        if (samePoint(p, q)) {
            return true;
        }
    }
    return false;
}

bool samePointSet(const std::vector<geometry::Point2D> &a,
                  const std::vector<geometry::Point2D> &b) {
    if (a.size() != b.size()) {
        return false;
    }
    for (const auto &p : a) {
        if (!containsPoint(b, p)) {
            return false;
        }
    }
    return true;
}

// cube：中心在原点，half 为半边长。
geometry::Mesh makeCube(float half) {
    geometry::Mesh m;
    const float h = half;
    const geometry::Vec3f v[8] = {
        {-h, -h, -h}, {h, -h, -h}, {h, h, -h}, {-h, h, -h},
        {-h, -h, h},  {h, -h, h},  {h, h, h},  {-h, h, h}};
    auto add = [&](int a, int b, int c) {
        m.triangles.push_back(
            tri(v[a].x(), v[a].y(), v[a].z(),
                v[b].x(), v[b].y(), v[b].z(),
                v[c].x(), v[c].y(), v[c].z()));
    };
    add(0, 1, 2); add(0, 2, 3);  // 底面 z=-h
    add(4, 5, 6); add(4, 6, 7);  // 顶面 z=+h
    add(0, 4, 7); add(0, 7, 3);  // 左面 x=-h
    add(1, 2, 6); add(1, 6, 5);  // 右面 x=+h
    add(0, 5, 4); add(0, 1, 5);  // 前面 y=-h
    add(2, 7, 6); add(2, 3, 7);  // 后面 y=+h
    m.computeBounds();
    return m;
}

// 三棱柱：底 (0,0),(10,0),(0,10)，沿 z 从 -h 到 +h。
geometry::Mesh makePrism(float h) {
    geometry::Mesh m;
    const geometry::Vec3f A(0, 0, -h), B(10, 0, -h), C(0, 10, -h);
    const geometry::Vec3f Ap(0, 0, h), Bp(10, 0, h), Cp(0, 10, h);
    auto add = [&](const geometry::Vec3f &a, const geometry::Vec3f &b,
                   const geometry::Vec3f &c) {
        m.triangles.push_back(
            tri(a.x(), a.y(), a.z(), b.x(), b.y(), b.z(), c.x(), c.y(), c.z()));
    };
    add(A, B, Bp); add(A, Bp, Ap);  // 侧面 AB
    add(B, C, Cp); add(B, Cp, Bp);  // 侧面 BC
    add(C, A, Ap); add(C, Ap, Cp);  // 侧面 CA
    add(A, C, B);                   // 底面
    add(Ap, Bp, Cp);                // 顶面
    m.computeBounds();
    return m;
}

} // namespace

int main() {
    slicing::Slicer slicer;
    const double tol = 1e-6;

    // 1. cube 中间切片 → 1 个闭合正方形（4 顶点）。
    {
        auto mesh = makeCube(10.0f);
        auto polys = slicer.slice(mesh, 0.0, tol);
        check(polys.size() == 1 && polys[0].closed, "cube 中间切片得到 1 个闭合轮廓");
        if (polys.size() == 1) {
            const std::vector<geometry::Point2D> expect = {
                {-10, -10}, {10, -10}, {10, 10}, {-10, 10}};
            check(polys[0].points.size() == 4 &&
                      samePointSet(polys[0].points, expect),
                  "cube 截面为正方形（4 顶点）");
        }
    }

    // 2. cube 顶部以上切片 → 空。
    {
        auto mesh = makeCube(10.0f);
        check(slicer.slice(mesh, 100.0, tol).empty(), "顶部以上切片返回空");
    }

    // 3. cube 底部以下切片 → 空。
    {
        auto mesh = makeCube(10.0f);
        check(slicer.slice(mesh, -100.0, tol).empty(), "底部以下切片返回空");
    }

    // 4. cube sliceAll 层数正确（高 20，layerHeight=5 → 4 层）。
    {
        auto mesh = makeCube(10.0f);
        auto layers = slicer.sliceAll(mesh, 5.0, tol);
        check(layers.size() == 4, "cube sliceAll 生成 4 层");
    }

    // 5. sliceAll 每层闭合（cube 任意水平截面都是正方形）。
    {
        auto mesh = makeCube(10.0f);
        auto layers = slicer.sliceAll(mesh, 5.0, tol);
        bool allClosed = !layers.empty();
        for (const auto &layer : layers) {
            allClosed = allClosed && layer.contours.size() == 1 &&
                        layer.contours[0].closed;
        }
        check(allClosed, "sliceAll 每层均为 1 个闭合轮廓");
    }

    // 6. sliceAll 层高值正确（zMin+layerHeight/2 起，递增 layerHeight）。
    {
        auto mesh = makeCube(10.0f);
        auto layers = slicer.sliceAll(mesh, 5.0, tol);
        bool zOk = layers.size() == 4 &&
                   nearD(layers[0].z, -7.5) && nearD(layers[1].z, -2.5) &&
                   nearD(layers[2].z, 2.5) && nearD(layers[3].z, 7.5);
        check(zOk, "sliceAll 层高值正确（-7.5/-2.5/2.5/7.5）");
    }

    // 7. 空 mesh sliceAll → 空。
    {
        geometry::Mesh empty;
        check(slicer.sliceAll(empty, 5.0, tol).empty(), "空 mesh 返回空");
    }

    // 8. 单三角形切片 → 1 条开放线段（不能闭合）。
    {
        geometry::Mesh m;
        m.triangles.push_back(tri(0, 0, 0, 10, 0, 0, 0, 10, 10));
        m.computeBounds();
        auto polys = slicer.slice(m, 5.0, tol);
        check(polys.size() == 1 && !polys[0].closed &&
                  polys[0].points.size() == 2,
              "单三角形切片返回 1 条开放线段");
    }

    // 9. 三棱柱中间切片 → 1 个闭合三角形（3 顶点）。
    {
        auto mesh = makePrism(10.0f);
        auto polys = slicer.slice(mesh, 0.0, tol);
        check(polys.size() == 1 && polys[0].closed &&
                  polys[0].points.size() == 3,
              "三棱柱中间切片得到闭合三角形（3 顶点）");
    }

    // 10. sliceLayer：分类结果填入 Layer（cube → 1 多边形 0 孔）。
    {
        auto mesh = makeCube(10.0f);
        auto layer = slicer.sliceLayer(mesh, 0.0, tol);
        check(layer.classified.polygons.size() == 1 &&
                  layer.classified.polygons[0].holes.empty(),
              "sliceLayer 分类：cube → 1 多边形 0 孔");
    }

    // 11. 可变层厚：显式 z 表 {-5, 0, 5} → 3 层且 z 值正确；区间外被跳过。
    {
        auto mesh = makeCube(10.0f);
        auto layers = slicer.sliceAll(mesh, std::vector<double>{-5.0, 0.0, 5.0, 100.0}, tol);
        bool zOk = layers.size() == 3 && nearD(layers[0].z, -5.0) &&
                   nearD(layers[1].z, 0.0) && nearD(layers[2].z, 5.0);
        check(zOk, "可变层厚 z 表正确（区间外 z 被跳过）");
        bool allClosed = layers.size() == 3;
        for (const auto &layer : layers) {
            allClosed = allClosed && layer.classified.polygons.size() == 1;
        }
        check(allClosed, "可变层厚每层分类出 1 个多边形");
    }

    // 12. 空 z 表 → 空。
    {
        auto mesh = makeCube(10.0f);
        check(slicer.sliceAll(mesh, std::vector<double>{}, tol).empty(),
              "空 z 表返回空");
    }

    // 13. 乱序 z 表 → 输出按 z 升序（扫描线算法约定）。
    {
        auto mesh = makeCube(10.0f);
        auto layers = slicer.sliceAll(mesh, std::vector<double>{5.0, -5.0, 0.0, 100.0}, tol);
        bool zOk = layers.size() == 3 && nearD(layers[0].z, -5.0) &&
                   nearD(layers[1].z, 0.0) && nearD(layers[2].z, 5.0);
        check(zOk, "乱序 z 表输出升序层");
    }

    // 14. 扫描线 sliceAll 与逐层 slice（全量遍历）结果一致（加速正确性交叉验证）。
    {
        auto mesh = makePrism(10.0f);
        auto layers = slicer.sliceAll(mesh, 2.5, tol);
        bool consistent = layers.size() == 8;
        for (const auto &layer : layers) {
            auto ref = slicer.slice(mesh, layer.z, tol);
            consistent = consistent && ref.size() == 1 && layer.contours.size() == 1 &&
                         layer.contours[0].closed &&
                         samePointSet(ref[0].points, layer.contours[0].points);
        }
        check(consistent, "扫描线 sliceAll 与逐层 slice 结果一致（三棱柱 8 层）");
    }

    // 15. 共面检测：切在 cube 顶面（z=+10）→ 顶面 2 个三角形共面，告警存在。
    {
        auto mesh = makeCube(10.0f);
        auto layer = slicer.sliceLayer(mesh, 10.0, tol);
        check(layer.coplanarTriangleCount == 2, "顶面 2 个共面三角形被计数");
        check(!layer.classified.warnings.empty(), "共面层产生显式告警");
        // 非共面层（z=0）无共面计数。
        auto mid = slicer.sliceLayer(mesh, 0.0, tol);
        check(mid.coplanarTriangleCount == 0, "中间层无共面三角形");
    }

    // 16. 并行分层与串行逐点一致（cube 40 层，threads=4 vs 1）。
    //     并行分块不改变任何一层的计算输入与顺序，输出应位级一致。
    {
        auto mesh = makeCube(10.0f);
        auto serial = slicer.sliceAll(mesh, 0.5, tol, 1);
        auto parallel = slicer.sliceAll(mesh, 0.5, tol, 4);
        bool same = serial.size() == 40 && parallel.size() == 40;
        for (size_t i = 0; same && i < serial.size(); ++i) {
            const auto &a = serial[i];
            const auto &b = parallel[i];
            same = nearD(a.z, b.z) &&
                   a.contours.size() == b.contours.size() &&
                   a.classified.polygons.size() == b.classified.polygons.size() &&
                   a.classified.openChains.size() == b.classified.openChains.size() &&
                   a.coplanarTriangleCount == b.coplanarTriangleCount &&
                   a.classified.warnings.size() == b.classified.warnings.size();
            for (size_t k = 0; same && k < a.contours.size(); ++k) {
                same = a.contours[k].closed == b.contours[k].closed &&
                       a.contours[k].points.size() == b.contours[k].points.size();
                for (size_t p = 0; same && p < a.contours[k].points.size(); ++p) {
                    same = samePoint(a.contours[k].points[p], b.contours[k].points[p]);
                }
            }
            for (size_t k = 0; same && k < a.classified.polygons.size(); ++k) {
                same = a.classified.polygons[k].holes.size() ==
                       b.classified.polygons[k].holes.size();
            }
        }
        check(same, "并行分层与串行逐点一致（40 层 × 4 线程）");

        // z 表版同样一致（乱序输入，验证分块与排序组合的正确性）。
        const std::vector<double> zs{5.0, -5.0, 0.0, 2.5, -2.5,
                                     7.5, -7.5, 1.0, -1.0, 3.0};
        auto s2 = slicer.sliceAll(mesh, zs, tol, 1);
        auto p2 = slicer.sliceAll(mesh, zs, tol, 3);
        bool same2 = s2.size() == 10 && p2.size() == 10;
        for (size_t i = 0; same2 && i < s2.size(); ++i) {
            same2 = nearD(s2[i].z, p2[i].z) &&
                    s2[i].contours.size() == p2[i].contours.size() &&
                    s2[i].classified.polygons.size() == p2[i].classified.polygons.size();
        }
        check(same2, "并行 z 表分层与串行一致（乱序输入 × 3 线程）");
    }

    std::cout << "\n" << (g_failures == 0 ? "ALL PASS" : "HAS FAILURES")
              << " (" << g_failures << " failures)\n";
    return g_failures == 0 ? 0 : 1;
}
