// Transform 单元测试（无第三方框架，失败返回非零）。
#include "geometry/Transform.h"

#include <cmath>
#include <iostream>
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

geometry::Vec3d V(double x, double y, double z) {
    return geometry::Vec3d(x, y, z);
}

bool nearV(const geometry::Vec3d &a, const geometry::Vec3d &b,
           double eps = 1e-9) {
    return nearD(a.x(), b.x(), eps) && nearD(a.y(), b.y(), eps) &&
           nearD(a.z(), b.z(), eps);
}

} // namespace

int main() {
    const double SQ2 = std::sqrt(2.0) / 2.0;

    // 1. 单位变换：点不变。
    {
        geometry::Transform t;
        check(nearV(t.apply(V(1, 2, 3)), V(1, 2, 3)), "单位变换点不变");
    }

    // 2. 平移。
    {
        auto t = geometry::Transform::translation(V(10, 20, 30));
        check(nearV(t.apply(V(1, 2, 3)), V(11, 22, 33)), "平移 offset (10,20,30)");
    }

    // 3. 绕 Z 旋转 90°。
    {
        auto t = geometry::Transform::fromOffsetRotationScale(V(0, 0, 0), 90.0, 1.0);
        check(nearV(t.apply(V(1, 0, 0)), V(0, 1, 0)), "绕 Z 旋转 90°: (1,0,0)→(0,1,0)");
    }

    // 4. 绕 Z 旋转 90°（多点验证）。
    {
        auto t = geometry::Transform::fromOffsetRotationScale(V(0, 0, 0), 90.0, 1.0);
        check(nearV(t.apply(V(1, 1, 0)), V(-1, 1, 0)), "绕 Z 旋转 90°: (1,1,0)→(-1,1,0)");
    }

    // 5. 均匀缩放 2。
    {
        auto t = geometry::Transform::fromOffsetRotationScale(V(0, 0, 0), 0.0, 2.0);
        check(nearV(t.apply(V(1, 2, 3)), V(2, 4, 6)), "均匀缩放 2");
    }

    // 6. 综合：缩放 2 + 旋转 90° + 平移 (10,0,0)。
    {
        auto t = geometry::Transform::fromOffsetRotationScale(V(10, 0, 0), 90.0, 2.0);
        // (1,0,0) → scale(2,0,0) → rotate90(0,2,0) → translate(10,2,0)
        check(nearV(t.apply(V(1, 0, 0)), V(10, 2, 0)),
              "综合变换 scale+rotate+translate");
    }

    // 7. compose 顺序：先 other 后 this。
    {
        auto t1 = geometry::Transform::translation(V(10, 0, 0));
        auto t2 = geometry::Transform::translation(V(0, 5, 0));
        auto t = t1.compose(t2);
        check(nearV(t.apply(V(0, 0, 0)), V(10, 5, 0)), "compose 先 other 后 this");
    }

    // 8. 缩放先于平移。
    {
        auto t = geometry::Transform::fromOffsetRotationScale(V(10, 0, 0), 0.0, 2.0);
        check(nearV(t.apply(V(1, 0, 0)), V(12, 0, 0)), "缩放先于平移: (1,0,0)→(12,0,0)");
    }

    // 9. 绕 Z 旋转 45°。
    {
        auto t = geometry::Transform::fromOffsetRotationScale(V(0, 0, 0), 45.0, 1.0);
        check(nearV(t.apply(V(1, 0, 0)), V(SQ2, SQ2, 0)), "绕 Z 旋转 45°");
    }

    // 10. 自适应容差（geometry 层补充）：小模型用下限，大模型按尺寸放宽。
    {
        geometry::Mesh small;  // 10mm 立方体 → maxDim=10，scaled=1e-8 < 下限
        small.triangles.push_back({geometry::Vec3f(0, 0, 0),
                                   geometry::Vec3f(10, 0, 0),
                                   geometry::Vec3f(0, 10, 10),
                                   geometry::Vec3f::Zero()});
        small.computeBounds();
        check(nearD(small.adaptiveTolerance(), geometry::kGeometryEpsilon),
              "小模型自适应容差取下限 1e-6");

        geometry::Mesh big;  // 10000mm → scaled=1e-5 > 下限
        big.triangles.push_back({geometry::Vec3f(0, 0, 0),
                                 geometry::Vec3f(10000, 0, 0),
                                 geometry::Vec3f(0, 10000, 10000),
                                 geometry::Vec3f::Zero()});
        big.computeBounds();
        check(nearD(big.adaptiveTolerance(), 1e-5),
              "大模型自适应容差按尺寸放宽（10000mm → 1e-5）");
    }

    std::cout << "\n" << (g_failures == 0 ? "ALL PASS" : "HAS FAILURES")
              << " (" << g_failures << " failures)\n";
    return g_failures == 0 ? 0 : 1;
}
