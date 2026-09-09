#pragma once

#include <filesystem>
#include <string>

#include "geometry/GeometryTypes.h"

namespace io {

// STL 读取结果（Binary / ASCII 共用）。
struct StlReadResult {
    bool ok = false;
    geometry::Mesh mesh;
    std::string error;  // ok == false 时的失败原因
};

// 读取 Binary STL（80 字节头 + 4 字节三角数 + 每面片 50 字节）。
// 严格校验文件长度布局；ASCII 文件在此返回失败（长度不匹配）。
// 不依赖 Qt，仅用标准库 + Eigen。
StlReadResult readBinaryStl(const std::filesystem::path &path);

// 读取 ASCII STL（solid / facet / vertex / endsolid 文本格式）。
// 法向字段忽略（由调用方按绕序重算）；facet 顶点数 != 3 视为格式错误。
StlReadResult readAsciiStl(const std::filesystem::path &path);

// 自动检测并读取 STL：文件长度匹配 binary 布局 → Binary；
// 否则文本以 "solid" 开头 → ASCII；都不满足返回失败。
StlReadResult readStl(const std::filesystem::path &path);

} // namespace io
