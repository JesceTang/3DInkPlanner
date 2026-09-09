#include "slicing/ContourClassifier.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <numeric>

namespace slicing {

namespace {

// shoelace 有向面积：CCW 为正。
double signedAreaOf(const std::vector<geometry::Point2D> &pts) {
    double a = 0.0;
    const std::size_t n = pts.size();
    for (std::size_t i = 0; i < n; ++i) {
        const auto &p = pts[i];
        const auto &q = pts[(i + 1) % n];
        a += p.x * q.y - q.x * p.y;
    }
    return a / 2.0;
}

// 射线法（+x 方向）点在环内判定。假设点不在环边上（由代表点选择保证）。
bool pointInRing(const std::vector<geometry::Point2D> &ring,
                 const geometry::Point2D &p) {
    bool inside = false;
    const std::size_t n = ring.size();
    for (std::size_t i = 0, j = n - 1; i < n; j = i++) {
        const auto &a = ring[i];
        const auto &b = ring[j];
        if ((a.y > p.y) != (b.y > p.y)) {
            const double xInt = a.x + (p.y - a.y) * (b.x - a.x) / (b.y - a.y);
            if (p.x < xInt) {
                inside = !inside;
            }
        }
    }
    return inside;
}

// 点到线段的最短距离。
double distToSegment(const geometry::Point2D &p, const geometry::Point2D &a,
                     const geometry::Point2D &b) {
    const double vx = b.x - a.x;
    const double vy = b.y - a.y;
    const double lenSq = vx * vx + vy * vy;
    double t = 0.0;
    if (lenSq > 0.0) {
        t = ((p.x - a.x) * vx + (p.y - a.y) * vy) / lenSq;
        t = std::max(0.0, std::min(1.0, t));
    }
    const double dx = p.x - (a.x + t * vx);
    const double dy = p.y - (a.y + t * vy);
    return std::hypot(dx, dy);
}

// 点到环边界的最小距离。
double distToRing(const std::vector<geometry::Point2D> &ring,
                  const geometry::Point2D &p) {
    double d = std::numeric_limits<double>::max();
    const std::size_t n = ring.size();
    for (std::size_t i = 0, j = n - 1; i < n; j = i++) {
        d = std::min(d, distToSegment(p, ring[i], ring[j]));
    }
    return d;
}

} // namespace

ClassifiedContours ContourClassifier::classify(
    const std::vector<geometry::Polyline> &contours, double tolerance) const {
    ClassifiedContours out;
    if (contours.empty()) {
        return out;
    }

    // ---- 分离闭合环 / 开放链，丢弃退化环 ----
    struct Ring {
        std::vector<geometry::Point2D> pts;
        double area = 0.0;
    };
    std::vector<Ring> rings;
    for (const auto &c : contours) {
        if (!c.closed || c.points.size() < 3) {
            if (!c.points.empty()) {
                out.openChains.push_back(c);
                out.warnings.push_back("断链：开放轮廓 " +
                                       std::to_string(c.points.size()) +
                                       " 顶点（不参与内外环分类）");
            }
            continue;
        }
        const double a = signedAreaOf(c.points);
        if (std::fabs(a) <= tolerance * tolerance) {
            out.warnings.push_back("退化环（面积≈0）已丢弃");
            continue;
        }
        rings.push_back({c.points, a});
    }
    if (rings.empty()) {
        return out;
    }

    // ---- 嵌套判定：射线法统计每个环被包含的次数 ----
    const int nr = static_cast<int>(rings.size());
    std::vector<int> containment(static_cast<std::size_t>(nr), 0);
    // contains[j][i]：环 j 是否包含环 i（按代表点判定）。
    std::vector<std::vector<char>> contains(static_cast<std::size_t>(nr),
                                            std::vector<char>(static_cast<std::size_t>(nr), 0));

    for (int i = 0; i < nr; ++i) {
        // 代表点：按 x 升序尝试顶点，取不在任何其他环边界 tol 邻域内的第一个；
        // 防御射线法在"代表点恰在另一环边上"时的退化。
        const auto &pts = rings[static_cast<std::size_t>(i)].pts;
        std::vector<std::size_t> order(pts.size());
        std::iota(order.begin(), order.end(), std::size_t{0});
        std::sort(order.begin(), order.end(),
                  [&](std::size_t a, std::size_t b) { return pts[a].x < pts[b].x; });

        geometry::Point2D rp = pts[order[0]];
        for (const std::size_t idx : order) {
            bool safe = true;
            for (int j = 0; j < nr; ++j) {
                if (j != i && distToRing(rings[static_cast<std::size_t>(j)].pts, pts[idx]) <= tolerance) {
                    safe = false;
                    break;
                }
            }
            if (safe) {
                rp = pts[idx];
                break;
            }
        }

        for (int j = 0; j < nr; ++j) {
            if (j != i && pointInRing(rings[static_cast<std::size_t>(j)].pts, rp)) {
                ++containment[static_cast<std::size_t>(i)];
                contains[static_cast<std::size_t>(j)][static_cast<std::size_t>(i)] = 1;
            }
        }
    }

    // ---- 奇偶分内外，内环配对最小面积父外环 ----
    std::vector<int> outerIdx;                        // 外环的环索引
    std::vector<int> outerPos(static_cast<std::size_t>(nr), -1);  // 环索引 → polygons 下标
    std::vector<int> parent(static_cast<std::size_t>(nr), -1);    // 内环 → 父外环环索引
    for (int i = 0; i < nr; ++i) {
        if (containment[static_cast<std::size_t>(i)] % 2 == 0) {
            outerPos[static_cast<std::size_t>(i)] = static_cast<int>(outerIdx.size());
            outerIdx.push_back(i);
        }
    }
    for (int i = 0; i < nr; ++i) {
        if (containment[static_cast<std::size_t>(i)] % 2 == 0) {
            continue;
        }
        double bestArea = std::numeric_limits<double>::max();
        int best = -1;
        for (const int o : outerIdx) {
            if (contains[static_cast<std::size_t>(o)][static_cast<std::size_t>(i)] &&
                std::fabs(rings[static_cast<std::size_t>(o)].area) < bestArea) {
                bestArea = std::fabs(rings[static_cast<std::size_t>(o)].area);
                best = o;
            }
        }
        parent[static_cast<std::size_t>(i)] = best;
        if (best < 0) {
            out.warnings.push_back("内环未找到宿主外环（输入环可能相交），按独立外环输出");
        }
    }

    // ---- 组织输出 + 方向统一（外环 CCW / 内环 CW）----
    out.polygons.resize(outerIdx.size());
    for (const int i : outerIdx) {
        auto &poly = out.polygons[static_cast<std::size_t>(outerPos[static_cast<std::size_t>(i)])];
        poly.vertices = rings[static_cast<std::size_t>(i)].pts;
        if (rings[static_cast<std::size_t>(i)].area < 0.0) {
            std::reverse(poly.vertices.begin(), poly.vertices.end());  // CW → CCW
        }
    }
    for (int i = 0; i < nr; ++i) {
        if (containment[static_cast<std::size_t>(i)] % 2 == 0) {
            continue;
        }
        auto hole = rings[static_cast<std::size_t>(i)].pts;
        if (rings[static_cast<std::size_t>(i)].area > 0.0) {
            std::reverse(hole.begin(), hole.end());  // CCW → CW
        }
        const int p = parent[static_cast<std::size_t>(i)];
        if (p >= 0) {
            out.polygons[static_cast<std::size_t>(outerPos[static_cast<std::size_t>(p)])]
                .holes.push_back(std::move(hole));
        } else {
            // 无宿主内环：降级为独立外环输出（保持不崩溃）。
            geometry::Polygon fallback;
            fallback.vertices = std::move(hole);
            out.polygons.push_back(std::move(fallback));
        }
    }

    return out;
}

} // namespace slicing
