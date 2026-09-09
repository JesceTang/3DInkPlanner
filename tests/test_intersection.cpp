// Triangle-Plane Intersection 单元测试（无第三方框架，失败返回非零）。
#include "slicing/TrianglePlaneIntersection.h"

#include <cmath>
#include <iostream>
#include <optional>
#include <string>

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

// 构造三角形（float 顶点）。
geometry::Triangle tri3f(float x0, float y0, float z0,
                         float x1, float y1, float z1,
                         float x2, float y2, float z2) {
    geometry::Triangle t;
    t.v0 = geometry::Vec3f(x0, y0, z0);
    t.v1 = geometry::Vec3f(x1, y1, z1);
    t.v2 = geometry::Vec3f(x2, y2, z2);
    return t;
}

// 检查返回的 segment 两个端点（无序）等于期望值。
void checkSegment(const std::optional<geometry::Segment2D> &got,
                  double ax, double ay, double bx, double by,
                  const std::string &msg) {
    if (!got) {
        check(false, msg + "（未返回 segment）");
        return;
    }
    const double gx0 = got->p0.x, gy0 = got->p0.y;
    const double gx1 = got->p1.x, gy1 = got->p1.y;
    const bool match =
        (nearD(gx0, ax) && nearD(gy0, ay) && nearD(gx1, bx) && nearD(gy1, by)) ||
        (nearD(gx0, bx) && nearD(gy0, by) && nearD(gx1, ax) && nearD(gy1, ay));
    check(match, msg);
}

} // namespace

int main() {
    constexpr double eps = 1e-6;

    // 1. 普通穿过：一顶点在上、两顶点在下，交线坐标精确。
    {
        auto seg = slicing::intersectTrianglePlane(
            tri3f(0, 0, 0, 10, 0, 0, 0, 10, 10), 5.0, eps);
        // 边(0,2) 与 边(1,2) 均在 z=5 处相交：t=0.5 → (0,5) 与 (5,5)。
        checkSegment(seg, 0.0, 5.0, 5.0, 5.0, "普通穿过得到正确 segment");
    }

    // 2. 完全在平面上方 → 无交点。
    {
        auto seg = slicing::intersectTrianglePlane(
            tri3f(0, 0, 10, 10, 0, 10, 0, 10, 20), 5.0, eps);
        check(!seg, "全在上方返回无交点");
    }

    // 3. 完全在平面下方 → 无交点。
    {
        auto seg = slicing::intersectTrianglePlane(
            tri3f(0, 0, -10, 10, 0, -10, 0, 10, 0), 5.0, eps);
        check(!seg, "全在下方返回无交点");
    }

    // 4. 一顶点在平面、另两在同侧 → 退化为单点，无 segment。
    {
        auto seg = slicing::intersectTrianglePlane(
            tri3f(0, 0, 5, 10, 0, 10, 0, 10, 20), 5.0, eps);
        check(!seg, "一顶点在平面(另两同侧)退化为点，返回无交点");
    }

    // 5. 一顶点在平面、另两在两侧 → on 顶点 + 对面边交点。
    {
        auto seg = slicing::intersectTrianglePlane(
            tri3f(0, 0, 5, 10, 0, 0, 0, 10, 10), 5.0, eps);
        // v0=(0,0,5) 在平面；边(1,2) z=0→10，t=0.5 → (5,5,5)。
        checkSegment(seg, 0.0, 0.0, 5.0, 5.0, "一顶点在平面(另两在两侧)");
    }

    // 6. 两顶点在平面 → 这两个顶点构成 segment。
    {
        auto seg = slicing::intersectTrianglePlane(
            tri3f(0, 0, 5, 10, 0, 5, 5, 10, 0), 5.0, eps);
        // v0、v1 在平面，v2 在下方 → segment 为 v0-v1。
        checkSegment(seg, 0.0, 0.0, 10.0, 0.0, "两顶点在平面得到该边");
    }

    // 7. 三角形共面 → 忽略。
    {
        auto seg = slicing::intersectTrianglePlane(
            tri3f(0, 0, 5, 10, 0, 5, 0, 10, 5), 5.0, eps);
        check(!seg, "共面三角形返回无交点");
    }

    // 8. 退化三角形（三顶点共线）→ 无交点且不崩溃。
    {
        auto seg = slicing::intersectTrianglePlane(
            tri3f(0, 0, 0, 5, 0, 10, 10, 0, 20), 5.0, eps);
        check(!seg, "退化三角形(共线)返回无交点");
    }

    // 9. 浮点容差：顶点距平面 5e-7（< eps）视为在平面（上方贴近）。
    {
        auto seg = slicing::intersectTrianglePlane(
            tri3f(0, 0, 0, 1, 0, 5e-7f, 0, 1, -1), 0.0, eps);
        // v0、v1 均在平面（v1 距平面 5e-7 判为 on），v2 在下方 → segment v0-v1。
        checkSegment(seg, 0.0, 0.0, 1.0, 0.0, "容差内(上方贴近)视为在平面");
    }

    // 10. 浮点容差：顶点距平面 -5e-7（< eps）视为在平面（下方贴近）。
    {
        auto seg = slicing::intersectTrianglePlane(
            tri3f(0, 0, 0, 1, 0, -5e-7f, 0, 1, 1), 0.0, eps);
        checkSegment(seg, 0.0, 0.0, 1.0, 0.0, "容差内(下方贴近)视为在平面");
    }

    std::cout << "\n" << (g_failures == 0 ? "ALL PASS" : "HAS FAILURES")
              << " (" << g_failures << " failures)\n";
    return g_failures == 0 ? 0 : 1;
}
