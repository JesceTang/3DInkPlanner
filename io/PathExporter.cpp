#include "io/PathExporter.h"

#include <fstream>
#include <iomanip>
#include <sstream>

namespace io {

namespace {

const char *typeToString(path::PathType t) {
    switch (t) {
    case path::PathType::Print:
        return "PRINT";
    case path::PathType::Travel:
        return "TRAVEL";
    }
    return "UNKNOWN";
}

} // namespace

std::string exportPathCsv(const std::vector<path::PathSegment> &segments, double z) {
    if (segments.empty()) {
        return {};
    }

    std::ostringstream oss;
    oss << std::fixed << std::setprecision(3);

    // 首点：第一段的 start。
    const path::PathSegment &first = segments.front();
    oss << 0 << ',' << first.start.x << ',' << first.start.y << ',' << z << ','
        << typeToString(first.type) << '\n';

    // 每段 end 点。
    for (std::size_t i = 0; i < segments.size(); ++i) {
        const path::PathSegment &s = segments[i];
        oss << (i + 1) << ',' << s.end.x << ',' << s.end.y << ',' << z << ','
            << typeToString(s.type) << '\n';
    }
    return oss.str();
}

bool writePathCsv(const std::filesystem::path &path,
                  const std::vector<path::PathSegment> &segments, double z,
                  std::string *errorOut) {
    std::ofstream out(path);
    if (!out) {
        if (errorOut) {
            *errorOut = "无法打开文件: " + path.string();
        }
        return false;
    }
    out << exportPathCsv(segments, z);
    return true;
}

std::string exportPathGcode(const std::vector<path::PathSegment> &segments, double z) {
    if (segments.empty()) {
        return {};
    }

    std::ostringstream oss;
    oss << std::fixed << std::setprecision(3);

    // 层 Z 设定 + 定位到首段起点（空走）。
    oss << "G0 Z" << z << '\n';
    const path::PathSegment &first = segments.front();
    oss << "G0 X" << first.start.x << " Y" << first.start.y << '\n';

    // 逐段输出 end 点：Print → G1，Travel → G0。
    for (const auto &s : segments) {
        oss << (s.type == path::PathType::Print ? "G1 X" : "G0 X") << s.end.x
            << " Y" << s.end.y << '\n';
    }
    return oss.str();
}

std::string exportAllLayersGcode(
    const std::vector<std::vector<path::PathSegment>> &layersSegments,
    const std::vector<double> &layerZs) {
    if (layersSegments.size() != layerZs.size()) {
        return {};
    }

    std::ostringstream oss;
    oss << "; 3DInkPlanner G-code-like export\n; layers: " << layerZs.size() << '\n';
    for (std::size_t i = 0; i < layerZs.size(); ++i) {
        if (layersSegments[i].empty()) {
            continue;  // 无路径层跳过（不产生 Z 提升）
        }
        oss << std::fixed << std::setprecision(3)
            << "; ----- layer " << i << "  z=" << layerZs[i] << " -----\n";
        oss << exportPathGcode(layersSegments[i], layerZs[i]);
    }
    return oss.str();
}

bool writeAllLayersGcode(
    const std::filesystem::path &path,
    const std::vector<std::vector<path::PathSegment>> &layersSegments,
    const std::vector<double> &layerZs, std::string *errorOut) {
    if (layersSegments.size() != layerZs.size()) {
        if (errorOut) {
            *errorOut = "层数与 z 表长度不一致";
        }
        return false;
    }
    std::ofstream out(path);
    if (!out) {
        if (errorOut) {
            *errorOut = "无法打开文件: " + path.string();
        }
        return false;
    }
    out << exportAllLayersGcode(layersSegments, layerZs);
    return true;
}

} // namespace io
