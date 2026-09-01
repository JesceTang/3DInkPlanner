#pragma once

#include <Eigen/Core>

#include "geometry/GeometryTypes.h"

// 轨道相机（orbit）：围绕目标点旋转 / 平移 / 缩放。
// 内部用 Eigen 手写 View / Projection 矩阵，体现对 MVP 的理解。
class Camera {
public:
    Camera() = default;

    void setAspectRatio(float aspect);

    // 交互：orbit 参数为角度增量；pan 参数为屏幕像素位移；zoom delta 为滚轮量。
    void orbit(float dxDegrees, float dyDegrees);
    void pan(float dxPixels, float dyPixels);
    void zoom(float delta);

    // 聚焦到给定中心与半径。
    void focus(const geometry::Vec3f &center, float radius);

    Eigen::Matrix4f viewMatrix() const;
    Eigen::Matrix4f projectionMatrix() const;

    geometry::Vec3f position() const;
    const geometry::Vec3f &target() const { return m_target; }

private:
    geometry::Vec3f m_target{0.0f, 0.0f, 0.0f};
    float m_distance = 10.0f;
    float m_yaw = -90.0f;    // 度，绕 Y
    float m_pitch = -30.0f;  // 度，绕 X
    float m_fov = 45.0f;     // 度
    float m_near = 0.1f;
    float m_far = 1000.0f;
    float m_aspect = 1.0f;
};
