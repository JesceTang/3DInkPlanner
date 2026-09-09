#include "slicing/SegmentConnector.h"

#include <array>
#include <cmath>
#include <cstddef>
#include <unordered_map>

namespace slicing {

namespace {

// 量化栅格键：floor(coord / tol)，容差内的点必落在同一格或相邻格。
struct GridKey2D {
    long long x = 0;
    long long y = 0;
    bool operator==(const GridKey2D &o) const noexcept {
        return x == o.x && y == o.y;
    }
};

struct GridKey2DHash {
    std::size_t operator()(const GridKey2D &k) const noexcept {
        std::size_t h = std::hash<long long>{}(k.x);
        h = h * 1000003u ^ std::hash<long long>{}(k.y);
        return h;
    }
};

double distSq(const geometry::Point2D &a, const geometry::Point2D &b) noexcept {
    const double dx = a.x - b.x;
    const double dy = a.y - b.y;
    return dx * dx + dy * dy;
}

} // namespace

std::vector<geometry::Polyline> SegmentConnector::connect(
    const std::vector<geometry::Segment2D> &segments, double tolerance) const {
    return connectWithReport(segments, tolerance).polylines;
}

SegmentConnector::Report SegmentConnector::connectWithReport(
    const std::vector<geometry::Segment2D> &segments, double tolerance) const {
    Report rep;
    if (segments.empty() || tolerance <= 0.0) {
        return rep;
    }
    const double tolSq = tolerance * tolerance;
    const double invTol = 1.0 / tolerance;

    // ---- 1. 端点焊接（2D 空间哈希，9 邻域查询；退化段显式丢弃并告警）----
    std::vector<geometry::Point2D> verts;          // 焊接后唯一顶点（代表点 = 首次出现坐标）
    std::vector<std::array<int, 2>> edges;         // 每条线段的两个顶点 ID
    std::unordered_map<GridKey2D, std::vector<int>, GridKey2DHash> grid;
    verts.reserve(segments.size());
    edges.reserve(segments.size());
    grid.reserve(segments.size());

    auto weldPoint = [&](const geometry::Point2D &p) -> int {
        const long long ix = static_cast<long long>(std::floor(p.x * invTol));
        const long long iy = static_cast<long long>(std::floor(p.y * invTol));
        int best = -1;
        double bestSq = tolSq;  // 只接受容差内的点
        for (long long dx = -1; dx <= 1; ++dx) {
            for (long long dy = -1; dy <= 1; ++dy) {
                const auto it = grid.find(GridKey2D{ix + dx, iy + dy});
                if (it == grid.end()) {
                    continue;
                }
                for (const int vi : it->second) {
                    const double d2 = distSq(verts[static_cast<std::size_t>(vi)], p);
                    if (d2 <= bestSq) {
                        bestSq = d2;
                        best = vi;
                    }
                }
            }
        }
        if (best >= 0) {
            return best;
        }
        const int idx = static_cast<int>(verts.size());
        verts.push_back(p);
        grid[GridKey2D{ix, iy}].push_back(idx);
        return idx;
    };

    for (const auto &s : segments) {
        if (distSq(s.p0, s.p1) <= tolSq) {
            rep.warnings.push_back("退化线段（两端点重合于容差内）已丢弃");
            continue;
        }
        edges.push_back({weldPoint(s.p0), weldPoint(s.p1)});
    }

    // ---- 2. 邻接表：顶点 → 关联线段 ----
    std::vector<std::vector<int>> incident(verts.size());
    for (int ei = 0; ei < static_cast<int>(edges.size()); ++ei) {
        incident[static_cast<std::size_t>(edges[static_cast<std::size_t>(ei)][0])].push_back(ei);
        incident[static_cast<std::size_t>(edges[static_cast<std::size_t>(ei)][1])].push_back(ei);
    }

    // ---- 3. 遍历拼接（每段仅用一次，整体 O(n)）----
    std::vector<char> used(edges.size(), 0);

    // 在顶点 v 处沿当前方向 (from → v) 选择未用的后继线段：唯一候选直接用，
    // 多候选（T 型接头）选方向点积最大（最直）者。返回 -1 表示无后继（链端）。
    auto pickNext = [&](int v, int from, std::vector<int> *branchEdges) -> int {
        int best = -1;
        double bestDot = -2.0;  // 单位向量点积下限 -1
        const geometry::Point2D &pv = verts[static_cast<std::size_t>(v)];
        const geometry::Point2D &pf = verts[static_cast<std::size_t>(from)];
        const double dcx = pv.x - pf.x;
        const double dcy = pv.y - pf.y;
        const double lenC = std::hypot(dcx, dcy);
        for (const int ei : incident[static_cast<std::size_t>(v)]) {
            if (used[static_cast<std::size_t>(ei)]) {
                continue;
            }
            const auto &e = edges[static_cast<std::size_t>(ei)];
            const int other = (e[0] == v) ? e[1] : e[0];
            if (branchEdges != nullptr) {
                branchEdges->push_back(ei);
            }
            if (lenC > 0.0) {
                const geometry::Point2D &po = verts[static_cast<std::size_t>(other)];
                double dx = po.x - pv.x;
                double dy = po.y - pv.y;
                const double len = std::hypot(dx, dy);
                const double dot = (len > 0.0) ? (dcx * dx + dcy * dy) / (lenC * len) : 1.0;
                if (dot > bestDot) {
                    bestDot = dot;
                    best = ei;
                }
            } else if (best < 0) {
                best = ei;
            }
        }
        return best;
    };

    for (int start = 0; start < static_cast<int>(edges.size()); ++start) {
        if (used[static_cast<std::size_t>(start)]) {
            continue;
        }
        used[static_cast<std::size_t>(start)] = 1;
        const auto &e0 = edges[static_cast<std::size_t>(start)];

        // 链首尾顶点 ID 与坐标序列（头部延伸结果先反转暂存，最后合并）。
        int headV = e0[0];
        int tailV = e0[1];
        std::vector<geometry::Point2D> chain{verts[static_cast<std::size_t>(headV)],
                                             verts[static_cast<std::size_t>(tailV)]};

        // 前向延伸（沿 tail）。
        int prevV = headV;
        while (true) {
            std::vector<int> branches;
            const int next = pickNext(tailV, prevV, &branches);
            if (branches.size() > 1) {
                const auto &p = verts[static_cast<std::size_t>(tailV)];
                rep.warnings.push_back(
                    "T 型接头：顶点 (" + std::to_string(p.x) + ", " + std::to_string(p.y) +
                    ") 度数 " + std::to_string(branches.size() + 1) + "，按最小转角选主链");
            }
            if (next < 0) {
                break;
            }
            used[static_cast<std::size_t>(next)] = 1;
            const auto &e = edges[static_cast<std::size_t>(next)];
            const int other = (e[0] == tailV) ? e[1] : e[0];
            chain.push_back(verts[static_cast<std::size_t>(other)]);
            prevV = tailV;
            tailV = other;
            if (tailV == headV) {
                break;  // 回到起点：闭合
            }
        }

        // 后向延伸（沿 head；新点按相反顺序追加到 frontBuf）。
        // 初始进入方向取自链中 head 的邻居 e0[1]（与 tailV 无关，tailV 可能已走远）。
        std::vector<geometry::Point2D> frontBuf;
        prevV = e0[1];
        while (headV != tailV) {  // 已闭合则无需后向延伸
            std::vector<int> branches;
            const int next = pickNext(headV, prevV, &branches);
            if (next < 0) {
                break;
            }
            used[static_cast<std::size_t>(next)] = 1;
            const auto &e = edges[static_cast<std::size_t>(next)];
            const int other = (e[0] == headV) ? e[1] : e[0];
            frontBuf.push_back(verts[static_cast<std::size_t>(other)]);
            prevV = headV;
            headV = other;
            if (headV == tailV) {
                break;
            }
        }

        geometry::Polyline pl;
        pl.points.reserve(frontBuf.size() + chain.size());
        for (auto it = frontBuf.rbegin(); it != frontBuf.rend(); ++it) {
            pl.points.push_back(*it);
        }
        pl.points.insert(pl.points.end(), chain.begin(), chain.end());
        pl.closed = (headV == tailV) && pl.points.size() > 1;
        if (pl.closed) {
            pl.points.pop_back();  // 去掉与首点重复的尾点（保留唯一顶点序列）
        } else {
            const double gap = std::sqrt(distSq(verts[static_cast<std::size_t>(headV)],
                                                verts[static_cast<std::size_t>(tailV)]));
            rep.warnings.push_back("断链告警：开放轮廓 " + std::to_string(pl.points.size()) +
                                   " 顶点，首尾间距 " + std::to_string(gap) + "（> 容差）");
        }
        rep.polylines.push_back(std::move(pl));
    }

    return rep;
}

} // namespace slicing
