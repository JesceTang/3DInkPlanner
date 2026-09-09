#include "topology/MeshBuilder.h"

#include <cmath>
#include <cstdint>
#include <unordered_map>
#include <utility>
#include <vector>

namespace topology {

namespace {

// ---- 顶点焊接：量化坐标空间哈希 ----

// 量化栅格键：floor(coord / tol)，同容差内的点必落在同一格或相邻格。
struct GridKey {
    long long x = 0;
    long long y = 0;
    long long z = 0;
    bool operator==(const GridKey &o) const noexcept {
        return x == o.x && y == o.y && z == o.z;
    }
};

struct GridKeyHash {
    std::size_t operator()(const GridKey &k) const noexcept {
        std::size_t h = std::hash<long long>{}(k.x);
        h = h * 1000003u ^ std::hash<long long>{}(k.y);
        h = h * 1000003u ^ std::hash<long long>{}(k.z);
        return h;
    }
};

// ---- 边表：无向边 → 邻接面列表 ----

// 无向边键：小索引高 32 位，大索引低 32 位。
std::uint64_t edgeKey(int a, int b) noexcept {
    if (a > b) {
        std::swap(a, b);
    }
    return (std::uint64_t(static_cast<std::uint32_t>(a)) << 32) |
           std::uint64_t(static_cast<std::uint32_t>(b));
}

struct EdgeRecord {
    std::vector<int> faces;  // 共享该边的面索引（流形边恰好 2 个）
};

using EdgeTable = std::unordered_map<std::uint64_t, EdgeRecord>;

EdgeTable buildEdgeTable(const IndexedMesh &mesh) {
    EdgeTable table;
    table.reserve(mesh.faces.size() * 3 / 2 + 1);
    for (int fi = 0; fi < static_cast<int>(mesh.faces.size()); ++fi) {
        const auto &f = mesh.faces[static_cast<std::size_t>(fi)];
        table[edgeKey(f[0], f[1])].faces.push_back(fi);
        table[edgeKey(f[1], f[2])].faces.push_back(fi);
        table[edgeKey(f[2], f[0])].faces.push_back(fi);
    }
    return table;
}

} // namespace

IndexedMesh MeshBuilder::buildIndexedMesh(const geometry::Mesh &soup, double weldTolerance,
                                          int *degenerateSkipped) const {
    IndexedMesh out;
    if (degenerateSkipped != nullptr) {
        *degenerateSkipped = 0;
    }
    if (soup.triangles.empty() || weldTolerance <= 0.0) {
        return out;
    }

    const double tolSq = weldTolerance * weldTolerance;
    const double invTol = 1.0 / weldTolerance;

    // 栅格 → 落在栅格内的候选顶点索引
    std::unordered_map<GridKey, std::vector<int>, GridKeyHash> grid;
    grid.reserve(soup.triangles.size());

    // 焊接单个顶点：27 邻域内找容差内最近点，找不到则新建。
    auto weldPoint = [&](const geometry::Vec3f &p) -> int {
        const double px = p.x();
        const double py = p.y();
        const double pz = p.z();
        const long long ix = static_cast<long long>(std::floor(px * invTol));
        const long long iy = static_cast<long long>(std::floor(py * invTol));
        const long long iz = static_cast<long long>(std::floor(pz * invTol));

        int best = -1;
        double bestSq = tolSq;  // 只接受容差内的点
        for (long long dx = -1; dx <= 1; ++dx) {
            for (long long dy = -1; dy <= 1; ++dy) {
                for (long long dz = -1; dz <= 1; ++dz) {
                    const auto it = grid.find(GridKey{ix + dx, iy + dy, iz + dz});
                    if (it == grid.end()) {
                        continue;
                    }
                    for (const int vi : it->second) {
                        const auto &q = out.vertices[static_cast<std::size_t>(vi)];
                        const double ddx = static_cast<double>(q.x()) - px;
                        const double ddy = static_cast<double>(q.y()) - py;
                        const double ddz = static_cast<double>(q.z()) - pz;
                        const double d2 = ddx * ddx + ddy * ddy + ddz * ddz;
                        if (d2 <= bestSq) {
                            bestSq = d2;
                            best = vi;
                        }
                    }
                }
            }
        }
        if (best >= 0) {
            return best;
        }
        const int idx = static_cast<int>(out.vertices.size());
        out.vertices.push_back(p);
        grid[GridKey{ix, iy, iz}].push_back(idx);
        return idx;
    };

    out.vertices.reserve(soup.triangles.size());      // 焊接后只少不多
    out.faces.reserve(soup.triangles.size());
    int skipped = 0;
    for (const auto &t : soup.triangles) {
        const int a = weldPoint(t.v0);
        const int b = weldPoint(t.v1);
        const int c = weldPoint(t.v2);
        // 焊接后顶点坍缩的退化面：丢弃并计数（显式处理，不静默）。
        if (a == b || b == c || a == c) {
            ++skipped;
            continue;
        }
        out.faces.push_back({a, b, c});
    }
    if (degenerateSkipped != nullptr) {
        *degenerateSkipped = skipped;
    }
    return out;
}

TopologyReport MeshBuilder::analyze(const IndexedMesh &mesh) const {
    TopologyReport rep;
    rep.vertexCount = static_cast<int>(mesh.vertices.size());
    rep.faceCount = static_cast<int>(mesh.faces.size());
    if (mesh.empty()) {
        return rep;
    }

    // 残留退化面统计（正常为 0；手工构造的索引网格可能仍有）。
    for (const auto &f : mesh.faces) {
        if (f[0] == f[1] || f[1] == f[2] || f[0] == f[2]) {
            ++rep.degenerateFaceCount;
        }
    }

    const EdgeTable edges = buildEdgeTable(mesh);
    rep.edgeCount = static_cast<int>(edges.size());

    // 面邻接表（任何共享边都构成连通，供连通分量统计）。
    std::vector<std::vector<int>> adj(mesh.faces.size());
    for (const auto &[key, rec] : edges) {
        const int cnt = static_cast<int>(rec.faces.size());
        if (cnt == 1) {
            ++rep.boundaryEdgeCount;
        } else if (cnt == 2) {
            ++rep.manifoldEdgeCount;
        } else {
            ++rep.nonManifoldEdgeCount;
        }
        for (int i = 0; i < cnt; ++i) {
            for (int j = i + 1; j < cnt; ++j) {
                adj[static_cast<std::size_t>(rec.faces[i])].push_back(rec.faces[j]);
                adj[static_cast<std::size_t>(rec.faces[j])].push_back(rec.faces[i]);
            }
        }
    }

    // 连通分量：迭代 DFS（避免大网格递归爆栈）。
    std::vector<char> visited(mesh.faces.size(), 0);
    std::vector<int> stack;
    for (int seed = 0; seed < static_cast<int>(mesh.faces.size()); ++seed) {
        if (visited[static_cast<std::size_t>(seed)]) {
            continue;
        }
        ++rep.componentCount;
        visited[static_cast<std::size_t>(seed)] = 1;
        stack.push_back(seed);
        while (!stack.empty()) {
            const int cur = stack.back();
            stack.pop_back();
            for (const int nb : adj[static_cast<std::size_t>(cur)]) {
                if (!visited[static_cast<std::size_t>(nb)]) {
                    visited[static_cast<std::size_t>(nb)] = 1;
                    stack.push_back(nb);
                }
            }
        }
    }

    rep.signedVolume = mesh.signedVolume();
    return rep;
}

int MeshBuilder::makeNormalsConsistent(IndexedMesh &mesh) const {
    if (mesh.empty()) {
        return 0;
    }
    const EdgeTable edges = buildEdgeTable(mesh);
    const int nf = static_cast<int>(mesh.faces.size());

    // 面邻接：流形边参与翻转传播；非流形边只连通分量（无法定义一致朝向）。
    struct Adj {
        int face;
        bool manifold;
    };
    std::vector<std::vector<Adj>> adj(static_cast<std::size_t>(nf));
    for (const auto &[key, rec] : edges) {
        const bool manifold = (rec.faces.size() == 2);
        for (std::size_t i = 0; i < rec.faces.size(); ++i) {
            for (std::size_t j = i + 1; j < rec.faces.size(); ++j) {
                adj[static_cast<std::size_t>(rec.faces[i])].push_back({rec.faces[j], manifold});
                adj[static_cast<std::size_t>(rec.faces[j])].push_back({rec.faces[i], manifold});
            }
        }
    }

    // 迭代 DFS：分配连通分量 + 翻转标志。
    // 一致性不变量：共享边在两个面中的有向出现必须相反（考虑翻转后）。
    //   最终方向 = 原始方向 ^ flip，要求 dCur ^ flipCur != dNext ^ flipNext
    //   => flipNext = flipCur ^ (dCur == dNext)
    std::vector<int> comp(static_cast<std::size_t>(nf), -1);
    std::vector<char> flip(static_cast<std::size_t>(nf), 0);
    int nComp = 0;
    std::vector<int> stack;
    for (int seed = 0; seed < nf; ++seed) {
        if (comp[static_cast<std::size_t>(seed)] >= 0) {
            continue;
        }
        comp[static_cast<std::size_t>(seed)] = nComp;
        stack.push_back(seed);
        while (!stack.empty()) {
            const int cur = stack.back();
            stack.pop_back();
            const auto &fCur = mesh.faces[static_cast<std::size_t>(cur)];
            for (const Adj &a : adj[static_cast<std::size_t>(cur)]) {
                if (comp[static_cast<std::size_t>(a.face)] >= 0) {
                    continue;  // 已访问：若存在一致性冲突（不可定向面片），保持现状不强行修复
                }
                comp[static_cast<std::size_t>(a.face)] = nComp;
                char want = flip[static_cast<std::size_t>(cur)];
                if (a.manifold) {
                    // 找到 cur 与邻面的公共边，比较两者有向出现。
                    const auto &fNb = mesh.faces[static_cast<std::size_t>(a.face)];
                    bool sameDir = false;
                    for (int i = 0; i < 3; ++i) {
                        const int u = fCur[static_cast<std::size_t>(i)];
                        const int v = fCur[static_cast<std::size_t>((i + 1) % 3)];
                        // 该边若在邻面中也出现，比较方向。
                        bool inNb = false;
                        for (int k = 0; k < 3; ++k) {
                            const int p = fNb[static_cast<std::size_t>(k)];
                            const int q = fNb[static_cast<std::size_t>((k + 1) % 3)];
                            if ((p == u && q == v) || (p == v && q == u)) {
                                inNb = true;
                                sameDir = (p == u);  // 邻面同向 = 同为 u→v
                                break;
                            }
                        }
                        if (inNb) {
                            break;
                        }
                    }
                    if (sameDir) {
                        want ^= 1;
                    }
                }
                flip[static_cast<std::size_t>(a.face)] = want;
                stack.push_back(a.face);
            }
        }
        ++nComp;
    }

    // 按连通分量计算“应用 BFS flip 后”的有符号体积，负体积分量整体翻转（保证整体朝外）。
    // 注意：BFS 给出的 flip 只满足相对一致性，整体朝向有正反两解；这里必须基于翻转后
    // 的实际状态求体积（面贡献随 flip 变号），否则用原始体积会把朝向判反。
    std::vector<double> compVol(static_cast<std::size_t>(nComp), 0.0);
    for (int fi = 0; fi < nf; ++fi) {
        const auto &f = mesh.faces[static_cast<std::size_t>(fi)];
        const auto &a = mesh.vertices[static_cast<std::size_t>(f[0])];
        const auto &b = mesh.vertices[static_cast<std::size_t>(f[1])];
        const auto &c = mesh.vertices[static_cast<std::size_t>(f[2])];
        const double v = static_cast<double>(a.x()) *
                             (static_cast<double>(b.y()) * c.z() - static_cast<double>(b.z()) * c.y()) -
                         static_cast<double>(a.y()) *
                             (static_cast<double>(b.x()) * c.z() - static_cast<double>(b.z()) * c.x()) +
                         static_cast<double>(a.z()) *
                             (static_cast<double>(b.x()) * c.y() - static_cast<double>(b.y()) * c.x());
        // flip 后面绕序反转，体积贡献变号。
        const double signedContrib = flip[static_cast<std::size_t>(fi)] ? -v : v;
        compVol[static_cast<std::size_t>(comp[static_cast<std::size_t>(fi)])] += signedContrib / 6.0;
    }
    for (int fi = 0; fi < nf; ++fi) {
        if (compVol[static_cast<std::size_t>(comp[static_cast<std::size_t>(fi)])] < 0.0) {
            flip[static_cast<std::size_t>(fi)] ^= 1;
        }
    }

    // 应用翻转（交换后两个顶点索引即反转绕序）。
    int flipped = 0;
    for (int fi = 0; fi < nf; ++fi) {
        if (flip[static_cast<std::size_t>(fi)]) {
            auto &f = mesh.faces[static_cast<std::size_t>(fi)];
            std::swap(f[1], f[2]);
            ++flipped;
        }
    }
    return flipped;
}

} // namespace topology
