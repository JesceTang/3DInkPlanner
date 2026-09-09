#pragma once

#include <vector>

#include "geometry/Polygon.h"
#include "path/ToolPath.h"

// 路径层：路径优化器。不依赖 Qt。
//
// 职责（KNOWN_ISSUES 挂账项：RasterFill 只生成 Print 段，本层补齐后续工序）：
//   1. 多 polygon（同层多个独立岛）按最近邻贪心排序，减少跨岛空走；
//   2. 相邻 Print 段之间的断点插入 Travel 连接段（喷头关闭移动）；
//   3. 统计喷印 / 空走长度（用于工艺评估与回归校验）。
//
// 非职责：不改变 Raster 行内的 serpentine 顺序（行序本身已是局部最优）；
// 不做 2-opt/遗传等全局优化（demo 规模最近邻已足够，留作扩展）。

namespace path {

// 优化结果：完整段序列（Print/Travel 交替）+ 长度统计。
struct OptimizedPath {
    std::vector<PathSegment> segments;  // 首段为 Print；断点前必有 Travel
    double printLength = 0.0;
    double travelLength = 0.0;
};

class PathOptimizer {
public:
    // polygons：一层内分类后的带孔多边形（各自独立 Raster 填充）。
    // startPoint：本层喷头起始位置（通常为上一位置或原点），用于选第一个岛。
    // tolerance：相邻段端点距离 ≤ tolerance 视为连续，不插 Travel。
    OptimizedPath optimize(const std::vector<geometry::Polygon> &polygons,
                           double spacing,
                           geometry::Point2D startPoint = {0.0, 0.0},
                           double tolerance = 1e-9) const;
};

} // namespace path
