#include "slicing/SegmentConnector.h"

#include <cmath>
#include <cstddef>

namespace slicing {

namespace {

// 两个 2D 端点是否在容差内重合。
bool endpointsMatch(const geometry::Point2D &a, const geometry::Point2D &b,
                    double tol) {
    return std::hypot(a.x - b.x, a.y - b.y) <= tol;
}

} // namespace

std::vector<geometry::Polyline> SegmentConnector::connect(
    const std::vector<geometry::Segment2D> &segments, double tolerance) const {
    const std::size_t n = segments.size();
    std::vector<bool> used(n, false);
    std::vector<geometry::Polyline> result;

    for (std::size_t i = 0; i < n; ++i) {
        if (used[i]) {
            continue;
        }
        used[i] = true;

        geometry::Polyline pl;
        pl.points.push_back(segments[i].p0);
        pl.points.push_back(segments[i].p1);

        // 前向延伸：沿 p1 方向接后续线段。
        bool extended = true;
        while (extended) {
            extended = false;
            const geometry::Point2D tail = pl.points.back();
            for (std::size_t j = 0; j < n; ++j) {
                if (used[j]) {
                    continue;
                }
                if (endpointsMatch(segments[j].p0, tail, tolerance)) {
                    pl.points.push_back(segments[j].p1);
                    used[j] = true;
                    extended = true;
                    break;
                }
                if (endpointsMatch(segments[j].p1, tail, tolerance)) {
                    pl.points.push_back(segments[j].p0);
                    used[j] = true;
                    extended = true;
                    break;
                }
            }
        }

        // 后向延伸：沿 p0 方向接前序线段（注意方向取反）。
        extended = true;
        while (extended) {
            extended = false;
            const geometry::Point2D head = pl.points.front();
            for (std::size_t j = 0; j < n; ++j) {
                if (used[j]) {
                    continue;
                }
                if (endpointsMatch(segments[j].p1, head, tolerance)) {
                    pl.points.insert(pl.points.begin(), segments[j].p0);
                    used[j] = true;
                    extended = true;
                    break;
                }
                if (endpointsMatch(segments[j].p0, head, tolerance)) {
                    pl.points.insert(pl.points.begin(), segments[j].p1);
                    used[j] = true;
                    extended = true;
                    break;
                }
            }
        }

        // 闭合判断：首尾重合则闭合，去掉与首点重复的尾点（保留唯一顶点序列）。
        pl.closed = endpointsMatch(pl.points.front(), pl.points.back(), tolerance);
        if (pl.closed && pl.points.size() > 1) {
            pl.points.pop_back();
        }
        result.push_back(std::move(pl));
    }

    return result;
}

} // namespace slicing
