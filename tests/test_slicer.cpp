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

    std::cout << "\n" << (g_failures == 0 ? "ALL PASS" : "HAS FAILURES")
              << " (" << g_failures << " failures)\n";
    return g_failures == 0 ? 0 : 1;
}
