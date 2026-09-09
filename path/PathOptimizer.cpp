#include "path/PathOptimizer.h"

#include <cmath>
#include <limits>

#include "path/RasterFillGenerator.h"

namespace path {

namespace {

double dist(const geometry::Point2D &a, const geometry::Point2D &b) {
    return std::hypot(a.x - b.x, a.y - b.y);
}

} // namespace

OptimizedPath PathOptimizer::optimize(
    const std::vector<geometry::Polygon> &polygons, double spacing,
    geometry::Point2D startPoint, double tolerance) const {
    OptimizedPath result;

    // 1. 每个 polygon 独立生成 Raster Print 段（保持各自 serpentine 行序）。
    const RasterFillGenerator raster;
    std::vector<std::vector<PathSegment>> islandPaths;
    islandPaths.reserve(polygons.size());
    for (const auto &poly : polygons) {
        auto segs = raster.generate(poly, spacing);
        if (!segs.empty()) {
            islandPaths.push_back(std::move(segs));
        }
    }
    if (islandPaths.empty()) {
        return result;
    }

    // 2. 岛间最近邻贪心排序：从 startPoint 出发，每次选「首段起点最近」的岛。
    //    复杂度 O(P²)，P 为同层岛数（正常模型 < 100，可接受）。
    std::vector<bool> used(islandPaths.size(), false);
    geometry::Point2D cursor = startPoint;
    for (size_t picked = 0; picked < islandPaths.size(); ++picked) {
        size_t best = islandPaths.size();
        double bestDist = std::numeric_limits<double>::max();
        for (size_t i = 0; i < islandPaths.size(); ++i) {
            if (used[i]) {
                continue;
            }
            const double d = dist(cursor, islandPaths[i].front().start);
            if (d < bestDist) {
                bestDist = d;
                best = i;
            }
        }
        used[best] = true;
        const auto &segs = islandPaths[best];

        // 3. 拼接入主序列：断点（prev.end != next.start）前插入 Travel 段。
        for (const auto &seg : segs) {
            if (!result.segments.empty()) {
                const auto &prevEnd = result.segments.back().end;
                if (dist(prevEnd, seg.start) > tolerance) {
                    PathSegment travel;
                    travel.start = prevEnd;
                    travel.end = seg.start;
                    travel.type = PathType::Travel;
                    result.travelLength += dist(prevEnd, seg.start);
                    result.segments.push_back(travel);
                }
            }
            result.printLength += dist(seg.start, seg.end);
            result.segments.push_back(seg);
        }
        cursor = segs.back().end;
    }
    return result;
}

} // namespace path
