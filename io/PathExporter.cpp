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

} // namespace io
