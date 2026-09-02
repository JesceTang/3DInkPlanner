# 3DInkPlanner

桌面端 3D 喷墨打印预处理与路径规划 Demo。基于 C++17 / Qt6 / OpenGL / Eigen 从零实现，覆盖「STL 导入 → 3D 显示 → 分层切片 → 二维轮廓重建 → 扫描填充路径 → 设备坐标变换 → CSV 导出」完整预处理流程。项目作为求职作品，强调**分层架构**与**可测试的几何/路径算法**，核心算法不依赖任何 UI 框架。

## 处理流程

```text
3D Mesh (STL)
      ↓
Triangle-Plane Intersection   分层切片
      ↓
Segment Connection            离散线段 → 有序轮廓
      ↓
Contour Simplification        移除共线冗余顶点
      ↓
Raster Fill                   等距扫描线填充（serpentine）
      ↓
Coordinate Transform          模型坐标 → 设备坐标
      ↓
CSV Export                    PRINT / TRAVEL 路径导出
```

## 功能特性

- **Binary STL 导入**：解析二进制 STL，校验文件头/三角形数，计算包围盒
- **OpenGL 3D Viewer**：轨道相机（旋转/平移/缩放）、Lambert 光照、坐标轴
- **分层切片**：三角形与 Z 平面求交（半开区间规则避免顶点重复计数）
- **轮廓重建**：无序切片线段贪心双向拼接为有序轮廓，移除共线点
- **扫描填充路径**：等距水平扫描线 + even-odd 规则 + serpentine 顺序
- **坐标变换**：`T = Translation · RotationZ · Scale` 齐次变换
- **CSV 导出**：逐点 `index,x,y,z,type`（PRINT / TRAVEL）
- **2D 路径预览**：轮廓 + 扫描路径 + 方向箭头 + 起点/终点，Layer Slider 逐层查看

## 架构

```text
app/        MainWindow         UI 编排：打开 STL、切片、预览、导出（不写几何算法）
widgets/    SliceView          2D 路径预览（QPainter）
graphics/   GLWidget/Camera/   3D 显示与相机交互（只做可视化）
           Shader/MeshRenderer
─────────────── 核心库 3DInkPlannerCore（不依赖 Qt UI）───────────────
io/         STLReader          二进制 STL 解析
            PathExporter       路径 CSV 导出
slicing/    TrianglePlaneIntersection  三角形-Z 平面求交
            SegmentConnector   线段拼接
            ContourBuilder     轮廓简化
            Slicer             完整分层编排
path/       RasterFillGenerator 扫描填充路径
geometry/   GeometryTypes / Polyline / Polygon / Transform
```

分层原则：**Geometry / IO / Slicing / Path 层不依赖 Qt**，可独立单元测试；Graphics / Widgets 层只负责可视化。

## 核心算法

### 1. 三角形-平面求交（`slicing/TrianglePlaneIntersection`）

按顶点相对 `z = h` 的位置分为 Above / Below / On 三类；全 On（共面）忽略；否则收集每条与平面相交的边上的交点。采用**半开区间 `[min, max)`** 规则判断顶点归属，避免共享边/共享顶点被相邻三角形重复计数。

### 2. 轮廓重建（`slicing/SegmentConnector`）

将无序线段按端点容差 `hypot ≤ tolerance` 贪心双向拼接为有序折线；首尾重合则标记闭合并去掉重复尾点。随后 `ContourBuilder` 以「点到直线距离 ≤ tolerance」移除共线中间点，得到最简轮廓。

### 3. 扫描填充（`path/RasterFillGenerator`）

生成水平扫描线 `y = y₀ + n·spacing`（偏移半个 spacing 避免切过顶点），与多边形边求交后按 x 排序，用 **even-odd 规则**两两配对得到内部扫描段；相邻行 **serpentine** 顺序（偶数行左→右、奇数行右→左）减少空走距离。

## 目录结构

```text
3DInkPlanner/
├── app/            MainWindow / main
├── widgets/        SliceView（2D 预览）
├── graphics/       GLWidget / Camera / Shader / MeshRenderer
├── geometry/       几何类型与变换
├── io/             STLReader / PathExporter
├── slicing/        切片 / 轮廓重建
├── path/           扫描填充路径
├── tests/          8 个无框架单元测试（自写 check 断言，失败返回非零）
├── tools/          generate_sample_stl.py（演示资产生成器）
└── assets/models/  cube_20mm.stl / box_with_hole.stl
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

1. `File → Open STL...` 打开 `assets/models/box_with_hole.stl`（或命令行 `3DInkPlanner.exe model.stl`）
2. 设置层高 / 填充间距，点击「切片并生成路径」
3. 用底部 Layer Slider 逐层查看轮廓与扫描路径（2D 预览在右侧）
4. `File → Export Current Layer CSV...` 导出当前层路径

## 测试

```bash
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```

8 个测试套件覆盖：STL 读取、三角形-平面求交、线段拼接、切片、扫描填充、坐标变换、CSV 导出、示例资产集成（多轮廓切片）。均使用自写无依赖断言框架，不引入 GoogleTest。

## Known Limitations

- 仅支持 **Binary STL**，ASCII 未实现（读取时明确报错，不会误解析）
- 不处理 **non-manifold / 自交 / 有洞**网格，无工业级 mesh repair；开放轮廓仅记录不修复
- 法线直接使用 STL 原始值（无共享顶点拓扑，坏法线会影响光照）
- 共面三角形被忽略，含大面积水平面且边界无垂直面的模型可能缺失该层轮廓
- Raster Fill 不处理自交多边形，仅生成 Print 段（未生成换行 Travel 连接段，空走优化留待 PathOptimizer）
- 坐标变换仅支持**均匀缩放 + 绕 Z 轴旋转**，任意 4×4 标定矩阵导入未实现
- 切片容差为绝对容差（`1e-6`），超大/超小尺寸模型需按尺寸调整
- 暂不直接控制打印设备（本 Demo 输出 CSV 路径文件）

## 示例数据

```bash
python tools/generate_sample_stl.py   # 重新生成 assets/models/ 下的演示 STL
```

- `cube_20mm.stl`：20×20×20 mm 立方体（单轮廓）
- `box_with_hole.stl`：40×40×10 mm 方块带 20×20 mm 方孔（多轮廓，展示内外壁同时切片）
