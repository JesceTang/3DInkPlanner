#pragma once

#include <vector>

#include "geometry/GeometryTypes.h"
#include "geometry/Polyline.h"

// 切片层：整合三角形求交 + 轮廓拼接，生成单层/全部层轮廓。不依赖 Qt。

namespace slicing {

// 一层切片结果。
struct Layer {
    double z = 0.0;                            // 切片平面高度
    std::vector<geometry::Polyline> contours;  // 该层轮廓（闭合 + 开放路径）
};

// 切片器：遍历网格所有三角形，生成指定高度或全部层的轮廓。
class Slicer {
public:
    // 单个 Z 平面切片：返回该层的闭合轮廓与开放路径。
    // tolerance 同时用于三角形-平面求交与轮廓拼接（统一几何容差）。
    std::vector<geometry::Polyline> slice(
        const geometry::Mesh &mesh, double z, double tolerance) const;

    // 完整分层：zMin → zMax，按 layerHeight 生成全部层。
    // 第一层从 zMin + layerHeight/2 开始，避免切片面正好落在底面/顶面。
    std::vector<Layer> sliceAll(
        const geometry::Mesh &mesh, double layerHeight, double tolerance) const;
};

} // namespace slicing
