// ContourClassifier 单元测试（无第三方框架，失败返回非零）。
#include "slicing/ContourClassifier.h"
#include "io/STLReader.h"
#include "slicing/Slicer.h"

#include <algorithm>
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

// 构造中心 (cx,cy)、半边长 h 的方形闭合环；ccw=true 逆时针。
geometry::Polyline squareRing(double cx, double cy, double h, bool ccw) {
    geometry::Polyline pl;
    pl.closed = true;
    std::vector<geometry::Point2D> pts = {
        {cx - h, cy - h}, {cx + h, cy - h}, {cx + h, cy + h}, {cx - h, cy + h}};
    if (!ccw) {
        std::reverse(pts.begin(), pts.end());
    }
    pl.points = std::move(pts);
    return pl;
}

double areaOf(const std::vector<geometry::Point2D> &pts) {
    double a = 0.0;
    const std::size_t n = pts.size();
    for (std::size_t i = 0; i < n; ++i) {
        const auto &p = pts[i];
        const auto &q = pts[(i + 1) % n];
        a += p.x * q.y - q.x * p.y;
    }
    return a / 2.0;
}

} // namespace

int main() {
    const slicing::ContourClassifier classifier;
    constexpr double kTol = 1e-6;

    // 1. 单方环 → 1 polygon、0 孔、外环 CCW。
    {
        auto res = classifier.classify({squareRing(0, 0, 10, true)}, kTol);
        check(res.polygons.size() == 1 && res.polygons[0].holes.empty(),
              "单方环 → 1 个多边形无孔");
        check(areaOf(res.polygons[0].vertices) > 0, "外环统一为 CCW");
    }

    // 2. 方环套方孔（输入方向打乱：外 CW、内 CCW）→ 方向自动纠正。
    {
        auto res = classifier.classify(
            {squareRing(0, 0, 10, false), squareRing(0, 0, 5, true)}, kTol);
        check(res.polygons.size() == 1 && res.polygons[0].holes.size() == 1,
              "环套孔 → 1 多边形 1 孔");
        if (res.polygons.size() == 1) {
            check(areaOf(res.polygons[0].vertices) > 0, "外环纠正为 CCW");
            check(areaOf(res.polygons[0].holes[0]) < 0, "内环纠正为 CW");
        }
    }

    // 3. 三孔板 → 1 polygon 3 holes。
    {
        auto res = classifier.classify(
            {squareRing(0, 0, 30, true), squareRing(-20, 0, 5, true),
             squareRing(0, 0, 5, true), squareRing(20, 0, 5, true)},
            kTol);
        check(res.polygons.size() == 1 && res.polygons[0].holes.size() == 3,
              "三孔板 → 1 多边形 3 孔");
    }

    // 4. 两个独立方环 → 2 polygons 均无孔。
    {
        auto res = classifier.classify(
            {squareRing(0, 0, 10, true), squareRing(100, 0, 10, true)}, kTol);
        check(res.polygons.size() == 2 && res.polygons[0].holes.empty() &&
                  res.polygons[1].holes.empty(),
              "两独立环 → 2 个独立多边形");
    }

    // 5. 三层嵌套（A ⊃ B ⊃ C）→ A 带孔 B，C 为独立多边形（even-odd 语义）。
    {
        auto res = classifier.classify(
            {squareRing(0, 0, 30, true), squareRing(0, 0, 20, true),
             squareRing(0, 0, 10, true)},
            kTol);
        check(res.polygons.size() == 2, "三层嵌套 → 2 个多边形");
        int withHole = 0;
        int withoutHole = 0;
        for (const auto &p : res.polygons) {
            if (p.holes.size() == 1) {
                ++withHole;
            } else if (p.holes.empty()) {
                ++withoutHole;
            }
        }
        check(withHole == 1 && withoutHole == 1, "A 含孔 B，岛环 C 独立成多边形");
    }

    // 6. 开放链 → openChains + 告警，不参与分类。
    {
        geometry::Polyline open;
        open.closed = false;
        open.points = {{0, 0}, {1, 0}, {2, 0}};
        auto res = classifier.classify({open, squareRing(0, 0, 10, true)}, kTol);
        check(res.polygons.size() == 1 && res.openChains.size() == 1 &&
                  !res.warnings.empty(),
              "开放链进 openChains 并告警");
    }

    // 7. 空输入 → 空结果。
    {
        auto res = classifier.classify({}, kTol);
        check(res.polygons.empty() && res.openChains.empty() && res.warnings.empty(),
              "空输入返回空结果");
    }

    // 8. 退化环（容差尺度的微环，面积 1e-16 < tol²）→ 丢弃 + 告警。
    {
        geometry::Polyline tiny;
        tiny.closed = true;
        tiny.points = {{0, 0}, {1e-8, 0}, {1e-8, 1e-8}, {0, 1e-8}};
        auto res = classifier.classify({tiny}, kTol);
        check(res.polygons.empty() && !res.warnings.empty(), "退化环被丢弃并告警");
    }

    // 9. 集成：空心圆柱切片 → 每层 1 多边形 1 孔；三孔板 → 1 多边形 3 孔。
    {
        slicing::Slicer slicer;
        const std::string dir = std::string(SOURCE_DIR) + "/assets/models/";

        const auto cyl = io::readBinaryStl(dir + "hollow_cylinder.stl");
        check(cyl.ok, "读取 hollow_cylinder.stl");
        if (cyl.ok) {
            const auto layers = slicer.sliceAll(cyl.mesh, 0.5, kTol);
            bool allOk = !layers.empty();
            for (const auto &layer : layers) {
                auto res = classifier.classify(layer.contours, kTol);
                if (res.polygons.size() != 1 || res.polygons[0].holes.size() != 1) {
                    allOk = false;
                }
            }
            check(allOk, "空心圆柱每层 1 外环 + 1 内环（全部 " +
                             std::to_string(layers.size()) + " 层）");
        }

        const auto plate = io::readBinaryStl(dir + "plate_3holes.stl");
        check(plate.ok, "读取 plate_3holes.stl");
        if (plate.ok) {
            const auto layers = slicer.sliceAll(plate.mesh, 0.5, kTol);
            bool allOk = !layers.empty();
            for (const auto &layer : layers) {
                auto res = classifier.classify(layer.contours, kTol);
                if (res.polygons.size() != 1 || res.polygons[0].holes.size() != 3) {
                    allOk = false;
                }
            }
            check(allOk, "三孔板每层 1 外环 + 3 内环（全部 " +
                             std::to_string(layers.size()) + " 层）");
        }
    }

    std::cout << "\n" << (g_failures == 0 ? "ALL PASS" : "HAS FAILURES") << " ("
              << g_failures << " failures)\n";
    return g_failures == 0 ? 0 : 1;
}
