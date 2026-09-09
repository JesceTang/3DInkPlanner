// Binary STL Reader 单元测试（无第三方框架，失败返回非零）。
#include "io/STLReader.h"

#include <array>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
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

bool near(float a, float b, float eps = 1e-4f) {
    return std::fabs(a - b) < eps;
}

// 写临时 Binary STL：header(80) + count(4) + 每个三角形 50 字节。
// declaredCount 写入文件头；bodyTris 为实际写入的三角形体。
std::filesystem::path writeStl(const std::string &name,
                               std::uint32_t declaredCount,
                               const std::vector<std::array<float, 12>> &bodyTris) {
    const auto path = std::filesystem::temp_directory_path() / name;
    std::ofstream out(path, std::ios::binary);
    std::vector<char> header(80, 0);
    out.write(header.data(), 80);
    out.write(reinterpret_cast<const char *>(&declaredCount), 4);
    for (const auto &t : bodyTris) {
        out.write(reinterpret_cast<const char *>(t.data()), 48);
        std::uint16_t attr = 0;
        out.write(reinterpret_cast<const char *>(&attr), 2);
    }
    return path;
}

// 构造一个 XY 平面三角形（z=0），normal(0,0,1)。
std::array<float, 12> tri(float x0, float y0, float x1, float y1, float x2, float y2) {
    return {0.f, 0.f, 1.f, x0, y0, 0.f, x1, y1, 0.f, x2, y2, 0.f};
}

} // namespace

int main() {
    // 1. 正常：单个三角形 + 顶点坐标。
    {
        auto p = writeStl("one.stl", 1, {tri(0.f, 0.f, 1.f, 0.f, 0.f, 1.f)});
        auto r = io::readBinaryStl(p);
        check(r.ok, "单三角形读取成功");
        check(r.mesh.triangles.size() == 1, "三角形数量 == 1");
        check(near(r.mesh.triangles[0].v0.x(), 0.f) &&
                  near(r.mesh.triangles[0].v2.y(), 1.f),
              "顶点坐标正确");
        std::filesystem::remove(p);
    }

    // 2. 正常：包围盒 min/max。
    {
        auto p = writeStl("bounds.stl", 2,
                          {tri(0.f, 0.f, 2.f, 0.f, 0.f, 2.f),
                           tri(10.f, 20.f, 12.f, 20.f, 10.f, 22.f)});
        auto r = io::readBinaryStl(p);
        check(r.ok, "两三角形读取成功");
        check(near(r.mesh.minBound.x(), 0.f) && near(r.mesh.maxBound.x(), 12.f) &&
                  near(r.mesh.minBound.y(), 0.f) && near(r.mesh.maxBound.y(), 22.f),
              "包围盒 min/max 正确");
        std::filesystem::remove(p);
    }

    // 3. 空网格：count == 0。
    {
        auto p = writeStl("empty.stl", 0, {});
        auto r = io::readBinaryStl(p);
        check(r.ok, "count=0 读取成功（空网格合法）");
        check(r.mesh.triangles.empty(), "空网格无三角形");
        std::filesystem::remove(p);
    }

    // 4. 文件不存在。
    {
        auto r = io::readBinaryStl(std::filesystem::path("no_such_file_xyz.stl"));
        check(!r.ok && !r.error.empty(), "文件不存在返回失败 + 错误信息");
    }

    // 5. 文件过短（仅 80 字节头，无 count）。
    {
        const auto p = std::filesystem::temp_directory_path() / "short.stl";
        {
            std::ofstream out(p, std::ios::binary);
            std::vector<char> header(80, 0);
            out.write(header.data(), 80);
        }
        auto r = io::readBinaryStl(p);
        check(!r.ok, "文件过短返回失败");
        std::filesystem::remove(p);
    }

    // 6. 声明 count 大于实际内容（防越界）。
    {
        auto p = writeStl("mismatch.stl", 10, {tri(0.f, 0.f, 1.f, 0.f, 0.f, 1.f)});
        auto r = io::readBinaryStl(p);
        check(!r.ok, "声明数量与文件长度不符返回失败");
        std::filesystem::remove(p);
    }

    // 7. 恶意超大 count（防内存爆炸）。
    {
        auto p = writeStl("huge.stl", 100000001u, {});
        auto r = io::readBinaryStl(p);
        check(!r.ok, "异常大 count 返回失败");
        std::filesystem::remove(p);
    }

    // 8. ASCII 文件 readBinaryStl 严格拒绝（长度布局不匹配）；readStl 自动检测成功。
    {
        const auto p = std::filesystem::temp_directory_path() / "ascii.stl";
        {
            std::ofstream out(p);
            out << "solid test\n"
                << "  facet normal 0 0 0\n"
                << "    outer loop\n"
                << "      vertex 0 0 0\n"
                << "      vertex 1 0 0\n"
                << "      vertex 0 1 0\n"
                << "    endloop\n"
                << "  endfacet\n"
                << "endsolid test\n";
        }
        auto rb = io::readBinaryStl(p);
        check(!rb.ok, "ASCII 文件 readBinaryStl 返回失败（严格 binary）");
        auto ra = io::readStl(p);
        check(ra.ok && ra.mesh.triangles.size() == 1,
              "readStl 自动检测 ASCII 成功（1 三角形）");
        if (ra.ok && ra.mesh.triangles.size() == 1) {
            const auto &t = ra.mesh.triangles[0];
            check(near(t.v1.x(), 1.f) && near(t.v2.y(), 1.f) &&
                      near(t.normal.z(), 1.f),
                  "ASCII 顶点与重算法向正确");
        }
        std::filesystem::remove(p);
    }

    // 10. ASCII 科学计数法坐标 + 多 facet。
    {
        const auto p = std::filesystem::temp_directory_path() / "ascii_sci.stl";
        {
            std::ofstream out(p);
            out << "solid sci\n"
                << "  facet normal 0 0 1\n"
                << "    outer loop\n"
                << "      vertex 0e0 0 0\n"
                << "      vertex 1.5e1 0 0\n"
                << "      vertex 0 2.5E1 0\n"
                << "    endloop\n"
                << "  endfacet\n"
                << "  facet normal 0 0 1\n"
                << "    outer loop\n"
                << "      vertex 0 0 5\n"
                << "      vertex 10 0 5\n"
                << "      vertex 0 10 5\n"
                << "    endloop\n"
                << "  endfacet\n"
                << "endsolid sci\n";
        }
        auto r = io::readStl(p);
        check(r.ok && r.mesh.triangles.size() == 2, "ASCII 科学计数法 2 面片读取成功");
        if (r.ok && r.mesh.triangles.size() == 2) {
            check(near(r.mesh.triangles[0].v1.x(), 15.f) &&
                      near(r.mesh.triangles[0].v2.y(), 25.f),
                  "科学计数法坐标值正确（15 / 25）");
        }
        std::filesystem::remove(p);
    }

    // 11. ASCII 损坏（facet 只有 2 顶点）→ 失败 + error。
    {
        const auto p = std::filesystem::temp_directory_path() / "ascii_bad.stl";
        {
            std::ofstream out(p);
            out << "solid bad\n"
                << "  facet normal 0 0 0\n"
                << "    outer loop\n"
                << "      vertex 0 0 0\n"
                << "      vertex 1 0 0\n"
                << "    endloop\n"
                << "  endfacet\n"
                << "endsolid bad\n";
        }
        auto r = io::readAsciiStl(p);
        check(!r.ok && !r.error.empty(), "ASCII 顶点数 != 3 返回失败 + error");
        std::filesystem::remove(p);
    }

    // 12. readStl 对 binary 文件仍走 binary 路径。
    {
        auto p = writeStl("binary_detect.stl", 1, {tri(0.f, 0.f, 1.f, 0.f, 0.f, 1.f)});
        auto r = io::readStl(p);
        check(r.ok && r.mesh.triangles.size() == 1,
              "readStl 对 binary 文件解析成功");
        std::filesystem::remove(p);
    }

    // 9. 法向重算：文件法向字段反向/为零时，以顶点绕序重算的归一化法向为准。
    {
        // 顶点 (0,0,0),(1,0,0),(0,1,0) → 几何法向 (0,0,1)；文件字段写反向 (0,0,-1)。
        std::array<float, 12> badNormal = {0.f, 0.f, -1.f,
                                           0.f, 0.f, 0.f,
                                           1.f, 0.f, 0.f,
                                           0.f, 1.f, 0.f};
        auto p = writeStl("badnormal.stl", 1, {badNormal});
        auto r = io::readBinaryStl(p);
        check(r.ok && r.mesh.triangles.size() == 1, "反向法向文件读取成功");
        if (r.ok && r.mesh.triangles.size() == 1) {
            const auto &n = r.mesh.triangles[0].normal;
            check(near(n.x(), 0.f) && near(n.y(), 0.f) && near(n.z(), 1.f),
                  "法向以绕序重算为准（(0,0,-1) 校正为 (0,0,1)）");
        }
        std::filesystem::remove(p);
    }

    std::cout << "\n" << (g_failures == 0 ? "ALL PASS" : "HAS FAILURES")
              << " (" << g_failures << " failures)\n";
    return g_failures == 0 ? 0 : 1;
}
