# 3DInkPlanner

**[English](README.md)**

桌面端 3D 喷墨打印预处理与路径规划 Demo。基于 C++17 / Qt6 / OpenGL / Eigen 从零实现，覆盖「STL 导入 → 3D 显示 → 分层切片 → 二维轮廓重建 → 扫描填充路径 → 设备坐标变换 → CSV 导出」完整预处理流程。项目作为求职作品，强调**分层架构**与**可测试的几何/路径算法**，核心算法不依赖任何 UI 框架。

## 处理流程

```text
3D Mesh (STL)
      ↓
Topology Check                顶点焊接 + 边表统计 + 法向一致性（只报告不改原数据）
      ↓
Triangle-Plane Intersection   分层切片（扫描线活动集加速）
      ↓
Segment Connection            离散线段 → 有序轮廓（O(n) 端点哈希 + 断链告警）
      ↓
Contour Classification        内外环判定（射线法）→ 带孔多边形
      ↓
Raster Fill                   等距扫描线填充（孔洞扣除 + serpentine）
      ↓
Path Optimization             多岛近邻排序 + Travel 空走连接段
      ↓
Coordinate Transform          模型坐标 → 设备坐标
      ↓
CSV / G-code Export           单层 CSV / 全层 G-code-like（G0 空走 / G1 喷印）
```

## 功能特性

- **Binary/ASCII STL 导入**：自动检测两种格式；解析二进制/文本 STL，校验文件头/三角形数，计算包围盒；法向以顶点绕序重算（免疫坏法向文件）
- **拓扑体检**：空间哈希顶点焊接 O(n) + 边表统计（流形/边界/非流形边）+ 连通分量 + 有符号体积校验；法向一致性 BFS 修复（派生结构上操作）
- **OpenGL 3D Viewer**：轨道相机（旋转/平移/缩放）、Lambert 光照、坐标轴
- **分层切片**：三角形与 Z 平面求交（半开区间规则避免顶点重复计数）；扫描线活动集加速 + 多线程层分块（输出与串行逐点一致）
- **轮廓重建**：端点哈希 O(n) 拼接为有序轮廓，T 型接头最小转角选择，断链显式告警
- **内外环分类**：射线法 containment 判定外环/孔洞，内环配对最小面积父环，方向统一（外 CCW 内 CW）
- **扫描填充路径**：等距扫描线 + even-odd 配对（孔洞区域自动扣除）+ serpentine 顺序
- **路径优化**：多岛最近邻贪心排序 + 断点插入 Travel 空走连接段，喷印/空走长度统计
- **可变层厚**：显式 z 高度表分层（GUI 逗号输入），区间外 z 显式跳过
- **自适应容差**：几何容差按模型尺寸缩放（max(1e-6, 尺寸×1e-9)）
- **共面告警**：与层面共面的水平三角形显式计数 + 告警（不完整优于错填）
- **坐标变换**：`T = Translation · RotationZ · Scale` 齐次变换
- **CSV / G-code 导出**：单层 CSV 逐点 `index,x,y,z,type`；全层 G-code-like（G0 空走 / G1 喷印 + 层间 Z 提升）
- **2D 路径预览**：外环浅灰 / 孔洞紫色 / 断链红色虚线分色 + 扫描路径 + 方向箭头 + 起点/终点，Layer Slider 逐层查看

## 架构

```text
app/        MainWindow         UI 编排：打开 STL、切片、预览、导出（不写几何算法）
widgets/    SliceView          2D 路径预览（QPainter）
graphics/   GLWidget/Camera/   3D 显示与相机交互（只做可视化）
           Shader/MeshRenderer
─────────────── 核心库 3DInkPlannerCore（不依赖 Qt UI）───────────────
io/         STLReader          STL 解析（Binary/ASCII 自动检测，法向绕序重算）
            PathExporter       路径 CSV / G-code-like 导出
topology/   IndexedMesh        索引网格（顶点焊接派生结构）
            MeshBuilder        边表拓扑统计 / 法向一致性修复
slicing/    TrianglePlaneIntersection  三角形-Z 平面求交
            SegmentConnector   线段拼接（O(n) 端点哈希 + 告警）
            ContourBuilder     轮廓简化
            ContourClassifier  内外环分类（射线法）
            Slicer             完整分层编排（扫描线加速）
path/       RasterFillGenerator 扫描填充路径（带孔多边形）
            PathOptimizer      岛间排序 + Travel 连接 + 长度统计
geometry/   GeometryTypes / Polyline / Polygon / Transform
```

分层原则：**Geometry / IO / Slicing / Path 层不依赖 Qt**，可独立单元测试；Graphics / Widgets 层只负责可视化。

## 核心算法

### 1. 三角形-平面求交（`slicing/TrianglePlaneIntersection`）

按顶点相对 `z = h` 的位置分为 Above / Below / On 三类；全 On（共面）忽略；否则收集每条与平面相交的边上的交点。采用**半开区间 `[min, max)`** 规则判断顶点归属，避免共享边/共享顶点被相邻三角形重复计数。

### 2. 拓扑构建与法向一致性（`topology/MeshBuilder`）

顶点焊接用**量化坐标空间哈希**（27 邻域查询）做到 O(n)；边表统计流形边（2 面共享）/边界边（1 面）/非流形边（≥3 面）；法向一致性以流形边为桥做 BFS 翻转传播，再按连通分量的**有符号体积** `Σ det(a,b,c)/6` 校正整体朝向。非流形只报告不修复，一切操作在派生的 IndexedMesh 上进行，原始 STL 数据只读。

### 3. 轮廓重建（`slicing/SegmentConnector`）

无序线段端点量化后建哈希表，O(n) 双向延伸拼接；度 > 2 的 T 型接头按最小转角选主链；首尾不闭合的链记入 openChains 并产生断链告警。随后 `ContourBuilder` 以「点到直线距离 ≤ tolerance」移除共线中间点。

### 4. 内外环分类（`slicing/ContourClassifier`）

射线法统计每个闭合环被其他环包含的次数：偶数为外环、奇数为内环（孔洞）；内环配对「包含它的面积最小外环」作为直接父环；最后统一方向（外环 CCW、内环 CW）。不使用面积启发式猜内外。

### 5. 扫描线分层加速（`slicing/Slicer`）

sliceAll 将三角形按 zMin 升序排序，层按 z 升序扫描：每层激活 zMin ≤ z 的面、清理 zMax < z 的面，只对**活动集**求交。复杂度从 O(层数×面数) 降为 O((面数+层数)·k)，k 为平均活跃面数。

多线程并行：层按连续区间均分给各线程，每线程独立运行扫描线（活动集从空重建，数学上与全程扫描等价），结果写入预分配固定槽位——无锁、无共享写，输出与串行**逐点一致**（测试用例交叉验证）。曾实测原子计数器动态调度，因块首重建扫描线的成本超过负载均衡收益而更慢，故保留静态均分。

### 6. 扫描填充（`path/RasterFillGenerator`）

生成水平扫描线 `y = y₀ + n·spacing`（偏移半个 spacing 避免切过顶点），与外环 + 所有孔洞内环的边一起求交后按 x 排序，用 **even-odd 规则**两两配对——孔洞区域被天然扣除；相邻行 **serpentine** 顺序（偶数行左→右、奇数行右→左）减少空走距离。

### 7. 路径优化（`path/PathOptimizer`）

同层多岛（独立多边形）按**最近邻贪心**排序（从喷头当前位置出发，每次选首段起点最近的岛）；相邻 Print 段之间的断点插入 **Travel 空走段**，使整层路径成为完全连续的 Print/Travel 交替序列；统计喷印/空走长度供工艺评估。不做 2-opt 全局优化（demo 规模最近邻已足够）。

## 目录结构

```text
3DInkPlanner/
├── app/            MainWindow / main
├── widgets/        SliceView（2D 预览）
├── graphics/       GLWidget / Camera / Shader / MeshRenderer
├── geometry/       几何类型与变换
├── io/             STLReader / PathExporter
├── topology/       IndexedMesh / MeshBuilder（拓扑体检）
├── slicing/        切片 / 轮廓重建 / 内外环分类
├── path/           扫描填充路径
├── tests/          11 个无框架单元测试（自写 check 断言，失败返回非零）
├── bench/          bench_pipeline（性能基准工具，不进 ctest）
├── tools/          generate_sample_stl.py（演示资产生成器，自带体积校验）
└── assets/models/  cube_20mm.stl / box_with_hole.stl / hollow_cylinder.stl / ...
```

## 构建

依赖：

- C++17 编译器（VS 2022）
- Qt 6.8（Widgets / OpenGL / OpenGLWidgets）
- Eigen 3.4（header-only）
- CMake ≥ 3.21

```bash
cmake -S . -B build -G "Visual Studio 17 2022" -A x64 \
      -DCMAKE_PREFIX_PATH=D:/Qt/6.8.3/msvc2022_64 \
      -DEIGEN3_INCLUDE_DIR=D:/eigen/eigen-3.4.0

cmake --build build --config Release
```

运行（需将 Qt `bin` 目录加入 PATH，用于定位平台插件 `qwindows.dll`）：

```bash
set PATH=D:\Qt\6.8.3\msvc2022_64\bin;%PATH%
build\Release\3DInkPlanner.exe
```

## 使用

1. `File → Open STL...` 打开 `assets/models/box_with_hole.stl`（或命令行 `3DInkPlanner.exe model.stl`）；状态栏显示拓扑体检结果（顶点/边界边/非流形边/分量/体积），非闭合网格弹窗提示
2. 设置层高 / 填充间距；或在「z表」输入逗号分隔的显式高度（如 `0.5,1.2,3.0`）走可变层厚
3. 点击「切片并生成路径」，用底部 Layer Slider 逐层查看（外环浅灰 / 孔洞紫色 / 断链红色虚线）
4. `File → Export Current Layer CSV...` 导出当前层路径；`File → Export All Layers G-code...` 导出全部层 G-code-like（G0 空走 / G1 喷印）

## 测试

```bash
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```

11 个测试套件覆盖：STL 读取（Binary/ASCII 自动检测）、三角形-平面求交、线段拼接（含断链/T 型接头告警）、切片（含扫描线与逐层一致性、并行/串行逐点一致性交叉验证、共面告警）、内外环分类、扫描填充（含孔洞扣除）、路径优化（岛间排序/Travel 连接/长度统计）、坐标变换（含自适应容差）、CSV 与 G-code 导出、示例资产集成、拓扑构建与法向一致性。均使用自写无依赖断言框架，不引入 GoogleTest。

## 性能基线

`bench_pipeline.exe <stl> [layerHeight] [spacing]`（Release，20 线程）：

| 模型 | 面数 | 层数 | 解析 | 切片（并行 / 串行） | 路径 | 总耗时 |
|---|---|---|---|---|---|---|
| sphere_r30_131k.stl（0.5mm） | 130,560 | 120 | 45ms | **28ms** / 98ms（3.5×） | 12ms | **84ms** |
| sphere_r30_131k.stl（0.2mm） | 130,560 | 300 | 43ms | **42ms** / 237ms（5.7×） | 30ms | **114ms** |

路径生成输出 Print + Travel 完整序列：球体 120 层共 11184 段（Print 5652 / Travel 5532），喷印 226.3 m / 空走 7.7 m（空走占比 3.3%）。

切片优化历程：V1.0 暴力 317ms → 扫描线活动集 41ms → 多线程分块 28ms（120 层）；层数越多并行加速比越高（300 层 5.7×，线程启动开销被摊薄）。

## Known Limitations

- 与层面**共面的水平三角形**被忽略并显式告警（2D 布尔补全留作扩展）；切片高度避免恰好取水平面可规避
- 非流形 / 边界边在拓扑体检中**只报告不修复**；断链层在预览中红色虚线标出，不参与填充
- Raster Fill 不处理自交多边形；PathOptimizer 岛间排序为最近邻贪心（非 2-opt 全局最优）
- G-code-like 导出为简化指令流（无挤出量/温度/速度等工艺参数），供流程演示而非直接上机
- 坐标变换仅支持**均匀缩放 + 绕 Z 轴旋转**，任意 4×4 标定矩阵导入未实现
- 切片容差默认按模型尺寸自适应（max(1e-6, 尺寸×1e-9)），极端尺寸仍建议人工核对
- 暂不直接控制打印设备（本 Demo 输出 CSV / G-code-like 路径文件）

## 示例数据

```bash
python tools/generate_sample_stl.py   # 重新生成 assets/models/ 下的演示 STL
```

- `cube_20mm.stl`：20×20×20 mm 立方体（单轮廓）
- `box_with_hole.stl`：40×40×10 mm 方块带 20×20 mm 通孔（内外壁两环，孔洞扣除填充）
- `hollow_cylinder.stl`：空心圆柱（每层 1 外环 + 1 孔洞）
- `plate_3holes.stl`：60×60×8 mm 三板带 3 个通孔（每层 1 外环 + 3 孔洞）
- `sphere_r30_131k.stl`：半径 30 mm UV 球（130,560 面，性能基准模型）
