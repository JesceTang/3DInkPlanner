// SegmentConnector 单元测试（无第三方框架，失败返回非零）。
#include "slicing/SegmentConnector.h"

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

geometry::Segment2D seg(double x0, double y0, double x1, double y1) {
    return geometry::Segment2D{{x0, y0}, {x1, y1}};
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

// 无序比较两个点集是否相等。
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

int countClosed(const std::vector<geometry::Polyline> &polys) {
    int c = 0;
    for (const auto &p : polys) {
        if (p.closed) {
            ++c;
        }
    }
    return c;
}

} // namespace

int main() {
    slicing::SegmentConnector connector;
    const double tol = 1e-6;

    // 1. 单个正方形（打乱顺序）→ 1 个闭合轮廓，4 个顶点。
    {
        std::vector<geometry::Segment2D> segs = {
            seg(1, 1, 0, 1), seg(0, 0, 1, 0), seg(0, 1, 0, 0), seg(1, 0, 1, 1)};
        auto polys = connector.connect(segs, tol);
        check(polys.size() == 1 && polys[0].closed, "正方形拼接为 1 个闭合轮廓");
        if (polys.size() == 1) {
            const std::vector<geometry::Point2D> expect = {
                {0, 0}, {1, 0}, {1, 1}, {0, 1}};
            check(polys[0].points.size() == 4 &&
                      samePointSet(polys[0].points, expect),
                  "正方形顶点集合正确（4 个唯一顶点）");
        }
    }

    // 2. 单个三角形 → 1 个闭合轮廓，3 个顶点。
    {
        std::vector<geometry::Segment2D> segs = {
            seg(0, 0, 10, 0), seg(10, 0, 0, 10), seg(0, 10, 0, 0)};
        auto polys = connector.connect(segs, tol);
        check(polys.size() == 1 && polys[0].closed &&
                  polys[0].points.size() == 3,
              "三角形拼接为 1 个闭合轮廓（3 顶点）");
    }

    // 3. 两个独立正方形 → 2 个闭合轮廓。
    {
        std::vector<geometry::Segment2D> segs = {
            seg(0, 0, 1, 0), seg(1, 0, 1, 1), seg(1, 1, 0, 1), seg(0, 1, 0, 0),
            seg(10, 10, 11, 10), seg(11, 10, 11, 11), seg(11, 11, 10, 11), seg(10, 11, 10, 10)};
        auto polys = connector.connect(segs, tol);
        check(polys.size() == 2 && countClosed(polys) == 2,
              "两个独立正方形拼接为 2 个闭合轮廓");
    }

    // 4. 开放折线（3 条共线线段）→ 1 个开放轮廓。
    {
        std::vector<geometry::Segment2D> segs = {
            seg(0, 0, 1, 0), seg(1, 0, 2, 0), seg(2, 0, 3, 0)};
        auto polys = connector.connect(segs, tol);
        check(polys.size() == 1 && !polys[0].closed &&
                  polys[0].points.size() == 4,
              "开放折线拼接为 1 个开放轮廓（4 顶点）");
    }

    // 5. 空输入 → 空结果。
    {
        auto polys = connector.connect({}, tol);
        check(polys.empty(), "空输入返回空结果");
    }

    // 6. 单条线段 → 1 个开放轮廓，2 顶点。
    {
        auto polys = connector.connect({seg(0, 0, 5, 5)}, tol);
        check(polys.size() == 1 && !polys[0].closed &&
                  polys[0].points.size() == 2,
              "单条线段返回 1 个开放轮廓（2 顶点）");
    }

    // 7. 容差内：端点间隙 5e-7 < tol → 闭合。
    {
        std::vector<geometry::Segment2D> segs = {
            seg(0, 0, 1, 0), seg(1.0000005, 0, 1, 1),
            seg(1, 1, 0, 1), seg(0, 1, 0, 0)};
        auto polys = connector.connect(segs, tol);
        check(polys.size() == 1 && polys[0].closed,
              "端点间隙 5e-7（< tol）仍闭合");
    }

    // 8. 容差外：端点间隙 0.01 > tol → 不闭合。
    {
        std::vector<geometry::Segment2D> segs = {
            seg(0, 0, 1, 0), seg(1.01, 0, 1, 1),
            seg(1, 1, 0, 1), seg(0, 1, 0, 0)};
        auto polys = connector.connect(segs, tol);
        check(polys.size() == 1 && !polys[0].closed,
              "端点间隙 0.01（> tol）不闭合");
    }

    // 9. 端点顺序相反（所有线段反向）→ 仍闭合。
    {
        std::vector<geometry::Segment2D> segs = {
            seg(1, 0, 0, 0), seg(1, 1, 1, 0), seg(0, 1, 1, 1), seg(0, 0, 0, 1)};
        auto polys = connector.connect(segs, tol);
        check(polys.size() == 1 && polys[0].closed &&
                  polys[0].points.size() == 4,
              "端点顺序相反仍正确闭合");
    }

    // 10. 两个断裂开放段 → 2 个开放轮廓。
    {
        std::vector<geometry::Segment2D> segs = {
            seg(0, 0, 1, 0), seg(1, 0, 2, 0),
            seg(5, 5, 6, 5), seg(6, 5, 7, 5)};
        auto polys = connector.connect(segs, tol);
        check(polys.size() == 2 && countClosed(polys) == 0,
              "两个断裂开放段返回 2 个开放轮廓");
    }

    std::cout << "\n" << (g_failures == 0 ? "ALL PASS" : "HAS FAILURES")
              << " (" << g_failures << " failures)\n";
    return g_failures == 0 ? 0 : 1;
}
