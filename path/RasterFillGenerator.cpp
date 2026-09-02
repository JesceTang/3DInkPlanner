#include "path/RasterFillGenerator.h"

#include <algorithm>

namespace path {

namespace {

// 一条扫描线内部的填充区间 [x0, x1]。
struct Span {
    double x0;
    double x1;
};

// 一行扫描线的填充结果。
struct Row {
    double y;
    std::vector<Span> spans;
};

} // namespace

std::vector<PathSegment> RasterFillGenerator::generate(
    const geometry::Polygon &polygon, double spacing) const {
    std::vector<PathSegment> result;
    const std::size_t n = polygon.vertices.size();
    if (n < 3 || spacing <= 0.0) {
        return result;
    }

    // 1. y 范围。
    double yMin = polygon.vertices[0].y;
    double yMax = polygon.vertices[0].y;
    for (const auto &v : polygon.vertices) {
        yMin = std::min(yMin, v.y);
        yMax = std::max(yMax, v.y);
    }

    // 2. 每条扫描线求交 + even-odd 配对。
    std::vector<Row> rows;
    for (double y = yMin + spacing * 0.5; y < yMax; y += spacing) {
        std::vector<double> xs;
        for (std::size_t i = 0; i < n; ++i) {
            const geometry::Point2D &p0 = polygon.vertices[i];
            const geometry::Point2D &p1 = polygon.vertices[(i + 1) % n];
            // 半开区间 [min, max)：避免顶点被相邻两条边重复计数。
            if ((p0.y <= y && y < p1.y) || (p1.y <= y && y < p0.y)) {
                const double t = (y - p0.y) / (p1.y - p0.y);
                xs.push_back(p0.x + t * (p1.x - p0.x));
            }
        }
        std::sort(xs.begin(), xs.end());

        Row row;
        row.y = y;
        for (std::size_t k = 0; k + 1 < xs.size(); k += 2) {
            row.spans.push_back({xs[k], xs[k + 1]});
        }
        rows.push_back(std::move(row));
    }

    // 3. serpentine 输出：偶数行左→右，奇数行右→左。
    for (std::size_t r = 0; r < rows.size(); ++r) {
        const Row &row = rows[r];
        if (r % 2 == 0) {
            for (const Span &s : row.spans) {
                result.push_back({{s.x0, row.y}, {s.x1, row.y}, PathType::Print});
            }
        } else {
            for (auto it = row.spans.rbegin(); it != row.spans.rend(); ++it) {
                result.push_back({{it->x1, row.y}, {it->x0, row.y}, PathType::Print});
            }
        }
    }
    return result;
}

} // namespace path
