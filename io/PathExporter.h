#pragma once

#include <filesystem>
#include <string>
#include <vector>

#include "path/ToolPath.h"

// IO 层：路径导出。不依赖 Qt UI。

namespace io {

// 导出路径为 CSV 文本。
// 每行：index,x,y,z,type（type = PRINT / TRAVEL），z 为所有点的层高。
// 一个 PathSegment 贡献其 end 点（type 为该段类型），首点取第一段的 start。
std::string exportPathCsv(const std::vector<path::PathSegment> &segments, double z);

// 写 CSV 到文件。失败返回 false 并填充 errorOut。
bool writePathCsv(const std::filesystem::path &path,
                  const std::vector<path::PathSegment> &segments, double z,
                  std::string *errorOut);

// 导出路径为 G-code-like 文本。
// Print 段 → G1（喷印移动），Travel 段 → G0（空走移动）。
// 先 G0 定位到首段起点，再逐段输出 end 点；Z 用 G0 Z<z> 开头设置一次。
std::string exportPathGcode(const std::vector<path::PathSegment> &segments, double z);

// 导出全部层为单个 G-code-like 文件：层间插入注释行 + G0 Z 提升。
// layersSegments 与 layerZs 必须等长；不等长返回空字符串。
std::string exportAllLayersGcode(
    const std::vector<std::vector<path::PathSegment>> &layersSegments,
    const std::vector<double> &layerZs);

// 写 G-code 到文件。失败返回 false 并填充 errorOut。
bool writeAllLayersGcode(
    const std::filesystem::path &path,
    const std::vector<std::vector<path::PathSegment>> &layersSegments,
    const std::vector<double> &layerZs, std::string *errorOut);

} // namespace io
