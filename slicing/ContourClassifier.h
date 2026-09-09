#pragma once

#include <string>
#include <vector>

#include "geometry/Polygon.h"
#include "geometry/Polyline.h"

// 切片层：闭合轮廓的内外环分类（嵌套孔洞识别）。不依赖 Qt。

namespace slicing {

// 分类结果：带孔多边形 + 无法闭合的断链 + 告警清单。
struct ClassifiedContours {
    std::vector<geometry::Polygon> polygons;     // 每个 Polygon = 外环(CCW) + 内环孔洞(CW)
    std::vector<geometry::Polyline> openChains;  // 未闭合的链（断面/非流形输入）
    std::vector<std::string> warnings;
};

// 把一层轮廓（闭合 + 开放混合）分类为「外环 + 内环」的带孔多边形。
//
// 算法（禁止面积启发式猜内外）：
//   1. 闭合环计算 shoelace 有向面积；开放链进 openChains 并告警。
//   2. 嵌套判定：射线法统计每个环被其他环包含的次数（containment）；
//      偶数 → 外环，奇数 → 内环（孔洞）。
//   3. 内环配对：分配给「包含它的外环中面积最小者」（直接父环）。
//   4. 方向统一：外环 CCW（面积正）、内环 CW（面积负），不符则反转顶点序。
//
// 约定：切片轮廓环之间互不相交（病态网格产生的相交环不保证分类正确）。
class ContourClassifier {
public:
    ClassifiedContours classify(const std::vector<geometry::Polyline> &contours,
                                double tolerance) const;
};

} // namespace slicing
