#include "io/STLReader.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <string>
#include <vector>

namespace io {

namespace {

constexpr std::size_t kHeaderSize = 80;
constexpr std::size_t kCountSize = 4;
constexpr std::size_t kTriangleSize = 50;                 // normal(12) + 3 顶点(36) + attr(2)
constexpr std::uint32_t kMaxTriangles = 100'000'000;      // 防止恶意 count 导致内存爆炸

} // namespace

StlReadResult readBinaryStl(const std::filesystem::path &path) {
    StlReadResult result;

    // 1. 打开文件（RAII，无需手动 close）。
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        result.error = "无法打开文件: " + path.string();
        return result;
    }

    // 2. 文件长度校验。
    in.seekg(0, std::ios::end);
    const std::streamoff fileSize = in.tellg();
    in.seekg(0, std::ios::beg);
    if (fileSize < static_cast<std::streamoff>(kHeaderSize + kCountSize)) {
        result.error = "文件过短，不是有效的 Binary STL";
        return result;
    }

    // 3. 读取并跳过 80 字节头。
    std::array<char, kHeaderSize> header{};
    in.read(header.data(), header.size());

    // 3b. ASCII STL 检测（以 "solid" 开头），第一版仅支持 Binary。
    if (std::string(header.data(), 5) == "solid") {
        result.error = "ASCII STL 暂不支持（当前仅支持 Binary STL）";
        return result;
    }

    // 4. 读取三角形数量。
    std::uint32_t triCount = 0;
    in.read(reinterpret_cast<char *>(&triCount), sizeof(triCount));

    // 5. 校验数量合法性 + 与文件长度一致性（防越界 / 防恶意内存申请）。
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

    // 6. 逐个读取三角形（normal + 3 顶点 + 2 字节属性）。
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

        geometry::Triangle t;
        t.normal = geometry::Vec3f(raw[0], raw[1], raw[2]);
        t.v0 = geometry::Vec3f(raw[3], raw[4], raw[5]);
        t.v1 = geometry::Vec3f(raw[6], raw[7], raw[8]);
        t.v2 = geometry::Vec3f(raw[9], raw[10], raw[11]);
        result.mesh.triangles.push_back(t);
    }

    result.mesh.computeBounds();
    result.ok = true;
    return result;
}

} // namespace io
