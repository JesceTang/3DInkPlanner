#pragma once

#include <string>
#include <vector>

#include "geometry/GeometryTypes.h"
#include "geometry/Polyline.h"

// 切片层：把无序线段拼接成有序轮廓。不依赖 Qt。

namespace slicing {

// 把三角网格切片得到的无序线段连接成有序轮廓（V2.0：端点哈希 O(n) 版）。
//
// 算法：
//   1. 端点焊接：量化坐标空间哈希，距离 <= tolerance 的端点合并为同一顶点（勿用 == 比浮点）。
//   2. 邻接拼接：建「顶点 → 关联线段」表，从任一未用线段出发双向延伸；
//      分支顶点（度数 > 2 的 T 型接头）按「最小转角」（方向点积最大）选择主链后继。
//   3. 闭合判断：延伸后首尾顶点 ID 相同即 closed=true（焊接保证容差语义）。
//   4. 断链告警：无法闭合的链保留为 open polyline 并记录 warning（不崩溃、不丢弃）。
//
// 输出：闭合轮廓（closed=true）与开放路径（closed=false）混在结果中。
class SegmentConnector {
public:
    // 拼接报告：结果 + 告警清单（断链 / T 型接头 / 退化段）。
    struct Report {
        std::vector<geometry::Polyline> polylines;
        std::vector<std::string> warnings;
    };

    // 兼容接口：仅返回拼接结果（等价于 connectWithReport 丢弃告警）。
    std::vector<geometry::Polyline> connect(
        const std::vector<geometry::Segment2D> &segments, double tolerance) const;

    // 完整接口：返回拼接结果与告警清单。
    Report connectWithReport(
        const std::vector<geometry::Segment2D> &segments, double tolerance) const;
};

} // namespace slicing
