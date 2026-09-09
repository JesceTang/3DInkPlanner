#pragma once

#include <Eigen/Core>

#include "geometry/GeometryTypes.h"

// 几何层 3D 仿射变换。按分层原则，本层不依赖 Qt UI。

namespace geometry {

// 3D 仿射变换（4×4 齐次矩阵），用于模型坐标 → 设备坐标。
//
// 变换顺序：先缩放、再绕 Z 旋转、最后平移，即 T = Translation · RotationZ · Scale。
// 设备坐标链：P_machine = T_machine_part · T_part_model · P_model。
class Transform {
public:
    // 单位变换。
    Transform();

    // 由 X/Y/Z 平移 + 绕 Z 旋转（度）+ 均匀缩放构造。
    static Transform fromOffsetRotationScale(
        const Vec3d &offset, double rotationZDeg, double scale);

    // 纯平移。
    static Transform translation(const Vec3d &offset);

    // 应用变换到 3D 点。
    Vec3d apply(const Vec3d &p) const;

    // 组合：返回 this ∘ other（先应用 other，再应用 this）。
    Transform compose(const Transform &other) const;

    // 底层 4×4 矩阵（用于导出/调试）。
    const Mat4d &matrix() const { return m_matrix; }

private:
    explicit Transform(const Mat4d &m);

    Mat4d m_matrix;
};

} // namespace geometry
