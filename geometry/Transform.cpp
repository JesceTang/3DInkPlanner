#include "geometry/Transform.h"

#include <cmath>

namespace geometry {

namespace {

constexpr double kPi = 3.14159265358979323846;

double degToRad(double deg) {
    return deg * kPi / 180.0;
}

} // namespace

Transform::Transform() : m_matrix(Mat4d::Identity()) {}

Transform Transform::translation(const Vec3d &offset) {
    Mat4d m = Mat4d::Identity();
    m(0, 3) = offset.x();
    m(1, 3) = offset.y();
    m(2, 3) = offset.z();
    return Transform(m);
}

Transform Transform::fromOffsetRotationScale(
    const Vec3d &offset, double rotationZDeg, double scale) {
    Mat4d s = Mat4d::Identity();
    s(0, 0) = scale;
    s(1, 1) = scale;
    s(2, 2) = scale;

    // 绕 Z 旋转，逆时针为正。
    const double rad = degToRad(rotationZDeg);
    Mat4d r = Mat4d::Identity();
    r(0, 0) = std::cos(rad);
    r(0, 1) = -std::sin(rad);
    r(1, 0) = std::sin(rad);
    r(1, 1) = std::cos(rad);

    Mat4d t = Mat4d::Identity();
    t(0, 3) = offset.x();
    t(1, 3) = offset.y();
    t(2, 3) = offset.z();

    return Transform(t * r * s);
}

Vec3d Transform::apply(const Vec3d &p) const {
    const Eigen::Vector4d h(p.x(), p.y(), p.z(), 1.0);
    const Eigen::Vector4d q = m_matrix * h;
    return Vec3d(q.x(), q.y(), q.z());
}

Transform Transform::compose(const Transform &other) const {
    return Transform(m_matrix * other.m_matrix);
}

Transform::Transform(const Mat4d &m) : m_matrix(m) {}

} // namespace geometry
