# 3DInkPlanner

3D 喷墨打印预处理与路径规划软件 Demo（求职展示项目，开发中）。

## 技术栈

- C++17
- Qt 6.8
- OpenGL 3.3 Core
- CMake

## 进度

- [x] Milestone 0：工程初始化（Qt + OpenGL 清屏窗口）
- [ ] Milestone 1：STL Viewer
- [ ] Milestone 2：单层切片
- [ ] Milestone 3：轮廓重建
- [ ] Milestone 4：完整分层
- [ ] Milestone 5：喷印路径
- [ ] Milestone 6：设备坐标
- [ ] Milestone 7：导出与演示

## 构建

```bash
cmake -S . -B build -G "Visual Studio 17 2022" -A x64 \
      -DCMAKE_PREFIX_PATH=D:/Qt/6.8.3/msvc2022_64
cmake --build build --config Release
```
