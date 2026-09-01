#pragma once

#include <vector>

#include "geometry/GeometryTypes.h"
#include "geometry/Polyline.h"

// 切片层：把无序线段拼接成有序轮廓。不依赖 Qt。

namespace slicing {

// 把三角网格切片得到的无序线段连接成有序轮廓。
//
// 算法（第一版优先正确性，不追求 O(n log n)）：
//   1. 端点近邻匹配：两个端点距离 <= tolerance 视为重合（勿用 == 直接比较浮点）。
//   2. 逐段拼接：从一条未用线段出发，沿两端（前向/后向）贪心拼接相邻线段。
//   3. 闭合判断：首尾端点在容差内即 closed=true。
//   4. 无法闭合的链保留为 open polyline（closed=false），不丢弃。
//
// 输出：闭合轮廓（closed=true）与开放路径（closed=false）混在结果中。
class SegmentConnector {
public:
    std::vector<geometry::Polyline> connect(
        const std::vector<geometry::Segment2D> &segments, double tolerance) const;
};

} // namespace slicing
