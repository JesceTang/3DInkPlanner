// 集成测试：读取示例 STL 资产，验证多轮廓切片 + 路径生成。
#include "io/STLReader.h"
#include "path/RasterFillGenerator.h"
#include "slicing/Slicer.h"

#include <filesystem>
#include <iostream>
#include <string>

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

} // namespace

int main() {
    const std::filesystem::path model =
        std::filesystem::path(SOURCE_DIR) / "assets/models/box_with_hole.stl";

    // 1. 示例资产可读且三角形数正确（32 = 16 面 × 2）。
    const io::StlReadResult r = io::readBinaryStl(model);
    check(r.ok, "box_with_hole.stl 可读");
    check(r.mesh.triangles.size() == 32, "box_with_hole.stl 含 32 三角形");

    // 2. 切片：高 10 mm，layerHeight=2 → 5 层，每层 2 个闭合轮廓（外壁 + 内孔）。
    slicing::Slicer slicer;
    const auto layers = slicer.sliceAll(r.mesh, 2.0, 1e-6);
    check(layers.size() == 5, "box_with_hole 切片得到 5 层");

    bool twoClosed = !layers.empty();
    for (const auto &layer : layers) {
        if (layer.contours.size() != 2) {
            twoClosed = false;
        }
        for (const auto &c : layer.contours) {
            if (!c.closed) {
                twoClosed = false;
            }
        }
    }
    check(twoClosed, "每层 2 个闭合轮廓（外壁 + 内孔）");

    // 3. 路径生成：首层外/内轮廓均产生非空扫描填充路径。
    if (!layers.empty()) {
        path::RasterFillGenerator raster;
        std::size_t total = 0;
        for (const auto &c : layers.front().contours) {
            if (c.closed && c.points.size() >= 3) {
                geometry::Polygon poly;
                poly.vertices = c.points;
                total += raster.generate(poly, 2.0).size();
            }
        }
        check(total > 0, "首层生成非空扫描路径");
    }

    std::cout << "\n" << (g_failures == 0 ? "ALL PASS" : "HAS FAILURES")
              << " (" << g_failures << " failures)\n";
    return g_failures == 0 ? 0 : 1;
}
