// PathOptimizer 单元测试（无第三方框架，失败返回非零）。
#include "path/PathOptimizer.h"

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

bool samePt(const geometry::Point2D &a, const geometry::Point2D &b,
            double eps = 1e-9) {
    return nearD(a.x, b.x, eps) && nearD(a.y, b.y, eps);
}

geometry::Polygon square(double x0, double y0, double x1, double y1) {
    geometry::Polygon p;
    p.vertices = {{x0, y0}, {x1, y0}, {x1, y1}, {x0, y1}};
    return p;
}

// 序列结构校验：首段必为 Print；Travel 插入后全序列完全连续（prev.end == cur.start）；
// 不存在相邻 Travel-Travel。
bool structureOk(const std::vector<path::PathSegment> &segs) {
    if (segs.empty() || segs.front().type != path::PathType::Print) {
        return false;
    }
    for (size_t i = 1; i < segs.size(); ++i) {
        const auto &prev = segs[i - 1];
        const auto &cur = segs[i];
        if (!samePt(prev.end, cur.start)) {
            return false;  // 序列不连续（断点未插 Travel）
        }
        if (prev.type == path::PathType::Travel &&
            cur.type == path::PathType::Travel) {
            return false;  // 连续 Travel（多余或断链）
        }
    }
    return true;
}

} // namespace

int main() {
    const path::PathOptimizer optimizer;

    // 1. 单正方形 spacing=2.5 → 4 Print + 3 换行 Travel 交替。
    {
        auto r = optimizer.optimize({square(0, 0, 10, 10)}, 2.5);
        check(r.segments.size() == 7, "单正方形：4 Print + 3 Travel 共 7 段");
        check(structureOk(r.segments), "序列结构合法（Print/Travel 交替）");
        // Print 长度 4×10=40；Travel 每段纵向 2.5，共 7.5。
        check(nearD(r.printLength, 40.0) && nearD(r.travelLength, 7.5),
              "长度统计正确（print=40, travel=7.5）");
    }

    // 2. 两岛最近邻排序：起点 (0,0) → 先打左岛 A。
    {
        auto r = optimizer.optimize({square(20, 0, 24, 4), square(0, 0, 4, 4)},
                                    2.0, {0.0, 0.0});
        check(structureOk(r.segments), "两岛序列结构合法");
        // A 的首段在 y=1，x∈[0,4]；验证第一段属于左岛。
        check(!r.segments.empty() && r.segments.front().start.x < 10.0,
              "最近邻排序：起点 (0,0) 先打左岛");
        // 存在一段跨岛 Travel（x 从 <10 跳到 >10）。
        bool crossIsland = false;
        for (const auto &s : r.segments) {
            if (s.type == path::PathType::Travel && s.start.x < 10.0 &&
                s.end.x > 10.0) {
                crossIsland = true;
            }
        }
        check(crossIsland, "两岛之间存在跨岛 Travel 段");
    }

    // 3. 起点 (30,30) → 先打右岛 B（排序对起点敏感）。
    {
        auto r = optimizer.optimize({square(0, 0, 4, 4), square(20, 0, 24, 4)},
                                    2.0, {30.0, 30.0});
        check(!r.segments.empty() && r.segments.front().start.x > 10.0,
              "最近邻排序：起点 (30,30) 先打右岛");
    }

    // 4. 空输入 → 空结果。
    {
        auto r = optimizer.optimize({}, 2.0);
        check(r.segments.empty() && nearD(r.printLength, 0.0) &&
                  nearD(r.travelLength, 0.0),
              "空输入返回空结果");
    }

    // 5. 退化 polygon（无填充段）被跳过，不影响其余岛。
    {
        geometry::Polygon degenerate;
        degenerate.vertices = {{0, 0}, {1, 1}};  // <3 顶点
        auto r = optimizer.optimize({degenerate, square(0, 0, 10, 10)}, 2.5);
        check(r.segments.size() == 7, "退化 polygon 被跳过，其余岛正常");
    }

    // 6. 带孔正方形：Print 段数与 RasterFill 直接生成一致（14 段），
    //    每个断点前都有 Travel。
    {
        auto p = square(-10, -10, 10, 10);
        p.holes.push_back({{-4, -4}, {4, -4}, {4, 4}, {-4, 4}});
        auto r = optimizer.optimize({p}, 2.0);
        size_t prints = 0;
        for (const auto &s : r.segments) {
            if (s.type == path::PathType::Print) {
                ++prints;
            }
        }
        check(prints == 14, "带孔正方形 Print 段数与 RasterFill 一致（14）");
        check(structureOk(r.segments), "带孔正方形序列结构合法");
    }

    std::cout << "\n" << (g_failures == 0 ? "ALL PASS" : "HAS FAILURES")
              << " (" << g_failures << " failures)\n";
    return g_failures == 0 ? 0 : 1;
}
