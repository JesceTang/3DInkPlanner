#pragma once

#include <vector>

#include "geometry/GeometryTypes.h"

// 几何层 2D 折线。按分层原则，本层不依赖 Qt UI。

namespace geometry {

// 折线：有序顶点序列，可为闭合轮廓或开放路径。
// closed 表示首尾端点是否在容差内相接（闭合轮廓）。
struct Polyline {
    std::vector<Point2D> points;
    bool closed = false;
};

} // namespace geometry
