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

    // 参与求交的环：外环 + 全部内环（孔洞）。
    // even-odd 配对天然扣除孔洞：进/出孔各翻转一次奇偶，孔内不产生填充段。
    std::vector<const std::vector<geometry::Point2D> *> rings;
    rings.reserve(1 + polygon.holes.size());
    rings.push_back(&polygon.vertices);
    for (const auto &hole : polygon.holes) {
        if (hole.size() >= 3) {
            rings.push_back(&hole);
        }
    }

    // y 范围按外环计算（孔洞必在外环内部）。
    double yMin = polygon.vertices[0].y;
    double yMax = polygon.vertices[0].y;
    for (const auto &v : polygon.vertices) {
        yMin = std::min(yMin, v.y);
        yMax = std::max(yMax, v.y);
    }

    // 每条扫描线与所有环求交 + even-odd 配对。
    std::vector<Row> rows;
    for (double y = yMin + spacing * 0.5; y < yMax; y += spacing) {
        std::vector<double> xs;
        for (const auto *ring : rings) {
            const auto &pts = *ring;
            const std::size_t m = pts.size();
            for (std::size_t i = 0; i < m; ++i) {
                const geometry::Point2D &p0 = pts[i];
                const geometry::Point2D &p1 = pts[(i + 1) % m];
                // 半开区间 [min, max)：避免顶点被相邻两条边重复计数。
                if ((p0.y <= y && y < p1.y) || (p1.y <= y && y < p0.y)) {
                    const double t = (y - p0.y) / (p1.y - p0.y);
                    xs.push_back(p0.x + t * (p1.x - p0.x));
                }
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

    // serpentine 输出：偶数行左→右，奇数行右→左。
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
