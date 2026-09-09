#pragma once

#include "topology/IndexedMesh.h"

// 拓扑层：网格拓扑构建与分析。不依赖 Qt。
//
// 约束（STL 切片工具开发计划 §3）：
//   - 顶点焊接用空间哈希 O(n)，禁止 O(n²) 双重循环；
//   - 坐标比较带显式容差，禁止裸 ==；
//   - 非流形边只检测报告，不修改几何去"修复"；
//   - 输入的 triangle soup 只读，一切处理在派生的 IndexedMesh 上进行。

namespace topology {

class MeshBuilder {
public:
    // 顶点焊接：按容差合并重复顶点（量化坐标空间哈希 + 27 邻域查询），输出索引网格。
    // 焊接后顶点坍缩（两个及以上顶点索引相同）的面被丢弃，数量经 degenerateSkipped 输出。
    IndexedMesh buildIndexedMesh(const geometry::Mesh &soup, double weldTolerance,
                                 int *degenerateSkipped = nullptr) const;

    // 拓扑分析：边表统计（流形/边界/非流形）+ 连通分量 + 有符号体积。
    TopologyReport analyze(const IndexedMesh &mesh) const;

    // 法向一致性修复：以流形边为桥做 BFS 翻转传播（共享边同向出现则邻面翻转），
    // 再按连通分量的有符号体积校正整体朝向（负体积分量整体翻转）。
    // 返回被翻转的面数。非流形边不参与翻转传播（无法定义一致朝向，由 analyze 报告）。
    int makeNormalsConsistent(IndexedMesh &mesh) const;
};

} // namespace topology
