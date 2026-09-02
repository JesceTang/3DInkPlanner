#pragma once

#include <filesystem>
#include <string>
#include <vector>

#include "path/ToolPath.h"

// IO 层：路径导出。不依赖 Qt UI。

namespace io {

// 导出路径为 CSV 文本（指导 §4.1 J）。
// 每行：index,x,y,z,type（type = PRINT / TRAVEL），z 为所有点的层高。
// 一个 PathSegment 贡献其 end 点（type 为该段类型），首点取第一段的 start。
std::string exportPathCsv(const std::vector<path::PathSegment> &segments, double z);

// 写 CSV 到文件。失败返回 false 并填充 errorOut。
bool writePathCsv(const std::filesystem::path &path,
                  const std::vector<path::PathSegment> &segments, double z,
                  std::string *errorOut);

} // namespace io
