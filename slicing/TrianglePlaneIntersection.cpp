#include "slicing/TrianglePlaneIntersection.h"

#include <cmath>
#include <vector>

namespace slicing {

namespace {

enum class Side : int {
    Below = -1,  // z < h - eps
    On = 0,      // |z - h| <= eps
    Above = 1,   // z > h + eps
};

Side classify(double z, double h, double eps) {
    if (z > h + eps) {
        return Side::Above;
    }
    if (z < h - eps) {
        return Side::Below;
    }
    return Side::On;
}

} // namespace

std::optional<geometry::Segment2D> intersectTrianglePlane(
    const geometry::Triangle &t, double h, double eps) {
    const double z[3] = {t.v0.z(), t.v1.z(), t.v2.z()};
    const double x[3] = {t.v0.x(), t.v1.x(), t.v2.x()};
    const double y[3] = {t.v0.y(), t.v1.y(), t.v2.y()};
    const Side side[3] = {classify(z[0], h, eps), classify(z[1], h, eps),
                          classify(z[2], h, eps)};

    // 三角形共面（3 顶点全在平面上）：忽略，由上层计数告警。
    if (side[0] == Side::On && side[1] == Side::On && side[2] == Side::On) {
        return std::nullopt;
    }

    // 收集交点：跨越平面的边线性插值；恰在平面上的顶点直接作为交点。
    std::vector<geometry::Point2D> pts;
    for (int i = 0; i < 3; ++i) {
        const int j = (i + 1) % 3;
        const int prod = static_cast<int>(side[i]) * static_cast<int>(side[j]);
        if (prod == -1) {
            // 一端严格在上、一端严格在下：线性插值求交点（此时分母必不为 0）。
            const double t = (h - z[i]) / (z[j] - z[i]);
            pts.push_back({x[i] + t * (x[j] - x[i]), y[i] + t * (y[j] - y[i])});
        } else if (side[i] == Side::On && side[j] != Side::On) {
            pts.push_back({x[i], y[i]});
        } else if (side[j] == Side::On && side[i] != Side::On) {
            pts.push_back({x[j], y[j]});
        }
    }

    // 去重：同一交点可能被相邻两条边重复报告，用 eps 判定两点重合。
    std::vector<geometry::Point2D> unique;
    for (const auto &p : pts) {
        bool dup = false;
        for (const auto &q : unique) {
            const double dx = p.x - q.x;
            const double dy = p.y - q.y;
            if (std::hypot(dx, dy) <= eps) {
                dup = true;
                break;
            }
        }
        if (!dup) {
            unique.push_back(p);
        }
    }

    // 恰好两个不同交点才构成有效线段；0/1 个为退化，>2 个理论不出现。
    if (unique.size() == 2) {
        return geometry::Segment2D{unique[0], unique[1]};
    }
    return std::nullopt;
}

} // namespace slicing
