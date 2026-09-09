#pragma once

#include <vector>

#include "geometry/GeometryTypes.h"

// 几何层 2D 多边形。按分层原则，本层不依赖 Qt UI。

namespace geometry {

// 带孔多边形：外环 + 若干内环（孔洞）。
// vertices 为外环（约定逆时针 CCW），holes 为内环（约定顺时针 CW），
// 均为有序顶点（首尾隐式相连，不重复存储首点）。
// 不处理自交多边形。
struct Polygon {
    std::vector<Point2D> vertices;              // 外环
    std::vector<std::vector<Point2D>> holes;    // 内环（孔洞），可为空

    // 外环有向面积（shoelace）：CCW 为正。
    double signedArea() const {
        double a = 0.0;
        const std::size_t n = vertices.size();
        for (std::size_t i = 0; i < n; ++i) {
            const Point2D &p = vertices[i];
            const Point2D &q = vertices[(i + 1) % n];
            a += p.x * q.y - q.x * p.y;
        }
        return a / 2.0;
    }
};

} // namespace geometry
