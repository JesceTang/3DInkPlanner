#pragma once

#include <limits>
#include <vector>

#include <Eigen/Core>

// 几何层基础类型。按分层原则，本层不依赖 Qt UI。

namespace geometry {

using Vec3f = Eigen::Vector3f;
using Vec3d = Eigen::Vector3d;
using Mat4d = Eigen::Matrix4d;

// 几何层全局浮点容差。所有浮点几何判断统一使用（勿用 == 直接比较浮点）。
// 实际使用中可根据 STL 尺寸调整（见 KNOWN_ISSUES.md）。
constexpr double kGeometryEpsilon = 1e-6;

// 2D 点（切片等平面几何用 double 精度）。
struct Point2D {
    double x = 0.0;
    double y = 0.0;
};

// 2D 线段（切片轮廓片段）。
struct Segment2D {
    Point2D p0;
    Point2D p1;
};

// 三角面片：STL 中每个面片独立存储 3 个顶点 + 法线（无共享顶点拓扑）。
struct Triangle {
    Vec3f v0;
    Vec3f v1;
    Vec3f v2;
    Vec3f normal;
};

// 三角网格：三角形列表 + 轴对齐包围盒（AABB）。
struct Mesh {
    std::vector<Triangle> triangles;
    Vec3f minBound = Vec3f::Constant(std::numeric_limits<float>::max());
    Vec3f maxBound = Vec3f::Constant(std::numeric_limits<float>::lowest());

    // 重新计算包围盒（读取模型后调用一次）。
    void computeBounds();

    // 包围盒中心（用于相机聚焦）。
    Vec3f center() const;

    // 包围盒最大边长（用于相机距离缩放）。
    float maxDimension() const;
};

inline void Mesh::computeBounds() {
    if (triangles.empty()) {
        minBound = maxBound = Vec3f::Zero();
        return;
    }
    minBound = Vec3f::Constant(std::numeric_limits<float>::max());
    maxBound = Vec3f::Constant(std::numeric_limits<float>::lowest());
    for (const Triangle &t : triangles) {
        const Vec3f verts[3] = {t.v0, t.v1, t.v2};
        for (const Vec3f &v : verts) {
            minBound = minBound.cwiseMin(v);
            maxBound = maxBound.cwiseMax(v);
        }
    }
}

inline Vec3f Mesh::center() const {
    return (minBound + maxBound) * 0.5f;
}

inline float Mesh::maxDimension() const {
    return (maxBound - minBound).maxCoeff();
}

} // namespace geometry
