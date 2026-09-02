#pragma once

#include <vector>

#include "geometry/GeometryTypes.h"

// 几何层 2D 多边形。按分层原则，本层不依赖 Qt UI。

namespace geometry {

// 简单闭合多边形：有序顶点（首尾隐式相连，不重复存储首点）。
// 第一版不处理自交多边形。
struct Polygon {
    std::vector<Point2D> vertices;
};

} // namespace geometry
