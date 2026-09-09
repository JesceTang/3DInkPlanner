# 已知问题与潜在风险（开发记录）

> 本文件持续记录开发过程中发现的潜在问题、技术债与已知限制，随里程碑推进更新。
> 求职版 README 中的 Known Limitations 段将从此文件提炼。

## 环境 / 构建

- [ ] Eigen 路径硬编码在 `CMakeLists.txt`（`D:/eigen/eigen-3.4.0`），换机器需改路径或改用 `find_package(Eigen3)`。
- [ ] 仅安装了 qtbase（无 qttools/windeployqt），发布部署需手动拷贝 Qt DLL（`bin/` + 平台插件）。
- [ ] aqtinstall 并发下载会损坏大文件（已用 curl + py7zr 绕过），重建环境时注意。

## 几何 / 数据

- [x] ~~STL 仅支持 Binary~~ → V2.1 已修：readStl 自动检测 Binary/ASCII（长度布局匹配优先，solid 开头回退 ASCII 解析）。
- [x] ~~法线直接用 STL 原始值~~ → V2.0 已修：STLReader 统一按顶点绕序重算归一化法向（文件字段不可靠），退化面置零向量。
- [x] ~~有洞网格不处理~~ → V2.0 已修：内外环射线法分类（ContourClassifier）+ 带孔 Raster 填充（even-odd 天然扣除孔洞），GUI 分色显示。非流形仍只报告不修复。
- [x] V1.0 存量 bug：`tools/generate_sample_stl.py` 的 box_with_hole 内壁 y=-i 面绕序反、多孔板顶/底面条带剖分铺到孔区间（有符号体积校验抓获，已修）。

## 渲染

- [ ] 未做背面剔除（双面渲染，对坏法线 STL 更宽容，但大模型性能略低）。
- [ ] 坐标轴长度固定为模型尺寸 × 0.5，未适配极端尺寸模型。
- [ ] 相机 pan 手感系数（`m_distance * 0.0015`）未经系统调优。
- [ ] SliceView 用 QPainter 在 `paintEvent` 全量重绘轮廓/路径/方向箭头，段数很多时可能卡顿（demo 规模 OK，后续可加降采样/分段缓存）。

## 算法

- [x] ~~SegmentConnector O(n²)~~ → V2.0 已重写为 O(n) 端点空间哈希版（2 万段拼接 9.8ms），T 型接头最小转角选主链，断链显式告警。
- [x] ~~切片 O(层数×面数)~~ → V2.0 扫描线活动集加速：三角形按 zMin 排序 + 层升序单趟激活/清理，13 万面球体 120 层切片 317ms → 41ms。
- [ ] 开放轮廓（非流形/有洞网格导致的断裂）仅记录为 openChains + 红色虚线显示，暂无自动修复策略。
- [ ] 共面三角形被忽略 → V2.1 已加显式计数 + 告警（`Layer::coplanarTriangleCount`，"不完整优于错填"）；完整的水平面 2D 布尔补全留作扩展。
- [x] ~~绝对容差~~ → V2.1 已修：`Mesh::adaptiveTolerance()`（max(1e-6, 尺寸×1e-9)），GUI 切片与拓扑体检已接入。
- [ ] 轮廓简化（ContourBuilder）用"点到直线距离 <= tolerance"判共线，与拼接容差共用同一值；极端扁平的退化闭合轮廓简化后可能 < 3 顶点，未单独处理。
- [ ] 内外环分类假定同层轮廓环互不相交（病态网格产生的相交环不保证分类正确）；跨层 z 表输入会被排序为升序输出。
- [ ] Raster Fill 不处理自交多边形（§25 明确暂不处理）；扫描线过顶点用半开区间 + 偏移半个 spacing 规避，但极端退化（顶点坐标恰为 spacing 整数倍）仍可能出奇数交点。
- [x] ~~Raster Fill 仅生成 Print 段~~ → V2.1 已修：PathOptimizer 岛间最近邻排序 + 断点插入 Travel 连接段（Print/Travel 完全连续交替序列）+ 喷印/空走长度统计。岛间排序为贪心（非 2-opt），留作优化空间。
- [ ] Transform 仅支持均匀缩放 + 绕 Z 旋转；任意 4×4 标定矩阵导入、非均匀缩放、绕 X/Y 轴旋转未实现（§4.1 I 扩展项）。
