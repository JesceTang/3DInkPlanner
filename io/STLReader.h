#pragma once

#include <filesystem>
#include <string>

#include "geometry/GeometryTypes.h"

namespace io {

// Binary STL 读取结果。
struct StlReadResult {
    bool ok = false;
    geometry::Mesh mesh;
    std::string error;  // ok == false 时的失败原因
};

// 读取 Binary STL（80 字节头 + 4 字节三角数 + 每面片 50 字节）。
// 不依赖 Qt，仅用标准库 + Eigen。
StlReadResult readBinaryStl(const std::filesystem::path &path);

} // namespace io
