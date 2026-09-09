// 管道性能基准（V2.0 基线/回归工具，不进 ctest，手动运行）。
//
// 用法:
//   bench_pipeline.exe <stl路径> [layerHeight=0.5] [spacing=1.0]
//
// 分阶段计时输出：
//   1. STL 解析（readBinaryStl）
//   2. 全层切片（求交 + 拼接 + 简化 + 内外环分类）
//   3. Raster 填充（逐层带孔多边形生成扫描路径）
//
// 仅编排调用核心库，不引入新算法；输出统计量用于核对结果合理性。

#include <chrono>
#include <cstdlib>
#include <iostream>
#include <string>
#include <thread>

#include "io/STLReader.h"
#include "path/PathOptimizer.h"
#include "slicing/Slicer.h"

namespace {

using Clock = std::chrono::steady_clock;

double elapsedMs(const Clock::time_point &t0) {
    return std::chrono::duration<double, std::milli>(Clock::now() - t0).count();
}

} // namespace

int main(int argc, char *argv[]) {
    if (argc < 2) {
        std::cerr << "用法: bench_pipeline.exe <stl路径> [layerHeight=0.5] [spacing=1.0]\n";
        return 1;
    }
    const std::string stlPath = argv[1];
    const double layerHeight = (argc > 2) ? std::atof(argv[2]) : 0.5;
    const double spacing = (argc > 3) ? std::atof(argv[3]) : 1.0;
    constexpr double kTol = 1e-6;

    std::cout << "== bench_pipeline ==\n模型: " << stlPath
              << "\n层厚: " << layerHeight << " mm, 填充间距: " << spacing << " mm\n\n";

    // 1. STL 解析（自动检测 Binary/ASCII）
    auto t0 = Clock::now();
    const auto read = io::readStl(stlPath);
    const double tRead = elapsedMs(t0);
    if (!read.ok) {
        std::cerr << "STL 读取失败: " << read.error << "\n";
        return 1;
    }
    const geometry::Mesh &mesh = read.mesh;
    const auto size = mesh.maxBound - mesh.minBound;
    std::cout << "[1] STL 解析: " << tRead << " ms\n"
              << "    三角形数: " << mesh.triangles.size()
              << ", 尺寸: " << size.x() << " x " << size.y() << " x " << size.z() << " mm\n\n";

    // 2. 全层切片（并行，默认 auto 线程；先跑一次串行基线用于对比加速比）
    slicing::Slicer slicer;
    const unsigned int hw = std::thread::hardware_concurrency();

    t0 = Clock::now();
    const auto layersSerial = slicer.sliceAll(mesh, layerHeight, kTol, 1);
    const double tSliceSerial = elapsedMs(t0);
    // 逐点一致性由 test_slicer 用例 16 覆盖；这里仅需串行计时基线。
    (void)layersSerial;

    t0 = Clock::now();
    const auto layers = slicer.sliceAll(mesh, layerHeight, kTol);
    const double tSlice = elapsedMs(t0);

    std::size_t totalContours = 0, closedContours = 0, totalPoints = 0;
    std::size_t totalPolygons = 0, totalHoles = 0, totalOpenChains = 0;
    for (const auto &layer : layers) {
        totalContours += layer.contours.size();
        for (const auto &c : layer.contours) {
            if (c.closed) {
                ++closedContours;
            }
            totalPoints += c.points.size();
        }
        totalPolygons += layer.classified.polygons.size();
        totalOpenChains += layer.classified.openChains.size();
        for (const auto &p : layer.classified.polygons) {
            totalHoles += p.holes.size();
        }
    }
    std::cout << "[2] 全层切片: " << tSlice << " ms（" << layers.size() << " 层, 平均 "
              << (layers.empty() ? 0.0 : tSlice / layers.size()) << " ms/层）\n"
              << "    并行: " << hw << " 线程（串行基线 " << tSliceSerial << " ms, 加速 "
              << (tSlice > 0.0 ? tSliceSerial / tSlice : 0.0) << "x）\n"
              << "    轮廓总数: " << totalContours << "（闭合 " << closedContours
              << " / 开放 " << (totalContours - closedContours) << "）"
              << ", 轮廓顶点总数: " << totalPoints << "\n"
              << "    分类结果: 多边形 " << totalPolygons << "（孔洞 " << totalHoles
              << "）, 断链 " << totalOpenChains << "\n\n";

    // 3. 路径生成（PathOptimizer：Raster + 岛间排序 + Travel 连接，与 GUI 同路径）
    t0 = Clock::now();
    path::PathOptimizer optimizer;
    std::size_t totalSegs = 0, totalTravel = 0;
    double totalPrintLen = 0.0, totalTravelLen = 0.0;
    for (const auto &layer : layers) {
        auto opt = optimizer.optimize(layer.classified.polygons, spacing);
        totalSegs += opt.segments.size();
        totalPrintLen += opt.printLength;
        totalTravelLen += opt.travelLength;
        for (const auto &s : opt.segments) {
            if (s.type == path::PathType::Travel) {
                ++totalTravel;
            }
        }
    }
    const double tFill = elapsedMs(t0);
    std::cout << "[3] 路径生成: " << tFill << " ms\n"
              << "    路径段总数: " << totalSegs << "（Print " << (totalSegs - totalTravel)
              << " / Travel " << totalTravel << "）\n"
              << "    喷印长度: " << totalPrintLen << " mm, 空走长度: " << totalTravelLen
              << " mm\n\n";

    std::cout << "总耗时: " << (tRead + tSlice + tFill) << " ms\n";
    return 0;
}
