// PathExporter 单元测试（无第三方框架，失败返回非零）。
#include "io/PathExporter.h"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <sstream>
#include <string>
#include <vector>

namespace {

int g_failures = 0;

void check(bool cond, const std::string &msg) {
    if (cond) {
        std::cout << "[PASS] " << msg << "\n";
    } else {
        std::cerr << "[FAIL] " << msg << "\n";
        ++g_failures;
    }
}

path::PathSegment seg(double x0, double y0, double x1, double y1,
                      path::PathType type) {
    return {{x0, y0}, {x1, y1}, type};
}

// 统计 CSV 行数（按 '\n' 切分，忽略末尾空行）。
int countLines(const std::string &csv) {
    int n = 0;
    std::istringstream in(csv);
    std::string line;
    while (std::getline(in, line)) {
        if (!line.empty()) {
            ++n;
        }
    }
    return n;
}

} // namespace

int main() {
    // 1. 单段 Print → 2 行，坐标与 type 精确。
    {
        std::vector<path::PathSegment> segs = {
            seg(0, 0, 10, 20, path::PathType::Print)};
        std::string csv = io::exportPathCsv(segs, 0.2);
        std::string expected =
            "0,0.000,0.000,0.200,PRINT\n"
            "1,10.000,20.000,0.200,PRINT\n";
        check(csv == expected, "单段 Print 导出精确 CSV");
    }

    // 2. 多段 → 段数 + 1 行。
    {
        std::vector<path::PathSegment> segs = {
            seg(0, 0, 1, 0, path::PathType::Print),
            seg(1, 0, 1, 1, path::PathType::Print),
            seg(1, 1, 0, 1, path::PathType::Print)};
        check(countLines(io::exportPathCsv(segs, 0.2)) == 4, "3 段导出 4 行");
    }

    // 3. index 递增。
    {
        std::vector<path::PathSegment> segs = {
            seg(0, 0, 1, 0, path::PathType::Print),
            seg(1, 0, 2, 0, path::PathType::Print)};
        std::string csv = io::exportPathCsv(segs, 0.2);
        bool ok = csv.find("\n1,") != std::string::npos &&
                  csv.find("\n2,") != std::string::npos;
        check(ok, "index 递增（0/1/2）");
    }

    // 4. type 区分 PRINT / TRAVEL。
    {
        std::vector<path::PathSegment> segs = {
            seg(0, 0, 1, 0, path::PathType::Print),
            seg(1, 0, 2, 0, path::PathType::Travel)};
        std::string csv = io::exportPathCsv(segs, 0.2);
        check(csv.find("PRINT") != std::string::npos &&
                  csv.find("TRAVEL") != std::string::npos,
              "type 输出 PRINT / TRAVEL");
    }

    // 5. 空输入 → 空字符串。
    {
        check(io::exportPathCsv({}, 0.2).empty(), "空输入返回空字符串");
    }

    // 6. z 值（层高）正确应用到所有行。
    {
        std::vector<path::PathSegment> segs = {
            seg(0, 0, 1, 0, path::PathType::Print)};
        std::string csv = io::exportPathCsv(segs, 5.5);
        std::string expected =
            "0,0.000,0.000,5.500,PRINT\n"
            "1,1.000,0.000,5.500,PRINT\n";
        check(csv == expected, "z 值 5.5 应用到所有行");
    }

    // 7. 坐标固定 3 位小数。
    {
        std::vector<path::PathSegment> segs = {
            seg(1.23456, 2.5, 3.0, 4.75, path::PathType::Print)};
        std::string csv = io::exportPathCsv(segs, 0.2);
        check(csv.find("1.235,2.500") != std::string::npos &&
                  csv.find("3.000,4.750") != std::string::npos,
              "坐标固定 3 位小数");
    }

    // 8. 写文件成功且内容正确。
    {
        const auto p = std::filesystem::temp_directory_path() / "export_test.csv";
        std::vector<path::PathSegment> segs = {
            seg(0, 0, 1, 0, path::PathType::Print)};
        std::string err;
        bool ok = io::writePathCsv(p, segs, 0.2, &err);
        std::string content;
        {
            std::ifstream in(p);
            content.assign((std::istreambuf_iterator<char>(in)),
                           std::istreambuf_iterator<char>());
        }  // 关闭文件后再删除（Windows 下删除打开的文件会失败）。
        std::filesystem::remove(p);
        check(ok && content == io::exportPathCsv(segs, 0.2), "写文件成功且内容一致");
    }

    // 9. 写文件到无效路径 → 失败 + error。
    {
        std::string err;
        bool ok = io::writePathCsv(
            std::filesystem::path("Z:/no_such_dir_xyz/out.csv"),
            {seg(0, 0, 1, 0, path::PathType::Print)}, 0.2, &err);
        check(!ok && !err.empty(), "写文件失败返回 false + error");
    }

    // 10. G-code 单层：G0 Z 开头 + 定位首段起点 + Print→G1 / Travel→G0。
    {
        std::vector<path::PathSegment> segs = {
            seg(0, 0, 10, 0, path::PathType::Print),
            seg(10, 0, 10, 1, path::PathType::Travel),
            seg(10, 1, 0, 1, path::PathType::Print)};
        std::string g = io::exportPathGcode(segs, 0.25);
        std::string expected =
            "G0 Z0.250\n"
            "G0 X0.000 Y0.000\n"
            "G1 X10.000 Y0.000\n"
            "G0 X10.000 Y1.000\n"
            "G1 X0.000 Y1.000\n";
        check(g == expected, "G-code 单层精确输出（Z/定位/G0/G1）");
    }

    // 11. G-code 空输入 → 空字符串。
    {
        check(io::exportPathGcode({}, 0.2).empty(), "G-code 空输入返回空字符串");
    }

    // 12. 全层批量：层注释 + 各层 Z 提升；空层跳过不产生 Z 提升。
    {
        std::vector<std::vector<path::PathSegment>> layers = {
            {seg(0, 0, 1, 0, path::PathType::Print)},
            {},  // 空层
            {seg(0, 0, 2, 0, path::PathType::Print)}};
        std::vector<double> zs = {0.25, 0.5, 0.75};
        std::string g = io::exportAllLayersGcode(layers, zs);
        bool ok = g.find("; layers: 3") != std::string::npos &&
                  g.find("; ----- layer 0  z=0.250") != std::string::npos &&
                  g.find("G0 Z0.250") != std::string::npos &&
                  g.find("; ----- layer 2  z=0.750") != std::string::npos &&
                  g.find("G0 Z0.750") != std::string::npos &&
                  g.find("layer 1") == std::string::npos;  // 空层被跳过
        check(ok, "全层批量 G-code（注释 + Z 提升 + 空层跳过）");
    }

    // 13. 层数与 z 表不等长 → 空字符串 / 写文件失败。
    {
        std::vector<std::vector<path::PathSegment>> layers = {
            {seg(0, 0, 1, 0, path::PathType::Print)}};
        check(io::exportAllLayersGcode(layers, {0.25, 0.5}).empty(),
              "层数与 z 表不等长返回空");
        std::string err;
        check(!io::writeAllLayersGcode(
                  std::filesystem::temp_directory_path() / "x.gcode", layers,
                  {0.25, 0.5}, &err) &&
                  !err.empty(),
              "不等长写文件失败 + error");
    }

    std::cout << "\n" << (g_failures == 0 ? "ALL PASS" : "HAS FAILURES")
              << " (" << g_failures << " failures)\n";
    return g_failures == 0 ? 0 : 1;
}
