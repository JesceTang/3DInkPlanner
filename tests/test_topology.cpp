// MeshBuilder 拓扑构建单元测试（无第三方框架，失败返回非零）。
#include "topology/MeshBuilder.h"
#include "io/STLReader.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <string>
#include <unordered_map>
#include <utility>
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

bool near(double a, double b, double eps = 1e-4) {
    return std::fabs(a - b) < eps;
}

// 半边长 h 的立方体 triangle soup（12 面、36 个独立顶点、绕序朝外，体积 8h³）。
geometry::Mesh makeCubeSoup(float h) {
    using V = geometry::Vec3f;
    const V verts[8] = {
        {-h, -h, -h}, {h, -h, -h}, {h, h, -h}, {-h, h, -h},
        {-h, -h, h},  {h, -h, h},  {h, h, h},  {-h, h, h},
    };
    // 与 tools/generate_sample_stl.py 的 make_cube 相同的 quad 拆分。
    const int quads[6][4] = {
        {4, 5, 6, 7},  // +z
        {1, 0, 3, 2},  // -z
        {0, 1, 5, 4},  // -y
        {2, 3, 7, 6},  // +y
        {3, 0, 4, 7},  // -x
        {1, 2, 6, 5},  // +x
    };
    geometry::Mesh mesh;
    for (const auto &q : quads) {
        mesh.triangles.push_back({verts[q[0]], verts[q[1]], verts[q[2]], V::UnitZ()});
        mesh.triangles.push_back({verts[q[0]], verts[q[2]], verts[q[3]], V::UnitZ()});
    }
    mesh.computeBounds();
    return mesh;
}

// 校验：所有流形边在两个邻面中的有向出现相反（法向一致性的定义式检查）。
bool allManifoldEdgesOpposite(const topology::IndexedMesh &mesh) {
    struct Rec {
        std::vector<bool> dir;  // 有向出现是否为"小索引 → 大索引"
    };
    std::unordered_map<std::uint64_t, Rec> edges;
    for (const auto &f : mesh.faces) {
        for (int i = 0; i < 3; ++i) {
            const int u = f[static_cast<std::size_t>(i)];
            const int v = f[static_cast<std::size_t>((i + 1) % 3)];
            const std::uint64_t key =
                (std::uint64_t(static_cast<std::uint32_t>(std::min(u, v))) << 32) |
                std::uint64_t(static_cast<std::uint32_t>(std::max(u, v)));
            edges[key].dir.push_back(u < v);
        }
    }
    for (const auto &[key, rec] : edges) {
        if (rec.dir.size() == 2 && rec.dir[0] == rec.dir[1]) {
            return false;  // 同向出现 = 绕序不一致
        }
    }
    return true;
}

void flipFace(topology::IndexedMesh &mesh, int fi) {
    auto &f = mesh.faces[static_cast<std::size_t>(fi)];
    std::swap(f[1], f[2]);
}

} // namespace

int main() {
    const topology::MeshBuilder builder;
    constexpr double kTol = 1e-6;

    // 1. 立方体焊接：12 面 soup（36 独立顶点）→ 8 顶点 12 面 18 边全流形。
    {
        const auto im = builder.buildIndexedMesh(makeCubeSoup(1.0f), kTol);
        const auto rep = builder.analyze(im);
        check(im.vertices.size() == 8, "立方体焊接后 8 个唯一顶点");
        check(rep.faceCount == 12 && rep.edgeCount == 18, "立方体 12 面 18 边（欧拉 V-E+F=2）");
        check(rep.manifoldEdgeCount == 18 && rep.boundaryEdgeCount == 0 &&
                  rep.nonManifoldEdgeCount == 0,
              "立方体全流形、无边界边、无非流形边");
        check(rep.componentCount == 1, "立方体 1 个连通分量");
        check(near(rep.signedVolume, 8.0), "立方体有符号体积 == 8（朝外）");
        check(allManifoldEdgesOpposite(im), "立方体生成即法向一致");
    }

    // 2. 容差焊接：容差内合并 / 容差外保留。
    {
        geometry::Mesh soup;
        const float eps = 1e-7f;  // < kTol
        soup.triangles.push_back(
            {{0.f, 0.f, 0.f}, {1.f, 0.f, 0.f}, {0.f, 1.f, 0.f}, {0, 0, 1}});
        soup.triangles.push_back(
            {{eps, 0.f, 0.f}, {1.f, 0.f, 0.f}, {0.f, 0.f, 1.f}, {0, 0, 1}});
        soup.computeBounds();
        const auto im = builder.buildIndexedMesh(soup, kTol);
        check(im.vertices.size() == 4, "容差内顶点被焊接（5 点 → 4 点）");

        geometry::Mesh soup2;
        soup2.triangles.push_back(
            {{0.f, 0.f, 0.f}, {1.f, 0.f, 0.f}, {0.f, 1.f, 0.f}, {0, 0, 1}});
        soup2.triangles.push_back(
            {{1e-3f, 0.f, 0.f}, {1.f, 0.f, 0.f}, {0.f, 0.f, 1.f}, {0, 0, 1}});
        soup2.computeBounds();
        const auto im2 = builder.buildIndexedMesh(soup2, kTol);
        check(im2.vertices.size() == 5, "容差外顶点不合并（5 点保留）");
    }

    // 3. 开放平面（2 面）：4 边界边 + 1 流形边。
    {
        geometry::Mesh soup;
        soup.triangles.push_back(
            {{0.f, 0.f, 0.f}, {1.f, 0.f, 0.f}, {1.f, 1.f, 0.f}, {0, 0, 1}});
        soup.triangles.push_back(
            {{0.f, 0.f, 0.f}, {1.f, 1.f, 0.f}, {0.f, 1.f, 0.f}, {0, 0, 1}});
        soup.computeBounds();
        const auto rep = builder.analyze(builder.buildIndexedMesh(soup, kTol));
        check(rep.manifoldEdgeCount == 1 && rep.boundaryEdgeCount == 4 &&
                  rep.nonManifoldEdgeCount == 0 && rep.componentCount == 1,
              "开放平面：1 流形边 + 4 边界边 + 1 分量");
    }

    // 4. 非流形 T 型：三面共享同一边。
    {
        geometry::Mesh soup;
        soup.triangles.push_back(
            {{0.f, 0.f, 0.f}, {1.f, 0.f, 0.f}, {0.f, 1.f, 0.f}, {0, 0, 1}});
        soup.triangles.push_back(
            {{0.f, 0.f, 0.f}, {1.f, 0.f, 0.f}, {0.f, 0.f, 1.f}, {0, 0, 1}});
        soup.triangles.push_back(
            {{0.f, 0.f, 0.f}, {1.f, 0.f, 0.f}, {0.f, 1.f, 1.f}, {0, 0, 1}});
        soup.computeBounds();
        const auto rep = builder.analyze(builder.buildIndexedMesh(soup, kTol));
        check(rep.nonManifoldEdgeCount == 1 && rep.boundaryEdgeCount == 6,
              "三面共边被报告为 1 条非流形边（只报告不修复）");
    }

    // 5. 双立方体分离：2 连通分量。
    {
        geometry::Mesh soup = makeCubeSoup(1.0f);
        const geometry::Mesh shifted = [&] {
            geometry::Mesh m = makeCubeSoup(1.0f);
            for (auto &t : m.triangles) {
                const geometry::Vec3f off(10.f, 0.f, 0.f);
                t.v0 += off;
                t.v1 += off;
                t.v2 += off;
            }
            return m;
        }();
        soup.triangles.insert(soup.triangles.end(), shifted.triangles.begin(),
                              shifted.triangles.end());
        soup.computeBounds();
        const auto rep = builder.analyze(builder.buildIndexedMesh(soup, kTol));
        check(rep.componentCount == 2 && rep.vertexCount == 16 && rep.faceCount == 24,
              "分离双立方体：2 分量、16 顶点、24 面");
    }

    // 6. 法向错乱修复：翻转 3 个面 → 修复后一致且体积为正。
    {
        auto im = builder.buildIndexedMesh(makeCubeSoup(1.0f), kTol);
        flipFace(im, 0);
        flipFace(im, 5);
        flipFace(im, 7);
        check(!allManifoldEdgesOpposite(im), "手工翻转后检测到不一致");
        const int flipped = builder.makeNormalsConsistent(im);
        check(flipped > 0, "修复执行了翻转");
        check(allManifoldEdgesOpposite(im), "修复后所有流形边方向相反");
        check(near(im.signedVolume(), 8.0), "修复后有符号体积恢复 +8");
    }

    // 7. 已一致网格：修复应为 0 翻转。
    {
        auto im = builder.buildIndexedMesh(makeCubeSoup(1.0f), kTol);
        check(builder.makeNormalsConsistent(im) == 0, "一致网格修复 0 翻转");
    }

    // 8. 退化面：焊接后顶点坍缩的面被丢弃并计数。
    {
        geometry::Mesh soup;
        soup.triangles.push_back(
            {{0.f, 0.f, 0.f}, {0.f, 0.f, 0.f}, {1.f, 0.f, 0.f}, {0, 0, 1}});  // 两顶点相同
        soup.triangles.push_back(
            {{0.f, 0.f, 0.f}, {1.f, 0.f, 0.f}, {0.f, 1.f, 0.f}, {0, 0, 1}});
        soup.computeBounds();
        int skipped = -1;
        const auto im = builder.buildIndexedMesh(soup, kTol, &skipped);
        check(skipped == 1 && im.faces.size() == 1, "退化面被显式丢弃并计数");
    }

    // 9. 空输入不崩溃。
    {
        const geometry::Mesh empty;
        auto im = builder.buildIndexedMesh(empty, kTol);
        const auto rep = builder.analyze(im);
        check(im.empty() && rep.faceCount == 0 && rep.edgeCount == 0 &&
                  builder.makeNormalsConsistent(im) == 0,
              "空网格全链路安全返回");
    }

    // 10. 13 万面球体集成：欧拉公式强校验 + 法向修复 0 翻转 + 性能阈值。
    {
        const std::string path = std::string(SOURCE_DIR) + "/assets/models/sphere_r30_131k.stl";
        const auto read = io::readBinaryStl(path);
        check(read.ok, "读取 13 万面球体 STL");
        if (read.ok) {
            const auto t0 = std::chrono::steady_clock::now();
            auto im = builder.buildIndexedMesh(read.mesh, 1e-5);
            const auto rep = builder.analyze(im);
            const int flipped = builder.makeNormalsConsistent(im);
            const double ms = std::chrono::duration<double, std::milli>(
                                  std::chrono::steady_clock::now() - t0)
                                  .count();
            std::cout << "  [perf] 焊接+分析+修复 " << ms << " ms（" << rep.faceCount
                      << " 面）\n";
            check(im.vertices.size() == 65282 && rep.faceCount == 130560,
                  "球体焊接：65282 顶点 / 130560 面");
            check(rep.edgeCount == 195840 && rep.manifoldEdgeCount == 195840,
                  "球体全流形：E=V+F-2=195840（欧拉公式）");
            check(rep.boundaryEdgeCount == 0 && rep.nonManifoldEdgeCount == 0 &&
                      rep.componentCount == 1,
                  "球体无边界/非流形边、1 分量");
            check(near(rep.signedVolume, 113081.7, 1.0), "球体有符号体积 ≈ 113081.7");
            check(flipped == 0, "生成器输出的球体法向已一致（修复 0 翻转）");
            check(ms < 2000.0, "13 万面拓扑构建 < 2s");
        }
    }

    std::cout << "\n" << (g_failures == 0 ? "ALL PASS" : "HAS FAILURES") << " ("
              << g_failures << " failures)\n";
    return g_failures == 0 ? 0 : 1;
}
