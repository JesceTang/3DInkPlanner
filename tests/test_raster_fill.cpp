// RasterFillGenerator 单元测试（无第三方框架，失败返回非零）。
#include "path/RasterFillGenerator.h"

#include <cmath>
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

bool nearD(double a, double b, double eps = 1e-9) {
    return std::fabs(a - b) < eps;
}

geometry::Polygon poly(std::initializer_list<std::pair<double, double>> pts) {
    geometry::Polygon p;
    for (const auto &pt : pts) {
        p.vertices.push_back({pt.first, pt.second});
    }
    return p;
}

} // namespace

int main() {
    path::RasterFillGenerator gen;

    // 1. 正方形 spacing=2.5 → 4 段（y=1.25/3.75/6.25/8.75）。
    {
        auto sq = poly({{0, 0}, {10, 0}, {10, 10}, {0, 10}});
        auto segs = gen.generate(sq, 2.5);
        check(segs.size() == 4, "正方形 spacing=2.5 生成 4 段");
        check(segs.size() == 4 && nearD(segs[0].start.y, 1.25) &&
                  nearD(segs[1].start.y, 3.75) && nearD(segs[2].start.y, 6.25) &&
                  nearD(segs[3].start.y, 8.75),
              "扫描线 y 值正确");
    }

    // 2. serpentine 方向：偶数行 L→R，奇数行 R→L。
    {
        auto sq = poly({{0, 0}, {10, 0}, {10, 10}, {0, 10}});
        auto segs = gen.generate(sq, 2.5);
        bool ok = segs.size() == 4 &&
                  nearD(segs[0].start.x, 0) && nearD(segs[0].end.x, 10) &&
                  nearD(segs[1].start.x, 10) && nearD(segs[1].end.x, 0) &&
                  nearD(segs[2].start.x, 0) && nearD(segs[2].end.x, 10) &&
                  nearD(segs[3].start.x, 10) && nearD(segs[3].end.x, 0);
        check(ok, "serpentine 方向交替（L→R / R→L）");
    }

    // 3. 三角形：段长度随高度递减。
    {
        auto tri = poly({{0, 0}, {10, 0}, {0, 10}});
        auto segs = gen.generate(tri, 2.5);
        bool ok = segs.size() == 4;
        const double expectLen[4] = {8.75, 6.25, 3.75, 1.25};
        for (int i = 0; i < 4 && ok; ++i) {
            ok = ok && nearD(std::fabs(segs[i].end.x - segs[i].start.x),
                             expectLen[i]);
        }
        check(ok, "三角形填充段长度递减（8.75/6.25/3.75/1.25）");
    }

    // 4. 凹多边形（顶部缺口）：y=2.5 行 1 段 [0,10]。
    {
        auto concave = poly({{0, 0}, {10, 0}, {10, 10}, {6, 10}, {6, 4},
                             {4, 4}, {4, 10}, {0, 10}});
        auto segs = gen.generate(concave, 5.0);
        // 行 0 (y=2.5) 1 段，行 1 (y=7.5) 2 段 → 共 3 段。
        check(segs.size() == 3, "凹多边形生成 3 段");
        if (segs.size() == 3) {
            check(nearD(segs[0].start.y, 2.5) && nearD(segs[0].start.x, 0) &&
                      nearD(segs[0].end.x, 10),
                  "y=2.5 行为单段 [0,10]");
        }
    }

    // 5. even-odd 多段配对：y=7.5 行产生 2 段 [0,4] 与 [6,10]（serpentine 反转）。
    {
        auto concave = poly({{0, 0}, {10, 0}, {10, 10}, {6, 10}, {6, 4},
                             {4, 4}, {4, 10}, {0, 10}});
        auto segs = gen.generate(concave, 5.0);
        bool ok = segs.size() == 3;
        if (ok) {
            // 奇数行 R→L：先 [6,10] 反向，再 [0,4] 反向。
            ok = nearD(segs[1].start.y, 7.5) && nearD(segs[1].start.x, 10) &&
                 nearD(segs[1].end.x, 6) &&
                 nearD(segs[2].start.y, 7.5) && nearD(segs[2].start.x, 4) &&
                 nearD(segs[2].end.x, 0);
        }
        check(ok, "even-odd 配对：y=7.5 行两段 [6,10] 与 [0,4]");
    }

    // 6. 退化多边形（<3 顶点）→ 空。
    {
        auto bad = poly({{0, 0}, {1, 1}});
        check(gen.generate(bad, 1.0).empty(), "退化多边形返回空");
    }

    // 7. 非法 spacing（<=0）→ 空。
    {
        auto sq = poly({{0, 0}, {10, 0}, {10, 10}, {0, 10}});
        check(gen.generate(sq, 0.0).empty() && gen.generate(sq, -1.0).empty(),
              "非法 spacing 返回空");
    }

    // 8. spacing 等于高度 → 恰好 1 行（首行在 height/2 处）。
    {
        auto sq = poly({{0, 0}, {10, 0}, {10, 10}, {0, 10}});
        auto segs = gen.generate(sq, 10.0);
        check(segs.size() == 1 && nearD(segs[0].start.y, 5.0),
              "spacing 等于高度时生成 1 行");
    }

    // 8b. spacing 远大于高度（首行已在多边形外）→ 空。
    {
        auto sq = poly({{0, 0}, {10, 0}, {10, 10}, {0, 10}});
        check(gen.generate(sq, 20.0).empty(),
              "spacing 过大（首行在多边形外）返回空");
    }

    // 9. 所有段类型均为 Print。
    {
        auto sq = poly({{0, 0}, {10, 0}, {10, 10}, {0, 10}});
        auto segs = gen.generate(sq, 2.5);
        bool allPrint = !segs.empty();
        for (const auto &s : segs) {
            allPrint = allPrint && (s.type == path::PathType::Print);
        }
        check(allPrint, "所有填充段类型为 Print");
    }

    // 10. 带孔正方形：外 [-10,10]² 孔 [-4,4]²，孔洞区域不产生填充段。
    {
        auto p = poly({{-10, -10}, {10, -10}, {10, 10}, {-10, 10}});
        p.holes.push_back({{-4, -4}, {4, -4}, {4, 4}, {-4, 4}});
        auto segs = gen.generate(p, 2.0);
        // 行 y=-9..9 共 10 行；|y|<4 的 4 行（-3,-1,1,3）各 2 段，其余 6 行各 1 段。
        check(segs.size() == 14, "带孔正方形生成 14 段（4 行×2 + 6 行×1）");
        // 所有段不得与孔内部 (-4,4)² 相交。
        bool clearOfHole = true;
        for (const auto &s : segs) {
            const double midX = (s.start.x + s.end.x) * 0.5;
            if (std::fabs(s.start.y) < 4.0 && std::fabs(midX) < 4.0 - 1e-9) {
                clearOfHole = false;
            }
        }
        check(clearOfHole, "填充段全部绕开孔洞区域");
    }

    // 11. 带孔正方形的跨孔行：偶数行 L→R，首段为 [−10,−4]。
    // 行 y=-9,-7,...,9（索引 0..9）；|y|<4 的跨孔行为索引 3..6，
    // 其中偶数索引 4 对应 y=-1（L→R：先 [-10,-4] 再 [4,10]）。
    {
        auto p = poly({{-10, -10}, {10, -10}, {10, 10}, {-10, 10}});
        p.holes.push_back({{-4, -4}, {4, -4}, {4, 4}, {-4, 4}});
        auto segs = gen.generate(p, 2.0);
        bool rowOk = false;
        for (const auto &s : segs) {
            if (nearD(s.start.y, -1.0)) {  // 偶数跨孔行（L→R）
                rowOk = nearD(s.start.x, -10.0) && nearD(s.end.x, -4.0);
                break;
            }
        }
        check(rowOk, "跨孔行首段为 [-10,-4]（even-odd 正确配对）");
    }

    // 12. 三孔：一竖行被 3 孔切成 4 段。
    {
        auto p = poly({{-30, -30}, {30, -30}, {30, 30}, {-30, 30}});
        p.holes.push_back({{-25, -5}, {-15, -5}, {-15, 5}, {-25, 5}});  // 孔1 x∈[-25,-15]
        p.holes.push_back({{-5, -5}, {5, -5}, {5, 5}, {-5, 5}});        // 孔2 x∈[-5,5]
        p.holes.push_back({{15, -5}, {25, -5}, {25, 5}, {15, 5}});      // 孔3 x∈[15,25]
        auto segs = gen.generate(p, 2.0);
        // y=±1 等行穿过 3 孔 → 4 段。
        bool foundQuad = false;
        for (size_t i = 0; i + 3 < segs.size(); ++i) {
            const double y = segs[i].start.y;
            if (nearD(segs[i + 1].start.y, y) && nearD(segs[i + 2].start.y, y) &&
                nearD(segs[i + 3].start.y, y)) {
                foundQuad = true;  // 存在 4 段同行
            }
        }
        check(foundQuad, "三孔截面存在 4 段同行的扫描线");
    }

    std::cout << "\n" << (g_failures == 0 ? "ALL PASS" : "HAS FAILURES")
              << " (" << g_failures << " failures)\n";
    return g_failures == 0 ? 0 : 1;
}
