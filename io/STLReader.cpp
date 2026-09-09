#include "io/STLReader.h"

#include <Eigen/Geometry>

#include <array>
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

namespace io {

namespace {

constexpr std::size_t kHeaderSize = 80;
constexpr std::size_t kCountSize = 4;
constexpr std::size_t kTriangleSize = 50;                 // normal(12) + 3 顶点(36) + attr(2)
constexpr std::uint32_t kMaxTriangles = 100'000'000;      // 防止恶意 count 导致内存爆炸

// 由 3 顶点构建三角形（法向按绕序重算，与 binary 路径同一约定）。
geometry::Triangle makeTriangle(const std::vector<geometry::Vec3f> &verts) {
    geometry::Triangle t;
    t.v0 = verts[0];
    t.v1 = verts[1];
    t.v2 = verts[2];
    const geometry::Vec3f geo = (t.v1 - t.v0).cross(t.v2 - t.v0);
    const float len = geo.norm();
    t.normal = (len > 1e-20f) ? geometry::Vec3f(geo / len)
                              : geometry::Vec3f::Zero();
    return t;
}

// 文件内容（前若干 KB 去前导空白后）是否以 "solid" 开头。
bool startsWithSolid(const std::filesystem::path &path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        return false;
    }
    std::array<char, 512> buf{};
    in.read(buf.data(), static_cast<std::streamsize>(buf.size()));
    const std::string head(buf.data(), static_cast<std::size_t>(in.gcount()));
    const auto pos = head.find_first_not_of(" \t\r\n");
    return pos != std::string::npos && head.compare(pos, 5, "solid") == 0;
}

} // namespace

StlReadResult readBinaryStl(const std::filesystem::path &path) {
    StlReadResult result;

    std::ifstream in(path, std::ios::binary);
    if (!in) {
        result.error = "无法打开文件: " + path.string();
        return result;
    }

    in.seekg(0, std::ios::end);
    const std::streamoff fileSize = in.tellg();
    in.seekg(0, std::ios::beg);
    if (fileSize < static_cast<std::streamoff>(kHeaderSize + kCountSize)) {
        result.error = "文件过短，不是有效的 Binary STL";
        return result;
    }

    std::array<char, kHeaderSize> header{};
    in.read(header.data(), header.size());

    std::uint32_t triCount = 0;
    in.read(reinterpret_cast<char *>(&triCount), sizeof(triCount));

    // 数量上限 + 文件长度一致性校验（防恶意 count 撑爆内存）。
    if (triCount > kMaxTriangles) {
        result.error = "三角形数量异常: " + std::to_string(triCount);
        return result;
    }
    const std::streamoff expectedSize =
        static_cast<std::streamoff>(kHeaderSize + kCountSize) +
        static_cast<std::streamoff>(triCount) * kTriangleSize;
    if (fileSize < expectedSize) {
        result.error = "文件长度不足：声明 " + std::to_string(triCount) +
                       " 个三角形，实际 " + std::to_string(fileSize) + " 字节";
        return result;
    }

    result.mesh.triangles.reserve(triCount);
    for (std::uint32_t i = 0; i < triCount; ++i) {
        std::array<float, 12> raw{};  // normal(3) + v0(3) + v1(3) + v2(3)
        std::uint16_t attr = 0;
        in.read(reinterpret_cast<char *>(raw.data()), sizeof(raw));
        in.read(reinterpret_cast<char *>(&attr), sizeof(attr));
        if (!in) {
            result.error = "读取第 " + std::to_string(i) + " 个三角形时失败";
            result.mesh.triangles.clear();
            return result;
        }

        // 法向以顶点绕序重算为准（makeTriangle 统一处理）：STL 文件法向字段常为
        // 零/错误（第三方导出器不可靠），且本项目约定 CCW 绕序对应正法向。
        // 切片不看本字段，重算仅影响渲染光照。
        result.mesh.triangles.push_back(makeTriangle(
            {geometry::Vec3f(raw[3], raw[4], raw[5]),
             geometry::Vec3f(raw[6], raw[7], raw[8]),
             geometry::Vec3f(raw[9], raw[10], raw[11])}));
    }

    result.mesh.computeBounds();
    result.ok = true;
    return result;
}

StlReadResult readAsciiStl(const std::filesystem::path &path) {
    StlReadResult result;

    std::ifstream in(path);
    if (!in) {
        result.error = "无法打开文件: " + path.string();
        return result;
    }

    // 逐行解析：vertex 收集 3 个顶点，endfacet 落一个三角形；
    // facet normal / outer loop / endloop / solid 行忽略（法向按绕序重算）。
    std::string line;
    std::vector<geometry::Vec3f> verts;
    bool sawFacet = false;
    while (std::getline(in, line)) {
        std::istringstream ls(line);
        std::string kw;
        if (!(ls >> kw)) {
            continue;  // 空行
        }
        if (kw == "facet") {
            sawFacet = true;
            verts.clear();
        } else if (kw == "vertex") {
            float x = 0.f, y = 0.f, z = 0.f;
            if (!(ls >> x >> y >> z)) {
                result.error = "ASCII STL 顶点行解析失败";
                return result;
            }
            verts.emplace_back(x, y, z);
        } else if (kw == "endfacet") {
            if (verts.size() != 3) {
                result.error = "ASCII STL facet 顶点数 != 3（格式损坏）";
                return result;
            }
            result.mesh.triangles.push_back(makeTriangle(verts));
            verts.clear();
        } else if (kw == "endsolid") {
            break;
        }
        // solid / outer / loop / endloop 等关键词忽略。
    }

    if (!sawFacet) {
        result.error = "不是有效的 ASCII STL（未找到 facet）";
        return result;
    }
    result.mesh.computeBounds();
    result.ok = true;
    return result;
}

StlReadResult readStl(const std::filesystem::path &path) {
    // Binary 优先：长度布局严格匹配才按 binary 解析（binary 头允许以 solid 开头）。
    auto binary = readBinaryStl(path);
    if (binary.ok) {
        return binary;
    }
    // 长度不匹配且文本以 solid 开头 → 尝试 ASCII。
    if (startsWithSolid(path)) {
        return readAsciiStl(path);
    }
    return binary;  // 返回 binary 的原始失败原因
}

} // namespace io
