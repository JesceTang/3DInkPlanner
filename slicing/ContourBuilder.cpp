#include "slicing/ContourBuilder.h"

#include <cmath>
#include <cstddef>

namespace slicing {

namespace {

// 判断 a-b-c 三点是否共线（b 到直线 ac 的距离 <= tolerance）。
// 用叉积除以 |ac| 得到点到直线距离；|ac| 过短时（退化）不判为共线。
bool collinear(const geometry::Point2D &a, const geometry::Point2D &b,
               const geometry::Point2D &c, double tol) {
    const double cross =
        (b.x - a.x) * (c.y - a.y) - (b.y - a.y) * (c.x - a.x);
    const double len = std::hypot(c.x - a.x, c.y - a.y);
    if (len <= tol) {
        return false;
    }
    return std::fabs(cross) <= tol * len;
}

} // namespace

geometry::Polyline simplifyContour(const geometry::Polyline &poly, double tolerance) {
    geometry::Polyline out;
    out.closed = poly.closed;
    out.points = poly.points;
    if (out.points.size() < 3) {
        return out;
    }

    // 逐点滚动检查：若末尾三点共线，移除中间点。
    std::vector<geometry::Point2D> pts;
    pts.reserve(out.points.size());
    for (const auto &p : out.points) {
        pts.push_back(p);
        while (pts.size() >= 3 &&
               collinear(pts[pts.size() - 3], pts[pts.size() - 2],
                         pts[pts.size() - 1], tolerance)) {
            pts.erase(pts.begin() + static_cast<std::ptrdiff_t>(pts.size() - 2));
        }
    }

    // 闭合轮廓额外处理首尾连接处的共线。
    if (out.closed && pts.size() >= 3) {
        // 首点：尾点-首点-次点共线则移除首点。
        while (pts.size() >= 3 &&
               collinear(pts[pts.size() - 1], pts[0], pts[1], tolerance)) {
            pts.erase(pts.begin());
        }
        // 尾点：次尾-尾-首共线则移除尾点。
        while (pts.size() >= 3 &&
               collinear(pts[pts.size() - 2], pts[pts.size() - 1], pts[0],
                         tolerance)) {
            pts.pop_back();
        }
    }

    out.points = std::move(pts);
    return out;
}

} // namespace slicing
