#include "graphics/Camera.h"

#include <Eigen/Geometry>

#include <algorithm>
#include <cmath>

namespace {

constexpr float kPi = 3.14159265358979f;

float radians(float deg) {
    return deg * kPi / 180.0f;
}

// 右手坐标系 lookAt（相机看向 -Z）。
Eigen::Matrix4f lookAt(const Eigen::Vector3f &eye, const Eigen::Vector3f &center,
                       const Eigen::Vector3f &up) {
    const Eigen::Vector3f f = (center - eye).normalized();
    const Eigen::Vector3f s = f.cross(up).normalized();
    const Eigen::Vector3f u = s.cross(f);

    Eigen::Matrix4f m = Eigen::Matrix4f::Identity();
    m(0, 0) = s.x();  m(0, 1) = s.y();  m(0, 2) = s.z();  m(0, 3) = -s.dot(eye);
    m(1, 0) = u.x();  m(1, 1) = u.y();  m(1, 2) = u.z();  m(1, 3) = -u.dot(eye);
    m(2, 0) = -f.x(); m(2, 1) = -f.y(); m(2, 2) = -f.z(); m(2, 3) = f.dot(eye);
    return m;
}

} // namespace

void Camera::setAspectRatio(float aspect) {
    m_aspect = aspect > 0.0f ? aspect : 1.0f;
}

void Camera::orbit(float dxDegrees, float dyDegrees) {
    m_yaw += dxDegrees;
    m_pitch += dyDegrees;
    m_pitch = std::clamp(m_pitch, -89.0f, 89.0f);  // 防止越过极点翻转
}

void Camera::pan(float dxPixels, float dyPixels) {
    // 沿相机右 / 上方向平移目标点，平移量与距离成正比以保持手感。
    const Eigen::Vector3f f = (m_target - position()).normalized();
    const Eigen::Vector3f worldUp(0.0f, 1.0f, 0.0f);
    const Eigen::Vector3f right = f.cross(worldUp).normalized();
    const Eigen::Vector3f up = right.cross(f);

    const float scale = m_distance * 0.0015f;
    m_target += (-dxPixels * right + dyPixels * up) * scale;
}

void Camera::zoom(float delta) {
    // delta > 0 表示靠近。
    m_distance *= (1.0f - delta * 0.001f);
    m_distance = std::clamp(m_distance, 0.01f, 10000.0f);
}

void Camera::focus(const geometry::Vec3f &center, float radius) {
    m_target = center;
    m_distance = radius * 3.0f;
    if (m_distance < 0.001f) {
        m_distance = 10.0f;
    }
    m_yaw = -90.0f;
    m_pitch = -30.0f;
}

geometry::Vec3f Camera::position() const {
    const float yawRad = radians(m_yaw);
    const float pitchRad = radians(m_pitch);
    const float cp = std::cos(pitchRad);
    return m_target + m_distance * geometry::Vec3f(
        cp * std::cos(yawRad),
        std::sin(pitchRad),
        cp * std::sin(yawRad));
}

Eigen::Matrix4f Camera::viewMatrix() const {
    return lookAt(position(), m_target, Eigen::Vector3f(0.0f, 1.0f, 0.0f));
}

Eigen::Matrix4f Camera::projectionMatrix() const {
    const float f = 1.0f / std::tan(radians(m_fov) * 0.5f);
    Eigen::Matrix4f p = Eigen::Matrix4f::Zero();
    p(0, 0) = f / m_aspect;
    p(1, 1) = f;
    p(2, 2) = (m_far + m_near) / (m_near - m_far);
    p(2, 3) = (2.0f * m_far * m_near) / (m_near - m_far);
    p(3, 2) = -1.0f;
    return p;
}
