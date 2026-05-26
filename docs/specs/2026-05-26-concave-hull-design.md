# POS文件凹包生成器 - 设计文档

**日期**：2026-05-26
**状态**：已实现

## 概述

读取包含影像位置（X, Y, Z）的POS文件，提取XY坐标，计算点集的凹包（concave hull），并输出边界顶点文本文件和可视化图像。

## 需求汇总

| 编号 | 需求 | 详情 |
|------|------|------|
| R1 | 输入格式 | POS文件：`影像名 X Y Z`，空格分隔，浮点坐标 |
| R2 | 坐标类型 | `cv::Point2d`（双精度浮点） |
| R3 | 凹包算法 | 凸包递归细化算法（convex hull edge refinement） |
| R4 | k参数 | 默认自动确定（`clamp(sqrt(N), 3, 60)`），可通过命令行参数覆盖 |
| R5 | 输出一 | 文本文件：每行一个 `x y`，首行为注释 |
| R6 | 输出二 | 可视化图像：原始点 + 凹包边界 |
| R7 | 文件路径 | 源代码中硬编码 |
| R8 | 依赖 | OpenCV4（vcpkg清单模式）+ C++17标准库 |
| R9 | 项目结构 | 多文件模块化结构，CMake构建系统 |

## 架构

模块化结构，按功能拆分为独立编译单元：

```
main.cpp
  ├── pos_reader.h/cpp      → ParsePosFile()
  ├── concave_hull.h/cpp    → ConcaveHull()
  ├── output.h/cpp          → SaveHullToFile() + Visualize()
  └── tests.h/cpp           → RunTests()
```

## 数据流

```
pos.txt ──ParsePosFile──▶ vector<cv::Point2d> ──ConcaveHull──▶ vector<cv::Point2d>
                               │                                    │
                               │                            ┌───────┴───────┐
                               │                            ▼               ▼
                               │                     concave_hull.txt  concave_hull.png
                               │
                               └── （点集同时传给Visualize用于绘制）
```

## 模块设计

### 1. pos_reader — POS文件解析

**文件**：`pos_reader.h` / `pos_reader.cpp`

```
函数签名：std::vector<cv::Point2d> ParsePosFile(const std::string& path)
```

**处理流程**：
- 打开文件，逐行读取
- 按空白字符分割每行
- 列顺序：`[0]=影像名, [1]=X, [2]=Y, [3]=Z`（丢弃Z）
- 使用 `std::stod()` 解析X/Y
- 以 `cv::Point2d(x, y)` 存入结果向量

**错误处理**：

| 情况 | 处理方式 |
|------|----------|
| 文件不存在 | 打印错误信息，`exit(1)` |
| 某行列数不足3列 | 打印警告（含行号），跳过该行 |
| 文件为空或无有效数据行 | 打印错误信息，`exit(1)` |

**边界情况**：
- 以 `#` 开头的行视为注释，跳过
- 空行跳过
- 重复XY坐标排序后去重（`std::unique`）

### 2. concave_hull — 凹包计算

**文件**：`concave_hull.h` / `concave_hull.cpp`

```
函数签名：std::vector<cv::Point2d> ConcaveHull(const std::vector<cv::Point2d>& points, int k)
```

**算法**：凸包递归细化（Convex Hull Edge Refinement）

1. **守护检查**：若 `points.size() < 3`，直接返回所有点
2. **计算凸包**：使用 `cv::convexHull()` 获取点集的凸包多边形（转换为 `cv::Point2f`）
3. **估算点密度**：对每个点计算最近邻距离，取中位值 `medianNN`
4. **计算边长阈值**：`maxEdgeLen = (k² / 60) × medianNN`，且不低于 `1.2 × medianNN`
   - k=3 → 阈值极小，紧密贴合
   - k=60 → 阈值极大，接近凸包
5. **迭代细化**：
   - 遍历当前多边形每条边
   - 找到长度超过阈值的最长边
   - 在该边中点附近寻找最近的未使用点
   - 将该点插入边的两端点之间
   - 重复直到所有边都不超过阈值，或迭代次数达到 `2N`
6. 返回凹包顶点序列

**复杂度**：
- 最近邻计算：O(N²)
- 凸包计算：O(N log N)（OpenCV内部实现）
- 迭代细化：O(H × N)，H为细化迭代次数

**k参数映射**：

| k范围 | 阈值因子 | 效果 |
|--------|----------|------|
| 3~10 | 0.15~1.67 | 紧密凹包 |
| 11~30 | 2.0~15 | 适度凹包 |
| 31~60 | 16~60 | 趋近凸包 |

**边界情况**：
- 点数不足3个：返回所有输入点
- 凸包顶点即满足阈值要求：返回凸包

### 3. output — 输出模块

**文件**：`output.h` / `output.cpp`

#### SaveHullToFile

```
函数签名：void SaveHullToFile(const std::vector<cv::Point2d>& hull,
                              const std::string& path,
                              int inputCount, int k)
```

- 首行写入包含元数据的注释
- 之后每行写入一对 `x y`，保留完整 double 精度
- 输出路径：与输入POS文件同目录，文件名 `concave_hull.txt`

**输出格式**：
```
# 凹包结果 - 输入451个点 -> 输出41个边界点 (k=21)
528928.835768999998 3802788.833287999965
528918.229182000039 3802815.035321999807
...
```

#### Visualize

```
函数签名：void Visualize(const std::vector<cv::Point2d>& points,
                          const std::vector<cv::Point2d>& hull,
                          const std::string& outputPath)
```

- 根据点集计算包围盒，留10%边距
- 将世界坐标映射到图像坐标（Y轴翻转）
- 图像尺寸：1920×1080（按数据宽高比等比缩放）
- 渲染层次（从背景到前景）：
  1. 黑色背景
  2. 输入点集：蓝色圆点，半径 2px
  3. 凹包边界：红色折线，线宽 2px，首尾闭合
  4. 凹包顶点：绿色填充圆，半径 4px
- 输出格式：PNG

### 4. tests — 自测模块

**文件**：`tests.h` / `tests.cpp`

```
函数签名：void RunTests()
```

5个自动化测试用例，在 `main()` 开始时运行：

| 测试 | 内容 | 验证点 |
|------|------|--------|
| 测试1 | 矩形+内部点 | 内部点被排除 |
| 测试2 | L形凹点集 | 凹包顶点数≥5 |
| 测试3a | 单点退化 | 返回1个顶点 |
| 测试3b | 两点退化 | 返回2个顶点 |
| 测试4 | 圆形+凹入区域 | 首尾接近闭合 |

### 5. main — 主函数

**文件**：`main.cpp`

**执行流程**：
1. 运行 `RunTests()` 自测
2. `ParsePosFile(POS_PATH)` → 点集
3. 确定k值（自动确定；可接受 `argv[1]` 作为整数k值覆盖）
4. `ConcaveHull(点集, k)` → 凹包
5. `SaveHullToFile(凹包, ...)` → 文本输出
6. `Visualize(点集, 凹包, ...)` → 图像输出

## 项目结构

```
ConcaveHullProj/
├── main.cpp              # 程序入口，流程编排
├── pos_reader.h/cpp      # POS文件解析模块
├── concave_hull.h/cpp    # 凹包计算核心算法
├── output.h/cpp          # 文本输出 + 图像可视化
├── tests.h/cpp           # 单元自测
├── CMakeLists.txt        # CMake构建配置
├── vcpkg.json            # vcpkg依赖清单（opencv4 4.7.0）
├── .gitignore
├── README.md
├── build/
└── docs/
    ├── specs/
    │   └── 2026-05-26-concave-hull-design.md
    └── superpowers/
        └── plans/
            └── 2026-05-26-concave-hull-plan.md
```

## CMake 配置

```cmake
cmake_minimum_required(VERSION 3.16)
project(ConcaveHullProj LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(MSVC)
    add_compile_options(/utf-8)
endif()

find_package(OpenCV REQUIRED)

add_executable(concave_hull
    main.cpp
    pos_reader.cpp
    concave_hull.cpp
    output.cpp
    tests.cpp
)
target_link_libraries(concave_hull PRIVATE ${OpenCV_LIBS})
```

## 构建方式

```bash
cd ConcaveHullProj
mkdir build && cd build
cmake .. -DCMAKE_TOOLCHAIN_FILE=<vcpkg路径>/scripts/buildsystems/vcpkg.cmake
cmake --build . --config Release
```

## 测试策略

**自动化自测**：5个单元测试在每次运行时自动执行，覆盖：
- 内部点排除
- 凹形边界提取
- 退化点集处理
- 凹包闭合性

**人工验证**：
- 使用不同k值运行，验证凹包松紧变化符合预期
- 检查 `concave_hull.txt` 顶点序列有序
- 检查 `concave_hull.png` 可视化效果

## 遗留问题

无。所有需求已实现。
