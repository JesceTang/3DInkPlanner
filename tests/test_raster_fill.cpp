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

    std::cout << "\n" << (g_failures == 0 ? "ALL PASS" : "HAS FAILURES")
              << " (" << g_failures << " failures)\n";
    return g_failures == 0 ? 0 : 1;
}
