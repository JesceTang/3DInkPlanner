#pragma once

#include <vector>

#include "geometry/Polygon.h"
#include "path/ToolPath.h"

// 路径层：为闭合轮廓生成 Raster（等距扫描线）填充路径。不依赖 Qt。

namespace path {

// 为闭合多边形（可带孔）生成等距水平扫描填充路径。
//
// 算法：
//   1. 生成水平扫描线 y = y0 + n * spacing（偏移半个 spacing 避免切过顶点）。
//   2. 扫描线与外环 + 所有内环（孔洞）的边求交，交点按 x 排序。
//   3. even-odd rule 两两配对得到内部扫描段（孔洞进/出各翻转一次奇偶，孔内自然无段）。
//   4. 相邻扫描行 serpentine 顺序（偶数行左→右，奇数行右→左），减少空走。
//
// 不处理自交多边形，不生成 Travel 连接段。
class RasterFillGenerator {
public:
    std::vector<PathSegment> generate(
        const geometry::Polygon &polygon, double spacing) const;
};

} // namespace path
