#pragma once

#include "geometry/GeometryTypes.h"

// 路径层基础类型。按分层原则，本层不依赖 Qt UI。

namespace path {

// 路径段类型。
enum class PathType {
    Print,   // 喷印移动（喷头出墨）
    Travel,  // 空走移动（喷头关闭）
};

// 一条 2D 路径段（起点 → 终点）。
struct PathSegment {
    geometry::Point2D start;
    geometry::Point2D end;
    PathType type = PathType::Print;
};

} // namespace path
