#pragma once

#include <optional>

#include "geometry/GeometryTypes.h"

// 切片层：三角网格与水平面 z=h 求交。
// 分层原则：本层不依赖 Qt，仅依赖 geometry 层。

namespace slicing {

// 三角形与水平面 z=h 求交。
//
// 输入：三角形（float 顶点）、切片高度 h、浮点容差 eps。
// 输出：
//   - 有交线：返回一个有效 Segment2D（两个不同的端点）
//   - 无交线：返回 std::nullopt
//
// 判定规则（对应指导文件 §23 的四个设计决策）：
//   1. 返回 Segment：交线有两个不同的交点（普通穿过 / 一顶点在平面且另两在两侧 / 两顶点在平面）。
//   2. 忽略（返回 nullopt）：全在平面上方 / 全在下方 / 一顶点在平面且另两同侧（退化为单点）
//      / 退化三角形（三顶点共线或重合）。
//   3. 共面三角形第一版：忽略（共面区域属于"填充"而非"边界"，轮廓由相邻非共面三角形产生）。
//   4. 避免重复交点：收集交点后用 eps 去重（同一交点可能被相邻两条边重复报告）。
std::optional<geometry::Segment2D> intersectTrianglePlane(
    const geometry::Triangle &t, double h, double eps);

} // namespace slicing
