#pragma once

#include <array>
#include <vector>

#include "geometry/GeometryTypes.h"

// 拓扑层：带共享顶点的索引网格（从 STL triangle soup 派生的只读视图 + 拓扑统计）。
// 原始 soup 数据不被修改，本层结构全部为派生数据。不依赖 Qt。

namespace topology {

// 索引网格：唯一顶点数组 + 面索引（v0,v1,v2 逆时针序对应正法向）。
struct IndexedMesh {
    std::vector<geometry::Vec3f> vertices;
    std::vector<std::array<int, 3>> faces;

    bool empty() const { return faces.empty(); }

    // 面法向（叉积 (v1-v0)×(v2-v0)，未归一化；模长为 2 倍面积）。
    geometry::Vec3f faceNormal(int fi) const {
        const auto &f = faces[static_cast<std::size_t>(fi)];
        const auto &a = vertices[static_cast<std::size_t>(f[0])];
        const auto &b = vertices[static_cast<std::size_t>(f[1])];
        const auto &c = vertices[static_cast<std::size_t>(f[2])];
        return (b - a).cross(c - a);
    }

    // 有符号体积：Σ det(a,b,c)/6。闭合且绕序一致朝外时为正，数值等于实体体积。
    double signedVolume() const {
        double vol = 0.0;
        for (const auto &f : faces) {
            const auto &a = vertices[static_cast<std::size_t>(f[0])];
            const auto &b = vertices[static_cast<std::size_t>(f[1])];
            const auto &c = vertices[static_cast<std::size_t>(f[2])];
            vol += static_cast<double>(a.x()) *
                       (static_cast<double>(b.y()) * c.z() - static_cast<double>(b.z()) * c.y()) -
                   static_cast<double>(a.y()) *
                       (static_cast<double>(b.x()) * c.z() - static_cast<double>(b.z()) * c.x()) +
                   static_cast<double>(a.z()) *
                       (static_cast<double>(b.x()) * c.y() - static_cast<double>(b.y()) * c.x());
        }
        return vol / 6.0;
    }
};

// 拓扑统计报告（analyze 的输出）。
struct TopologyReport {
    int vertexCount = 0;
    int faceCount = 0;
    int edgeCount = 0;             // 唯一无向边总数
    int manifoldEdgeCount = 0;     // 恰好 2 个面共享的边（健康闭合网格）
    int boundaryEdgeCount = 0;     // 仅 1 个面的边（开放边界 / 模型孔洞）
    int nonManifoldEdgeCount = 0;  // 3 个及以上面共享的边（病态网格，只报告不修复）
    int componentCount = 0;        // 面邻接连通分量数
    int degenerateFaceCount = 0;   // 焊接后顶点坍缩被丢弃的退化面数
    double signedVolume = 0.0;
};

} // namespace topology
