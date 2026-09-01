#pragma once

#include "geometry/Polyline.h"

// 切片层：轮廓清理（拼接后处理）。不依赖 Qt。

namespace slicing {

// 移除轮廓中的共线冗余顶点（三点共线时删除中间点）。
//
// 切片时相邻三角形共享边，产生的线段端点会保留为"共线中间点"，
// 这些点对轮廓几何无贡献，需在拼接后移除（避免后续路径生成产生冗余停顿点）。
// 闭合轮廓额外检查首尾连接处的共线。
geometry::Polyline simplifyContour(const geometry::Polyline &poly, double tolerance);

} // namespace slicing
