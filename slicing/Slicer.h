#pragma once

#include <vector>

#include "geometry/GeometryTypes.h"
#include "geometry/Polyline.h"
#include "slicing/ContourClassifier.h"

// 切片层：整合三角形求交 + 轮廓拼接 + 内外环分类，生成单层/全部层轮廓。不依赖 Qt。

namespace slicing {

// 一层切片结果。
struct Layer {
    double z = 0.0;                            // 切片平面高度
    std::vector<geometry::Polyline> contours;  // 拼接+简化后的轮廓（闭合 + 开放，调试/兼容用）
    ClassifiedContours classified;             // 内外环分类结果（polygons / openChains / warnings）
    int coplanarTriangleCount = 0;             // 与该层共面的水平三角形数（>0 时轮廓可能不完整）
};

// 切片器：遍历网格所有三角形，生成指定高度或全部层的轮廓。
class Slicer {
public:
    // 单个 Z 平面切片（轻量版：求交 + 拼接 + 简化，不做内外环分类）。
    // tolerance 同时用于三角形-平面求交与轮廓拼接（统一几何容差）。
    std::vector<geometry::Polyline> slice(
        const geometry::Mesh &mesh, double z, double tolerance) const;

    // 单个 Z 平面完整切片：在 slice 基础上做内外环分类（孔洞识别 + 断链告警）。
    Layer sliceLayer(const geometry::Mesh &mesh, double z, double tolerance) const;

    // 完整分层（等距）：zMin → zMax，按 layerHeight 生成全部层。
    // 第一层从 zMin + layerHeight/2 开始，避免切片面正好落在底面/顶面。
    // numThreads：并行线程数；0 = auto（hardware_concurrency），1 = 强制串行。
    // 各层独立计算、结果槽位固定，并行输出与串行逐点一致。
    std::vector<Layer> sliceAll(
        const geometry::Mesh &mesh, double layerHeight, double tolerance,
        unsigned int numThreads = 0) const;

    // 完整分层（可变层厚）：按显式 z 高度表逐层切片。
    // 落在 (zMin, zMax) 区间外的 z 被跳过（显式忽略，不算告警）；
    // 输出层统一按 z 升序（扫描线算法要求，也符合打印顺序）。
    std::vector<Layer> sliceAll(
        const geometry::Mesh &mesh, const std::vector<double> &zList, double tolerance,
        unsigned int numThreads = 0) const;
};

} // namespace slicing
