#include "slicing/Slicer.h"

#include <algorithm>
#include <future>
#include <numeric>
#include <thread>

#include "slicing/ContourBuilder.h"
#include "slicing/SegmentConnector.h"
#include "slicing/TrianglePlaneIntersection.h"

namespace slicing {

namespace {

// 对候选三角形集合做单层切片：求交 + 拼接 + 共线简化。
// candidates 为 nullptr 时遍历全量三角形（单层接口 slice 的路径）。
// coplanarOut 非空时输出与层面共面（3 顶点全 On）被忽略的三角形数。
std::vector<geometry::Polyline> sliceCandidates(
    const std::vector<geometry::Triangle> &triangles,
    const std::vector<size_t> *candidates, double z, double tolerance,
    int *coplanarOut = nullptr) {
    if (coplanarOut) {
        *coplanarOut = 0;
    }
    std::vector<geometry::Segment2D> segments;
    segments.reserve(candidates ? candidates->size() : triangles.size());
    const auto collect = [&](size_t ti) {
        const auto &t = triangles[ti];
        // 共面检测：3 顶点都在层上（求交内部会忽略它们，这里显式计数用于告警）。
        if (coplanarOut && std::fabs(t.v0.z() - z) <= tolerance &&
            std::fabs(t.v1.z() - z) <= tolerance &&
            std::fabs(t.v2.z() - z) <= tolerance) {
            ++(*coplanarOut);
        }
        auto seg = intersectTrianglePlane(t, z, tolerance);
        if (seg) {
            segments.push_back(*seg);
        }
    };
    if (candidates) {
        for (const size_t ti : *candidates) {
            collect(ti);
        }
    } else {
        for (size_t ti = 0; ti < triangles.size(); ++ti) {
            collect(ti);
        }
    }

    SegmentConnector connector;
    auto polys = connector.connect(segments, tolerance);
    for (geometry::Polyline &poly : polys) {
        poly = simplifyContour(poly, tolerance);
    }
    return polys;
}

// 扫描线索引：zMin/zMax 包围盒 + 按 zMin 升序的面索引（主线程构建一次，
// 并行时各工作线程只读共享）。
struct SweepIndex {
    std::vector<float> zMin;
    std::vector<float> zMax;
    std::vector<size_t> order;
};

SweepIndex buildSweepIndex(const std::vector<geometry::Triangle> &tris) {
    SweepIndex idx;
    const size_t n = tris.size();
    idx.zMin.resize(n);
    idx.zMax.resize(n);
    idx.order.resize(n);
    std::iota(idx.order.begin(), idx.order.end(), 0);
    for (size_t i = 0; i < n; ++i) {
        const auto &t = tris[i];
        idx.zMin[i] = std::min({t.v0.z(), t.v1.z(), t.v2.z()});
        idx.zMax[i] = std::max({t.v0.z(), t.v1.z(), t.v2.z()});
    }
    std::sort(idx.order.begin(), idx.order.end(),
              [&](size_t a, size_t b) { return idx.zMin[a] < idx.zMin[b]; });
    return idx;
}

// 对层区间 [begin, end) 执行扫描线切片，结果写入 layers 对应槽位（调用方预分配）。
// 活动集从空建立：激活条件 zMin <= z+tol 对所有跨越当前层的面是必要条件，
// 因此任意起始层都能重建出与全程扫描完全一致的活动集（内容、顺序均相同，
// 代价是块首层一次 O(F) 激活推进）。这是并行分块确定性的依据。
void sweepRange(const std::vector<geometry::Triangle> &tris,
                const SweepIndex &idx,
                const std::vector<double> &zs, size_t begin, size_t end,
                double tolerance, std::vector<Layer> &layers) {
    const size_t n = tris.size();
    const float tol = static_cast<float>(tolerance);
    const ContourClassifier classifier;
    std::vector<size_t> active;
    size_t nextIn = 0;
    for (size_t li = begin; li < end; ++li) {
        const double zd = zs[li];
        const float z = static_cast<float>(zd);
        // 激活：zMin <= z + tol 的面进入活动集（层升序，激活指针不回退）。
        while (nextIn < n && idx.zMin[idx.order[nextIn]] <= z + tol) {
            active.push_back(idx.order[nextIn]);
            ++nextIn;
        }
        // 清理：zMax < z - tol 的面不再跨越当前及后续层（保序压缩）。
        size_t w = 0;
        for (size_t r = 0; r < active.size(); ++r) {
            if (idx.zMax[active[r]] >= z - tol) {
                active[w++] = active[r];
            }
        }
        active.resize(w);

        Layer layer;
        layer.z = zd;
        layer.contours = sliceCandidates(tris, &active, zd, tolerance,
                                         &layer.coplanarTriangleCount);
        layer.classified = classifier.classify(layer.contours, tolerance);
        // 共面面片显式告警：水平盖板被忽略，该层轮廓可能不完整（不完整优于错填）。
        if (layer.coplanarTriangleCount > 0) {
            layer.classified.warnings.push_back(
                "存在 " + std::to_string(layer.coplanarTriangleCount) +
                " 个与层面共面的三角形被忽略，水平面区域轮廓可能不完整");
        }
        layers[li] = std::move(layer);
    }
}

// 扫描线分层（Z-bucket 的扫描线形态）：层按 z 升序处理，三角形按 zMin 升序
// 排序后单趟激活，活动集只保留跨越当前层的面，复杂度 O((F+L)·k)，
// k = 平均活跃面数；替代朴素版 O(L·F) 的全量遍历。
// 容差约定：激活/清理各放宽一个 tolerance（贴层面由求交内部二次分类），不漏面。
// 并行：层均分连续块，每线程独立扫描线，结果写固定槽位——无锁、无共享写、
// 输出与串行逐点一致。层数过少时直接串行，避免线程开销超过收益。
// 注：曾实测原子计数器动态调度（小块抢夺），因每块从空重建扫描线的激活
// 成本（块首 O(F) 推进）超过负载均衡收益而更慢（球体 44.7ms vs 静态 28.8ms），
// 故保留静态均分；若未来单层耗时差异极大（如支撑结构），可再评估动态方案。
std::vector<Layer> sliceAllSweep(const geometry::Mesh &mesh,
                                 std::vector<double> zs,  // 调用前已过滤 + 升序
                                 double tolerance,
                                 unsigned int numThreads) {
    std::vector<Layer> layers;
    if (mesh.triangles.empty() || zs.empty()) {
        return layers;
    }

    const SweepIndex idx = buildSweepIndex(mesh.triangles);
    layers.resize(zs.size());

    unsigned int threads = numThreads;
    if (threads == 0) {
        threads = std::thread::hardware_concurrency();
    }
    if (threads == 0) {  // hardware_concurrency 可能返回 0（未知）
        threads = 1;
    }
    threads = std::min(threads, static_cast<unsigned int>(zs.size()));

    if (threads <= 1 || zs.size() < 8) {
        sweepRange(mesh.triangles, idx, zs, 0, zs.size(), tolerance, layers);
        return layers;
    }

    const size_t chunk = (zs.size() + threads - 1) / threads;
    std::vector<std::future<void>> futures;
    futures.reserve(threads);
    for (size_t begin = 0; begin < zs.size(); begin += chunk) {
        const size_t end = std::min(begin + chunk, zs.size());
        futures.push_back(std::async(
            std::launch::async, sweepRange,
            std::cref(mesh.triangles), std::cref(idx), std::cref(zs),
            begin, end, tolerance, std::ref(layers)));
    }
    // get() 传播工作线程异常（如 bad_alloc），不让切片静默缺层。
    for (auto &f : futures) {
        f.get();
    }
    return layers;
}

} // namespace

std::vector<geometry::Polyline> Slicer::slice(
    const geometry::Mesh &mesh, double z, double tolerance) const {
    return sliceCandidates(mesh.triangles, nullptr, z, tolerance);
}

Layer Slicer::sliceLayer(
    const geometry::Mesh &mesh, double z, double tolerance) const {
    Layer layer;
    layer.z = z;
    layer.contours = sliceCandidates(mesh.triangles, nullptr, z, tolerance,
                                     &layer.coplanarTriangleCount);

    // 内外环分类：孔洞识别（射线法）+ 断链告警汇总。
    const ContourClassifier classifier;
    layer.classified = classifier.classify(layer.contours, tolerance);
    if (layer.coplanarTriangleCount > 0) {
        layer.classified.warnings.push_back(
            "存在 " + std::to_string(layer.coplanarTriangleCount) +
            " 个与层面共面的三角形被忽略，水平面区域轮廓可能不完整");
    }
    return layer;
}

std::vector<Layer> Slicer::sliceAll(
    const geometry::Mesh &mesh, double layerHeight, double tolerance,
    unsigned int numThreads) const {
    std::vector<Layer> empty;
    if (mesh.triangles.empty() || layerHeight <= 0.0) {
        return empty;
    }

    const double zMin = mesh.minBound.z();
    const double zMax = mesh.maxBound.z();

    // 第一层偏移半个层高，避免切片面正好落在底面；后续每层递增 layerHeight。
    std::vector<double> zs;
    for (double z = zMin + layerHeight * 0.5; z < zMax; z += layerHeight) {
        zs.push_back(z);
    }
    return sliceAllSweep(mesh, std::move(zs), tolerance, numThreads);
}

std::vector<Layer> Slicer::sliceAll(
    const geometry::Mesh &mesh, const std::vector<double> &zList, double tolerance,
    unsigned int numThreads) const {
    std::vector<Layer> empty;
    if (mesh.triangles.empty() || zList.empty()) {
        return empty;
    }

    const double zMin = mesh.minBound.z();
    const double zMax = mesh.maxBound.z();
    std::vector<double> zs;
    zs.reserve(zList.size());
    for (const double z : zList) {
        // 区间外的 z 显式跳过（切片面贴底/顶面会与共面面片退化，故为严格开区间）。
        if (z <= zMin || z >= zMax) {
            continue;
        }
        zs.push_back(z);
    }
    // 扫描线要求升序；输出层统一按 z 升序（打印顺序自下而上）。
    std::sort(zs.begin(), zs.end());
    return sliceAllSweep(mesh, std::move(zs), tolerance, numThreads);
}

} // namespace slicing
