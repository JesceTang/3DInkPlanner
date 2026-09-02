#pragma once

#include <vector>

#include "geometry/Polygon.h"
#include "path/ToolPath.h"

// 路径层：为闭合轮廓生成 Raster（等距扫描线）填充路径。不依赖 Qt。

namespace path {

// 为简单闭合 Polygon 生成等距水平扫描填充路径。
//
// 算法（指导 §25）：
//   1. 生成水平扫描线 y = y0 + n * spacing（偏移半个 spacing 避免切过顶点）。
//   2. 每条扫描线与多边形边求交，交点按 x 排序。
//   3. even-odd rule 两两配对得到内部扫描段。
//   4. 相邻扫描行 serpentine 顺序（偶数行左→右，奇数行右→左），减少空走。
//
// 第一版：angle 固定 0°（水平），不处理自交多边形，不生成 Travel 连接段。
class RasterFillGenerator {
public:
    std::vector<PathSegment> generate(
        const geometry::Polygon &polygon, double spacing) const;
};

} // namespace path
